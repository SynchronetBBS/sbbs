/* sbbscon.c */

/* Synchronet vanilla/console-mode "front-end" */

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

/* ANSI headers */
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#ifdef __QNX__
#include <locale.h>
#endif

/* Synchronet-specific headers */
#include "conwrap.h"	/* kbhit/getch */
#include "sbbs.h"		/* load_cfg() */
#include "sbbs_ini.h"	/* sbbs_read_ini() */
#include "ftpsrvr.h"	/* ftp_startup_t, ftp_server */
#include "mailsrvr.h"	/* mail_startup_t, mail_server */
#include "services.h"	/* services_startup_t, services_thread */

#ifdef __unix__

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include <stdlib.h>  /* Is this included from somewhere else? */
#include <syslog.h>

#endif

/* Temporary: Do not include web server in 3.1x-Win32 release build */
#if defined(_MSC_VER)
	#define NO_WEB_SERVER
#endif

/* Services doesn't work without JavaScript support */
#if !defined(JAVASCRIPT)
	#define	NO_SERVICES
#endif

/* Constants */
#define SBBS_PID_FILE	"/var/run/sbbs.pid"
#define SBBS_LOG_NAME	"synchronet"

/* Global variables */
BOOL				run_bbs=FALSE;
BOOL				bbs_running=FALSE;
BOOL				bbs_stopped=FALSE;
BOOL				has_bbs=FALSE;
bbs_startup_t		bbs_startup;
BOOL				run_ftp=FALSE;
BOOL				ftp_running=FALSE;
BOOL				ftp_stopped=FALSE;
BOOL				has_ftp=FALSE;
ftp_startup_t		ftp_startup;
BOOL				run_mail=FALSE;
BOOL				mail_running=FALSE;
BOOL				mail_stopped=FALSE;
BOOL				has_mail=FALSE;
mail_startup_t		mail_startup;
BOOL				run_services=FALSE;
BOOL				services_running=FALSE;
BOOL				services_stopped=FALSE;
BOOL				has_services=FALSE;
services_startup_t	services_startup;
BOOL				run_web=FALSE;
BOOL				web_running=FALSE;
BOOL				web_stopped=FALSE;
BOOL				has_web=FALSE;
web_startup_t		web_startup;
uint				thread_count=1;
uint				socket_count=0;
uint				client_count=0;
int					prompt_len=0;
static scfg_t		scfg;					/* To allow rerun */
static ulong		served=0;

#ifdef __unix__
char				new_uid_name[32];
char				new_gid_name[32];
uid_t				new_uid;
uid_t				old_uid;
gid_t				new_gid;
gid_t				old_gid;
BOOL				is_daemon=FALSE;
BOOL				std_facilities=FALSE;
FILE*				pidfile;

#endif

static const char* prompt;

static const char* usage  = "\nusage: %s [[setting] [...]] [path/ini_file]\n"
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
							"\t           if 'S' is specified, uses standard facilities\n"
#endif
							"\tgi         get user identity (using IDENT protocol)\n"
							"\tnh         disable hostname lookups\n"
							"\tnj         disable JavaScript support\n"
							"\tne         disable event thread\n"
							"\tni         do not read settings from .ini file\n"
#ifdef __unix__
							"\tnd         do not read run as daemon - overrides .ini file\n"
#endif
							"\tlt         use local timezone (do not force UTC/GMT)\n"
							"\tdefaults   show default settings and options\n"
							"\n"
							;
static const char* telnet_usage  = "Telnet server settings:\n\n"
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
							"\tt-         disable Telnet/RLogin server\n"
							"\n"
							;
static const char* ftp_usage  = "FTP server settings:\n"
							"\n"
							"\tfp<port>   set FTP server port\n"
							"\tfo<value>  set FTP server options value (advanced)\n"
							"\tf-         disable FTP server\n"
							"\n"
							;
static const char* mail_usage  = "Mail server settings:\n"
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
							;
static const char* services_usage  = "Services settings:\n"
							"\n"
							"\tso<value>  set Services option value (advanced)\n"
							"\ts-         disable Services (no services module)\n"
							"\n"
							;

static int lputs(char *str)
{
	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;

#ifdef __unix__

	if (is_daemon)  {
		if(str!=NULL) {
			if (std_facilities)
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
	if(prompt!=NULL)
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

	if(new_uid_name[0]==0)	/* not set? */
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

#ifdef _WINSOCKAPI_

static WSADATA WSAData;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0)
		return(TRUE);

    fprintf(stderr,"!WinSock startup ERROR %d\n", status);
	return(FALSE);
}

static BOOL winsock_cleanup(void)	
{
	if(WSACleanup()==0)
		return(TRUE);

	fprintf(stderr,"!WinSock cleanup ERROR %d\n",ERROR_VALUE);
	return(FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)	
#define winsock_cleanup()	(TRUE)

#endif

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
/* BBS local/log print routine												*/
/****************************************************************************/
static int bbs_lputs(char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm	tm;

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(LOG_INFO|LOG_AUTH,"%s",str);
		else
			syslog(LOG_INFO,"     %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	if(localtime_r(&t,&tm)==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm.tm_mon+1,tm.tm_mday
			,tm.tm_hour,tm.tm_min,tm.tm_sec);

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
	struct tm	tm;

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
#ifdef __solaris__
			syslog(LOG_INFO|LOG_DAEMON,"%s",str);
#else
			syslog(LOG_INFO|LOG_FTP,"%s",str);
#endif
		else
			syslog(LOG_INFO,"ftp  %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	if(localtime_r(&t,&tm)==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm.tm_mon+1,tm.tm_mday
			,tm.tm_hour,tm.tm_min,tm.tm_sec);

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
	struct tm	tm;

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(LOG_INFO|LOG_MAIL,"%s",str);
		else
			syslog(LOG_INFO,"mail %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	if(localtime_r(&t,&tm)==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm.tm_mon+1,tm.tm_mday
			,tm.tm_hour,tm.tm_min,tm.tm_sec);

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
	struct tm	tm;

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(LOG_INFO|LOG_DAEMON,"%s",str);
		else
			syslog(LOG_INFO,"srvc %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	if(localtime_r(&t,&tm)==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm.tm_mon+1,tm.tm_mday
			,tm.tm_hour,tm.tm_min,tm.tm_sec);

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
	struct tm	tm;

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(LOG_INFO|LOG_CRON,"%s",str);
		else
			syslog(LOG_INFO,"evnt %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	if(localtime_r(&t,&tm)==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm.tm_mon+1,tm.tm_mday
			,tm.tm_hour,tm.tm_min,tm.tm_sec);

	sprintf(logline,"%sevnt %.*s",tstr,(int)sizeof(logline)-32,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

/****************************************************************************/
/* web local/log print routine											*/
/****************************************************************************/
static int web_lputs(char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm	tm;

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(LOG_INFO|LOG_DAEMON,"%s",str);
		else
			syslog(LOG_INFO,"web  %s",str);
		return(strlen(str));
	}
#endif

	t=time(NULL);
	if(localtime_r(&t,&tm)==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm.tm_mon+1,tm.tm_mday
			,tm.tm_hour,tm.tm_min,tm.tm_sec);

	sprintf(logline,"%sweb  %.*s",tstr,(int)sizeof(logline)-32,str);
	truncsp(logline);
	lputs(logline);
	
    return(strlen(logline)+1);
}

static void web_started(void)
{
	web_running=TRUE;
	web_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid();
	#endif
}

static void web_terminated(int code)
{
	web_running=FALSE;
	web_stopped=TRUE;
}

static void terminate(void)
{
	ulong count=0;

	bbs_terminate();
	ftp_terminate();
#ifndef NO_WEB_SERVER
	web_terminate();
#endif
	mail_terminate();
#ifndef NO_SERVICES
	services_terminate();
#endif

	while(bbs_running || ftp_running || web_running || mail_running || services_running)  {
		if(count && (count%10)==0) {
			if(bbs_running)
				bbs_lputs("BBS System thread still running");
			if(ftp_running)
				ftp_lputs("FTP Server thread still running");
			if(web_running)
				web_lputs("Web Server thread still running");
			if(mail_running)
				mail_lputs("Mail Server thread still running");
			if(services_running)
				services_lputs("Services thread still running");
		}
		count++;
		SLEEP(1000);
	}
}

#ifdef __unix__
void _sighandler_quit(int sig)
{
	char	str[1024];
	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;

	if(!mutex_initialized) {
		pthread_mutex_init(&mutex,NULL);
		mutex_initialized=TRUE;
	}
	pthread_mutex_lock(&mutex);
	/* Can I get away with leaving this locked till exit? */

	sprintf(str,"     Got quit signal (%d)",sig);
	lputs(str);
	terminate();

	if(is_daemon)
		unlink(SBBS_PID_FILE);

    exit(0);
}

void _sighandler_rerun(int sig)
{

	lputs("     Got HUP (rerun) signal");
	
#if 0	/* old way, we don't want to recycle all nodes, necessarily */
	for(i=1;i<=scfg.sys_nodes;i++) {
		getnodedat(&scfg,i,&node,NULL /* file */);
		node.misc|=NODE_RRUN;
		printnodedat(&scfg,i,&node);
	}
#else	/* instead, recycle all our servers */
	bbs_startup.recycle_now=TRUE;
	ftp_startup.recycle_now=TRUE;
	web_startup.recycle_now=TRUE;
	mail_startup.recycle_now=TRUE;
	services_startup.recycle_now=TRUE;
#endif
}

#ifdef NEEDS_DAEMON
/****************************************************************************/
/* Daemonizes the process                                                   */
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
#endif /* NEEDS_DAEMON */

static void handle_sigs(void)  {
	int		sig;
	sigset_t			sigs;
	char		str[1024];

	thread_up(TRUE,TRUE);

	if (is_daemon) {
		if(pidfile!=NULL) {
			fprintf(pidfile,"%d",getpid());
			fclose(pidfile);
		}
	}

	/* Set up blocked signals */
	sigemptyset(&sigs);
	sigaddset(&sigs,SIGINT);
	sigaddset(&sigs,SIGQUIT);
	sigaddset(&sigs,SIGABRT);
	sigaddset(&sigs,SIGTERM);
	sigaddset(&sigs,SIGHUP);
	sigaddset(&sigs,SIGALRM);
	sigaddset(&sigs,SIGPIPE);
	pthread_sigmask(SIG_BLOCK,&sigs,NULL);
	while(1)  {
		sigwait(&sigs,&sig);    /* wait here until signaled */
		sprintf(str,"     Got signal (%d)",sig);
		lputs(str);
		switch(sig)  {
			/* QUIT-type signals */
			case SIGINT:
			case SIGQUIT:
			case SIGABRT:
			case SIGTERM:
				_sighandler_quit(sig);
				break;
			case SIGHUP:    /* rerun */
				_sighandler_rerun(sig);
				break;
			default:
				sprintf(str,"     Signal has no handler (unexpected)");
				lputs(str);
		}
	}
}
#endif	/* __unix__ */

/****************************************************************************/
/* Displays appropriate usage info											*/
/****************************************************************************/
static void show_usage(char *cmd)
{
	printf(usage,cmd);
	if(has_bbs)
		printf(telnet_usage);
	if(has_ftp)
		printf(ftp_usage);
	if(has_mail)
		printf(mail_usage);
	if(has_services)
		printf(services_usage);
}

static int command_is(char *cmdline, char *cmd)
{
	return(strnicmp(getfname(cmdline),cmd,strlen(cmd))==0);
}

/****************************************************************************/
/* Main Entry Point															*/
/****************************************************************************/
int main(int argc, char** argv)
{
	int		i;
	int		n;
	int		file;
	char	ch;
	char*	p;
	char*	arg;
	char*	ctrl_dir;
	char	str[MAX_PATH+1];
    char	error[256];
	char	ini_file[MAX_PATH+1];
	char	host_name[128]="";
	BOOL	quit=FALSE;
	FILE*	fp=NULL;
	node_t	node;
#ifdef __unix__
	char	daemon_type[2];
	char	value[MAX_VALUE_LEN];
	struct passwd* pw_entry;
	struct group*  gr_entry;
	sigset_t			sigs;
#endif

	if(command_is(argv[0],"sbbs_ftp"))
		run_ftp=has_ftp=TRUE;
	else if(command_is(argv[0],"sbbs_mail"))
		run_mail=has_mail=TRUE;
	else if(command_is(argv[0],"sbbs_bbs"))
		run_bbs=has_bbs=TRUE;
	else if(command_is(argv[0],"sbbs_srvc"))
		run_services=has_services=TRUE;
	else if(command_is(argv[0],"sbbs_web"))
		run_web=has_web=TRUE;
	else {
		run_bbs=has_bbs=TRUE;
		run_ftp=has_ftp=TRUE;
		run_mail=has_mail=TRUE;
		run_services=has_services=TRUE;
		run_web=has_web=TRUE;
	}

#ifdef __QNX__
	setlocale( LC_ALL, "C-TRADITIONAL" );
#endif
#ifdef __unix__
	umask(077);
#endif
	printf("\nSynchronet Console for %s  Version %s%c  %s\n\n"
		,PLATFORM_DESC,VERSION,REVISION,COPYRIGHT_NOTICE);

	ctrl_dir=getenv("SBBSCTRL");	/* read from environment variable */
	if(ctrl_dir==NULL || ctrl_dir[0]==0) {
		ctrl_dir="/sbbs/ctrl";		/* Not set? Use default */
		printf("!SBBSCTRL environment variable not set, using default value: %s\n\n"
			,ctrl_dir);
	}

	if(!winsock_startup())
		return(-1);

	gethostname(host_name,sizeof(host_name)-1);

	if(!winsock_cleanup())
		return(-1);

	sprintf(ini_file,"%s%c%s.ini",ctrl_dir,PATH_DELIM,host_name);
#if defined(__unix__) && defined(PREFIX)
	if(!fexistcase(ini_file))
		sprintf(ini_file,PREFIX"/etc/sbbs.ini");
#endif
	if(!fexistcase(ini_file))
		sprintf(ini_file,"%s%csbbs.ini",ctrl_dir,PATH_DELIM);

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

	/* Initialize Web Server startup structure */
    memset(&web_startup,0,sizeof(web_startup));
    web_startup.size=sizeof(web_startup);
	web_startup.lputs=web_lputs;
    web_startup.started=web_started;
    web_startup.terminated=web_terminated;
	web_startup.thread_up=thread_up;
    web_startup.socket_open=socket_open;
#ifdef __unix__
	web_startup.seteuid=do_seteuid;
#endif
    strcpy(web_startup.ctrl_dir,ctrl_dir);

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
		if(strcspn(arg,"\\/")!=strlen(arg)) {
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

	prompt = "[Threads: %d  Sockets: %d  Clients: %d  Served: %lu] (?=Help): ";

	/* We call this function to set defaults, even if there's no .ini file */
	sbbs_read_ini(fp, 
		&run_bbs,		&bbs_startup,
		&run_ftp,		&ftp_startup, 
		&run_web,		&web_startup,
		&run_mail,		&mail_startup, 
		&run_services,	&services_startup);

	/* read/default any sbbscon-specific .ini keys here */
#if defined(__unix__)
	SAFECOPY(new_uid_name,iniGetString(fp,"UNIX","User","",value));
	SAFECOPY(new_gid_name,iniGetString(fp,"UNIX","Group","",value));
	is_daemon=iniGetBool(fp,"UNIX","Daemonize",FALSE);
	SAFECOPY(daemon_type,iniGetString(fp,"UNIX","LogFacility","U",value));
	umask(iniGetInteger(fp,"UNIX","umask",077));
#endif
	/* close .ini file here */
	if(fp!=NULL)
		fclose(fp);

	/* Post-INI command-line switches */
	for(i=1;i<argc;i++) {
		arg=argv[i];
		while(*arg=='-')
			arg++;
		if(strcspn(arg,"\\/")!=strlen(arg))	/* ini_file name */
			continue;
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
					is_daemon=TRUE;
					SAFECOPY(daemon_type,arg++);
				break;
#endif
			case 'T':	/* Telnet settings */
				switch(toupper(*(arg++))) {
					case '-':	
						run_bbs=FALSE;
						break;
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
						show_usage(argv[0]);
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
						show_usage(argv[0]);
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
						show_usage(argv[0]);
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
								show_usage(argv[0]);
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
								show_usage(argv[0]);
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
						show_usage(argv[0]);
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
						show_usage(argv[0]);
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
						show_usage(argv[0]);
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
							SAFECOPY(bbs_startup.host_name,host_name);
							SAFECOPY(ftp_startup.host_name,host_name);
							SAFECOPY(mail_startup.host_name,host_name);
							SAFECOPY(services_startup.host_name,host_name);
						}
						printf("Setting hostname: %s\n",bbs_startup.host_name);
						break;
					default:
						show_usage(argv[0]);
						return(0);
				}
				break;
			case 'U':	/* runtime UID */
				switch(toupper(*(arg++))) {
					case 'N': /* username */
#ifdef __unix__
						if(strlen(arg) > 1)
						{
							SAFECOPY(new_uid_name,arg);
						}
#endif			
						break;
					case 'G': /* groupname */
#ifdef __unix__
						if(strlen(arg) > 1)
						{
							SAFECOPY(new_gid_name,arg);
						}
#endif			
						break;
					default:
						show_usage(argv[0]);
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
					case 'T':	/* Telnet */
						run_bbs=FALSE;
						break;
					case 'E': /* No Events */
						bbs_startup.options		|=BBS_OPT_NO_EVENTS;
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
					case 'D':
#if defined(__unix__)
						is_daemon=FALSE;
#endif
						break;
					default:
						show_usage(argv[0]);
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
						show_usage(argv[0]);
						return(0);
				}
				break;

			default:
				show_usage(argv[0]);
				return(0);
		}
	}

/* Daemonize / Set uid/gid */
#ifdef __unix__

	if(is_daemon) {
		switch(toupper(daemon_type[0])) {
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
			case 'F':	/* this is legacy */
			case 'S':
				/* Use standard facilities */
				openlog(SBBS_LOG_NAME,LOG_CONS,LOG_USER);
				std_facilities = TRUE;
				break;
			default:
				openlog(SBBS_LOG_NAME,LOG_CONS,LOG_USER);
		}

		printf("Running as daemon\n");
		if(daemon(TRUE,FALSE))  { /* Daemonize, DON'T switch to / and DO close descriptors */
			printf("!ERROR %d running as daemon",errno);
			is_daemon=FALSE;
		}

		/* Write the standard .pid file if running as a daemon */
		/* Must be here so signals are sent to the correct thread */

		pidfile=fopen(SBBS_PID_FILE,"w");
	}

	old_uid = getuid();
	if((pw_entry=getpwnam(new_uid_name))!=0)
	{
		new_uid=pw_entry->pw_uid;
		new_gid=pw_entry->pw_gid;
	}
	else  {
		new_uid=getuid();
		new_gid=getgid();
	}
	old_gid = getgid();
	if((gr_entry=getgrnam(new_gid_name))!=0)
		new_gid=gr_entry->gr_gid;
	
#endif

	/* Read in configuration files */
    memset(&scfg,0,sizeof(scfg));
    SAFECOPY(scfg.ctrl_dir,bbs_startup.ctrl_dir);

	if(chdir(scfg.ctrl_dir)!=0)
		fprintf(stderr,"\n!ERROR %d changing directory to: %s\n", errno, scfg.ctrl_dir);

    scfg.size=sizeof(scfg);
	SAFECOPY(error,UNKNOWN_LOAD_ERROR);
	sprintf(str,"Loading configuration files from %s", scfg.ctrl_dir);
	bbs_lputs(str);
	if(!load_cfg(&scfg, NULL /* text.dat */, TRUE /* prep */, error)) {
		fprintf(stderr,"\n!ERROR Loading Configuration Files: %s\n", error);
        return(-1);
    }


#ifdef __unix__
	/* Set up blocked signals */
	sigemptyset(&sigs);
	sigaddset(&sigs,SIGINT);
	sigaddset(&sigs,SIGQUIT);
	sigaddset(&sigs,SIGABRT);
	sigaddset(&sigs,SIGTERM);
	sigaddset(&sigs,SIGHUP);
	sigaddset(&sigs,SIGALRM);
	sigaddset(&sigs,SIGPIPE);
	pthread_sigmask(SIG_BLOCK,&sigs,NULL);
    signal(SIGALRM, SIG_IGN);       /* Ignore "Alarm" signal */
	_beginthread((void(*)(void*))handle_sigs,0,NULL);
#endif

	if(run_bbs)
		_beginthread((void(*)(void*))bbs_thread,0,&bbs_startup);
	if(run_ftp)
		_beginthread((void(*)(void*))ftp_server,0,&ftp_startup);
	if(run_mail)
		_beginthread((void(*)(void*))mail_server,0,&mail_startup);
#ifdef NO_SERVICES
	run_services=FALSE;
#else
	if(run_services)
		_beginthread((void(*)(void*))services_thread,0,&services_startup);
#endif
#ifdef NO_WEB_SERVER
	run_web=FALSE;
#else
	if(run_web)
		_beginthread((void(*)(void*))web_server,0,&web_startup);
#endif

#ifdef __unix__
	if(getuid())  { /*  are we running as a normal user?  */
		sprintf(str,"!Started as non-root user.  Cannot bind() to ports below %u.", IPPORT_RESERVED);
		bbs_lputs(str);
	}
	
	else if(new_uid_name[0]==0)   /*  check the user arg, if we have uid 0 */
		bbs_lputs("Warning: No user account specified, running as root.");
	
	else 
	{
		bbs_lputs("Waiting for child threads to bind ports...");
		while((run_bbs && !(bbs_running || bbs_stopped)) 
				|| (run_ftp && !(ftp_running || ftp_stopped)) 
				|| (run_web && !(web_running || web_stopped)) 
				|| (run_mail && !(mail_running || mail_stopped)) 
				|| (run_services && !(services_running || services_stopped)))  {
			mswait(1000);
			if(run_bbs && !(bbs_running || bbs_stopped))
				bbs_lputs("Waiting for BBS thread");
			if(run_web && !(web_running || web_stopped))
				bbs_lputs("Waiting for Web thread");
			if(run_ftp && !(ftp_running || ftp_stopped))
				bbs_lputs("Waiting for FTP thread");
			if(run_mail && !(mail_running || mail_stopped))
				bbs_lputs("Waiting for Mail thread");
			if(run_services && !(services_running || services_stopped))
				bbs_lputs("Waiting for Services thread");
		}

		if(!do_setuid())
				/* actually try to change the uid of this process */
			bbs_lputs("!Setting new user_id failed!  (Does the user exist?)");
	
		else {
			char str[256];
			sprintf(str,"Successfully changed user_id to %s", new_uid_name);
			bbs_lputs(str);

			/* Can't recycle servers (re-bind ports) as non-root user */
			/* ToDo: Something seems to be broken here on FreeBSD now */
			/* ToDo: Now, they try to re-bind on FreeBSD */
			/* ToDo: Seems like I switched problems with Linux */
 			if(bbs_startup.telnet_port < IPPORT_RESERVED
				|| (bbs_startup.options & BBS_OPT_ALLOW_RLOGIN
					&& bbs_startup.rlogin_port < IPPORT_RESERVED))
				bbs_startup.options|=BBS_OPT_NO_RECYCLE;
			if(ftp_startup.port < IPPORT_RESERVED)
				ftp_startup.options|=FTP_OPT_NO_RECYCLE;
			if(web_startup.port < IPPORT_RESERVED)
				web_startup.options|=BBS_OPT_NO_RECYCLE;
			if((mail_startup.options & MAIL_OPT_ALLOW_POP3
				&& mail_startup.pop3_port < IPPORT_RESERVED)
				|| mail_startup.smtp_port < IPPORT_RESERVED)
				mail_startup.options|=MAIL_OPT_NO_RECYCLE;
			/* Perhaps a BBS_OPT_NO_RECYCLE_LOW option? */
			services_startup.options|=BBS_OPT_NO_RECYCLE;
		}
	}

	if(!isatty(fileno(stdin)))  			/* redirected */
		select(0,NULL,NULL,NULL,NULL);	/* Sleep forever - Should this just exit the thread? */
	else								/* interactive */
#endif
		while(!quit) {
			ch=getch();
			printf("%c\n",ch);
			switch(ch) {
				case 'q':
					quit=TRUE;
					break;
				case 'w':	/* who's online */
					printf("\nNodes in use:\n");
				case 'n':	/* nodelist */
					printf("\n");
					for(i=1;i<=scfg.sys_nodes;i++) {
						getnodedat(&scfg,i,&node,NULL /* file */);
						if(ch=='w' && node.status!=NODE_INUSE && node.status!=NODE_QUIET)
							continue;
						printnodedat(&scfg, i,&node);
					}
					break;
				case 'l':	/* lock node */
				case 'd':	/* down node */
				case 'i':	/* interrupt node */
					printf("\nNode number: ");
					if((n=atoi(fgets(str,sizeof(str),stdin)))<1)
						break;
					fflush(stdin);
					printf("\n");
					if((i=getnodedat(&scfg,n,&node,&file))!=0) {
						printf("!Error %d getting node %d data\n",i,n);
						break;
					}
					switch(ch) {
						case 'l':
							node.misc^=NODE_LOCK;
							break;
						case 'd':
							node.misc^=NODE_DOWN;
							break;
						case 'i':
							node.misc^=NODE_INTR;
							break;
					}
					putnodedat(&scfg,n,&node,file);
					printnodedat(&scfg,n,&node);
					break;
				default:
					printf("\nSynchronet Console Version %s%c Help\n\n",VERSION,REVISION);
					printf("q   = quit\n");
					printf("n   = node list\n");
					printf("w   = who's online\n");
					printf("l   = lock node (toggle)\n");
					printf("d   = down node (toggle)\n");
					printf("i   = interrupt node (toggle)\n");
#if 0	/* to do */	
					printf("c#  = chat with node #\n");
					printf("s#  = spy on node #\n");
#endif
					break;
			}
			lputs("");	/* redisplay prompt */
		}

	terminate();

	/* erase the prompt */
	printf("\r%*s\r",prompt_len,"");

	return(0);
}
