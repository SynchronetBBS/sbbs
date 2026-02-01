/* Synchronet QWK reply (REP) packet creation routine */

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
#include "qwk.h"
#include "filedat.h"

/****************************************************************************/
/* Creates an REP packet for upload to QWK hub 'hubnum'.                    */
/* Returns 1 if successful, 0 if not.										*/
/****************************************************************************/
bool sbbs_t::pack_rep(uint hubnum)
{
	char        str[MAX_PATH + 1];
	char        tmp[MAX_PATH + 1], tmp2[MAX_PATH + 1];
	char        hubid_upper[LEN_QWKID + 1];
	char        hubid_lower[LEN_QWKID + 1];
	char        error[256];
	int         mode;
	const char* fmode;
	int         i, j, k;
	long        msgcnt, submsgs, packedmail, netfiles = 0, deleted;
	uint32_t    u;
	uint32_t    posts;
	uint32_t    mailmsgs;
	ulong       msgs;
	uint32_t    last;
	post_t*     post;
	mail_t*     mail;
	FILE*       rep;
	FILE*       hdrs = NULL;
	FILE*       voting = NULL;
	DIR*        dir;
	DIRENT*     dirent;
	smbmsg_t    msg;

	msgcnt = 0L;
	delfiles(cfg.temp_dir, ALLFILES);

	SAFECOPY(hubid_upper, cfg.qhub[hubnum]->id);
	strupr(hubid_upper);
	SAFECOPY(hubid_lower, cfg.qhub[hubnum]->id);
	strlwr(hubid_lower);

	SAFEPRINTF2(str, "%s%s.REP", cfg.data_dir, hubid_upper);
	if (fexistcase(str)) {
		lprintf(LOG_INFO, "Updating %s", str);
		long file_count = extract_files_from_archive(str
		                                             , /* outdir: */ cfg.temp_dir
		                                             , /* allowed_filename_chars: */ NULL /* any */
		                                             , /* with_path: */ false
		                                             , /* overwrite: */ true
		                                             , /* max_files: */ 0 /* unlimited */
		                                             , /* file_list: */ NULL /* all files */
		                                             , /* recurse: */ false
		                                             , error, sizeof(error));
		if (file_count > 0) {
			lprintf(LOG_DEBUG, "libarchive extracted %lu files from %s", file_count, str);
		} else {
			if (*error)
				lprintf(LOG_NOTICE, "libarchive error (%s) extracting %s", error, str);
			if (*cfg.qhub[hubnum]->unpack)
				external(cmdstr(cfg.qhub[hubnum]->unpack, str, ALLFILES, NULL), EX_OFFLINE);
		}
	} else
		lprintf(LOG_INFO, "Creating %s", str);
	/*************************************************/
	/* Create SYSID.MSG, write header and leave open */
	/*************************************************/
	SAFEPRINTF2(str, "%s%s.MSG", cfg.temp_dir, hubid_upper);
	if (fexistcase(str))
		fmode = "r+b";
	else
		fmode = "w+b";
	if ((rep = fopen(str, fmode)) == NULL) {
		errormsg(WHERE, ERR_CREATE, str, 0, fmode);
		return false;
	}
	fseek(rep, 0, SEEK_END);
	if (ftell(rep) < 1) {                        /* New REP packet */
		fprintf(rep, "%-*s"
		        , QWK_BLOCK_LEN, hubid_upper);  /* So write header */
	}
	if (!(cfg.qhub[hubnum]->misc & QHUB_NOHEADERS)) {
		SAFEPRINTF(str, "%sHEADERS.DAT", cfg.temp_dir);
		fexistcase(str);
		if ((hdrs = fopen(str, "a")) == NULL)
			errormsg(WHERE, ERR_CREATE, str, 0);
	}
	if (!(cfg.qhub[hubnum]->misc & QHUB_NOVOTING)) {
		SAFEPRINTF(str, "%sVOTING.DAT", cfg.temp_dir);
		fexistcase(str);
		if ((voting = fopen(str, "a")) == NULL)
			errormsg(WHERE, ERR_CREATE, str, 0);
	}
	/*********************/
	/* Pack new messages */
	/*********************/
	SAFEPRINTF(smb.file, "%smail", cfg.data_dir);
	smb.retry_time = cfg.smb_retry_time;
	smb.subnum = INVALID_SUB;
	if ((i = smb_open(&smb)) != 0) {
		fclose(rep);
		if (hdrs != NULL)
			fclose(hdrs);
		if (voting != NULL)
			fclose(voting);
		errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
		return false;
	}

	/***********************/
	/* Pack E-mail, if any */
	/***********************/
	qwkmail_last = 0;
	mail = loadmail(&smb, &mailmsgs, 0, MAIL_YOUR, 0);
	packedmail = 0;
	if (mailmsgs) {
		lprintf(LOG_INFO, "Packing NetMail for %s", cfg.qhub[hubnum]->id);
		for (u = 0; u < mailmsgs; u++) {
			//		bprintf("\b\b\b\b\b%-5lu",u+1);

			memset(&msg, 0, sizeof(msg));
			msg.idx = mail[u];
			if (msg.idx.number > qwkmail_last)
				qwkmail_last = msg.idx.number;
			if (loadmsg(&msg, mail[u].number) < 1)
				continue;

			SAFEPRINTF(str, "%s/", cfg.qhub[hubnum]->id);
			if (msg.to_net.type != NET_QWK
			    || (strcmp((char *)msg.to_net.addr, cfg.qhub[hubnum]->id)
			        && strncmp((char *)msg.to_net.addr, str, strlen(str)))) {
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);
				continue;
			}

			mode = QM_TO_QNET | QM_REP;
			mode |= (cfg.qhub[hubnum]->misc & (QHUB_EXT | QHUB_CTRL_A | QHUB_UTF8 | QHUB_WORDWRAP | QHUB_MIME));
			/* For an unclear reason, kludge lines (including @VIA and @TZ) were not included in NetMail previously */
			if (!(cfg.qhub[hubnum]->misc & QHUB_NOHEADERS))
				mode |= (QM_VIA | QM_TZ | QM_MSGID | QM_REPLYTO);
			if (msgtoqwk(&msg, rep, mode, &smb, /* confnum: */ 0, hdrs) > 0)
				packedmail++;
			smb_unlockmsghdr(&smb, &msg);
			smb_freemsgmem(&msg);
			YIELD();    /* yield */
		}
		lprintf(LOG_INFO, "Packed %ld NetMail messages", packedmail);
	}
	smb_close(&smb);                    /* Close the e-mail */
	if (mailmsgs)
		free(mail);

	for (i = 0; i < cfg.qhub[hubnum]->subs; i++) {
		j = cfg.qhub[hubnum]->sub[i]->subnum;             /* j now equals the real sub num */
		msgs = getlastmsg(j, &last, 0);
		term->lncntr = 0;                       /* defeat pause */
		if (!msgs || last <= subscan[j].ptr) {
			if (subscan[j].ptr > last) {
				subscan[j].ptr = last;
				subscan[j].last = last;
			}
			lprintf(LOG_INFO, remove_ctrl_a(text[NScanStatusFmt], tmp)
			        , cfg.grp[cfg.sub[j]->grp]->sname
			        , cfg.sub[j]->lname, 0L, msgs);
			continue;
		}

		SAFEPRINTF2(smb.file, "%s%s"
		            , cfg.sub[j]->data_dir, cfg.sub[j]->code);
		smb.retry_time = cfg.smb_retry_time;
		smb.subnum = j;
		if ((k = smb_open(&smb)) != 0) {
			errormsg(WHERE, ERR_OPEN, smb.file, k, smb.last_error);
			continue;
		}

		post = loadposts(&posts, j, subscan[j].ptr, LP_BYSELF | LP_OTHERS | LP_PRIVATE | LP_REP | LP_VOTES | LP_POLLS, NULL);
		lprintf(LOG_INFO, remove_ctrl_a(text[NScanStatusFmt], tmp)
		        , cfg.grp[cfg.sub[j]->grp]->sname
		        , cfg.sub[j]->lname, posts, msgs);
		if (!posts)  { /* no new messages */
			smb_close(&smb);
			continue;
		}

		subscan[j].ptr = last;                   /* set pointer */
		lprintf(LOG_INFO, "%s", remove_ctrl_a(text[QWKPackingSubboard], tmp)); /* ptr to last msg	*/
		submsgs = 0;
		for (u = 0; u < posts; u++) {
			//		bprintf("\b\b\b\b\b%-5lu",u+1);

			memset(&msg, 0, sizeof(msg));
			msg.idx = post[u].idx;
			if (loadmsg(&msg, post[u].idx.number) < 1)
				continue;

			if (msg.from_net.type && msg.from_net.type != NET_QWK &&
			    !(cfg.sub[j]->misc & SUB_GATE)) {
				smb_freemsgmem(&msg);
				smb_unlockmsghdr(&smb, &msg);
				continue;
			}

			if (!strnicmp(msg.subj, "NE:", 3) || (msg.from_net.type == NET_QWK &&
			                                      route_circ((char *)msg.from_net.addr, cfg.qhub[hubnum]->id))) {
				smb_freemsgmem(&msg);
				smb_unlockmsghdr(&smb, &msg);
				continue;
			}

			mode = cfg.qhub[hubnum]->mode[i] | QM_TO_QNET | QM_REP;
			mode |= (cfg.qhub[hubnum]->misc & (QHUB_EXT | QHUB_CTRL_A | QHUB_UTF8));
			if (!(cfg.qhub[hubnum]->misc & QHUB_NOHEADERS))
				mode |= (QM_VIA | QM_TZ | QM_MSGID | QM_REPLYTO);
			if (msg.from_net.type != NET_QWK)
				mode |= QM_TAGLINE;

			if (msgtoqwk(&msg, rep, mode, &smb, cfg.qhub[hubnum]->conf[i], hdrs, voting) > 0) {
				msgcnt++;
				submsgs++;
			}

			smb_freemsgmem(&msg);
			smb_unlockmsghdr(&smb, &msg);
			if (!(u % 50))
				YIELD(); /* yield */
		}
		lprintf(LOG_INFO, remove_ctrl_a(text[QWKPackedSubboard], tmp), submsgs, msgcnt);
		free(post);
		smb_close(&smb);
		YIELD();    /* yield */
	}

	BOOL voting_data = FALSE;
	if (hdrs != NULL)
		fclose(hdrs);
	if (voting != NULL) {
		voting_data = ftell(voting);
		fclose(voting);
	}
	fclose(rep);            /* close HUB_ID.MSG */
	term->newline();
	/* Look for extra files to send out */
	SAFEPRINTF2(str, "%sqnet/%s.out", cfg.data_dir, hubid_lower);
	dir = opendir(str);
	while (dir != NULL && (dirent = readdir(dir)) != NULL) {
		SAFEPRINTF3(str, "%sqnet/%s.out/%s", cfg.data_dir, hubid_lower, dirent->d_name);
		if (isdir(str))
			continue;
		SAFEPRINTF2(tmp2, "%s%s", cfg.temp_dir, dirent->d_name);
		lprintf(LOG_INFO, remove_ctrl_a(text[RetrievingFile], tmp), str);
		if (!mv(str, tmp2, /* copy: */ TRUE))
			netfiles++;
	}
	if (dir != NULL)
		closedir(dir);
	if (netfiles)
		term->newline();

	if (!msgcnt && !netfiles && !packedmail && !voting_data) {
		lprintf(LOG_INFO, "%s", remove_ctrl_a(text[QWKNoNewMessages], tmp));
		return true;   // Changed from false Mar-11-2005 (needs to be true to save updated ptrs)
	}

	/*******************/
	/* Compress Packet */
	/*******************/
	SAFEPRINTF2(str, "%s%s.REP", cfg.data_dir, hubid_upper);
	SAFEPRINTF2(tmp2, "%s%s", cfg.temp_dir, ALLFILES);
	if (strListFind(cfg.supported_archive_formats, cfg.qhub[hubnum]->fmt, /* case_sensitive */ FALSE) >= 0) {
		str_list_t file_list = directory(tmp2);
		long       file_count = create_archive(str, cfg.qhub[hubnum]->fmt, /* with_path: */ false, file_list, error, sizeof(error));
		strListFree(&file_list);
		if (file_count < 0)
			lprintf(LOG_ERR, "libarchive error %ld (%s) creating %s", file_count, error, str);
		else
			lprintf(LOG_INFO, "libarchive created %s from %ld files", str, file_count);
	} else {
		i = external(cmdstr(cfg.qhub[hubnum]->pack, str, tmp2, NULL)
		             , EX_OFFLINE | EX_WILDCARD);
		if (i)
			errormsg(WHERE, ERR_EXEC, cmdstr(cfg.qhub[hubnum]->pack, str, tmp2, NULL), i);
	}
	if (!fexistcase(str)) {
		lprintf(LOG_WARNING, "%s", remove_ctrl_a(text[QWKCompressionFailed], tmp));
		lprintf(LOG_ERR, "Couldn't compress REP packet");
		return false;
	}
	SAFEPRINTF2(str, "%sqnet/%s.out/", cfg.data_dir, hubid_lower);
	delfiles(str, ALLFILES);

	if (packedmail) {                        /* Delete NetMail */
		SAFEPRINTF(smb.file, "%smail", cfg.data_dir);
		smb.retry_time = cfg.smb_retry_time;
		smb.subnum = INVALID_SUB;
		if ((i = smb_open(&smb)) != 0) {
			errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
			return true;
		}

		mail = loadmail(&smb, &mailmsgs, 0, MAIL_YOUR, 0);

		if ((i = smb_locksmbhdr(&smb)) != 0) {             /* Lock the base, so nobody */
			if (mailmsgs)
				free(mail);
			smb_close(&smb);
			errormsg(WHERE, ERR_LOCK, smb.file, i, smb.last_error); /* messes with the index */
			return true;
		}

		if ((i = smb_getstatus(&smb)) != 0) {
			if (mailmsgs)
				free(mail);
			smb_close(&smb);
			errormsg(WHERE, ERR_READ, smb.file, i, smb.last_error);
			return true;
		}

		deleted = 0;
		/* Mark as READ and DELETE */
		for (u = 0; u < mailmsgs; u++) {
			if (mail[u].number > qwkmail_last)
				continue;
			memset(&msg, 0, sizeof(msg));
			/* !IMPORTANT: search by number (do not initialize msg.idx.offset) */
			if (loadmsg(&msg, mail[u].number) < 1)
				continue;

			SAFEPRINTF(str, "%s/", cfg.qhub[hubnum]->id);
			if (msg.to_net.type != NET_QWK
			    || (strcmp((char *)msg.to_net.addr, cfg.qhub[hubnum]->id)
			        && strncmp((char *)msg.to_net.addr, str, strlen(str)))) {
				smb_unlockmsghdr(&smb, &msg);
				smb_freemsgmem(&msg);
				continue;
			}

			msg.hdr.attr |= MSG_DELETE;
			msg.idx.attr = msg.hdr.attr;
			if ((i = smb_putmsg(&smb, &msg)) != 0)
				errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
			else
				deleted++;
			smb_unlockmsghdr(&smb, &msg);
			smb_freemsgmem(&msg);
		}

		if (deleted && (cfg.sys_misc & SM_DELEMAIL) && smb_lock(&smb) == SMB_SUCCESS) {
			delmail(0, MAIL_YOUR);
			smb_unlock(&smb);
		}
		smb_close(&smb);
		if (mailmsgs)
			free(mail);
		lprintf(LOG_INFO, "Deleted %ld sent NetMail messages", deleted);
	}

	return true;
}
