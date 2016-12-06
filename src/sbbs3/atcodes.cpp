/* Synchronet "@code" functions */

/* $Id$ */

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
#include "cmdshell.h"

#if defined(_WINSOCKAPI_)
	extern WSADATA WSAData;
	#define SOCKLIB_DESC WSAData.szDescription
#else
	#define	SOCKLIB_DESC NULL
#endif

/****************************************************************************/
/* Returns 0 if invalid @ code. Returns length of @ code if valid.          */
/****************************************************************************/
int sbbs_t::show_atcode(const char *instr)
{
	char	str[128],str2[128],*tp,*sp,*p;
    int     len;
	int		disp_len;
	bool	padded_left=false;
	bool	padded_right=false;
	bool	centered=false;
	const char *cp;

	SAFECOPY(str,instr);
	tp=strchr(str+1,'@');
	if(!tp)                 /* no terminating @ */
		return(0);
	sp=strchr(str+1,' ');
	if(sp && sp<tp)         /* space before terminating @ */
		return(0);
	len=(tp-str)+1;
	(*tp)=0;
	sp=(str+1);

	disp_len=len;
	if((p=strstr(sp,"-L"))!=NULL)
		padded_left=true;
	else if((p=strstr(sp,"-R"))!=NULL)
		padded_right=true;
	else if((p=strstr(sp,"-C"))!=NULL)
		centered=true;
	if(p!=NULL) {
		if(*(p+2) && isdigit(*(p+2)))
			disp_len=atoi(p+2);
		*p=0;
	}

	cp=atcode(sp,str2,sizeof(str2));
	if(cp==NULL)
		return(0);

	if(padded_left)
		bprintf("%-*.*s",disp_len,disp_len,cp);
	else if(padded_right)
		bprintf("%*.*s",disp_len,disp_len,cp);
	else if(centered) {
		int vlen = strlen(cp);
		if(vlen < disp_len) {
			int left = (disp_len - vlen) / 2;
			bprintf("%*s%-*s", left, "", disp_len - left, cp);
		} else
			bputs(cp);
	} else
		bputs(cp);

	return(len);
}

const char* sbbs_t::atcode(char* sp, char* str, size_t maxlen)
{
	char*	tp;
	uint	i;
	uint	ugrp;
	uint	usub;
	long	l;
    stats_t stats;
    node_t  node;
	struct	tm tm;

	str[0]=0;

	if(!strcmp(sp,"VER"))
		return(VERSION);

	if(!strcmp(sp,"REV")) {
		safe_snprintf(str,maxlen,"%c",REVISION);
		return(str);
	}

	if(!strcmp(sp,"FULL_VER")) {
		safe_snprintf(str,maxlen,"%s%c%s",VERSION,REVISION,beta_version);
		truncsp(str);
#if defined(_DEBUG)
		strcat(str," Debug");
#endif
		return(str);
	}

	if(!strcmp(sp,"VER_NOTICE"))
		return(VERSION_NOTICE);

	if(!strcmp(sp,"OS_VER"))
		return(os_version(str));

#ifdef JAVASCRIPT
	if(!strcmp(sp,"JS_VER"))
		return((char *)JS_GetImplementationVersion());
#endif

	if(!strcmp(sp,"PLATFORM"))
		return(PLATFORM_DESC);

	if(!strcmp(sp,"COPYRIGHT"))
		return(COPYRIGHT_NOTICE);

	if(!strcmp(sp,"COMPILER")) {
		DESCRIBE_COMPILER(str);
		return(str);
	}

	if(!strcmp(sp,"UPTIME")) {
		extern volatile time_t uptime;
		time_t up=0;
		now = time(NULL);
		if (uptime != 0 && now >= uptime)
			up = now-uptime;
		char   days[64]="";
		if((up/(24*60*60))>=2) {
	        sprintf(days,"%lu days ",(ulong)(up/(24L*60L*60L)));
			up%=(24*60*60);
		}
		safe_snprintf(str,maxlen,"%s%lu:%02lu"
	        ,days
			,(ulong)(up/(60L*60L))
			,(ulong)((up/60L)%60L)
			);
		return(str);
	}

	if(!strcmp(sp,"SERVED")) {
		extern volatile ulong served;
		safe_snprintf(str,maxlen,"%lu",served);
		return(str);
	}

	if(!strcmp(sp,"SOCKET_LIB"))
		return(socklib_version(str,SOCKLIB_DESC));

	if(!strcmp(sp,"MSG_LIB")) {
		safe_snprintf(str,maxlen,"SMBLIB %s",smb_lib_ver());
		return(str);
	}

	if(!strcmp(sp,"BBS") || !strcmp(sp,"BOARDNAME"))
		return(cfg.sys_name);

	if(!strcmp(sp,"BAUD") || !strcmp(sp,"BPS")) {
		safe_snprintf(str,maxlen,"%lu",cur_rate);
		return(str);
	}

	if(!strcmp(sp,"CONN"))
		return(connection);

	if(!strcmp(sp,"SYSOP"))
		return(cfg.sys_op);

	if(!strcmp(sp,"LOCATION"))
		return(cfg.sys_location);

	if(!strcmp(sp,"NODE")) {
		safe_snprintf(str,maxlen,"%u",cfg.node_num);
		return(str);
	}

	if(!strcmp(sp,"TNODE")) {
		safe_snprintf(str,maxlen,"%u",cfg.sys_nodes);
		return(str);
	}

	if(!strcmp(sp,"INETADDR"))
		return(cfg.sys_inetaddr);

	if(!strcmp(sp,"HOSTNAME"))
		return(startup->host_name);

	if(!strcmp(sp,"FIDOADDR")) {
		if(cfg.total_faddrs)
			return(smb_faddrtoa(&cfg.faddr[0],str));
		return(nulstr);
	}

	if(!strcmp(sp,"EMAILADDR"))
		return(usermailaddr(&cfg, str
			,cfg.inetmail_misc&NMAIL_ALIAS ? useron.alias : useron.name));

	if(!strcmp(sp,"QWKID"))
		return(cfg.sys_id);

	if(!strcmp(sp,"TIME") || !strcmp(sp,"SYSTIME")) {
		now=time(NULL);
		memset(&tm,0,sizeof(tm));
		localtime_r(&now,&tm);
		if(cfg.sys_misc&SM_MILITARY)
			safe_snprintf(str,maxlen,"%02d:%02d:%02d"
		        	,tm.tm_hour,tm.tm_min,tm.tm_sec);
		else
			safe_snprintf(str,maxlen,"%02d:%02d %s"
				,tm.tm_hour==0 ? 12
				: tm.tm_hour>12 ? tm.tm_hour-12
				: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am");
		return(str);
	}

	if(!strcmp(sp,"TIMEZONE"))
		return(smb_zonestr(sys_timezone(&cfg),str));

	if(!strcmp(sp,"DATE") || !strcmp(sp,"SYSDATE")) {
		return(unixtodstr(&cfg,time32(NULL),str));
	}

	if(!strcmp(sp,"DATETIME"))
		return(timestr(time(NULL)));

	if(!strcmp(sp,"TMSG")) {
		l=0;
		for(i=0;i<cfg.total_subs;i++)
			l+=getposts(&cfg,i); 		/* l=total posts */
		safe_snprintf(str,maxlen,"%lu",l);
		return(str);
	}

	if(!strcmp(sp,"TUSER")) {
		safe_snprintf(str,maxlen,"%u",total_users(&cfg));
		return(str);
	}

	if(!strcmp(sp,"TFILE")) {
		l=0;
		for(i=0;i<cfg.total_dirs;i++)
			l+=getfiles(&cfg,i);
		safe_snprintf(str,maxlen,"%lu",l);
		return(str);
	}

	if(!strcmp(sp,"TCALLS") || !strcmp(sp,"NUMCALLS")) {
		getstats(&cfg,0,&stats);
		safe_snprintf(str,maxlen,"%lu",stats.logons);
		return(str);
	}

	if(!strcmp(sp,"PREVON") || !strcmp(sp,"LASTCALLERNODE")
		|| !strcmp(sp,"LASTCALLERSYSTEM"))
		return(lastuseron);

	if(!strcmp(sp,"CLS")) {
		CLS;
		return(nulstr);
	}

	if(!strcmp(sp,"PAUSE") || !strcmp(sp,"MORE")) {
		pause();
		return(nulstr);
	}

	if(!strcmp(sp,"RESETPAUSE")) {
		lncntr=0;
		return(nulstr);
	}

	if(!strcmp(sp,"NOPAUSE") || !strcmp(sp,"POFF")) {
		sys_status^=SS_PAUSEOFF;
		return(nulstr);
	}

	if(!strcmp(sp,"PON") || !strcmp(sp,"AUTOMORE")) {
		sys_status^=SS_PAUSEON;
		return(nulstr);
	}

	/* NOSTOP */

	/* STOP */

	if(!strcmp(sp,"BELL") || !strcmp(sp,"BEEP"))
		return("\a");

	if(!strcmp(sp,"EVENT")) {
		if(event_time==0)
			return("<none>");
		return(timestr(event_time));
	}

	/* LASTCALL */

	if(!strncmp(sp,"NODE",4)) {
		i=atoi(sp+4);
		if(i && i<=cfg.sys_nodes) {
			getnodedat(i,&node,0);
			printnodedat(i,&node);
		}
		return(nulstr);
	}

	if(!strcmp(sp,"WHO")) {
		whos_online(true);
		return(nulstr);
	}

	/* User Codes */

	if(!strcmp(sp,"USER") || !strcmp(sp,"ALIAS") || !strcmp(sp,"NAME"))
		return(useron.alias);

	if(!strcmp(sp,"FIRST")) {
		safe_snprintf(str,maxlen,"%s",useron.alias);
		tp=strchr(str,' ');
		if(tp) *tp=0;
		return(str);
	}

	if(!strcmp(sp,"USERNUM")) {
		safe_snprintf(str,maxlen,"%u",useron.number);
		return(str);
	}

	if(!strcmp(sp,"PHONE") || !strcmp(sp,"HOMEPHONE")
		|| !strcmp(sp,"DATAPHONE") || !strcmp(sp,"DATA"))
		return(useron.phone);

	if(!strcmp(sp,"ADDR1"))
		return(useron.address);

	if(!strcmp(sp,"FROM"))
		return(useron.location);

	if(!strcmp(sp,"CITY")) {
		safe_snprintf(str,maxlen,"%s",useron.location);
		char* p=strchr(str,',');
		if(p) {
			*p=0;
			return(str);
		}
		return(nulstr);
	}

	if(!strcmp(sp,"STATE")) {
		char* p=strchr(useron.location,',');
		if(p) {
			p++;
			if(*p==' ')
				p++;
			return(p);
		}
		return(nulstr);
	}

	if(!strcmp(sp,"CPU"))
		return(useron.comp);

	if(!strcmp(sp,"HOST"))
		return(client_name);

	if(!strcmp(sp,"BDATE"))
		return(useron.birth);

	if(!strcmp(sp,"AGE")) {
		safe_snprintf(str,maxlen,"%u",getage(&cfg,useron.birth));
		return(str);
	}

	if(!strcmp(sp,"CALLS") || !strcmp(sp,"NUMTIMESON")) {
		safe_snprintf(str,maxlen,"%u",useron.logons);
		return(str);
	}

	if(!strcmp(sp,"MEMO"))
		return(unixtodstr(&cfg,useron.pwmod,str));

	if(!strcmp(sp,"SEC") || !strcmp(sp,"SECURITY")) {
		safe_snprintf(str,maxlen,"%u",useron.level);
		return(str);
	}

	if(!strcmp(sp,"SINCE"))
		return(unixtodstr(&cfg,useron.firston,str));

	if(!strcmp(sp,"TIMEON") || !strcmp(sp,"TIMEUSED")) {
		now=time(NULL);
		safe_snprintf(str,maxlen,"%lu",(ulong)(now-logontime)/60L);
		return(str);
	}

	if(!strcmp(sp,"TUSED")) {              /* Synchronet only */
		now=time(NULL);
		return(sectostr((uint)(now-logontime),str)+1);
	}

	if(!strcmp(sp,"TLEFT")) {              /* Synchronet only */
		gettimeleft();
		return(sectostr(timeleft,str)+1);
	}

	if(!strcmp(sp,"TPERD"))                /* Synchronet only */
		return(sectostr(cfg.level_timeperday[useron.level],str)+1);

	if(!strcmp(sp,"TPERC"))                /* Synchronet only */
		return(sectostr(cfg.level_timepercall[useron.level],str)+1);

	if(!strcmp(sp,"TIMELIMIT")) {
		safe_snprintf(str,maxlen,"%u",cfg.level_timepercall[useron.level]);
		return(str);
	}

	if(!strcmp(sp,"MINLEFT") || !strcmp(sp,"LEFT") || !strcmp(sp,"TIMELEFT")) {
		gettimeleft();
		safe_snprintf(str,maxlen,"%lu",timeleft/60);
		return(str);
	}

	if(!strcmp(sp,"LASTON"))
		return(timestr(useron.laston));

	if(!strcmp(sp,"LASTDATEON"))
		return(unixtodstr(&cfg,useron.laston,str));

	if(!strcmp(sp,"LASTTIMEON")) {
		memset(&tm,0,sizeof(tm));
		localtime32(&useron.laston,&tm);
		if(cfg.sys_misc&SM_MILITARY)
			safe_snprintf(str,maxlen,"%02d:%02d:%02d"
				,tm.tm_hour, tm.tm_min, tm.tm_sec);
		else
			safe_snprintf(str,maxlen,"%02d:%02d %s"
				,tm.tm_hour==0 ? 12
				: tm.tm_hour>12 ? tm.tm_hour-12
				: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am");
		return(str);
	}

	if(!strcmp(sp,"MSGLEFT") || !strcmp(sp,"MSGSLEFT")) {
		safe_snprintf(str,maxlen,"%u",useron.posts);
		return(str);
	}

	if(!strcmp(sp,"MSGREAD")) {
		safe_snprintf(str,maxlen,"%lu",posts_read);
		return(str);
	}

	if(!strcmp(sp,"FREESPACE")) {
		safe_snprintf(str,maxlen,"%lu",getfreediskspace(cfg.temp_dir,0));
		return(str);
	}

	if(!strcmp(sp,"FREESPACEK")) {
		safe_snprintf(str,maxlen,"%lu",getfreediskspace(cfg.temp_dir,1024));
		return(str);
	}

	if(!strcmp(sp,"UPBYTES")) {
		safe_snprintf(str,maxlen,"%lu",useron.ulb);
		return(str);
	}

	if(!strcmp(sp,"UPK")) {
		safe_snprintf(str,maxlen,"%lu",useron.ulb/1024L);
		return(str);
	}

	if(!strcmp(sp,"UPS") || !strcmp(sp,"UPFILES")) {
		safe_snprintf(str,maxlen,"%u",useron.uls);
		return(str);
	}

	if(!strcmp(sp,"DLBYTES")) {
		safe_snprintf(str,maxlen,"%lu",useron.dlb);
		return(str);
	}

	if(!strcmp(sp,"DOWNK")) {
		safe_snprintf(str,maxlen,"%lu",useron.dlb/1024L);
		return(str);
	}

	if(!strcmp(sp,"DOWNS") || !strcmp(sp,"DLFILES")) {
		safe_snprintf(str,maxlen,"%u",useron.dls);
		return(str);
	}

	if(!strcmp(sp,"LASTNEW"))
		return(unixtodstr(&cfg,(time32_t)ns_time,str));

	if(!strcmp(sp,"NEWFILETIME"))
		return(timestr(ns_time));

	/* MAXDL */

	if(!strcmp(sp,"MAXDK") || !strcmp(sp,"DLKLIMIT") || !strcmp(sp,"KBLIMIT")) {
		safe_snprintf(str,maxlen,"%lu",cfg.level_freecdtperday[useron.level]/1024L);
		return(str);
	}

	if(!strcmp(sp,"DAYBYTES")) {    /* amt of free cdts used today */
		safe_snprintf(str,maxlen,"%lu",cfg.level_freecdtperday[useron.level]-useron.freecdt);
		return(str);
	}

	if(!strcmp(sp,"BYTELIMIT")) {
		safe_snprintf(str,maxlen,"%lu",cfg.level_freecdtperday[useron.level]);
		return(str);
	}

	if(!strcmp(sp,"KBLEFT")) {
		safe_snprintf(str,maxlen,"%lu",(useron.cdt+useron.freecdt)/1024L);
		return(str);
	}

	if(!strcmp(sp,"BYTESLEFT")) {
		safe_snprintf(str,maxlen,"%lu",useron.cdt+useron.freecdt);
		return(str);
	}

	if(!strcmp(sp,"CONF")) {
		safe_snprintf(str,maxlen,"%s %s"
			,usrgrps ? cfg.grp[usrgrp[curgrp]]->sname :nulstr
			,usrgrps ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);
		return(str);
	}

	if(!strcmp(sp,"CONFNUM")) {
		safe_snprintf(str,maxlen,"%u %u",curgrp+1,cursub[curgrp]+1);
		return(str);
	}

	if(!strcmp(sp,"NUMDIR")) {
		safe_snprintf(str,maxlen,"%u %u",usrlibs ? curlib+1 : 0,usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"EXDATE") || !strcmp(sp,"EXPDATE"))
		return(unixtodstr(&cfg,useron.expire,str));

	if(!strcmp(sp,"EXPDAYS")) {
		now=time(NULL);
		l=(long)(useron.expire-now);
		if(l<0)
			l=0;
		safe_snprintf(str,maxlen,"%lu",l/(1440L*60L));
		return(str);
	}

	if(!strcmp(sp,"MEMO1"))
		return(useron.note);

	if(!strcmp(sp,"MEMO2") || !strcmp(sp,"COMPANY"))
		return(useron.name);

	if(!strcmp(sp,"ZIP"))
		return(useron.zipcode);

	if(!strcmp(sp,"HANGUP")) {
		hangup();
		return(nulstr);
	}

	/* Synchronet Specific */

	if(!strncmp(sp,"SETSTR:",7)) {
		strcpy(main_csi.str,sp+7);
		return(nulstr);
	}

	if(!strncmp(sp,"EXEC:",5)) {
		exec_bin(sp+5,&main_csi);
		return(nulstr);
	}

	if(!strncmp(sp,"EXEC_XTRN:",10)) {
		for(i=0;i<cfg.total_xtrns;i++)
			if(!stricmp(cfg.xtrn[i]->code,sp+10))
				break;
		if(i<cfg.total_xtrns)
			exec_xtrn(i);
		return(nulstr);
	}

	if(!strncmp(sp,"MENU:",5)) {
		menu(sp+5);
		return(nulstr);
	}

	if(!strncmp(sp,"TYPE:",5)) {
		printfile(cmdstr(sp+5,nulstr,nulstr,str),0);
		return(nulstr);
	}

	if(!strncmp(sp,"INCLUDE:",8)) {
		printfile(cmdstr(sp+8,nulstr,nulstr,str),P_NOCRLF|P_SAVEATR);
		return(nulstr);
	}

	if(!strcmp(sp,"QUESTION"))
		return(question);

	if(!strcmp(sp,"HANDLE"))
		return(useron.handle);

	if(!strcmp(sp,"CID") || !strcmp(sp,"IP"))
		return(cid);

	if(!strcmp(sp,"LOCAL-IP"))
		return(local_addr);

	if(!strcmp(sp,"CRLF"))
		return("\r\n");

	if(!strcmp(sp,"PUSHXY")) {
		ansi_save();
		return(nulstr);
	}

	if(!strcmp(sp,"POPXY")) {
		ansi_restore();
		return(nulstr);
	}

	if(!strcmp(sp,"HOME")) {
		cursor_home();
		return(nulstr);
	}

	if(!strcmp(sp,"CLRLINE")) {
		clearline();
		return(nulstr);
	}

	if(!strcmp(sp,"CLR2EOL")) {
		cleartoeol();
		return(nulstr);
	}

	if(!strcmp(sp,"CLR2EOS")) {
		cleartoeos();
		return(nulstr);
	}

	if(!strncmp(sp,"UP:",3)) {
		cursor_up(atoi(sp+3));
		return(str);
	}

	if(!strncmp(sp,"DOWN:",5)) {
		cursor_down(atoi(sp+5));
		return(str);
	}

	if(!strncmp(sp,"LEFT:",5)) {
		cursor_left(atoi(sp+5));
		return(str);
	}

	if(!strncmp(sp,"RIGHT:",6)) {
		cursor_right(atoi(sp+6));
		return(str);
	}

	if(!strncmp(sp,"GOTOXY:",7)) {
		tp=strchr(sp,',');
		if(tp!=NULL) {
			tp++;
			ansi_gotoxy(atoi(sp+7),atoi(tp));
		}
		return(nulstr);
	}

	if(!strcmp(sp,"GRP")) {
		if(SMB_IS_OPEN(&smb)) {
			if(smb.subnum==INVALID_SUB)
				return("Local");
			if(smb.subnum<cfg.total_subs)
				return(cfg.grp[cfg.sub[smb.subnum]->grp]->sname);
		}
		return(usrgrps ? cfg.grp[usrgrp[curgrp]]->sname : nulstr);
	}

	if(!strcmp(sp,"GRPL")) {
		if(SMB_IS_OPEN(&smb)) {
			if(smb.subnum==INVALID_SUB)
				return("Local");
			if(smb.subnum<cfg.total_subs)
				return(cfg.grp[cfg.sub[smb.subnum]->grp]->lname);
		}
		return(usrgrps ? cfg.grp[usrgrp[curgrp]]->lname : nulstr);
	}

	if(!strcmp(sp,"GN")) {
		if(SMB_IS_OPEN(&smb))
			ugrp=getusrgrp(smb.subnum);
		else
			ugrp=usrgrps ? curgrp+1 : 0;
		safe_snprintf(str,maxlen,"%u",ugrp);
		return(str);
	}

	if(!strcmp(sp,"GL")) {
		if(SMB_IS_OPEN(&smb))
			ugrp=getusrgrp(smb.subnum);
		else
			ugrp=usrgrps ? curgrp+1 : 0;
		safe_snprintf(str,maxlen,"%-4u",ugrp);
		return(str);
	}

	if(!strcmp(sp,"GR")) {
		if(SMB_IS_OPEN(&smb))
			ugrp=getusrgrp(smb.subnum);
		else
			ugrp=usrgrps ? curgrp+1 : 0;
		safe_snprintf(str,maxlen,"%4u",ugrp);
		return(str);
	}

	if(!strcmp(sp,"SUB")) {
		if(SMB_IS_OPEN(&smb)) {
			if(smb.subnum==INVALID_SUB)
				return("Mail");
			else if(smb.subnum<cfg.total_subs)
				return(cfg.sub[smb.subnum]->sname);
		}
		return(usrgrps ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);
	}

	if(!strcmp(sp,"SUBL")) {
		if(SMB_IS_OPEN(&smb)) {
			if(smb.subnum==INVALID_SUB)
				return("Mail");
			else if(smb.subnum<cfg.total_subs)
				return(cfg.sub[smb.subnum]->lname);
		}
		return(usrgrps  ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->lname : nulstr);
	}

	if(!strcmp(sp,"SN")) {
		if(SMB_IS_OPEN(&smb))
			usub=getusrsub(smb.subnum);
		else
			usub=usrgrps ? cursub[curgrp]+1 : 0;
		safe_snprintf(str,maxlen,"%u",usub);
		return(str);
	}

	if(!strcmp(sp,"SL")) {
		if(SMB_IS_OPEN(&smb))
			usub=getusrsub(smb.subnum);
		else
			usub=usrgrps ? cursub[curgrp]+1 : 0;
		safe_snprintf(str,maxlen,"%-4u",usub);
		return(str);
	}

	if(!strcmp(sp,"SR")) {
		if(SMB_IS_OPEN(&smb))
			usub=getusrsub(smb.subnum);
		else
			usub=usrgrps ? cursub[curgrp]+1 : 0;
		safe_snprintf(str,maxlen,"%4u",usub);
		return(str);
	}

	if(!strcmp(sp,"LIB"))
		return(usrlibs ? cfg.lib[usrlib[curlib]]->sname : nulstr);

	if(!strcmp(sp,"LIBL"))
		return(usrlibs ? cfg.lib[usrlib[curlib]]->lname : nulstr);

	if(!strcmp(sp,"LN")) {
		safe_snprintf(str,maxlen,"%u",usrlibs ? curlib+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"LL")) {
		safe_snprintf(str,maxlen,"%-4u",usrlibs ? curlib+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"LR")) {
		safe_snprintf(str,maxlen,"%4u",usrlibs  ? curlib+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"DIR"))
		return(usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->sname :nulstr);

	if(!strcmp(sp,"DIRL"))
		return(usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr);

	if(!strcmp(sp,"DN")) {
		safe_snprintf(str,maxlen,"%u",usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"DL")) {
		safe_snprintf(str,maxlen,"%-4u",usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"DR")) {
		safe_snprintf(str,maxlen,"%4u",usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"NOACCESS")) {
		if(noaccess_str==text[NoAccessTime])
			safe_snprintf(str,maxlen,noaccess_str,noaccess_val/60,noaccess_val%60);
		else if(noaccess_str==text[NoAccessDay])
			safe_snprintf(str,maxlen,noaccess_str,wday[noaccess_val]);
		else
			safe_snprintf(str,maxlen,noaccess_str,noaccess_val);
		return(str);
	}

	if(!strcmp(sp,"LAST")) {
		tp=strrchr(useron.alias,' ');
		if(tp) tp++;
		else tp=useron.alias;
		return(tp);
	}

	if(!strcmp(sp,"REAL") || !strcmp(sp,"FIRSTREAL")) {
		safe_snprintf(str,maxlen,"%s",useron.name);
		tp=strchr(str,' ');
		if(tp) *tp=0;
		return(str);
	}

	if(!strcmp(sp,"LASTREAL")) {
		tp=strrchr(useron.name,' ');
		if(tp) tp++;
		else tp=useron.name;
		return(tp);
	}

	if(!strcmp(sp,"MAILW")) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,useron.number,0));
		return(str);
	}

	if(!strcmp(sp,"MAILP")) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,useron.number,1));
		return(str);
	}

	if(!strncmp(sp,"MAILW:",6)) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,atoi(sp+6),0));
		return(str);
	}

	if(!strncmp(sp,"MAILP:",6)) {
		safe_snprintf(str,maxlen,"%u",getmail(&cfg,atoi(sp+6),1));
		return(str);
	}

	if(!strcmp(sp,"MSGREPLY")) {
		safe_snprintf(str,maxlen,"%c",cfg.sys_misc&SM_RA_EMU ? 'R' : 'A');
		return(str);
	}

	if(!strcmp(sp,"MSGREREAD")) {
		safe_snprintf(str,maxlen,"%c",cfg.sys_misc&SM_RA_EMU ? 'A' : 'R');
		return(str);
	}

	if(!strncmp(sp,"STATS.",6)) {
		getstats(&cfg,0,&stats);
		sp+=6;
		if(!strcmp(sp,"LOGONS"))
			safe_snprintf(str,maxlen,"%lu",stats.logons);
		else if(!strcmp(sp,"LTODAY"))
			safe_snprintf(str,maxlen,"%lu",stats.ltoday);
		else if(!strcmp(sp,"TIMEON"))
			safe_snprintf(str,maxlen,"%lu",stats.timeon);
		else if(!strcmp(sp,"TTODAY"))
			safe_snprintf(str,maxlen,"%lu",stats.ttoday);
		else if(!strcmp(sp,"ULS"))
			safe_snprintf(str,maxlen,"%lu",stats.uls);
		else if(!strcmp(sp,"ULB"))
			safe_snprintf(str,maxlen,"%lu",stats.ulb);
		else if(!strcmp(sp,"DLS"))
			safe_snprintf(str,maxlen,"%lu",stats.dls);
		else if(!strcmp(sp,"DLB"))
			safe_snprintf(str,maxlen,"%lu",stats.dlb);
		else if(!strcmp(sp,"PTODAY"))
			safe_snprintf(str,maxlen,"%lu",stats.ptoday);
		else if(!strcmp(sp,"ETODAY"))
			safe_snprintf(str,maxlen,"%lu",stats.etoday);
		else if(!strcmp(sp,"FTODAY"))
			safe_snprintf(str,maxlen,"%lu",stats.ftoday);
		else if(!strcmp(sp,"NUSERS"))
			safe_snprintf(str,maxlen,"%u",stats.nusers);
		return(str);
	}

	/* Message header codes */
	if(!strcmp(sp,"MSG_TO") && current_msg!=NULL) {
		if(current_msg->to==NULL)
			return(nulstr);
		if(current_msg->to_ext!=NULL)
			safe_snprintf(str,maxlen,"%s #%s",current_msg->to,current_msg->to_ext);
		else if(current_msg->to_net.addr != NULL) {
			char tmp[128];
			safe_snprintf(str,maxlen,"%s (%s)",current_msg->to
				,smb_netaddrstr(&current_msg->to_net,tmp));
		} else
			return(current_msg->to);
		return(str);
	}
	if(!strcmp(sp,"MSG_TO_NAME") && current_msg!=NULL)
		return(current_msg->to==NULL ? nulstr : current_msg->to);
	if(!strcmp(sp,"MSG_TO_EXT") && current_msg!=NULL) {
		if(current_msg->to_ext==NULL)
			return(nulstr);
		return(current_msg->to_ext);
	}
	if(!strcmp(sp,"MSG_TO_NET") && current_msg!=NULL) {
		if(current_msg->to_net.addr == NULL)
			return nulstr;
		return(smb_netaddrstr(&current_msg->to_net,str));
	}
	if(!strcmp(sp,"MSG_TO_NETTYPE") && current_msg!=NULL) {
		if(current_msg->to_net.type==NET_NONE)
			return nulstr;
		return(smb_nettype((enum smb_net_type)current_msg->to_net.type));
	}
	if(!strcmp(sp,"MSG_FROM") && current_msg!=NULL) {
		if(current_msg->from==NULL)
			return(nulstr);
		if(current_msg->hdr.attr&MSG_ANONYMOUS && !SYSOP)
			return(text[Anonymous]);
		if(current_msg->from_ext!=NULL)
			safe_snprintf(str,maxlen,"%s #%s",current_msg->from,current_msg->from_ext);
		else if(current_msg->from_net.addr != NULL) {
			char tmp[128];
			safe_snprintf(str,maxlen,"%s (%s)",current_msg->from
				,smb_netaddrstr(&current_msg->from_net,tmp));
		} else
			return(current_msg->from);
		return(str);
	}
	if(!strcmp(sp,"MSG_FROM_NAME") && current_msg!=NULL) {
		if(current_msg->from==NULL)
			return(nulstr);
		if(current_msg->hdr.attr&MSG_ANONYMOUS && !SYSOP)
			return(text[Anonymous]);
		return(current_msg->from);
	}
	if(!strcmp(sp,"MSG_FROM_EXT") && current_msg!=NULL) {
		if(!(current_msg->hdr.attr&MSG_ANONYMOUS) || SYSOP)
			if(current_msg->from_ext!=NULL)
				return(current_msg->from_ext);
		return(nulstr);
	}
	if(!strcmp(sp,"MSG_FROM_NET") && current_msg!=NULL) {
		if(current_msg->from_net.addr!=NULL
			&& (!(current_msg->hdr.attr&MSG_ANONYMOUS) || SYSOP))
			return(smb_netaddrstr(&current_msg->from_net,str));
		return(nulstr);
	}
	if(!strcmp(sp,"MSG_FROM_NETTYPE") && current_msg!=NULL) {
		if(current_msg->from_net.type==NET_NONE)
			return nulstr;
		return(smb_nettype((enum smb_net_type)current_msg->from_net.type));
	}
	if(!strcmp(sp,"MSG_SUBJECT") && current_msg!=NULL)
		return(current_msg->subj==NULL ? nulstr : current_msg->subj);
	if(!strcmp(sp,"MSG_DATE") && current_msg!=NULL)
		return(timestr(current_msg->hdr.when_written.time));
	if(!strcmp(sp,"MSG_AGE") && current_msg!=NULL)
		return age_of_posted_item(str, maxlen
			, current_msg->hdr.when_written.time - (smb_tzutc(current_msg->hdr.when_written.zone) * 60));
	if(!strcmp(sp,"MSG_TIMEZONE") && current_msg!=NULL)
		return(smb_zonestr(current_msg->hdr.when_written.zone,NULL));
	if(!strcmp(sp,"MSG_ATTR") && current_msg!=NULL) {
		uint16_t attr = current_msg->hdr.attr;
		uint16_t poll = attr&MSG_POLL_VOTE_MASK;
		uint32_t auxattr = current_msg->hdr.auxattr;
		safe_snprintf(str,maxlen,"%s%s%s%s%s%s%s%s%s%s%s%s%s"
			,attr&MSG_PRIVATE						? "Private  "   :nulstr
			,attr&MSG_READ							? "Read  "      :nulstr
			,attr&MSG_DELETE						? "Deleted  "   :nulstr
			,attr&MSG_KILLREAD						? "Kill  "      :nulstr
			,attr&MSG_ANONYMOUS						? "Anonymous  " :nulstr
			,attr&MSG_LOCKED						? "Locked  "    :nulstr
			,attr&MSG_PERMANENT						? "Permanent  " :nulstr
			,attr&MSG_MODERATED						? "Moderated  " :nulstr
			,attr&MSG_VALIDATED						? "Validated  " :nulstr
			,attr&MSG_REPLIED						? "Replied  "	:nulstr
			,attr&MSG_NOREPLY						? "NoReply  "	:nulstr
			,poll == MSG_POLL						? "Poll  "		:nulstr
			,poll == MSG_POLL && auxattr&POLL_CLOSED	? "(Closed)  "	:nulstr
			);
		return(str);
	}
	if(!strcmp(sp,"MSG_AUXATTR") && current_msg!=NULL) {
		safe_snprintf(str,maxlen,"%s%s%s%s%s%s%s"
			,current_msg->hdr.auxattr&MSG_FILEREQUEST	? "FileRequest  "   :nulstr
			,current_msg->hdr.auxattr&MSG_FILEATTACH	? "FileAttach  "    :nulstr
			,current_msg->hdr.auxattr&MSG_TRUNCFILE		? "TruncFile  "		:nulstr
			,current_msg->hdr.auxattr&MSG_KILLFILE		? "KillFile  "      :nulstr
			,current_msg->hdr.auxattr&MSG_RECEIPTREQ	? "ReceiptReq  "	:nulstr
			,current_msg->hdr.auxattr&MSG_CONFIRMREQ	? "ConfirmReq  "    :nulstr
			,current_msg->hdr.auxattr&MSG_NODISP		? "DontDisplay  "	:nulstr
			);
		return(str);
	}
	if(!strcmp(sp,"MSG_NETATTR") && current_msg!=NULL) {
		safe_snprintf(str,maxlen,"%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s"
			,current_msg->hdr.netattr&MSG_LOCAL			? "Local  "			:nulstr
			,current_msg->hdr.netattr&MSG_INTRANSIT		? "InTransit  "     :nulstr
			,current_msg->hdr.netattr&MSG_SENT			? "Sent  "			:nulstr
			,current_msg->hdr.netattr&MSG_KILLSENT		? "KillSent  "      :nulstr
			,current_msg->hdr.netattr&MSG_ARCHIVESENT	? "ArcSent  "		:nulstr
			,current_msg->hdr.netattr&MSG_HOLD			? "Hold  "			:nulstr
			,current_msg->hdr.netattr&MSG_CRASH			? "Crash  "			:nulstr
			,current_msg->hdr.netattr&MSG_IMMEDIATE		? "Immediate  "		:nulstr
			,current_msg->hdr.netattr&MSG_DIRECT		? "Direct  "		:nulstr
			,current_msg->hdr.netattr&MSG_GATE			? "Gate  "			:nulstr
			,current_msg->hdr.netattr&MSG_ORPHAN		? "Orphan  "		:nulstr
			,current_msg->hdr.netattr&MSG_FPU			? "ForcePickup  "	:nulstr
			,current_msg->hdr.netattr&MSG_TYPELOCAL		? "LocalUse  "		:nulstr
			,current_msg->hdr.netattr&MSG_TYPEECHO		? "EchoMail  "		:nulstr
			,current_msg->hdr.netattr&MSG_TYPENET		? "NetMail  "		:nulstr
			);
		return(str);
	}
	if(!strcmp(sp,"MSG_ID") && current_msg!=NULL)
		return(current_msg->id==NULL ? nulstr : current_msg->id);
	if(!strcmp(sp,"MSG_REPLY_ID") && current_msg!=NULL)
		return(current_msg->reply_id==NULL ? nulstr : current_msg->reply_id);
	if(!strcmp(sp,"MSG_NUM") && current_msg!=NULL) {
		safe_snprintf(str,maxlen,"%lu",current_msg->hdr.number);
		return(str);
	}
	if(!strcmp(sp,"MSG_SCORE") && current_msg!=NULL) {
		safe_snprintf(str, maxlen, "%ld", current_msg->upvotes - current_msg->downvotes);
		return(str);
	}
	if(!strcmp(sp,"MSG_UPVOTES") && current_msg!=NULL) {
		safe_snprintf(str, maxlen, "%lu", current_msg->upvotes);
		return(str);
	}
	if(!strcmp(sp,"MSG_DOWNVOTES") && current_msg!=NULL) {
		safe_snprintf(str, maxlen, "%lu", current_msg->downvotes);
		return(str);
	}
	if(!strcmp(sp,"MSG_TOTAL_VOTES") && current_msg!=NULL) {
		safe_snprintf(str, maxlen, "%lu", current_msg->total_votes);
		return(str);
	}
	if(!strcmp(sp,"MSG_VOTED"))
		return (current_msg != NULL && current_msg->user_voted) ? text[PollAnswerChecked] : nulstr;
	if(!strcmp(sp,"MSG_UPVOTED"))
		return (current_msg != NULL && current_msg->user_voted == 1) ? text[PollAnswerChecked] : nulstr;
	if(!strcmp(sp,"MSG_DOWNVOTED"))
		return (current_msg != NULL && current_msg->user_voted == 2) ? text[PollAnswerChecked] : nulstr;

	if(!strcmp(sp,"SMB_AREA")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			safe_snprintf(str,maxlen,"%s %s"
				,cfg.grp[cfg.sub[smb.subnum]->grp]->sname
				,cfg.sub[smb.subnum]->sname);
		else
			strncpy(str, "Email", maxlen);
		return(str);
	}
	if(!strcmp(sp,"SMB_AREA_DESC")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			safe_snprintf(str,maxlen,"%s %s"
				,cfg.grp[cfg.sub[smb.subnum]->grp]->lname
				,cfg.sub[smb.subnum]->lname);
		else
			strncpy(str, "Personal Email", maxlen);
		return(str);
	}
	if(!strcmp(sp,"SMB_GROUP")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			return(cfg.grp[cfg.sub[smb.subnum]->grp]->sname);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_GROUP_DESC")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			return(cfg.grp[cfg.sub[smb.subnum]->grp]->lname);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_GROUP_NUM")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			safe_snprintf(str,maxlen,"%u",getusrgrp(smb.subnum));
		return(str);
	}
	if(!strcmp(sp,"SMB_SUB")) {
		if(smb.subnum==INVALID_SUB)
			return("Mail");
		else if(smb.subnum<cfg.total_subs)
			return(cfg.sub[smb.subnum]->sname);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_SUB_DESC")) {
		if(smb.subnum==INVALID_SUB)
			return("Mail");
		else if(smb.subnum<cfg.total_subs)
			return(cfg.sub[smb.subnum]->lname);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_SUB_CODE")) {
		if(smb.subnum==INVALID_SUB)
			return("MAIL");
		else if(smb.subnum<cfg.total_subs)
			return(cfg.sub[smb.subnum]->code);
		return(nulstr);
	}
	if(!strcmp(sp,"SMB_SUB_NUM")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			safe_snprintf(str,maxlen,"%u",getusrsub(smb.subnum));
		return(str);
	}
	if(!strcmp(sp,"SMB_MSGS")) {
		safe_snprintf(str,maxlen,"%ld",smb.msgs);
		return(str);
	}
	if(!strcmp(sp,"SMB_CURMSG")) {
		safe_snprintf(str,maxlen,"%ld",smb.curmsg+1);
		return(str);
	}
	if(!strcmp(sp,"SMB_LAST_MSG")) {
		safe_snprintf(str,maxlen,"%lu",smb.status.last_msg);
		return(str);
	}
	if(!strcmp(sp,"SMB_MAX_MSGS")) {
		safe_snprintf(str,maxlen,"%lu",smb.status.max_msgs);
		return(str);
	}
	if(!strcmp(sp,"SMB_MAX_CRCS")) {
		safe_snprintf(str,maxlen,"%lu",smb.status.max_crcs);
		return(str);
	}
	if(!strcmp(sp,"SMB_MAX_AGE")) {
		safe_snprintf(str,maxlen,"%hu",smb.status.max_age);
		return(str);
	}
	if(!strcmp(sp,"SMB_TOTAL_MSGS")) {
		safe_snprintf(str,maxlen,"%lu",smb.status.total_msgs);
		return(str);
	}

	return(NULL);
}

