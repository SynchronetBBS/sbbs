/* Synchronet message database scanning routines */

/* $Id: scansubs.cpp,v 1.30 2019/05/02 19:37:04 rswindell Exp $ */

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
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "sbbs.h"

/****************************************************************************/
/* Used to scan single or multiple sub-boards. 'mode' is the scan type.     */
/****************************************************************************/
void sbbs_t::scansubs(long mode)
{
	char	ch,str[256];
	char 	tmp[512];
	uint	i=0,found=0;
	ulong	subs_scanned=0;
	bool	subj_only=false;

	if(cfg.scansubs_mod[0] && !scansubs_inside) {
		char cmdline[256];

		scansubs_inside = true;
		safe_snprintf(cmdline, sizeof(cmdline), "%s 0 %ld", cfg.scansubs_mod, mode);
		exec_bin(cmdline, &main_csi);
		scansubs_inside = false;
		return;
	}

	mnemonics(text[SubGroupOrAll]);
	ch=(char)getkeys("SGA\r",0);
	if(sys_status&SS_ABORT || ch==CR)
		return;

	if(ch!='A' && mode&(SCAN_FIND|SCAN_TOYOU)) {
		if(text[DisplaySubjectsOnlyQ][0])
			subj_only = yesno(text[DisplaySubjectsOnlyQ]);
		if((mode&SCAN_TOYOU) && !(mode&SCAN_UNREAD)
			&& text[DisplayUnreadMessagesOnlyQ][0] && yesno(text[DisplayUnreadMessagesOnlyQ]))
			mode|=SCAN_UNREAD;
		if(mode&SCAN_FIND) {
			bputs(text[SearchStringPrompt]);
			if(!getstr(str,40,K_LINE|K_UPPER))
				return;
			if(subj_only) {
				if(ch=='S') {
					found=listsub(usrsub[curgrp][cursub[curgrp]],SCAN_FIND,0,str);
					subs_scanned++;
				} else if(ch=='G')
					for(i=0;i<usrsubs[curgrp] && !msgabort();i++) {
						found=listsub(usrsub[curgrp][i],SCAN_FIND,0,str);
						subs_scanned++;
					}
				SAFEPRINTF2(tmp, "searched %lu sub-boards for '%s'", subs_scanned, str);
				logline(nulstr,tmp);
				if(!found)
					CRLF;
				return;
			}
		}
		else if(mode&SCAN_TOYOU && subj_only) {
			if(ch=='S')
				found=listsub(usrsub[curgrp][cursub[curgrp]],mode,0,NULL);
			else if(ch=='G')
				for(i=0;i<usrsubs[curgrp] && !msgabort();i++) {
					if(subscan[usrsub[curgrp][i]].cfg&SUB_CFG_SSCAN)
						found=listsub(usrsub[curgrp][i],mode,0,NULL);
				}
			if(!found)
				CRLF;
			return;
		}
	}

	if(ch=='S') {
		if(useron.misc&(RIP|WIP|HTML) && !(useron.misc&EXPERT)) {
			menu("msgscan");
		}
		i=scanposts(usrsub[curgrp][cursub[curgrp]],mode,str);
		subs_scanned++;
		bputs(text[MessageScan]);
		if(i) bputs(text[MessageScanAborted]);
		else bprintf(text[MessageScanComplete],subs_scanned);
		return;
	}
	if(ch=='G') {
		if(useron.misc&(RIP|WIP|HTML) && !(useron.misc&EXPERT)) {
			menu("msgscan");
		}
		for(i=0;i<usrsubs[curgrp] && !msgabort();i++) {
			if((mode&SCAN_NEW) && !(subscan[usrsub[curgrp][i]].cfg&SUB_CFG_NSCAN) && !(cfg.sub[usrsub[curgrp][i]]->misc&SUB_FORCED))
				continue;
			if((mode&SCAN_TOYOU) && !(subscan[usrsub[curgrp][i]].cfg&SUB_CFG_SSCAN))
				continue;
			if((mode&SCAN_POLLS) && cfg.sub[usrsub[curgrp][i]]->misc&SUB_NOVOTING)
				continue;
			if(mode&SCAN_POLLS)
				progress(text[Scanning], i, usrsubs[curgrp], 10);
			if(scanposts(usrsub[curgrp][i],mode,str))
				break;
			subs_scanned++;
		}
		if(mode&SCAN_POLLS) {
			progress(text[Done], subs_scanned, usrsubs[curgrp]);
			cleartoeol();
		}
		bputs(text[MessageScan]);
		if(i==usrsubs[curgrp]) bprintf(text[MessageScanComplete],subs_scanned);
			else bputs(text[MessageScanAborted]);
		return;
	}

	scanallsubs(mode);
}

/****************************************************************************/
/* Performs a new message scan of all sub-boards							*/
/****************************************************************************/
void sbbs_t::scanallsubs(long mode)
{
	char	str[256];
	char 	tmp[512];
	uint	i,j,found=0;
	ulong	subs_scanned=0;
	uint*	sub;
	ulong	total_subs=0;
	bool	subj_only=false;

	if(cfg.scansubs_mod[0] && !scansubs_inside) {
		char cmdline[256];

		scansubs_inside = true;
		safe_snprintf(cmdline, sizeof(cmdline), "%s 1 %ld", cfg.scansubs_mod, mode);
		exec_bin(cmdline, &main_csi);
		scansubs_inside = false;
		return;
	}

	if(mode&(SCAN_FIND|SCAN_TOYOU)) {
		if(text[DisplaySubjectsOnlyQ][0])
			subj_only=yesno(text[DisplaySubjectsOnlyQ]);
		if((mode&SCAN_TOYOU) && !(mode&SCAN_UNREAD)
			&& text[DisplayUnreadMessagesOnlyQ][0] && yesno(text[DisplayUnreadMessagesOnlyQ]))
			mode|=SCAN_UNREAD;
		if(mode&SCAN_FIND) {
			bputs(text[SearchStringPrompt]);
			if(!getstr(str,40,K_LINE|K_UPPER))
				return;
			if(subj_only) {
				for(i=0;i<usrgrps;i++) {
					for(j=0;j<usrsubs[i] && !msgabort();j++) {
						found=listsub(usrsub[i][j],SCAN_FIND,0,str);
						subs_scanned++;
					}
					if(j<usrsubs[i])
						break;
				}
				if(!found)
					CRLF;
				sprintf(tmp,"searched %lu sub-boards for '%s'"
					,subs_scanned,str);
				logline(nulstr,tmp);
				return;
			}
		}
		else if((mode&SCAN_TOYOU) && subj_only) {
			for(i=0;i<usrgrps;i++) {
				for(j=0;j<usrsubs[i] && !msgabort();j++) {
					if(subscan[usrsub[i][j]].cfg&SUB_CFG_SSCAN)
						found=listsub(usrsub[i][j],mode,0,NULL);
				}
				if(j<usrsubs[i])
					break;
			}
			if(!found)
				CRLF;
			return;
		}
	}

	if(useron.misc&(RIP|WIP|HTML) && !(useron.misc&EXPERT)) {
		menu("msgscan");
	}
	if((sub = (uint*)malloc(sizeof(uint) * cfg.total_subs)) == NULL) {
		errormsg(WHERE, ERR_ALLOC, "subs", sizeof(uint)*cfg.total_subs);
		return;
	}

	for(i=0; i<usrgrps; i++)
		for(j=0; j<usrsubs[i]; j++) {
			if((mode&SCAN_NEW) && !(subscan[usrsub[i][j]].cfg&SUB_CFG_NSCAN) && !(cfg.sub[usrsub[i][j]]->misc&SUB_FORCED))
				continue;
			if((mode&SCAN_TOYOU) && !(subscan[usrsub[i][j]].cfg&SUB_CFG_SSCAN))
				continue;
			if((mode&SCAN_POLLS) && cfg.sub[usrsub[i][j]]->misc&SUB_NOVOTING)
				continue;
			sub[total_subs++] = usrsub[i][j];
		}
	for(i=0; i<total_subs && !msgabort(); i++) {
		if(mode&SCAN_POLLS)
			progress(text[Scanning], i, total_subs, 10);
		if(scanposts(sub[i],mode,str))
			break;
	}
	subs_scanned = i;
	free(sub);
	if(mode&SCAN_POLLS) {
		progress(text[Done], subs_scanned, total_subs);
		cleartoeol();
	}
	bputs(text[MessageScan]);
	if(subs_scanned<total_subs) {
		bputs(text[MessageScanAborted]);
		return;
	}
	bprintf(text[MessageScanComplete],subs_scanned);
	if(mode&SCAN_NEW && !(mode&(SCAN_MSGSONLY|SCAN_BACK|SCAN_TOYOU))
		&& useron.misc&ANFSCAN && !(useron.rest&FLAG('T'))) {
		xfer_cmds++;
		scanalldirs(FL_ULTIME);
	}
}

void sbbs_t::new_scan_ptr_cfg()
{
	uint	i,j;
	long	s;
	uint32_t	l;
	time_t	t;
	ulong	total_subs;
	ulong	subs;

	while(online) {
		bputs(text[CfgGrpLstHdr]);
		for(i=0;i<usrgrps && !msgabort();i++) {
			checkline();
			if(i<9) outchar(' ');
			if(i<99) outchar(' ');
			bprintf(text[CfgGrpLstFmt],i+1,cfg.grp[usrgrp[i]]->lname);
		}
		SYNC;
		mnemonics(text[WhichOrAll]);
		s=getkeys("AQ",usrgrps);
		if(!s || s==-1 || s=='Q')
			break;
		if(s=='A') {
			mnemonics("\r\nEnter number of messages from end, ~Date, ~Quit, or"
				" [Last Message]: ");
			s=getkeys("DLQ",9999);
			if(s==-1 || s=='Q')
				continue;
			if(s=='D') {
				t=time(NULL);
				for(i=0, total_subs=0; i<usrgrps; i++)
					total_subs += usrsubs[i];
				if(inputnstime(&t) && !(sys_status&SS_ABORT)) {
					for(i=0, subs=0; i<usrgrps && online; i++) {
						for(j=0;j<usrsubs[i] && online;j++) {
							progress(text[LoadingMsgPtrs], subs++, total_subs, 10);
							checkline();
							subscan[usrsub[i][j]].ptr=getmsgnum(usrsub[i][j],t);
						}
					}
					progress(text[LoadingMsgPtrs], subs, total_subs);
				}
				continue;
			}
			if(s=='L')
				s=0;
			if(s)
				s&=~0x80000000L;
			for(i=0, total_subs=0; i<usrgrps; i++)
				total_subs += usrsubs[i];
			for(i=0, subs=0; i<usrgrps; i++)
				for(j=0;j<usrsubs[i] && online;j++) {
					progress(text[LoadingMsgPtrs], subs++, total_subs, 10);
					checkline();
					if(s == 0) {
						subscan[usrsub[i][j]].ptr = ~0;
						continue;
					}
					getlastmsg(usrsub[i][j],&l,0);
					if(s>(long)l)
						subscan[usrsub[i][j]].ptr=0;
					else
						subscan[usrsub[i][j]].ptr=l-s;
				}
			progress(text[LoadingMsgPtrs], subs, total_subs);
			continue;
		}
		i=(s&~0x80000000L)-1;
		while(online) {
			l=0;
			bprintf(text[CfgSubLstHdr],cfg.grp[usrgrp[i]]->lname);
			for(j=0;j<usrsubs[i] && !msgabort();j++) {
				checkline();
				if(j<9) outchar(' ');
				if(j<99) outchar(' ');
				t=getmsgtime(usrsub[i][j],subscan[usrsub[i][j]].ptr);
				if(t>(long)l)
					l=(uint32_t)t;
				bprintf(text[SubPtrLstFmt],j+1,cfg.sub[usrsub[i][j]]->lname
					,timestr(t),nulstr);
			}
			SYNC;
			mnemonics(text[WhichOrAll]);
			s=getkeys("AQ",usrsubs[i]);
			if(sys_status&SS_ABORT) {
				lncntr=0;
				return;
			}
			if(s==-1 || !s || s=='Q')
				break;
			if(s=='A') {    /* The entire group */
				mnemonics("\r\nEnter number of messages from end, ~Date, ~Quit, or"
					" [Last Message]: ");
				s=getkeys("DLQ",9999);
				if(s==-1 || s=='Q')
					continue;
				if(s=='D') {
					t=l;
					if(inputnstime(&t) && !(sys_status&SS_ABORT)) {
						for(j=0;j<usrsubs[i] && online;j++) {
							progress(text[LoadingMsgPtrs], j, usrsubs[i], 10);
							checkline();
							subscan[usrsub[i][j]].ptr=getmsgnum(usrsub[i][j],t);
						}
						progress(text[LoadingMsgPtrs], j, usrsubs[i]);
					}
					continue;
				}
				if(s=='L')
					s=0;
				if(s)
					s&=~0x80000000L;
				for(j=0;j<usrsubs[i] && online;j++) {
					progress(text[LoadingMsgPtrs], j, usrsubs[i], 10);
					checkline();
					if(s == 0) {
						subscan[usrsub[i][j]].ptr = ~0;
						continue;
					}
					getlastmsg(usrsub[i][j],&l,0);
					if(s>(long)l)
						subscan[usrsub[i][j]].ptr=0;
					else
						subscan[usrsub[i][j]].ptr=l-s;
				}
				progress(text[LoadingMsgPtrs], j, usrsubs[i]);
				continue;
			}
			else {
				j=(s&~0x80000000L)-1;
				mnemonics("\r\nEnter number of messages from end, ~Date, ~Quit, or"
					" [Last Message]: ");
				s=getkeys("DLQ",9999);
				if(s==-1 || s=='Q')
					continue;
				if(s=='D') {
					t=getmsgtime(usrsub[i][j],subscan[usrsub[i][j]].ptr);
					if(inputnstime(&t) && !(sys_status&SS_ABORT)) {
						bputs(text[LoadingMsgPtrs]);
						subscan[usrsub[i][j]].ptr=getmsgnum(usrsub[i][j],t);
					}
					continue;
				}
				if(s=='L') {
					subscan[usrsub[i][j]].ptr = ~0;
					continue;
				}
				if(s)
					s&=~0x80000000L;
				getlastmsg(usrsub[i][j],&l,0);
				if(s>(long)l)
					subscan[usrsub[i][j]].ptr=0;
				else
					subscan[usrsub[i][j]].ptr=l-s;
			}
		}
	}
}

void sbbs_t::new_scan_cfg(ulong misc)
{
	long	s;
	ulong	i,j;
	ulong	t;

	while(online) {
		bputs(text[CfgGrpLstHdr]);
		for(i=0;i<usrgrps && !msgabort();i++) {
			checkline();
			if(i<9) outchar(' ');
			if(i<99) outchar(' ');
			bprintf(text[CfgGrpLstFmt],i+1,cfg.grp[usrgrp[i]]->lname);
		}
		SYNC;
		if(misc&SUB_CFG_NSCAN)
			mnemonics(text[NScanCfgWhichGrp]);
		else
			mnemonics(text[SScanCfgWhichGrp]);
		s=getnum(i);
		if(s<1)
			break;
		i=s-1;
		while(online) {
			if(misc&SUB_CFG_NSCAN)
				misc&=~SUB_CFG_YSCAN;
			bprintf(text[CfgSubLstHdr],cfg.grp[usrgrp[i]]->lname);
			for(j=0;j<usrsubs[i] && !msgabort();j++) {
				checkline();
				if(j<9) outchar(' ');
				if(j<99) outchar(' ');
				bprintf(text[CfgSubLstFmt],j+1
					,cfg.sub[usrsub[i][j]]->lname
					,subscan[usrsub[i][j]].cfg&misc ?
						(misc&SUB_CFG_NSCAN && subscan[usrsub[i][j]].cfg&SUB_CFG_YSCAN) ?
						"To You Only" : text[On] : text[Off]);
					}
			SYNC;
			if(misc&SUB_CFG_NSCAN)
				mnemonics(text[NScanCfgWhichSub]);
			else
				mnemonics(text[SScanCfgWhichSub]);
			s=getkeys("AQ",usrsubs[i]);
			if(sys_status&SS_ABORT) {
				lncntr=0;
				return;
			}
			if(!s || s==-1 || s=='Q')
				break;
			if(s=='A') {
				t=subscan[usrsub[i][0]].cfg&misc;
				if(misc&SUB_CFG_NSCAN && !t && !(useron.misc&FLAG('Q')))
					if(!noyes("Messages to you only"))
						misc|=SUB_CFG_YSCAN;
				for(j=0;j<usrsubs[i] && online;j++) {
					checkline();
					if(t) subscan[usrsub[i][j]].cfg&=~misc;
					else  {
						if(misc&SUB_CFG_NSCAN)
							subscan[usrsub[i][j]].cfg&=~SUB_CFG_YSCAN;
						subscan[usrsub[i][j]].cfg|=misc;
					}
				}
				continue;
			}
			j=(s&~0x80000000L)-1;
			if(misc&SUB_CFG_NSCAN && !(subscan[usrsub[i][j]].cfg&misc)) {
				if(!(useron.rest&FLAG('Q')) && !noyes("Messages to you only"))
					subscan[usrsub[i][j]].cfg|=SUB_CFG_YSCAN;
				else
					subscan[usrsub[i][j]].cfg&=~SUB_CFG_YSCAN;
			}
			subscan[usrsub[i][j]].cfg^=misc;
		}
	}
}

