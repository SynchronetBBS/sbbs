/* umonitor.c */

/* Synchronet for *nix node activity monitor */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "conwrap.h"	/* this has to go BEFORE curses.h so getkey() can be macroed around */
#include <curses.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#ifdef __QNX__
#include <string.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include "genwrap.h"
#include "uifc.h"
#include "sbbsdefs.h"
#include "genwrap.h"	/* stricmp */
#include "dirwrap.h"	/* lock/unlock/sopen */
#include "filewrap.h"	/* lock/unlock/sopen */
#include "sbbs_ini.h"	/* INI parsing */
#include "scfglib.h"	/* SCFG files */
#include "ars_defs.h"	/* needed for SCFG files */
#include "userdat.h"	/* getnodedat() */
#include "spyon.h"
#include "chat.h"

#define CTRL(x) (x&037)

/********************/
/* Global Variables */
/********************/
uifcapi_t uifc; /* User Interface (UIFC) Library API */
const char *YesStr="Yes";
const char *NoStr="No";

int lprintf(char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];
	int	len;

	va_start(argptr,fmt);
	len=vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
	va_end(argptr);
	uifc.msg(sbuf);
	return(len);
}

void bail(int code)
{
    if(code) {
        puts("\nHit a key...");
        getch(); 
	}
    uifc.bail();

    exit(code);
}

void allocfail(uint size)
{
    printf("\7Error allocating %u bytes of memory.\n",size);
    bail(1);
}

void freeopt(char** opt)
{
	int i;

	for(i=0;i<(MAX_OPTS+1);i++)
		free(opt[i]);

	free(opt);
}

void node_toggles(scfg_t *cfg,int nodenum)  {
	int nodefile;
	char**	opt;
	int		i,j;
	node_t	node;
	int		save=0;

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	uifc.helpbuf=	"`Node Toggles:`\n"
					"\nToDo: Add help (Mention that changes take effect immediately)";
	while(save==0) {
		if(getnodedat(cfg,nodenum,&node,&nodefile)) {
			uifc.msg("Error reading node data!");
			break;
		}
		j=0;
		sprintf(opt[j++],"%-30s%3s","Locked for SysOps only",node.misc&NODE_LOCK ? YesStr : NoStr);
		sprintf(opt[j++],"%-30s%3s","Interrupt (Hangup)",node.misc&NODE_INTR ? YesStr : NoStr);
		sprintf(opt[j++],"%-30s%3s","Re-run on logoff",node.misc&NODE_RRUN ? YesStr : NoStr);
		sprintf(opt[j++],"%-30s%3s","Down node after logoff"
			,(node.misc&NODE_DOWN || (node.status==NODE_OFFLINE)) ? YesStr : NoStr);
		opt[j][0]=0;

		switch(uifc.list(WIN_MID,0,0,0,&i,0,"Node Toggles",opt)) {
			case 0:	/* Locked */
				node.misc ^= NODE_LOCK;
				break;

			case 1:	/* Interrupt */
				node.misc ^= NODE_INTR;
				break;

			case 2:	/* Re-run */
				node.misc ^= NODE_RRUN;
				break;

			case 3:	/* Down */
				if(node.status != NODE_WFC && node.status != NODE_OFFLINE)
					node.misc ^= NODE_DOWN;
				else {
					if(node.status!=NODE_OFFLINE)
						node.status=NODE_OFFLINE;
					else
						node.status=NODE_WFC;
				}
				break;

			case -1:
				save=1;
				break;
				
			default:
				uifc.msg("Option not implemented");
				continue;
		}
		putnodedat(cfg,nodenum,&node,nodefile);
	}
	freeopt(opt);
}

int dospy(int nodenum, bbs_startup_t *bbs_startup)  {
	char str[80],str2[80];
	int i;

	if(bbs_startup->temp_dir[0])
    	snprintf(str,sizeof(str),"%slocalspy%d.sock", bbs_startup->temp_dir, nodenum);
	else
		snprintf(str,sizeof(str),"%slocalspy%d.sock", bbs_startup->ctrl_dir, nodenum);
	endwin();
	i=spyon(str);
	refresh();
	switch(i) {
		case SPY_NOSOCKET:
			uifc.msg("Could not create socket");
			return(-1);
					
		case SPY_NOCONNECT:
			sprintf(str2,"Failed to connect to %s",str);
			uifc.msg(str2);
			return(-1);

		case SPY_SELECTFAILED:
			uifc.msg("select() failed, connection terminated.");
			return(-1);

		case SPY_SOCKETLOST:
			uifc.msg("Spy socket lost");
			return(-1);
							
		case SPY_STDINLOST:
			uifc.msg("STDIN has gone away... you probably can't close this window.  :-)");
			return(-1);
							
		case SPY_CLOSED:
			break;
			
		default:
			sprintf(str,"Unknown return code %d",i);
			uifc.msg(str);
			return(-1);
	}
	return(0);
}

int sendmessage(scfg_t *cfg, int nodenum,node_t *node)  {
	char str[80],str2[80];

	uifc.input(WIN_MID,0,0,"Telegram",str2,58,K_WRAP|K_MSG);
	sprintf(str,"\1n\1y\1hMessage From Sysop:\1w %s\r\n",str2);
	if(getnodedat(cfg,nodenum,node,NULL))
		return(-1);
	if(node->useron==0)
		return(-1);
	putsmsg(cfg, node->useron, str);
	return(0);
}

int clearerrors(scfg_t *cfg, int nodenum, node_t *node) {
	int nodefile;
	if(getnodedat(cfg,nodenum,node,&nodefile)) {
		uifc.msg("getnodedat() failed! (Nothing done)");
		return(-1);
	}
	node->errors=0;
	putnodedat(cfg,nodenum,node,nodefile);
	return(0);
}

/* Assumes a 12 char outstr */
char *getsizestr(char *outstr, long size, BOOL bytes) {
	if(bytes) {
		if(size < 1000) {	/* Bytes */
			snprintf(outstr,12,"%ld bytes",size);
			return(outstr);			
		}
		if(size<10000) {	/* Bytes with comma */
			snprintf(outstr,12,"%ld,%03ld bytes",(size/1000),(size%1000));
			return(outstr);			
		}
		size = size/1024;
	}
	if(size<1000) {	/* KB */
		snprintf(outstr,12,"%ld KB",size);
		return(outstr);			
	}
	if(size<999999) { /* KB With comma */
		snprintf(outstr,12,"%ld,%03ld KB",(size/1000),(size%1000));
		return(outstr);			
	}
	size = size/1024;
	if(size<1000) {	/* MB */
		snprintf(outstr,12,"%ld MB",size);
		return(outstr);			
	}
	if(size<999999) { /* MB With comma */
		snprintf(outstr,12,"%ld,%03ld MB",(size/1000),(size%1000));
		return(outstr);			
	}
	size = size/1024;
	if(size<1000) {	/* GB */
		snprintf(outstr,12,"%ld GB",size);
		return(outstr);			
	}
	if(size<999999) { /* GB With comma */
		snprintf(outstr,12,"%ld,%03ld GB",(size/1000),(size%1000));
		return(outstr);			
	}
	size = size/1024;
	if(size<1000) {	/* TB (Yeah, right) */
		snprintf(outstr,12,"%ld TB",size);
		return(outstr);			
	}
	sprintf(outstr,"Plenty");
	return(outstr);			
}

/* Assumes a 12 char outstr */
char *getnumstr(char *outstr, ulong size) {
	if(size < 1000) {
		snprintf(outstr,12,"%ld",size);
		return(outstr);			
	}
	if(size<1000000) {
		snprintf(outstr,12,"%ld,%03ld",(size/1000),(size%1000));
		return(outstr);			
	}
	if(size<1000000000) {
		snprintf(outstr,12,"%ld,%03ld,%03ld",(size/1000000),((size/1000)%1000),(size%1000));
		return(outstr);			
	}
	size=size/1000000;
	if(size<1000000) {
		snprintf(outstr,12,"%ld,%03ld M",(size/1000),(size%1000));
		return(outstr);			
	}
	if(size<10000000) {
		snprintf(outstr,12,"%ld,%03ld,%03ld M",(size/1000000),((size/1000)%1000),(size%1000));
		return(outstr);			
	}
	sprintf(outstr,"Plenty");
	return(outstr);			
}

int drawstats(scfg_t *cfg, int nodenum, node_t *node, int *curp, int *barp) {
	stats_t	sstats;
	stats_t	nstats;
	char	statbuf[6*78];		/* Buffer to hold the stats for passing to uifc.showbuf() */
	char	str[4][4][12];
	char	usrname[128];
	ulong	free;
	uint	i,l,m;
	time_t	t;
	int		shownode=1;

	if(getnodedat(cfg,nodenum,node,NULL)) {
		shownode=0;
	}
	else {
		getstats(cfg, nodenum, &nstats);
	}
	username(cfg,node->useron,usrname);

	getstats(cfg, 0, &sstats);
	t=time(NULL);
	strftime(str[0][0],12,"%b %e",localtime(&t));
	free=getfreediskspace(cfg->temp_dir,1024);
	if(free<1000) {
		free=getfreediskspace(cfg->temp_dir,0);
		getsizestr(str[0][1],free,TRUE);
	}
	else
		getsizestr(str[0][1],free,FALSE);
	if(shownode) {
		snprintf(str[1][0],12,"%s/%s",getnumstr(str[3][2],nstats.ltoday),getnumstr(str[3][3],sstats.ltoday));
		getnumstr(str[1][1],sstats.logons);
		snprintf(str[1][2],12,"%s/%s",getnumstr(str[3][2],nstats.ttoday),getnumstr(str[3][3],sstats.ttoday));
		getnumstr(str[1][3],sstats.timeon);
		snprintf(str[2][0],12,"%s/%s",getnumstr(str[3][2],sstats.etoday),getnumstr(str[3][3],getmail(cfg,0,0)));
		l=m=0;
		for(i=0;i<cfg->total_subs;i++)
			l+=getposts(cfg,i); 			/* l=total posts */
		for(i=0;i<cfg->total_dirs;i++)
			m+=getfiles(cfg,i); 			/* m=total files */
		snprintf(str[2][1],12,"%s/%s",getnumstr(str[3][2],sstats.ptoday),getnumstr(str[3][3],l));
		snprintf(str[2][2],12,"%s/%s",getnumstr(str[3][2],sstats.ftoday),getnumstr(str[3][3],getmail(cfg,1,0)));
		snprintf(str[2][3],12,"%s/%s",getnumstr(str[3][2],sstats.nusers),getnumstr(str[3][3],total_users(cfg)));
		getsizestr(str[3][0],sstats.ulb,TRUE);
		snprintf(str[3][1],12,"%s/%s",getnumstr(str[3][2],sstats.uls),getnumstr(str[3][3],m));
		getsizestr(str[3][2],sstats.dlb,TRUE);
		getnumstr(str[3][3],sstats.dls);
	}
	else {
		snprintf(str[1][0],12,"%s",getnumstr(str[3][3],sstats.ltoday));
		getnumstr(str[1][1],sstats.logons);
		snprintf(str[1][2],12,"%s",getnumstr(str[3][3],sstats.ttoday));
		getnumstr(str[1][3],sstats.timeon);
		snprintf(str[2][0],12,"%s/%s",getnumstr(str[3][2],sstats.etoday),getnumstr(str[3][3],getmail(cfg,0,0)));
		l=m=0;
		for(i=0;i<cfg->total_subs;i++)
			l+=getposts(cfg,i); 			/* l=total posts */
		for(i=0;i<cfg->total_dirs;i++)
			m+=getfiles(cfg,i); 			/* m=total files */
		snprintf(str[2][1],12,"%s/%s",getnumstr(str[3][2],sstats.ptoday),getnumstr(str[3][3],l));
		snprintf(str[2][2],12,"%s/%s",getnumstr(str[3][2],sstats.ftoday),getnumstr(str[3][3],getmail(cfg,1,0)));
		snprintf(str[2][3],12,"%s/%s",getnumstr(str[3][2],sstats.nusers),getnumstr(str[3][3],total_users(cfg)));
		getsizestr(str[3][0],sstats.ulb,TRUE);
		snprintf(str[3][1],12,"%s/%s",getnumstr(str[3][2],sstats.uls),getnumstr(str[3][3],m));
		getsizestr(str[3][2],sstats.dlb,TRUE);
		getnumstr(str[3][3],sstats.dls);
	}
	snprintf(statbuf,sizeof(statbuf),"`Node #`: %-3d %6s  `Space`: %s"
			"\n`Logons`: %-11s `Total`: %-11s `Timeon`: %-11s `Total`: %-11s"
			"\n`Emails`: %-11s `Posts`: %-11s `Fbacks`: %-11s `Users`: %-11s"
			"\n`Uloads`: %-11s `Files`: %-11s `Dloads`: %-11s `Files`: %-11s",
			nodenum,str[0][0],str[0][1],
			str[1][0],str[1][1],str[1][2],str[1][3],
			str[2][0],str[2][1],str[2][2],str[2][3],
			str[3][0],str[3][1],str[3][2],str[3][3]);

	uifc.showbuf(WIN_HLP|WIN_L2R|WIN_DYN|WIN_PACK,1,1,80,6,"Statistics",statbuf,curp,barp);
/* Node 5 :	Mar 11  Space: 162,024k
   Logons: 23/103      Total: 62,610      Timeon: 322/2430    Total: 5,321,900   
   Emails: 4/265       Posts: 4/12811     Fbacks: 2/17	       Users: 1/592
   Uloads: 324k        Files: 1/2195      Dloads: 9,308k      Files: 52 */
	return(0);
}

int view_log(char *filename, char *title)
{
	char str[1024];
	int buffile;
	int j;
	char *buf;

	if(fexist(filename)) {
		if((buffile=sopen(filename,O_RDONLY,SH_DENYWR))>=0) {
			j=filelength(buffile);
			if((buf=(char *)MALLOC(j+1))!=NULL) {
				read(buffile,buf,j);
				close(buffile);
				*(buf+j)=0;
				uifc.showbuf(WIN_MID,0,0,76,uifc.scrn_len-2,title,buf,NULL,NULL);
				free(buf);
				return(0);
			}
			close(buffile);
			uifc.msg("Error allocating memory for the error log");
			return(3);
		}
		sprintf(str,"Error opening %s",title);
		uifc.msg(str);
		return(1);
	}
	sprintf(str,"%s does not exists",title);
	uifc.msg(str);
	return(2);
}

int view_logs(scfg_t *cfg)
{
	char**	opt;
	int		i;
	char	str[1024];
	struct tm tm;
	struct tm tm_yest;
	time_t	now;

	now=time(NULL);
	localtime_r(&now,&tm);
	now -= 60*60*24;
	localtime_r(&now,&tm_yest);
	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	strcpy(opt[i++],"Todays callers");
	strcpy(opt[i++],"Yesterdays callers");
	strcpy(opt[i++],"Error log");
	strcpy(opt[i++],"Todays log");
	strcpy(opt[i++],"Yesterdays log");
	strcpy(opt[i++],"Spam log");
	strcpy(opt[i++],"SBBSEcho log");
	strcpy(opt[i++],"Guru log");
	strcpy(opt[i++],"Hack log");
	opt[i][0]=0;
	i=0;
	uifc.helpbuf=	"`View Logs:`\n"
					"\nToDo: Add help";

	while(1) {
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"View Logs",opt))  {
			case -1:
				freeopt(opt);
				return(0);
			case 0:
				sprintf(str,"%slogs/%2.2d%2.2d%2.2d.lol",cfg->logs_dir,tm.tm_mon+1,tm.tm_mday
					,TM_YEAR(tm.tm_year));
				view_log(str,"Todays Callers");
				break;
			case 1:
				sprintf(str,"%slogs/%2.2d%2.2d%2.2d.lol",cfg->logs_dir,tm_yest.tm_mon+1
					,tm_yest.tm_mday,TM_YEAR(tm_yest.tm_year));
				view_log(str,"Yesterdays Callers");
				break;
			case 2:
				sprintf(str,"%s/error.log",cfg->logs_dir);
				view_log(str,"Error Log");
				break;
			case 3:
				sprintf(str,"%slogs/%2.2d%2.2d%2.2d.log",cfg->logs_dir,tm.tm_mon+1,tm.tm_mday
					,TM_YEAR(tm.tm_year));
				view_log(str,"Todays Log");
				break;
			case 4:
				sprintf(str,"%slogs/%2.2d%2.2d%2.2d.log",cfg->logs_dir,tm_yest.tm_mon+1
					,tm_yest.tm_mday,TM_YEAR(tm_yest.tm_year));
				view_log(str,"Yesterdays Log");
				break;
			case 5:
				sprintf(str,"%sspam.log",cfg->logs_dir);
				view_log(str,"SPAM Log");
				break;
			case 6:
				sprintf(str,"%ssbbsecho.log",cfg->logs_dir);
				view_log(str,"SBBSEcho Log");
				break;
			case 7:
				sprintf(str,"%sguru.log",cfg->logs_dir);
				view_log(str,"Guru Log");
				break;
			case 8:
				sprintf(str,"%shack.log",cfg->logs_dir);
				view_log(str,"Hack Log");
				break;
		}
	}
}

int do_cmd(char *cmd)
{
	int i;
	
	endwin();
	i=system(cmd);
	refresh();
	return(i);
}

int qwk_callouts(scfg_t *cfg)
{
	char**	opt;
	int		i,j;
	char	str[1024];

	if(cfg->total_qhubs<1) {
		uifc.msg("No QWK hubs configured!");
		return(1);
	}

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);


	uifc.helpbuf=	"`QWK Callouts:`\n"
					"\nToDo: Add help";

	j=0;
	while(1) {
		for(i=0;i<cfg->total_qhubs;i++) {
			strcpy(opt[i],cfg->qhub[i]->id);
			sprintf(str,"%sqnet/%s.now",cfg->data_dir,cfg->qhub[i]->id);
			if(fexist(str))
				strcat(opt[i]," (pending)");
		}
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"QWK Callouts",opt))  {
			case -1:
				freeopt(opt);
				return(0);
				break;
			default:
				sprintf(str,"%sqnet/%s.now",cfg->data_dir,cfg->qhub[j]->id);
				ftouch(str);
		}
	}
	return(0);
}

int run_events(scfg_t *cfg)
{
	char**	opt;
	int		i,j;
	char	str[1024];

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	if(cfg->total_events<1) {
		uifc.msg("No events configured!");
		return(1);
	}
	j=0;

	uifc.helpbuf=	"`Run Events:`\n"
					"\nToDo: Add help";

	while(1) {
		for(i=0;i<cfg->total_events;i++) {
			strcpy(opt[i],cfg->event[i]->code);
			sprintf(str,"%s%s.now",cfg->data_dir,cfg->event[i]->code);
			if(fexist(str))
				strcat(opt[i]," (pending)");
		}
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"Run Events",opt))  {
			case -1:
				freeopt(opt);
				return(0);
				break;
			default:
				sprintf(str,"%s%s.now",cfg->data_dir,cfg->event[j]->code);
				ftouch(str);
		}
	}
	return(0);
}

int recycle_servers(scfg_t *cfg)
{
	char str[1024];
	char **opt;
	int i=0;

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	strcpy(opt[i++],"FTP server");
	strcpy(opt[i++],"Mail server");
	strcpy(opt[i++],"Services");
	strcpy(opt[i++],"Telnet server");
	strcpy(opt[i++],"Web server");
	opt[i][0]=0;

	uifc.helpbuf=	"`Recycle Servers:`\n"
					"\nToDo: Add help";

	i=0;
	while(1) {
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Recycle Servers",opt))  {
			case -1:
				freeopt(opt);
				return(0);
				break;
			case 0:
				sprintf(str,"%sftpsrvr.rec",cfg->ctrl_dir);
				ftouch(str);
				break;
			case 1:
				sprintf(str,"%smailsrvr.rec",cfg->ctrl_dir);
				ftouch(str);
				break;
			case 2:
				sprintf(str,"%sservices.rec",cfg->ctrl_dir);
				ftouch(str);
				break;
			case 3:
				sprintf(str,"%stelnet.rec",cfg->ctrl_dir);
				ftouch(str);
				break;
			case 4:
				sprintf(str,"%swebsrvr.rec",cfg->ctrl_dir);
				ftouch(str);
				break;
		}
	}
	return(0);
}

char *geteditor(char *edit)
{
	if(getenv("EDITOR")==NULL && (getenv("VISUAL")==NULL || getenv("DISPLAY")==NULL))
		strcpy(edit,"vi");
	else {
		if(getenv("DISPLAY")!=NULL && getenv("VISUAL")!=NULL)
			strcpy(edit,getenv("VISUAL"));
		else
			strcpy(edit,getenv("EDITOR"));
	}
	return(edit);
}


int edit_cfg(scfg_t *cfg)
{
	char**	opt;
	int		i;
	char	cmd[1024];
	char	editcmd[1024];

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	strcpy(opt[i++],"alias.cfg");
	strcpy(opt[i++],"attr.cfg");
	strcpy(opt[i++],"dns_blacklist.cfg");
	strcpy(opt[i++],"dnsbl_exempt.cfg");
	strcpy(opt[i++],"domains.cfg");
	strcpy(opt[i++],"mailproc.cfg");
	strcpy(opt[i++],"mime_types.cfg");
	strcpy(opt[i++],"relay.cfg");
	strcpy(opt[i++],"sbbsecho.cfg");
	strcpy(opt[i++],"services.cfg");
	strcpy(opt[i++],"ftpalias.cfg");
	strcpy(opt[i++],"sockopts.cfg");
	strcpy(opt[i++],"spambait.cfg");
	strcpy(opt[i++],"spamblock.cfg");
	strcpy(opt[i++],"twitlist.cfg");
	opt[i][0]=0;
	i=0;
	while(1) {
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"System Options",opt))  {
			case -1:
				freeopt(opt);
				return(0);
				break;
			default:
				sprintf(cmd,"%s %s%s",geteditor(editcmd),cfg->ctrl_dir,opt[i]);
				do_cmd(cmd);
				break;
		}
	}
	return(0);
}

int edit_can(scfg_t *cfg)
{
	char**	opt;
	int		i;
	char	cmd[1024];
	char	editcmd[1024];

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	strcpy(opt[i++],"email.can");
	strcpy(opt[i++],"file.can");
	strcpy(opt[i++],"host.can");
	strcpy(opt[i++],"ip.can");
	strcpy(opt[i++],"ip-silent.can");
	strcpy(opt[i++],"name.can");
	strcpy(opt[i++],"phone.can");
	strcpy(opt[i++],"rlogin.can");
	strcpy(opt[i++],"subject.can");
	opt[i][0]=0;
	i=0;
	while(1) {
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"System Options",opt))  {
			case -1:
				freeopt(opt);
				return(0);
				break;
			default:
				sprintf(cmd,"%s %s%s",geteditor(editcmd),cfg->text_dir,opt[i]);
				do_cmd(cmd);
				break;
		}
	}
	return(0);
}

int main(int argc, char** argv)  {
	char**	opt;
	char**	mopt;
	int		main_dflt=0;
	int		main_bar=0;
	char	revision[16];
	char	str[256],ctrl_dir[41],*p;
	char	title[256];
	int		i,j;
	node_t	node;
	int		nodefile;
	box_t	boxch;
	scfg_t	cfg;
	int		done;
	/******************/
	/* Ini file stuff */
	/******************/
	char	ini_file[MAX_PATH+1];
	FILE*				fp;
	bbs_startup_t		bbs_startup;

	sscanf("$Revision$", "%*s %s", revision);

    printf("\nSynchronet UNIX Monitor %s-%s  Copyright 2003 "
        "Rob Swindell\n",revision,PLATFORM_DESC);

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		printf("\7\nSBBSCTRL environment variable is not set.\n");
		printf("This environment variable must be set to your CTRL directory.");
		printf("\nExample: SET SBBSCTRL=/sbbs/ctrl\n");
		exit(1); }

	sprintf(ctrl_dir,"%.40s",p);
	if(ctrl_dir[strlen(ctrl_dir)-1]!='\\'
		&& ctrl_dir[strlen(ctrl_dir)-1]!='/')
		strcat(ctrl_dir,"/");

	gethostname(str,sizeof(str)-1);

	sbbs_get_ini_fname(ini_file, ctrl_dir, str);

	/* Initialize BBS startup structure */
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);
    strcpy(bbs_startup.ctrl_dir,ctrl_dir);

	/* Read .ini file here */
	if(ini_file[0]!=0 && (fp=fopen(ini_file,"r"))!=NULL) {
		printf("Reading %s\n",ini_file);
	}
	/* We call this function to set defaults, even if there's no .ini file */
	sbbs_read_ini(fp, 
		NULL, &bbs_startup, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

	/* close .ini file here */
	if(fp!=NULL)
		fclose(fp);

	chdir(bbs_startup.ctrl_dir);
	
	/* Read .cfg files here */
    memset(&cfg,0,sizeof(cfg));
	cfg.size=sizeof(cfg);
	SAFECOPY(cfg.ctrl_dir,bbs_startup.ctrl_dir);
	if(!load_cfg(&cfg, NULL, TRUE, str)) {
		printf("ERROR! %s\n",str);
		exit(1);
	}
	prep_dir(cfg.data_dir, cfg.temp_dir, sizeof(cfg.temp_dir));

    memset(&uifc,0,sizeof(uifc));

	uifc.esc_delay=500;

	boxch.ls=ACS_VLINE; 
	boxch.rs=ACS_VLINE;
	boxch.ts=ACS_HLINE;
	boxch.bs=ACS_HLINE;
	boxch.tl=ACS_ULCORNER;
	boxch.tr=ACS_URCORNER;
	boxch.bl=ACS_LLCORNER;
	boxch.br=ACS_LRCORNER;
	for(i=1;i<argc;i++) {
        if(argv[i][0]=='-'
            )
            switch(toupper(argv[i][1])) {
                case 'C':
        			uifc.mode|=UIFC_COLOR;
                    break;
                case 'L':
                    uifc.scrn_len=atoi(argv[i]+2);
                    break;
                case 'E':
                    uifc.esc_delay=atoi(argv[i]+2);
                    break;
				case 'I':
					/* Set up ex-ascii codes */
					boxch.ls=186; 
					boxch.rs=186;
					boxch.ts=205;
					boxch.bs=205;
					boxch.tl=201;
					boxch.tr=187;
					boxch.bl=200;
					boxch.br=188;
					uifc.mode|=UIFC_IBM;
					break;
                default:
                    printf("\nusage: %s [ctrl_dir] [options]"
                        "\n\noptions:\n\n"
                        "-c  =  force color mode\n"
                        "-e# =  set escape delay to #msec\n"
						"-i  =  force IBM charset\n"
                        "-l# =  set screen lines to #\n"
						,argv[0]
                        );
        			exit(0);
           }
    }

	signal(SIGPIPE, SIG_IGN);   

	uifc.size=sizeof(uifc);
#ifdef USE_CURSES
	i=uifcinic(&uifc);  /* curses */
#else
	i=uifcini32(&uifc);  /* curses */
#endif
	if(i!=0) {
		printf("uifc library init returned error %d\n",i);
		exit(1);
	}

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	if((mopt=(char **)MALLOC(sizeof(char *)*MAX_OPTS))==NULL)
		allocfail(sizeof(char *)*MAX_OPTS);
	for(i=0;i<MAX_OPTS;i++)
		if((mopt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	sprintf(title,"Synchronet UNIX Monitor %s-%s",revision,PLATFORM_DESC);
	if(uifc.scrn(title)) {
		printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		bail(1);
	}

	while(1) {
		strcpy(mopt[0],"System Options");
		for(i=1;i<=cfg.sys_nodes;i++) {
			if((j=getnodedat(&cfg,i,&node,NULL)))
				sprintf(mopt[i],"Error reading node data (%d)!",j);
			else
				sprintf(mopt[i],"%3d: %s",i,nodestatus(&cfg,&node,str,71));
		}
		mopt[i][0]=0;

		uifc.helpbuf=	"`Synchronet Monitor:`\n"
						"\nCTRL-E displays the error log"
						"\nF12 spys on the currently selected node"
						"\nF11 send message to the currently selected node"
						"\nF10 Chats with the user on the currently selected node"
						"\nDEL Clear errors on currently selected node"
						"\nCTRL-L Lock node toggle"
						"\nCTRL-R Rerun node"
						"\nCTRL-D Down node toggle"
						"\nCTRL-I Interrupt node"
						"\nToDo: Add more help. (Explain what you're looking at)";

		drawstats(&cfg, main_dflt, &node, &main_dflt, &main_bar);

		j=uifc.list(WIN_L2R|WIN_ESC|WIN_ACT|WIN_DYN,0,5,70,&main_dflt,&main_bar
			,title,mopt);

		if(j == -2)
			continue;

		if(j==-7) {	/* CTRL-E */
			sprintf(str,"%s/error.log",cfg.data_dir);
			view_log(str,"Error Log");
			continue;
		}
		
		if(j==0) {
			/* System Options */
			i=0;
			strcpy(opt[i++],"Run SCFG");
			strcpy(opt[i++],"View logs");
			strcpy(opt[i++],"Force QWK Net callout");
			strcpy(opt[i++],"Run event");
			strcpy(opt[i++],"Recycle servers");
			strcpy(opt[i++],"Edit CFG files");
			strcpy(opt[i++],"Edit trashcan files");
			opt[i][0]=0;
			uifc.helpbuf=	"`System Options:`\n"
							"\nToDo: Add help";

			done=0;
			i=0;
			while(!done) {
				switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"System Options",opt))  {
					case -1:
						done=1;
						break;
					case 0:
						sprintf(str,"%sscfg ",cfg.exec_dir);
						for(j=1; j<argc; j++) {
							strcat(str,"'");
							strcat(str,argv[j]);
							strcat(str,"' ");
						}
						do_cmd(str);
						break;
					case 1:
						view_logs(&cfg);
						break;
					case 2:
						qwk_callouts(&cfg);
						break;
					case 3:
						run_events(&cfg);
						break;
					case 4:
						recycle_servers(&cfg);
						break;
					case 5:
						edit_cfg(&cfg);
						break;
					case 6:
						edit_can(&cfg);
						break;
				}
			}
			continue;
		}

		if(j==-1) {
			i=0;
			strcpy(opt[0],YesStr);
			strcpy(opt[1],NoStr);
			opt[2][0]=0;
			uifc.helpbuf=	"`Exit Synchronet UNIX Monitor:`\n"
							"\n"
							"\nIf you want to exit the Synchronet UNIX monitor utility,"
							"\nselect `Yes`. Otherwise, select `No` or hit ~ ESC ~.";
			i=uifc.list(WIN_MID,0,0,0,&i,0,"Exit Synchronet Monitor",opt);
			if(!i)
				bail(0);
			continue;
		}

		/* Everything after this point is invalid for the System Options */
		if(main_dflt==0)
			continue;

		if(j==-2-KEY_DC) {	/* Clear errors */
			clearerrors(&cfg, main_dflt,&node);
			continue;
		}

		if(j==-2-KEY_F(10)) {	/* Chat */
			if(getnodedat(&cfg,main_dflt,&node,NULL)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			if((node.status==NODE_INUSE) && node.useron)
				chat(&cfg,main_dflt,&node,&boxch,uifc.timedisplay);
			continue;
		}

		if(j==-2-KEY_F(11)) {	/* Send message */
			sendmessage(&cfg, main_dflt,&node);
			continue;
		}
		
		if(j==-2-KEY_F(12)) {	/* Spy */
			dospy(main_dflt,&bbs_startup);
			continue;
		}
		
		if(j==-2-CTRL('l')) {	/* Lock node */
			if(getnodedat(&cfg,main_dflt,&node,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			node.misc^=NODE_LOCK;
			putnodedat(&cfg,main_dflt,&node,nodefile);
			continue;
		}
		
		if(j==-2-CTRL('r')) {	/* Rerun node */
			if(getnodedat(&cfg,main_dflt,&node,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			node.misc^=NODE_RRUN;
			putnodedat(&cfg,main_dflt,&node,nodefile);
			continue;
		}

		if(j==-2-CTRL('d')) {	/* Down node */
			if(getnodedat(&cfg,main_dflt,&node,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			if(node.status != NODE_WFC && node.status != NODE_OFFLINE)
				node.misc ^= NODE_DOWN;
			else {
				if(node.status!=NODE_OFFLINE)
					node.status=NODE_OFFLINE;
				else
					node.status=NODE_WFC;
			}
			putnodedat(&cfg,main_dflt,&node,nodefile);
			continue;
		}

		if(j==-2-CTRL('i')) {	/* Interrupt node */
			if(getnodedat(&cfg,main_dflt,&node,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			node.misc^=NODE_INTR;
			putnodedat(&cfg,main_dflt,&node,nodefile);
			continue;
		}
		
		if(j <= -2)
			continue;

		if(j<=cfg.sys_nodes && j>0) {
			i=0;
			strcpy(opt[i++],"Spy on node");
			strcpy(opt[i++],"Node toggles");
			strcpy(opt[i++],"Clear Errors");
			strcpy(opt[i++],"View node log");
			strcpy(opt[i++],"View crash log");
			if(!getnodedat(&cfg,j,&node,NULL)) {
				if((node.status==NODE_INUSE) && node.useron) {
					strcpy(opt[i++],"Send message to user");
					strcpy(opt[i++],"Chat with user");
				}
			}
			opt[i][0]=0;
			i=0;
			uifc.helpbuf=	"`Node Options:`\n"
							"\nToDo: Add help";
			done=0;
			while(!done) {
				switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Node Options",opt))  {
					case 0:	/* Spy */
						dospy(j,&bbs_startup);
						break;

					case 1: /* Node Toggles */
						node_toggles(&cfg, j);
						break;
	
					case 2:
						clearerrors(&cfg, j,&node);
						break;
	
					case 3: /* Node log */
						sprintf(str,"%snode.log",cfg.node_path[j-1]);
						view_log(str,"Node Log");
						break;

					case 4: /* Crash log */
						sprintf(str,"%scrash.log",cfg.node_path[j-1]);
						view_log(str,"Crash Log");
						break;
					
					case 5:	/* Send message */
						sendmessage(&cfg, j,&node);
						break;
	
					case 6:
						chat(&cfg,main_dflt,&node,&boxch,uifc.timedisplay);
						break;
					
					case -1:
						done=1;
						break;
					
					default:
						uifc.msg("Option not implemented");
						break;
				}
			}
		}
	}
}
