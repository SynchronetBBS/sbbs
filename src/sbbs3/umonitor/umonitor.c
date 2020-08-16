/* Synchronet for *nix node activity monitor */

/* $Id: umonitor.c,v 1.98 2020/04/12 20:23:00 rswindell Exp $ */
// vi: tabstop=4

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

#include <signal.h>
#include <sys/types.h>
#include <time.h>
#ifdef __QNX__
#include <string.h>
#endif
#include <stdio.h>

#include "ciolib.h"
#define __COLORS		/* Disable the colour macros in sbbsdefs.h ToDo */
#include "sbbs.h"
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

void node_toggles(scfg_t *cfg,int nodenum)  {
	int nodefile = -1;
	char**	opt;
	int		i,j;
	node_t	node;
	int		save=0;

	if((opt=(char **)alloca(sizeof(char *)*(4+1)))==NULL)
		allocfail(sizeof(char *)*(4+1));
	for(i=0;i<(4+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	uifc.helpbuf=	"`Node Toggles\n"
	                "`------------`\n\n"
	                "`The following are `Yes/No `options.  Hitting Enter toggles between.\n\n"
	                "`Locked for SysOps only : `Locks the node so that only SysOps may \n"
	                "                         logon to them.\n"
	                "`Interrupt (Hangup)     : `The current user will be kicked as soon as it \n"
	                "                         is safe to do so.  A brief message is given\n"
	                "                         to user.\n"
	                "`Re-run on logoff       : `Toggles the system to reload the configuration\n"
	                "                         files when the current user logs off.\n"
	                "`Down node after logoff : `Takes the node offline after current user logs\n"
	                "                         off.\n\n"
	                "`[Note] `These toggles take effect immediately.";
	while(save==0) {
		if(getnodedat(cfg,nodenum,&node,FALSE,&nodefile)) {
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

		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Node Toggles",opt)) {
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
		putnodedat(cfg,nodenum,&node,FALSE,nodefile);
	}
}

int dospy(int nodenum, bbs_startup_t *bbs_startup)  {
	char str[80],str2[80];
	int i;

	if(bbs_startup->temp_dir[0])
		snprintf(str,sizeof(str),"%slocalspy%d.sock", bbs_startup->temp_dir, nodenum);
	else
		snprintf(str,sizeof(str),"%slocalspy%d.sock", bbs_startup->ctrl_dir, nodenum);
	i=spyon(str);
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

	uifc.input(WIN_MID|WIN_SAV,0,0,"Telegram",str2,58,K_WRAP|K_MSG);
	sprintf(str,"\1n\1y\1hMessage From Sysop:\1w %s\r\n",str2);
	if(getnodedat(cfg,nodenum,node,FALSE,NULL))
		return(-1);
	if(node->useron==0)
		return(-1);
	putsmsg(cfg, node->useron, str);
	return(0);
}

int clearerrors(scfg_t *cfg, int nodenum, node_t *node) {
	int nodefile = -1;
	if(getnodedat(cfg,nodenum,node,TRUE,&nodefile)) {
		uifc.msg("getnodedat() failed! (Nothing done)");
		return(-1);
	}
	node->errors=0;
	putnodedat(cfg,nodenum,node,TRUE,nodefile);
	uifc.msg("Error count cleared for this node.");
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

	if(getnodedat(cfg,nodenum,node,FALSE,NULL)) {
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
		snprintf(str[2][0],12,"%s/%s",getnumstr(str[3][2],sstats.etoday),getnumstr(str[3][3],getmail(cfg,0,0,0)));
		l=m=0;
		for(i=0;i<cfg->total_subs;i++)
			l+=getposts(cfg,i); 			/* l=total posts */
		for(i=0;i<cfg->total_dirs;i++)
			m+=getfiles(cfg,i); 			/* m=total files */
		snprintf(str[2][1],12,"%s/%s",getnumstr(str[3][2],sstats.ptoday),getnumstr(str[3][3],l));
		snprintf(str[2][2],12,"%s/%s",getnumstr(str[3][2],sstats.ftoday),getnumstr(str[3][3],getmail(cfg,1,0,0)));
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
		snprintf(str[2][0],12,"%s/%s",getnumstr(str[3][2],sstats.etoday),getnumstr(str[3][3],getmail(cfg,0,0,0)));
		l=m=0;
		for(i=0;i<cfg->total_subs;i++)
			l+=getposts(cfg,i); 			/* l=total posts */
		for(i=0;i<cfg->total_dirs;i++)
			m+=getfiles(cfg,i); 			/* m=total files */
		snprintf(str[2][1],12,"%s/%s",getnumstr(str[3][2],sstats.ptoday),getnumstr(str[3][3],l));
		snprintf(str[2][2],12,"%s/%s",getnumstr(str[3][2],sstats.ftoday),getnumstr(str[3][3],getmail(cfg,1,0,0)));
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
			if(j >= 0 && (buf=(char *)malloc(j+1))!=NULL) {
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
	const int num_opts = 12;
	if((opt=(char **)alloca(sizeof(char *)*(num_opts+1)))==NULL)
		allocfail(sizeof(char *)*(num_opts+1));
	for(i=0;i<(num_opts+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	strcpy(opt[i++],"Today's callers");
	strcpy(opt[i++],"Yesterday's callers");
	strcpy(opt[i++],"Error log");
	strcpy(opt[i++],"Today's log");
	strcpy(opt[i++],"Yesterday's log");
	strcpy(opt[i++],"Spam log");
	strcpy(opt[i++],"SBBSecho log");
	strcpy(opt[i++],"EchoMail stats");
	strcpy(opt[i++],"BinkP stats");
	strcpy(opt[i++],"Bad Areas list");
	strcpy(opt[i++],"Guru log");
	strcpy(opt[i++],"Hack log");
	opt[i][0]=0;
	i=0;
	uifc.helpbuf=	"`View Logs\n"
	                "`---------\n\n"
	                "`Today's callers     : `View a list of Today's callers.\n"
	                "`Yesterday's callers : `View a list of Yesterday's callers.\n"
	                "`Error log           : `View the Error log.\n"
	                "`Today's log         : `View Today's system activity.\n"
	                "`Yesterday's log     : `View Yesterday's system activity.\n"
	                "`Spam log            : `View the log of Spam E-Mail sent to the system.\n"
	                "`SBBSecho log        : `View the FidoNet EchoMail program log.\n"
	                "`EchoMail stats      : `view the FidoNet EchoMail statistics.\n"
					"`Binkp stats         : `view the BinkP FidoNet mailer statistics.\n"
	                "`Bad Areas list      : `view the list of unknown EchoMail areas.\n"
	                "`Guru log            : `View the transcriptions of chats with the Guru.\n"
	                "`Hack log            : `View the Hack attempt log.";

	while(1) {
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"View Logs",opt))  {
			case -1:
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
				view_log(str,"SBBSecho Log");
				break;
			case 7:
				sprintf(str,"%sechostats.ini",cfg->data_dir);
				view_log(str,"EchoMail Stats");
				break;
			case 8:
				sprintf(str,"%sbinkstats.ini",cfg->data_dir);
				view_log(str,"BinkP Stats");
				break;
			case 9:
				sprintf(str,"%sbadareas.lst",cfg->data_dir);
				view_log(str,"Bad Area List");
				break;
			case 10:
				sprintf(str,"%sguru.log",cfg->logs_dir);
				view_log(str,"Guru Log");
				break;
			case 11:
				sprintf(str,"%shack.log",cfg->logs_dir);
				view_log(str,"Hack Log");
				break;
		}
	}
}

int do_cmd(char *cmd)
{
	int i;
	struct text_info ti;
	char *p;

	gettextinfo(&ti);
	p=alloca(ti.screenheight*ti.screenwidth*2);
	gettext(1,1,ti.screenwidth,ti.screenheight,p);
	i=system(cmd);
	puttext(1,1,ti.screenwidth,ti.screenheight,p);
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

	if((opt=(char **)alloca(sizeof(char *)*(cfg->total_qhubs+1)))==NULL)
		allocfail(sizeof(char *)*(cfg->total_qhubs+1));
	for(i=0;i<(cfg->total_qhubs+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);


	uifc.helpbuf=	"`QWK Callouts\n"
					"`------------\n\n"
					"Select the Hub and press enter to initiate a call out to\n"
					"the highlighted system.";

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

	if((opt=(char **)alloca(sizeof(char *)*(cfg->total_events+1)))==NULL)
		allocfail(sizeof(char *)*(cfg->total_events+1));
	for(i=0;i<(cfg->total_events+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	if(cfg->total_events<1) {
		uifc.msg("No events configured!");
		return(1);
	}
	j=0;

	uifc.helpbuf=	"`Run Events\n"
					"`----------\n\n"
					"To run and event, highlight it and press Enter";

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

	if((opt=(char **)alloca(sizeof(char *)*(5+1)))==NULL)
		allocfail(sizeof(char *)*(5+1));
	for(i=0;i<(5+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	strcpy(opt[i++],"FTP server");
	strcpy(opt[i++],"Mail server");
	strcpy(opt[i++],"Services");
	strcpy(opt[i++],"Telnet server");
	strcpy(opt[i++],"Web server");
	opt[i][0]=0;

	uifc.helpbuf=	"`Recycle Servers\n"
					"`---------------\n\n"
					"To rerun a server, highlight it and press Enter.\n"
					"This will reload the configuration files for selected\n"
					"server.";

	i=0;
	while(1) {
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Recycle Servers",opt))  {
			case -1:
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

	const int num_opts = 16;
	if((opt=(char **)alloca(sizeof(char *)*(num_opts+1)))==NULL)
		allocfail(sizeof(char *)*(num_opts+1));
	for(i=0;i<(num_opts+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	i=0;
	strcpy(opt[i++],"sbbs.ini");
	strcpy(opt[i++],"modopts.ini");
	strcpy(opt[i++],"alias.cfg");
	strcpy(opt[i++],"attr.cfg");
	strcpy(opt[i++],"dns_blacklist.cfg");
	strcpy(opt[i++],"dnsbl_exempt.cfg");
	strcpy(opt[i++],"domains.cfg");
	strcpy(opt[i++],"mailproc.ini");
	strcpy(opt[i++],"mime_types.ini");
	strcpy(opt[i++],"relay.cfg");
	strcpy(opt[i++],"sbbsecho.ini");
	strcpy(opt[i++],"../data/areas.bbs");
	strcpy(opt[i++],"services.ini");
	strcpy(opt[i++],"ftpalias.cfg");
	strcpy(opt[i++],"sockopts.ini");
	strcpy(opt[i++],"spambait.cfg");
	opt[i][0]=0;
	uifc.helpbuf= "Highlight desired file and hit Enter to edit it.";
	i=0;
	while(1) {
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Edit Config File",opt))  {
			case -1:
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

	const int num_opts = 11;
	if((opt=(char **)alloca(sizeof(char *)*(num_opts+1)))==NULL)
		allocfail(sizeof(char *)*(num_opts+1));
	for(i=0;i<(num_opts+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
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
	strcpy(opt[i++],"../ctrl/twitlist.cfg");
	strcpy(opt[i++],"../ctrl/spamblock.cfg");
	opt[i][0]=0;
	uifc.helpbuf="Highlight desired file and hit Enter to edit it.";
	i=0;
	while(1) {
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Edit Filter File",opt))  {
			case -1:
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
	char	str[256],ctrl_dir[MAX_PATH + 1];
	char	title[256];
	int		i,j;
	node_t	node;
	int		nodefile = -1;
	box_t	boxch;
	scfg_t	cfg;
	int		done;
	int		ciolib_mode=CIOLIB_MODE_AUTO;
	time_t	last_semfile_check = time(NULL);
	int		idle_sleep=100;

	/******************/
	/* Ini file stuff */
	/******************/
	char	ini_file[MAX_PATH+1];
	FILE*				fp=NULL;
	bbs_startup_t		bbs_startup;

	sscanf("$Revision: 1.98 $", "%*s %s", revision);

	printf("\nSynchronet UNIX Monitor %s-%s  Copyright %s "
		"Rob Swindell\n",revision,PLATFORM_DESC,&__DATE__[7]);

	SAFECOPY(ctrl_dir, get_ctrl_dir());
	backslash(ctrl_dir);

	gethostname(str,sizeof(str)-1);

	sbbs_get_ini_fname(ini_file, ctrl_dir, str);

	/* Initialize BBS startup structure */
	memset(&bbs_startup,0,sizeof(bbs_startup));
	bbs_startup.size=sizeof(bbs_startup);
	SAFECOPY(bbs_startup.ctrl_dir,ctrl_dir);

	/* Read .ini file here */
	if(ini_file[0]!=0 && (fp=fopen(ini_file,"r"))!=NULL) {
		printf("Reading %s\n",ini_file);
	}
	/* We call this function to set defaults, even if there's no .ini file */
	sbbs_read_ini(fp, ini_file,
		NULL,		/* global_startup */
		NULL, &bbs_startup,
		NULL, NULL, /* ftp_startup */
		NULL, NULL, /* web_startup */
		NULL, NULL, /* mail_startup */
		NULL, NULL  /* services_startup */
		);

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
	uifc.mode|=UIFC_NOCTRL;

	uifc.esc_delay=500;

	boxch.ls=(char)186;
	boxch.rs=(char)186;
	boxch.ts=(char)205;
	boxch.bs=(char)205;
	boxch.tl=(char)201;
	boxch.tr=(char)187;
	boxch.bl=(char)200;
	boxch.br=(char)188;
	for(i=1;i<argc;i++) {
		if(argv[i][0]=='-')
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
				case 'S':
					idle_sleep=atoi(argv[i]+2);
					break;
				case 'I':
					switch(toupper(argv[i][2])) {
						case 'A':
							ciolib_mode=CIOLIB_MODE_ANSI;
							break;
						case 'C':
							ciolib_mode=CIOLIB_MODE_CURSES;
							break;
						case 0:
							printf("NOTICE: The -i option is deprecated, use -if instead\n");
							SLEEP(2000);
						case 'F':
							ciolib_mode=CIOLIB_MODE_CURSES_IBM;
							break;
						case 'I':
							ciolib_mode=CIOLIB_MODE_CURSES_ASCII;
							break;
						case 'X':
							ciolib_mode=CIOLIB_MODE_X;
							break;
						case 'W':
							ciolib_mode=CIOLIB_MODE_CONIO;
							break;
						default:
							goto USAGE;
					}
					break;
				default:
USAGE:
					printf("\nusage: %s [ctrl_dir] [options]\n"
					         "options:\n\n"
					         "-c  =  force color mode\n"
					         "-e# =  set escape delay to #msec\n"
					         "-iX =  set interface mode to X (default=auto) where X is one of:\n"
#ifdef __unix__
					         "       X = X11 mode\n"
					         "       C = Curses mode\n"
					         "       F = Curses mode with forced IBM charset\n"
							 "       I = Curses mode with forced ASCII charset\n"
#else
					         "       W = Win32 native mode\n"
#endif
					         "       A = ANSI mode\n"
					         "-l# =  set screen lines to #\n"
					         "-s# =  set idle slsep to # milliseconds (defualt: %d)\n"
					    ,argv[0]
					    ,idle_sleep
					);
					exit(0);
			}
	}

#ifdef SIGPIPE
	signal(SIGPIPE, SIG_IGN);
#endif

	uifc.size=sizeof(uifc);
	i=initciolib(ciolib_mode);
	if(i!=0) {
		printf("ciolib library init returned error %d\n",i);
		exit(1);
	}
	i=uifcini32(&uifc);  /* curses */
	if(i!=0) {
		printf("uifc library init returned error %d\n",i);
		exit(1);
	}

	const int main_menu_opts = 11;
	if((opt=(char **)alloca(sizeof(char *)*(main_menu_opts+1)))==NULL)
		allocfail(sizeof(char *)*(main_menu_opts+1));
	for(i=0;i<(main_menu_opts+1);i++)
		if((opt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	if((mopt=(char **)alloca(sizeof(char *)*cfg.sys_nodes+2))==NULL)
		allocfail(sizeof(char *)*cfg.sys_nodes+2);
	for(i=0;i<cfg.sys_nodes+2;i++)
		if((mopt[i]=(char *)alloca(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	sprintf(title,"Synchronet UNIX Monitor %s-%s",revision,PLATFORM_DESC);
	if(uifc.scrn(title)) {
		printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		bail(1);
	}

	int paging_node = 0;
	while(1) {
		strcpy(mopt[0],"System Options");
		for(i=1;i<=cfg.sys_nodes;i++) {
			if((j=getnodedat(&cfg,i,&node,FALSE,NULL)))
				sprintf(mopt[i],"Error reading node data (%d)!",j);
			else {
				nodestatus(&cfg, &node, str, 71, i);
				if(i == paging_node) {
					strupr(str);
					strcat(str,  " <PAGING>");
				}
				sprintf(mopt[i],"%3d: %s", i, str);
			}
		}
		mopt[i][0]=0;

		uifc.helpbuf=  "`Synchronet UNIX Monitor\n"
		               "`------------------\n"
		                "Welcome to the Synchronet UNIX Monitor.\n"
		                "Displayed on this screen are the statitics for the BBS\n"
		                "You can scroll through the list starting at \"System Options\" \n"
		                "Pressing Enter on each will give a menu of option to perform.\n"
		                "Additionally these keys are available from this screen:\n\n"
		               "`CTRL-E : `Displays the error log\n"
		               "`F12    : `Spys on the currently selected node\n"
		               "`F11    : `Send message to the currently selected node\n"
		               "`F10    : `Chats with the user on the currently selected node\n"
		               "`DEL    : `Clear errors on currently selected node\n"
		               "`CTRL-L : `Lock node toggle\n"
		               "`CTRL-R : `Rerun node\n"
		               "`CTRL-D : `Down node toggle\n"
		               "`CTRL-I : `Interrupt node\n";

		drawstats(&cfg, main_dflt, &node, &main_dflt, &main_bar);

		if(time(NULL) - last_semfile_check > bbs_startup.sem_chk_freq * 2) {
			paging_node = 0;
			SAFEPRINTF(str, "%ssyspage.*", cfg.ctrl_dir);
			if(fexistcase(str) && (paging_node = atoi(getfext(str) + 1)) > 0) {
				putch(BEL);
				char msg[128];
				SAFEPRINTF(msg, "Node %u is paging you to chat", paging_node);
				uifc.pop(msg);
				SLEEP(1500);
				uifc.pop(NULL);
			}
			last_semfile_check = time(NULL);
		}

		j=uifc.list(WIN_L2R|WIN_ESC|WIN_ACT|WIN_DYN,0,5,70,&main_dflt,&main_bar
			,title,mopt);

		if(j == -2) {
			SLEEP(idle_sleep);
			continue;
		}

		last_semfile_check = time(NULL);

		if(j==-7) {	/* CTRL-E */
			sprintf(str,"%s/error.log",cfg.data_dir);
			view_log(str,"Error Log");
			continue;
		}

		if(j==0) {
			BOOL sysop_avail = sysop_available(&cfg);
			int sysop_chat_opt;

			/* System Options */
			i=0;
			strcpy(opt[i++],"Configure BBS");
			strcpy(opt[i++],"Configure FidoNet");
			strcpy(opt[i++],"Edit Users");
			strcpy(opt[i++],"Run SyncTERM");
			strcpy(opt[i++],"View Logs");
			strcpy(opt[i++],"Force QWKnet Callout");
			strcpy(opt[i++],"Force Timed Event");
			strcpy(opt[i++],"Recycle Servers");
			strcpy(opt[i++],"Edit Config Files");
			strcpy(opt[i++],"Edit Filter Files");
			sysop_chat_opt = i++;
			opt[i][0]=0;
			uifc.helpbuf=	"`System Options`\n"
			                "`------------`\n\n"
			                "`Configure BBS         : `Run the Synchronet Configuration Utility (SCFG).\n"
							"`Configure FidoNet     : `Run the FidoNet Configuration Utility (EchoCFG).\n"
			                "`Edit Users            : `Run the Synchronet User Editor.\n"
			                "`Run SyncTERM          : `Run SyncTERM for RLogin.  SyncTERM must be\n"
			                 "                        in the exec directory.\n"
			                "`View Logs             : `View the various system logs.\n"
			                "`Force QWKnet callout  : `Force a callout to QWK Network Hub. Select which\n"
			                 "                        Hub from a popup list of configured Hubs.\n"
			                "`Force Timed Event     : `Call up a menu of system events that can be\n"
			                "                        manually.\n"
			                "`Recycle Servers       : `Have the Servers reload their configuration \n"
			                "                        files.\n"
			                "`Edit Config Files     : `Edit the various configuration files.\n"
			                "`Edit Filter Files     : `Edit the various filter files, e.g. ip.can.";

			done=0;
			i=0;
			while(!done) {
				sprintf(opt[sysop_chat_opt], "Turn Sysop Chat availability %s", sysop_avail ? "Off" : "On");
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
						sprintf(str,"%sechocfg ",cfg.exec_dir);
						for(j=1; j<argc; j++) {
							strcat(str,"'");
							strcat(str,argv[j]);
							strcat(str,"' ");
						}
						do_cmd(str);
						break;
					case 2:
						sprintf(str,"%suedit ",cfg.exec_dir);
						for(j=1; j<argc; j++) {
							strcat(str,"'");
							strcat(str,argv[j]);
							strcat(str,"' ");
						}
						do_cmd(str);
						break;
					case 3:
						sprintf(str,"%ssyncterm",cfg.exec_dir);
						for(j=1; j<argc; j++) {
							strcat(str,"'");
							strcat(str,argv[j]);
							strcat(str,"' ");
						}
						do_cmd(str);
						break;
					case 4:
						view_logs(&cfg);
						break;
					case 5:
						qwk_callouts(&cfg);
						break;
					case 6:
						run_events(&cfg);
						break;
					case 7:
						recycle_servers(&cfg);
						break;
					case 8:
						edit_cfg(&cfg);
						break;
					case 9:
						edit_can(&cfg);
						break;
					case 10:
						sysop_avail = !sysop_avail;
						set_sysop_availability(&cfg, sysop_avail);
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
			i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Exit Synchronet Monitor",opt);
			if(!i)
				bail(0);
			continue;
		}

		/* Everything after this point is invalid for the System Options */
		if(main_dflt==0)
			continue;

		if(j==-2-CIO_KEY_DC) {	/* Clear errors */
			clearerrors(&cfg, main_dflt,&node);
			continue;
		}

		if(j==-2-CIO_KEY_F(10)) {	/* Chat */
			if(getnodedat(&cfg,main_dflt,&node,FALSE,NULL)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			if((node.status==NODE_INUSE) && node.useron)
				chat(&cfg,main_dflt,&node,&boxch,NULL);
			continue;
		}

		if(j==-2-CIO_KEY_F(11)) {	/* Send message */
			sendmessage(&cfg, main_dflt,&node);
			continue;
		}

		if(j==-2-CIO_KEY_F(12)) {	/* Spy */
			dospy(main_dflt,&bbs_startup);
			continue;
		}

		if(j==-2-CTRL('l')) {	/* Lock node */
			if(getnodedat(&cfg,main_dflt,&node,TRUE,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			node.misc^=NODE_LOCK;
			putnodedat(&cfg,main_dflt,&node,FALSE,nodefile);
			continue;
		}

		if(j==-2-CTRL('r')) {	/* Rerun node */
			if(getnodedat(&cfg,main_dflt,&node,TRUE,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			node.misc^=NODE_RRUN;
			putnodedat(&cfg,main_dflt,&node,FALSE,nodefile);
			continue;
		}

		if(j==-2-CTRL('d')) {	/* Down node */
			if(getnodedat(&cfg,main_dflt,&node,TRUE,&nodefile)) {
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
			putnodedat(&cfg,main_dflt,&node,FALSE,nodefile);
			continue;
		}

		if(j==-2-CTRL('i')) {	/* Interrupt node */
			if(getnodedat(&cfg,main_dflt,&node,TRUE,&nodefile)) {
				uifc.msg("Error reading node data!");
				continue;
			}
			node.misc^=NODE_INTR;
			putnodedat(&cfg,main_dflt,&node,FALSE,nodefile);
			continue;
		}

		if(j <= -2)
			continue;

		if(j<=cfg.sys_nodes && j>0) {
			if((node.status==NODE_INUSE) && node.useron) {
				i=0;
				strcpy(opt[i++],"Edit User");
				strcpy(opt[i++],"Spy on node");
				strcpy(opt[i++],"Send message to user");
				strcpy(opt[i++],"Chat with user");
				strcpy(opt[i++],"Node toggles");
				strcpy(opt[i++],"Clear Errors");
				strcpy(opt[i++],"View node log");
				strcpy(opt[i++],"View crash log");

				opt[i][0]=0;
				i=0;
				uifc.helpbuf=	"`Node Options\n"
						"`------------\n\n"
						"`Edit User            : `Call up the user editor to edit current user.\n"
						"`Spy on User          : `Spy on current user.\n"
						"`Send message to user : `Send on online message to user.\n"
						"`Chat with user       : `Initiate private split-screen chat wityh user.\n"
						"`Node Toggles         : `Call up Node Toggles menu to set various toggles\n"
						 "                       for current node.\n"
						"`Clear Errors         : `Clears the error count for node.\n"
						"`View node log        : `View activity log for current node.\n"
						"`View crash log       : `View the crash log for current node.";
				done=0;
				while(!done) {
					switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Node Options",opt))  {

						case 0:  /* Edit Users */
							sprintf(str,"%suedit %d",cfg.exec_dir,node.useron);
							for(j=1; j<argc; j++) {
							  strcat(str,"'");
							  strcat(str,argv[j]);
							  strcat(str,"' ");
							}
							do_cmd(str);
							break;

						case 1:	/* Spy */
							dospy(j,&bbs_startup);
							break;

						case 2:	/* Send message */
							sendmessage(&cfg, j,&node);
							break;

						case 3: /* Chat with User */
							chat(&cfg,main_dflt,&node,&boxch,NULL);
							break;

						case 4: /* Node Toggles */
							node_toggles(&cfg, j);
							break;

						case 5:
							clearerrors(&cfg, j,&node);
							break;

						case 6: /* Node log */
							sprintf(str,"%snode.log",cfg.node_path[j-1]);
							view_log(str,"Node Log");
							break;

						case 7: /* Crash log */
							sprintf(str,"%scrash.log",cfg.node_path[j-1]);
							view_log(str,"Crash Log");
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
		if(j<=cfg.sys_nodes && j>0) {
			if(!((node.status==NODE_INUSE) && node.useron)) {
				i=0;
				strcpy(opt[i++],"Node toggles");
				strcpy(opt[i++],"Clear Errors");
				strcpy(opt[i++],"View node log");
				strcpy(opt[i++],"View crash log");

				opt[i][0]=0;
				i=0;
				uifc.helpbuf="`Node Options\n"
				             "`------------\n\n"
				             "`Node Toggles   : `Call up Node Toggles menu to set various toggles\n"
				              "                 for current node.\n"
				             "`Clear Errors   : `Clears the error count for node.\n"
				             "`View node log  : `View activity log for current node.\n"
				             "`View crash log : `View the crash log for current node.";
				done=0;
				while(!done) {
					switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,"Node Options",opt))  {

						case 0: /* Node Toggles */
							node_toggles(&cfg, j);
							break;

						case 1:
							clearerrors(&cfg, j,&node);
							break;

						case 2: /* Node log */
							sprintf(str,"%snode.log",cfg.node_path[j-1]);
							view_log(str,"Node Log");
							break;

						case 3: /* Crash log */
							sprintf(str,"%scrash.log",cfg.node_path[j-1]);
							view_log(str,"Crash Log");
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
}
