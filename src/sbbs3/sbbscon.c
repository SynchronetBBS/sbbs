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
#include "conwrap.h"	/* kbhit/getch */
#include "sbbs.h"
#include "ftpsrvr.h"	/* ftp_startup_t, ftp_server */
#include "mailsrvr.h"	/* mail_startup_t, mail_server */
#include "services.h"	/* services_startup_t, services_thread */

/* Constants */
#define SBBSCON_VERSION		"1.10"

/* Global variables */
BOOL				bbs_running=FALSE;
bbs_startup_t		bbs_startup;
BOOL				ftp_running=FALSE;
ftp_startup_t		ftp_startup;
BOOL				mail_running=FALSE;
mail_startup_t		mail_startup;
BOOL				services_running=FALSE;
services_startup_t	services_startup;

static const char* prompt = "Command (?=Help): ";

static const char* usage  = "\nusage: %s [[option] [...]]\n"
							"\noptions:\n\n"
							"defaults   show default settings\n"
							"tf<node>   set First Telnet Node Number\n"
							"tl<node>   set Last Telnet Node Number\n"
							"tp<port>   set Telnet Server Port\n"
							"rp<port>   set RLogin Server Port (and enable RLogin Server)\n"
							"fp<port>   set FTP Server Port\n"
							"sp<port>   set SMTP Server Port\n"
							"sr<port>   set SMTP Relay Port\n"
							"pp<port>   set POP3 Server Port\n"
							"\n"
							;

static void lputs(char *str)
{
	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;

	if(!mutex_initialized) {
		pthread_mutex_init(&mutex,NULL);
		mutex_initialized=TRUE;
	}

	pthread_mutex_lock(&mutex);
	printf("\r%*s\r%s\n",strlen(prompt),"",str);
	printf(prompt);
	fflush(stdout);
	pthread_mutex_unlock(&mutex);
}

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
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%sbbs  %.*s",tstr,sizeof(logline)-2,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

static void bbs_started(void)
{
	bbs_running=TRUE;
}

static void bbs_terminated(int code)
{
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
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%sftp  %.*s",tstr,sizeof(logline)-2,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

static void ftp_started(void)
{
	ftp_running=TRUE;
}

static void ftp_terminated(int code)
{
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
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%smail %.*s",tstr,sizeof(logline)-2,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

static void mail_started(void)
{
	mail_running=TRUE;
}

static void mail_terminated(int code)
{
	mail_running=FALSE;
}

/****************************************************************************/
/* Services local/log print routine											*/
/****************************************************************************/
static int services_lputs(char *str)
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
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%ssvc  %.*s",tstr,sizeof(logline)-2,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

static void services_started(void)
{
	services_running=TRUE;
}

static void services_terminated(int code)
{
	services_running=FALSE;
}


#ifdef __unix__
void _sighandler_quit(int sig)
{
        // Close threads
        bbs_terminate();
        ftp_terminate();
        mail_terminate();
        while(bbs_running || ftp_running || mail_running)
        mswait(1);

        exit(0);
}
#endif


/****************************************************************************/
/* Main Entry Point															*/
/****************************************************************************/
int main(int argc, char** argv)
{
	int		i;
	char	ch;
	char*	arg;
	char*	ctrl_dir;
	BOOL	quit=FALSE;

	printf("\nSynchronet BBS Console Version %s  Copyright 2001 Rob Swindell\n"
		,SBBSCON_VERSION);

	ctrl_dir=getenv("SBBSCTRL");	/* read from environment variable */
	if(ctrl_dir==NULL)
		ctrl_dir="/sbbs/ctrl/";		/* Not set? Use default */

	/* Initialize BBS startup structure */
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);
    bbs_startup.first_node=1;
    bbs_startup.last_node=4;
    bbs_startup.telnet_interface=INADDR_ANY;
	bbs_startup.options|=BBS_OPT_NO_QWK_EVENTS;

#ifdef USE_RLOGIN
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
	ftp_startup.lputs=ftp_lputs;
    ftp_startup.started=ftp_started;
    ftp_startup.terminated=ftp_terminated;
	ftp_startup.options=FTP_OPT_INDEX_FILE|FTP_OPT_ALLOW_QWK;
    strcpy(ftp_startup.index_file_name,"00index");
    strcpy(ftp_startup.ctrl_dir,ctrl_dir);

	/* Initialize Mail Server startup structure */
    memset(&mail_startup,0,sizeof(mail_startup));
    mail_startup.size=sizeof(mail_startup);
	mail_startup.lputs=mail_lputs;
    mail_startup.started=mail_started;
    mail_startup.terminated=mail_terminated;
	mail_startup.options|=MAIL_OPT_ALLOW_POP3;
	/* Spam filtering */
	mail_startup.options|=MAIL_OPT_USE_RBL;	/* Realtime Blackhole List */
	mail_startup.options|=MAIL_OPT_USE_RSS;	/* Relay Spam Stopper */
    strcpy(mail_startup.ctrl_dir,ctrl_dir);

#ifdef __unix__	/* Look up DNS server address */
	{
		FILE*	fp;
		char*	p;
		char	str[128];

		if((fp=fopen("/etc/resolv.conf","r"))!=NULL) {
			while(!feof(fp)) {
				if(fgets(str,sizeof(str),fp)==NULL)
					break;
				truncsp(str);
				p=str;
				while(*p && *p<=' ') p++;	/* skip white-space */
				if(strnicmp(p,"nameserver",10)!=0) /* no match */
					continue;
				p+=10;	/* skip "nameserver" */
				while(*p && *p<=' ') p++;	/* skip more white-space */
				sprintf(mail_startup.dns_server,"%.*s"
					,sizeof(mail_startup.dns_server)-1,p);
				break;
			}
			fclose(fp);
		}
	}
#endif /* __unix__ */

	/* Initialize Services startup structure */
    memset(&services_startup,0,sizeof(services_startup));
    services_startup.size=sizeof(services_startup);
	services_startup.lputs=services_lputs;
    services_startup.started=services_started;
    services_startup.terminated=services_terminated;
    strcpy(services_startup.ctrl_dir,ctrl_dir);

	/* Process arguments */
	for(i=1;i<argc;i++) {
		arg=argv[i];
		if(*arg=='-')/* ignore prepended slashes */
			arg++;
		if(!stricmp(arg,"defaults")) {
			printf("default settings\n");
			return(0);
		}
		switch(toupper(*(arg++))) {
			case 'T':	/* Telnet settings */
				switch(toupper(*(arg++))) {
					case 'P':
						bbs_startup.telnet_port=atoi(arg);
						break;
					case 'F':
						bbs_startup.first_node=atoi(arg);
						break;
					case 'L':
						bbs_startup.last_node=atoi(arg);
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'R':	/* RLogin */
				bbs_startup.options|=BBS_OPT_ALLOW_RLOGIN;
				switch(toupper(*(arg++))) {
					case 'P':
						bbs_startup.rlogin_port=atoi(arg);
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'F':	/* FTP */
				switch(toupper(*(arg++))) {
					case 'P':
						ftp_startup.port=atoi(arg);
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'S':	/* SMTP */
				switch(toupper(*(arg++))) {
					case 'P':
						mail_startup.smtp_port=atoi(arg);
						break;
					case 'R':
					    mail_startup.relay_port=atoi(arg);
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'P':	/* POP3 */
				switch(toupper(*(arg++))) {
					case 'P':
						mail_startup.pop3_port=atoi(arg);
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			default:
				printf(usage,argv[0]);
				return(0);
		}
	}

	_beginthread((void(*)(void*))bbs_thread,0,&bbs_startup);
	_beginthread((void(*)(void*))ftp_server,0,&ftp_startup);
	_beginthread((void(*)(void*))mail_server,0,&mail_startup);
	_beginthread((void(*)(void*))services_thread,0,&services_startup);

#ifdef __unix__
	// Set up QUIT-type signals so they clean up properly.
	signal(SIGHUP, _sighandler_quit);
	signal(SIGINT, _sighandler_quit);
	signal(SIGQUIT, _sighandler_quit);
	signal(SIGABRT, _sighandler_quit);
	signal(SIGTERM, _sighandler_quit);
#endif

	if(!isatty(fileno(stdin)))			/* redirected */
		select(0,NULL,NULL,NULL,NULL);	/* so wait here until signaled */
	else								/* interactive */
		while(!quit) {
			ch=getch();
			printf("%c\n",ch);
			switch(ch) {
				case 'q':
					quit=TRUE;
					break;
				default:
					printf("\nSynchronet BBS Console Version %s Help\n\n",SBBSCON_VERSION);
					printf("q   = quit\n");
#if 0	/* to do */	
					printf("n   = node list\n");
					printf("w   = who's online\n");
					printf("l#  = lock node #\n");
					printf("d#  = down node #\n");
					printf("i#  = interrupt node #\n");
					printf("c#  = chat with node #\n");
					printf("s#  = spy on node #\n");
#endif
					lputs("");	/* redisplay prompt */
					break;
			}
		}

	bbs_terminate();
	ftp_terminate();
	mail_terminate();
	services_terminate();

	while(bbs_running || ftp_running || mail_running || services_running)
		mswait(1);

	/* erase the prompt */
	printf("\r%*s\r",strlen(prompt),"");

	return(0);
}
