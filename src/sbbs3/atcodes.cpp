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

extern "C" const char* beta_version;

/****************************************************************************/
/* Returns 0 if invalid @ code. Returns length of @ code if valid.          */
/****************************************************************************/
int sbbs_t::show_atcode(char *instr)
{
	char	str[128],str2[128],*p,*tp,*sp;
    int     len;
	bool	padded_left=false;
	bool	padded_right=false;

	sprintf(str,"%.80s",instr);
	tp=strchr(str+1,'@');
	if(!tp)                 /* no terminating @ */
		return(0);
	sp=strchr(str+1,SP);
	if(sp && sp<tp)         /* space before terminating @ */
		return(0);
	len=(tp-str)+1;
	(*tp)=0;
	sp=(str+1);

	if((p=strstr(sp,"-L"))!=NULL)
		padded_left=true;
	else if((p=strstr(sp,"-R"))!=NULL)
		padded_right=true;
	if(p!=NULL)
		*p=0;

	p=atcode(sp,str2);
	if(p==NULL)
		return(0);

	if(padded_left)
		rprintf("%-*.*s",len,len,p);
	else if(padded_right)
		rprintf("%*.*s",len,len,p);
	else
		rputs(p);

	return(len);
}

char* sbbs_t::atcode(char* sp, char* str)
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
		sprintf(str,"%c",REVISION);
		return(str);
	}

	if(!strcmp(sp,"FULL_VER")) {
		sprintf(str,"%s%c%s",VERSION,REVISION,beta_version);
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
		extern time_t uptime;
		time_t up=time(NULL)-uptime;
		if(up<0)
			up=0;
		char   days[64]="";
		if((up/(24*60*60))>=2) {
	        sprintf(days,"%lu days ",(ulong)(up/(24L*60L*60L)));
			up%=(24*60*60);
		}
		sprintf(str,"%s%lu:%02lu"
	        ,days
			,(ulong)(up/(60L*60L))
			,(ulong)((up/60L)%60L)
			);
		return(str);
	}

	if(!strcmp(sp,"SOCKET_LIB")) 
		return(socklib_version(str));

	if(!strcmp(sp,"MSG_LIB")) {
		sprintf(str,"SMBLIB %s",smb_lib_ver());
		return(str);
	}

	if(!strcmp(sp,"BBS") || !strcmp(sp,"BOARDNAME"))
		return(cfg.sys_name);

	if(!strcmp(sp,"BAUD") || !strcmp(sp,"BPS")) {
		sprintf(str,"%lu",cur_rate);
		return(str);
	}

	if(!strcmp(sp,"CONN"))
		return(connection);

	if(!strcmp(sp,"SYSOP"))
		return(cfg.sys_op);

	if(!strcmp(sp,"LOCATION"))
		return(cfg.sys_location);

	if(!strcmp(sp,"NODE")) {
		sprintf(str,"%u",cfg.node_num);
		return(str);
	}

	if(!strcmp(sp,"TNODE")) {
		sprintf(str,"%u",cfg.sys_nodes);
		return(str);
	}

	if(!strcmp(sp,"INETADDR"))
		return(cfg.sys_inetaddr);

	if(!strcmp(sp,"HOSTNAME"))
		return(startup->host_name);

	if(!strcmp(sp,"FIDOADDR")) {
		if(cfg.total_faddrs)
			return(faddrtoa(&cfg.faddr[0],str));
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
		sprintf(str,"%02d:%02d %s"
			,tm.tm_hour==0 ? 12
			: tm.tm_hour>12 ? tm.tm_hour-12
			: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am"); 
		return(str);
	}

	if(!strcmp(sp,"DATE") || !strcmp(sp,"SYSDATE")) {
		now=time(NULL);
		return(unixtodstr(&cfg,now,str)); 
	}

	if(!strcmp(sp,"TMSG")) {
		l=0;
		for(i=0;i<cfg.total_subs;i++)
			l+=getposts(&cfg,i); 		/* l=total posts */
		sprintf(str,"%lu",l); 
		return(str);
	}

	if(!strcmp(sp,"TUSER")) {
		sprintf(str,"%u",lastuser(&cfg));
		return(str);
	}

	if(!strcmp(sp,"TFILE")) {
		l=0;
		for(i=0;i<cfg.total_dirs;i++)
			l+=getfiles(&cfg,i);
		sprintf(str,"%lu",l); 
		return(str);
	}

	if(!strcmp(sp,"TCALLS") || !strcmp(sp,"NUMCALLS")) {
		getstats(&cfg,0,&stats);
		sprintf(str,"%lu",stats.logons); 
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

	// else if(!strcmp(sp,"EVENT"))
	//	  return(sectostr(sys_eventtime,str2));

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
		strcpy(str,useron.alias);
		tp=strchr(str,SP);
		if(tp) *tp=0;
		return(str); 
	}

	if(!strcmp(sp,"USERNUM")) {
		sprintf(str,"%u",useron.number);
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
		strcpy(str,useron.location);
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
			if(*p==SP)
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
		sprintf(str,"%u",getage(&cfg,useron.birth));
		return(str);
	}

	if(!strcmp(sp,"CALLS") || !strcmp(sp,"NUMTIMESON")) {
		sprintf(str,"%u",useron.logons);
		return(str);
	}

	if(!strcmp(sp,"MEMO"))
		return(unixtodstr(&cfg,useron.pwmod,str));

	if(!strcmp(sp,"SEC") || !strcmp(sp,"SECURITY")) {
		sprintf(str,"%u",useron.level);
		return(str);
	}

	if(!strcmp(sp,"SINCE"))
		return(unixtodstr(&cfg,useron.firston,str));

	if(!strcmp(sp,"TIMEON") || !strcmp(sp,"TIMEUSED")) {
		now=time(NULL);
		sprintf(str,"%lu",(ulong)(now-logontime)/60L); 
		return(str);
	}

	if(!strcmp(sp,"TUSED")) {              /* Synchronet only */
		now=time(NULL);
		return(sectostr(now-logontime,str)+1); 
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
		sprintf(str,"%u",cfg.level_timepercall[useron.level]);
		return(str);
	}

	if(!strcmp(sp,"MINLEFT") || !strcmp(sp,"LEFT") || !strcmp(sp,"TIMELEFT")) {
		gettimeleft();
		sprintf(str,"%lu",timeleft/60); 
		return(str);
	}

	if(!strcmp(sp,"LASTON")) 
		return(timestr(&useron.laston));

	if(!strcmp(sp,"LASTDATEON"))
		return(unixtodstr(&cfg,useron.laston,str));

	if(!strcmp(sp,"LASTTIMEON")) {
		memset(&tm,0,sizeof(tm));
		localtime_r(&useron.laston,&tm);
		sprintf(str,"%02d:%02d %s"
			,tm.tm_hour==0 ? 12
			: tm.tm_hour>12 ? tm.tm_hour-12
			: tm.tm_hour, tm.tm_min, tm.tm_hour>11 ? "pm":"am"); 
		return(str);
	}

	if(!strcmp(sp,"MSGLEFT") || !strcmp(sp,"MSGSLEFT")) {
		sprintf(str,"%u",useron.posts);
		return(str);
	}

	if(!strcmp(sp,"MSGREAD")) {
		sprintf(str,"%lu",posts_read);
		return(str);
	}

	if(!strcmp(sp,"FREESPACE")) {
		sprintf(str,"%lu",getfreediskspace(cfg.temp_dir)); 
		return(str);
	}

	if(!strcmp(sp,"UPBYTES")) {
		sprintf(str,"%lu",useron.ulb);
		return(str);
	}

	if(!strcmp(sp,"UPK")) {
		sprintf(str,"%lu",useron.ulb/1024L);
		return(str);
	}

	if(!strcmp(sp,"UPS") || !strcmp(sp,"UPFILES")) {
		sprintf(str,"%u",useron.uls);
		return(str);
	}

	if(!strcmp(sp,"DLBYTES")) {
		sprintf(str,"%lu",useron.dlb);
		return(str);
	}

	if(!strcmp(sp,"DOWNK")) {
		sprintf(str,"%lu",useron.dlb/1024L);
		return(str);
	}

	if(!strcmp(sp,"DOWNS") || !strcmp(sp,"DLFILES")) {
		sprintf(str,"%u",useron.dls);
		return(str);
	}

	if(!strcmp(sp,"LASTNEW"))
		return(unixtodstr(&cfg,ns_time,str));

	if(!strcmp(sp,"NEWFILETIME"))
		return(timestr(&ns_time));

	/* MAXDL */

	if(!strcmp(sp,"MAXDK") || !strcmp(sp,"DLKLIMIT") || !strcmp(sp,"KBLIMIT")) {
		sprintf(str,"%lu",cfg.level_freecdtperday[useron.level]/1024L);
		return(str);
	}

	if(!strcmp(sp,"DAYBYTES")) {    /* amt of free cdts used today */
		sprintf(str,"%lu",cfg.level_freecdtperday[useron.level]-useron.freecdt);
		return(str);
	}

	if(!strcmp(sp,"BYTELIMIT")) {
		sprintf(str,"%lu",cfg.level_freecdtperday[useron.level]);
		return(str);
	}

	if(!strcmp(sp,"KBLEFT")) {
		sprintf(str,"%lu",(useron.cdt+useron.freecdt)/1024L);
		return(str);
	}

	if(!strcmp(sp,"BYTESLEFT")) {
		sprintf(str,"%lu",useron.cdt+useron.freecdt);
		return(str);
	}

	if(!strcmp(sp,"CONF")) {
		sprintf(str,"%s %s"
			,usrgrps ? cfg.grp[usrgrp[curgrp]]->sname :nulstr
			,usrgrps ? cfg.sub[usrsub[curgrp][cursub[curgrp]]]->sname : nulstr);
		return(str);
	}

	if(!strcmp(sp,"CONFNUM")) {
		sprintf(str,"%u %u",curgrp+1,cursub[curgrp]+1);
		return(str);
	}

	if(!strcmp(sp,"NUMDIR")) {
		sprintf(str,"%u %u",usrlibs ? curlib+1 : 0,usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"EXDATE") || !strcmp(sp,"EXPDATE"))
		return(unixtodstr(&cfg,useron.expire,str));

	if(!strcmp(sp,"EXPDAYS")) {
		now=time(NULL);
		l=useron.expire-now;
		if(l<0)
			l=0;
		sprintf(str,"%lu",l/(1440L*60L)); 
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

	if(!strncmp(sp,"MENU:",5)) {
		menu(sp+5);
		return(nulstr);
	}

	if(!strncmp(sp,"TYPE:",5)) {
		printfile(cmdstr(sp+5,nulstr,nulstr,str),0);
		return(nulstr);
	}

	if(!strcmp(sp,"QUESTION"))
		return(question);

	if(!strcmp(sp,"HANDLE"))
		return(useron.handle);

	if(!strcmp(sp,"CID") || !strcmp(sp,"IP"))
		return(cid);

	if(!strcmp(sp,"LOCAL-IP")) {
		struct in_addr in_addr;
		in_addr.s_addr=local_addr;
		return(inet_ntoa(in_addr));
	}

	if(!strcmp(sp,"CRLF"))
		return("\r\n");

	if(!strcmp(sp,"PUSHXY")) {
		ANSI_SAVE();
		return(nulstr);
	}

	if(!strcmp(sp,"POPXY")) {
		ANSI_RESTORE();
		return(nulstr);
	}

	if(!strcmp(sp,"UP")) 
		return("\x1b[A");

	if(!strcmp(sp,"DOWN")) 
		return("\x1b[B");

	if(!strcmp(sp,"RIGHT")) 
		return("\x1b[C");

	if(!strcmp(sp,"LEFT")) 
		return("\x1b[D");

	if(!strncmp(sp,"UP:",3)) {
		sprintf(str,"\x1b[%dA",atoi(sp+3));
		return(str);
	}

	if(!strncmp(sp,"DOWN:",5)) {
		sprintf(str,"\x1b[%dB",atoi(sp+5));
		return(str);
	}

	if(!strncmp(sp,"LEFT:",5)) {
		sprintf(str,"\x1b[%dC",atoi(sp+5));
		return(str);
	}

	if(!strncmp(sp,"RIGHT:",6)) {
		sprintf(str,"\x1b[%dD",atoi(sp+6));
		return(str);
	}

	if(!strncmp(sp,"GOTOXY:",7)) {
		tp=strchr(sp,',');
		if(tp!=NULL) {
			tp++;
			GOTOXY(atoi(sp+7),atoi(tp));
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
		sprintf(str,"%u",ugrp);
		return(str);
	}

	if(!strcmp(sp,"GL")) {
		if(SMB_IS_OPEN(&smb))
			ugrp=getusrgrp(smb.subnum);
		else 
			ugrp=usrgrps ? curgrp+1 : 0;
		sprintf(str,"%-4u",ugrp);
		return(str);
	}

	if(!strcmp(sp,"GR")) {
		if(SMB_IS_OPEN(&smb))
			ugrp=getusrgrp(smb.subnum);
		else 
			ugrp=usrgrps ? curgrp+1 : 0;
		sprintf(str,"%4u",ugrp);
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
		sprintf(str,"%u",usub);
		return(str);
	}

	if(!strcmp(sp,"SL")) {
		if(SMB_IS_OPEN(&smb))
			usub=getusrsub(smb.subnum);
		else
			usub=usrgrps ? cursub[curgrp]+1 : 0;
		sprintf(str,"%-4u",usub);
		return(str);
	}

	if(!strcmp(sp,"SR")) {
		if(SMB_IS_OPEN(&smb))
			usub=getusrsub(smb.subnum);
		else
			usub=usrgrps ? cursub[curgrp]+1 : 0;
		sprintf(str,"%4u",usub);
		return(str);
	}

	if(!strcmp(sp,"LIB"))
		return(usrlibs ? cfg.lib[usrlib[curlib]]->sname : nulstr);

	if(!strcmp(sp,"LIBL"))
		return(usrlibs ? cfg.lib[usrlib[curlib]]->lname : nulstr);

	if(!strcmp(sp,"LN")) {
		sprintf(str,"%u",usrlibs ? curlib+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"LL")) {
		sprintf(str,"%-4u",usrlibs ? curlib+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"LR")) {
		sprintf(str,"%4u",usrlibs  ? curlib+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"DIR"))
		return(usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->sname :nulstr);

	if(!strcmp(sp,"DIRL"))
		return(usrlibs ? cfg.dir[usrdir[curlib][curdir[curlib]]]->lname : nulstr);

	if(!strcmp(sp,"DN")) {
		sprintf(str,"%u",usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"DL")) {
		sprintf(str,"%-4u",usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"DR")) {
		sprintf(str,"%4u",usrlibs ? curdir[curlib]+1 : 0);
		return(str);
	}

	if(!strcmp(sp,"NOACCESS")) {
		if(noaccess_str==text[NoAccessTime])
			sprintf(str,noaccess_str,noaccess_val/60,noaccess_val%60);
		else if(noaccess_str==text[NoAccessDay])
			sprintf(str,noaccess_str,wday[noaccess_val]);
		else
			sprintf(str,noaccess_str,noaccess_val); 
		return(str);
	}

	if(!strcmp(sp,"LAST")) {
		tp=strrchr(useron.alias,SP);
		if(tp) tp++;
		else tp=useron.alias;
		return(tp); 
	}

	if(!strcmp(sp,"REAL")) {
		strcpy(str,useron.name);
		tp=strchr(str,SP);
		if(tp) *tp=0;
		return(str); 
	}

	if(!strcmp(sp,"FIRSTREAL")) {
		strcpy(str,useron.name);
		tp=strchr(str,SP);
		if(tp) *tp=0;
		return(str); 
	}

	if(!strcmp(sp,"LASTREAL")) {
		tp=strrchr(useron.name,SP);
		if(tp) tp++;
		else tp=useron.name;
		return(tp); 
	}

	if(!strcmp(sp,"MAILW")) {
		sprintf(str,"%u",getmail(&cfg,useron.number,0));
		return(str);
	}

	if(!strcmp(sp,"MAILP")) {
		sprintf(str,"%u",getmail(&cfg,useron.number,1));
		return(str);
	}

	if(!strncmp(sp,"MAILW:",6)) {
		sprintf(str,"%u",getmail(&cfg,atoi(sp+6),0));
		return(str);
	}

	if(!strncmp(sp,"MAILP:",6)) {
		sprintf(str,"%u",getmail(&cfg,atoi(sp+6),1));
		return(str);
	}

	if(!strcmp(sp,"MSGREPLY")) {
		sprintf(str,"%c",cfg.sys_misc&SM_RA_EMU ? 'R' : 'A');
		return(str);
	}

	if(!strcmp(sp,"MSGREREAD")) {
		sprintf(str,"%c",cfg.sys_misc&SM_RA_EMU ? 'A' : 'R');
		return(str);
	}

	if(!strncmp(sp,"STATS.",6)) {
		getstats(&cfg,0,&stats);
		sp+=6;
		if(!strcmp(sp,"LOGONS")) 
			sprintf(str,"%lu",stats.logons);
		else if(!strcmp(sp,"LTODAY")) 
			sprintf(str,"%lu",stats.ltoday);
		else if(!strcmp(sp,"TIMEON")) 
			sprintf(str,"%lu",stats.timeon);
		else if(!strcmp(sp,"TTODAY")) 
			sprintf(str,"%lu",stats.ttoday);
		else if(!strcmp(sp,"ULS")) 
			sprintf(str,"%lu",stats.uls);
		else if(!strcmp(sp,"ULB")) 
			sprintf(str,"%lu",stats.ulb);
		else if(!strcmp(sp,"DLS")) 
			sprintf(str,"%lu",stats.dls);
		else if(!strcmp(sp,"DLB")) 
			sprintf(str,"%lu",stats.dlb);
		else if(!strcmp(sp,"PTODAY")) 
			sprintf(str,"%lu",stats.ptoday);
		else if(!strcmp(sp,"ETODAY")) 
			sprintf(str,"%lu",stats.etoday);
		else if(!strcmp(sp,"FTODAY")) 
			sprintf(str,"%lu",stats.ftoday);
		else if(!strcmp(sp,"NUSERS")) 
			sprintf(str,"%u",stats.nusers);
		return(str);
	}

	/* Message header codes */
	if(!strcmp(sp,"MSG_TO") && current_msg!=NULL) {
		if(current_msg->to==NULL)
			return(nulstr);
		if(current_msg->to_ext!=NULL)
			sprintf(str,"%s #%s",current_msg->to,current_msg->to_ext);
		else if(current_msg->to_net.type!=NET_NONE)
			sprintf(str,"%s (%s)",current_msg->to
				,net_addr(&current_msg->to_net));
		else
			strcpy(str,current_msg->to);
		return(str);
	}
	if(!strcmp(sp,"MSG_TO_NAME") && current_msg!=NULL)
		return(current_msg->to==NULL ? nulstr : current_msg->to);
	if(!strcmp(sp,"MSG_TO_EXT") && current_msg!=NULL) {
		if(current_msg->to_ext==NULL)
			return(nulstr);
		return(current_msg->to_ext);
	}
	if(!strcmp(sp,"MSG_TO_NET") && current_msg!=NULL)
		return(net_addr(&current_msg->to_net));
	if(!strcmp(sp,"MSG_FROM") && current_msg!=NULL) {
		if(current_msg->from==NULL)
			return(nulstr);
		if(current_msg->hdr.attr&MSG_ANONYMOUS && !SYSOP)
			return(text[Anonymous]);
		if(current_msg->from_ext!=NULL)
			sprintf(str,"%s #%s",current_msg->from,current_msg->from_ext);
		else if(current_msg->from_net.type!=NET_NONE)
			sprintf(str,"%s (%s)",current_msg->from
				,net_addr(&current_msg->from_net));
		else
			strcpy(str,current_msg->from);
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
		if(current_msg->from_net.type!=NET_NONE
			&& (!(current_msg->hdr.attr&MSG_ANONYMOUS) || SYSOP))
			return(net_addr(&current_msg->from_net));
		return(nulstr);
	}
	if(!strcmp(sp,"MSG_SUBJECT") && current_msg!=NULL)
		return(current_msg->subj==NULL ? nulstr : current_msg->subj);
	if(!strcmp(sp,"MSG_DATE") && current_msg!=NULL)
		return(timestr((time_t *)&current_msg->hdr.when_written.time));
	if(!strcmp(sp,"MSG_TIMEZONE") && current_msg!=NULL)
		return(zonestr(current_msg->hdr.when_written.zone));
	if(!strcmp(sp,"MSG_ATTR") && current_msg!=NULL) {
		sprintf(str,"%s%s%s%s%s%s%s%s%s%s"
			,current_msg->hdr.attr&MSG_PRIVATE		? "Private  "   :nulstr
			,current_msg->hdr.attr&MSG_READ			? "Read  "      :nulstr
			,current_msg->hdr.attr&MSG_DELETE		? "Deleted  "   :nulstr
			,current_msg->hdr.attr&MSG_KILLREAD		? "Kill  "      :nulstr
			,current_msg->hdr.attr&MSG_ANONYMOUS	? "Anonymous  " :nulstr
			,current_msg->hdr.attr&MSG_LOCKED		? "Locked  "    :nulstr
			,current_msg->hdr.attr&MSG_PERMANENT	? "Permanent  " :nulstr
			,current_msg->hdr.attr&MSG_MODERATED	? "Moderated  " :nulstr
			,current_msg->hdr.attr&MSG_VALIDATED	? "Validated  " :nulstr
			,current_msg->hdr.attr&MSG_REPLIED		? "Replied  "	:nulstr
			);
		return(str);
	}
	if(!strcmp(sp,"MSG_ID") && current_msg!=NULL)
		return(current_msg->id==NULL ? nulstr : current_msg->id);
	if(!strcmp(sp,"MSG_REPLY_ID") && current_msg!=NULL)
		return(current_msg->reply_id==NULL ? nulstr : current_msg->reply_id);
	if(!strcmp(sp,"MSG_NUM") && current_msg!=NULL) {
		sprintf(str,"%lu",current_msg->hdr.number);
		return(str);
	}

	if(!strcmp(sp,"SMB_AREA")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			sprintf(str,"%s %s"
				,cfg.grp[cfg.sub[smb.subnum]->grp]->sname
				,cfg.sub[smb.subnum]->sname);
		return(str);
	}
	if(!strcmp(sp,"SMB_AREA_DESC")) {
		if(smb.subnum!=INVALID_SUB && smb.subnum<cfg.total_subs)
			sprintf(str,"%s %s"
				,cfg.grp[cfg.sub[smb.subnum]->grp]->lname
				,cfg.sub[smb.subnum]->lname);
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
			sprintf(str,"%u",getusrgrp(smb.subnum));
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
			sprintf(str,"%u",getusrsub(smb.subnum));
		return(str);
	}
	if(!strcmp(sp,"SMB_MSGS")) {
		sprintf(str,"%ld",smb.msgs);
		return(str);
	}
	if(!strcmp(sp,"SMB_CURMSG")) {
		sprintf(str,"%ld",smb.curmsg+1);
		return(str);
	}
	if(!strcmp(sp,"SMB_LAST_MSG")) {
		sprintf(str,"%lu",smb.status.last_msg);
		return(str);
	}
	if(!strcmp(sp,"SMB_MAX_MSGS")) {
		sprintf(str,"%lu",smb.status.max_msgs);
		return(str);
	}
	if(!strcmp(sp,"SMB_MAX_CRCS")) {
		sprintf(str,"%lu",smb.status.max_crcs);
		return(str);
	}
	if(!strcmp(sp,"SMB_MAX_AGE")) {
		sprintf(str,"%hu",smb.status.max_age);
		return(str);
	}
	if(!strcmp(sp,"SMB_TOTAL_MSGS")) {
		sprintf(str,"%lu",smb.status.total_msgs);
		return(str);
	}

	return(NULL);
}

