/* sbbsctrl.c */

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
#include "bbs_thrd.h"	/* bbs_thread */

/* Global variables */
bbs_startup_t	bbs_startup;

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
/* Local print routine														*/
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
		sprintf(tstr,"%d/%d %02d:%02d:%02d  "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%s%.*s",tstr,sizeof(logline)-2,str);
	truncsp(logline);
	printf("%s\n",logline);
	
    return(strlen(logline)+1);
}


/****************************************************************************/
/* Main Entry Point															*/
/****************************************************************************/
int main(int argc, char** argv)
{
	char*	ctrl_dir;

	/* Initialize startup structure */
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

/*	These callbacks haven't been created yet
    bbs_startup.status=bbs_status;
    bbs_startup.clients=bbs_clients;
    bbs_startup.started=bbs_started;
    bbs_startup.terminated=bbs_terminated;
    bbs_startup.thread_up=thread_up;
    bbs_startup.client_on=client_on;
    bbs_startup.socket_open=socket_open;
*/

	ctrl_dir=getenv("SBBSCTRL");	/* read from environment variable */
	if(ctrl_dir==NULL)
		ctrl_dir="/sbbs/ctrl";		/* Not set? Use default */

    strcpy(bbs_startup.ctrl_dir,ctrl_dir);

	bbs_thread(&bbs_startup);

	return(0);
}