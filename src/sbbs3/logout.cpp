/* logout.cpp */

/* Synchronet user logout routines */

/* $Id: logout.cpp,v 1.33 2019/05/05 10:54:22 rswindell Exp $ */

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

/****************************************************************************/
/* Function that is called after a user hangs up or logs off				*/
/****************************************************************************/
void sbbs_t::logout()
{
	char	path[MAX_PATH+1];
	char	str[256];
	char 	tmp[512];
	int 	i,j;
	ushort	ttoday;
	node_t	node;
	struct	tm tm;

	now=time(NULL);
	if(localtime_r(&now,&tm)==NULL)
		errormsg(WHERE,ERR_CHK,"localtime",(ulong)now);

	if(!useron.number) {				 /* Not logged in, so do nothing */
		if(!online) {
			SAFEPRINTF2(str,"%s  T:%3u sec\r\n"
				,hhmmtostr(&cfg,&tm,tmp)
				,(uint)(now-answertime));
			logline("@-",str); 
		}
		return; 
	}
	lprintf(LOG_INFO, "logout initiated");
	SAFECOPY(lastuseron,useron.alias);
	if(!online && getnodedat(cfg.node_num, &node, /* lock: */true) == 0) {
		node.status = NODE_LOGOUT;
		putnodedat(cfg.node_num, &node);
	}

	if(useron.rest&FLAG('G')) {
		putuserrec(&cfg,useron.number,U_NAME,LEN_NAME,nulstr);		
		batdn_total=0; 
	}

	batch_create_list();

	if(sys_status&SS_USERON && thisnode.status!=NODE_QUIET && !(useron.rest&FLAG('Q')))
		for(i=1;i<=cfg.sys_nodes;i++)
			if(i!=cfg.node_num) {
				getnodedat(i,&node,0);
				if((node.status==NODE_INUSE || node.status==NODE_QUIET)
					&& !(node.misc&NODE_AOFF) && node.useron!=useron.number) {
					SAFEPRINTF2(str,text[NodeLoggedOff],cfg.node_num
						,thisnode.misc&NODE_ANON
						? text[UNKNOWN_USER] : useron.alias);
					putnmsg(&cfg,i,str); 
				}
			}

	if(!online) {		/* NOT re-login */
		if(cfg.sys_logout[0]) {		/* execute system logout event */
			lprintf(LOG_DEBUG, "executing logout event: %s", cfg.sys_logout);
			external(cmdstr(cfg.sys_logout,nulstr,nulstr,NULL),EX_OUTL|EX_OFFLINE);
		}
	}

	if(cfg.logout_mod[0]) {
		lprintf(LOG_DEBUG, "executing logout module: %s", cfg.logout_mod);
		exec_bin(cfg.logout_mod,&main_csi);
	}
	backout();
	SAFEPRINTF2(path,"%smsgs/%4.4u.msg",cfg.data_dir,useron.number);
	if(fexistcase(path) && !flength(path))		/* remove any 0 byte message files */
		remove(path);

	delfiles(cfg.temp_dir,ALLFILES);
	if(sys_status&SS_USERON) {	// Insures the useron actually when through logon()/getmsgptrs() first
		putmsgptrs();
	}
	if(!REALSYSOP)
		logofflist();
	useron.laston=(time32_t)now;

	ttoday=useron.ttoday-useron.textra; 		/* billable time used prev calls */
	if(ttoday>=cfg.level_timeperday[useron.level])
		i=0;
	else
		i=cfg.level_timeperday[useron.level]-ttoday;
	if(i>cfg.level_timepercall[useron.level])      /* i=amount of time without min */
		i=cfg.level_timepercall[useron.level];
	j=(int)(now-starttime)/60;			/* j=billable time online in min */
	if(i<0) i=0;
	if(j<0) j=0;

	if(useron.min && j>i) {
		j-=i;                               /* j=time to deduct from min */
		SAFEPRINTF(str,"Minute Adjustment: %d",-j);
		logline(">>",str);
		if(useron.min>(ulong)j)
			useron.min-=j;
		else
			useron.min=0L;
		putuserrec(&cfg,useron.number,U_MIN,10,ultoa(useron.min,str,10)); 
	}

	if(timeleft>0 && starttime-logontime>0) 			/* extra time */
		useron.textra+=(ushort)((starttime-logontime)/60);

	putuserrec(&cfg,useron.number,U_TEXTRA,5,ultoa(useron.textra,str,10));
	putuserrec(&cfg,useron.number,U_NS_TIME,8,ultoa((ulong)last_ns_time,str,16));

	logoutuserdat(&cfg, &useron, now, logontime);

	getusrsubs();
	getusrdirs();
	if(usrgrps>0)
		putuserrec(&cfg,useron.number,U_CURSUB,0,cfg.sub[usrsub[curgrp][cursub[curgrp]]]->code);
	if(usrlibs>0)
		putuserrec(&cfg,useron.number,U_CURDIR,0,cfg.dir[usrdir[curlib][curdir[curlib]]]->code);
	hhmmtostr(&cfg,&tm,str);
	SAFECAT(str,"  ");
	if(sys_status&SS_USERON)
		safe_snprintf(tmp,sizeof(tmp),"T:%3u   R:%3lu   P:%3lu   E:%3lu   F:%3lu   "
			"U:%3luk %lu   D:%3luk %lu"
			,(uint)(now-logontime)/60,posts_read,logon_posts
			,logon_emails,logon_fbacks,logon_ulb/1024UL,logon_uls
			,logon_dlb/1024UL,logon_dls);
	else
		SAFEPRINTF(tmp,"T:%3u sec",(uint)(now-answertime));
	SAFECAT(str,tmp);
	SAFECAT(str,"\r\n");
	logline("@-",str);
	sys_status&=~SS_USERON;
	answertime=now; // In case we're re-logging on

	lprintf(LOG_DEBUG, "logout completed");
}

/****************************************************************************/
/* Backout of transactions and statuses for this node 						*/
/****************************************************************************/
void sbbs_t::backout()
{
	char path[MAX_PATH+1],code[128],*buf;
	int i,file;
	long length,l;
	file_t f;

	SAFEPRINTF(path,"%sbackout.dab",cfg.node_dir);
	if(flength(path)<1L) {
		remove(path);
		return; 
	}
	if((file=nopen(path,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,path,O_RDONLY);
		return; 
	}
	length=(long)filelength(file);
	if((buf=(char *)malloc(length))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,path,length);
		return; 
	}
	if(read(file,buf,length)!=length) {
		close(file);
		free(buf);
		errormsg(WHERE,ERR_READ,path,length);
		return; 
	}
	close(file);
	for(l=0;l<length;l+=BO_LEN) {
		switch(buf[l]) {
			case BO_OPENFILE:	/* file left open */
				memcpy(code,buf+l+1,8);
				code[8]=0;
				for(i=0;i<cfg.total_dirs;i++)			/* search by code */
					if(!stricmp(cfg.dir[i]->code,code))
						break;
				if(i<cfg.total_dirs) {		/* found internal code */
					f.dir=i;
					memcpy(&f.datoffset,buf+l+9,4);
					closefile(&f); 
				}
				break;
			default:
				errormsg(WHERE,ERR_CHK,path,buf[l]); 
		} 
	}
	free(buf);
	remove(path);	/* always remove the backout file */
}

/****************************************************************************/
/* Detailed usage stats for each logon                                      */
/****************************************************************************/
void sbbs_t::logofflist()
{
    char str[256];
    int file;
    struct tm tm, tm_now;

	if(localtime_r(&now,&tm_now)==NULL)
		return;
	if(localtime_r(&logontime,&tm)==NULL)
		return;
	SAFEPRINTF4(str,"%slogs/%2.2d%2.2d%2.2d.lol",cfg.logs_dir,tm.tm_mon+1,tm.tm_mday
		,TM_YEAR(tm.tm_year));
	if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
		return; 
	}
	safe_snprintf(str,sizeof(str),"%-*.*s %-2d %-8.8s %2.2d:%2.2d %2.2d:%2.2d %3d%3ld%3ld%3ld%3ld"
		"%3ld%3ld\r\n",LEN_ALIAS,LEN_ALIAS,useron.alias,cfg.node_num,connection
		,tm.tm_hour,tm.tm_min,tm_now.tm_hour,tm_now.tm_min
		,(int)(now-logontime)/60,posts_read,logon_posts,logon_emails
		,logon_fbacks,logon_uls,logon_dls);
	write(file,str,strlen(str));
	close(file);
}
