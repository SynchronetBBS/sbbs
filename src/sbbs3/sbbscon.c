/* sbbscon.c */

/* Synchronet vanilla/console-mode "front-end" */

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

/* Synchronet-specific headers */
#include "sbbs.h"
#include "conwrap.h"	/* kbhit/getch */
#include "bbs_thrd.h"	/* bbs_thread */
#include "ftpsrvr.h"	/* ftp_startup_t, ftp_server */
#include "mailsrvr.h"	/* mail_startup_t, mail_server */

/* Constants */
#define SBBSCON_VERSION		"1.00"

/* Global variables */
BOOL			bbs_running=FALSE;
bbs_startup_t	bbs_startup;
BOOL			ftp_running=FALSE;
ftp_startup_t	ftp_startup;
BOOL			mail_running=FALSE;
mail_startup_t	mail_startup;

/************************************************/
/* Truncates white-space chars off end of 'str' */
/************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=' ') c--;
	str[c]=0;
}

/****************************************************************************/
/* BBS local/log print routine												*/
/****************************************************************************/
static int bbs_lputs(char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm*	tm_p;

	t=time(NULL);
	tm_p=localtime(&t);
	if(tm_p==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d  BBS  "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%s%.*s",tstr,sizeof(logline)-2,str);
	truncsp(logline);
	printf("%s\n",logline);
	
    return(strlen(logline)+1);
}

static void bbs_started(void)
{
	bbs_lputs("bbs_started");
	bbs_running=TRUE;
}

static void bbs_terminated(int code)
{
	bbs_lputs("bbs_terminated");
	bbs_running=FALSE;
}

/****************************************************************************/
/* FTP local/log print routine												*/
/****************************************************************************/
static int ftp_lputs(char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm*	tm_p;

	t=time(NULL);
	tm_p=localtime(&t);
	if(tm_p==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d  FTP  "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%s%.*s",tstr,sizeof(logline)-2,str);
	truncsp(logline);
	printf("%s\n",logline);
	
    return(strlen(logline)+1);
}

static void ftp_started(void)
{
	ftp_lputs("ftp_started");
	ftp_running=TRUE;
}

static void ftp_terminated(int code)
{
	ftp_lputs("ftp_terminated");
	ftp_running=FALSE;
}

/****************************************************************************/
/* Mail Server local/log print routine										*/
/****************************************************************************/
static int mail_lputs(char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm*	tm_p;

	t=time(NULL);
	tm_p=localtime(&t);
	if(tm_p==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d  MAIL "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%s%.*s",tstr,sizeof(logline)-2,str);
	truncsp(logline);
	printf("%s\n",logline);
	
    return(strlen(logline)+1);
}

static void mail_started(void)
{
	mail_lputs("mail_started");
	mail_running=TRUE;
}

static void mail_terminated(int code)
{
	mail_lputs("mail_terminated");
	mail_running=FALSE;
}

/****************************************************************************/
/* Main Entry Point															*/
/****************************************************************************/
int main(int argc, char** argv)
{
	char*	ctrl_dir;

	printf("\nSynchronet BBS Console Version %s  Copyright 2000 Rob Swindell\n"
		,SBBSCON_VERSION);

	ctrl_dir=getenv("SBBSCTRL");	/* read from environment variable */
	if(ctrl_dir==NULL)
		ctrl_dir="/sbbs/ctrl/";		/* Not set? Use default */

	/* Initialize BBS startup structure */
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);
    bbs_startup.first_node=1;
    bbs_startup.last_node=4;
    bbs_startup.telnet_port=IPPORT_TELNET;
    bbs_startup.telnet_interface=INADDR_ANY;

#ifdef USE_RLOGIN
    bbs_startup.rlogin_port=513;
    bbs_startup.rlogin_interface=INADDR_ANY;
	bbs_startup.options|=BBS_OPT_ALLOW_RLOGIN;
#endif

	bbs_startup.lputs=bbs_lputs;
    bbs_startup.started=bbs_started;
    bbs_startup.terminated=bbs_terminated;
/*	These callbacks haven't been created yet
    bbs_startup.status=bbs_status;
    bbs_startup.clients=bbs_clients;
    bbs_startup.thread_up=thread_up;
    bbs_startup.client_on=client_on;
    bbs_startup.socket_open=socket_open;
*/
    strcpy(bbs_startup.ctrl_dir,ctrl_dir);

	/* Initialize FTP startup structure */
    memset(&ftp_startup,0,sizeof(ftp_startup));
    ftp_startup.size=sizeof(ftp_startup);
    ftp_startup.port=IPPORT_FTP;
    ftp_startup.interface_addr=INADDR_ANY;
	ftp_startup.lputs=ftp_lputs;
    ftp_startup.started=ftp_started;
    ftp_startup.terminated=ftp_terminated;
    strcpy(ftp_startup.ctrl_dir,ctrl_dir);

	/* Initialize Mail Server startup structure */
    memset(&mail_startup,0,sizeof(mail_startup));
    mail_startup.size=sizeof(mail_startup);
    mail_startup.smtp_port=IPPORT_SMTP;
    mail_startup.pop3_port=110;
    mail_startup.max_clients=10;
    mail_startup.max_inactivity=120;
    mail_startup.max_delivery_attempts=10;
    mail_startup.rescan_frequency=300;	/* In seconds */
    mail_startup.relay_port=IPPORT_SMTP;
    mail_startup.interface_addr=INADDR_ANY;
	mail_startup.lputs=mail_lputs;
    strcpy(mail_startup.ctrl_dir,ctrl_dir);

	_beginthread(bbs_thread,0,&bbs_startup);
	_beginthread(ftp_server,0,&ftp_startup);
//	_beginthread(mail_server,0,&mail_startup);

	while(1) {
		if(kbhit())
			break;
		mswait(1);
	}

	bbs_terminate();
	ftp_terminate();
//	mail_terminate();

	printf("\nWaiting for servers to terminate...\n");

	while(bbs_running || ftp_running || mail_running)
		mswait(1);

	printf("\nAll servers terminated\n");

	return(0);
}