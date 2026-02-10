/* Synchronet public message reading function */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"
#include "utf8.h"

int sbbs_t::sub_op(int subnum)
{
	return user_is_subop(&cfg, subnum, &useron, &client);
}

bool sbbs_t::can_view_deleted_msgs(int subnum)
{
	if ((cfg.sys_misc & SM_SYSVDELM) == 0) // No one can view deleted msgs
		return false;
	return (cfg.sys_misc & SM_USRVDELM) || sub_op(subnum);
}

uchar sbbs_t::msg_listing_flag(int subnum, smbmsg_t* msg, post_t* post)
{
	if (msg->hdr.attr & MSG_DELETE)
		return '-';
	if ((stricmp(msg->to, useron.alias) == 0 || stricmp(msg->to, useron.name) == 0)
	    && !(msg->hdr.attr & MSG_READ))
		return '!';
	if (msg->hdr.attr & MSG_PERMANENT)
		return 'p';
	if (msg->hdr.attr & MSG_LOCKED)
		return 'L';
	if (msg->hdr.attr & MSG_KILLREAD)
		return 'K';
	if (msg->hdr.attr & MSG_NOREPLY)
		return '#';
	if (msg->hdr.number > subscan[subnum].ptr)
		return '*';
	if (msg->hdr.attr & MSG_PRIVATE)
		return 'P';
	if (msg->hdr.attr & MSG_POLL)
		return '?';
	if (post->upvotes > post->downvotes)
		return 251;
	if (post->upvotes || post->downvotes)
		return 'v';
	if (msg->hdr.attr & MSG_REPLIED)
		return 'R';
	if (sub_op(subnum) && msg->hdr.attr & MSG_ANONYMOUS)
		return 'A';
	return ' ';
}

int sbbs_t::listmsgs(int subnum, int mode, post_t *post, int start, int posts, bool reading)
{
	smbmsg_t msg;
	int      listed = 0;

	for (int i = start; i < posts && !msgabort(); i++) {
		if (mode & SCAN_NEW && post[i].idx.number <= subscan[subnum].ptr)
			continue;
		msg.idx.offset = post[i].idx.offset;
		if (loadmsg(&msg, post[i].idx.number) < 0)
			break;
		smb_unlockmsghdr(&smb, &msg);
		if (listed == 0)
			bputs(text[MailOnSystemLstHdr]);
		bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
		        , msghdr_text(&msg, SubMsgLstFmt), reading ? (i + 1) : post[i].num
		        , msg.hdr.attr & MSG_ANONYMOUS && !sub_op(subnum)
		    ? text[Anonymous] : msg.from
		        , msg.to
		        , msg_listing_flag(subnum, &msg, &post[i])
		        , msg.subj);
		smb_freemsgmem(&msg);
		msg.total_hfields = 0;
		listed++;
	}

	return listed;
}

void sbbs_t::dump_msghdr(smbmsg_t* msg)
{
	term->newline();
	str_list_t list = smb_msghdr_str_list(msg);
	if (list != NULL) {
		for (int i = 0; list[i] != NULL && !msgabort(); i++) {
			bprintf("%s\r\n", list[i]);
		}
		strListFree(&list);
	}
}

/****************************************************************************/
/* posts is the actual number of posts returned in the allocated array		*/
/* visible is the number of visible posts to the user (not all included in	*/
/* returned array)															*/
/****************************************************************************/
post_t * sbbs_t::loadposts(uint32_t *posts, int subnum, uint ptr, int mode, uint *unvalidated_num, uint32_t* visible)
{
	ushort   aliascrc, namecrc, sysop;
	int      i, skip;
	uint     l = 0, total, alloc_len;
	uint32_t curmsg = 0;
	smbmsg_t msg;
	idxrec_t idx;
	post_t * post;

	if (posts == NULL)
		return NULL;

	(*posts) = 0;

	if ((i = smb_locksmbhdr(&smb)) != 0) {               /* Be sure noone deletes or */
		errormsg(WHERE, ERR_LOCK, smb.file, i, smb.last_error);     /* adds while we're reading */
		return NULL;
	}

	total = (int)filelength(fileno(smb.sid_fp)) / sizeof(idxrec_t); /* total msgs in sub */

	if (!total) {            /* empty */
		smb_unlocksmbhdr(&smb);
		return NULL;
	}

	namecrc = smb_name_crc(useron.name);
	aliascrc = smb_name_crc(useron.alias);
	sysop = crc16("sysop", 0);

	rewind(smb.sid_fp);

	alloc_len = sizeof(post_t) * total;
	if ((post = (post_t *)malloc(alloc_len)) == NULL) {  /* alloc for max */
		smb_unlocksmbhdr(&smb);
		errormsg(WHERE, ERR_ALLOC, smb.file, alloc_len);
		return NULL;
	}
	memset(post, 0, alloc_len);

	if (unvalidated_num)
		*unvalidated_num = UINT_MAX;

	size_t offset = 0;
	while (!feof(smb.sid_fp)) {
		skip = 0;
		if (smb_fread(&smb, &idx, sizeof(idx), smb.sid_fp) != sizeof(idx))
			break;

		msg.idx_offset = offset++;

		if (idx.number == 0)   /* invalid message number, ignore */
			continue;

		if (mode & LP_NOMSGS && (idx.attr & MSG_POLL_VOTE_MASK) == 0)
			continue;

		if (idx.attr & MSG_DELETE) {       /* Pre-flagged */
			if (mode & LP_REP)             /* Don't include deleted msgs in REP pkt */
				continue;
			if (!can_view_deleted_msgs(subnum))
				continue;
			if (!sub_op(subnum)          /* not sub-op */
			    && idx.from != namecrc && idx.from != aliascrc) /* not for you */
				continue;
		}

		if ((idx.attr & (MSG_MODERATED | MSG_VALIDATED | MSG_DELETE)) == MSG_MODERATED) {
			if (mode & LP_REP || !sub_op(subnum))
				break;
		}

		switch (idx.attr & MSG_POLL_VOTE_MASK) {
			case MSG_VOTE:
			case MSG_UPVOTE:
			case MSG_DOWNVOTE:
			{
				uint u;
				for (u = 0; u < l; u++)
					if (post[u].idx.number == idx.remsg)
						break;
				if (u < l) {
					post[u].total_votes++;
					switch (idx.attr & MSG_VOTE) {
						case MSG_UPVOTE:
							post[u].upvotes++;
							break;
						case MSG_DOWNVOTE:
							post[u].downvotes++;
							break;
						default:
							for (int b = 0; b < MSG_POLL_MAX_ANSWERS; b++) {
								if (idx.votes & (1 << b))
									post[u].votes[b]++;
							}
					}
				}
				if (!(mode & LP_VOTES))
					continue;
				break;
			}
			case MSG_POLL:
				if (!(mode & LP_POLLS))
					continue;
				break;
			case MSG_POLL_CLOSURE:
				if (!(mode & LP_VOTES))
					continue;
				break;
		}

		if (idx.attr & MSG_PRIVATE && !(mode & LP_PRIVATE)
		    && !sub_op(subnum) && !(useron.rest & FLAG('Q'))) {
			if (idx.to != namecrc && idx.from != namecrc
			    && idx.to != aliascrc && idx.from != aliascrc
			    && (useron.number != 1 || idx.to != sysop))
				continue;
			msg.idx = idx;
			if (!smb_lockmsghdr(&smb, &msg)) {
				if (!smb_getmsghdr(&smb, &msg)) {
					if (stricmp(msg.to, useron.alias)
					    && stricmp(msg.from, useron.alias)
					    && stricmp(msg.to, useron.name)
					    && stricmp(msg.from, useron.name)
					    && (useron.number != 1 || stricmp(msg.to, "sysop")
					        || msg.from_net.type))
						skip = 1;
					smb_freemsgmem(&msg);
				}
				smb_unlockmsghdr(&smb, &msg);
			}
			if (skip)
				continue;
		}

		curmsg++;

		if (idx.number <= ptr)
			continue;

		if (idx.attr & MSG_READ && mode & LP_UNREAD) /* Skip read messages */
			continue;

		if (!(mode & LP_BYSELF) && (idx.from == namecrc || idx.from == aliascrc)) {
			msg.idx = idx;
			if (!smb_lockmsghdr(&smb, &msg)) {
				if (!smb_getmsghdr(&smb, &msg)) {
					if (!stricmp(msg.from, useron.alias)
					    || !stricmp(msg.from, useron.name))
						skip = 1;
					smb_freemsgmem(&msg);
				}
				smb_unlockmsghdr(&smb, &msg);
			}
			if (skip)
				continue;
		}

		if (!(mode & LP_OTHERS)) {
			if (idx.to != namecrc && idx.to != aliascrc
			    && (useron.number != 1 || idx.to != sysop))
				continue;
			msg.idx = idx;
			if (!smb_lockmsghdr(&smb, &msg)) {
				if (!smb_getmsghdr(&smb, &msg)) {
					if (stricmp(msg.to, useron.alias) && stricmp(msg.to, useron.name)
					    && (useron.number != 1 || stricmp(msg.to, "sysop")
					        || msg.from_net.type))
						skip = 1;
					smb_freemsgmem(&msg);
				}
				smb_unlockmsghdr(&smb, &msg);
			}
			if (skip)
				continue;
		}

		if ((idx.attr & (MSG_MODERATED | MSG_VALIDATED | MSG_DELETE)) == MSG_MODERATED) {
			if (unvalidated_num && *unvalidated_num > l)
				*unvalidated_num = l;
		}

		memcpy(&post[l].idx, &idx, sizeof(idx));
		post[l].num = curmsg;
		l++;
	}
	smb_unlocksmbhdr(&smb);
	if (!l)
		FREE_AND_NULL(post);

	if (visible != NULL)   /* Total number of currently visible/readable messages to the user */
		*visible = curmsg;
	(*posts) = l;
	return post;
}

int64_t sbbs_t::get_start_msgnum(smb_t* smb, int next)
{
	uint32_t j = smb->curmsg + next;
	int64_t  i;

	if (j < smb->msgs)
		j++;
	else
		j = 1;
	bprintf(text[StartWithN], j);
	if ((i = getnum(smb->msgs)) < 0)
		return i;
	if (i == 0)
		return j - 1;
	return i - 1;
}

static int score_post(post_t* post)
{
	if ((post->idx.attr & MSG_POLL_VOTE_MASK) == MSG_POLL)
		return 0;
	return (post->upvotes * 2) + ((post->idx.attr & MSG_REPLIED) ? 1:0) - (post->downvotes * 2);
}

static int rank_post(const void* a1, const void* a2)
{
	post_t* p1 = (post_t*)a1;
	post_t* p2 = (post_t*)a2;
	int     diff = score_post(p2) - score_post(p1);
	if (diff == 0)
		return p2->idx.time - p1->idx.time;
	return diff;
}

static int find_post(smb_t* smb, uint32_t msgnum, post_t* post)
{
	uint32_t i;

	/* ToDo: optimize search */
	for (i = 0; i < smb->msgs; i++)
		if (post[i].idx.number == msgnum)
			return i;
	return -1;
}

void sbbs_t::show_thread(uint32_t msgnum, post_t* post, unsigned curmsg, int thread_depth, uint64_t reply_mask)
{
	smbmsg_t msg;

	int      i = find_post(&smb, msgnum, post);
	if (i < 0)
		return;

	memset(&msg, 0, sizeof(msg));
	msg.idx = post[i].idx;
	if (smb_getmsghdr(&smb, &msg) != SMB_SUCCESS)
		return;
	attr(LIGHTGRAY);
	if (thread_depth) {
		for (int j = 0; j < thread_depth; j++)
			bprintf("%*s%c", j > 10 ? 0 : j > 5 ? 1 : 2, ""
			        , j + 1 == thread_depth ? msg.hdr.thread_next ? 195 : 192 : (reply_mask & (1LL << j)) ? 179 : ' ');
	}
	if ((unsigned)i == curmsg)
		attr(HIGH);
	bprintf("\1c%u\1g%c "
	        , post[i].num
//		,msg.hdr.number
	        , (unsigned)i == curmsg ? '>' : ':');
	bprintf("\1w%-*.*s\1g%c\1g%c \1w%s\r\n"
	        , (int)(term->cols - term->column - 12)
	        , (int)(term->cols - term->column - 12)
	        , msg.hdr.attr & MSG_ANONYMOUS && !sub_op(smb.subnum)
	        ? text[Anonymous] : msghdr_field(&msg, msg.from)
	        , (unsigned)i == curmsg ? '<' : ' '
	        , msg_listing_flag(smb.subnum, &msg, &post[i])
	        , datestr(smb_time(msg.hdr.when_written)));

	if (thread_depth) {
		if (msg.hdr.thread_first)
			reply_mask |= (1LL << (thread_depth - 1));
		else
			reply_mask &= ~(1LL << (thread_depth - 1));
	}
	if (thread_depth && !msg.hdr.thread_next)
		reply_mask &= ~(1LL << (thread_depth - 1));
	if (msg.hdr.thread_first)
		show_thread(msg.hdr.thread_first, post, curmsg, thread_depth + 1, reply_mask);
	if (msg.hdr.thread_next)
		show_thread(msg.hdr.thread_next, post, curmsg, thread_depth, reply_mask);
	smb_freemsgmem(&msg);
}

/****************************************************************************/
/* Reads posts on subboard sub. 'mode' determines new-posts only, browse,   */
/* or continuous read.                                                      */
/* Returns 0 if normal completion, 1 if aborted.                            */
/* Called from function main_sec                                            */
/****************************************************************************/
int sbbs_t::scanposts(int subnum, int mode, const char *find)
{
	char     str[256], str2[256], do_find = true, mismatches = 0
	, done = 0, domsg = 1, *buf;
	char     find_buf[128];
	char     tmp[128];
	char     savepath[MAX_PATH + 1]{};
	int      i;
	int64_t  i64;
	int      quit = 0;
	uint     usub, ugrp, reads = 0;
	uint     lp = LP_BYSELF;
	int      org_mode = mode;
	uint     msgs, l, unvalidated;
	uint32_t last;
	uint32_t u;
	post_t * post;
	smbmsg_t msg;
	bool     thread_mode = false;

	action = NODE_RMSG;
	cursubnum = subnum;   /* for ARS */

	bool invoked;
	i = exec_mod("scan messages", cfg.scanposts_mod, &invoked, "%s %u %s", cfg.sub[subnum]->code, mode, find);
	if (invoked)
		return i;
	find_buf[0] = 0;
	if (!chk_ar(cfg.sub[subnum]->read_ar, &useron, &client)) {
		bprintf(text[CantReadSub]
		        , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->sname);
		return 0;
	}
	ZERO_VAR(msg);              /* init to NULL, specify not-allocated */
	if (!(mode & SCAN_CONT))
		term->lncntr = 0;
	if ((msgs = getlastmsg(subnum, &last, 0)) == 0) {
		if (mode & (SCAN_NEW | SCAN_TOYOU))
			bprintf(text[NScanStatusFmt]
			        , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->lname, 0L, 0L);
		else if (!(mode & SCAN_POLLS))
			bprintf(text[NoMsgsOnSub]
			        , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->sname);
		return 0;
	}
	if (mode & SCAN_NEW && subscan[subnum].ptr >= last && !(mode & SCAN_BACK)) {
		if (subscan[subnum].ptr > last)
			subscan[subnum].ptr = last;
		if (subscan[subnum].last > last)
			subscan[subnum].last = last;
		bprintf(text[NScanStatusFmt]
		        , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->lname, 0L, msgs);
		return 0;
	}

	if ((i = smb_stack(&smb, SMB_STACK_PUSH)) != 0) {
		errormsg(WHERE, ERR_OPEN, cfg.sub[subnum]->code, i);
		return 0;
	}
	SAFEPRINTF2(smb.file, "%s%s", cfg.sub[subnum]->data_dir, cfg.sub[subnum]->code);
	smb.retry_time = cfg.smb_retry_time;
	smb.subnum = subnum;
	if ((i = smb_open(&smb)) != 0) {
		errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
		smb_stack(&smb, SMB_STACK_POP);
		return 0;
	}
	usub = getusrsub(subnum);
	ugrp = getusrgrp(subnum);

	if (!(mode & SCAN_TOYOU)
	    && (!mode || mode & SCAN_FIND || !(subscan[subnum].cfg & SUB_CFG_YSCAN)))
		lp |= LP_OTHERS;
	if (mode & SCAN_TOYOU && mode & SCAN_UNREAD)
		lp |= LP_UNREAD;
	if (!(cfg.sub[subnum]->misc & SUB_NOVOTING))
		lp |= LP_POLLS;
	if (mode & SCAN_POLLS)
		lp |= LP_NOMSGS;
	post = loadposts(&smb.msgs, subnum, 0, lp, &unvalidated);
	if (mode & SCAN_NEW) {           /* Scanning for new messages */
		for (smb.curmsg = 0; smb.curmsg < smb.msgs; smb.curmsg++)
			if (subscan[subnum].ptr < post[smb.curmsg].idx.number)
				break;
		term->lncntr = 0;
		bprintf(text[NScanStatusFmt]
		        , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->lname, smb.msgs - smb.curmsg, msgs);
		if (!smb.msgs) {       /* no messages at all */
			smb_close(&smb);
			smb_stack(&smb, SMB_STACK_POP);
			return 0;
		}
		if (smb.curmsg == smb.msgs) {  /* no new messages */
			if (!(mode & SCAN_BACK)) {
				if (post)
					free(post);
				smb_close(&smb);
				smb_stack(&smb, SMB_STACK_POP);
				return 0;
			}
			smb.curmsg = smb.msgs - 1;
		}
	}
	else {
		term->cleartoeol();
		term->lncntr = 0;
		if (mode & SCAN_TOYOU)
			bprintf(text[NScanStatusFmt]
			        , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->lname, smb.msgs, msgs);
		if (!smb.msgs) {
			if (!(mode & (SCAN_TOYOU | SCAN_POLLS)))
				bprintf(text[NoMsgsOnSub]
				        , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->sname);
			smb_close(&smb);
			smb_stack(&smb, SMB_STACK_POP);
			return 0;
		}
		if (mode & SCAN_FIND) {
			bprintf(text[SearchSubFmt]
			        , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->lname, smb.msgs);
			domsg = 1;
			smb.curmsg = 0;
		}
		else if (mode & (SCAN_TOYOU | SCAN_POLLS))
			smb.curmsg = 0;
		else {
			for (smb.curmsg = 0; smb.curmsg < smb.msgs; smb.curmsg++)
				if (post[smb.curmsg].idx.number >= subscan[subnum].last)
					break;
			if (smb.curmsg == smb.msgs)
				smb.curmsg = smb.msgs - 1;

			domsg = 1;
		}
	}

	if (useron.misc & RIP)
		menu("msgscan");

	if ((i = smb_locksmbhdr(&smb)) != 0) {
		smb_close(&smb);
		errormsg(WHERE, ERR_LOCK, smb.file, i, smb.last_error);
		smb_stack(&smb, SMB_STACK_POP);
		return 0;
	}
	if ((i = smb_getstatus(&smb)) != 0) {
		smb_close(&smb);
		errormsg(WHERE, ERR_READ, smb.file, i, smb.last_error);
		smb_stack(&smb, SMB_STACK_POP);
		return 0;
	}
	smb_unlocksmbhdr(&smb);
	last = smb.status.last_msg;

	if (mode & SCAN_CONT) {   /* update action */
		if (getnodedat(cfg.node_num, &thisnode, true)) {
			thisnode.action = NODE_RMSG;
			putnodedat(cfg.node_num, &thisnode);
		}
	}
	current_msg = &msg;   /* For MSG_* @-codes and bbs.msg_* property values */
	while (online && !done) {

		action = NODE_RMSG;

		if (mode & (SCAN_CONT | SCAN_FIND) && sys_status & SS_ABORT)
			break;

		if (post == NULL)  /* Been unloaded */
			post = loadposts(&smb.msgs, subnum, 0, lp, &unvalidated); /* So re-load */

		if (!smb.msgs) {
			done = 1;
			continue;
		}

		while (smb.curmsg >= smb.msgs) smb.curmsg--;

		msg.idx = post[smb.curmsg].idx;

		if ((i = smb_locksmbhdr(&smb)) != 0) {
			errormsg(WHERE, ERR_LOCK, smb.file, i, smb.last_error);
			break;
		}
		if ((i = smb_getstatus(&smb)) != 0) {
			smb_unlocksmbhdr(&smb);
			errormsg(WHERE, ERR_READ, smb.file, i, smb.last_error);
			break;
		}
		smb_unlocksmbhdr(&smb);

		if (smb.status.last_msg != last) {     /* New messages */
			last = smb.status.last_msg;
			if (post) {
				free((void *)post);
			}
			post = loadposts(&smb.msgs, subnum, 0, lp, &unvalidated);   /* So re-load */
			if (!smb.msgs)
				break;
			for (smb.curmsg = 0; smb.curmsg < smb.msgs; smb.curmsg++)
				if (post[smb.curmsg].idx.number == msg.idx.number)
					break;
			if (smb.curmsg > (smb.msgs - 1))
				smb.curmsg = (smb.msgs - 1);
			continue;
		}

		if (msg.total_hfields)
			smb_freemsgmem(&msg);
		msg.total_hfields = 0;

		if (loadmsg(&msg, post[smb.curmsg].idx.number) < 0) {
			if (mismatches > 5) {  /* We can't do this too many times in a row */
				errormsg(WHERE, ERR_CHK, smb.file, post[smb.curmsg].idx.number);
				break;
			}
			if (post)
				free(post);
			post = loadposts(&smb.msgs, subnum, 0, lp, &unvalidated);
			if (!smb.msgs)
				break;
			if (smb.curmsg > (smb.msgs - 1))
				smb.curmsg = (smb.msgs - 1);
			mismatches++;
			continue;
		}
		smb_unlockmsghdr(&smb, &msg);

		mismatches = 0;

		if (thread_mode) {
			uint32_t first = smb_first_in_thread(&smb, &msg, NULL);
			if (first <= 0) {
				bputs(text[NoMessagesFound]);
				break;
			}
			bprintf("\1n\1l\1h\1bThread\1n\1b: \1h\1c");
			bprintf("%-.*s\r\n", (int)(term->cols - (term->column + 1)), msghdr_field(&msg, msg.subj));
			show_thread(first, post, smb.curmsg);
			subscan[subnum].last = post[smb.curmsg].idx.number;
		}
		else if (domsg && !(sys_status & SS_ABORT)) {

			if (do_find && mode & SCAN_FIND) {             /* Find text in messages */
				buf = smb_getmsgtxt(&smb, &msg, GETMSGTXT_ALL);
				if (!buf) {
					if (smb.curmsg < smb.msgs - 1)
						smb.curmsg++;
					else if (org_mode & SCAN_FIND)
						done = 1;
					else if (smb.curmsg >= smb.msgs - 1)
						domsg = 0;
					continue;
				}
				if (strcasestr(buf, find) == NULL && strcasestr(msg.subj, find) == NULL
				    && (msg.tags == NULL || strcasestr(msg.tags, find) == NULL)) {
					free(buf);
					if (smb.curmsg < smb.msgs - 1)
						smb.curmsg++;
					else if (org_mode & SCAN_FIND)
						done = 1;
					else if (smb.curmsg >= smb.msgs - 1)
						domsg = 0;
					continue;
				}
				free(buf);
			}

			if (mode & SCAN_CONT)
				bprintf(text[ZScanPostHdr], ugrp, usub, smb.curmsg + 1, smb.msgs);

			if (!reads && mode)
				term->newline();

			msg.upvotes = post[smb.curmsg].upvotes;
			msg.downvotes = post[smb.curmsg].downvotes;
			msg.total_votes = post[smb.curmsg].total_votes;
			show_msg(&smb, &msg
			         , msg.from_ext && !strcmp(msg.from_ext, "1") && !msg.from_net.type
			        ? 0:P_NOATCODES
			         , &post[smb.curmsg]);

			reads++;    /* number of messages actually read during this sub-scan */

			/* Message is to this user and hasn't been read, so flag as read */
			if ((!stricmp(msg.to, useron.name) || !stricmp(msg.to, useron.alias)
			     || (useron.number == 1 && !stricmp(msg.to, "sysop")
			         && !msg.from_net.type))
			    && !(msg.hdr.attr & MSG_READ)) {
				if (msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields = 0;
				msg.idx.offset = 0;
				if (!smb_locksmbhdr(&smb)) {               /* Lock the entire base */
					if (loadmsg(&msg, msg.idx.number) >= 0) {
						msg.hdr.attr |= MSG_READ;
						msg.idx.attr = msg.hdr.attr;
						if ((i = smb_putmsg(&smb, &msg)) != 0)
							errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
						smb_unlockmsghdr(&smb, &msg);
					}
					smb_unlocksmbhdr(&smb);
				}
				if (!msg.total_hfields) {                /* unsuccessful reload */
					domsg = 0;
					continue;
				}
			}

			subscan[subnum].last = post[smb.curmsg].idx.number;

			if (subscan[subnum].ptr < post[smb.curmsg].idx.number && !(mode & SCAN_TOYOU)) {
				posts_read++;
				subscan[subnum].ptr = post[smb.curmsg].idx.number;
			}

			if (sub_op(subnum) && (msg.hdr.attr & (MSG_MODERATED | MSG_VALIDATED | MSG_DELETE)) == MSG_MODERATED) {
				uint16_t msg_attr = msg.hdr.attr;
				SAFEPRINTF2(str, text[ValidatePostQ], smb.curmsg + 1, msghdr_field(&msg, msg.subj));
				if (!noyes(str))
					msg_attr |= MSG_VALIDATED;
				else {
					SAFEPRINTF2(str, text[DeletePostQ], smb.curmsg + 1, msghdr_field(&msg, msg.subj));
					if (yesno(str))
						msg_attr |= MSG_DELETE;
				}
				if (msg_attr != msg.hdr.attr) {
					if (msg.total_hfields)
						smb_freemsgmem(&msg);
					msg.total_hfields = 0;
					msg.idx.offset = 0;
					if (!smb_locksmbhdr(&smb)) {               /* Lock the entire base */
						if (loadmsg(&msg, msg.idx.number) >= 0) {
							msg.hdr.attr = msg.idx.attr = msg_attr;
							if ((i = smb_putmsg(&smb, &msg)) != 0)
								errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
							smb_unlockmsghdr(&smb, &msg);
						}
						smb_unlocksmbhdr(&smb);
					}
					if (msg_attr & MSG_DELETE) {
						if (can_view_deleted_msgs(subnum))
							domsg = 0;  // If you can view deleted messages, don't redisplay.
					}
					else {
						domsg = 0;        // If you just validated, don't redisplay.
					}
					if (post)
						free(post);
					post = loadposts(&smb.msgs, subnum, 0, lp, &unvalidated);
					if (!smb.msgs)
						break;
					if (smb.curmsg > (smb.msgs - 1))
						smb.curmsg = (smb.msgs - 1);
					mismatches++;
					continue;
				}
			}
		}
		else {
			if (sys_status & SS_ABORT)
				break;
			domsg = 1;
		}
		if (mode & SCAN_CONT) {
			if (smb.curmsg < smb.msgs - 1)
				smb.curmsg++;
			else
				done = 1;
			continue;
		}
		sync();
		if (unvalidated < smb.curmsg)
			bprintf(text[UnvalidatedWarning], unvalidated + 1);
		if (term->lncntr >= term->rows - 2)
			term->lncntr--;
		bprintf(text[ReadingSub], ugrp, cfg.grp[cfg.sub[subnum]->grp]->sname
		        , usub, cfg.sub[subnum]->sname, smb.curmsg + 1, smb.msgs);
		snprintf(str, sizeof str, "ABCDEFHILMNPQRTUVY?*<>[]{}-+()\b%c%c%c%c"
		         , TERM_KEY_LEFT
		         , TERM_KEY_RIGHT
		         , TERM_KEY_HOME
		         , TERM_KEY_END
		         );
		if (thread_mode)
			snprintf(str + strlen(str), 3, "%c%c"
			         , TERM_KEY_UP
			         , TERM_KEY_DOWN);

		if (sub_op(subnum))
			strcat(str, "O");
		do_find = true;
		l = getkeys(str, smb.msgs);
		if (l & 0x80000000L) {
			if ((int)l == -1) { /* ctrl-c */
				quit = 1;
				break;
			}
			smb.curmsg = (l & ~0x80000000L) - 1;
			do_find = false;
			thread_mode = false;
			domsg = true;
			continue;
		}
		if (thread_mode && (IS_ALPHA(l) || l == '?')) {
			thread_mode = false;
			domsg = true;
		}
		switch (l) {
			case '*':
				thread_mode = !thread_mode;
				if (!thread_mode)
					domsg = true;
				continue;
			case 'A':
			case 'R':
				if ((char)l == (cfg.sys_misc & SM_RA_EMU ? 'A' : 'R')) {
					do_find = false;  /* re-read last message */
					break;
				}
				/* Reply to last message */
				domsg = 0;
				if (!chk_ar(cfg.sub[subnum]->post_ar, &useron, &client)) {
					bputs(text[CantPostOnSub]);
					break;
				}
				if (msg.hdr.attr & MSG_NOREPLY && !sub_op(subnum)) {
					bputs(text[CantReplyToMsg]);
					break;
				}
				FREE_AND_NULL(post);
				postmsg(subnum, WM_NONE, &smb, &msg);
				if (mode & SCAN_TOYOU)
					domsg = 1;
				break;
			case 'B':   /* Skip sub-board */
				if (mode & SCAN_NEW && text[RemoveFromNewScanQ][0] && !noyes(text[RemoveFromNewScanQ]))
					subscan[subnum].cfg &= ~SUB_CFG_NSCAN;
				if (msg.total_hfields)
					smb_freemsgmem(&msg);
				if (post)
					free(post);
				smb_close(&smb);
				smb_stack(&smb, SMB_STACK_POP);
				current_msg = NULL;
				return 0;
			case 'C':   /* Continuous */
				mode |= SCAN_CONT;
				if (smb.curmsg < smb.msgs - 1)
					smb.curmsg++;
				else
					done = 1;
				break;
			case 'D':       /* Delete message on sub-board */
				if (!(msg.hdr.attr & MSG_DELETE) && msg.hdr.type == SMB_MSG_TYPE_POLL
				    && smb_msg_is_from(&msg, cfg.sub[subnum]->misc & SUB_NAME ? useron.name : useron.alias, NET_NONE, NULL)
				    && !(msg.hdr.auxattr & POLL_CLOSED)) {
					if (!noyes("Close Poll")) {
						i = closepoll(&cfg, &smb, msg.hdr.number, cfg.sub[subnum]->misc & SUB_NAME ? useron.name : useron.alias);
						if (i != SMB_SUCCESS)
							errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
						break;
					}
				}
				domsg = 0;
				if (!sub_op(subnum)) {
					if (!(cfg.sub[subnum]->misc & SUB_DEL)) {
						bputs(text[CantDeletePosts]);
						domsg = 0;
						break;
					}
					if (cfg.sub[subnum]->misc & SUB_DELLAST && smb.curmsg != (smb.msgs - 1)) {
						bprintf(text[CantDeleteMsg], smb.curmsg + 1);
						domsg = 0;
						break;
					}
					if (stricmp(cfg.sub[subnum]->misc & SUB_NAME
					    ? useron.name : useron.alias, msg.from)
					    && stricmp(cfg.sub[subnum]->misc & SUB_NAME
					    ? useron.name : useron.alias, msg.to)) {
						bprintf(text[YouDidntPostMsgN], smb.curmsg + 1);
						break;
					}
				}
				if (msg.hdr.attr & MSG_PERMANENT) {
					bprintf(text[CantDeleteMsg], smb.curmsg + 1);
					domsg = 0;
					break;
				}
				if (msg.hdr.type == SMB_MSG_TYPE_POLL)
					SAFEPRINTF(str, text[DeleteTextFileQ], "Poll");
				else
					SAFEPRINTF2(str, text[DeletePostQ], smb.curmsg + 1, msghdr_field(&msg, msg.subj));
				if (!(msg.hdr.attr & MSG_DELETE) && str[0] && noyes(str)) {
					domsg = false;
					break;
				}

				FREE_AND_NULL(post);

				if (msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields = 0;
				msg.idx.offset = 0;
				if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
					if (loadmsg(&msg, msg.idx.number) >= 0) {
						msg.idx.attr ^= MSG_DELETE;
						msg.hdr.attr = msg.idx.attr;
						if ((i = smb_putmsg(&smb, &msg)) != 0)
							errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
						smb_unlockmsghdr(&smb, &msg);
						if (i == 0 && msg.idx.attr & MSG_DELETE) {
							llprintf("P-", "removed post from %s %s"
							            , cfg.grp[cfg.sub[subnum]->grp]->sname, cfg.sub[subnum]->lname);
							term->center(text[Deleted]);
							if (!stricmp(cfg.sub[subnum]->misc & SUB_NAME
							    ? useron.name : useron.alias, msg.from))
								useron.posts = (uint)adjustuserval(&cfg, &useron, USER_POSTS, -1);
						}
					}
					smb_unlocksmbhdr(&smb);
				}
				domsg = 1;
				if (can_view_deleted_msgs(subnum))
					smb.curmsg++;
				if (smb.curmsg >= smb.msgs)
					done = 1;
				break;
			case 'E':   /* edit last post */
				if (!sub_op(subnum)) {
					if (!(cfg.sub[subnum]->misc & SUB_EDIT)) {
						bputs(text[CantEditMsg]);
						domsg = 0;
						break;
					}
					if (cfg.sub[subnum]->misc & SUB_EDITLAST && smb.curmsg != (smb.msgs - 1)) {
						bputs(text[CantEditMsg]);
						domsg = 0;
						break;
					}
					if (stricmp(cfg.sub[subnum]->misc & SUB_NAME
					    ? useron.name : useron.alias, msg.from)) {
						bprintf(text[YouDidntPostMsgN], smb.curmsg + 1);
						domsg = 0;
						break;
					}
				}
				FREE_AND_NULL(post);
				editmsg(&smb, &msg);
				break;
			case 'F':   /* find text in messages */
				domsg = 0;
				mode &= ~SCAN_FIND;   /* turn off find mode */
				if ((i64 = get_start_msgnum(&smb, 1)) < 0)
					break;
				i = (int)i64;
				bputs(text[SearchStringPrompt]);
				if (!getstr(find_buf, 40, K_LINE | K_UPPER | K_EDIT | K_AUTODEL))
					break;
				if (text[DisplaySubjectsOnlyQ][0] && yesno(text[DisplaySubjectsOnlyQ]))
					searchposts(subnum, post, (int)i, smb.msgs, find_buf);
				else {
					smb.curmsg = i;
					find = find_buf;
					mode |= SCAN_FIND;
					domsg = 1;
				}
				break;
			case 'H':   /* Highest ranked messages */
			{
				post_t* ranked = (post_t*)malloc(sizeof(post_t) * smb.msgs);
				domsg = false;
				if (ranked == NULL) {
					errormsg(WHERE, ERR_ALLOC, "messages", smb.msgs);
					break;
				}
				memcpy(ranked, post, sizeof(post_t) * smb.msgs);
				qsort(ranked, smb.msgs, sizeof(post_t), rank_post);
				listmsgs(subnum, 0, ranked, 0, 20, /* reading: */ false);
				free(ranked);
				break;
			}
			case 'I':   /* Sub-board information */
				domsg = 0;
				subinfo(subnum);
				break;
			case 'L':   /* List messages */
				domsg = 0;
				bool invoked;
				exec_mod("list messages", cfg.listmsgs_mod, &invoked, "%s %u", cfg.sub[subnum]->code, mode);
				if (invoked)
					break;
				if ((i64 = get_start_msgnum(&smb, 1)) < 0)
					break;
				i = (int)i64;
				listmsgs(subnum, 0, post, i, smb.msgs);
				clearabort();
				break;
			case 'N':   /* New messages */
				domsg = 0;
				if (!listmsgs(subnum, SCAN_NEW, post, 0, smb.msgs))
					bputs(text[NoMessagesFound]);
				clearabort();
				break;
			case 'M':   /* Reply to last post in mail */
				domsg = 0;
				if (msg.hdr.attr & (MSG_NOREPLY | MSG_ANONYMOUS) && !sub_op(subnum)) {
					bputs(text[CantReplyToMsg]);
					break;
				}
				if (!sub_op(subnum) && msg.hdr.attr & MSG_PRIVATE
				    && stricmp(msg.to, useron.name)
				    && stricmp(msg.to, useron.alias))
					break;
				SAFECOPY(str2, format_text(Regarding
				                           , msghdr_field(&msg, msg.subj)
				                           , timestr(smb_time(msg.hdr.when_written))));
				if (msg.from_net.addr == NULL)
					SAFECOPY_UTF8(str, msg.from);
				else if (msg.from_net.type == NET_FIDO)
					SAFEPRINTF2(str, "%s@%s", msg.from
					            , smb_faddrtoa((faddr_t *)msg.from_net.addr, tmp));
				else if (msg.from_net.type == NET_INTERNET || strchr((char*)msg.from_net.addr, '@') != NULL) {
					if (msg.replyto_net.type == NET_INTERNET)
						SAFECOPY(str, (char *)msg.replyto_net.addr);
					else
						SAFECOPY(str, (char *)msg.from_net.addr);
				}
				else
					SAFEPRINTF2(str, "%s@%s", msg.from, (char *)msg.from_net.addr);
				bputs(text[Email]);
				if (!getstr(str, 60, K_EDIT | K_AUTODEL))
					break;

				FREE_AND_NULL(post);
				quotemsg(&smb, &msg, /* include tails: */ true);
				if (strchr(str, '@') != NULL) {
					if (smb_netaddr_type(str) == NET_INTERNET)
						inetmail(str, msg.subj, WM_QUOTE);
					else    /* FidoNet or QWKnet */
						netmail(str, msg.subj, WM_QUOTE);
				}
				else {
					i = atoi(str);
					if (!i) {
						if (cfg.sub[subnum]->misc & SUB_NAME)
							i = finduserstr(0, USER_NAME, str);
						else
							i = matchuser(&cfg, str, TRUE /* sysop_alias */);
					}
					email(i, str2, msg.subj, WM_QUOTE);
				}
				break;
			case 'P':   /* Post message on sub-board */
				domsg = 0;
				if (!chk_ar(cfg.sub[subnum]->post_ar, &useron, &client))
					bputs(text[CantPostOnSub]);
				else {
					FREE_AND_NULL(post);
					postmsg(subnum, 0, 0);
				}
				break;
			case 'Q':   /* Quit */
				quit = 1;
				done = 1;
				break;
			case 'T':   /* List titles of next ten messages */
				domsg = 0;
				if (!smb.msgs)
					break;
				if (smb.curmsg >= smb.msgs - 1) {
					done = 1;
					break;
				}
				u = smb.curmsg + 11;
				if (u > smb.msgs)
					u = smb.msgs;
				listmsgs(subnum, 0, post, smb.curmsg + 1, u);
				smb.curmsg = u - 1;
				if (subscan[subnum].ptr < post[smb.curmsg].idx.number)
					subscan[subnum].ptr = post[smb.curmsg].idx.number;
				break;
			case 'Y':   /* Your messages */
				domsg = 0;
				if (!showposts_toyou(subnum, post, 0, smb.msgs))
					bputs(text[NoMessagesFound]);
				break;
			case 'U':   /* Your unread messages */
				domsg = 0;
				if (!showposts_toyou(subnum, post, 0, smb.msgs, SCAN_UNREAD))
					bputs(text[NoMessagesFound]);
				break;
			case 'V':   /* Vote in reply to message */
			{
				smbmsg_t    vote;
				const char* notice = NULL;

				if (cfg.sub[subnum]->misc & SUB_NOVOTING) {
					bputs(text[VotingNotAllowed]);
					domsg = false;
					break;
				}

				if (smb_voted_already(&smb, msg.hdr.number
				                      , cfg.sub[subnum]->misc & SUB_NAME ? useron.name : useron.alias, NET_NONE, NULL)) {
					bputs(text[VotedAlready]);
					domsg = false;
					break;
				}

				if (useron.rest & FLAG('V')) {
					bputs(text[R_Voting]);
					domsg = false;
					break;
				}

				if (msg.hdr.auxattr & POLL_CLOSED) {
					bputs(text[CantReplyToMsg]);
					domsg = false;
					break;
				}

				ZERO_VAR(vote);
				if (msg.hdr.type == SMB_MSG_TYPE_POLL) {
					str_list_t answers = NULL;
					for (i = 0; i < msg.total_hfields; i++) {
						if (msg.hfield[i].type == SMB_POLL_ANSWER)
							strListPush(&answers, (char*)msg.hfield_dat[i]);
					}
					SAFEPRINTF(str, text[BallotHdr], msghdr_field(&msg, msg.subj));
					i = mselect(str, answers, msg.hdr.votes ? msg.hdr.votes : 1, text[BallotAnswerFmt]
					            , text[PollAnswerChecked], nulstr, text[BallotVoteWhich]);
					strListFree(&answers);
					if (i <= 0) {
						domsg = false;
						break;
					}
					vote.hdr.votes = i;
					vote.hdr.attr = MSG_VOTE;
					notice = text[PollVoteNotice];
				} else {
					mnemonics(text[VoteMsgUpDownOrQuit]);
					int cmd = getkeys("UDQ", 0);
					if (cmd != 'U' && cmd != 'D')
						break;
					vote.hdr.attr = (cmd == 'U' ? MSG_UPVOTE : MSG_DOWNVOTE);
					notice = text[vote.hdr.attr & MSG_UPVOTE ? MsgUpVoteNotice : MsgDownVoteNotice];
				}
				vote.hdr.thread_back = msg.hdr.number;

				smb_hfield_str(&vote, SENDER, (cfg.sub[subnum]->misc & SUB_NAME) ? useron.name : useron.alias);
				if (msg.id != NULL)
					smb_hfield_str(&vote, RFC822REPLYID, msg.id);

				snprintf(str, sizeof str, "%u", useron.number);
				smb_hfield_str(&vote, SENDEREXT, str);

				/* Security logging */
				msg_client_hfields(&vote, &client);
				smb_hfield_str(&vote, SENDERSERVER, server_host_name());

				if ((i = votemsg(&cfg, &smb, &vote, notice, text[VoteNoticeFmt])) != SMB_SUCCESS)
					errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);

				break;
			}
			case '-':
			case '\b':
				if (thread_mode && msg.hdr.thread_id) {
					for (l = 0; l < 2; l++) {
						for (i = smb.curmsg - 1; i >= 0; i--) {
							smbmsg_t nextmsg;
							memset(&nextmsg, 0, sizeof(nextmsg));
							nextmsg.idx = post[i].idx;
							if (smb_getmsghdr(&smb, &nextmsg) != SMB_SUCCESS)
								continue;
							smb_freemsgmem(&nextmsg);
							if (l == 0 && nextmsg.hdr.thread_id < msg.hdr.thread_id)
								break;
							if (l == 1 && nextmsg.hdr.thread_id != msg.hdr.thread_id)
								break;
						}
						if (i >= 0) {
							smb.curmsg = i;
							break;
						}
					}
					break;
				}
				if (smb.curmsg > 0)
					smb.curmsg--;
				do_find = false;
				break;
			case 'O':   /* Operator commands */
				while (online) {
					if (!(useron.misc & EXPERT))
						menu("sysmscan");
					bputs(text[OperatorPrompt]);
					strcpy(str, "?ADCEHMQTUV");
					if (useron_is_sysop())
						strcat(str, "SP");
					switch (getkeys(str, 0)) {
						case '?':
							if (useron.misc & EXPERT)
								menu("sysmscan");
							continue;
						case 'A':   /* Add comment */
							bputs(text[UeditComment]);
							if (!getstr(str, LEN_TITLE, K_LINE))
								break;
							smb_hfield_str(&msg, SMB_COMMENT, str);
							msg.idx.offset = 0;
							if ((i = smb_updatemsg(&smb, &msg)) != SMB_SUCCESS)
								errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
							break;
						case 'P':   /* Purge user */
							if (noyes(text[AreYouSureQ]))
								break;
							purgeuser(cfg.sub[subnum]->misc & SUB_NAME
							    ? finduserstr(0, USER_NAME, msg.from)
							    : matchuser(&cfg, msg.from, FALSE));
							break;
						case 'C':   /* Change message attributes */
							i = chmsgattr(&msg);
							if (msg.hdr.attr == i)
								break;
							if (msg.total_hfields)
								smb_freemsgmem(&msg);
							msg.total_hfields = 0;
							msg.idx.offset = 0;
							if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
								if (loadmsg(&msg, msg.idx.number) >= 0) {
									msg.hdr.attr = msg.idx.attr = i;
									if ((i = smb_putmsg(&smb, &msg)) != 0)
										errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
									smb_unlockmsghdr(&smb, &msg);
								}
								smb_unlocksmbhdr(&smb);
							}
							break;
						case 'E':   /* edit last post */
							FREE_AND_NULL(post);
							editmsg(&smb, &msg);
							break;
						case 'H':   /* View message header */
							dump_msghdr(&msg);
							domsg = 0;
							break;
						case 'M':   /* Move message */
							domsg = 0;
							FREE_AND_NULL(post);
							if (msg.total_hfields)
								smb_freemsgmem(&msg);
							msg.total_hfields = 0;
							msg.idx.offset = 0;
							if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
								if (loadmsg(&msg, msg.idx.number) < 0) {
									errormsg(WHERE, ERR_READ, smb.file, msg.idx.number);
									break;
								}
								SAFEPRINTF2(str, text[DeletePostQ], smb.curmsg + 1, msghdr_field(&msg, msg.subj));
								if (movemsg(&msg, subnum) && yesno(str)) {
									msg.idx.attr |= MSG_DELETE;
									msg.hdr.attr = msg.idx.attr;
									if ((i = smb_putmsg(&smb, &msg)) != 0)
										errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
								}
								smb_unlockmsghdr(&smb, &msg);
							}
							smb_unlocksmbhdr(&smb);
							break;

						case 'Q':
							break;
						case 'S':   /* Save/Append message to another file */
							bputs(text[FileToWriteTo]);
							{
								char section[128];
								const char* key = "savepath";
								SAFEPRINTF(section, "sub %s", cfg.sub[subnum]->code);
								user_get_property(&cfg, useron.number, section, key, savepath, sizeof(savepath) - 1);
								if (getstr(savepath, sizeof(savepath) - 1, K_EDIT | K_LINE | K_AUTODEL) > 0) {
									if (msgtotxt(&smb, &msg, savepath, /* header: */ true, /* mode: */ GETMSGTXT_ALL))
										user_set_property(&cfg, useron.number, section, key, savepath);
								}
							}
							break;
						case 'T':   /* Twit-list the sender */
							domsg = false;
							if (name_is_twit(&cfg, msg.from)) {
								bprintf("\r\n%s is already twit-listed!\r\n", msg.from);
								break;
							}
							list_twit(&cfg, msg.from, timestr(time(NULL)));
							break;
						case 'U':   /* User edit */
							useredit(cfg.sub[subnum]->misc & SUB_NAME
							    ? finduserstr(0, USER_NAME, msg.from)
							    : matchuser(&cfg, msg.from, TRUE /* sysop_alias */));
							break;
						case 'V':   /* Validate message */
							if (msg.total_hfields)
								smb_freemsgmem(&msg);
							msg.total_hfields = 0;
							msg.idx.offset = 0;
							if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
								if (loadmsg(&msg, msg.idx.number) >= 0) {
									msg.idx.attr |= MSG_VALIDATED;
									msg.hdr.attr = msg.idx.attr;
									if ((i = smb_putmsg(&smb, &msg)) != 0)
										errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
									smb_unlockmsghdr(&smb, &msg);
								}
								smb_unlocksmbhdr(&smb);
							}
							break;
						case 'D':   /* Delete a range of posts */
						{
							domsg = false;
							bprintf("\r\nFirst message to delete [%u]: ", smb.curmsg + 1);
							int  first = getnum(INT_MAX, smb.curmsg + 1);
							if (first < 1 || first > (int)smb.msgs)
								break;
							bprintf(" Last message to delete [%u]: ", smb.msgs);
							int  last = getnum(INT_MAX, smb.msgs);
							if (first > last || last > (int)smb.msgs)
								break;
							uint already = 0;
							uint deleted = 0;
							uint failed = 0;
							if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
								for (int n = first; n <= last; n++) {
									if (post[n - 1].idx.attr & MSG_DELETE) {
										already++;
										continue;   // Already deleted
									}
									if (post[n - 1].idx.attr & MSG_PERMANENT) {
										bprintf(text[CantDeleteMsg], smb.curmsg + 1);
										failed++;
										continue;
									}
									smb_freemsgmem(&msg);
									msg.idx.offset = 0;
									msg.idx.number = post[n - 1].idx.number;
									if (loadmsg(&msg, msg.idx.number) >= SMB_SUCCESS) {
										msg.idx.attr |= MSG_DELETE;
										msg.hdr.attr = msg.idx.attr;
										if ((i = smb_putmsg(&smb, &msg)) != SMB_SUCCESS) {
											errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
											failed++;
										} else
											deleted++;
										smb_unlockmsghdr(&smb, &msg);
									} else {
										bprintf("Error loading message");
										failed++;
									}
								}
								smb_unlocksmbhdr(&smb);
							}
							bprintf("Messages-deleted: %u, Already-deleted: %u, Failed-to-delete: %u\n"
							        , deleted, already, failed);
							break;
						}
						default:
							continue;
					}
					break;
				}
				break;
			case TERM_KEY_HOME: /* Home */
			{
				if (!thread_mode) {
					smb.curmsg = 0;
					term->newline();
					break;
				}
				uint32_t first = smb_first_in_thread(&smb, &msg, NULL);
				if (first <= 0) {
					bputs(text[NoMessagesFound]);
					break;
				}
				i = find_post(&smb, first, post);
				if (i >= 0)
					smb.curmsg = i;
				do_find = false;
				break;
			}
			case TERM_KEY_END:  /* End */
			{
				if (!thread_mode) {
					smb.curmsg = smb.msgs - 1;
					term->newline();
					break;
				}
				uint32_t last = smb_last_in_thread(&smb, &msg);
				if (last <= 0) {
					bputs(text[NoMessagesFound]);
					break;
				}
				i = find_post(&smb, last, post);
				if (i >= 0)
					smb.curmsg = i;
				do_find = false;
				break;
			}
			case ')':   /* Thread forward */
			case TERM_KEY_DOWN: /* down-arrow */
				l = msg.hdr.thread_next;
				if (!l)
					l = msg.hdr.thread_first;
				if (!l) {
					domsg = 0;
					outchar('\a');
					break;
				}
				for (u = 0; u < smb.msgs; u++)
					if (l == post[u].idx.number)
						break;
				if (u < smb.msgs) {
					smb.curmsg = u;
				} else {
					domsg = 0;
					if (thread_mode)
						outchar('\a');
					else
						bputs(text[NoMessagesFound]);
				}
				do_find = false;
				break;
			case TERM_KEY_RIGHT:    /* Right-arrow */
				if (!thread_mode) {
					if (smb.curmsg < smb.msgs - 1)
						smb.curmsg++;
					else
						done = 1;
					term->newline();
					break;
				}
				l = msg.hdr.thread_first;
				if (!l)
					l = msg.hdr.thread_next;
				if (!l) {
					domsg = 0;
					outchar('\a');
					break;
				}
				for (u = 0; u < smb.msgs; u++)
					if (l == post[u].idx.number)
						break;
				if (u < smb.msgs)
					smb.curmsg = u;
				else {
					domsg = 0;
					outchar('\a');
				}
				do_find = false;
				break;
			case TERM_KEY_LEFT: /* left arrow */
				if (!thread_mode) {
					if (smb.curmsg > 0)
						smb.curmsg--;
					term->newline();
					break;
				}
			case '(':   /* Thread backwards */
				if (!msg.hdr.thread_back) {
					domsg = 0;
					outchar('\a');
					break;
				}
				for (u = 0; u < smb.msgs; u++)
					if (msg.hdr.thread_back == post[u].idx.number)
						break;
				if (u < smb.msgs) {
					smb.curmsg = u;
				} else {
					domsg = 0;
					if (thread_mode)
						outchar('\a');
					else
						bputs(text[NoMessagesFound]);
				}
				do_find = false;
				break;
			case TERM_KEY_UP:   /* up arrow */
				if (!msg.hdr.thread_id) {
					domsg = 0;
					bputs(text[NoMessagesFound]);
					break;
				}
				for (i = smb.curmsg - 1; i >= 0; i--) {
					smbmsg_t nextmsg;
					memset(&nextmsg, 0, sizeof(nextmsg));
					nextmsg.idx = post[i].idx;
					if (smb_getmsghdr(&smb, &nextmsg) != SMB_SUCCESS)
						continue;
					smb_freemsgmem(&nextmsg);
					if (nextmsg.hdr.thread_id == msg.hdr.thread_id)
						break;
				}
				if (i < 0)
					break;
				smb.curmsg = i;
				break;
			case '>':   /* Search Title forward */
				for (u = smb.curmsg + 1; u < smb.msgs; u++)
					if (post[u].idx.subj == msg.idx.subj)
						break;
				if (u < smb.msgs)
					smb.curmsg = u;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				do_find = false;
				break;
			case '<':   /* Search Title backward */
				for (i = smb.curmsg - 1; i > -1; i--)
					if (post[i].idx.subj == msg.idx.subj)
						break;
				if (i > -1)
					smb.curmsg = i;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				do_find = false;
				break;
			case '}':   /* Search Author forward */
				strcpy(str, msg.from);
				for (u = smb.curmsg + 1; u < smb.msgs; u++)
					if (post[u].idx.from == msg.idx.from)
						break;
				if (u < smb.msgs)
					smb.curmsg = u;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				do_find = false;
				break;
			case '{':   /* Search Author backward */
				strcpy(str, msg.from);
				for (i = smb.curmsg - 1; i > -1; i--)
					if (post[i].idx.from == msg.idx.from)
						break;
				if (i > -1)
					smb.curmsg = i;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				do_find = false;
				break;
			case ']':   /* Search To User forward */
				strcpy(str, msg.to);
				for (u = smb.curmsg + 1; u < smb.msgs; u++)
					if (post[u].idx.to == msg.idx.to)
						break;
				if (u < smb.msgs)
					smb.curmsg = u;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				do_find = false;
				break;
			case '[':   /* Search To User backward */
				strcpy(str, msg.to);
				for (i = smb.curmsg - 1; i > -1; i--)
					if (post[i].idx.to == msg.idx.to)
						break;
				if (i > -1)
					smb.curmsg = i;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				do_find = false;
				break;
			case 0: /* Carriage return - Next Message/Thread */
			case '+':
				if (thread_mode) {
					for (l = 0; l < 2; l++) {
						for (u = smb.curmsg + 1; u < smb.msgs; u++) {
							smbmsg_t nextmsg;
							memset(&nextmsg, 0, sizeof(nextmsg));
							nextmsg.idx = post[u].idx;
							if (smb_getmsghdr(&smb, &nextmsg) != SMB_SUCCESS)
								continue;
							smb_freemsgmem(&nextmsg);
							if (l == 0 && nextmsg.hdr.thread_id > msg.hdr.thread_id)
								break;
							if (l == 1 && nextmsg.hdr.thread_id != msg.hdr.thread_id)
								break;
						}
						if (u < smb.msgs) {
							smb.curmsg = u;
							break;
						}
					}
					if (l == 2)
						done = 1;
					break;
				}
				if (smb.curmsg < smb.msgs - 1)
					smb.curmsg++;
				else
					done = 1;
				break;
			case '?':
				menu("msgscan");
				domsg = 0;
				break;
		}
		clearabort();
	}
	if (msg.total_hfields)
		smb_freemsgmem(&msg);
	if (post)
		free(post);
	if (!quit
	    && !(org_mode & (SCAN_CONT | SCAN_TOYOU | SCAN_FIND | SCAN_POLLS)) && !(cfg.sub[subnum]->misc & SUB_PONLY)
	    && reads && user_can_post(&cfg, subnum, &useron, &client, /* reason: */ NULL) && text[Post][0]) {
		SAFEPRINTF2(str, text[Post], cfg.grp[cfg.sub[subnum]->grp]->sname
		            , cfg.sub[subnum]->lname);
		if (!noyes(str))
			postmsg(subnum, 0, 0);
	}
	if (!(org_mode & (SCAN_CONT | SCAN_TOYOU | SCAN_FIND | SCAN_POLLS))
	    && !(subscan[subnum].cfg & SUB_CFG_NSCAN) && !noyes(text[AddSubToNewScanQ]))
		subscan[subnum].cfg |= SUB_CFG_NSCAN;
	smb_close(&smb);
	smb_stack(&smb, SMB_STACK_POP);
	current_msg = NULL;
	return quit;
}

/****************************************************************************/
/* This function lists all messages in sub-board							*/
/* Displays msg header information only (no body text)						*/
/* Returns number of messages found/displayed.                              */
/****************************************************************************/
int sbbs_t::listsub(int subnum, int mode, int start, const char* search)
{
	int      i;
	uint32_t posts;
	uint32_t total = 0;
	int      displayed = 0;
	int      lp_mode = LP_BYSELF;
	post_t * post;

	if ((mode & SCAN_INDEX)) {
		bool invoked;
		i = exec_mod("list messages", cfg.listmsgs_mod, &invoked, "%s %u", cfg.sub[subnum]->code, mode);
		if (invoked)
			return i;
	}

	if ((i = smb_stack(&smb, SMB_STACK_PUSH)) != 0) {
		errormsg(WHERE, ERR_OPEN, cfg.sub[subnum]->code, i);
		return 0;
	}
	SAFEPRINTF2(smb.file, "%s%s", cfg.sub[subnum]->data_dir, cfg.sub[subnum]->code);
	smb.retry_time = cfg.smb_retry_time;
	smb.subnum = subnum;
	if ((i = smb_open(&smb)) != 0) {
		errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
		smb_stack(&smb, SMB_STACK_POP);
		return 0;
	}
	if (!(mode & SCAN_TOYOU))
		lp_mode |= LP_OTHERS;
	if (mode & SCAN_UNREAD)
		lp_mode |= LP_UNREAD;
	if (!(cfg.sub[subnum]->misc & SUB_NOVOTING))
		lp_mode |= LP_POLLS;
	post = loadposts(&posts, subnum, 0, lp_mode, NULL, &total);
	char grp[128];
	char sub[128];
	uint ugrp = getusrgrp(subnum);
	uint usub = getusrsub(subnum);
	SAFEPRINTF4(grp, "%*s[%u] %s", DEC_DIGITS(usrgrps) - DEC_DIGITS(ugrp), "", ugrp, cfg.grp[cfg.sub[subnum]->grp]->sname);
	SAFEPRINTF4(sub, "%*s[%u] %s", DEC_DIGITS(usrsubs[ugrp - 1]) - DEC_DIGITS(usub), "", usub, cfg.sub[subnum]->lname);
	bprintf(text[SearchSubFmt], grp, sub, total);
	if (posts) {
		if (mode & SCAN_FIND)
			displayed = searchposts(subnum, post, start, posts, search);
		else
			displayed = listmsgs(subnum, mode, post, start, posts, /* reading: */ false);
		free(post);
	}
	smb_close(&smb);

	smb_stack(&smb, SMB_STACK_POP);

	return displayed;
}

/****************************************************************************/
/* Will search the messages pointed to by 'msg' for the occurrence of the   */
/* string 'search' and display any messages (number of message, author and  */
/* title). 'msgs' is the total number of valid messages.                    */
/* Returns number of messages found.                                        */
/****************************************************************************/
int sbbs_t::searchposts(int subnum, post_t *post, int start, int posts
                        , const char *search)
{
	char*    buf;
	int      l, found = 0;
	smbmsg_t msg;

	msg.total_hfields = 0;
	for (l = start; l < posts && !msgabort(); l++) {
		msg.idx.offset = post[l].idx.offset;
		if (loadmsg(&msg, post[l].idx.number) < 0)
			continue;
		smb_unlockmsghdr(&smb, &msg);
		buf = smb_getmsgtxt(&smb, &msg, GETMSGTXT_ALL);
		if (!buf) {
			smb_freemsgmem(&msg);
			continue;
		}
		if (strcasestr(buf, search) != NULL || strcasestr(msg.subj, search) != NULL
		    || (msg.tags != NULL && strcasestr(msg.tags, search) != NULL)) {
			if (!found)
				bputs(text[MailOnSystemLstHdr]);
			bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
			        , msghdr_text(&msg, SubMsgLstFmt), l + 1
			        , (msg.hdr.attr & MSG_ANONYMOUS) && !sub_op(subnum) ? text[Anonymous]
			    : msg.from
			        , msg.to
			        , msg_listing_flag(subnum, &msg, &post[l])
			        , msg.subj);
			found++;
		}
		free(buf);
		smb_freemsgmem(&msg);
	}

	return found;
}

/****************************************************************************/
/* Will search the messages pointed to by 'msg' for message to the user on  */
/* Returns number of messages found.                                        */
/****************************************************************************/
int sbbs_t::showposts_toyou(int subnum, post_t *post, uint start, int posts, int mode)
{
	char     str[128];
	ushort   namecrc, aliascrc, sysop;
	int      l, found;
	smbmsg_t msg;

	strcpy(str, useron.alias);
	strlwr(str);
	aliascrc = crc16(str, 0);
	strcpy(str, useron.name);
	strlwr(str);
	namecrc = crc16(str, 0);
	sysop = crc16("sysop", 0);
	msg.total_hfields = 0;
	for (l = start, found = 0; l < posts && !msgabort(); l++) {

		if ((useron.number != 1 || post[l].idx.to != sysop)
		    && post[l].idx.to != aliascrc && post[l].idx.to != namecrc)
			continue;

		if ((post[l].idx.attr & MSG_READ) && (mode & SCAN_UNREAD)) /* Skip read messages */
			continue;

		if (msg.total_hfields)
			smb_freemsgmem(&msg);
		msg.total_hfields = 0;
		msg.idx.offset = post[l].idx.offset;
		if (loadmsg(&msg, post[l].idx.number) < 0)
			continue;
		smb_unlockmsghdr(&smb, &msg);
		if ((useron.number == 1 && !stricmp(msg.to, "sysop") && !msg.from_net.type)
		    || !stricmp(msg.to, useron.alias) || !stricmp(msg.to, useron.name)) {
			if (!found)
				bputs(text[MailOnSystemLstHdr]);
			found++;
			bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
			        , msghdr_text(&msg, SubMsgLstFmt), l + 1
			        , (msg.hdr.attr & MSG_ANONYMOUS) && !useron_is_sysop()
			    ? text[Anonymous] : msg.from
			        , msg.to
			        , msg_listing_flag(subnum, &msg, &post[l])
			        , msg.subj);
		}
	}

	if (msg.total_hfields)
		smb_freemsgmem(&msg);

	return found;
}
