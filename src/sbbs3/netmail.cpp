/* Synchronet network mail-related functions */

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

static bool pt_zone_kludge(const fmsghdr_t* hdr, int fido)
{
	char str[256];

	snprintf(str, sizeof str, "\1INTL %hu:%hu/%hu %hu:%hu/%hu\r"
	         , hdr->destzone, hdr->destnet, hdr->destnode
	         , hdr->origzone, hdr->orignet, hdr->orignode);
	if (write(fido, str, strlen(str)) < 1)
		return false;

	if (hdr->destpoint) {
		snprintf(str, sizeof str, "\1TOPT %hu\r"
		         , hdr->destpoint);
		if (write(fido, str, strlen(str)) < 1)
			return false;
	}

	if (hdr->origpoint) {
		snprintf(str, sizeof str, "\1FMPT %hu\r"
		         , hdr->origpoint);
		if (write(fido, str, strlen(str)) < 1)
			return false;
	}
	return true;
}

/****************************************************************************/
/* Returns the FidoNet address (struct) parsed from str (in ASCII text).    */
/****************************************************************************/
static faddr_t atofaddr(scfg_t* cfg, const char *str)
{
	return smb_atofaddr(&cfg->faddr[0], str);
}

/****************************************************************************/
/* Send FidoNet/QWK/Internet NetMail from BBS								*/
/****************************************************************************/
bool sbbs_t::netmail(const char *into, const char *title, int mode, smb_t* resmb, smbmsg_t* remsg, str_list_t cc_list)
{
	char        str[256], fname[128], *buf, *p, ch;
	char        to[256] = "";
	char        from[FIDO_NAME_LEN] = "";
	char        subj[FIDO_SUBJ_LEN] = "";
	char        msgpath[MAX_PATH + 1];
	char        tmp[512];
	const char* editor = NULL;
	const char* charset = NULL;
	int         file, x;
	uint        i;
	long        length, l;
	faddr_t     src_addr;
	faddr_t     dest_addr;
	uint16_t    net_type = NET_NONE;
	smbmsg_t    msg;
	memset(&msg, 0, sizeof(msg));

	if (useron.etoday >= cfg.level_emailperday[useron.level] && !useron_is_sysop() && !(useron.exempt & FLAG('M'))) {
		bputs(text[TooManyEmailsToday]);
		return false;
	}

	if (useron.rest & FLAG('M')) {
		bputs(text[NoNetMailAllowed]);
		return false;
	}

	if (title != NULL)
		SAFECOPY(subj, title);
	if (into != NULL)
		SAFECOPY(to, into);
	if (remsg != NULL) {
		if (subj[0] == 0 && remsg->subj != NULL)
			SAFECOPY(subj, remsg->subj);
		if (to[0] == 0 && strListCount(cc_list) < 1) {
			if ((p = smb_netaddrstr(&remsg->from_net, tmp)) != NULL) {
				if (strchr(p, '@')) {
					SAFECOPY(to, p);
				} else {
					SAFEPRINTF2(to, "%s@%s", remsg->from, p);
				}
			} else {
				SAFECOPY(to, remsg->from);
			}
		}
	}

	if (*to == '\0' && cc_list != NULL && (*cc_list) != NULL) {
		SAFECOPY(to, cc_list[0]);
		cc_list++;
	}
	lookup_netuser(to);

	if (!netmail_addr_is_supported(&cfg, to)) {
		int usernum = finduserstr(useron.number, USER_NETMAIL, to);
		if (usernum > 0)
			return email(usernum, nullptr, subj, WM_NONE, resmb, remsg);
		bprintf(text[InvalidNetMailAddr], to);
		return false;
	}

	net_type = smb_netaddr_type(to);

	lprintf(LOG_DEBUG, "parsed net type of '%s' is %s", to, smb_nettype((enum smb_net_type)net_type));
	if (net_type == NET_QWK) {
		if (mode & WM_FILE) {
			bputs(text[EmailFilesNotAllowed]);
			mode &= ~WM_FILE;
		}
		return qnetmail(to, subj, mode, resmb, remsg);
	}
	if (net_type == NET_INTERNET) {
		if (!(cfg.inetmail_misc & NMAIL_ALLOW)) {
			bputs(text[NoNetMailAllowed]);
			return false;
		}
		if (mode & WM_FILE && !useron_is_sysop() && !(cfg.inetmail_misc & NMAIL_FILE)) {
			bputs(text[EmailFilesNotAllowed]);
			mode &= ~WM_FILE;
		}
		return inetmail(to, subj, mode, resmb, remsg, cc_list);
	}
	p = strrchr(to, '@');      /* Find '@' in name@addr */
	if (p == NULL || net_type != NET_FIDO) {
		if (!(sys_status & SS_ABORT))
			bprintf(text[InvalidNetMailAddr], to);
		return false;
	}
	if (!cfg.total_faddrs || (!useron_is_sysop() && !(cfg.netmail_misc & NMAIL_ALLOW))) {
		bputs(text[NoNetMailAllowed]);
		return false;
	}
	*p = 0;                   /* Chop off address */
	p++;
	SKIP_WHITESPACE(p);
	dest_addr = atofaddr(&cfg, p);     /* Get fido address */

	if ((mode & WM_FILE) && !useron_is_sysop() && !(cfg.netmail_misc & NMAIL_FILE)) {
		bputs(text[EmailFilesNotAllowed]);
		mode &= ~WM_FILE;
	}

	truncsp(to);                /* Truncate off space */

	SAFECOPY(from, (cfg.netmail_misc & NMAIL_ALIAS) || (useron.rest & FLAG('O')) ? useron.alias : useron.name);

	/* Look-up in nodelist? */

	if (cfg.netmail_cost && !(useron.exempt & FLAG('S'))) {
		if (user_available_credits(&useron) < cfg.netmail_cost) {
			bputs(text[NotEnoughCredits]);
			return false;
		}
		snprintf(str, sizeof str, text[NetMailCostContinueQ], cfg.netmail_cost);
		if (noyes(str))
			return false;
	}

	i = nearest_sysfaddr_index(&cfg, &dest_addr);

	if ((cfg.netmail_misc & NMAIL_CHSRCADDR) && cfg.total_faddrs > 1) {
		for (int j = 0; j < cfg.total_faddrs; j++)
			uselect(/* add: */ true, j, text[OriginFidoAddr], smb_faddrtoa(&cfg.faddr[j], tmp), /* ar: */ NULL);
		int choice = uselect(/* add: */ false, /* default: */ i, NULL, NULL, NULL);
		if (choice < 0)
			return false;
		i = choice;
	}
	src_addr = cfg.faddr[i];

	smb_faddrtoa(&cfg.faddr[i], str);
	bprintf(text[NetMailing], to, smb_faddrtoa(&dest_addr, tmp), from, str);

	if (cfg.netmail_misc & NMAIL_CRASH)
		msg.hdr.netattr |= NETMSG_CRASH;
	if (cfg.netmail_misc & NMAIL_HOLD)
		msg.hdr.netattr |= NETMSG_HOLD;
	if (cfg.netmail_misc & NMAIL_KILL)
		msg.hdr.netattr |= NETMSG_KILLSENT;
	if (mode & WM_FILE)
		msg.hdr.auxattr |= (MSG_FILEATTACH | MSG_KILLFILE);

	if (remsg != NULL && resmb != NULL && !(mode & WM_QUOTE)) {
		if (quotemsg(resmb, remsg, /* include tails: */ true))
			mode |= WM_QUOTE;
	}

	msg_tmp_fname(useron.xedit, msgpath, sizeof(msgpath));
	if (!writemsg(msgpath, nulstr, subj, WM_NETMAIL | mode, INVALID_SUB, to, from, &editor, &charset)) {
		bputs(text[Aborted]);
		return false;
	}

	if (mode & WM_FILE) {
		SAFECOPY(fname, subj);
		snprintf(str, sizeof str, "%sfile/%04u.out", cfg.data_dir, useron.number);
		(void)MKDIR(str);
		SAFECOPY(tmp, cfg.data_dir);
		if (tmp[0] == '.')    /* Relative path */
			snprintf(tmp, sizeof tmp, "%s%s", cfg.node_dir, cfg.data_dir);
		SAFEPRINTF3(str, "%sfile/%04u.out/%s", tmp, useron.number, fname);
		SAFECOPY(subj, str);
		if (fexistcase(str)) {
			bprintf(text[FileAlreadyThere], str);
			return false;
		}
		{ /* Remote */
			char keys[128];
			xfer_prot_menu(XFER_UPLOAD, &useron, keys, sizeof keys);
			SAFECAT(keys, quit_key(str));
			mnemonics(text[ProtocolOrQuit]);
			ch = (char)getkeys(keys, 0);
			if (ch == quit_key() || sys_status & SS_ABORT) {
				bputs(text[Aborted]);
				return false;
			}
			x = protnum(ch, XFER_UPLOAD);
			if (x < cfg.total_prots)   /* This should be always */
				protocol(cfg.prot[x], XFER_UPLOAD, subj, nulstr, true);
		}
		snprintf(tmp, sizeof tmp, "%s%s", cfg.temp_dir, subj);
		if (!fexistcase(subj) && fexistcase(tmp))
			mv(tmp, subj, 0);
		l = (long)flength(subj);
		if (l > 0)
			bprintf(text[FileNBytesReceived], fname, ultoac(l, tmp));
		else {
			bprintf(text[FileNotReceived], fname);
			return false;
		}
	}

	msg.hdr.netattr |= NETMSG_LOCAL;
	lprintf(LOG_DEBUG, "NetMail subject: %s", subj);
	p = subj;
	if ((useron_is_sysop() || useron.exempt & FLAG('F'))
	    && !strnicmp(p, "CR:", 3)) {     /* Crash over-ride by sysop */
		p += 3;               /* skip CR: */
		SKIP_WHITESPACE(p);
		msg.hdr.netattr |= NETMSG_CRASH;
	}

	if ((useron_is_sysop() || useron.exempt & FLAG('F'))
	    && !strnicmp(p, "FR:", 3)) {     /* File request */
		p += 3;               /* skip FR: */
		SKIP_WHITESPACE(p);
		msg.hdr.auxattr |= MSG_FILEREQUEST;
	}

	if ((useron_is_sysop() || useron.exempt & FLAG('F'))
	    && !strnicmp(p, "RR:", 3)) {     /* Return receipt request */
		p += 3;               /* skip RR: */
		SKIP_WHITESPACE(p);
		msg.hdr.auxattr |= MSG_RECEIPTREQ;
	}

	if ((useron_is_sysop() || useron.exempt & FLAG('F'))
	    && !strnicmp(p, "FA:", 3)) {     /* File Attachment */
		p += 3;               /* skip FA: */
		SKIP_WHITESPACE(p);
//		hdr.attr|=FIDO_FILE;	TODO
	}

	if (p != subj)
		SAFECOPY(subj, p);

	if ((file = nopen(msgpath, O_RDONLY)) == -1) {
		errormsg(WHERE, ERR_OPEN, msgpath, O_RDONLY);
		return false;
	}
	length = (long)filelength(file);
	if (length < 0) {
		close(file);
		errormsg(WHERE, ERR_LEN, msgpath, length);
		return false;
	}
	if ((buf = (char *)calloc(1, length + 1)) == NULL) {
		close(file);
		errormsg(WHERE, ERR_ALLOC, str, length);
		return false;
	}
	if (read(file, buf, length) != length) {
		close(file);
		free(buf);
		errormsg(WHERE, ERR_READ, str, length);
		return false;
	}
	close(file);

	smb_net_type_t nettype = NET_FIDO;
	smb_hfield_str(&msg, SENDER, from);
	snprintf(str, sizeof str, "%u", useron.number);
	smb_hfield_str(&msg, SENDEREXT, str);
	smb_hfield(&msg, SENDERNETTYPE, sizeof(nettype), &nettype);
	smb_hfield(&msg, SENDERNETADDR, sizeof(dest_addr), &src_addr);

	smb_hfield_str(&msg, RECIPIENT, to);
	smb_hfield(&msg, RECIPIENTNETTYPE, sizeof(nettype), &nettype);
	smb_hfield(&msg, RECIPIENTNETADDR, sizeof(dest_addr), &dest_addr);

	normalize_msg_hfield_encoding(charset, subj, sizeof subj);

	smb_hfield_str(&msg, SUBJECT, subj);

	editor_info_to_msg(&msg, editor, charset);

	if (cfg.netmail_misc & NMAIL_DIRECT)
		msg.hdr.netattr |= NETMSG_DIRECT;

	smb_t smb;
	memset(&smb, 0, sizeof(smb));
	smb.subnum = INVALID_SUB;
	int   result = savemsg(&cfg, &smb, &msg, &client, server_host_name(), buf, remsg);
	free(buf);
	smb_close(&smb);
	smb_freemsgmem(&msg);
	if (result != SMB_SUCCESS) {
		errormsg(WHERE, ERR_WRITE, smb.file, result, smb.last_error);
		return false;
	}

	useron.emails = (uint)adjustuserval(&cfg, &useron, USER_EMAILS, 1);
	logon_emails++;
	useron.etoday = (uint)adjustuserval(&cfg, &useron, USER_ETODAY, 1);
	if (!(useron.exempt & FLAG('S')))
		subtract_cdt(&cfg, &useron, cfg.netmail_cost);

	bprintf(text[FidoNetMailSent], to, smb_faddrtoa(&dest_addr, tmp));
	if (mode & WM_FILE)
		llprintf("EN", "sent NetMail file attachment to %s (%s)"
		            , to, smb_faddrtoa(&dest_addr, tmp));
	else
		llprintf("EN", "sent NetMail to %s (%s)"
		            , to, smb_faddrtoa(&dest_addr, tmp));

	return true;
}

/****************************************************************************/
/* Send NetMail from QWK REP Packet 										*/
/****************************************************************************/
void sbbs_t::qwktonetmail(FILE *rep, char *block, char *into, uchar fromhub)
{
	char *     qwkbuf, to[129], name[129], sender[129], senderaddr[129]
	, str[256], *p, *cp, *addr, fulladdr[129], ch;
	char*      sender_id = fromhub ? cfg.qhub[fromhub - 1]->id : useron.alias;
	char*      subject = NULL;
	char       tmp[512];
	int        i, fido, inet = 0, qnet = 0;
	uint16_t   net;
	uint16_t   xlat;
	long       l, length, m, n;
	off_t      offset;
	faddr_t    fidoaddr;
	fmsghdr_t  hdr;
	smbmsg_t   msg;
	struct  tm tm;

	if (useron.rest & FLAG('M')) {
		bputs(text[NoNetMailAllowed]);
		return;
	}

	to[0] = 0;
	name[0] = 0;
	sender[0] = 0;
	senderaddr[0] = 0;
	fulladdr[0] = 0;

	snprintf(str, sizeof str, "%.6s", block + 116);
	n = atol(str);      /* number of 128 byte records */

	if (n < 2L || n > 999999L) {
		errormsg(WHERE, ERR_CHK, "QWK blocks", n);
		return;
	}
	// Allocate/zero an extra block of NULs for strchr() usage and other ASCIIZ goodness
	if ((qwkbuf = (char *)calloc(n + 1, QWK_BLOCK_LEN)) == NULL) {
		errormsg(WHERE, ERR_ALLOC, nulstr, n * QWK_BLOCK_LEN);
		return;
	}
	memcpy((char *)qwkbuf, block, QWK_BLOCK_LEN);
	if (fread(qwkbuf + QWK_BLOCK_LEN, QWK_BLOCK_LEN, n - 1, rep) != (size_t)n - 1) {
		errormsg(WHERE, ERR_READ, "QWK block", n - 1);
		free(qwkbuf);
		return;
	}

	size_t kludge_hdrlen = 0;
	char*  beg = qwkbuf + QWK_BLOCK_LEN;
	char*  end = qwkbuf + (n * QWK_BLOCK_LEN);
	p = beg;
	if (into == NULL) {
		SAFECOPY(to, p);  /* To user on first line */
		char* tp = strchr(to, QWK_NEWLINE);     /* chop off at first CR */
		if (tp != NULL)
			*tp = 0;
		p += strlen(to) + 1;
	}
	else
		SAFECOPY(to, into);

	// Parse QWKE Kludge Lines here:
	while (p < end && *p != QWK_NEWLINE) {
		if (strncmp(p, "To:", 3) == 0) {
			p += 3;
			SKIP_WHITESPACE(p);
			char* tp = strchr(p, QWK_NEWLINE);      /* chop off at first CR */
			if (tp != NULL)
				*tp = 0;
			SAFECOPY(to, p);
			p += strlen(p) + 1;
			continue;
		}
		if (strncmp(p, "Subject:", 8) == 0) {
			p += 8;
			SKIP_WHITESPACE(p);
			char* tp = strchr(p, QWK_NEWLINE);      /* chop off at first CR */
			if (tp != NULL)
				*tp = 0;
			subject = p;
			p += strlen(p) + 1;
			continue;
		}
		break;
	}
	kludge_hdrlen += (p - beg) + 1;

	SAFECOPY(name, to);
	p = strchr(name, '@');
	if (p)
		*p = 0;
	truncsp(name);


	p = strrchr(to, '@');       /* Find '@' in name@addr */
	if (p && !IS_DIGIT(*(p + 1)) && !strchr(p, '.') && !strchr(p, ':')) { /* QWKnet */
		qnet = 1;
		*p = 0;
	}
	else if (p == NULL || !IS_DIGIT(*(p + 1)) || !cfg.total_faddrs) {
		if (cfg.inetmail_misc & NMAIL_ALLOW) { /* Internet */
			inet = 1;
		} else {
			bprintf(text[InvalidNetMailAddr], to);
			free(qwkbuf);
			return;
		}
	}
	else {
		fidoaddr = atofaddr(&cfg, p + 1);    /* Get fido address */
		*p = 0;                   /* Chop off address */
	}


	if (!inet && !qnet &&        /* FidoNet */
	    ((!useron_is_sysop() && !(cfg.netmail_misc & NMAIL_ALLOW)) || !cfg.total_faddrs)) {
		bputs(text[NoNetMailAllowed]);
		free(qwkbuf);
		return;
	}

	truncsp(to);            /* Truncate off space */

	if (!stricmp(to, "SBBS") && !useron_is_sysop() && qnet) {
		free(qwkbuf);
		return;
	}

	l = QWK_BLOCK_LEN + kludge_hdrlen;      /* Start of message text */

	if (qnet || inet) {

		memset(&msg, 0, sizeof(smbmsg_t));
		msg.hdr.version = smb_ver();
		msg.hdr.when_imported.time = time32(NULL);
		msg.hdr.when_imported.zone = sys_timezone(&cfg);

		if (fromhub || useron.rest & FLAG('Q')) {
			net = NET_QWK;
			smb_hfield(&msg, SENDERNETTYPE, sizeof(net), &net);
			if (!strncmp(qwkbuf + l, "@VIA:", 5)) {
				snprintf(str, sizeof str, "%.128s", qwkbuf + l + 5);
				cp = strchr(str, QWK_NEWLINE);
				if (cp)
					*cp = 0;
				l += strlen(str) + 1;
				cp = str;
				while (*cp && *cp <= ' ') cp++;
				safe_snprintf(senderaddr, sizeof(senderaddr), "%s/%s", sender_id, cp);
				strupr(senderaddr);
				smb_hfield(&msg, SENDERNETADDR, strlen(senderaddr), senderaddr);
			}
			else {
				SAFECOPY(senderaddr, sender_id);
				strupr(senderaddr);
				smb_hfield(&msg, SENDERNETADDR, strlen(senderaddr), senderaddr);
			}
			snprintf(sender, sizeof sender, "%.25s", block + 46);     /* From name */
		}
		else {  /* Not Networked */
			msg.hdr.when_written.zone = sys_timezone(&cfg);
			snprintf(str, sizeof str, "%u", useron.number);
			smb_hfield(&msg, SENDEREXT, strlen(str), str);
			SAFECOPY(sender, (qnet || cfg.inetmail_misc & NMAIL_ALIAS)
			    ? useron.alias : useron.name);
		}
		truncsp(sender);
		smb_hfield(&msg, SENDER, strlen(sender), sender);
		if (fromhub)
			msg.idx.from = 0;
		else
			msg.idx.from = useron.number;
		if (!strncmp(qwkbuf + l, "@TZ:", 4)) {
			snprintf(str, sizeof str, "%.128s", qwkbuf + l);
			cp = strchr(str, QWK_NEWLINE);
			if (cp)
				*cp = 0;
			l += strlen(str) + 1;
			cp = str + 4;
			while (*cp && *cp <= ' ') cp++;
			msg.hdr.when_written.zone = (short)ahtoul(cp);
		}
		else
			msg.hdr.when_written.zone = sys_timezone(&cfg);
		memset(&tm, 0, sizeof(tm));
		tm.tm_mon = ((qwkbuf[8] & 0xf) * 10) + (qwkbuf[9] & 0xf);
		if (tm.tm_mon)
			tm.tm_mon--;            /* 0 based */
		tm.tm_mday = ((qwkbuf[11] & 0xf) * 10) + (qwkbuf[12] & 0xf);
		tm.tm_year = ((qwkbuf[14] & 0xf) * 10) + (qwkbuf[15] & 0xf);
		if (tm.tm_year < Y2K_2DIGIT_WINDOW)
			tm.tm_year += 100;
		tm.tm_hour = ((qwkbuf[16] & 0xf) * 10) + (qwkbuf[17] & 0xf);
		tm.tm_min = ((qwkbuf[19] & 0xf) * 10) + (qwkbuf[20] & 0xf);  /* From QWK time */
		tm.tm_sec = 0;

		tm.tm_isdst = -1; /* Do not adjust for DST */
		msg.hdr.when_written = smb_when(mktime(&tm), msg.hdr.when_written.zone);

		if (subject == NULL) {
			snprintf(str, sizeof str, "%.25s", block + 71);
			subject = str;
		}
		smb_hfield_str(&msg, SUBJECT, subject);
	}

	if (qnet) {

		p++;
		addr = p;
		msg.idx.to = qwk_route(&cfg, addr, fulladdr, sizeof(fulladdr) - 1);
		if (!fulladdr[0]) {      /* Invalid address, so BOUNCE it */
			/**
			    errormsg(WHERE,ERR_CHK,addr,0);
			    free(qwkbuf);
			    smb_freemsgmem(msg);
			    return;
			**/
			smb_hfield(&msg, SENDER, strlen(cfg.sys_id), cfg.sys_id);
			msg.idx.from = 0;
			msg.idx.to = useron.number;
			SAFECOPY(to, sender);
			SAFECOPY(fulladdr, senderaddr);
			SAFEPRINTF(str, "BADADDR: %s", addr);
			smb_hfield(&msg, SUBJECT, strlen(str), str);
			net = NET_NONE;
			smb_hfield(&msg, SENDERNETTYPE, sizeof(net), &net);
		}
		/* This is required for fixsmb to be able to rebuild the index */
		SAFEPRINTF(str, "%u", msg.idx.to);
		smb_hfield_str(&msg, RECIPIENTEXT, str);

		smb_hfield(&msg, RECIPIENT, strlen(name), name);
		net = NET_QWK;
		smb_hfield(&msg, RECIPIENTNETTYPE, sizeof(net), &net);

		truncsp(fulladdr);
		if (fulladdr[0])
			smb_hfield(&msg, RECIPIENTNETADDR, strlen(fulladdr), fulladdr);

		bprintf(text[NetMailing], to, fulladdr, sender, cfg.sys_id);
	}

	if (inet) {              /* Internet E-mail */

		if (cfg.inetmail_cost && !(useron.exempt & FLAG('S'))) {
			if (user_available_credits(&useron) < cfg.inetmail_cost) {
				bputs(text[NotEnoughCredits]);
				free(qwkbuf);
				smb_freemsgmem(&msg);
				return;
			}
			snprintf(str, sizeof str, text[NetMailCostContinueQ], cfg.inetmail_cost);
			if (noyes(str)) {
				free(qwkbuf);
				smb_freemsgmem(&msg);
				return;
			}
		}

		net = NET_INTERNET;
		smb_hfield(&msg, RECIPIENT, strlen(name), name);
		msg.idx.to = 0;   /* Out-bound NetMail set to 0 */
		smb_hfield(&msg, RECIPIENTNETTYPE, sizeof(net), &net);
		smb_hfield(&msg, RECIPIENTNETADDR, strlen(to), to);

		bprintf(text[NetMailing], name, to
		        , (cfg.inetmail_misc & NMAIL_ALIAS) || (useron.rest & FLAG('O')) ? useron.alias : useron.name
		        , cfg.sys_inetaddr);
	}

	if (qnet || inet) {

		bputs(text[WritingIndx]);

		if ((i = smb_stack(&smb, SMB_STACK_PUSH)) != 0) {
			errormsg(WHERE, ERR_OPEN, "MAIL", i);
			free(qwkbuf);
			smb_freemsgmem(&msg);
			return;
		}
		snprintf(smb.file, sizeof smb.file, "%smail", cfg.data_dir);
		smb.retry_time = cfg.smb_retry_time;
		smb.subnum = INVALID_SUB;
		if ((i = smb_open(&smb)) != 0) {
			smb_stack(&smb, SMB_STACK_POP);
			errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
			free(qwkbuf);
			smb_freemsgmem(&msg);
			return;
		}
		add_msg_ids(&cfg, &smb, &msg, /* remsg: */ NULL);

		if (smb_fgetlength(smb.shd_fp) < 1L) {   /* Create it if it doesn't exist */
			smb.status.max_crcs = cfg.mail_maxcrcs;
			smb.status.max_msgs = 0;
			smb.status.max_age = cfg.mail_maxage;
			smb.status.attr = SMB_EMAIL;
			if ((i = smb_create(&smb)) != 0) {
				smb_close(&smb);
				smb_stack(&smb, SMB_STACK_POP);
				errormsg(WHERE, ERR_CREATE, smb.file, i, smb.last_error);
				free(qwkbuf);
				smb_freemsgmem(&msg);
				return;
			}
		}

		length = n * 256L;  // Extra big for CRLF xlat, was (n-1L)*256L (03/16/96)


		if (length & 0xfff00000UL || !length) {
			smb_close(&smb);
			smb_stack(&smb, SMB_STACK_POP);
			snprintf(str, sizeof str, "REP msg (%ld)", n);
			errormsg(WHERE, ERR_LEN, str, length);
			free(qwkbuf);
			smb_freemsgmem(&msg);
			return;
		}

		if ((i = smb_open_da(&smb)) != 0) {
			smb_close(&smb);
			smb_stack(&smb, SMB_STACK_POP);
			errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
			free(qwkbuf);
			smb_freemsgmem(&msg);
			return;
		}
		if (cfg.sys_misc & SM_FASTMAIL)
			offset = smb_fallocdat(&smb, length, 1);
		else
			offset = smb_allocdat(&smb, length, 1);
		smb_close_da(&smb);

		smb_fseek(smb.sdt_fp, offset, SEEK_SET);
		xlat = XLAT_NONE;
		smb_fwrite(&smb, &xlat, 2, smb.sdt_fp);
		m = 2;
		for (; l < n * QWK_BLOCK_LEN && m < length; l++) {
			if (qwkbuf[l] == 0 || qwkbuf[l] == LF)
				continue;
			if (qwkbuf[l] == QWK_NEWLINE) {
				if (m <= 2)  /* Ignore blank lines at top of message */
					continue;
				smb_fwrite(&smb, crlf, 2, smb.sdt_fp);
				m += 2;
				continue;
			}
			smb_fputc(qwkbuf[l], smb.sdt_fp);
			m++;
		}

		for (ch = 0; m < length; m++)          /* Pad out with NULLs */
			smb_fputc(ch, smb.sdt_fp);
		smb_fflush(smb.sdt_fp);

		msg.hdr.offset = (uint32_t)offset;

		smb_dfield(&msg, TEXT_BODY, length);

		i = smb_addmsghdr(&smb, &msg, smb_storage_mode(&cfg, &smb));
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);

		smb_freemsgmem(&msg);
		if (i != SMB_SUCCESS) {
			errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
			smb_freemsgdat(&smb, offset, length, 1);
		}
		else {      /* Successful */
			if (inet) {
				if (cfg.inetmail_sem[0])      /* update semaphore file */
					ftouch(cmdstr(cfg.inetmail_sem, nulstr, nulstr, NULL));
				if (!(useron.exempt & FLAG('S')))
					subtract_cdt(&cfg, &useron, cfg.inetmail_cost);
			}

			useron.emails = (uint)adjustuserval(&cfg, &useron, USER_EMAILS, 1);
			logon_emails++;
			useron.etoday = (uint)adjustuserval(&cfg, &useron, USER_ETODAY, 1);

			if (qnet)
				bprintf(text[QWKNetMailSent], name, fulladdr);
			else
				bprintf(text[InternetMailSent], name, to);
			llprintf("EN", "%s (%s) sent %s NetMail to %s (%s) via QWK"
			              , sender, sender_id
			              , qnet ? "QWK":"Internet", name, qnet ? fulladdr : to);
		}

		free((char *)qwkbuf);
		return;
	}


	/****************************** FidoNet **********************************/

	if (!fidoaddr.zone || !cfg.netmail_dir[0]) {  // No fido netmail allowed
		bprintf(text[InvalidNetMailAddr], to);
		free(qwkbuf);
		return;
	}

	memset(&hdr, 0, sizeof(hdr));   /* Initialize header to null */

	if (fromhub || useron.rest & FLAG('Q')) {
		snprintf(str, sizeof str, "%.25s", block + 46);              /* From */
		truncsp(str);
		snprintf(tmp, sizeof tmp, "@%s", sender_id);
		strupr(tmp);
		strcat(str, tmp);
	}
	else
		SAFECOPY(str, (cfg.netmail_misc & NMAIL_ALIAS) || (useron.rest & FLAG('O')) ? useron.alias : useron.name);
	SAFECOPY(hdr.from, str);

	SAFECOPY(hdr.to, to);

	/* Look-up in nodelist? */

	if (cfg.netmail_cost && !(useron.exempt & FLAG('S'))) {
		if (user_available_credits(&useron) < cfg.netmail_cost) {
			bputs(text[NotEnoughCredits]);
			free(qwkbuf);
			return;
		}
		snprintf(str, sizeof str, text[NetMailCostContinueQ], cfg.netmail_cost);
		if (noyes(str)) {
			free(qwkbuf);
			return;
		}
	}

	hdr.destzone    = fidoaddr.zone;
	hdr.destnet     = fidoaddr.net;
	hdr.destnode    = fidoaddr.node;
	hdr.destpoint   = fidoaddr.point;

	for (i = 0; i < cfg.total_faddrs; i++)
		if (fidoaddr.zone == cfg.faddr[i].zone && fidoaddr.net == cfg.faddr[i].net)
			break;
	if (i == cfg.total_faddrs) {
		for (i = 0; i < cfg.total_faddrs; i++)
			if (fidoaddr.zone == cfg.faddr[i].zone)
				break;
	}
	if (i == cfg.total_faddrs)
		i = 0;
	hdr.origzone    = cfg.faddr[i].zone;
	hdr.orignet     = cfg.faddr[i].net;
	hdr.orignode    = cfg.faddr[i].node;
	hdr.origpoint   = cfg.faddr[i].point;

	smb_faddrtoa(&cfg.faddr[i], str);
	bprintf(text[NetMailing], hdr.to, smb_faddrtoa(&fidoaddr, tmp), hdr.from, str);
	tm.tm_mon = ((qwkbuf[8] & 0xf) * 10) + (qwkbuf[9] & 0xf);
	if (tm.tm_mon)
		tm.tm_mon--;
	tm.tm_mday = ((qwkbuf[11] & 0xf) * 10) + (qwkbuf[12] & 0xf);
	tm.tm_year = ((qwkbuf[14] & 0xf) * 10) + (qwkbuf[15] & 0xf) + 1900;
	tm.tm_hour = ((qwkbuf[16] & 0xf) * 10) + (qwkbuf[17] & 0xf);
	tm.tm_min = ((qwkbuf[19] & 0xf) * 10) + (qwkbuf[20] & 0xf);       /* From QWK time */
	tm.tm_sec = 0;
	safe_snprintf(hdr.time, sizeof(hdr.time), "%02u %3.3s %02u  %02u:%02u:%02u"          /* To FidoNet */
	              , tm.tm_mday, mon[tm.tm_mon], TM_YEAR(tm.tm_year)
	              , tm.tm_hour, tm.tm_min, tm.tm_sec);
	hdr.attr = (FIDO_LOCAL | FIDO_PRIVATE);

	if (cfg.netmail_misc & NMAIL_CRASH)
		hdr.attr |= FIDO_CRASH;
	if (cfg.netmail_misc & NMAIL_HOLD)
		hdr.attr |= FIDO_HOLD;
	if (cfg.netmail_misc & NMAIL_KILL)
		hdr.attr |= FIDO_KILLSENT;

	snprintf(str, sizeof str, "%.25s", block + 71);      /* Title */
	truncsp(str);
	p = str;
	if ((useron_is_sysop() || useron.exempt & FLAG('F'))
	    && !strnicmp(p, "CR:", 3)) {     /* Crash over-ride by sysop */
		p += 3;               /* skip CR: */
		if (*p == ' ')
			p++;             /* skip extra space if it exists */
		hdr.attr |= FIDO_CRASH;
	}

	if ((useron_is_sysop() || useron.exempt & FLAG('F'))
	    && !strnicmp(p, "FR:", 3)) {     /* File request */
		p += 3;               /* skip FR: */
		if (*p == ' ')
			p++;
		hdr.attr |= FIDO_FREQ;
	}

	if ((useron_is_sysop() || useron.exempt & FLAG('F'))
	    && !strnicmp(p, "RR:", 3)) {     /* Return receipt request */
		p += 3;               /* skip RR: */
		if (*p == ' ')
			p++;
		hdr.attr |= FIDO_RRREQ;
	}

	if ((useron_is_sysop() || useron.exempt & FLAG('F'))
	    && !strnicmp(p, "FA:", 3)) {     /* File attachment */
		p += 3;               /* skip FA: */
		if (*p == ' ')
			p++;
		hdr.attr |= FIDO_FILE;
	}

	if (subject != NULL)
		SAFECOPY(hdr.subj, subject);
	else
		SAFECOPY(hdr.subj, p);

	md(cfg.netmail_dir);
	for (i = 1; i; i++) {
		snprintf(str, sizeof str, "%s%u.msg", cfg.netmail_dir, i);
		if (!fexistcase(str))
			break;
	}
	if (!i) {
		bputs(text[TooManyEmailsToday]);
		return;
	}
	if ((fido = nopen(str, O_WRONLY | O_CREAT | O_EXCL)) == -1) {
		free(qwkbuf);
		errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_EXCL);
		return;
	}
	if (write(fido, &hdr, sizeof(hdr)) != sizeof hdr) {
		free(qwkbuf);
		errormsg(WHERE, ERR_WRITE, str, sizeof hdr);
		return;
	}

	pt_zone_kludge(&hdr, fido);

	if (cfg.netmail_misc & NMAIL_DIRECT) {
		snprintf(str, sizeof str, "\1FLAGS DIR\r\n");
		if (write(fido, str, strlen(str)) < 1)
			errormsg(WHERE, ERR_WRITE, str, 0);
	}

	l = QWK_BLOCK_LEN + kludge_hdrlen;

	length = n * QWK_BLOCK_LEN;
	while (l < length) {
		if (qwkbuf[l] == CTRL_A) {   /* Ctrl-A, so skip it and the next char */
			l++;
			if (l >= length)
				break;
		}
		else if (qwkbuf[l] != LF) {
			if (qwkbuf[l] == QWK_NEWLINE) /* QWK cr/lf char converted to hard CR */
				qwkbuf[l] = CR;
			if (write(fido, (char *)qwkbuf + l, 1) != 1)
				errormsg(WHERE, ERR_WRITE, "fidonet netmail", 1);
		}
		l++;
	}
	l = 0;
	if (write(fido, (BYTE*)&l, sizeof(BYTE)) != sizeof(BYTE))  /* Null terminator */
		errormsg(WHERE, ERR_WRITE, "fidonet netmail", sizeof(BYTE));
	close(fido);
	free((char *)qwkbuf);
	if (cfg.netmail_sem[0])      /* update semaphore file */
		ftouch(cmdstr(cfg.netmail_sem, nulstr, nulstr, NULL));
	if (!(useron.exempt & FLAG('S')))
		subtract_cdt(&cfg, &useron, cfg.netmail_cost);

	useron.emails = (uint)adjustuserval(&cfg, &useron, USER_EMAILS, 1);
	logon_emails++;
	useron.etoday = (uint)adjustuserval(&cfg, &useron, USER_ETODAY, 1);

	bprintf(text[FidoNetMailSent], hdr.to, smb_faddrtoa(&fidoaddr, tmp));
	llprintf("EN", "%s sent NetMail to %s @%s via QWK"
	         , sender_id
	         , hdr.to, smb_faddrtoa(&fidoaddr, tmp));
}

/****************************************************************************/
/* Internet mail															*/
/****************************************************************************/
bool sbbs_t::inetmail(const char *into, const char *subj, int mode, smb_t* resmb, smbmsg_t* remsg, str_list_t cc_list)
{
	char        str[256], str2[256], msgpath[256], ch, *p;
	char        tmp[512];
	char        title[256] = "";
	char        name[256] = "";
	char        addr[256] = "";
	const char* editor = NULL;
	const char* charset = NULL;
	char        your_addr[128];
	int         i, x, file;
	long        l;
	FILE *      instream;
	smbmsg_t    msg;

	if ((!useron_is_sysop() && !(cfg.inetmail_misc & NMAIL_ALLOW)) || useron.rest & FLAG('M')) {
		bputs(text[NoNetMailAllowed]);
		return false;
	}

	str_list_t rcpt_list = strListInit();
	if (into != NULL && *into)
		strListPush(&rcpt_list, into);
	for (i = 0; cc_list != NULL && cc_list[i] != NULL; i++) {
		char* p = cc_list[i];
		char* at = strchr(p, '@');
		if (at == NULL) {
			bprintf(text[InvalidNetMailAddr], p);
			continue;
		}
		while (at > p && *at > ' ')
			at--;
		p = at;
		SKIP_WHITESPACE(p);
		uint16_t net_type = smb_netaddr_type(p);
		if (net_type != NET_INTERNET) {
			bprintf(text[InvalidNetMailAddr], p);
			break;
		}
		strListPush(&rcpt_list, p);
	}
	strListStripStrings(rcpt_list, "<>");
	size_t rcpt_count = strListDedupe(&rcpt_list, /* case-sensitive */ false);

	if (useron.etoday + rcpt_count > cfg.level_emailperday[useron.level] && !useron_is_sysop() && !(useron.exempt & FLAG('M'))) {
		strListFree(&rcpt_list);
		bputs(text[TooManyEmailsToday]);
		return false;
	}

	if (subj != NULL)
		SAFECOPY(title, subj);
	if (remsg != NULL) {
		if (title[0] == 0 && remsg->subj != NULL)
			SAFECOPY(title, remsg->subj);
		if (remsg->from_net.addr != NULL && rcpt_count < 1) {
			strListPush(&rcpt_list, smb_netaddrstr(&remsg->from_net, addr));
			rcpt_count = 1;
		}
	}

	/* Get this user's Internet mailing address */
	usermailaddr(&cfg, your_addr
	             , (cfg.inetmail_misc & NMAIL_ALIAS) || (useron.rest & FLAG('O')) ? useron.alias : useron.name);

	if (rcpt_count > 1) { /* remove "self" from reply-all list */
		int found = strListFind(rcpt_list, your_addr, /* case_sensitive */ false);
		if (found >= 0) {
			strListDelete(&rcpt_list, found);
			rcpt_count--;
		}
	}

	if (rcpt_count < 1) {
		bprintf(text[InvalidNetMailAddr], into);
		strListFree(&rcpt_list);
		return false;
	}

	if (cfg.inetmail_cost && !(useron.exempt & FLAG('S'))) {
		if (user_available_credits(&useron) < cfg.inetmail_cost * rcpt_count) {
			strListFree(&rcpt_list);
			bputs(text[NotEnoughCredits]);
			return false;
		}
		SAFEPRINTF(str, text[NetMailCostContinueQ], cfg.inetmail_cost * rcpt_count);
		if (noyes(str)) {
			strListFree(&rcpt_list);
			return false;
		}
	}

	char to_list[512];
	strListJoin(rcpt_list, to_list, sizeof(to_list), ", ");

	bprintf(text[InternetMailing], to_list, your_addr);
	action = NODE_SMAL;
	nodesync();

	if (remsg != NULL && resmb != NULL && !(mode & WM_QUOTE)) {
		if (quotemsg(resmb, remsg, /* include tails: */ true))
			mode |= WM_QUOTE;
	}

	msg_tmp_fname(useron.xedit, msgpath, sizeof(msgpath));
	if (!writemsg(msgpath, nulstr, title, WM_NETMAIL | mode, INVALID_SUB, to_list, /* from: */ your_addr, &editor, &charset)) {
		strListFree(&rcpt_list);
		bputs(text[Aborted]);
		return false;
	}

	if (mode & WM_FILE) {
		SAFEPRINTF2(str2, "%sfile/%04u.out", cfg.data_dir, useron.number);
		(void)MKDIR(str2);
		SAFEPRINTF3(str2, "%sfile/%04u.out/%s", cfg.data_dir, useron.number, title);
		if (fexistcase(str2)) {
			strListFree(&rcpt_list);
			bprintf(text[FileAlreadyThere], str2);
			(void)remove(msgpath);
			return false;
		}
		{ /* Remote */
			char keys[128];
			xfer_prot_menu(XFER_UPLOAD, &useron, keys, sizeof keys);
			SAFECAT(keys, quit_key(str));
			mnemonics(text[ProtocolOrQuit]);
			ch = (char)getkeys(keys, 0);
			if (ch == quit_key() || sys_status & SS_ABORT) {
				bputs(text[Aborted]);
				strListFree(&rcpt_list);
				(void)remove(msgpath);
				return false;
			}
			x = protnum(ch, XFER_UPLOAD);
			if (x < cfg.total_prots)   /* This should be always */
				protocol(cfg.prot[x], XFER_UPLOAD, str2, nulstr, true);
		}
		SAFEPRINTF2(tmp, "%s%s", cfg.temp_dir, title);
		if (!fexistcase(str2) && fexistcase(tmp))
			mv(tmp, str2, 0);
		l = (long)flength(str2);
		if (l > 0)
			bprintf(text[FileNBytesReceived], title, ultoac(l, tmp));
		else {
			bprintf(text[FileNotReceived], title);
			strListFree(&rcpt_list);
			(void)remove(msgpath);
			return false;
		}
	}

	smb_t smb;
	if ((i = smb_open_sub(&cfg, &smb, INVALID_SUB)) != SMB_SUCCESS) {
		strListFree(&rcpt_list);
		errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
		return false;
	}

	if ((instream = fnopen(&file, msgpath, O_RDONLY | O_BINARY)) == NULL) {
		strListFree(&rcpt_list);
		smb_close(&smb);
		errormsg(WHERE, ERR_OPEN, msgpath, O_RDONLY | O_BINARY);
		return false;
	}
	long length = (long)filelength(file);
	if (length < 1) {
		strListFree(&rcpt_list);
		fclose(instream);
		smb_close(&smb);
		errormsg(WHERE, ERR_LEN, msgpath, length);
		return false;
	}
	char* msgbuf;
	if ((msgbuf = (char*)malloc(length + 1)) == NULL) {
		strListFree(&rcpt_list);
		fclose(instream);
		smb_close(&smb);
		errormsg(WHERE, ERR_ALLOC, msgpath, length);
		return false;
	}
	if (fread(msgbuf, sizeof(char), length, instream) != (size_t)length) {
		strListFree(&rcpt_list);
		fclose(instream);
		free(msgbuf);
		smb_close(&smb);
		errormsg(WHERE, ERR_READ, msgpath, length);
		return false;
	}
	msgbuf[length] = 0;
	fclose(instream);

	memset(&msg, 0, sizeof(smbmsg_t));
	msg.hdr.version = smb_ver();
	if (mode & WM_FILE)
		msg.hdr.auxattr |= (MSG_FILEATTACH | MSG_KILLFILE);
	msg.hdr.when_written = smb_when(time(NULL), sys_timezone(&cfg));
	msg.hdr.when_imported.time = time32(NULL);
	msg.hdr.when_imported.zone = sys_timezone(&cfg);

	msg.hdr.netattr |= NETMSG_LOCAL;
	if (cfg.inetmail_misc & NMAIL_KILL)
		msg.hdr.netattr |= NETMSG_KILLSENT;

	if (rcpt_count > 1)
		smb_hfield_str(&msg, RECIPIENTLIST, to_list);

	smb_hfield_str(&msg, SENDER, (cfg.inetmail_misc & NMAIL_ALIAS) || (useron.rest & FLAG('O')) ? useron.alias : useron.name);

	SAFEPRINTF(str, "%u", useron.number);
	smb_hfield_str(&msg, SENDEREXT, str);

	/* Security logging */
	msg_client_hfields(&msg, &client);
	smb_hfield_str(&msg, SENDERSERVER, server_host_name());

	normalize_msg_hfield_encoding(charset, title, sizeof title);

	smb_hfield_str(&msg, SUBJECT, title);

	editor_info_to_msg(&msg, editor, charset);

	i = savemsg(&cfg, &smb, &msg, &client, server_host_name(), msgbuf, remsg);
	free(msgbuf);

	if (i != SMB_SUCCESS) {
		smb_close(&smb);
		smb_freemsgmem(&msg);
		strListFree(&rcpt_list);
		errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
		return false;
	}

	for (rcpt_count = 0; rcpt_list[rcpt_count] != NULL; rcpt_count++) {

		uint16_t net_type = smb_netaddr_type(rcpt_list[rcpt_count]);
		if (net_type != NET_INTERNET) {
			bprintf(text[InvalidNetMailAddr], rcpt_list[rcpt_count]);
			break;
		}

		/* Get destination user address */
		if ((p = strrchr(rcpt_list[rcpt_count], '<')) != NULL) {
			p++;
			SKIP_WHITESPACE(p);
			SAFECOPY(addr, p);
			p = strrchr(addr, '>');
			if (p == NULL) {
				bprintf(text[InvalidNetMailAddr], rcpt_list[rcpt_count]);
				break;
			}
			*p = 0;
		} else {
			p = rcpt_list[rcpt_count];
			SKIP_WHITESPACE(p);
			SAFECOPY(addr, p);
		}
		truncsp(addr);

		/* Get destination user name */
		p = rcpt_list[rcpt_count];
		SKIP_WHITESPACE(p);
		SAFECOPY(name, p);
		p = strrchr(name, '<');
		if (!p)
			p = strrchr(name, '@');
		if (!p)
			p = strrchr(name, '!');
		if (p) {
			*p = 0;
			if (name[0] == 0)
				SAFECOPY(name, addr);
		}
		truncsp(name);

		smbmsg_t newmsg;
		if ((i = smb_copymsgmem(&smb, &newmsg, &msg)) != SMB_SUCCESS) {
			errormsg(WHERE, "copying", "message header", i, smb.last_error);
			break;
		}

		smb_hfield_str(&newmsg, RECIPIENT, name);
		smb_hfield(&newmsg, RECIPIENTNETTYPE, sizeof(net_type), &net_type);
		smb_hfield_str(&newmsg, RECIPIENTNETADDR, addr);
		i = smb_addmsghdr(&smb, &newmsg, smb_storage_mode(&cfg, &smb));
		smb_freemsgmem(&newmsg);
		if (i != SMB_SUCCESS) {
			errormsg(WHERE, "adding", "message header", i, smb.last_error);
			break;
		}
	}

	if (rcpt_count > 1)
		smb_incmsg_dfields(&smb, &msg, (uint16_t)(rcpt_count - 1));

	smb_close(&smb);

	smb_freemsgmem(&msg);
	strListFree(&rcpt_list);

	if (mode & WM_FILE && online == ON_REMOTE)
		autohangup();

	if (cfg.inetmail_sem[0])      /* update semaphore file */
		ftouch(cmdstr(cfg.inetmail_sem, nulstr, nulstr, NULL));

	if (!(useron.exempt & FLAG('S')))
		subtract_cdt(&cfg, &useron, cfg.inetmail_cost * rcpt_count);

	useron.emails = (uint)adjustuserval(&cfg, &useron, USER_EMAILS, rcpt_count);
	logon_emails += rcpt_count;
	useron.etoday = (uint)adjustuserval(&cfg, &useron, USER_ETODAY, rcpt_count);

	bprintf(text[InternetMailSent], to_list);
	llprintf("EN", "sent Internet Mail to %s", to_list);
	return true;
}

bool sbbs_t::qnetmail(const char *into, const char *subj, int mode, smb_t* resmb, smbmsg_t* remsg)
{
	char        str[256], msgpath[128], fulladdr[128]
	, buf[SDT_BLOCK_LEN], *addr;
	char        title[128] = "";
	char        to[128] = "";
	const char* editor = NULL;
	const char* charset = NULL;
	ushort      xlat = XLAT_NONE, net = NET_QWK, touser;
	int         i, j, x, file;
	ulong       length;
	off_t       offset;
	FILE *      instream;
	smbmsg_t    msg;

	if (useron.etoday >= cfg.level_emailperday[useron.level] && !useron_is_sysop() && !(useron.exempt & FLAG('M'))) {
		bputs(text[TooManyEmailsToday]);
		return false;
	}

	if (into != NULL)
		SAFECOPY(to, into);
	if (subj != NULL)
		SAFECOPY(title, subj);

	if (useron.rest & FLAG('M')) {
		bputs(text[NoNetMailAllowed]);
		return false;
	}

	addr = strrchr(to, '@');
	if (!addr) {
		bprintf(text[InvalidNetMailAddr], to);
		return false;
	}
	*addr = 0;
	addr++;
	strupr(addr);
	truncsp(addr);
	touser = qwk_route(&cfg, addr, fulladdr, sizeof(fulladdr) - 1);
	if (!fulladdr[0]) {
		bprintf(text[InvalidNetMailAddr], addr);
		return false;
	}

	truncsp(to);
	if (!stricmp(to, "SBBS") && !useron_is_sysop()) {
		bprintf(text[InvalidNetMailAddr], to);
		return false;
	}
	bprintf(text[NetMailing], to, fulladdr
	        , useron.alias, cfg.sys_id);
	action = NODE_SMAL;
	nodesync();

	if (remsg != NULL && resmb != NULL && !(mode & WM_QUOTE)) {
		if (quotemsg(resmb, remsg, /* include tails: */ true))
			mode |= WM_QUOTE;
	}

	msg_tmp_fname(useron.xedit, msgpath, sizeof(msgpath));
	if (!writemsg(msgpath, nulstr, title, (mode | WM_QWKNET | WM_NETMAIL), INVALID_SUB, to, /* from: */ useron.alias, &editor, &charset)) {
		bputs(text[Aborted]);
		return false;
	}

	if ((i = smb_stack(&smb, SMB_STACK_PUSH)) != SMB_SUCCESS) {
		errormsg(WHERE, ERR_OPEN, "MAIL", i);
		return false;
	}
	snprintf(smb.file, sizeof smb.file, "%smail", cfg.data_dir);
	smb.retry_time = cfg.smb_retry_time;
	smb.subnum = INVALID_SUB;
	if ((i = smb_open(&smb)) != SMB_SUCCESS) {
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_OPEN, smb.file, i, smb.last_error);
		return false;
	}

	if (filelength(fileno(smb.shd_fp)) < 1) {   /* Create it if it doesn't exist */
		smb.status.max_crcs = cfg.mail_maxcrcs;
		smb.status.max_msgs = 0;
		smb.status.max_age = cfg.mail_maxage;
		smb.status.attr = SMB_EMAIL;
		if ((i = smb_create(&smb)) != SMB_SUCCESS) {
			smb_close(&smb);
			smb_stack(&smb, SMB_STACK_POP);
			errormsg(WHERE, ERR_CREATE, smb.file, i, smb.last_error);
			return false;
		}
	}

	if ((i = smb_locksmbhdr(&smb)) != SMB_SUCCESS) {
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_LOCK, smb.file, i, smb.last_error);
		return false;
	}

	length = (long)flength(msgpath) + 2;     /* +2 for translation string */

	if (length & 0xfff00000UL) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_LEN, msgpath, length);
		return false;
	}

	if ((i = smb_open_da(&smb)) != SMB_SUCCESS) {
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

	if (offset < 0) {
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_ALLOC, msgpath, length);
		return false;
	}

	if ((instream = fnopen(&file, msgpath, O_RDONLY | O_BINARY)) == NULL) {
		smb_freemsgdat(&smb, offset, length, 1);
		smb_unlocksmbhdr(&smb);
		smb_close(&smb);
		smb_stack(&smb, SMB_STACK_POP);
		errormsg(WHERE, ERR_OPEN, msgpath, O_RDONLY | O_BINARY);
		return false;
	}

	fseeko(smb.sdt_fp, offset, SEEK_SET);
	xlat = XLAT_NONE;
	if (fwrite(&xlat, 2, 1, smb.sdt_fp) != 1)
		errormsg(WHERE, ERR_WRITE, smb.file, 2);
	x = SDT_BLOCK_LEN - 2;              /* Don't read/write more than 255 */
	while (!feof(instream)) {
		memset(buf, 0, x);
		j = fread(buf, 1, x, instream);
		if (j < 1)
			break;
		if (j > 1 && (j != x || feof(instream)) && buf[j - 1] == LF && buf[j - 2] == CR)
			buf[j - 1] = buf[j - 2] = 0;
		if (fwrite(buf, j, 1, smb.sdt_fp) != 1)
			errormsg(WHERE, ERR_WRITE, smb.file, j);
		x = SDT_BLOCK_LEN;
	}
	fflush(smb.sdt_fp);
	fclose(instream);

	memset(&msg, 0, sizeof(smbmsg_t));
	msg.hdr.version = smb_ver();
	if (mode & WM_FILE)
		msg.hdr.auxattr |= (MSG_FILEATTACH | MSG_KILLFILE);
	msg.hdr.when_written = smb_when(time(NULL), sys_timezone(&cfg));
	msg.hdr.when_imported.time = time32(NULL);
	msg.hdr.when_imported.zone = sys_timezone(&cfg);

	msg.hdr.offset = (uint32_t)offset;

	net = NET_QWK;
	smb_hfield_str(&msg, RECIPIENT, to);
	snprintf(str, sizeof str, "%u", touser);
	smb_hfield_str(&msg, RECIPIENTEXT, str);
	smb_hfield(&msg, RECIPIENTNETTYPE, sizeof(net), &net);
	smb_hfield_str(&msg, RECIPIENTNETADDR, fulladdr);

	smb_hfield_str(&msg, SENDER, useron.alias);

	snprintf(str, sizeof str, "%u", useron.number);
	smb_hfield_str(&msg, SENDEREXT, str);

	/* Security logging */
	msg_client_hfields(&msg, &client);
	smb_hfield_str(&msg, SENDERSERVER, server_host_name());

	normalize_msg_hfield_encoding(charset, title, sizeof title);

	smb_hfield_str(&msg, SUBJECT, title);

	add_msg_ids(&cfg, &smb, &msg, /* remsg: */ NULL);

	editor_info_to_msg(&msg, editor, charset);

	smb_dfield(&msg, TEXT_BODY, length);

	i = smb_addmsghdr(&smb, &msg, smb_storage_mode(&cfg, &smb)); // calls smb_unlocksmbhdr()
	smb_close(&smb);
	smb_stack(&smb, SMB_STACK_POP);

	smb_freemsgmem(&msg);
	if (i != SMB_SUCCESS) {
		errormsg(WHERE, ERR_WRITE, smb.file, i, smb.last_error);
		smb_freemsgdat(&smb, offset, length, 1);
		return false;
	}

	useron.emails = (uint)adjustuserval(&cfg, &useron, USER_EMAILS, 1);
	logon_emails++;
	useron.etoday = (uint)adjustuserval(&cfg, &useron, USER_ETODAY, 1);

	bprintf(text[QWKNetMailSent], to, fulladdr);
	llprintf("EN", "sent QWK NetMail to %s (%s)"
	            , to, fulladdr);
	return true;
}

extern "C" bool netmail_addr_is_supported(scfg_t* cfg, const char* addr)
{
	const char* p;
	fidoaddr_t  faddr;

	if ((p = strchr(addr, '@')) == NULL)
		return false;
	p++;
	switch (smb_netaddr_type(addr)) {
		case NET_FIDO:
			if (!(cfg->netmail_misc & NMAIL_ALLOW))
				return false;
			if (cfg->total_faddrs < 1)
				return false;
			faddr = atofaddr(cfg, p);
			for (int i = 0; i < cfg->total_faddrs; i++)
				if (memcmp(&cfg->faddr[i], &faddr, sizeof(faddr)) == 0)
					return false;
			return true;
		case NET_INTERNET:
			if (!(cfg->inetmail_misc & NMAIL_ALLOW))
				return false;
			if (stricmp(p, cfg->sys_inetaddr) == 0)
				return false;
			char domain_list[MAX_PATH + 1];
			SAFEPRINTF(domain_list, "%sdomains.cfg", cfg->ctrl_dir);
			return findstr(p, domain_list) == false;
		case NET_QWK:
		{
			char fulladdr[256] = "";
			qwk_route(cfg, p, fulladdr, sizeof(fulladdr) - 1);
			return fulladdr[0] != 0;
		}
		default:
			return false;
	}
	return false;
}
