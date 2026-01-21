/* Synchronet (oh, so old) data access routines */

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

/**************************************************************/
/* Functions that store and retrieve data from disk or memory */
/**************************************************************/

#include "sbbs.h"

/****************************************************************************/
/* Looks for close or perfect matches between str and valid usernames and  	*/
/* numbers and prompts user for near perfect matches in names.				*/
/* Returns the number of the matched user or 0 if unsuccessful				*/
/* Called from functions main_sec, useredit and readmailw					*/
/****************************************************************************/
uint sbbs_t::finduser(const char* name, bool silent_failure)
{
	int   file, i;
	char  buf[256], ynq[25], c, pass = 1;
	char  path[MAX_PATH + 1];
	int   l, length;
	FILE *stream;

	SKIP_WHITESPACE(name);
	i = atoi(name);
	if (i > 0) {
		username(&cfg, i, buf);
		if (buf[0] && strcmp(buf, "DELETED USER"))
			return i;
	}
	SAFEPRINTF(path, "%suser/" USER_INDEX_FILENAME, cfg.data_dir);
	if (flength(path) < 1L)
		return 0;
	if ((stream = fnopen(&file, path, O_RDONLY)) == NULL) {
		errormsg(WHERE, ERR_OPEN, path, O_RDONLY);
		return 0;
	}
	SAFEPRINTF3(ynq, "%c%c%c", yes_key(), no_key(), quit_key());
	length = (int)filelength(file);
	while (pass < 3) {
		fseek(stream, 0L, SEEK_SET);  /* seek to beginning for each pass */
		for (l = 0; l < length; l += LEN_ALIAS + 2) {
			if (!online)
				break;
			if (fread(buf, LEN_ALIAS + 2, 1, stream) != 1)
				break;
			for (c = 0; c < LEN_ALIAS; c++)
				if (buf[c] == ETX)
					break;
			buf[c] = 0;
			if (!c)      /* deleted user */
				continue;
			if (pass == 1 && matchusername(&cfg, name, buf)) {
				fclose(stream);
				return (l / (LEN_ALIAS + 2)) + 1;
			}
			if (pass == 2 && strcasestr(buf, name)) {
				char tmp[256];
				char str[256];
				snprintf(tmp, sizeof tmp, expand_atcodes(text[DoYouMeanThisUserQ], str, sizeof str), buf
				         , (uint)(l / (LEN_ALIAS + 2)) + 1);
				mnemonics(tmp);
				c = (char)getkeys(ynq, 0);
				if (sys_status & SS_ABORT) {
					fclose(stream);
					return 0;
				}
				if (c == yes_key()) {
					fclose(stream);
					return (l / (LEN_ALIAS + 2)) + 1;
				}
				if (c == quit_key()) {
					fclose(stream);
					sys_status |= SS_ABORT;
					return 0;
				}
			}
		}
		pass++;
	}
	if (!silent_failure)
		bputs(text[UnknownUser]);
	fclose(stream);
	return 0;
}

/****************************************************************************/
/* Return date/time that the specified event should run next				*/
/****************************************************************************/
extern "C" time_t getnexteventtime(const event_t* event)
{
	struct tm tm;
	time_t    t = time(NULL);
	time_t    now = t;

	if (event->misc & EVENT_DISABLED)
		return 0;

	if ((event->days & 0x7f) == 0 || event->freq != 0)
		return 0;

	if (localtime_r(&t, &tm) == NULL)
		return 0;

	tm.tm_hour = event->time / 60;
	tm.tm_min = event->time % 60;
	tm.tm_sec = 0;
	tm.tm_isdst = -1;   /* Do not adjust for DST */
	t = mktime(&tm);

	if (event->last >= t)
		t += 24 * 60 * 60; /* already ran today, so add 24hrs */

	do {
		if (t > now + (1500 * 24 * 60 * 60)) /* Handle crazy configs, e.g. Feb-29, Apr-31 */
			return 0;
		if (localtime_r(&t, &tm) == NULL)
			return 0;
		if ((event->days & (1 << tm.tm_wday))
		    && (event->mdays <= 1 || (event->mdays & (1 << tm.tm_mday)))
		    && (event->months == 0 || (event->months & (1 << tm.tm_mon))))
			break;
		t += 24 * 60 * 60;
	} while (t > 0);

	return t;
}

/****************************************************************************/
/* Return time of next forced timed event									*/
/* 'event' may be NULL														*/
/****************************************************************************/
extern "C" time_t getnextevent(scfg_t* cfg, event_t** event)
{
	int    i;
	time_t event_time = 0;
	time_t thisevent;

	for (i = 0; i < cfg->total_events; i++) {
		if (!cfg->event[i]->node || cfg->event[i]->node > cfg->sys_nodes
		    || cfg->event[i]->misc & EVENT_DISABLED)
			continue;
		if (!(cfg->event[i]->misc & EVENT_FORCE)
		    || (!(cfg->event[i]->misc & EVENT_EXCL) && cfg->event[i]->node != cfg->node_num))
			continue;

		thisevent = getnexteventtime(cfg->event[i]);
		if (thisevent <= 0)
			continue;

		if (!event_time || thisevent < event_time) {
			event_time = thisevent;
			if (event != NULL)
				*event = cfg->event[i];
		}
	}

	return event_time;
}

/****************************************************************************/
/* Fills the timeleft variable with the correct value. Hangs up on the      */
/* user if their time is up.                                                */
/* Called from functions main_sec and xfer_sec                              */
/****************************************************************************/
uint sbbs_t::gettimeleft(bool handle_out_of_time)
{
	char     str[128];
	char     tmp[512];
	event_t* nextevent{nullptr};

	now = time(NULL);

	timeleft = (uint)::gettimeleft(&cfg, &useron, starttime);

	/* Timed event time reduction handler */
	event_time = getnextevent(&cfg, &nextevent);
	if (event_time && nextevent != nullptr)
		event_code = nextevent->code;

	if (event_time && now + (time_t)timeleft > event_time) {    /* less time, set flag */
		if (event_time < now)
			timeleft = 0;
		else
			timeleft = (uint)(event_time - now);
		if (!(sys_status & SS_EVENT)) {
			lprintf(LOG_NOTICE, "Node %d Time reduced (to %s) due to upcoming event (%s) on %s"
			        , cfg.node_num, sectostr(timeleft, tmp), event_code, timestr(event_time));
			sys_status |= SS_EVENT;
		}
	}

	if ((int)timeleft < 0)  /* timeleft can't go negative */
		timeleft = 0;
	if (thisnode.status == NODE_NEWUSER) {
		timeleft = cfg.level_timepercall[cfg.new_level];
		if (timeleft < 10 * 60L)
			timeleft = 10 * 60L;
	}

	if (handle_out_of_time && !gettimeleft_inside)           /* The following code is not recursive */
	{
		gettimeleft_inside = 1;

		if (!timeleft && !useron_is_sysop() && !(sys_status & SS_LCHAT)) {
			logline(LOG_NOTICE, nulstr, "Ran out of time");
			term->saveline();
			if (sys_status & SS_EVENT)
				bprintf(text[ReducedTime], timestr(event_time));
			bputs(text[TimesUp]);
			if (!(sys_status & (SS_EVENT | SS_USERON)) && useron.cdt >= 100L * 1024L
			    && !(cfg.sys_misc & SM_NOCDTCVT)) {
				SAFEPRINTF(tmp, text[Convert100ktoNminQ], cfg.cdt_min_value);
				if (yesno(tmp)) {
					logline("  ", "Credit to Minute Conversion");
					useron.min = (uint32_t)adjustuserval(&cfg, &useron, USER_MIN, cfg.cdt_min_value);
					useron.cdt = adjustuserval(&cfg, &useron, USER_CDT, -(102400LL));
					llprintf("$-", "Credit Adjustment: %ld", -(102400L));
					llprintf("*+", "Minute Adjustment: %u", cfg.cdt_min_value);
					term->restoreline();
					gettimeleft();
					gettimeleft_inside = 0;
					return timeleft;
				}
			}
			if (cfg.sys_misc & SM_TIME_EXP && !(sys_status & SS_EVENT)
			    && !(useron.exempt & FLAG('E'))) {
				/* set to expired values */
				bputs(text[AccountHasExpired]);
				SAFEPRINTF2(str, "%s #%u Expired", useron.alias, useron.number);
				logentry("!%", str);
				if (cfg.level_misc[useron.level] & LEVEL_EXPTOVAL
				    && cfg.level_expireto[useron.level] < 10) {
					useron.flags1 = cfg.val_flags1[cfg.level_expireto[useron.level]];
					useron.flags2 = cfg.val_flags2[cfg.level_expireto[useron.level]];
					useron.flags3 = cfg.val_flags3[cfg.level_expireto[useron.level]];
					useron.flags4 = cfg.val_flags4[cfg.level_expireto[useron.level]];
					useron.exempt = cfg.val_exempt[cfg.level_expireto[useron.level]];
					useron.rest = cfg.val_rest[cfg.level_expireto[useron.level]];
					if (cfg.val_expire[cfg.level_expireto[useron.level]])
						useron.expire = (time32_t)now
						                + (cfg.val_expire[cfg.level_expireto[useron.level]] * 24 * 60 * 60);
					else
						useron.expire = 0;
					useron.level = cfg.val_level[cfg.level_expireto[useron.level]];
				}
				else {
					if (cfg.level_misc[useron.level] & LEVEL_EXPTOLVL)
						useron.level = cfg.level_expireto[useron.level];
					else
						useron.level = cfg.expired_level;
					useron.flags1 &= ~cfg.expired_flags1; /* expired status */
					useron.flags2 &= ~cfg.expired_flags2; /* expired status */
					useron.flags3 &= ~cfg.expired_flags3; /* expired status */
					useron.flags4 &= ~cfg.expired_flags4; /* expired status */
					useron.exempt &= ~cfg.expired_exempt;
					useron.rest |= cfg.expired_rest;
					useron.expire = 0;
				}
				putuserdec32(useron.number, USER_LEVEL, useron.level);
				putuserflags(useron.number, USER_FLAGS1, useron.flags1);
				putuserflags(useron.number, USER_FLAGS2, useron.flags2);
				putuserflags(useron.number, USER_FLAGS3, useron.flags3);
				putuserflags(useron.number, USER_FLAGS4, useron.flags4);
				putuserdatetime(useron.number, USER_EXPIRE, useron.expire);
				putuserflags(useron.number, USER_EXEMPT, useron.exempt);
				putuserflags(useron.number, USER_REST, useron.rest);
				exec_mod("user expired", cfg.expire_mod);
				term->restoreline();
				gettimeleft();
				gettimeleft_inside = 0;
				return timeleft;
			}
			sync();
			hangup();
		}
		gettimeleft_inside = 0;
	}
	return timeleft;
}

/****************************************************************************/
/* Read the user.tab record for the specified usernumber					*/
/* (or current useron.number) into 'useron'.								*/
/* Logs error and does *not* change/zero-out 'useron' upon failure			*/
/****************************************************************************/
bool sbbs_t::getuseron(int line, const char* function, const char *source, uint usernum)
{
	user_t user{};
	if (usernum == 0)
		usernum = useron.number;
	user.number = usernum;
	int    retval = getuserdat(&cfg, &user);
	if (retval == USER_SUCCESS) {
		useron = user;
		return true;
	}
	char str[128];
	snprintf(str, sizeof str, "getuserdat returned %d", retval);
	errormsg(line, function, source, ERR_READ, USER_DATA_FILENAME, usernum, str);
	return false;
}
