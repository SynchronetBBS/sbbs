/* Synchronet email function - for sending private e-mail */

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
#include "cmdshell.h"
#include "utf8.h"

/****************************************************************************/
/* Mails a message to usernumber. 'top' is a buffer to place at beginning   */
/* of message.                                                              */
/* Called from functions main_sec, newuser, readmail and scanposts			*/
/****************************************************************************/
bool sbbs_t::email(int usernumber, const char *top, const char *subj, int mode, smb_t* resmb, smbmsg_t* remsg)
{
	char        str[256], str2[256], msgpath[256], ch
	, buf[SDT_BLOCK_LEN];
	char        keys[128];
	char        tmp[512];
	char        title[LEN_TITLE + 1] = "";
	const char* editor = NULL;
	const char* charset = NULL;
	uint16_t    msgattr = 0;
	uint16_t    xlat = XLAT_NONE;
	int         i, j, x, file;
	int         length;
	off_t       offset;
	uint32_t    crc = 0xffffffffUL;
	FILE*       instream;
	node_t      node;
	user_t      user{};
	smbmsg_t    msg;

	if (subj != NULL)
		SAFECOPY_UTF8(title, subj);
	if (remsg != NULL && title[0] == 0)
		SAFECOPY_UTF8(title, remsg->subj);

	if (useron.etoday >= cfg.level_emailperday[useron.level] && !useron_is_sysop() && !(useron.exempt & FLAG('M'))) {
		bputs(text[TooManyEmailsToday]);
		return false;
	}
	user.number = usernumber;
	if (getuserdat(&cfg, &user) != 0 || (user.misc & (DELETED | INACTIVE))) {
		bputs(text[UnknownUser]);
		return false;
	}
	bool to_sysop = user_is_sysop(&user);
	if (to_sysop && useron.rest & FLAG('S')
	    && (cfg.valuser != usernumber || useron.fbacks || useron.emails)) { /* ! val fback */
		bprintf(text[R_Feedback], cfg.sys_op);
		return false;
	}
	if (!to_sysop && useron.rest & FLAG('E')
	    && (cfg.valuser != usernumber || useron.fbacks || useron.emails)) {
		bputs(text[R_Email]);
		return false;
	}
	if ((user.misc & NETMAIL) && (cfg.sys_misc & SM_FWDTONET) && !(mode & WM_NOFWD) && !(useron.rest & FLAG('M'))) {
		if (netmail_addr_is_supported(&cfg, user.netmail)) {
			bprintf(text[UserNetMail], user.netmail);
			if ((mode & WM_FORCEFWD) || yesno(text[ForwardMailQ])) /* Forward to netmail address */
				return netmail(user.netmail, subj, mode, resmb, remsg);
		} else {
			bprintf(text[InvalidNetMailAddr], user.netmail);
		}
	}
	if (sys_status & SS_ABORT) {
		bputs(text[Aborted]);
		return false;
	}
	bprintf(text[Emailing], username(&cfg, usernumber, tmp), usernumber);
	action = NODE_SMAL;
	nodesync();

	if (to_sysop && !useron_is_sysop() && (useron.fbacks || usernumber != cfg.valuser)) {
		bool invoked = false;
		if (exec_mod("send feedback", cfg.feedback_mod, &invoked) != 0 && invoked)
			return false;
	}

	if (cfg.sys_misc & SM_ANON_EM && useron.exempt & FLAG('A')
	    && !noyes(text[AnonymousQ])) {
		msgattr |= MSG_ANONYMOUS;
		mode |= WM_ANON;
	}

	if (cfg.sys_misc & SM_DELREADM)
		msgattr |= MSG_KILLREAD;

	if (remsg != NULL && resmb != NULL && !(mode & WM_QUOTE)) {
		if (quotemsg(resmb, remsg, /* include tails: */ true))
			mode |= WM_QUOTE;
	}

	msg_tmp_fname(useron.xedit, msgpath, sizeof(msgpath));
	username(&cfg, usernumber, str2);
	if (!writemsg(msgpath, top, /* subj: */ title, WM_EMAIL | mode, INVALID_SUB, /* to: */ str2, /* from: */ useron.alias, &editor, &charset)) {
		bputs(text[Aborted]);
		return false;
	}

	if (mode & WM_FILE && !useron_is_sysop() && !(cfg.sys_misc & SM_FILE_EM)) {
		bputs(text[EmailFilesNotAllowed]);
		mode &= ~WM_FILE;
	}

	if (mode & WM_FILE) {
		if (!checkfname(title)) {
			bprintf(text[BadFilename], title);
			(void)remove(msgpath);
			return false;
		}
		SAFEPRINTF2(str2, "%sfile/%04u.in", cfg.data_dir, usernumber);
		(void)MKDIR(str2);
		SAFEPRINTF3(str2, "%sfile/%04u.in/%s", cfg.data_dir, usernumber, title);
		if (fexistcase(str2)) {
			bprintf(text[FileAlreadyThere], str2);
			(void)remove(msgpath);
			return false;
		}
		xfer_prot_menu(XFER_UPLOAD, &useron, keys, sizeof keys);
		SAFECAT(keys, quit_key(str));
		mnemonics(text[ProtocolOrQuit]);
		ch = (char)getkeys(keys, 0);
		if (ch == quit_key() || sys_status & SS_ABORT) {
			bputs(text[Aborted]);
			(void)remove(msgpath);
			return false;
		}
		x = protnum(ch, XFER_UPLOAD);
		if (x < cfg.total_prots)   /* This should be always */
			protocol(cfg.prot[x], XFER_UPLOAD, str2, nulstr, true);
		safe_snprintf(tmp, sizeof(tmp), "%s%s", cfg.temp_dir, title);
		if (!fexistcase(str2) && fexistcase(tmp))
			mv(tmp, str2, 0);
		off_t l = flength(str2);
		if (l > 0)
			bprintf(text[FileNBytesReceived], title, u64toac(l, tmp));
		else {
			bprintf(text[FileNotReceived], title);
			(void)remove(msgpath);
			return false;
		}
	}

	bputs(text[WritingIndx]);

	if ((i = smb_stack(&smb, SMB_STACK_PUSH)) != 0) {
		errormsg(WHERE, ERR_OPEN, "MAIL", i);
		return false;
	}

	if ((i = smb_open_sub(&cfg, &smb, INVALID_SUB)) != 0) {
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
		return false;
	}

	if ((i = smb_locksmbhdr(&smb)) != 0) {
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_LOCK, smb.file, i, smb.last_error);
		return false;
	}

	length = (int)flength(msgpath) + 2;  /* +2 for translation string */

	if (length & 0xfff00000UL) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_LEN, msgpath, length);
		return false;
	}

	if ((i = smb_open_da(&smb)) != 0) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
		return false;
	}
	if (cfg.sys_misc & SM_FASTMAIL)
		offset = smb_fallocdat(&smb, length, 1);
	else
		offset = smb_allocdat(&smb, length, 1);
	smb_close_da(&smb);

	if ((instream = fnopen(&file, msgpath, O_RDONLY | O_BINARY)) == NULL) {
		smb_freemsgdat(&smb, offset, length, 1);
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_OPEN, msgpath, O_RDONLY | O_BINARY);
		return false;
	}

	smb_fseek(smb.sdt_fp, offset, SEEK_SET);
	xlat = XLAT_NONE;
	smb_fwrite(&smb, &xlat, 2, smb.sdt_fp);
	x = SDT_BLOCK_LEN - 2;              /* Don't read/write more than 255 */
	while (!feof(instream)) {
		memset(buf, 0, x);
		j = fread(buf, 1, x, instream);
		if (j < 1)
			break;
		if (j > 1 && (j != x || feof(instream)) && buf[j - 1] == LF && buf[j - 2] == CR)
			buf[j - 1] = buf[j - 2] = 0;
		if (cfg.mail_maxcrcs) {
			for (i = 0; i < j; i++)
				crc = ucrc32(buf[i], crc);
		}
		smb_fwrite(&smb, buf, j, smb.sdt_fp);
		x = SDT_BLOCK_LEN;
	}
	smb_fflush(smb.sdt_fp);
	fclose(instream);
	crc = ~crc;

	memset(&msg, 0, sizeof(smbmsg_t));
	msg.hdr.version = smb_ver();
	msg.hdr.attr = msgattr;
	if (mode & WM_FILE)
		msg.hdr.auxattr |= (MSG_FILEATTACH | MSG_KILLFILE);
	msg.hdr.when_imported.time = time32(NULL);
	msg.hdr.when_imported.zone = sys_timezone(&cfg);
	msg.hdr.when_written = smb_when(msg.hdr.when_imported.time, msg.hdr.when_imported.zone);

	if (cfg.mail_maxcrcs) {
		i = smb_addcrc(&smb, crc);
		if (i) {
			smb_freemsgdat(&smb, offset, length, 1);
			smb_unlocksmbhdr(&smb);
			smb_close(&smb);
			smb_stack(&smb, SMB_STACK_POP);
			bprintf(text[CantPostMsg], i == SMB_DUPE_MSG ? "duplicate" : "other");
			return false;
		}
	}

	msg.hdr.offset = (uint32_t)offset;

	username(&cfg, usernumber, str);
	smb_hfield_str(&msg, RECIPIENT, str);

	SAFEPRINTF(str, "%u", usernumber);
	smb_hfield_str(&msg, RECIPIENTEXT, str);

	SAFECOPY(str, useron.alias);
	smb_hfield_str(&msg, SENDER, str);

	SAFEPRINTF(str, "%u", useron.number);
	smb_hfield_str(&msg, SENDEREXT, str);

	if (useron.misc & NETMAIL) {
		if (useron_is_guest())
			smb_hfield_str(&msg, REPLYTO, useron.name);
		smb_hfield_netaddr(&msg, REPLYTONETADDR, useron.netmail, NULL);
	}

	/* Security logging */
	msg_client_hfields(&msg, &client);
	smb_hfield_str(&msg, SENDERSERVER, server_host_name());

	normalize_msg_hfield_encoding(charset, title, sizeof title);

	smb_hfield_str(&msg, SUBJECT, title);

	add_msg_ids(&cfg, &smb, &msg, remsg);

	editor_info_to_msg(&msg, editor, charset);

	smb_dfield(&msg, TEXT_BODY, length);

	i = smb_addmsghdr(&smb, &msg, smb_storage_mode(&cfg, &smb)); // calls smb_unlocksmbhdr()
	if (i == SMB_SUCCESS) {
		if (remsg != NULL)
			smb_updatethread(&smb, remsg, msg.hdr.number);
	} else {
		errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
		smb_freemsgdat(&smb, offset, length, 1);
	}
	smb_close(&smb);
	smb_stack(&smb, SMB_STACK_POP);
	smb_freemsgmem(&msg);
	if (i != SMB_SUCCESS)
		return false;

	if (usernumber == 1)
		logon_fbacks++;
	else
		logon_emails++;
	user_sent_email(&cfg, &useron, 1, usernumber == 1);
	bprintf(text[Emailed], username(&cfg, usernumber, tmp), usernumber);
	llprintf("E+", "sent e-mail to %s #%d"
	              , username(&cfg, usernumber, tmp), usernumber);
	if (mode & WM_FILE && online == ON_REMOTE)
		autohangup();
	if (msgattr & MSG_ANONYMOUS)               /* Don't tell user if anonymous */
		return true;
	for (i = 1; i <= cfg.sys_nodes; i++) { /* Tell user, if online */
		getnodedat(i, &node);
		if (node.useron == usernumber && !(node.misc & NODE_POFF)
		    && (node.status == NODE_INUSE || node.status == NODE_QUIET)) {
			safe_snprintf(str, sizeof(str), text[EmailNodeMsg], cfg.node_num, useron.alias);
			putnmsg(i, str);
			break;
		}
	}
	if (i > cfg.sys_nodes) {   /* User wasn't online, so leave short msg */
		safe_snprintf(str, sizeof(str), text[UserSentYouMail], useron.alias);
		putsmsg(usernumber, str);
	}
	return true;
}
