/* Synchronet user logout routines */

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

/****************************************************************************/
/* Function that is called after a user hangs up or logs off				*/
/****************************************************************************/
void sbbs_t::logout()
{
	char   path[MAX_PATH + 1];
	char   str[256];
	char   tmp[512];
	int    i, j;
	ushort ttoday;
	node_t node;

	now = time(NULL);

	if (!useron.number) {                 /* Not logged in, so do nothing */
		if (!online) {
			llprintf("@-", "%-6s  T:%3u sec\r\n"
			            , time_as_hhmm(&cfg, now, tmp)
			            , (uint)(now - answertime));
		}
		return;
	}
	lprintf(LOG_INFO, "logout initiated");
	if (!online && getnodedat(cfg.node_num, &node, /* lock: */ true)) {
		node.status = NODE_LOGOUT;
		putnodedat(cfg.node_num, &node);
	}

	if (useron_is_guest()) {
		putuserstr(useron.number, USER_NAME, nulstr);
		clearbatdl();
	}

	if (sys_status & SS_USERON && thisnode.status != NODE_QUIET && !(useron.rest & FLAG('Q')) && user_login_state == user_logged_on) {
		for (i = 1; i <= cfg.sys_nodes; i++) {
			if (i != cfg.node_num) {
				getnodedat(i, &node);
				if ((node.status == NODE_INUSE || node.status == NODE_QUIET)
				    && !(node.misc & NODE_AOFF) && node.useron != useron.number) {
					putnmsg(i, format_text(NodeLoggedOff
					                       , cfg.node_num
					                       , thisnode.misc & NODE_ANON
					            ? text[UNKNOWN_USER] : useron.alias));
				}
			}
		}
		if (!useron_is_sysop() || (cfg.sys_misc & SM_SYSSTAT)) {
			SAFECOPY(lastuseron, useron.alias); // TODO: race condition here
			laston_time = now;
		}
	}

	if (!online) {       /* NOT re-login */
		for (i = 0; cfg.sys_logout.cmd != nullptr && cfg.sys_logout.cmd[i] != nullptr; ++i) {
			if (cfg.sys_logout.misc[i] & EVENT_DISABLED)
				continue;
			lprintf(LOG_DEBUG, "Executing logout event: %s", cfg.sys_logout.cmd[i]);
			external(cmdstr(cfg.sys_logout.cmd[i], nulstr, nulstr, NULL, cfg.sys_logout.misc[i]), EX_OUTL | EX_OFFLINE | cfg.sys_logout.misc[i]);
		}
	}

	exec_mod("logout", cfg.logout_mod);
	SAFEPRINTF2(path, "%smsgs/%4.4u.msg", cfg.data_dir, useron.number);
	if (fexistcase(path) && !flength(path))      /* remove any 0 byte message files */
		fremove(WHERE, path);

	delfiles(cfg.temp_dir, ALLFILES);
	if (sys_status & SS_USERON) {  // Insures the useron actually went through logon()/getmsgptrs() first
		putmsgptrs();
	}
	if (!user_is_sysop(&useron))
		logofflist();
	useron.laston = (time32_t)now;

	ttoday = useron.ttoday - useron.textra;         /* billable time used prev calls */
	if (ttoday >= cfg.level_timeperday[useron.level])
		i = 0;
	else
		i = cfg.level_timeperday[useron.level] - ttoday;
	if (i > cfg.level_timepercall[useron.level])      /* i=amount of time without min */
		i = cfg.level_timepercall[useron.level];
	j = (int)(now - starttime) / 60;          /* j=billable time online in min */
	if (i < 0)
		i = 0;
	if (j < 0)
		j = 0;

	if (useron.min && j > i) {
		j -= i;                               /* j=time to deduct from min */
		llprintf(">>", "Minute Adjustment: %d", -j);
		if (useron.min > (ulong)j)
			useron.min -= j;
		else
			useron.min = 0L;
		putuserdec32(useron.number, USER_MIN, useron.min);
	}

	if (timeleft > 0 && starttime - logontime > 0)             /* extra time */
		useron.textra += (ushort)((starttime - logontime) / 60);

	putuserdec32(useron.number, USER_TEXTRA, useron.textra);
	if (last_ns_time > 0)
		putuserdatetime(useron.number, USER_NS_TIME, last_ns_time);

	if ((i = logoutuserdat(&cfg, &useron, logontime)) != USER_SUCCESS)
		errormsg(WHERE, ERR_WRITE, "user.tab", i, "updating user record in logoutuserdat");

	getusrsubs();
	getusrdirs();
	if (usrgrps > 0)
		putuserstr(useron.number, USER_CURSUB, cfg.sub[usrsub[curgrp][cursub[curgrp]]]->code);
	if (usrlibs > 0)
		putuserstr(useron.number, USER_CURDIR, cfg.dir[usrdir[curlib][curdir[curlib]]]->code);
	snprintf(str, sizeof str, "%-6s  ", time_as_hhmm(&cfg, now, tmp));
	if (sys_status & SS_USERON) {
		char ulb[64];
		char dlb[64];
		safe_snprintf(tmp, sizeof(tmp), "T:%3u   R:%3u   P:%3u   E:%3u   F:%3u   "
		              "U:%4s %u   D:%4s %u"
		              , timeon() / 60, posts_read, logon_posts
		              , logon_emails, logon_fbacks
		              , byte_estimate_to_str(logon_ulb, ulb, sizeof(ulb), 1024, /* precision: */ logon_ulb > 1024 * 1024)
		              , logon_uls
		              , byte_estimate_to_str(logon_dlb, dlb, sizeof(dlb), 1024, /* precision: */ logon_dlb > 1024 * 1204)
		              , logon_dls);
	} else
		SAFEPRINTF(tmp, "T:%3u sec", (uint)(now - answertime));
	SAFECAT(str, tmp);
	SAFECAT(str, "\r\n");
	logline("@-", str);
	sys_status &= ~SS_USERON;
	answertime = now; // In case we're re-logging on

#ifdef _WIN32
	if (startup->sound.logout[0] && !sound_muted(&cfg))
		PlaySound(startup->sound.logout, NULL, SND_ASYNC | SND_FILENAME);
#endif

	mqtt_user_logout(mqtt, &client, logontime);

	lprintf(LOG_DEBUG, "logout completed");
}

/****************************************************************************/
/****************************************************************************/
bool sbbs_t::logoff(bool prompt)
{
	if (!prompt || !noyes(text[LogOffQ])) {
		exec_mod("logoff", cfg.logoff_mod);
		user_event(EVENT_LOGOFF);
		menu("logoff");
		sync();
		hangup();
		return true;
	}
	return false;
}

/****************************************************************************/
/* Detailed usage stats for each logon                                      */
/****************************************************************************/
bool sbbs_t::logofflist()
{
	char      str[256];
	int       file;
	struct tm tm, tm_now;

	if (localtime_r(&now, &tm_now) == NULL)
		return false;
	if (localtime_r(&logontime, &tm) == NULL)
		return false;
	SAFEPRINTF4(str, "%slogs/%2.2d%2.2d%2.2d.lol", cfg.logs_dir, tm.tm_mon + 1, tm.tm_mday
	            , TM_YEAR(tm.tm_year));
	if ((file = nopen(str, O_WRONLY | O_CREAT | O_APPEND)) == -1) {
		errormsg(WHERE, ERR_OPEN, str, O_WRONLY | O_CREAT | O_APPEND);
		return false;
	}
	safe_snprintf(str, sizeof(str), "%-*.*s %-2u %-8.8s %2.2u:%2.2u %2.2u:%2.2u %3u %2u %2u %2u %2u "
	              "%2u %2u\r\n", LEN_ALIAS, LEN_ALIAS, useron.alias, cfg.node_num, connection
	              , tm.tm_hour, tm.tm_min, tm_now.tm_hour, tm_now.tm_min
	              , (int)(now - logontime) / 60, posts_read, logon_posts, logon_emails
	              , logon_fbacks, logon_uls, logon_dls);
	int wr = write(file, str, strlen(str));
	close(file);
	return wr == (int)strlen(str);
}
