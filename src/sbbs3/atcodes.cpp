/* atcodes.cpp */

/* Synchronet "@code" functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "cmdshell.h"

/****************************************************************************/
/* Returns 0 if invalid @ code. Returns length of @ code if valid.          */
/****************************************************************************/
int sbbs_t::atcodes(char *instr)
{
	char	str[64],str2[64],*p,*tp,*sp;
    int     i,len;
	long	l;
    stats_t stats;
    node_t  node;
	struct	tm tm;
	struct	tm * tm_p;

	sprintf(str,"%.40s",instr);
	tp=strchr(str+1,'@');
	if(!tp)                 /* no terminating @ */
		return(0);
	sp=strchr(str+1,SP);
	if(sp && sp<tp)         /* space before terminating @ */
		return(0);
	len=(tp-str)+1;
	(*tp)=0;
	sp=(str+1);

	if(!strcmp(sp,"VER"))
		bputs(VERSION);

	else if(!strcmp(sp,"REV"))
		bprintf("%c",REVISION);

	else if(!strcmp(sp,"BBS") || !strcmp(sp,"BOARDNAME"))
		bputs(cfg.sys_name);

	else if(!strcmp(sp,"BAUD") || !strcmp(sp,"BPS"))
		bprintf("%lu",cur_rate);

	else if(!strcmp(sp,"CONN"))
		bputs(connection);

	else if(!strcmp(sp,"SYSOP"))
		bputs(cfg.sys_op);

	else if(!strcmp(sp,"LOCATION"))
		bputs(cfg.sys_location);

	else if(!strcmp(sp,"NODE"))
		bprintf("%u",cfg.node_num);

	else if(!strcmp(sp,"TNODE"))
		bprintf("%u",cfg.sys_nodes);

	else if(!strcmp(sp,"INETADDR"))
		bputs(cfg.sys_inetaddr);

	else if(!strcmp(sp,"FIDOADDR")) {
		if(cfg.total_faddrs)
			bputs(faddrtoa(cfg.faddr[0]));
	}

	else if(!strcmp(sp,"QWKID"))
		bputs(cfg.sys_id);

	else if(!strcmp(sp,"TIME") || !strcmp(sp,"SYSTIME")) {
		now=time(NULL);
		tm_p=gmtime(&now);
		if(tm_p!=NULL)
			tm=*tm_p;
		else
			memset(&tm,0,sizeof(tm));
		bprintf("%02d:%02d %s"
			,tm.tm_hour==0 ? 12
			: tm.tm_hour>12 ? tm.tm_hour-12
			: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am"); 
	}

	else if(!strcmp(sp,"DATE") || !strcmp(sp,"SYSDATE")) {
		now=time(NULL);
		bputs(unixtodstr(&cfg,now,str2)); }

	else if(!strcmp(sp,"TMSG")) {
		l=0;
		for(i=0;i<cfg.total_subs;i++)
			l+=getposts(&cfg,i); 		/* l=total posts */
		bprintf("%lu",l); }

	else if(!strcmp(sp,"TUSER"))
		bprintf("%u",lastuser(&cfg));

	else if(!strcmp(sp,"TFILE")) {
		l=0;
		for(i=0;i<cfg.total_dirs;i++)
			l+=getfiles(&cfg,i);
		bprintf("%lu",l); }

	else if(!strcmp(sp,"TCALLS") || !strcmp(sp,"NUMCALLS")) {
		getstats(&cfg,0,&stats);
		bprintf("%lu",stats.logons); }

	else if(!strcmp(sp,"PREVON") || !strcmp(sp,"LASTCALLERNODE")
		|| !strcmp(sp,"LASTCALLERSYSTEM"))
		bputs(lastuseron);

	else if(!strcmp(sp,"CLS"))
		CLS;

	else if(!strcmp(sp,"PAUSE") || !strcmp(sp,"MORE"))
		pause();

	else if(!strcmp(sp,"NOPAUSE") || !strcmp(sp,"POFF"))
		sys_status^=SS_PAUSEOFF;

	else if(!strcmp(sp,"PON") || !strcmp(sp,"AUTOMORE"))
		sys_status^=SS_PAUSEON;

	/* NOSTOP */

	/* STOP */

	else if(!strcmp(sp,"BELL") || !strcmp(sp,"BEEP"))
		outchar(BEL);

	// else if(!strcmp(sp,"EVENT"))
	//	  bputs(sectostr(sys_eventtime,str2));

	/* LASTCALL */

	else if(!strncmp(sp,"NODE",4)) {
		i=atoi(sp+4);
		if(i && i<=cfg.sys_nodes) {
			getnodedat(i,&node,0);
			printnodedat(i,&node); } }

	else if(!strcmp(sp,"WHO"))
		whos_online(true);

	/* User Codes */

	else if(!strcmp(sp,"USER") || !strcmp(sp,"ALIAS") || !strcmp(sp,"NAME"))
		bputs(useron.alias);

	else if(!strcmp(sp,"FIRST")) {
		strcpy(str2,useron.alias);
		tp=strchr(str2,SP);
		if(tp) *tp=0;
		bputs(str2); }

	else if(!strcmp(sp,"USERNUM")) 
		bprintf("%u",useron.number);

	else if(!strcmp(sp,"PHONE") || !strcmp(sp,"HOMEPHONE")
		|| !strcmp(sp,"DATAPHONE") || !strcmp(sp,"DATA"))
		bputs(useron.phone);

	else if(!strcmp(sp,"ADDR1"))
		bputs(useron.address);

	else if(!strcmp(sp,"FROM"))
		bputs(useron.location);

	else if(!strcmp(sp,"CITY")) {
		strcpy(str2,useron.location);
		p=strchr(str2,',');
		if(p) {
			*p=0;
			bputs(str2); } }

	else if(!strcmp(sp,"STATE")) {
		p=strchr(useron.location,',');
		if(p) {
			p++;
			if(*p==SP)
				p++;
			bputs(p); } }

	else if(!strcmp(sp,"CPU"))
		bputs(useron.comp);
		
	else if(!strcmp(sp,"HOST"))
		bputs(client_name);

	else if(!strcmp(sp,"BDATE"))
		bputs(useron.birth);

	else if(!strcmp(sp,"CALLS") || !strcmp(sp,"NUMTIMESON"))
		bprintf("%u",useron.logons);

	else if(!strcmp(sp,"MEMO"))
		bputs(unixtodstr(&cfg,useron.pwmod,str2));

	else if(!strcmp(sp,"SEC") || !strcmp(sp,"SECURITY"))
		bprintf("%u",useron.level);

	else if(!strcmp(sp,"SINCE"))
		bputs(unixtodstr(&cfg,useron.firston,str2));

	else if(!strcmp(sp,"TIMEON") || !strcmp(sp,"TIMEUSED")) {
		now=time(NULL);
		bprintf("%u",(now-logontime)/60); }

	else if(!strcmp(sp,"TUSED")) {              /* Synchronet only */
		now=time(NULL);
		bputs(sectostr(now-logontime,str2)+1); }

	else if(!strcmp(sp,"TLEFT")) {              /* Synchronet only */
		gettimeleft();
		bputs(sectostr(timeleft,str2)+1); }

	else if(!strcmp(sp,"TPERD"))                /* Synchronet only */
		bputs(sectostr(cfg.level_timeperday[useron.level],str)+1);

	else if(!strcmp(sp,"TPERC"))                /* Synchronet only */
		bputs(sectostr(cfg.level_timepercall[useron.level],str)+1);

	else if(!strcmp(sp,"TIMELIMIT"))
		bprintf("%u",cfg.level_timepercall[useron.level]);

	else if(!strcmp(sp,"MINLEFT") || !strcmp(sp,"LEFT") || !strcmp(sp,"TIMELEFT")) {
		gettimeleft();
		bprintf("%u",timeleft/60); }

	else if(!strcmp(sp,"LASTON"))
		bputs(timestr(&useron.laston));

	else if(!strcmp(sp,"LASTDATEON"))
		bputs(unixtodstr(&cfg,useron.laston,str2));

	else if(!strcmp(sp,"LASTTIMEON")) {
		tm_p=gmtime(&useron.laston);
		if(tm_p)
			tm=*tm_p;
		else
			memset(&tm,0,sizeof(tm));
		bprintf("%02d:%02d %s"
			,tm.tm_hour==0 ? 12
			: tm.tm_hour>12 ? tm.tm_hour-12
			: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am"); 
	}

	else if(!strcmp(sp,"MSGLEFT") || !strcmp(sp,"MSGSLEFT"))
		bprintf("%u",useron.posts);

	else if(!strcmp(sp,"MSGREAD"))
		bprintf("%u",posts_read);

	else if(!strcmp(sp,"FREESPACE")) 
		bprintf("%lu",getfreediskspace(cfg.temp_dir)); 

	else if(!strcmp(sp,"UPBYTES"))
		bprintf("%lu",useron.ulb);

	else if(!strcmp(sp,"UPK"))
		bprintf("%lu",useron.ulb/1024L);

	else if(!strcmp(sp,"UPS") || !strcmp(sp,"UPFILES"))
		bprintf("%u",useron.uls);

	else if(!strcmp(sp,"DLBYTES"))
		bprintf("%lu",useron.dlb);

	else if(!strcmp(sp,"DOWNK"))
		bprintf("%lu",useron.dlb/1024L);

	else if(!strcmp(sp,"DOWNS") || !strcmp(sp,"DLFILES"))
		bprintf("%u",useron.dls);

	else if(!strcmp(sp,"LASTNEW"))
		bputs(unixtodstr(&cfg,ns_time,str2));

	else if(!strcmp(sp,"NEWFILETIME"))
		bputs(timestr(&ns_time));

	/* MAXDL */

	else if(!strcmp(sp,"MAXDK") || !strcmp(sp,"DLKLIMIT") || !strcmp(sp,"KBLIMIT"))
		bprintf("%lu",cfg.level_freecdtperday[useron.level]/1024L);

	else if(!strcmp(sp,"DAYBYTES"))     /* amt of free cdts used today */
		bprintf("%lu",cfg.level_freecdtperday[useron.level]-useron.freecdt);

	else if(!strcmp(sp,"BYTELIMIT"))
		bprintf("%lu",cfg.level_freecdtperday[useron.level]);

	else if(!strcmp(sp,"KBLEFT"))
		bprintf("%lu",(useron.cdt+useron.freecdt)/1024L);

	else if(!strcmp(sp,"BYTESLEFT"))
		bprintf("%lu",useron.cdt+useron.freecdt);

	else if(!strcmp(sp,"CONF"))
		bprintf("%s %s"
			,usrgrps ? cfg.grp[usrgrp[curgrp]]->sname :nulstr
			,usrgrps ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);

	else if(!strcmp(sp,"CONFNUM"))
		bprintf("%u %u",curgrp+1,cursub[curgrp]+1);

	else if(!strcmp(sp,"NUMDIR"))
		bprintf("%u %u",usrlibs ? curlib+1 : 0,usrlibs ? curdir[curlib]+1 : 0);

	else if(!strcmp(sp,"EXDATE") || !strcmp(sp,"EXPDATE"))
		bputs(unixtodstr(&cfg,useron.expire,str2));

	else if(!strcmp(sp,"EXPDAYS")) {
		now=time(NULL);
		l=useron.expire-now;
		if(l<0)
			l=0;
		bprintf("%u",l/(1440L*60L)); }

	else if(!strcmp(sp,"MEMO1"))
		bputs(useron.note);

	else if(!strcmp(sp,"MEMO2") || !strcmp(sp,"COMPANY"))
		bputs(useron.name);

	else if(!strcmp(sp,"ZIP"))
		bputs(useron.zipcode);

	else if(!strcmp(sp,"HANGUP"))
		hangup();

	else
		return(syncatcodes(sp, len));

	return(len);
}

int sbbs_t::syncatcodes(char *sp, int len)
{
	char str2[128],*tp;
    stats_t stats;

	/* Synchronet Specific */

	if(!strncmp(sp,"SETSTR:",7))
		strcpy(main_csi.str,sp+7);

	else if(!strncmp(sp,"EXEC:",5))
		exec_bin(sp+5,&main_csi);

	else if(!strncmp(sp,"MENU:",5))
		menu(sp+5);

	else if(!strncmp(sp,"TYPE:",5))
		printfile(cmdstr(sp+5,nulstr,nulstr,str2),0);

	else if(!strcmp(sp,"QUESTION"))
		bputs(question);

	else if(!strncmp(sp,"NAME-L",6))
		bprintf("%-*.*s",len,len,useron.alias);

	else if(!strncmp(sp,"NAME-R",6))
		bprintf("%*.*s",len,len,useron.alias);

	else if(!strcmp(sp,"HANDLE"))
		bputs(useron.handle);

	else if(!strcmp(sp,"CID") || !strcmp(sp,"IP"))
		bputs(cid);

	else if(!strcmp(sp,"LOCAL-IP")) {
		struct in_addr in_addr;
		in_addr.s_addr=local_addr;
		bprintf("%s",inet_ntoa(in_addr));
	}

	else if(!strcmp(sp,"CRLF")) {
		CRLF;
	}

	else if(!strcmp(sp,"PUSHXY")) 
		ANSI_SAVE();

	else if(!strcmp(sp,"POPXY")) 
		ANSI_RESTORE();

	else if(!strcmp(sp,"UP")) 
		rputs("\x1b[A");

	else if(!strcmp(sp,"DOWN")) 
		rputs("\x1b[B");

	else if(!strcmp(sp,"RIGHT")) 
		rputs("\x1b[C");

	else if(!strcmp(sp,"LEFT")) 
		rputs("\x1b[D");

	else if(!strncmp(sp,"UP:",3))
		rprintf("\x1b[%dA",atoi(sp+3));

	else if(!strncmp(sp,"DOWN:",5))
		rprintf("\x1b[%dB",atoi(sp+5));

	else if(!strncmp(sp,"LEFT:",5))
		rprintf("\x1b[%dC",atoi(sp+5));

	else if(!strncmp(sp,"RIGHT:",6))
		rprintf("\x1b[%dD",atoi(sp+6));

	else if(!strncmp(sp,"GOTOXY:",7)) {
		tp=strchr(sp,',');
		if(tp!=NULL) {
			tp++;
			GOTOXY(atoi(sp+7),atoi(tp));
		}
	}

	else if(!strcmp(sp,"GRP"))
		bputs(usrgrps ? cfg.grp[usrgrp[curgrp]]->sname : nulstr);

	else if(!strncmp(sp,"GRP-L",5))
		bprintf("%-*.*s",len,len,usrgrps ? cfg.grp[usrgrp[curgrp]]->sname : nulstr);

	else if(!strncmp(sp,"GRP-R",5))
		bprintf("%*.*s",len,len,usrgrps ? cfg.grp[usrgrp[curgrp]]->sname : nulstr);

	else if(!strcmp(sp,"GRPL"))
		bputs(usrgrps ? cfg.grp[usrgrp[curgrp]]->lname : nulstr);

	else if(!strncmp(sp,"GRPL-L",6))
		bprintf("%-*.*s",len,len,usrgrps ? cfg.grp[usrgrp[curgrp]]->lname : nulstr);

	else if(!strncmp(sp,"GRPL-R",6))
		bprintf("%*.*s",len,len,usrgrps ? cfg.grp[usrgrp[curgrp]]->lname : nulstr);

	else if(!strcmp(sp,"GN"))
		bprintf("%u",usrgrps ? curgrp+1 : 0);

	else if(!strcmp(sp,"GL"))
		bprintf("%-4u",usrgrps ? curgrp+1 : 0);

	else if(!strcmp(sp,"GR"))
		bprintf("%4u",usrgrps ? curgrp+1 : 0);

	else if(!strcmp(sp,"SUB"))
		bputs(usrgrps ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);

	else if(!strncmp(sp,"SUB-L",5))
		bprintf("%-*.*s",len,len,usrgrps
			? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);

	else if(!strncmp(sp,"SUB-R",5))
		bprintf("%*.*s",len,len,usrgrps
			? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);

	else if(!strcmp(sp,"SUBL"))
		bputs(usrgrps  ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->lname : nulstr);

	else if(!strncmp(sp,"SUBL-L",6))
		bprintf("%-*.*s",len,len,usrgrps
			? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->lname : nulstr);

	else if(!strncmp(sp,"SUBL-R",6))
		bprintf("%*.*s",len,len,usrgrps
			? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->lname : nulstr);

	else if(!strcmp(sp,"SN"))
		bprintf("%u",usrgrps ? cursub[curgrp]+1 : 0);

	else if(!strcmp(sp,"SL"))
		bprintf("%-4u",usrgrps ? cursub[curgrp]+1 : 0);

	else if(!strcmp(sp,"SR"))
		bprintf("%4u",usrgrps ? cursub[curgrp]+1 : 0);

	else if(!strcmp(sp,"LIB"))
		bputs(usrlibs ? cfg.lib[usrlib[curlib]]->sname : nulstr);

	else if(!strncmp(sp,"LIB-L",5))
		bprintf("%-*.*s",len,len,usrlibs ? cfg.lib[usrlib[curlib]]->sname : nulstr);

	else if(!strncmp(sp,"LIB-R",5))
		bprintf("%*.*s",len,len,usrlibs ? cfg.lib[usrlib[curlib]]->sname : nulstr);

	else if(!strcmp(sp,"LIBL"))
		bputs(usrlibs ? cfg.lib[usrlib[curlib]]->lname : nulstr);

	else if(!strncmp(sp,"LIBL-L",6))
		bprintf("%-*.*s",len,len,usrlibs ? cfg.lib[usrlib[curlib]]->lname : nulstr);

	else if(!strncmp(sp,"LIBL-R",6))
		bprintf("%*.*s",len,len,usrlibs ? cfg.lib[usrlib[curlib]]->lname : nulstr);

	else if(!strcmp(sp,"LN"))
		bprintf("%u",usrlibs ? curlib+1 : 0);

	else if(!strcmp(sp,"LL"))
		bprintf("%-4u",usrlibs ? curlib+1 : 0);

	else if(!strcmp(sp,"LR"))
		bprintf("%4u",usrlibs  ? curlib+1 : 0);

	else if(!strcmp(sp,"DIR"))
		bputs(usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->sname :nulstr);

	else if(!strncmp(sp,"DIR-L",5))
		bprintf("%-*.*s",len,len,usrlibs
			? cfg.dir[usrdir[curlib][curdir[curlib]]]->sname : nulstr);

	else if(!strncmp(sp,"DIR-R",5))
		bprintf("%*.*s",len,len,usrlibs
			? cfg.dir[usrdir[curlib][curdir[curlib]]]->sname : nulstr);

	else if(!strcmp(sp,"DIRL"))
		bputs(usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr);

	else if(!strncmp(sp,"DIRL-L",6))
		bprintf("%-*.*s",len,len,usrlibs
			? cfg.dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr);

	else if(!strncmp(sp,"DIRL-R",6))
		bprintf("%*.*s",len,len,usrlibs
			? cfg.dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr);

	else if(!strcmp(sp,"DN"))
		bprintf("%u",usrlibs ? curdir[curlib]+1 : 0);

	else if(!strcmp(sp,"DL"))
		bprintf("%-4u",usrlibs ? curdir[curlib]+1 : 0);

	else if(!strcmp(sp,"DR"))
		bprintf("%4u",usrlibs ? curdir[curlib]+1 : 0);

	else if(!strcmp(sp,"NOACCESS")) {
		if(noaccess_str==text[NoAccessTime])
			bprintf(noaccess_str,noaccess_val/60,noaccess_val%60);
		else if(noaccess_str==text[NoAccessDay])
			bprintf(noaccess_str,wday[noaccess_val]);
		else
			bprintf(noaccess_str,noaccess_val); }

	else if(!strcmp(sp,"LAST")) {
		tp=strrchr(useron.alias,SP);
		if(tp) tp++;
		else tp=useron.alias;
		bputs(tp); }

	else if(!strcmp(sp,"REAL")) {
		strcpy(str2,useron.name);
		tp=strchr(str2,SP);
		if(tp) *tp=0;
		bputs(str2); }

	else if(!strcmp(sp,"FIRSTREAL")) {
		strcpy(str2,useron.name);
		tp=strchr(str2,SP);
		if(tp) *tp=0;
		bputs(str2); }

	else if(!strcmp(sp,"LASTREAL")) {
		tp=strrchr(useron.name,SP);
		if(tp) tp++;
		else tp=useron.name;
		bputs(tp); }

	else if(!strcmp(sp,"MAILW"))
		bprintf("%u",getmail(&cfg,useron.number,0));

	else if(!strcmp(sp,"MAILP"))
		bprintf("%u",getmail(&cfg,useron.number,1));

	else if(!strncmp(sp,"MAILW:",6))
		bprintf("%u",getmail(&cfg,atoi(sp+6),0));

	else if(!strncmp(sp,"MAILP:",6))
		bprintf("%u",getmail(&cfg,atoi(sp+6),1));

	else if(!strcmp(sp,"MSGREPLY"))
		bprintf("%c",cfg.sys_misc&SM_RA_EMU ? 'R' : 'A');

	else if(!strcmp(sp,"MSGREREAD"))
		bprintf("%c",cfg.sys_misc&SM_RA_EMU ? 'A' : 'R');

	else if(!strncmp(sp,"STATS.",6)) {
		getstats(&cfg,0,&stats);
		sp+=6;
		if(!strcmp(sp,"LOGONS")) 
			bprintf("%u",stats.logons);
		else if(!strcmp(sp,"LTODAY")) 
			bprintf("%u",stats.ltoday);
		else if(!strcmp(sp,"TIMEON")) 
			bprintf("%u",stats.timeon);
		else if(!strcmp(sp,"TTODAY")) 
			bprintf("%u",stats.ttoday);
		else if(!strcmp(sp,"ULS")) 
			bprintf("%u",stats.uls);
		else if(!strcmp(sp,"ULB")) 
			bprintf("%u",stats.ulb);
		else if(!strcmp(sp,"DLS")) 
			bprintf("%u",stats.dls);
		else if(!strcmp(sp,"DLB")) 
			bprintf("%u",stats.dlb);
		else if(!strcmp(sp,"PTODAY")) 
			bprintf("%u",stats.ptoday);
		else if(!strcmp(sp,"ETODAY")) 
			bprintf("%u",stats.etoday);
		else if(!strcmp(sp,"FTODAY")) 
			bprintf("%u",stats.ftoday);
		else if(!strcmp(sp,"NUSERS")) 
			bprintf("%u",stats.nusers);
	}

	else return(0);

	return(len);
}

