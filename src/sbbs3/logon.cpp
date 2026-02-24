/* Synchronet user logon routines */

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
#include "filedat.h"

extern "C" void client_on(SOCKET sock, client_t* client, BOOL update);

/****************************************************************************/
/* Just a wrapper around logon() to handle failure logging and hangup		*/
/****************************************************************************/
bool sbbs_t::logon()
{
	bool success = logon_process();
	if (!success) {
		lprintf(LOG_NOTICE, "Logon process aborted");
		hangup();
	}
	return success;
}

/****************************************************************************/
/* Called once upon each user logging on the board							*/
/****************************************************************************/
bool sbbs_t::logon_process()
{
	char       str[256], c;
	char       tmp[512];
	char       path[MAX_PATH + 1];
	int        i, j;
	uint       mailw, mailr;
	int        kmode;
	uint       totallogons;
	node_t     node;
	struct  tm tm;

	now = time(NULL);
	if (localtime_r(&now, &tm) == NULL)
		return false;

	if (!useron.number)
		return false;

	SAFECOPY(client.user, useron.alias);
	client.usernum = useron.number;
	client_on(client_socket, &client, TRUE /* update */);

	register_login();

#ifdef JAVASCRIPT
	js_create_user_objects(js_cx, js_glob);
#endif

	if (useron.dlcps)
		cur_cps = useron.dlcps;

	if (useron.rest & FLAG('Q'))
		sys_status ^= SS_QWKLOGON;

	if (useron_is_guest()) {
		useron.misc = (cfg.new_misc & (~ASK_NSCAN));
		useron.rows = TERM_ROWS_AUTO;
		useron.cols = TERM_COLS_AUTO;
		useron.misc &= ~TERM_FLAGS;
		useron.misc |= autoterm;
		if (!set_editor(cfg.new_xedit))
			set_editor("");
		useron.prot = cfg.new_prot;
		useron.shell = cfg.new_shell;
		*useron.lang = '\0';
	}
	load_user_text();

	if (!chk_ars(startup->login_ars, &useron, &client)) {
		bputs(text[NoNodeAccess]);
		llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  Insufficient server access: %s"
		              , useron.number, useron.alias, startup->login_ars);
		return false;
	}

	if (!chk_ar(cfg.node_ar, &useron, &client)) {
		bputs(text[NoNodeAccess]);
		llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  Insufficient node access: %s"
		              , useron.number, useron.alias, cfg.node_arstr);
		return false;
	}

	if (!getnodedat(cfg.node_num, &thisnode, true)) {
		errormsg(WHERE, ERR_LOCK, "nodefile", cfg.node_num);
		return false;
	}

	if (thisnode.misc & NODE_LOCK) {
		unlocknodedat(cfg.node_num);    /* must unlock! */
		if (!useron_is_sysop() && !(useron.exempt & FLAG('N'))) {
			bputs(text[NodeLocked]);
			llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  Locked node logon attempt"
			              , useron.number, useron.alias);
			return false;
		}
		bool rmlock = yesno(text[RemoveNodeLockQ]);
		if (!getnodedat(cfg.node_num, &thisnode, true)) {
			errormsg(WHERE, ERR_LOCK, "nodefile", cfg.node_num);
			return false;
		}
		if (rmlock) {
			logline("S-", "Removed Node Lock");
			thisnode.misc &= ~NODE_LOCK;
		}
	}

	if ((useron.exempt & FLAG('Q') && useron.misc & QUIET))
		thisnode.status = NODE_QUIET;
	else
		thisnode.status = NODE_INUSE;
	action = thisnode.action = NODE_LOGN;
	thisnode.connection = node_connection;
	thisnode.misc &= ~(NODE_ANON | NODE_INTR | NODE_MSGW | NODE_POFF | NODE_AOFF);
	if (useron.chat & CHAT_NOACT)
		thisnode.misc |= NODE_AOFF;
	if (useron.chat & CHAT_NOPAGE)
		thisnode.misc |= NODE_POFF;
	thisnode.useron = useron.number;
	putnodedat(cfg.node_num, &thisnode);

	getusrsubs();
	getusrdirs();

	if ((useron.misc & CURSUB) && !useron_is_guest()) {
		for (i = 0; i < usrgrps; i++) {
			for (j = 0; j < usrsubs[i]; j++) {
				if (!strcmp(cfg.sub[usrsub[i][j]]->code, useron.cursub))
					break;
			}
			if (j < usrsubs[i]) {
				curgrp = i;
				cursub[i] = j;
				break;
			}
		}
		for (i = 0; i < usrlibs; i++) {
			for (j = 0; j < usrdirs[i]; j++)
				if (!strcmp(cfg.dir[usrdir[i][j]]->code, useron.curdir))
					break;
			if (j < usrdirs[i]) {
				curlib = i;
				curdir[i] = j;
				break;
			}
		}
	}

	const int manual_term = ANSI | RIP | PETSCII | UTF8; // Note: don't turn off NO_EXASCII flag (issue #923)
	if ((useron.misc & AUTOTERM)
	    // User manually-enabled PETSCII, but they're logging in with an ANSI (auto-detected) terminal (or vice versa)
	    || ((useron.misc & manual_term) && (useron.misc & manual_term) != (autoterm & manual_term))
		|| ((autoterm & UTF8) && !(useron.misc & UTF8))) {
		useron.misc &= ~manual_term;
		useron.misc |= (AUTOTERM | autoterm);
	}

	if (!chk_ar(cfg.shell[useron.shell]->ar, &useron, &client)) {
		if (!set_shell(cfg.new_shell))
			set_shell(0);
	}

	logon_ml = useron.level;
	logontime = time(NULL);
	starttime = logontime;
	last_ns_time = ns_time = useron.ns_time;
	// ns_time-=(useron.tlast*60); /* file newscan time == last logon time */

	if (!useron_is_sysop() && online == ON_REMOTE && !(sys_status & SS_QWKLOGON)) {
		rioctl(IOCM | ABORT); /* users can't abort anything */
		rioctl(IOCS | ABORT);
	}

	bputs(text[LoggingOn]);
	if (useron.rows != TERM_ROWS_AUTO)
		term->rows = useron.rows;
	if (useron.cols != TERM_COLS_AUTO)
		term->cols = useron.cols;
	update_nodeterm();
	if (birthdate_is_valid(&cfg, useron.birth) && tm.tm_mon + 1 == getbirthmonth(&cfg, useron.birth) && tm.tm_mday == getbirthday(&cfg, useron.birth)
	    && !(useron.rest & FLAG('Q'))) {
		if (text[HappyBirthday][0]) {
			bputs(text[HappyBirthday]);
			pause();
			cls();
		}
		user_event(EVENT_BIRTHDAY);
	}
	useron.ltoday++;

	gettimeleft();

	// TODO: GUEST accounts
	/* Inform the user of what's in their batch upload queue */
	{
		str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_UPLOAD);
		str_list_t filenames = iniGetSectionList(ini, NULL);
		for (size_t i = 0; filenames[i] != NULL; i++) {
			const char* filename = filenames[i];
			file_t      f = {{}};
			if (batch_file_get(&cfg, ini, filename, &f)) {
				bprintf(text[FileAddedToUlQueue], f.name, i + 1, cfg.max_batup);
				smb_freefilemem(&f);
			} else
				batch_file_remove(&cfg, useron.number, XFER_BATCH_UPLOAD, filename);
		}
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
	}

	/* Remove defunct files from user's batch download queue and inform them of what's remaining */
	{
		str_list_t ini = batch_list_read(&cfg, useron.number, XFER_BATCH_DOWNLOAD);
		str_list_t filenames = iniGetSectionList(ini, NULL);
		for (size_t i = 0; filenames[i] != NULL; i++) {
			const char* filename = filenames[i];
			file_t      f = {{}};
			if (!batch_file_load(&cfg, ini, filename, &f)
			    || !user_can_download(&cfg, f.dir, &useron, &client, /* reason: */ NULL)) {
				lprintf(LOG_NOTICE, "Removing defunct file from user's batch download queue: %s", filename);
				batch_file_remove(&cfg, useron.number, XFER_BATCH_DOWNLOAD, filename);
			}
			else {
				char tmp2[256];
				getfilesize(&cfg, &f);
				bprintf(text[FileAddedToBatDlQueue]
				        , f.name, i + 1, cfg.max_batdn
				        , byte_estimate_to_str(f.cost, tmp, sizeof(tmp), 1, 1)
				        , byte_estimate_to_str(f.size, tmp2, sizeof(tmp2), 1, 1)
				        , sectostr((uint)(f.size / (uint64_t)cur_cps), str));
			}
			smb_freefilemem(&f);
		}
		iniFreeStringList(ini);
		iniFreeStringList(filenames);
	}
	if (!(sys_status & SS_QWKLOGON)) {      /* QWK Nodes don't go through this */

		if (cfg.sys_pwdays && useron.pass[0]
		    && (ulong)logontime > (useron.pwmod + ((ulong)cfg.sys_pwdays * 24UL * 60UL * 60UL))) {
			bprintf(text[TimeToChangePw], cfg.sys_pwdays);

			c = 0;
			while (c < MAX(RAND_PASS_LEN, cfg.min_pwlen)) {              /* Create random password */
				str[c] = sbbs_random(43) + '0';
				if (IS_ALPHANUMERIC(str[c]))
					c++;
			}
			str[c] = 0;
			bprintf(text[YourPasswordIs], str);

			if (cfg.sys_misc & SM_PWEDIT && yesno(text[NewPasswordQ]))
				while (online) {
					bprintf(text[NewPasswordPromptFmt], cfg.min_pwlen, LEN_PASS);
					getstr(str, LEN_PASS, K_UPPER | K_LINE | K_TRIM);
					truncsp(str);
					if (chkpass(str, &useron, /* unique: */ true))
						break;
					term->newline();
				}

			while (online) {
				if (cfg.sys_misc & SM_PWEDIT) {
					term->newline();
					bputs(text[VerifyPassword]);
				}
				else {
					if (!text[NewUserPasswordVerify][0])
						break;
					bputs(text[NewUserPasswordVerify]);
				}
				console |= CON_PASSWORD;
				getstr(tmp, LEN_PASS * 2, K_UPPER);
				console &= ~CON_PASSWORD;
				if (strcmp(str, tmp)) {
					bputs(text[Wrong]); // Should be WrongPassword instead?
					continue;
				}
				break;
			}
			SAFECOPY(useron.pass, str);
			useron.pwmod = time32(NULL);
			putuserdatetime(useron.number, USER_PWMOD, useron.pwmod);
			bputs(text[PasswordChanged]);
			pause();
		}
		if (useron.ltoday > cfg.level_callsperday[useron.level]
		    && !(useron.exempt & FLAG('L'))) {
			bputs(text[NoMoreLogons]);
			llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  Out of logons"
			              , useron.number, useron.alias);
			return false;
		}
		if (useron.rest & FLAG('L') && useron.ltoday > 1) {
			bputs(text[R_Logons]);
			llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  Out of logons"
			              , useron.number, useron.alias);
			return false;
		}
		kmode = (cfg.uq & UQ_NOEXASC) | K_TRIM;
		if (!(cfg.uq & UQ_NOUPRLWR))
			kmode |= K_UPRLWR;

		if (!useron_is_guest()) {
			if (cfg.new_sif[0]) {
				safe_snprintf(str, sizeof(str), "%suser/%4.4u.dat", cfg.data_dir, useron.number);
				if (flength(str) < 1L)
					create_sif_dat(cfg.new_sif, str);
			}
		}
	}
	if (!online) {
		llprintf(LOG_NOTICE, "+!", "(%04u)  %-25s  Unsuccessful logon (disconnected)"
		              , useron.number, useron.alias);
		return false;
	}
	useron.logons++;
	i = loginuserdat(&cfg, &useron, &client, /* use_prot: */true, /* save_ars: */NULL);
	if (i != USER_SUCCESS)
		lprintf(LOG_ERR, "%s !Error %d writing user data for user #%d", __FUNCTION__, i, useron.number);
	getmsgptrs();
	sys_status |= SS_USERON;          /* moved from further down */
	// Needs to be called after SS_USERON is set
	update_nodeterm();

	mqtt_user_login(mqtt, &client);

	if (useron.rest & FLAG('Q')) {
		llprintf("++", "(%04u)  %-25s  QWK Network Connection"
		              , useron.number, useron.alias);
		return true;
	}

	/********************/
	/* SUCCESSFUL LOGON */
	/********************/
	totallogons = logonstats();
	llprintf("++", "(%04u)  %-25s  %sLogon %u - %u"
	              , useron.number, useron.alias, (sys_status & SS_FASTLOGON) ? "Fast-":"", totallogons, useron.ltoday);

	if (!(sys_status & SS_QWKLOGON)) {
		bool invoked;
		if (exec_mod("logon", cfg.logon_mod, &invoked) != 0 && invoked)
			return false;
		if (!online)
			return false;
	}

	if (thisnode.status != NODE_QUIET && (!user_is_sysop(&useron) || cfg.sys_misc & SM_SYSSTAT)) {
		int file;
		safe_snprintf(path, sizeof(path), "%slogon.lst", cfg.data_dir);
		if ((file = nopen(path, O_WRONLY | O_CREAT | O_APPEND)) == -1) {
			errormsg(WHERE, ERR_OPEN, path, O_RDWR | O_CREAT | O_APPEND);
			return false;
		}
		getuserstr(&cfg, useron.number, USER_NOTE, useron.note, sizeof(useron.note));
		getuserstr(&cfg, useron.number, USER_LOCATION, useron.location, sizeof(useron.location));
		safe_snprintf(str, sizeof(str), text[LastFewCallersFmt], cfg.node_num
		              , totallogons, useron.alias
		              , cfg.sys_misc & SM_LISTLOC ? useron.location : useron.note
		              , tm.tm_hour, tm.tm_min
		              , connection, useron.ltoday > 999 ? 999 : useron.ltoday);
		int wr = write(file, str, strlen(str));
		close(file);
		if (wr < 0)
			errormsg(WHERE, ERR_WRITE, path, strlen(str));
	}

	for (i = 0; cfg.sys_logon.cmd != nullptr && cfg.sys_logon.cmd[i] != nullptr; ++i) {
		if (cfg.sys_logon.misc[i] & EVENT_DISABLED)
			continue;
		lprintf(LOG_DEBUG, "Executing logon event: %s", cfg.sys_logon.cmd[i]);
		external(cmdstr(cfg.sys_logon.cmd[i], nulstr, nulstr, NULL, cfg.sys_logon.misc[i]), EX_STDOUT | cfg.sys_logon.misc[i]); /* EX_SH */
		if (!online)
			return false;
	}

	if (sys_status & SS_QWKLOGON)
		return true;

	sys_status |= SS_PAUSEON; /* always force pause on during this section */
	mailw = mail_waiting.get();
	mailr = mail_read.get();

	if (!(cfg.sys_misc & SM_NOSYSINFO)) {
		if (!menu("logoninfo", P_NOERROR)) {
			bprintf(text[SiSysName], cfg.sys_name);
			//bprintf(text[SiNodeNumberName],cfg.node_num,cfg.node_name);
			bprintf(text[LiUserNumberName], useron.number, useron.alias);
			bprintf(text[LiLogonsToday], useron.ltoday
					, cfg.level_callsperday[useron.level]);
			bprintf(text[LiTimeonToday], useron.ttoday
					, cfg.level_timeperday[useron.level] + useron.min);
			bprintf(text[LiMailWaiting], mailw, mailw - mailr);
			bprintf(text[LiSysopIs]
					, text[sysop_available(&cfg) ? LiSysopAvailable : LiSysopNotAvailable]);
			term->newline();
		}
	}

	if (sys_status & SS_EVENT)
		bprintf(text[ReducedTime], timestr(event_time));
	if (getnodedat(cfg.node_num, &thisnode, true)) {
		thisnode.misc &= ~(NODE_AOFF | NODE_POFF);
		if (useron.chat & CHAT_NOACT)
			thisnode.misc |= NODE_AOFF;
		if (useron.chat & CHAT_NOPAGE)
			thisnode.misc |= NODE_POFF;
		putnodedat(cfg.node_num, &thisnode);
	}

	getsmsg(useron.number);         /* Moved from further down */
	sync();
	c = 0;
	for (i = 1; i <= cfg.sys_nodes; i++) {
		if (i != cfg.node_num) {
			getnodedat(i, &node);
			if (!(cfg.sys_misc & SM_NONODELIST)
			    && (node.status == NODE_INUSE
			        || ((node.status == NODE_QUIET || node.errors) && useron_is_sysop()))) {
				if (!c)
					bputs(text[NodeLstHdr]);
				printnodedat(i, &node);
				c = 1;
			}
			if (node.status == NODE_INUSE && i != cfg.node_num && node.useron == useron.number
			    && !useron_is_sysop() && !useron_is_guest()) {
				llprintf(LOG_NOTICE, "+!", "On more than one node at the same time");
				bputs(text[UserOnTwoNodes]);
				return false;
			}
		}
	}
	for (i = 1; i <= cfg.sys_nodes; i++) {
		if (i != cfg.node_num) {
			getnodedat(i, &node);
			if (thisnode.status != NODE_QUIET
			    && (node.status == NODE_INUSE || node.status == NODE_QUIET)
			    && !(node.misc & NODE_AOFF) && node.useron != useron.number) {
				putnmsg(i, format_text(NodeLoggedOnAtNbps
				                       , cfg.node_num
				                       , thisnode.misc & NODE_ANON ? text[UNKNOWN_USER] : useron.alias
				                       , connection));
			}
		}
	}

	if (cfg.sys_exp_warn && useron.expire && useron.expire > now /* Warn user of coming */
	    && (useron.expire - now) / (1440L * 60L) <= cfg.sys_exp_warn) /* expiration */
		bprintf(text[AccountWillExpireInNDays], (useron.expire - now) / (1440L * 60L));

	if (criterrs && useron_is_sysop())
		bprintf(text[CriticalErrors], criterrs);
	if ((i = getuserxfers(&cfg, /* from: */ NULL, useron.number)) != 0)
		bprintf(text[UserXferForYou], i, i > 1 ? "s" : nulstr);
	if ((i = getuserxfers(&cfg, useron.alias, /* to: */ 0)) != 0)
		bprintf(text[UnreceivedUserXfer], i, i > 1 ? "s" : nulstr);
	sync();
	sys_status &= ~SS_PAUSEON;    /* Turn off the pause override flag */
	if (online == ON_REMOTE)
		rioctl(IOSM | ABORT);   /* Turn abort ability on */
	if (mailw) {
		uint32_t user_mail = useron.mail & ~MAIL_LM_MODE;
		int result = useron.mail & MAIL_LM_MODE;
		if (mailw == mailr) {
			if (!noyes(text[ReadYourMailNowQ]))
				result = readmail(useron.number, MAIL_YOUR, useron.mail & MAIL_LM_MODE);
		} else if (yesno(text[ReadYourUnreadMailNowQ]))
			result = readmail(useron.number, MAIL_YOUR, (useron.mail & MAIL_LM_MODE) | LM_UNREAD, /* listmsgs: */false);
		user_mail |= result & MAIL_LM_MODE;
		if (user_mail != useron.mail)
			putusermail(&cfg, useron.number, useron.mail = user_mail);
	}
	if (usrgrps && useron.misc & ASK_NSCAN && text[NScanAllGrpsQ][0] && yesno(text[NScanAllGrpsQ]))
		scanallsubs(SCAN_NEW);
	if (usrgrps && useron.misc & ASK_SSCAN && text[SScanAllGrpsQ][0] && yesno(text[SScanAllGrpsQ]))
		scanallsubs(SCAN_TOYOU | SCAN_UNREAD);

	user_login_state = user_logged_on; // notify other nodes upon (later) logoff

	return true;
}

/****************************************************************************/
/* Checks the system dsts.ini to see if it is a new day, if it is, all the  */
/* nodes' and the system's csts.tab are added to, and the dsts.ini's daily  */
/* stats are cleared. Also increments the logon values in dsts.ini if       */
/* applicable.                                                              */
/****************************************************************************/
uint sbbs_t::logonstats()
{
	char      path[MAX_PATH + 1];
	FILE*     csts;
	FILE*     dsts;
	uint      i;
	stats_t   stats;
	node_t    node;

	sys_status &= ~SS_DAILY;
	if (!getstats(&cfg, 0, &stats)) {
		errormsg(WHERE, ERR_READ, "system stats");
		return 0;
	}

	now = time(NULL);
	if (stats.date > now + (24L * 60L * 60L)) /* More than a day in the future? */
		errormsg(WHERE, ERR_CHK, "Daily stats date/time stamp", (int)stats.date);

	if (stats.date < now && !dates_are_same(now, stats.date)) {

		struct tm tm{};
		struct tm update_tm{};
		time_t t = stats.date;
		if (localtime_r(&t, &update_tm) == NULL) {
			errormsg(WHERE, ERR_CHK, "Daily stats date/time break down", (int)stats.date);
			return 0;
		}
		if (localtime_r(&now, &tm) == NULL) {
			errormsg(WHERE, ERR_CHK, "Current date/time break down", (int)stats.date);
			return 0;
		}

		sys_status |= SS_NEW_DAY;
		if (tm.tm_mon != update_tm.tm_mon)
			sys_status |= SS_NEW_MONTH;
		if (tm.tm_wday == 0 || difftime(now, stats.date) > (7 * 24 * 60 * 60))
			sys_status |= SS_NEW_WEEK;
		llprintf(LOG_NOTICE, "!=", "New Day%s%s - Prev: %s "
			          , (sys_status & SS_NEW_WEEK) ? " and Week" :""
		              , (sys_status & SS_NEW_MONTH) ? " and Month" :""
			          , timestr(stats.date));
		safe_snprintf(path, sizeof(path), "%slogon.lst", cfg.data_dir);    /* Truncate logon list (LEGACY) */
		int file;
		if ((file = nopen(path, O_TRUNC | O_CREAT | O_WRONLY)) == -1) {
			errormsg(WHERE, ERR_OPEN, path, O_TRUNC | O_CREAT | O_WRONLY);
			return 0;
		}
		close(file);
		for (i = 0; i <= cfg.sys_nodes; i++) {
			dstats_fname(&cfg, i, path, sizeof path);
			if ((dsts = fopen_dstats(&cfg, i, /* for_write: */ TRUE)) == NULL) /* doesn't have stats yet */
				continue;

			if ((csts = fopen_cstats(&cfg, i, /* for_write: */ TRUE)) == NULL) {
				fclose_dstats(dsts);
				errormsg(WHERE, ERR_OPEN, "csts.tab", i);
				continue;
			}

			if (!fread_dstats(dsts, &stats)) {
				errormsg(WHERE, ERR_READ, path, i);
			} else {
				if(stats.date > now || dates_are_same(now, stats.date))
					lprintf(LOG_NOTICE, "%s already updated on %s", path, timestr(stats.date));
				else {
					stats.date = time32(NULL);
					fwrite_cstats(csts, &stats);
					rolloverstats(&stats);
					if (!fwrite_dstats(dsts, &stats, __FUNCTION__))
						errormsg(WHERE, ERR_WRITE, path, i);
					if (i) {     /* updating a node */
						if (getnodedat(i, &node, true)) {
							node.misc |= NODE_EVENT;
							putnodedat(i, &node);
						}
					}
				}
			}
			fclose_dstats(dsts);
			fclose_cstats(csts);
		}
	}
	if (cfg.node_num == 0) /* called from event_thread() */
		return 0;

	if (thisnode.status == NODE_QUIET)       /* Quiet users aren't counted */
		return 0;

	if (user_is_sysop(&useron) && !(cfg.sys_misc & SM_SYSSTAT))
		return 0;

	for (i = 0; i < 2; i++) {
		FILE* fp = fopen_dstats(&cfg, i ? 0 : cfg.node_num, /* for_write: */ TRUE);
		if (fp == NULL) {
			errormsg(WHERE, ERR_OPEN, "dsts.ini", i);
			return 0;
		}
		if (!fread_dstats(fp, &stats)) {
			errormsg(WHERE, ERR_READ, "dsts.ini", i);
		} else {
			stats.today.logons++;
			stats.total.logons++;
			if (!fwrite_dstats(fp, &stats, __FUNCTION__))
				errormsg(WHERE, ERR_WRITE, "dsts.ini", i);
		}
		fclose_dstats(fp);
	}

	return stats.logons;
}
