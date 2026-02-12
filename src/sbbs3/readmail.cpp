/* Synchronet private mail reading function */

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

static char mail_listing_flag(smbmsg_t* msg)
{
	if (msg->hdr.attr & MSG_DELETE)
		return '-';
	if (msg->hdr.attr & MSG_SPAM)
		return 'S';
	if (msg->hdr.attr & MSG_REPLIED)
		return 'R';
	if (msg->hdr.attr & MSG_READ)
		return ' ';
	if (msg->hdr.attr & MSG_PERMANENT)
		return 'p';
	if (msg->hdr.attr & MSG_LOCKED)
		return 'L';
	if (msg->hdr.attr & MSG_KILLREAD)
		return 'K';
	if (msg->hdr.attr & MSG_NOREPLY)
		return '#';
	if (msg->from_net.type || msg->to_net.type)
		return 'N';
	if (msg->hdr.attr & MSG_ANONYMOUS)
		return 'A';
	return '*';
}

static uint count_msgs(mail_t* mail, uint total)
{
	uint count = 0;

	for (uint i = 0; i < total; ++i) {
		if (!(mail[i].attr & MSG_DELETE))
			++count;
	}
	return count;
}

static const char* msg_to(smbmsg_t* msg)
{
	if(msg->to != nullptr && *msg->to != '\0')
		return msg->to;
	if(msg->forward_path != nullptr)
		return msg->forward_path;
	return "";
}

/****************************************************************************/
/* Reads mail waiting for usernumber.                                       */
/****************************************************************************/
int sbbs_t::readmail(uint usernumber, int which, int lm_mode, bool listmsgs)
{
	char     str[256], str2[256], done = 0, domsg = 1;
	char     tmp[512];
	char     savepath[MAX_PATH + 1]{};
	int      i;
	int64_t  i64;
	uint32_t u, v;
	int      mismatches = 0, act;
	int      unum;
	int      l, last_mode;
	uint     last;
	bool     replied;
	mail_t * mail;
	smbmsg_t msg;
	char     search_str[128] = "";
	const char* property_section = "mail";

	if (which == MAIL_SENT)
		act = NODE_RSML;
	else if (which == MAIL_ALL)
		act = NODE_SYSP;
	else
		act = NODE_RMAL;
	action = act;

	lm_mode &= ~(LM_NOSPAM | LM_SPAMONLY | LM_INCDEL);
	if (cfg.sys_misc & SM_SYSVDELM && (useron_is_sysop() || cfg.sys_misc & SM_USRVDELM))
		lm_mode |= LM_INCDEL;

	bool invoked;
	int result = exec_mod("read mail", cfg.readmail_mod, &invoked, "%d %u %u", which, usernumber, lm_mode);
	if (invoked)
		return result;

	if (which == MAIL_SENT && useron.rest & FLAG('K')) {
		bputs(text[R_ReadSentMail]);
		return lm_mode;
	}

	msg.total_hfields = 0;            /* init to NULL, cause not allocated yet */

	if ((i = smb_stack(&smb, SMB_STACK_PUSH)) != 0) {
		errormsg(WHERE, ERR_OPEN, "MAIL", i);
		return lm_mode;
	}
	SAFEPRINTF(smb.file, "%smail", cfg.data_dir);
	smb.retry_time = cfg.smb_retry_time;
	smb.subnum = INVALID_SUB;
	if ((i = smb_open(&smb)) != 0) {
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
		return lm_mode;
	}

	bool reload = false;
	mail = loadmail(&smb, &smb.msgs, usernumber, which, lm_mode);
	last_mode = lm_mode;
	if (!smb.msgs || mail == nullptr) {
		if (which == MAIL_SENT)
			bputs(text[NoMailSent]);
		else if (which == MAIL_ALL)
			bputs(text[NoMailOnSystem]);
		else
			bprintf(text[NoMailWaiting], lm_mode & LM_UNREAD ? "un-read mail" : "mail");
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);
		free(mail);
		return lm_mode;
	}

	last = smb.status.last_msg;

	const char* order = (lm_mode & LM_REVERSE) ? "newest" : "oldest";
	if (smb.msgs > 1 && listmsgs) {
		if (which == MAIL_SENT)
			bprintf(text[MailSentLstHdr], order);
		else if (which == MAIL_ALL)
			bprintf(text[MailOnSystemLstHdr], order);
		else
			bprintf(text[MailWaitingLstHdr], order);

		for (smb.curmsg = 0; smb.curmsg < smb.msgs; smb.curmsg++) {
			if (msg.total_hfields)
				smb_freemsgmem(&msg);
			msg.total_hfields = 0;
			msg.idx.offset = mail[smb.curmsg].offset;
			if (loadmsg(&msg, mail[smb.curmsg].number) < 1)
				continue;
			smb_unlockmsghdr(&smb, &msg);
			bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
			        , msghdr_text(&msg, MailWaitingLstFmt), smb.curmsg + 1
			        , which == MAIL_SENT ? msg_to(&msg) : (msg.hdr.attr & MSG_ANONYMOUS) && !useron_is_sysop() ? text[Anonymous] : msg.from
			        , mail_listing_flag(&msg)
			        , msg.subj);
			smb_freemsgmem(&msg);
			msg.total_hfields = 0;
			if (msgabort())
				break;
		}
		domsg = 0;
		if (smb.curmsg >= smb.msgs)
			smb.curmsg = 0;

		sync();
		clearabort();
	}
	else {
		smb.curmsg = 0;
		if (which == MAIL_ALL)
			domsg = 0;
	}
	if (which == MAIL_SENT) {
		logline("E", "read sent mail");
	} else if (which == MAIL_ALL) {
		logline("S+", "read all mail");
	} else {
		logline("E", "read mail");
	}
	const char* menu_file = (which == MAIL_ALL ? "allmail" : which == MAIL_SENT ? "sentmail" : "mailread");
	if (useron.misc & RIP)
		menu(menu_file);

	bool wide = user_get_bool_property(&cfg, useron.number, property_section, "wide", false);

	current_msg = &msg;   /* For MSG_* @-codes and bbs.msg_* property values */
	while (online && !done) {
		action = act;

		if (msg.total_hfields)
			smb_freemsgmem(&msg);
		msg.total_hfields = 0;

		msg.idx.offset = mail[smb.curmsg].offset;
		msg.idx.number = mail[smb.curmsg].number;
		msg.idx.to = mail[smb.curmsg].to;
		msg.idx.from = mail[smb.curmsg].from;
		msg.idx.subj = mail[smb.curmsg].subj;

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

		if (smb.status.last_msg != last || lm_mode != last_mode || reload) {   /* New messages */
			reload = false;
			last = smb.status.last_msg;
			free(mail);
			order = (lm_mode & LM_REVERSE) ? "newest" : "oldest";
			mail = loadmail(&smb, &smb.msgs, usernumber, which, lm_mode);   /* So re-load */
			if (!smb.msgs)
				break;
			if (lm_mode != last_mode)
				smb.curmsg = 0;
			else {
				for (smb.curmsg = 0; smb.curmsg < smb.msgs; smb.curmsg++)
					if (mail[smb.curmsg].number == msg.idx.number)
						break;
				if (smb.curmsg >= smb.msgs)
					smb.curmsg = (smb.msgs - 1);
			}
			last_mode = lm_mode;
			continue;
		}

		if (loadmsg(&msg, mail[smb.curmsg].number) < 0) { /* Message header gone */
			if (mismatches > 5) {  /* We can't do this too many times in a row */
				errormsg(WHERE, ERR_CHK, "message number", mail[smb.curmsg].number);
				break;
			}
			free(mail);
			mail = loadmail(&smb, &smb.msgs, usernumber, which, lm_mode);
			if (!smb.msgs)
				break;
			last_mode = lm_mode;
			if (smb.curmsg > (smb.msgs - 1))
				smb.curmsg = (smb.msgs - 1);
			mismatches++;
			continue;
		}
		smb_unlockmsghdr(&smb, &msg);
		msg.idx.attr = msg.hdr.attr;

		mismatches = 0;

		if (domsg && !(sys_status & SS_ABORT)) {

			int pmode = (msg.from_ext && msg.idx.from == 1 && !msg.from_net.type) ? 0 : P_NOATCODES;
			if (!wide)
				pmode |= P_80COLS;
			if (!show_msg(&smb, &msg, pmode))
				errormsg(WHERE, "showing", "mail message", msg.hdr.number, smb.last_error);
			download_msg_attachments(&smb, &msg, which == MAIL_YOUR);
			if (which == MAIL_YOUR && !(msg.hdr.attr & MSG_READ)) {
				mail[smb.curmsg].attr |= MSG_READ;
				if (thisnode.status == NODE_INUSE)
					telluser(&msg);
				if (msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields = 0;
				msg.idx.offset = 0;                       /* Search by number */
				if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
					if (loadmsg(&msg, msg.idx.number) >= 0) {
						msg.hdr.attr |= MSG_READ;
						if (msg.hdr.attr & MSG_KILLREAD)
							msg.hdr.attr |= MSG_DELETE;
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
		}
		else
			domsg = 1;

		sync();
		if (which == MAIL_SENT)
			bprintf(P_ATCODES, text[ReadingSentMail], smb.curmsg + 1, smb.msgs);
		else if (which == MAIL_ALL)
			bprintf(P_ATCODES, text[ReadingAllMail], smb.curmsg + 1, smb.msgs);
		else
			bprintf(P_ATCODES, text[ReadingMail], smb.curmsg + 1, smb.msgs);
		snprintf(str, sizeof str, "ADFLNQRTW?<>[]{}()-+/!%c%c%c%c"
		         , TERM_KEY_LEFT
		         , TERM_KEY_RIGHT
		         , TERM_KEY_HOME
		         , TERM_KEY_END);
		if (useron_is_sysop())
			strcat(str, "CUSPH");
		if (which == MAIL_YOUR)
			strcat(str, "K");   // kill all (visible)
		else
			strcat(str, "E");   // edit msg
		if (which != MAIL_SENT)
			strcat(str, "V");   // View SPAM toggle
		l = getkeys(str, smb.msgs);
		if (l & 0x80000000L) {
			if (l == -1)   /* ctrl-c */
				break;
			smb.curmsg = (l & ~0x80000000L) - 1;
			continue;
		}
		switch (l) {
			case '!':
				lm_mode ^= LM_REVERSE;
				domsg = 0;
				break;
			case 'W':
				wide = !wide;
				user_set_bool_property(&cfg, useron.number, property_section, "wide", wide);
				bprintf(text[WideModeIsNow], wide ? text[On] : text[Off]);
				break;
			case 'A':   /* Auto-reply to last piece */
			case 'R':
				if (l == (cfg.sys_misc & SM_RA_EMU ? 'A' : 'R'))  /* re-read last message */
					break;

				if (which == MAIL_SENT)
					break;
				if ((msg.hdr.attr & (MSG_NOREPLY | MSG_ANONYMOUS)) && !useron_is_sysop()) {
					bputs(text[CantReplyToMsg]);
					break;
				}

				if (msg.from_net.addr == NULL)
					SAFECOPY(str, msg.from);
				else if (msg.from_net.type == NET_FIDO)    /* FidoNet type */
					SAFEPRINTF2(str, "%s@%s", msg.from
					            , smb_faddrtoa((faddr_t *)msg.from_net.addr, tmp));
				else if (msg.from_net.type == NET_INTERNET || strchr((char*)msg.from_net.addr, '@') != NULL) {
					if (msg.replyto_net.type == NET_INTERNET)
						SAFECOPY(str, (char *)msg.replyto_net.addr);
					else
						SAFECOPY(str, (char *)msg.from_net.addr);
				} else
					SAFEPRINTF2(str, "%s@%s", msg.from, (char*)msg.from_net.addr);

				SAFECOPY(str2, str);

				bputs(text[Email]);
				if (!getstr(str, 64, K_EDIT | K_AUTODEL))
					break;
				msg.hdr.number = msg.idx.number;
				smb_getmsgidx(&smb, &msg);

				enum text text_num;
				if (!stricmp(str2, str))      /* Reply to sender */
					text_num = Regarding;
				else                        /* Reply to other */
					text_num = RegardingByOn;
				SAFECOPY(str2, format_text(text_num, msghdr_field(&msg, msg.subj), msghdr_field(&msg, msg.from, tmp)
				                           , timestr(smb_time(msg.hdr.when_written))));

				if (netmail_addr_is_supported(&cfg, str)) {                             /* name @addr */
					replied = netmail(str, msg.subj, WM_NONE, &smb, &msg);
					SAFEPRINTF(str2, text[DeleteMailQ], msghdr_field(&msg, msg.from));
				}
				else {
					if (text_num == Regarding && msg.idx.from)
						replied = email(msg.idx.from, str2, msg.subj, WM_NONE, &smb, &msg);
					else if (!stricmp(str, "SYSOP"))
						replied = email(1, str2, msg.subj, WM_NONE, &smb, &msg);
					else if ((i = finduser(str)) != 0)
						replied = email(i, str2, msg.subj, WM_NONE, &smb, &msg);
					else
						replied = false;
					SAFEPRINTF(str2, text[DeleteMailQ], msghdr_field(&msg, msg.from));
				}

				if (replied == true && !(msg.hdr.attr & MSG_REPLIED)) {
					if (msg.total_hfields)
						smb_freemsgmem(&msg);
					msg.total_hfields = 0;
					msg.idx.offset = 0;
					if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
						if (loadmsg(&msg, msg.idx.number) >= 0) {
							msg.hdr.attr |= MSG_REPLIED;
							msg.idx.attr = msg.hdr.attr;
							if ((i = smb_putmsg(&smb, &msg)) != 0)
								errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
							smb_unlockmsghdr(&smb, &msg);
						}
						smb_unlocksmbhdr(&smb);
					}
				}

				if (msg.hdr.attr & MSG_DELETE || noyes(str2)) {
					if (smb.curmsg < smb.msgs - 1)
						smb.curmsg++;
					else
						done = 1;
					break;
				}
			/* Case 'D': must follow! */
			case 'D':   /* Delete last piece (toggle) */
				if (msg.hdr.attr & MSG_PERMANENT) {
					bprintf(text[CantDeleteMsg], smb.curmsg + 1);
					domsg = 0;
					break;
				}
				if (msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields = 0;
				msg.idx.offset = 0;
				if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
					if (loadmsg(&msg, msg.idx.number) >= 0) {
						msg.hdr.attr ^= MSG_DELETE;
						msg.idx.attr = msg.hdr.attr;
						mail[smb.curmsg].attr = msg.hdr.attr;
						if ((i = smb_putmsg(&smb, &msg)) != 0)
							errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
						smb_unlockmsghdr(&smb, &msg);
					}
					smb_unlocksmbhdr(&smb);
				}
				if (smb.curmsg < smb.msgs - 1)
					smb.curmsg++;
				else
					done = 1;
				break;
			case 'K':   /* Kill All Mail */
			{
				domsg = false;
				uint count = count_msgs(mail, smb.msgs);
				if (!count) {
					bputs(text[NoMessagesFound]);
					break;
				}
				snprintf(tmp, sizeof tmp, "%s (%u %s)", text[All], count, text[E_Mails]);
				snprintf(str, sizeof str, text[DeleteMailQ], tmp);
				if (!noyes(str)) {
					bprintf(P_ATCODES, text[DeletedNumberItems], delallmail(usernumber, which, /* permanent: */ false, lm_mode), text[E_Mails]);
					reload = true;
				}
				break;
			}
			case 'F':  /* Forward last piece */
				domsg = 0;
				bputs(text[ForwardMailTo]);
				if (!getstr(str, sizeof(str) - 1, K_TRIM))
					break;
				smb_getmsgidx(&smb, &msg);
				if (!forwardmsg(&smb, &msg, str))
					break;
				domsg = 1;
				if (smb.curmsg < smb.msgs - 1)
					smb.curmsg++;
				else
					done = 1;
				if (msg.hdr.attr & (MSG_PERMANENT | MSG_DELETE))
					break;
				SAFEPRINTF(str2, text[DeleteMailQ], msghdr_field(&msg, msg.from));
				if (!yesno(str2))
					break;
				smb_freemsgmem(&msg);
				msg.idx.offset = 0;
				if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
					if (loadmsg(&msg, msg.idx.number) >= 0) {
						msg.hdr.attr |= MSG_DELETE;
						msg.idx.attr = msg.hdr.attr;
						mail[smb.curmsg].attr = msg.hdr.attr;
						if ((i = smb_putmsg(&smb, &msg)) != 0)
							errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
						smb_unlockmsghdr(&smb, &msg);
					}
					smb_unlocksmbhdr(&smb);
				}
				break;
			case 'H':
				domsg = 0;
				dump_msghdr(&msg);
				break;
			case 'L':     /* List mail */
				domsg = 0;
				bool invoked;
				exec_mod("list messages", cfg.listmsgs_mod, &invoked, "mail %d %u %u", which, usernumber, lm_mode);
				if(invoked)
					break;

				i64 = get_start_msgnum(&smb, 1);
				if (i64 < 0)
					break;
				i = (int)i64;
				if (which == MAIL_SENT)
					bprintf(text[MailSentLstHdr], order);
				else if (which == MAIL_ALL)
					bprintf(text[MailOnSystemLstHdr], order);
				else
					bprintf(text[MailWaitingLstHdr], order);
				for (u = i; u < smb.msgs && !msgabort(); u++) {
					if (msg.total_hfields)
						smb_freemsgmem(&msg);
					msg.total_hfields = 0;
					msg.idx.offset = mail[u].offset;
					if (loadmsg(&msg, mail[u].number) < 0)
						continue;
					smb_unlockmsghdr(&smb, &msg);
					if (which == MAIL_ALL)
						bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
						        , msghdr_text(&msg, MailOnSystemLstFmt)
						        , u + 1, msg.from, msg_to(&msg)
						        , mail_listing_flag(&msg)
						        , msg.subj);
					else
						bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
						        , msghdr_text(&msg, MailWaitingLstFmt), u + 1
						        , which == MAIL_SENT ? msg_to(&msg) : (msg.hdr.attr & MSG_ANONYMOUS) && !useron_is_sysop() ? text[Anonymous] : msg.from
						        , mail_listing_flag(&msg)
						        , msg.subj);
					smb_freemsgmem(&msg);
					msg.total_hfields = 0;
				}
				break;
			case 'Q':
				done = 1;
				break;
			case 'C':   /* Change attributes of last piece */
				i = chmsgattr(&msg);
				if (msg.hdr.attr == i)
					break;
				if (msg.total_hfields)
					smb_freemsgmem(&msg);
				msg.total_hfields = 0;
				msg.idx.offset = 0;
				if (smb_locksmbhdr(&smb) == SMB_SUCCESS) { /* Lock the entire base */
					if (loadmsg(&msg, msg.idx.number) >= 0) {
						msg.hdr.attr = msg.idx.attr = (ushort)i;
						if ((i = smb_putmsg(&smb, &msg)) != 0)
							errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
						smb_unlockmsghdr(&smb, &msg);
					}
					smb_unlocksmbhdr(&smb);
				}
				break;
			case '>':
				for (u = smb.curmsg + 1; u < smb.msgs; u++)
					if (mail[u].subj == msg.idx.subj)
						break;
				if (u < smb.msgs)
					smb.curmsg = u;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				break;
			case '<':   /* Search Title backward */
				for (i = smb.curmsg - 1; i > -1; i--)
					if (mail[i].subj == msg.idx.subj)
						break;
				if (i > -1)
					smb.curmsg = i;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				break;
			case ')':
			case '}':   /* Search Author forward */
				for (u = smb.curmsg + 1; u < smb.msgs; u++) {
					if (msg.idx.from == 0) {
						smbmsg_t piece{};
						piece.idx.offset = mail[u].offset;
						if (loadmsg(&piece, mail[u].number) < 0)
							continue;
						smb_unlockmsghdr(&smb, &piece);
						bool match = stricmp(piece.from, msg.from) == 0;
						smb_freemsgmem(&piece);
						if (match)
							break;
						continue;
					}
					if (mail[u].from == msg.idx.from)
						break;
				}
				if (u < smb.msgs)
					smb.curmsg = u;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				break;
			case 'N':   /* Got to next un-read message */
				for (u = smb.curmsg + 1; u < smb.msgs; u++)
					if (!(mail[u].attr & MSG_READ))
						break;
				if (u < smb.msgs)
					smb.curmsg = u;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				break;
			case '(':
			case '{':   /* Search Author backward */
			{
				uint n = smb.curmsg;
				if (smb.curmsg > 0) {
					for (u = smb.curmsg - 1; n == smb.curmsg && (int)u >= 0; u--) {
						if (msg.idx.from == 0) {
							smbmsg_t piece{};
							piece.idx.offset = mail[u].offset;
							if (loadmsg(&piece, mail[u].number) < 0)
								continue;
							smb_unlockmsghdr(&smb, &piece);
							bool match = stricmp(piece.from, msg.from) == 0;
							smb_freemsgmem(&piece);
							if (match)
								smb.curmsg = u;
						}
						else if (mail[u].from == msg.idx.from) {
							smb.curmsg = u;
						}
					}
				}
				if (n == smb.curmsg) {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				break;
			}
			case ']':   /* Search To User forward */
				for (u = smb.curmsg + 1; u < smb.msgs; u++)
					if (mail[u].to == msg.idx.to)
						break;
				if (u < smb.msgs)
					smb.curmsg = u;
				else {
					domsg = 0;
					bputs(text[NoMessagesFound]);
				}
				break;
			case '[':   /* Search To User backward */
				if (smb.curmsg > 0) {
					for (u = smb.curmsg - 1;; u--) {
						if (mail[u].to == msg.idx.to) {
							smb.curmsg = u;
							break;
						}
						if (u == 0) {
							domsg = 0;
							bputs(text[NoMessagesFound]);
							break;
						}
					}
				}
				break;
			case TERM_KEY_HOME:
				smb.curmsg = 0;
				term->newline();
				break;
			case TERM_KEY_END:
				smb.curmsg = smb.msgs - 1;
				term->newline();
				break;
			case TERM_KEY_RIGHT:
				term->newline();
			// fall-through
			case 0:
			case '+':
				if (smb.curmsg < smb.msgs - 1)
					smb.curmsg++;
				else
					done = 1;
				break;
			case TERM_KEY_LEFT:
				term->newline();
			// fall-through
			case '-':
				if (smb.curmsg > 0)
					smb.curmsg--;
				break;
			case 'S':
				domsg = 0;
				/*
				            if(!yesno(text[SaveMsgToFile]))
				            	break;
				*/
				bputs(text[FileToWriteTo]);
				{
					const char* key = "savepath";
					user_get_property(&cfg, useron.number, property_section, key, savepath, sizeof(savepath) - 1);
					if (getstr(savepath, sizeof(savepath) - 1, K_EDIT | K_LINE | K_AUTODEL) > 0) {
						if (msgtotxt(&smb, &msg, savepath, /* header: */ true, /* mode: */ GETMSGTXT_ALL))
							user_set_property(&cfg, useron.number, property_section, key, savepath);
					}
				}
				break;
			case 'E':
				editmsg(&smb, &msg);
				break;
			case 'T':
				domsg = 0;
				u = smb.curmsg;
				if (u)
					u++;
				v = u + 10;
				if (v > smb.msgs)
					v = smb.msgs;

				if (which == MAIL_SENT)
					bprintf(text[MailSentLstHdr], order);
				else if (which == MAIL_ALL)
					bprintf(text[MailOnSystemLstHdr], order);
				else
					bprintf(text[MailWaitingLstHdr], order);
				for (; u < v; u++) {
					if (msg.total_hfields)
						smb_freemsgmem(&msg);
					msg.total_hfields = 0;
					msg.idx.offset = mail[u].offset;
					if (loadmsg(&msg, mail[u].number) < 0)
						continue;
					smb_unlockmsghdr(&smb, &msg);
					if (which == MAIL_ALL)
						bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
						        , msghdr_text(&msg, MailOnSystemLstFmt)
						        , u + 1, msg.from, msg_to(&msg)
						        , mail_listing_flag(&msg)
						        , msg.subj);
					else
						bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
						        , msghdr_text(&msg, MailWaitingLstFmt), u + 1
						        , which == MAIL_SENT ? msg_to(&msg) : (msg.hdr.attr & MSG_ANONYMOUS) && !useron_is_sysop() ? text[Anonymous] : msg.from
						        , mail_listing_flag(&msg)
						        , msg.subj);
					smb_freemsgmem(&msg);
					msg.total_hfields = 0;
				}
				smb.curmsg = (u - 1);
				break;
			case 'U':   /* user edit */
				msg.hdr.number = msg.idx.number;
				smb_getmsgidx(&smb, &msg);
				unum = msg.idx.from;
				if (unum == 0)
					unum = matchuser(&cfg, msg.from, /*sysop_alias: */ FALSE);
				if (unum == 0 && which != MAIL_YOUR)
					unum = msg.idx.to;
				if (unum == 0 || unum > lastuser(&cfg)) {
					bputs(text[UnknownUser]);
					domsg = false;
				} else
					useredit(unum);
				break;
#if 0
			case 'U':   /* View Unread-Only (toggle) */
			{
				domsg = false;
				if (!(lm_mode & LM_UNREAD)) {
					if (getmail(&cfg, usernumber, /* Sent: */ FALSE, /* attr: */ 0)
					    == getmail(&cfg, usernumber, /* Sent: */ FALSE, /* attr: */ MSG_READ)) {
						bprintf(text[NoMailWaiting], "un-read mail");
						break;
					}
				}
				lm_mode ^= LM_UNREAD;
				bprintf("%s: %s"
				        , text[DisplayUnreadMessagesOnlyQ]
				        , (lm_mode & LM_UNREAD) ? text[On] : text[Off]);
				term->newline();
				break;
			}
#endif
			case 'V':   /* View SPAM (toggle) */
			{
				domsg = false;
				int spam = getmail(&cfg, usernumber, /* Sent: */ FALSE, /* attr: */ MSG_SPAM);
				if (!spam) {
					bprintf(text[NoMailWaiting], "SPAM");
					break;
				}
				if (spam >= getmail(&cfg, usernumber, /* Sent: */ FALSE, /* attr: */ 0)) {
					bprintf(text[NoMailWaiting], "HAM");
					break;
				}
				bputs(text[SPAMVisibilityIsNow]);
				switch (lm_mode & (LM_SPAMONLY | LM_NOSPAM)) {
					case 0:
						lm_mode |= LM_NOSPAM;
						bputs(text[Off]);
						break;
					case LM_NOSPAM:
						lm_mode ^= (LM_SPAMONLY | LM_NOSPAM);
						bputs(text[Only]);
						break;
					case LM_SPAMONLY:
						lm_mode &= ~LM_SPAMONLY;
						bputs(text[On]);
						break;
				}
				term->newline();
				break;
			}
			case 'P':   /* Purge author and all mail to/from */
				if (noyes(text[UeditDeleteQ]))
					break;
				msg.hdr.number = msg.idx.number;
				smb_getmsgidx(&smb, &msg);
				if ((which == MAIL_SENT ? msg.idx.to : msg.idx.from) == 0) {
					bputs(text[UnknownUser]);
					domsg = false;
				} else {
					purgeuser(msg.idx.from);
					if (smb.curmsg < smb.msgs - 1)
						smb.curmsg++;
				}
				break;
			case '/':
				domsg = false;
				int64_t i64;
				if ((i64 = get_start_msgnum(&smb)) < 0)
					break;
				bputs(text[SearchStringPrompt]);
				if (!getstr(search_str, 40, K_LINE | K_UPPER | K_EDIT | K_AUTODEL))
					break;
				searchmail(mail, (int)i64, smb.msgs, which, search_str, order);
				break;
			case '?':
				menu(menu_file);
				if (useron_is_sysop() && (which == MAIL_SENT || which == MAIL_YOUR))
					menu("sysmailr");   /* Sysop Mail Read */
				domsg = 0;
				break;

		}
	}

	if (msg.total_hfields)
		smb_freemsgmem(&msg);

	free(mail);

	/***************************************/
	/* Delete messages marked for deletion */
	/***************************************/

	if (cfg.sys_misc & SM_DELEMAIL) {
		if ((i = smb_lock(&smb)) != SMB_SUCCESS)             /* Lock the base, so nobody */
			errormsg(WHERE, ERR_LOCK, smb.file, i, smb.last_error); /* messes with the index */
		else {
			delmail(usernumber, which);
			smb_unlock(&smb);
		}
	}

	smb_close(&smb);
	smb_stack(&smb, SMB_STACK_POP);
	current_msg = NULL;

	return lm_mode;
}

int sbbs_t::searchmail(mail_t *mail, int start, int msgs, int which, const char *search, const char* order)
{
	char*    buf;
	char     subj[128];
	int      l, found = 0;
	smbmsg_t msg;

	msg.total_hfields = 0;
	for (l = start; l < msgs && !msgabort(); l++) {
		msg.idx.offset = mail[l].offset;
		if (loadmsg(&msg, mail[l].number) < 0)
			continue;
		smb_unlockmsghdr(&smb, &msg);
		buf = smb_getmsgtxt(&smb, &msg, GETMSGTXT_ALL);
		if (!buf) {
			smb_freemsgmem(&msg);
			continue;
		}
		strupr(buf);
		strip_ctrl(buf, buf);
		SAFECOPY(subj, msg.subj);
		strupr(subj);
		if (strstr(buf, search) || strstr(subj, search)) {
			if (!found) {
				if (which == MAIL_SENT)
					bprintf(text[MailSentLstHdr], order);
				else if (which == MAIL_ALL)
					bprintf(text[MailOnSystemLstHdr], order);
				else
					bprintf(text[MailWaitingLstHdr], order);
			}
			if (which == MAIL_ALL)
				bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
				        , msghdr_text(&msg, MailOnSystemLstFmt)
				        , l + 1, msg.from, msg_to(&msg)
				        , mail_listing_flag(&msg)
				        , msg.subj);
			else
				bprintf(P_TRUNCATE | (msg.hdr.auxattr & MSG_HFIELDS_UTF8)
				        , msghdr_text(&msg, MailWaitingLstFmt), l + 1
				        , which == MAIL_SENT ? msg_to(&msg) : (msg.hdr.attr & MSG_ANONYMOUS) && !useron_is_sysop() ? text[Anonymous] : msg.from
				        , mail_listing_flag(&msg)
				        , msg.subj);
			found++;
		}
		free(buf);
		smb_freemsgmem(&msg);
	}

	return found;
}
