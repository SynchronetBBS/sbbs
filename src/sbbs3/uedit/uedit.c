/* uedit.c */

/* Synchronet for *nix user editor */

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

#define CTRL(x) (x&037)

/********************/
/* Global Variables */
/********************/
uifcapi_t uifc; /* User Interface (UIFC) Library API */
char *YesStr="Yes";
char *NoStr="No";
int 		modified=0;

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

int confirm(char *prompt)
{
	int i=0;
	char *opt[3]={
		 YesStr
		,NoStr
		,""
	};

	i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&i,0,prompt,opt);
	if(i==0)
		return(1);
	if(i==-1)
		return(-1);
	return(0);
}

int check_save(scfg_t *cfg,user_t *user)
{
	int i;

	if(modified) {
		i=confirm("Save Changes?");
		if(i==1) {
			putuserdat(cfg,user);
		}
	}
	return(i);
}

/****************************************************************************/
/* Takes a string in the format HH:MM:SS and returns in seconds             */
/****************************************************************************/
time_t DLLCALL strtosec(char *str)
{
	char *p1;
	char *p2;
	long int hour=0;
	long int min=0;
	long int sec2=0;
	time_t	sec=0;

	hour=strtol(str,&p1,10);
	if(hour<0 || hour > 24)
		return(-1);
	if(*p1==':') {
		p1++;
		min=strtol(p1,&p2,10);
		if(min<0 || min > 59)
			return(-1);
		if(*p2==':') {
			p2++;
			sec2=strtol(p2,&p1,10);
			if(sec2 < 0 || sec2 > 59)
				return(-1);
		}
	}

	sec=hour*60*60;
	sec+=min*60;
	sec+=sec2;
	return(sec);
}

/* Edit "Extended comment" */
int edit_comment(scfg_t *cfg, user_t *user)
{
	return(0);
}

/* MSG and File settings
 *      - Message Options
 *      - QWK Message Packet
 *      - File Options
 */
int edit_msgfile(scfg_t *cfg, user_t *user)
{
	return(0);
}

/* Settings
 *     - Terminal Settings
 *     - Logon Toggles
 *     - Chat Toggles
 *     - Command Shell
 */
int edit_settings(scfg_t *cfg, user_t *user)
{
	return(0);
}

/* Statistics
 *     First On
 *     Last On
 *     Total Logons
 *     Todays Logons
 *     Total Posts
 *     Todays Posts
 *     Total Uploads
 *     Todays Uploads
 *     Total Time On
 *     Todays Time On
 *     Last Call Time On
 *     Extra
 *     Total Downloads
 *     Bytes
 *     Leech
 *     Total Email
 *     Todays Email
 *     Email to Sysop
 */
int edit_stats(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];
	time_t	temptime,temptime2;

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		i=0;
		sprintf(opt[i++],"First On:          %s",user->firston?timestr(cfg, &user->firston, str):"Never");
		sprintf(opt[i++],"Last On:           %s",user->laston?timestr(cfg, &user->laston, str):"Never");
		sprintf(opt[i++],"Logon Time:        %s",user->laston?timestr(cfg, &user->logontime, str):"Not On");
		sprintf(opt[i++],"Total Logons:      %hu",user->logons);
		sprintf(opt[i++],"Todays Logons:     %hu",user->ltoday);
		sprintf(opt[i++],"Total Posts:       %hu",user->posts);
		sprintf(opt[i++],"Todays Posts:      %hu",user->ptoday);
		sprintf(opt[i++],"Total Email:       %hu",user->emails);
		sprintf(opt[i++],"Todays Email:      %hu",user->etoday);
		sprintf(opt[i++],"Email To Sysop:    %hu",user->fbacks);
		sprintf(opt[i++],"Total Time On:     %hu",user->timeon);
		sprintf(opt[i++],"Time On Today:     %hu",user->ttoday);
		sprintf(opt[i++],"Time On Last Call: %hu",user->tlast);
		sprintf(opt[i++],"Extra Time Today:  %hu",user->textra);
		sprintf(opt[i++],"Total Downloads:   %hu",user->dls);
		sprintf(opt[i++],"Downloaded Bytes:  %lu",user->dlb);
		sprintf(opt[i++],"Total Uploads:     %hu",user->uls);
		sprintf(opt[i++],"Uploaded Bytes:    %lu",user->ulb);
		sprintf(opt[i++],"Leech:             %u",user->leech);
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"Statistics",opt)) {
			case -1:
				freeopt(opt);
				return(0);
				break;
			case 0:
				/* First On */
				temptime=user->firston;
				unixtodstr(cfg,temptime,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"First On Date",str,8,K_EDIT);
				user->firston=dstrtounix(cfg, str);
				temptime2=temptime-user->firston;
				sectostr(temptime2,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"First On Time",str,8,K_EDIT);
				temptime2=strtosec(str);
				if(temptime2!=-1)
					user->firston += temptime2;
				if(temptime!=user->firston)
					modified=1;
				break;
			case 1:
				/* Last On */
				temptime=user->laston;
				unixtodstr(cfg,temptime,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Last On Date",str,8,K_EDIT);
				user->laston=dstrtounix(cfg, str);
				temptime2=temptime-user->laston;
				sectostr(temptime2,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Last On Time",str,8,K_EDIT);
				temptime2=strtosec(str);
				if(temptime2!=-1)
					user->laston += temptime2;
				if(temptime!=user->laston)
					modified=1;
				break;
			case 2:
				/* Logon Time */
				temptime=user->logontime;
				unixtodstr(cfg,temptime,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Logon Date",str,8,K_EDIT);
				user->logontime=dstrtounix(cfg, str);
				temptime2=temptime-user->logontime;
				sectostr(temptime2,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Logon Time",str,8,K_EDIT);
				temptime2=strtosec(str);
				if(temptime2!=-1)
					user->logontime += temptime2;
				if(temptime!=user->logontime)
					modified=1;
				break;
			case 3:
				/* Total Logons */
				sprintf(str,"%hu",user->logons);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Total Logons",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->logons=strtoul(str,NULL,10);
				}
				break;
			case 4:
				/* Todays Logons */
				sprintf(str,"%hu",user->ltoday);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Todays Logons",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->ltoday=strtoul(str,NULL,10);
				}
				break;
			case 5:
				/* Total Posts */
				sprintf(str,"%hu",user->posts);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Total Posts",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->posts=strtoul(str,NULL,10);
				}
				break;
			case 6:
				/* Todays Posts */
				sprintf(str,"%hu",user->ptoday);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Todays Posts",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->ptoday=strtoul(str,NULL,10);
				}
				break;
			case 7:
				/* Total Emails */
				sprintf(str,"%hu",user->emails);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Total Emails",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->emails=strtoul(str,NULL,10);
				}
				break;
			case 8:
				/* Todays Emails */
				sprintf(str,"%hu",user->etoday);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Todays Emails",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->etoday=strtoul(str,NULL,10);
				}
				break;
			case 9:
				/* Emails to Sysop */
				sprintf(str,"%hu",user->fbacks);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Emails to Sysop",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->fbacks=strtoul(str,NULL,10);
				}
				break;
			case 10:
				/* Total Time On */
				sprintf(str,"%hu",user->timeon);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Total Time On",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->timeon=strtoul(str,NULL,10);
				}
				break;
			case 11:
				/* Time On Today */
				sprintf(str,"%hu",user->ttoday);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Time On Today",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->ttoday=strtoul(str,NULL,10);
				}
				break;
			case 12:
				/* Time On Last Call */
				sprintf(str,"%hu",user->tlast);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Time On Last Call",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->tlast=strtoul(str,NULL,10);
				}
				break;
			case 13:
				/* Extra Time Today */
				sprintf(str,"%hu",user->textra);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Extra Time Today",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->textra=strtoul(str,NULL,10);
				}
				break;
			case 14:
				/* Total Downloads */
				sprintf(str,"%hu",user->dls);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Total Downloads",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->dls=strtoul(str,NULL,10);
				}
				break;
			case 15:
				/* Downloaded Bytes */
				sprintf(str,"%lu",user->dlb);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Downloaded Bytes",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->dlb=strtoul(str,NULL,10);
				}
				break;
			case 16:
				/* Total Uploads */
				sprintf(str,"%hu",user->uls);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Total Uploads",str,5,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->uls=strtoul(str,NULL,10);
				}
				break;
			case 17:
				/* Uploaded Bytes */
				sprintf(str,"%lu",user->ulb);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Uploaded Bytes",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->ulb=strtoul(str,NULL,10);
				}
				break;
			case 18:
				/* Leech Counter */
				sprintf(str,"%u",user->leech);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Leech Counter",str,3,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->leech=strtoul(str,NULL,10);
				}
				break;
		}
	}
	return(0);
}

/* Security settings
 *     Level
 *     Expiration
 *     Flag Set 1
 *     Flag Set 2
 *     Flag Set 3
 *     Flag Set 4
 *     Exemptions
 *     Restrictions
 *     Credits
 *     Free Credits
 *     Minutes
 */
int edit_security(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	str[256];

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		i=0;
		sprintf(opt[i++],"Level:        %d",user->level);
		sprintf(opt[i++],"Expiration:   %s",user->expire?unixtodstr(cfg, user->expire, str):"Never");
		sprintf(opt[i++],"Flag Set 1:   %s",ltoaf(user->flags1,str));
		sprintf(opt[i++],"Flag Set 2:   %s",ltoaf(user->flags2,str));
		sprintf(opt[i++],"Flag Set 3:   %s",ltoaf(user->flags3,str));
		sprintf(opt[i++],"Flag Set 4:   %s",ltoaf(user->flags4,str));
		sprintf(opt[i++],"Exemptions:   %s",ltoaf(user->exempt,str));
		sprintf(opt[i++],"Restrictions: %s",ltoaf(user->rest,str));
		sprintf(opt[i++],"Credits:      %lu",user->cdt);
		sprintf(opt[i++],"Free Credits: %lu",user->freecdt);
		sprintf(opt[i++],"Minutes:      %lu",user->min);
		opt[i][0]=0;
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"Security Settings",opt)) {
			case -1:
				freeopt(opt);
				return(0);
				break;
			case 0:
				/* Level */
				sprintf(str,"%d",user->level);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Level",str,2,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->level=atoi(str);
				}
				break;
			case 1:
				/* Expiration */
				unixtodstr(cfg,user->expire,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Expiration",str,8,K_EDIT);
				if(uifc.changes && dstrtounix(cfg, str)!=user->expire) {
					modified=1;
					user->expire=dstrtounix(cfg, str);
				}
				break;
			case 2:
				/* Flag Set 1 */
				ltoaf(user->flags1,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Flag Set 1",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					modified=1;
					user->flags1=aftol(str);
				}
				break;
			case 3:
				/* Flag Set 2 */
				ltoaf(user->flags2,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Flag Set 2",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					modified=1;
					user->flags2=aftol(str);
				}
				break;
			case 4:
				/* Flag Set 3 */
				ltoaf(user->flags3,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Flag Set 3",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					modified=1;
					user->flags3=aftol(str);
				}
				break;
			case 5:
				/* Flag Set 4 */
				ltoaf(user->flags4,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Flag Set 4",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					modified=1;
					user->flags4=aftol(str);
				}
				break;
			case 6:
				/* Exemptions */
				ltoaf(user->exempt,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Exemptions",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					modified=1;
					user->exempt=aftol(str);
				}
				break;
			case 7:
				/* Restrictions */
				ltoaf(user->rest,str);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Restrictions",str,26,K_EDIT|K_UPPER|K_ALPHA);
				if(uifc.changes) {
					modified=1;
					user->rest=aftol(str);
				}
				break;
			case 8:
				/* Credits */
				sprintf(str,"%lu",user->cdt);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Credits",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->cdt=strtoul(str,NULL,10);
				}
				break;
			case 9:
				/* Free Credits */
				sprintf(str,"%lu",user->freecdt);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Free Credits",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->freecdt=strtoul(str,NULL,10);
				}
				break;
			case 10:
				/* Minutes */
				sprintf(str,"%lu",user->min);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Minutes",str,10,K_EDIT|K_NUMBER);
				if(uifc.changes) {
					modified=1;
					user->min=strtoul(str,NULL,10);
				}
				break;
		}
	}

	return(0);
}

/*
 * Personal settings... 
 *     Real Name
 *     Computer
 *     NetMail
 *     Phone
 *     Note
 *     Comment
 *     Gender
 *     Connection
 *     Handle
 *     Password
 *     Address 1
 *     Address 2
 *     Postal/ZIP?
 */
int edit_personal(scfg_t *cfg, user_t *user)
{
	int 	i,j;
	char 	**opt;
	char	onech[2];

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	j=0;
	while(1) {
		i=0;
		sprintf(opt[i++],"Real Name:  %s",user->name);
		sprintf(opt[i++],"Computer:   %s",user->comp);
		sprintf(opt[i++],"NetMail:    %s",user->netmail);
		sprintf(opt[i++],"Phone:      %s",user->phone);
		sprintf(opt[i++],"Note:       %s",user->note);
		sprintf(opt[i++],"Comment:    %s",user->comment);
		sprintf(opt[i++],"Gender:     %c",user->sex);
		sprintf(opt[i++],"Connection: %s",user->modem);
		sprintf(opt[i++],"Handle:     %s",user->alias);
		sprintf(opt[i++],"Password:   %s",user->pass);
		sprintf(opt[i++],"Location:   %s",user->location);
		sprintf(opt[i++],"Address:    %s",user->address);
		sprintf(opt[i++],"Postal/Zip: %s",user->zipcode);
		opt[i][0]=0;
		uifc.changes=FALSE;
		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"Personal Settings",opt)) {
			case -1:
				freeopt(opt);
				return(0);
			case 0:
				/* Real Name */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Real Name",user->name,LEN_NAME,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 1:
				/* Computer */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Computer",user->comp,LEN_COMP,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 2:
				/* NetMail */
				uifc.input(WIN_MID|WIN_SAV,0,0,"NetMail Address",user->netmail,LEN_NETMAIL,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 3:
				/* Phone */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Phone",user->phone,LEN_PHONE,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 4:
				/* Note */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Note",user->note,LEN_NOTE,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 5:
				/* Comment */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Comment",user->comment,LEN_COMMENT,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 6:
				/* Gender */
				sprintf(onech,"%c",user->sex);
				uifc.input(WIN_MID|WIN_SAV,0,0,"Gender",onech,1,K_UPPER|K_ALPHA|K_EDIT);
				if(onech[0]!=user->sex && (onech[0]=='M' || onech[0]=='F')) {
					modified=1;
					user->sex=onech[0];
				}
				break;
			case 7:
				/* Connection */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Connection",user->modem,LEN_MODEM,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 8:
				/* Handle */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Handle",user->alias,LEN_ALIAS,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 9:
				/* Password */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Password",user->pass,LEN_PASS,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 10:
				/* Location */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Location",user->location,LEN_LOCATION,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 11:
				/* Address */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Address",user->address,LEN_ADDRESS,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
			case 12:
				/* Postal/Zip */
				uifc.input(WIN_MID|WIN_SAV,0,0,"Postal/Zip Code",user->zipcode,LEN_ZIPCODE,K_EDIT);
				if(uifc.changes)
					modified=1;
				break;
		}
	}

	return(0);
}

/*
 * This is where the good stuff happens
 */
int edit_user(scfg_t *cfg, int usernum)
{
	char**	opt;
	int 	i,j;
	user_t	user;

	if((opt=(char **)MALLOC(sizeof(char *)*(MAX_OPTS+1)))==NULL)
		allocfail(sizeof(char *)*(MAX_OPTS+1));
	for(i=0;i<(MAX_OPTS+1);i++)
		if((opt[i]=(char *)MALLOC(MAX_OPLN))==NULL)
			allocfail(MAX_OPLN);

	user.number=usernum;
	getuserdat(cfg,&user);

	modified=0;
	j=0;
	while(1) {
		i=0;
		strcpy(opt[i++],"Reload");
		if (user.misc & DELETED)
			strcpy(opt[i++],"Undelete");
		else
			strcpy(opt[i++],"Delete");
		if (user.misc & INACTIVE)
			strcpy(opt[i++],"Activate");
		else
			strcpy(opt[i++],"Deactivate");
		strcpy(opt[i++],"Personal");
		strcpy(opt[i++],"Security");
		strcpy(opt[i++],"Statistics");
		strcpy(opt[i++],"Settings");
		strcpy(opt[i++],"MSG/File Settings");
		strcpy(opt[i++],"Extended Comment");
		opt[i][0]=0;

		switch(uifc.list(WIN_MID|WIN_SAV,0,0,0,&j,0,"Edit User",opt)) {
			case -1:
				if(modified) {
					i=check_save(cfg,&user);
				}
				if(i!=-1) {
					freeopt(opt);
					return(0);
				}
				break;

			case 0:
				if(modified && confirm("This will undo any changes you have made.  Continue?")==1) {
					getuserdat(cfg,&user);
					modified=0;
				}
				modified=0;
				break;

			case 1:
				user.misc ^= DELETED;
				modified=1;
				break;

			case 2:
				user.misc ^= INACTIVE;
				modified=1;
				break;

			case 3:
				edit_personal(cfg,&user);
				break;

			case 4:
				edit_security(cfg,&user);
				break;

			case 5:
				edit_stats(cfg,&user);
				break;

			case 6:
				edit_settings(cfg,&user);
				break;

			case 7:
				edit_msgfile(cfg,&user);
				break;

			case 8:
				edit_comment(cfg,&user);
				break;

			default:
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
	scfg_t	cfg;
	int		done;
	int		last;
	user_t	user;
	/******************/
	/* Ini file stuff */
	/******************/
	char	ini_file[MAX_PATH+1];
	FILE*				fp;
	bbs_startup_t		bbs_startup;

	sscanf("$Revision$", "%*s %s", revision);

    printf("\nSynchronet User Editor %s-%s  Copyright 2003 "
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

	sprintf(title,"Synchronet User Editor %s-%s",revision,PLATFORM_DESC);
	if(uifc.scrn(title)) {
		printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		bail(1);
	}

	strcpy(mopt[0],"New User");
	strcpy(mopt[1],"Find User");
	strcpy(mopt[2],"User List");
	mopt[3][0]=0;

	uifc.helpbuf=	"`User Editor:`\n"
					"\nToDo: Add Help";

	while(1) {
		j=uifc.list(WIN_L2R|WIN_ESC|WIN_ACT|WIN_DYN|WIN_ORG,0,5,70,&main_dflt,&main_bar
			,title,mopt);

		if(j == -2)
			continue;

		if(j==-8) {	/* CTRL-F */
			/* Find User */
			continue;
		}
		
		if(j==-2-KEY_F(4)) {	/* Find? */
			continue;
		}

		if(j <= -2)
			continue;

		if(j==-1) {
			uifc.helpbuf=	"`Exit Synchronet User Editor:`\n"
							"\n"
							"\nIf you want to exit the Synchronet user editor,"
							"\nselect `Yes`. Otherwise, select `No` or hit ~ ESC ~.";
			if(confirm("Exit Synchronet User Editor"))
				bail(0);
			continue;
		}

		if(j==0) {
			/* New User */
		}
		if(j==1) {
			/* Find User */
		}
		if(j==2) {
			/* User List */
			done=0;
			while(!done) {
				last=lastuser(&cfg);
				for(i=1; i<=last; i++) {
					user.number=i;
					getuserdat(&cfg,&user);
					sprintf(opt[i-1],"%s (%s)",user.name,user.alias);
				}
				opt[i-1][0]=0;
				i=0;
				switch(uifc.list(WIN_MID,0,0,0,&i,0,"Select User",opt)) {
					case -1:
						done=1;
						break;
					default:
						edit_user(&cfg, i+1);
						break;
				}
			}
		}
	}
}
