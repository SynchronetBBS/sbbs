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

/* ANSI headers */
#include <string.h>
#include <signal.h>
#include <ctype.h>

/* Synchronet-specific headers */
#include "conwrap.h"	/* kbhit/getch */
#include "dirwrap.h"	/* BACKSLASH */
#include "startup.h"	/* bbs_startup_t, bbs_thread */
#include "ftpsrvr.h"	/* ftp_startup_t, ftp_server */
#include "mailsrvr.h"	/* mail_startup_t, mail_server */
#include "services.h"	/* services_startup_t, services_thread */
#include "ini_file.h"
#include "sbbs_ini.h"
#include "sbbsdefs.h"	/* VERSION, REVISION, and COPYRIGHT_NOTICE */

#ifdef __unix__

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdlib.h>  /* Is this included from somewhere else? */

#endif

/* Constants */
#define SBBS_PID_FILE	"/var/run/sbbs.pid"
#define SBBS_LOG_NAME	"synchronet"

/* Global variables */
BOOL				run_bbs=TRUE;
BOOL				bbs_running=FALSE;
BOOL				bbs_stopped=FALSE;
bbs_startup_t		bbs_startup;
BOOL				run_ftp=TRUE;
BOOL				ftp_running=FALSE;
BOOL				ftp_stopped=FALSE;
ftp_startup_t		ftp_startup;
BOOL				run_mail=TRUE;
BOOL				mail_running=FALSE;
BOOL				mail_stopped=FALSE;
mail_startup_t		mail_startup;
BOOL				run_services=TRUE;
BOOL				services_running=FALSE;
BOOL				services_stopped=FALSE;
services_startup_t	services_startup;
uint				thread_count=1;
uint				socket_count=0;
uint				client_count=0;
ulong				served=0;
int					prompt_len=0;

#ifdef __unix__
char*				new_uid_name=NULL;
char*				new_gid_name=NULL;
uid_t				new_uid;
uid_t				old_uid;
gid_t				new_gid;
gid_t				old_gid;
BOOL				is_daemon=FALSE;
BOOL				use_facilities=FALSE;
#endif

static const char* prompt = 
"[Threads: %d  Sockets: %d  Clients: %d  Served: %lu] (?=Help): ";

static const char* usage  = "usage: %s [[setting] [...]]\n"
							"\n"
							"Telnet server settings:\n\n"
							"\ttf<node>   set first Telnet node number\n"
							"\ttl<node>   set last Telnet node number\n"
							"\ttp<port>   set Telnet server port\n"
							"\trp<port>   set RLogin server port (and enable RLogin server)\n"
							"\tr2         use second RLogin name in BSD RLogin\n"
							"\tto<value>  set Telnet server options value (advanced)\n"
							"\tta         enable auto-logon via IP address\n"
							"\ttd         enable Telnet command debug output\n"
							"\ttc         emabble sysop availability for chat\n"
							"\ttq         disable QWK events\n"
							"\n"
							"FTP server settings:\n"
							"\n"
							"\tfp<port>   set FTP server port\n"
							"\tfo<value>  set FTP server options value (advanced)\n"
							"\tf-         disable FTP server\n"
							"\n"
							"Mail server settings:\n"
							"\n"
							"\tms<port>   set SMTP server port\n"
							"\tmp<port>   set POP3 server port\n"
							"\tmr<addr>   set SMTP relay server (and enable SMTP relay)\n"
							"\tmd<addr>   set DNS server address for MX-record lookups\n"
							"\tmo<value>  set Mail server options value (advanced)\n"
							"\tma         allow SMTP relays from authenticated users\n"
							"\tm-         disable Mail server (entirely)\n"
							"\tmp-        disable POP3 server\n"
							"\tms-        disable SendMail thread\n"
							"\n"
							"Services settings:\n"
							"\n"
							"\tso<value>  set Services option value (advanced)\n"
							"\ts-         disable Services (no services module)\n"
							"\n"
							"Global settings:\n"
							"\n"
							"\thn[host]   set hostname for this instance\n"
							"\t           if host not specified, uses gethostname\n"
#ifdef __unix__
							"\tun<user>   set username for BBS to run as\n"
							"\tug<group>  set group for BBS to run as\n"
							"\td[x]       run as daemon, log using syslog\n"
							"\t           x is the optional LOCALx facility to use\n"
							"\t           if none is specified, uses USER\n"
							"\t           if 'f' is specified, uses standard facilities\n"
#endif
							"\tgi         get user identity (using IDENT protocol)\n"
							"\tnh         disable hostname lookups\n"
							"\tnj         disable JavaScript support\n"
							"\tni         do not read settings from .ini file\n"
							"\tlt         use local timezone (do not force UTC/GMT)\n"
							"\tdefaults   show default settings and options\n"
							;

static int lputs(char *str)
{
	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;

#ifdef __unix__

	if (is_daemon)  {
		if(str!=NULL) {
			if (use_facilities)
				syslog(LOG_INFO|LOG_AUTH,"%s",str);
			else
				syslog(LOG_INFO,"%s",str);
		}
		return(0);
	}
#endif
	if(!mutex_initialized) {
		pthread_mutex_init(&mutex,NULL);
		mutex_initialized=TRUE;
	}
	pthread_mutex_lock(&mutex);
	/* erase prompt */
	printf("\r%*s\r",prompt_len,"");
	if(str!=NULL)
		printf("%s\n",str);
	/* re-display prompt with current stats */
	prompt_len = printf(prompt, thread_count, socket_count, client_count, served);
	fflush(stdout);
	pthread_mutex_unlock(&mutex);

    return(prompt_len);
}

#ifdef __unix__
/**********************************************************
* Change uid of the calling process to the user if specified
* **********************************************************/
static BOOL do_setuid(void) 
{
	BOOL	result=FALSE;

	setregid(-1,old_gid);
	setreuid(-1,old_uid);
	if(!setregid(new_gid,new_gid) && !setreuid(new_uid,new_uid)) 
		result=TRUE;

	if(!result) {
		lputs("!setuid FAILED");
		lputs(strerror(errno));
	}
	return result;
}

/**********************************************************
* Change uid of the calling process to the user if specified
* **********************************************************/
static BOOL do_seteuid(BOOL to_new) 
{
	BOOL	result=FALSE;
	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;

	if(new_uid_name==NULL)	/* not set? */
		return(TRUE);		/* do nothing */

	if(!mutex_initialized) {
		pthread_mutex_init(&mutex,NULL);
		mutex_initialized=TRUE;
	}

	pthread_mutex_lock(&mutex);

	if(to_new)
		if(!setregid(-1,new_gid) && !setreuid(-1,new_uid))
			result=TRUE;
		else
			result=FALSE;
	else
		if(!setregid(-1,old_gid) && !setreuid(-1,old_uid))
			result=TRUE;
		else
			result=FALSE;

		
	pthread_mutex_unlock(&mutex);

	if(!result) {
		lputs("!seteuid FAILED");
		lputs(strerror(errno));
	}
	return result;
}
#endif   /* __unix__ */

static void thread_up(BOOL up, BOOL setuid)
{
   	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;

#ifdef _THREAD_SUID_BROKEN
	if(up && setuid) {
		do_seteuid(FALSE);
		do_setuid();
	}
#endif

	if(!mutex_initialized) {
		pthread_mutex_init(&mutex,NULL);
		mutex_initialized=TRUE;
	}

	pthread_mutex_lock(&mutex);
	if(up)
	    thread_count++;
    else if(thread_count>0)
    	thread_count--;
	pthread_mutex_unlock(&mutex);
	lputs(NULL); /* update displayed stats */
}

static void socket_open(BOOL open)
{
   	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;

	if(!mutex_initialized) {
		pthread_mutex_init(&mutex,NULL);
		mutex_initialized=TRUE;
	}

	pthread_mutex_lock(&mutex);
	if(open)
	    socket_count++;
    else if(socket_count>0)
    	socket_count--;
	pthread_mutex_unlock(&mutex);
	lputs(NULL); /* update displayed stats */
}

static void client_on(BOOL on, int sock, client_t* client, BOOL update)
{
   	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;

	if(!mutex_initialized) {
		pthread_mutex_init(&mutex,NULL);
		mutex_initialized=TRUE;
	}

	pthread_mutex_lock(&mutex);
	if(on && !update) {
		client_count++;
	    served++;
	} else if(!on && client_count>0)
		client_count--;
	pthread_mutex_unlock(&mutex);
	lputs(NULL); /* update displayed stats */
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
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

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (use_facilities)
			syslog(LOG_INFO|LOG_AUTH,"%s",str);
		else
			syslog(LOG_INFO,"     %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	tm_p=localtime(&t);
	if(tm_p==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%s     %.*s",tstr,(int)sizeof(logline)-32,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

static void bbs_started(void)
{
	bbs_running=TRUE;
	bbs_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid();
	#endif
}

static void bbs_terminated(int code)
{
	bbs_running=FALSE;
	bbs_stopped=TRUE;
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

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (use_facilities)
			syslog(LOG_INFO|LOG_FTP,"%s",str);
		else
			syslog(LOG_INFO,"ftp  %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	tm_p=localtime(&t);
	if(tm_p==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%sftp  %.*s",tstr,(int)sizeof(logline)-32,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

static void ftp_started(void)
{
	ftp_running=TRUE;
	ftp_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid();
	#endif
}

static void ftp_terminated(int code)
{
	ftp_running=FALSE;
	ftp_stopped=TRUE;
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

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (use_facilities)
			syslog(LOG_INFO|LOG_MAIL,"%s",str);
		else
			syslog(LOG_INFO,"mail %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	tm_p=localtime(&t);
	if(tm_p==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%smail %.*s",tstr,(int)sizeof(logline)-32,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

static void mail_started(void)
{
	mail_running=TRUE;
	mail_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid();
	#endif
}

static void mail_terminated(int code)
{
	mail_running=FALSE;
	mail_stopped=TRUE;
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

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (use_facilities)
			syslog(LOG_INFO|LOG_DAEMON,"%s",str);
		else
			syslog(LOG_INFO,"srvc %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	tm_p=localtime(&t);
	if(tm_p==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%ssrvc %.*s",tstr,(int)sizeof(logline)-32,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

static void services_started(void)
{
	services_running=TRUE;
	services_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid();
	#endif
}

static void services_terminated(int code)
{
	services_running=FALSE;
	services_stopped=TRUE;
}

/****************************************************************************/
/* Event thread local/log print routine										*/
/****************************************************************************/
static int event_lputs(char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm*	tm_p;

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (use_facilities)
			syslog(LOG_INFO|LOG_CRON,"%s",str);
		else
			syslog(LOG_INFO,"evnt %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	tm_p=localtime(&t);
	if(tm_p==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm_p->tm_mon+1,tm_p->tm_mday
			,tm_p->tm_hour,tm_p->tm_min,tm_p->tm_sec);

	sprintf(logline,"%sevnt %.*s",tstr,(int)sizeof(logline)-32,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}



#ifdef __unix__
void _sighandler_quit(int sig)
{
    // Close threads
    bbs_terminate();
    ftp_terminate();
    mail_terminate();
#ifdef JAVASCRIPT
	services_terminate();
#endif
    while(bbs_running || ftp_running || mail_running || services_running)
		mswait(1);
	if(is_daemon)
		unlink(SBBS_PID_FILE);

    exit(0);
}

#if 0 // Apparently, Linux hasa daemon call too!
#ifndef __FreeBSD__
/****************************************************************************/
/* Daemonizes the process													*/
/****************************************************************************/
int
daemon(nochdir, noclose)
	int nochdir, noclose;
{
	int fd;

	switch (fork()) {
	case -1:
		return (-1);
	case 0:
		break;
	default:
		_exit(0);
	}

	if (setsid() == -1)
		return (-1);

	if (!nochdir)
		(void)chdir("/");

	if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
		(void)dup2(fd, STDIN_FILENO);
		(void)dup2(fd, STDOUT_FILENO);
		(void)dup2(fd, STDERR_FILENO);
		if (fd > 2)
			(void)close(fd);
	}
	return (0);
}
#endif	/* !__FreeBSD__ */
#endif /* 0 */

#endif	/* __unix__ */

/****************************************************************************/
/* Main Entry Point															*/
/****************************************************************************/
int main(int argc, char** argv)
{
	int		i;
	char	ch;
	char*	p;
	char*	arg;
	char*	ctrl_dir;
	char	str[MAX_PATH+1];
	char	ini_file[MAX_PATH+1];
	BOOL	quit=FALSE;
	FILE*	fp=NULL;
#ifdef __unix__
	FILE *pidfile;
	struct passwd* pw_entry;
	struct group*  gr_entry;
#endif

	printf("\nSynchronet Console for %s  Version %s%c  %s\n\n"
		,PLATFORM_DESC,VERSION,REVISION,COPYRIGHT_NOTICE);

	ctrl_dir=getenv("SBBSCTRL");	/* read from environment variable */
	if(ctrl_dir==NULL)
		ctrl_dir="/sbbs/ctrl";		/* Not set? Use default */

	sprintf(ini_file,"%s%csbbs.ini",ctrl_dir,BACKSLASH);

	/* Initialize BBS startup structure */
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);

	bbs_startup.lputs=bbs_lputs;
	bbs_startup.event_log=event_lputs;
    bbs_startup.started=bbs_started;
    bbs_startup.terminated=bbs_terminated;
    bbs_startup.thread_up=thread_up;
    bbs_startup.socket_open=socket_open;
    bbs_startup.client_on=client_on;
#ifdef __unix__
	bbs_startup.seteuid=do_seteuid;
#endif
/*	These callbacks haven't been created yet
    bbs_startup.status=bbs_status;
    bbs_startup.clients=bbs_clients;
*/
    strcpy(bbs_startup.ctrl_dir,ctrl_dir);

	/* Initialize FTP startup structure */
    memset(&ftp_startup,0,sizeof(ftp_startup));
    ftp_startup.size=sizeof(ftp_startup);
	ftp_startup.lputs=ftp_lputs;
    ftp_startup.started=ftp_started;
    ftp_startup.terminated=ftp_terminated;
	ftp_startup.thread_up=thread_up;
    ftp_startup.socket_open=socket_open;
    ftp_startup.client_on=client_on;
#ifdef __unix__
	ftp_startup.seteuid=do_seteuid;
#endif
    strcpy(ftp_startup.index_file_name,"00index");
    strcpy(ftp_startup.ctrl_dir,ctrl_dir);

	/* Initialize Mail Server startup structure */
    memset(&mail_startup,0,sizeof(mail_startup));
    mail_startup.size=sizeof(mail_startup);
	mail_startup.lputs=mail_lputs;
    mail_startup.started=mail_started;
    mail_startup.terminated=mail_terminated;
	mail_startup.thread_up=thread_up;
    mail_startup.socket_open=socket_open;
    mail_startup.client_on=client_on;
#ifdef __unix__
	mail_startup.seteuid=do_seteuid;
#endif
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
				SAFECOPY(mail_startup.dns_server,p);
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
	services_startup.thread_up=thread_up;
    services_startup.socket_open=socket_open;
    services_startup.client_on=client_on;
#ifdef __unix__
	services_startup.seteuid=do_seteuid;
#endif
    strcpy(services_startup.ctrl_dir,ctrl_dir);

	/* Pre-INI command-line switches */
	for(i=1;i<argc;i++) {
		arg=argv[i];
		while(*arg=='-')
			arg++;
		if(strchr(arg,BACKSLASH)) {
			strcpy(ini_file,arg);
			continue;
		}
		if(!stricmp(arg,"ni")) {
			ini_file[0]=0;
			break;
		}
	}

	/* Read .ini file here */
	if(ini_file[0]!=0 && (fp=fopen(ini_file,"r"))!=NULL) {
		sprintf(str,"Reading %s",ini_file);
		bbs_lputs(str);
	}

	/* We call this function to set defaults, even if there's no .ini file */
	sbbs_read_ini(fp, 
		&run_bbs,		&bbs_startup,
		&run_ftp,		&ftp_startup, 
		&run_mail,		&mail_startup, 
		&run_services,	&services_startup);

	/* read any sbbscon-specific ini keys here */

	if(fp!=NULL)
		fclose(fp);

	/* Post-INI command-line switches */
	for(i=1;i<argc;i++) {
		arg=argv[i];
		while(*arg=='-')
			arg++;
		if(!stricmp(arg,"defaults")) {
			printf("Default settings:\n");
			printf("\n");
			printf("Telnet server port:\t%u\n",bbs_startup.telnet_port);
			printf("Telnet first node:\t%u\n",bbs_startup.first_node);
			printf("Telnet last node:\t%u\n",bbs_startup.last_node);
			printf("Telnet server options:\t0x%08lX\n",bbs_startup.options);
			printf("FTP server port:\t%u\n",ftp_startup.port);
			printf("FTP server options:\t0x%08lX\n",ftp_startup.options);
			printf("Mail SMTP server port:\t%u\n",mail_startup.smtp_port);
			printf("Mail SMTP relay port:\t%u\n",mail_startup.relay_port);
			printf("Mail POP3 server port:\t%u\n",mail_startup.pop3_port);
			printf("Mail server options:\t0x%08lX\n",mail_startup.options);
			printf("Services options:\t0x%08lX\n",services_startup.options);
			return(0);
		}
		switch(toupper(*(arg++))) {
#ifdef __unix__
				case 'D': /* Run as daemon */
					printf("Running as daemon\n");
					if(!daemon(TRUE,FALSE))  { /* Daemonize, DON'T switch to / and DO close descriptors */
						is_daemon=TRUE;
						switch(toupper(*(arg++))) {
							case '0':
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_LOCAL0);
								break;
							case '1':
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_LOCAL1);
								break;
							case '2':
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_LOCAL2);
								break;
							case '3':
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_LOCAL3);
								break;
							case '4':
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_LOCAL4);
								break;
							case '5':
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_LOCAL5);
								break;
							case '6':
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_LOCAL6);
								break;
							case '7':
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_LOCAL7);
								break;
							case 'F':
								/* Use appropriate facilities */
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_USER);
								use_facilities = TRUE;
								break;
							default:
								openlog(SBBS_LOG_NAME,LOG_CONS,LOG_USER);
						}
					}
				break;
#endif
			case 'T':	/* Telnet settings */
				switch(toupper(*(arg++))) {
					case 'D': /* debug output */
						bbs_startup.options|=BBS_OPT_DEBUG_TELNET;
						break;
					case 'A': /* Auto-logon via IP */
						bbs_startup.options|=BBS_OPT_AUTO_LOGON;
						break;
					case 'Q': /* No QWK events */
						bbs_startup.options|=BBS_OPT_NO_QWK_EVENTS;
						break;
					case 'C': /* Sysop available for chat */
						bbs_startup.options|=BBS_OPT_SYSOP_AVAILABLE;
						break;
					case 'O': /* Set options */
						bbs_startup.options=strtoul(arg,NULL,0);
						break;
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
					case '2':
						bbs_startup.options|=BBS_OPT_USE_2ND_RLOGIN;
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'F':	/* FTP */
				switch(toupper(*(arg++))) {
					case '-':	
						run_ftp=FALSE;
						break;
					case 'P':
						ftp_startup.port=atoi(arg);
						break;
					case 'O': /* Set options */
						ftp_startup.options=strtoul(arg,NULL,0);
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'M':	/* Mail */
				switch(toupper(*(arg++))) {
					case '-':
						run_mail=FALSE;
						break;
					case 'O': /* Set options */
						mail_startup.options=strtoul(arg,NULL,0);
						break;
					case 'S':	/* SMTP/SendMail */
						if(isdigit(*arg)) {
							mail_startup.smtp_port=atoi(arg);
							break;
						}
						switch(toupper(*(arg++))) {
							case '-':
								mail_startup.options|=MAIL_OPT_NO_SENDMAIL;
								break;
							default:
								printf(usage,argv[0]);
								return(0);
						}
						break;
					case 'P':	/* POP3 */
						if(isdigit(*arg)) {
							mail_startup.pop3_port=atoi(arg);
							break;
						}
						switch(toupper(*(arg++))) {
							case '-':
								mail_startup.options&=~MAIL_OPT_ALLOW_POP3;
								break;
							default:
								printf(usage,argv[0]);
								return(0);
						}
						break;
					case 'R':	/* Relay */
						mail_startup.options|=MAIL_OPT_RELAY_TX;
						p=strchr(arg,':');	/* port specified */
						if(p!=NULL) {
							*p=0;
							mail_startup.relay_port=atoi(p+1);
						}
						SAFECOPY(mail_startup.relay_server,arg);
						break;
					case 'D':	/* DNS Server */
						SAFECOPY(mail_startup.dns_server,arg);
						break;
					case 'A':
						mail_startup.options|=MAIL_OPT_ALLOW_RELAY;
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'S':	/* Services */
				switch(toupper(*(arg++))) {
					case '-':
						run_services=FALSE;
						break;
					case 'O': /* Set options */
						services_startup.options=strtoul(arg,NULL,0);
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'G':	/* GET */
				switch(toupper(*(arg++))) {
					case 'I': /* Identity */
						bbs_startup.options|=BBS_OPT_GET_IDENT;
						ftp_startup.options|=BBS_OPT_GET_IDENT;
						mail_startup.options|=BBS_OPT_GET_IDENT;
						services_startup.options|=BBS_OPT_GET_IDENT;
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'H':	/* Host */
				switch(toupper(*(arg++))) {
					case 'N':	/* Name */
						if(*arg) {
							SAFECOPY(bbs_startup.host_name,arg);
							SAFECOPY(ftp_startup.host_name,arg);
							SAFECOPY(mail_startup.host_name,arg);
							SAFECOPY(services_startup.host_name,arg);
						} else {
							gethostname(bbs_startup.host_name
								,sizeof(bbs_startup.host_name)-1);
							gethostname(ftp_startup.host_name
								,sizeof(ftp_startup.host_name)-1);
							gethostname(mail_startup.host_name
								,sizeof(mail_startup.host_name)-1);
							gethostname(services_startup.host_name
								,sizeof(services_startup.host_name)-1);
						}
						printf("Setting hostname: %s\n",bbs_startup.host_name);
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'U':	/* runtime UID */
				switch(toupper(*(arg++))) {
					case 'N': /* username */
#ifdef __unix__
						if(new_gid_name!=NULL) {
							printf("!Must specify user before group");
							break;
						}
						if(strlen(arg) > 1)
						{
							new_uid_name=arg;
							old_uid = getuid();
							if((pw_entry=getpwnam(new_uid_name))!=0)
							{
								new_uid=pw_entry->pw_uid;
								new_gid=pw_entry->pw_gid;
								do_seteuid(TRUE);
							}
							else  {
								new_uid=getuid();
								new_gid=getgid();
							}
						}
#endif			
						break;
					case 'G': /* groupname */
#ifdef __unix__
						if(strlen(arg) > 1)
						{
							new_gid_name=arg;
							old_gid = getgid();
							if((gr_entry=getgrnam(new_gid_name))!=0 && (new_gid=gr_entry->gr_gid)!=0)
								do_seteuid(TRUE);
						}
#endif			
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'N':	/* No */
				switch(toupper(*(arg++))) {
					case 'F':	/* FTP Server */
						run_ftp=FALSE;
						break;
					case 'M':	/* Mail Server */
						run_mail=FALSE;
						break;
					case 'S':	/* Services */
						run_services=FALSE;
						break;
					case 'Q': /* No QWK events */
						bbs_startup.options		|=BBS_OPT_NO_QWK_EVENTS;
						break;
					case 'H':	/* Hostname lookup */
						bbs_startup.options		|=BBS_OPT_NO_HOST_LOOKUP;
						ftp_startup.options		|=BBS_OPT_NO_HOST_LOOKUP;
						mail_startup.options	|=BBS_OPT_NO_HOST_LOOKUP;
						services_startup.options|=BBS_OPT_NO_HOST_LOOKUP;
						break;
					case 'J':	/* JavaScript */
						bbs_startup.options		|=BBS_OPT_NO_JAVASCRIPT;
						ftp_startup.options		|=BBS_OPT_NO_JAVASCRIPT;
						mail_startup.options	|=BBS_OPT_NO_JAVASCRIPT;
						services_startup.options|=BBS_OPT_NO_JAVASCRIPT;
						break;
					case 'I':	/* no .ini file */
						break;
					default:
						printf(usage,argv[0]);
						return(0);
				}
				break;
			case 'L':	/* Local */
				switch(toupper(*(arg++))) {
					case 'T': /* timezone */
						bbs_startup.options		|=BBS_OPT_LOCAL_TIMEZONE;
						ftp_startup.options		|=BBS_OPT_LOCAL_TIMEZONE;
						mail_startup.options	|=BBS_OPT_LOCAL_TIMEZONE;
						services_startup.options|=BBS_OPT_LOCAL_TIMEZONE;
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

#ifdef __unix__
	/* Write the standard .pid file if running as a daemon */
	if(is_daemon)  {
		pidfile=fopen(SBBS_PID_FILE,"w");
		if(pidfile!=NULL) {
			fprintf(pidfile,"%d",getpid());
			fclose(pidfile);
		}
	}

#endif

	if(run_bbs)
		_beginthread((void(*)(void*))bbs_thread,0,&bbs_startup);
	if(run_ftp)
		_beginthread((void(*)(void*))ftp_server,0,&ftp_startup);
	if(run_mail)
		_beginthread((void(*)(void*))mail_server,0,&mail_startup);
#ifdef JAVASCRIPT
	if(run_services)
		_beginthread((void(*)(void*))services_thread,0,&services_startup);
#endif

#ifdef __unix__
	// Set up QUIT-type signals so they clean up properly.
	signal(SIGHUP, _sighandler_quit);
	signal(SIGINT, _sighandler_quit);
	signal(SIGQUIT, _sighandler_quit);
	signal(SIGABRT, _sighandler_quit);
	signal(SIGTERM, _sighandler_quit);

	if(getuid())  /*  are we running as a normal user?  */
		bbs_lputs("!Started as non-root user.  Cannot bind() to ports below 1024.");
	
	else if(new_uid_name==NULL)   /*  check the user arg, if we have uid 0 */
		bbs_lputs("Warning: No user account specified, running as root.");
	
	else 
	{
		bbs_lputs("Waiting for child threads to bind ports...");
		while(!bbs_stopped && !ftp_stopped && !mail_stopped && !services_stopped
			&& ((run_bbs && !bbs_running) 
				|| (run_ftp && !ftp_running) 
				|| (run_mail && !mail_running) 
				|| (run_services && !services_running)))
			mswait(1);

		if(!do_setuid())
				/* actually try to change the uid of this process */
			bbs_lputs("!Setting new user_id failed!  (Does the user exist?)");
	
		else {
			char str[256];
			sprintf(str,"Successfully changed user_id to %s", new_uid_name);
			bbs_lputs(str);
		}
	}

	if(!isatty(fileno(stdin)))			/* redirected */
		select(0,NULL,NULL,NULL,NULL);	/* so wait here until signaled */
	else								/* interactive */
#endif
		while(!quit) {
			ch=getch();
			printf("%c\n",ch);
			switch(ch) {
				case 'q':
					quit=TRUE;
					break;
				default:
					printf("\nSynchronet Console Version %s%c Help\n\n",VERSION,REVISION);
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
#ifdef JAVASCRIPT
	services_terminate();
#endif

	while(bbs_running || ftp_running || mail_running || services_running)
		SLEEP(1);

	/* erase the prompt */
	printf("\r%*s\r",prompt_len,"");

	return(0);
}
