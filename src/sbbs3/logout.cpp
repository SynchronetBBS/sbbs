/* logout.cpp */

/* Synchronet user logout routines */

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
/* Function that is called after a user hangs up or logs off				*/
/****************************************************************************/
void sbbs_t::logout()
{
	char	str[256];
	char 	tmp[512];
	int 	i,j;
	ushort	ttoday;
	node_t	node;
	struct	tm tm;

	now=time(NULL);
	if(localtime_r(&now,&tm)==NULL)
		return;

	if(!useron.number) {				 /* Not logged in, so do nothing */
		if(!online) {
			sprintf(str,"%s  T:%3u sec\r\n"
				,hhmmtostr(&cfg,&tm,tmp)
				,(uint)(now-answertime));
			logline("@-",str); }
		return; 
	}
	strcpy(lastuseron,useron.alias);	/* for use with WFC status display */

	if(useron.rest&FLAG('G')) {        /* Reset guest's msg scan cfg */
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
					sprintf(str,text[NodeLoggedOff],cfg.node_num
						,thisnode.misc&NODE_ANON
						? text[UNKNOWN_USER] : useron.alias);
					putnmsg(&cfg,i,str); } }

	if(!online) {		/* NOT re-login */

#if 0	/* too soon, handled in node_thread */
		getnodedat(cfg.node_num,&thisnode,1);
		thisnode.status=NODE_WFC;
		thisnode.misc&=~(NODE_INTR|NODE_MSGW|NODE_NMSG
			|NODE_UDAT|NODE_POFF|NODE_AOFF|NODE_EXT);
		putnodedat(cfg.node_num,&thisnode);
#endif

#if 0	/* beep? */ 
		if(sys_status&SS_SYSALERT) {
			mswait(500);
			offhook();
			CLS;
			lputs("\r\n\r\nAlerting Sysop...");
			while(!lkbrd(1)) {
				sbbs_beep(1000,200);
				nosound();
				mswait(200); }
			lkbrd(0); }
#endif
		sys_status&=~SS_SYSALERT;
		if(cfg.sys_logout[0])		/* execute system logout event */
			external(cmdstr(cfg.sys_logout,nulstr,nulstr,NULL),EX_OUTL|EX_OFFLINE);
		}

	if(cfg.logout_mod[0])
		exec_bin(cfg.logout_mod,&main_csi);
	backout();
	sprintf(str,"%smsgs/%4.4u.msg",cfg.data_dir,useron.number);
	if(!flength(str))		/* remove any 0 byte message files */
		remove(str);

	delfiles(cfg.temp_dir,ALLFILES);
	putmsgptrs();
	if(!REALSYSOP)
		logofflist();
	useron.laston=now;

	ttoday=useron.ttoday-useron.textra; 		/* billable time used prev calls */
	if(ttoday>=cfg.level_timeperday[useron.level])
		i=0;
	else
		i=cfg.level_timeperday[useron.level]-ttoday;
	if(i>cfg.level_timepercall[useron.level])      /* i=amount of time without min */
		i=cfg.level_timepercall[useron.level];
	j=(now-starttime)/60;			/* j=billable time online in min */
	if(i<0) i=0;
	if(j<0) j=0;

	if(useron.min && j>i) {
		j-=i;                               /* j=time to deduct from min */
		sprintf(str,"Minute Adjustment: %d",-j);
		logline(">>",str);
		if(useron.min>(ulong)j)
			useron.min-=j;
		else
			useron.min=0L;
		putuserrec(&cfg,useron.number,U_MIN,10,ultoa(useron.min,str,10)); }

	if(timeleft>0 && starttime-logontime>0) 			/* extra time */
		useron.textra+=(ushort)((starttime-logontime)/60);

	putuserrec(&cfg,useron.number,U_TEXTRA,5,ultoa(useron.textra,str,10));
	putuserrec(&cfg,useron.number,U_NS_TIME,8,ultoa(last_ns_time,str,16));

	logoutuserdat(&cfg, &useron, now, logontime);

	getusrsubs();
	getusrdirs();
	if(usrgrps>0)
		putuserrec(&cfg,useron.number,U_CURSUB,0,cfg.sub[usrsub[curgrp][cursub[curgrp]]]->code);
	if(usrlibs>0)
		putuserrec(&cfg,useron.number,U_CURDIR,0,cfg.dir[usrdir[curlib][curdir[curlib]]]->code);
	hhmmtostr(&cfg,&tm,str);
	strcat(str,"  ");
	if(sys_status&SS_USERON)
		sprintf(tmp,"T:%3u   R:%3lu   P:%3lu   E:%3lu   F:%3lu   "
			"U:%3luk %lu   D:%3luk %lu"
			,(uint)(now-logontime)/60,posts_read,logon_posts
			,logon_emails,logon_fbacks,logon_ulb/1024UL,logon_uls
			,logon_dlb/1024UL,logon_dls);
	else
		sprintf(tmp,"T:%3u sec",(uint)(now-answertime));
	strcat(str,tmp);
	strcat(str,"\r\n");
	logline("@-",str);
	sys_status&=~SS_USERON;
	answertime=now; // Incase we're relogging on
}

/****************************************************************************/
/* Backout of transactions and statuses for this node 						*/
/****************************************************************************/
void sbbs_t::backout()
{
	char str[256],code[128],*buf;
	int i,file;
	long length,l;
	file_t f;

	sprintf(str,"%sbackout.dab",cfg.node_dir);
	if(flength(str)<1L) {
		remove(str);
		return; }
	if((file=nopen(str,O_RDONLY))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_RDONLY);
		return; }
	length=filelength(file);
	if((buf=(char *)MALLOC(length))==NULL) {
		close(file);
		errormsg(WHERE,ERR_ALLOC,str,length);
		return; }
	if(read(file,buf,length)!=length) {
		close(file);
		FREE(buf);
		errormsg(WHERE,ERR_READ,str,length);
		return; }
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
					closefile(&f); }
				break;
			default:
				errormsg(WHERE,ERR_CHK,str,buf[l]); } }
	FREE(buf);
	remove(str);	/* always remove the backout file */
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
	sprintf(str,"%slogs/%2.2d%2.2d%2.2d.LOL",cfg.logs_dir,tm.tm_mon+1,tm.tm_mday
		,TM_YEAR(tm.tm_year));
	if((file=nopen(str,O_WRONLY|O_CREAT|O_APPEND))==-1) {
		errormsg(WHERE,ERR_OPEN,str,O_WRONLY|O_CREAT|O_APPEND);
		return; }
	sprintf(str,"%-*.*s %-2d %-8.8s %2.2d:%2.2d %2.2d:%2.2d %3d%3ld%3ld%3ld%3ld"
		"%3ld%3ld\r\n",LEN_ALIAS,LEN_ALIAS,useron.alias,cfg.node_num,connection
		,tm.tm_hour,tm.tm_min,tm_now.tm_hour,tm_now.tm_min
		,(int)(now-logontime)/60,posts_read,logon_posts,logon_emails
		,logon_fbacks,logon_uls,logon_dls);
	write(file,str,strlen(str));
	close(file);
}
