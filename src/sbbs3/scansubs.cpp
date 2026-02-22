/* Synchronet message database scanning routines */

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

/****************************************************************************/
/* Used to scan single or multiple sub-boards. 'mode' is the scan type.     */
/****************************************************************************/
void sbbs_t::scansubs(int mode)
{
	char keys[32];
	char ch, str[256] = "";
	int  i = 0;
	uint found = 0;
	uint subs_scanned = 0;
	bool subj_only = false;

	bool invoked;
	exec_mod("scan sub-boards", cfg.scansubs_mod, &invoked, "0 %u", mode);
	if (invoked)
		return;

	mnemonics(text[SubGroupOrAll]);
	SAFEPRINTF2(keys, "%s%c\r", text[SubGroupKeys], all_key());
	ch = (char)getkeys(keys, 0);
	if (sys_status & SS_ABORT || ch == CR)
		return;

	if (ch != all_key() && mode & (SCAN_FIND | SCAN_TOYOU)) {
		if (text[DisplaySubjectsOnlyQ][0])
			subj_only = yesno(text[DisplaySubjectsOnlyQ]);
		if (sys_status & SS_ABORT)
			return;
		if ((mode & SCAN_TOYOU) && !(mode & SCAN_UNREAD)
		    && text[DisplayUnreadMessagesOnlyQ][0] && yesno(text[DisplayUnreadMessagesOnlyQ]))
			mode |= SCAN_UNREAD;
		if (mode & SCAN_FIND) {
			bputs(text[SearchStringPrompt]);
			if (!getstr(str, 40, K_LINE | K_UPPER))
				return;
			if (subj_only) {
				if (ch == text[SubGroupKeys][0] /* 'S' */) {
					found = listsub(usrsub[curgrp][cursub[curgrp]], SCAN_FIND, 0, str);
					subs_scanned++;
				} else if (ch == text[SubGroupKeys][1] /* 'G' */)
					for (i = 0; i < usrsubs[curgrp] && !msgabort(); i++) {
						found = listsub(usrsub[curgrp][i], SCAN_FIND, 0, str);
						subs_scanned++;
					}
				llprintf(nulstr, "searched %u sub-boards for '%s'", subs_scanned, str);
				if (!found)
					term->newline();
				return;
			}
		}
		else if (mode & SCAN_TOYOU && subj_only) {
			if (ch == text[SubGroupKeys][0] /* 'S' */)
				found = listsub(usrsub[curgrp][cursub[curgrp]], mode, 0, NULL);
			else if (ch == text[SubGroupKeys][1] /* 'G' */)
				for (i = 0; i < usrsubs[curgrp] && !msgabort(); i++) {
					if (subscan[usrsub[curgrp][i]].cfg & SUB_CFG_SSCAN)
						found = listsub(usrsub[curgrp][i], mode, 0, NULL);
				}
			if (!found)
				term->newline();
			return;
		}
	}

	if (ch == text[SubGroupKeys][0] /* 'S' */) {
		if (useron.misc & (RIP) && !(useron.misc & EXPERT)) {
			menu("msgscan");
		}
		i = scanposts(usrsub[curgrp][cursub[curgrp]], mode, str);
		subs_scanned++;
		bputs(text[MessageScan]);
		if (i)
			bputs(text[MessageScanAborted]);
		else
			bprintf(text[MessageScanComplete], subs_scanned);
		return;
	}
	if (ch == text[SubGroupKeys][1] /* 'G' */) {
		if (useron.misc & (RIP) && !(useron.misc & EXPERT)) {
			menu("msgscan");
		}
		for (i = 0; i < usrsubs[curgrp] && !msgabort(); i++) {
			if ((mode & SCAN_NEW) && !(subscan[usrsub[curgrp][i]].cfg & SUB_CFG_NSCAN) && !(cfg.sub[usrsub[curgrp][i]]->misc & SUB_FORCED))
				continue;
			if ((mode & SCAN_TOYOU) && !(subscan[usrsub[curgrp][i]].cfg & SUB_CFG_SSCAN))
				continue;
			if ((mode & SCAN_POLLS) && cfg.sub[usrsub[curgrp][i]]->misc & SUB_NOVOTING)
				continue;
			if (mode & SCAN_POLLS)
				progress(text[Scanning], i, usrsubs[curgrp]);
			if (scanposts(usrsub[curgrp][i], mode, str))
				break;
			subs_scanned++;
		}
		if (mode & SCAN_POLLS) {
			progress(text[Done], subs_scanned, usrsubs[curgrp]);
			term->cleartoeol();
		}
		bputs(text[MessageScan]);
		if (i == usrsubs[curgrp])
			bprintf(text[MessageScanComplete], subs_scanned);
		else
			bputs(text[MessageScanAborted]);
		return;
	}

	scanallsubs(mode);
}

/****************************************************************************/
/* Performs a new message scan of all sub-boards							*/
/****************************************************************************/
void sbbs_t::scanallsubs(int mode)
{
	char  str[256] = "";
	int   i, j;
	uint  found = 0;
	int   subs_scanned = 0;
	uint* sub;
	int   total_subs = 0;
	bool  subj_only = false;

	bool invoked;
	exec_mod("scan sub-boards", cfg.scansubs_mod, &invoked, "1 %u", mode);
	if (invoked)
		return;

	if (mode & (SCAN_FIND | SCAN_TOYOU)) {
		if (text[DisplaySubjectsOnlyQ][0])
			subj_only = yesno(text[DisplaySubjectsOnlyQ]);
		if (sys_status & SS_ABORT)
			return;
		if ((mode & SCAN_TOYOU) && !(mode & SCAN_UNREAD)
		    && text[DisplayUnreadMessagesOnlyQ][0] && yesno(text[DisplayUnreadMessagesOnlyQ]))
			mode |= SCAN_UNREAD;
		if (mode & SCAN_FIND) {
			bputs(text[SearchStringPrompt]);
			if (!getstr(str, 40, K_LINE | K_UPPER))
				return;
			if (subj_only) {
				for (i = 0; i < usrgrps; i++) {
					for (j = 0; j < usrsubs[i] && !msgabort(); j++) {
						found = listsub(usrsub[i][j], SCAN_FIND, 0, str);
						subs_scanned++;
					}
					if (j < usrsubs[i])
						break;
				}
				if (!found)
					term->newline();
				llprintf(nulstr, "searched %u sub-boards for '%s'"
				         , subs_scanned, str);
				return;
			}
		}
		else if ((mode & SCAN_TOYOU) && subj_only) {
			for (i = 0; i < usrgrps; i++) {
				for (j = 0; j < usrsubs[i] && !msgabort(); j++) {
					if (subscan[usrsub[i][j]].cfg & SUB_CFG_SSCAN)
						found = listsub(usrsub[i][j], mode, 0, NULL);
				}
				if (j < usrsubs[i])
					break;
			}
			if (!found)
				term->newline();
			return;
		}
	}

	if (useron.misc & (RIP) && !(useron.misc & EXPERT)) {
		menu("msgscan");
	}
	if ((sub = (uint*)malloc(sizeof(uint) * cfg.total_subs)) == NULL) {
		errormsg(WHERE, ERR_ALLOC, "subs", sizeof(uint) * cfg.total_subs);
		return;
	}

	for (i = 0; i < usrgrps; i++)
		for (j = 0; j < usrsubs[i]; j++) {
			if ((mode & SCAN_NEW) && !(subscan[usrsub[i][j]].cfg & SUB_CFG_NSCAN) && !(cfg.sub[usrsub[i][j]]->misc & SUB_FORCED))
				continue;
			if ((mode & SCAN_TOYOU) && !(subscan[usrsub[i][j]].cfg & SUB_CFG_SSCAN))
				continue;
			if ((mode & SCAN_POLLS) && cfg.sub[usrsub[i][j]]->misc & SUB_NOVOTING)
				continue;
			sub[total_subs++] = usrsub[i][j];
		}
	for (i = 0; i < total_subs && !msgabort(); i++) {
		if (mode & SCAN_POLLS)
			progress(text[Scanning], i, total_subs);
		if (scanposts(sub[i], mode, str))
			break;
	}
	subs_scanned = i;
	free(sub);
	if (mode & SCAN_POLLS) {
		progress(text[Done], subs_scanned, total_subs);
		term->cleartoeol();
	}
	bputs(text[MessageScan]);
	if (subs_scanned < total_subs) {
		bputs(text[MessageScanAborted]);
		return;
	}
	bprintf(text[MessageScanComplete], subs_scanned);
	if (mode & SCAN_NEW && !(mode & (SCAN_MSGSONLY | SCAN_BACK | SCAN_TOYOU))
	    && useron.misc & ANFSCAN && !(useron.rest & FLAG('T')) && ns_time != 0) {
		xfer_cmds++;
		scanalldirs(FL_ULTIME);
	}
}

void sbbs_t::new_scan_ptr_cfg()
{
	char     keys[32];
	int      i, j;
	int      s;
	uint32_t l;
	time_t   t;
	uint     total_subs;
	uint     subs;

	while (online) {
		bputs(text[CfgGrpLstHdr]);
		for (i = 0; i < usrgrps && !msgabort(); i++) {
			checkline();
			if (i < 9)
				outchar(' ');
			if (i < 99)
				outchar(' ');
			bprintf(text[CfgGrpLstFmt], i + 1, cfg.grp[usrgrp[i]]->lname);
		}
		sync();
		mnemonics(text[WhichOrAll]);
		snprintf(keys, sizeof keys, "%c%c", all_key(), quit_key());
		s = getkeys(keys, usrgrps);
		if (!s || s == -1 || s == quit_key())
			break;
		if (s == all_key()) {
			mnemonics(text[SetMsgPtrPrompt]);
			SAFEPRINTF2(keys, "%s%c", text[DateLastKeys], quit_key());
			s = getkeys(keys, 9999);
			if (s == -1 || s == quit_key())
				continue;
			if (s == text[DateLastKeys][0]) {
				t = time(NULL);
				for (i = 0, total_subs = 0; i < usrgrps; i++)
					total_subs += usrsubs[i];
				if (inputnstime(&t) && !(sys_status & SS_ABORT)) {
					for (i = 0, subs = 0; i < usrgrps && online; i++) {
						for (j = 0; j < usrsubs[i] && online; j++) {
							progress(text[LoadingMsgPtrs], subs++, total_subs);
							checkline();
							subscan[usrsub[i][j]].ptr = getmsgnum(usrsub[i][j], t);
						}
					}
					progress(text[LoadingMsgPtrs], subs, total_subs);
				}
				continue;
			}
			if (s == text[DateLastKeys][1])
				s = 0;
			if (s)
				s &= ~0x80000000L;
			for (i = 0, total_subs = 0; i < usrgrps; i++)
				total_subs += usrsubs[i];
			for (i = 0, subs = 0; i < usrgrps; i++)
				for (j = 0; j < usrsubs[i] && online; j++) {
					progress(text[LoadingMsgPtrs], subs++, total_subs);
					checkline();
					if (s == 0) {
						subscan[usrsub[i][j]].ptr = ~0;
						continue;
					}
					getlastmsg(usrsub[i][j], &l, 0);
					if (s > (int)l)
						subscan[usrsub[i][j]].ptr = 0;
					else
						subscan[usrsub[i][j]].ptr = l - s;
				}
			progress(text[LoadingMsgPtrs], subs, total_subs);
			continue;
		}
		i = (s & ~0x80000000L) - 1;
		while (online) {
			l = 0;
			bprintf(text[CfgSubLstHdr], cfg.grp[usrgrp[i]]->lname);
			for (j = 0; j < usrsubs[i] && !msgabort(); j++) {
				checkline();
				if (j < 9)
					outchar(' ');
				if (j < 99)
					outchar(' ');
				t = getmsgtime(usrsub[i][j], subscan[usrsub[i][j]].ptr);
				if (t > l)
					l = (uint32_t)t;
				bprintf(text[SubPtrLstFmt], j + 1, cfg.sub[usrsub[i][j]]->lname
				        , timestr(t), nulstr);
			}
			sync();
			mnemonics(text[WhichOrAll]);
			snprintf(keys, sizeof keys, "%c%c", all_key(), quit_key());
			s = getkeys(keys, usrsubs[i]);
			if (sys_status & SS_ABORT) {
				term->lncntr = 0;
				return;
			}
			if (s == -1 || !s || s == quit_key())
				break;
			if (s == all_key()) {    /* The entire group */
				mnemonics(text[SetMsgPtrPrompt]);
				SAFEPRINTF2(keys, "%s%c", text[DateLastKeys], quit_key());
				s = getkeys(keys, 9999);
				if (s == -1 || s == quit_key())
					continue;
				if (s == text[DateLastKeys][0]) {
					t = l;
					if (inputnstime(&t) && !(sys_status & SS_ABORT)) {
						for (j = 0; j < usrsubs[i] && online; j++) {
							progress(text[LoadingMsgPtrs], j, usrsubs[i]);
							checkline();
							subscan[usrsub[i][j]].ptr = getmsgnum(usrsub[i][j], t);
						}
						progress(text[LoadingMsgPtrs], j, usrsubs[i]);
					}
					continue;
				}
				if (s == text[DateLastKeys][1])
					s = 0;
				if (s)
					s &= ~0x80000000L;
				for (j = 0; j < usrsubs[i] && online; j++) {
					progress(text[LoadingMsgPtrs], j, usrsubs[i]);
					checkline();
					if (s == 0) {
						subscan[usrsub[i][j]].ptr = ~0;
						continue;
					}
					getlastmsg(usrsub[i][j], &l, 0);
					if (s > (int)l)
						subscan[usrsub[i][j]].ptr = 0;
					else
						subscan[usrsub[i][j]].ptr = l - s;
				}
				progress(text[LoadingMsgPtrs], j, usrsubs[i]);
				continue;
			}
			else {
				j = (s & ~0x80000000L) - 1;
				mnemonics(text[SetMsgPtrPrompt]);
				SAFEPRINTF2(keys, "%s%c", text[DateLastKeys], quit_key());
				s = getkeys(keys, 9999);
				if (s == -1 || s == quit_key())
					continue;
				if (s == text[DateLastKeys][0]) {
					t = getmsgtime(usrsub[i][j], subscan[usrsub[i][j]].ptr);
					if (inputnstime(&t) && !(sys_status & SS_ABORT)) {
						bputs(text[LoadingMsgPtrs]);
						subscan[usrsub[i][j]].ptr = getmsgnum(usrsub[i][j], t);
					}
					continue;
				}
				if (s == text[DateLastKeys][1]) {
					subscan[usrsub[i][j]].ptr = ~0;
					continue;
				}
				if (s)
					s &= ~0x80000000L;
				getlastmsg(usrsub[i][j], &l, 0);
				if (s > (int)l)
					subscan[usrsub[i][j]].ptr = 0;
				else
					subscan[usrsub[i][j]].ptr = l - s;
			}
		}
	}
}

void sbbs_t::new_scan_cfg(uint misc)
{
	char keys[32];
	int  s;
	int  i, j;
	uint t;

	while (online) {
		bputs(text[CfgGrpLstHdr]);
		for (i = 0; i < usrgrps && !msgabort(); i++) {
			checkline();
			if (i < 9)
				outchar(' ');
			if (i < 99)
				outchar(' ');
			bprintf(text[CfgGrpLstFmt], i + 1, cfg.grp[usrgrp[i]]->lname);
		}
		sync();
		if (misc & SUB_CFG_NSCAN)
			mnemonics(text[NScanCfgWhichGrp]);
		else
			mnemonics(text[SScanCfgWhichGrp]);
		s = getnum(i);
		if (s < 1)
			break;
		i = s - 1;
		while (online) {
			if (misc & SUB_CFG_NSCAN)
				misc &= ~SUB_CFG_YSCAN;
			bprintf(text[CfgSubLstHdr], cfg.grp[usrgrp[i]]->lname);
			for (j = 0; j < usrsubs[i] && !msgabort(); j++) {
				checkline();
				if (j < 9)
					outchar(' ');
				if (j < 99)
					outchar(' ');
				bprintf(text[CfgSubLstFmt], j + 1
				        , cfg.sub[usrsub[i][j]]->lname
				        , subscan[usrsub[i][j]].cfg & misc ?
				        (misc & SUB_CFG_NSCAN && subscan[usrsub[i][j]].cfg & SUB_CFG_YSCAN) ?
				        text[ToYouOnly] : text[On] : text[Off]);
			}
			sync();
			if (misc & SUB_CFG_NSCAN)
				mnemonics(text[NScanCfgWhichSub]);
			else
				mnemonics(text[SScanCfgWhichSub]);
			snprintf(keys, sizeof keys, "%c%c", all_key(), quit_key());
			s = getkeys(keys, usrsubs[i]);
			if (sys_status & SS_ABORT) {
				term->lncntr = 0;
				return;
			}
			if (!s || s == -1 || s == quit_key())
				break;
			if (s == all_key()) {
				t = subscan[usrsub[i][0]].cfg & misc;
				if (misc & SUB_CFG_NSCAN && !t && !(useron.misc & FLAG('Q')))
					if (!noyes(text[MsgsToYouOnlyQ]))
						misc |= SUB_CFG_YSCAN;
				for (j = 0; j < usrsubs[i] && online; j++) {
					checkline();
					if (t)
						subscan[usrsub[i][j]].cfg &= ~misc;
					else  {
						if (misc & SUB_CFG_NSCAN)
							subscan[usrsub[i][j]].cfg &= ~SUB_CFG_YSCAN;
						subscan[usrsub[i][j]].cfg |= misc;
					}
				}
				continue;
			}
			j = (s & ~0x80000000L) - 1;
			if (misc & SUB_CFG_NSCAN && !(subscan[usrsub[i][j]].cfg & misc)) {
				if (!(useron.rest & FLAG('Q')) && !noyes(text[MsgsToYouOnlyQ]))
					subscan[usrsub[i][j]].cfg |= SUB_CFG_YSCAN;
				else
					subscan[usrsub[i][j]].cfg &= ~SUB_CFG_YSCAN;
			}
			subscan[usrsub[i][j]].cfg ^= misc;
		}
	}
}

