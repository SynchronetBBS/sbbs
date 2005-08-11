/* sbbscon.c */

/* Synchronet vanilla/console-mode "front-end" */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#undef SBBS	/* this shouldn't be defined unless building sbbs.dll/libsbbs.so */
#include "sbbs.h"		/* load_cfg() */
#include "sbbs_ini.h"	/* sbbs_read_ini() */
#include "ftpsrvr.h"	/* ftp_startup_t, ftp_server */
#include "mailsrvr.h"	/* mail_startup_t, mail_server */
#include "services.h"	/* services_startup_t, services_thread */

/* XPDEV headers */
#include "conwrap.h"	/* kbhit/getch */
#include "threadwrap.h"	/* pthread_mutex_t */

#ifdef __unix__

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <stdarg.h>
#include <stdlib.h>  /* Is this included from somewhere else? */
#include <syslog.h>

#endif

/* Services doesn't work without JavaScript support */
#if !defined(JAVASCRIPT)
	#define	NO_SERVICES
#endif

/* Global variables */
BOOL				terminated=FALSE;

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
char				ini_file[MAX_PATH+1];

#ifdef __unix__
char				new_uid_name[32];
char				new_gid_name[32];
uid_t				new_uid;
uid_t				old_uid;
gid_t				new_gid;
gid_t				old_gid;
BOOL				is_daemon=FALSE;
char				log_facility[2];
char				log_ident[128];
BOOL				std_facilities=FALSE;
FILE *				pidf;
char				pid_fname[MAX_PATH+1];
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

static int lputs(int level, char *str)
{
	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;
	char	*p;

#ifdef __unix__

	if (is_daemon)  {
		if(str!=NULL) {
			if (std_facilities)
				syslog(level|LOG_AUTH,"%s",str);
			else
				syslog(level,"%s",str);
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
	if(str!=NULL) {
		for(p=str; *p; p++) {
			if(iscntrl(*p))
				printf("^%c",'@'+*p);
			else
				printf("%c",*p);
		}
		puts("");
	}
	/* re-display prompt with current stats */
	if(prompt!=NULL)
		prompt_len = printf(prompt, thread_count, socket_count, client_count, served);
	fflush(stdout);
	pthread_mutex_unlock(&mutex);

    return(prompt_len);
}

static int lprintf(int level, char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(lputs(level,sbuf));
}

#ifdef __unix__
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
		lputs(LOG_ERR,"!seteuid FAILED");
		lputs(LOG_ERR,strerror(errno));
	}
	return result;
}

/**********************************************************
* Change uid of the calling process to the user if specified
* **********************************************************/
BOOL do_setuid(BOOL force)
{
	BOOL result=TRUE;
#if defined(DONT_BLAME_SYNCHRONET) || defined(_THREAD_SUID_BROKEN)
	if(!force)
		return(do_seteuid(TRUE));
#endif
	setregid(-1,old_gid);
	setreuid(-1,old_uid);
	if(setregid(new_gid,new_gid))
	{
		lputs(LOG_ERR,"!setgid FAILED");
		lputs(LOG_ERR,strerror(errno));
		result=FALSE;
	}

	if(setreuid(new_uid,new_uid))
	{
		lputs(LOG_ERR,"!setuid FAILED");
		lputs(LOG_ERR,strerror(errno));
		result=FALSE;
	}
	if(force && (!result))
		exit(1);

	return(result);
}
#endif   /* __unix__ */

#ifdef _WINSOCKAPI_

static WSADATA WSAData;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0)
		return(TRUE);

    lprintf(LOG_ERR,"!WinSock startup ERROR %d", status);
	return(FALSE);
}

static BOOL winsock_cleanup(void)	
{
	if(WSACleanup()==0)
		return(TRUE);

	lprintf(LOG_ERR,"!WinSock cleanup ERROR %d",ERROR_VALUE);
	return(FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)	
#define winsock_cleanup()	(TRUE)

#endif

static void thread_up(void* p, BOOL up, BOOL setuid)
{
   	static pthread_mutex_t mutex;
	static BOOL mutex_initialized;

#ifdef _THREAD_SUID_BROKEN
	if(up && setuid) {
		do_seteuid(FALSE);
		do_setuid(FALSE);
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
	lputs(LOG_INFO,NULL); /* update displayed stats */
}

static void socket_open(void* p, BOOL open)
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
	lputs(LOG_INFO,NULL); /* update displayed stats */
}

static void client_on(void* p, BOOL on, int sock, client_t* client, BOOL update)
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
	lputs(LOG_INFO,NULL); /* update displayed stats */
}

/****************************************************************************/
/* BBS local/log print routine												*/
/****************************************************************************/
static int bbs_lputs(void* p, int level, char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm	tm;

	if(!(bbs_startup.log_mask&(1<<level)))
		return(0);

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(level|LOG_AUTH,"%s",str);
		else
			syslog(level,"     %s",str);
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
	lputs(level,logline);
	
    return(strlen(logline)+1);
}

static void bbs_started(void* p)
{
	bbs_running=TRUE;
	bbs_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid(FALSE);
	#endif
}

static void bbs_terminated(void* p, int code)
{
	bbs_running=FALSE;
	bbs_stopped=TRUE;
}

/****************************************************************************/
/* FTP local/log print routine												*/
/****************************************************************************/
static int ftp_lputs(void* p, int level, char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm	tm;

	if(!(ftp_startup.log_mask&(1<<level)))
		return(0);

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
#ifdef __solaris__
			syslog(level|LOG_DAEMON,"%s",str);
#else
			syslog(level|LOG_FTP,"%s",str);
#endif
		else
			syslog(level,"ftp  %s",str);
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
	lputs(level,logline);
	
    return(strlen(logline)+1);
}

static void ftp_started(void* p)
{
	ftp_running=TRUE;
	ftp_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid(FALSE);
	#endif
}

static void ftp_terminated(void* p, int code)
{
	ftp_running=FALSE;
	ftp_stopped=TRUE;
}

/****************************************************************************/
/* Mail Server local/log print routine										*/
/****************************************************************************/
static int mail_lputs(void* p, int level, char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm	tm;

	if(!(mail_startup.log_mask&(1<<level)))
		return(0);

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(level|LOG_MAIL,"%s",str);
		else
			syslog(level,"mail %s",str);
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
	lputs(level,logline);
	
    return(strlen(logline)+1);
}

static void mail_started(void* p)
{
	mail_running=TRUE;
	mail_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid(FALSE);
	#endif
}

static void mail_terminated(void* p, int code)
{
	mail_running=FALSE;
	mail_stopped=TRUE;
}

/****************************************************************************/
/* Services local/log print routine											*/
/****************************************************************************/
static int services_lputs(void* p, int level, char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm	tm;

	if(!(services_startup.log_mask&(1<<level)))
		return(0);

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(level|LOG_DAEMON,"%s",str);
		else
			syslog(level,"srvc %s",str);
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
	lputs(level,logline);
	
    return(strlen(logline)+1);
}

static void services_started(void* p)
{
	services_running=TRUE;
	services_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid(FALSE);
	#endif
}

static void services_terminated(void* p, int code)
{
	services_running=FALSE;
	services_stopped=TRUE;
}

/****************************************************************************/
/* Event thread local/log print routine										*/
/****************************************************************************/
static int event_lputs(int level, char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm	tm;

	if(!(bbs_startup.log_mask&(1<<level)))
		return(0);

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(level|LOG_CRON,"%s",str);
		else
			syslog(level,"evnt %s",str);
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
	lputs(level,logline);
	
    return(strlen(logline)+1);
}

/****************************************************************************/
/* web local/log print routine											*/
/****************************************************************************/
static int web_lputs(void* p, int level, char *str)
{
	char		logline[512];
	char		tstr[64];
	time_t		t;
	struct tm	tm;

	if(!(web_startup.log_mask&(1<<level)))
		return(0);

#ifdef __unix__
	if (is_daemon)  {
		if(str==NULL)
			return(0);
		if (std_facilities)
			syslog(level|LOG_DAEMON,"%s",str);
		else
			syslog(level,"web  %s",str);
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
	lputs(level,logline);
	
    return(strlen(logline)+1);
}

static void web_started(void* p)
{
	web_running=TRUE;
	web_stopped=FALSE;
	#ifdef _THREAD_SUID_BROKEN
	    do_seteuid(FALSE);
	    do_setuid(FALSE);
	#endif
}

static void web_terminated(void* p, int code)
{
	web_running=FALSE;
	web_stopped=TRUE;
}

static void terminate(void)
{
	ulong count=0;

	if(bbs_running)
		bbs_terminate();
	if(ftp_running)
		ftp_terminate();
#ifndef NO_WEB_SERVER
	if(web_running)
		web_terminate();
#endif
	if(mail_running)
		mail_terminate();
#ifndef NO_SERVICES
	if(services_running)
		services_terminate();
#endif

	while(bbs_running || ftp_running || web_running || mail_running || services_running)  {
		if(count && (count%10)==0) {
			if(bbs_running)
				lputs(LOG_INFO,"BBS System thread still running");
			if(ftp_running)
				lputs(LOG_INFO,"FTP Server thread still running");
			if(web_running)
				lputs(LOG_INFO,"Web Server thread still running");
			if(mail_running)
				lputs(LOG_INFO,"Mail Server thread still running");
			if(services_running)
				lputs(LOG_INFO,"Services thread still running");
		}
		count++;
		SLEEP(1000);
	}
}

static void read_startup_ini(BOOL recycle
							 ,bbs_startup_t* bbs, ftp_startup_t* ftp, web_startup_t* web
							 ,mail_startup_t* mail, services_startup_t* services)
{
	FILE*	fp=NULL;

	/* Read .ini file here */
	if(ini_file[0]!=0) { 
		if((fp=fopen(ini_file,"r"))==NULL) {
			lprintf(LOG_ERR,"!ERROR %d (%s) opening %s",errno,strerror(errno),ini_file);
		} else {
			lprintf(LOG_INFO,"Reading %s",ini_file);
		}
	}
	if(fp==NULL)
		lputs(LOG_WARNING,"Using default initialization values");

	/* We call this function to set defaults, even if there's no .ini file */
	sbbs_read_ini(fp, 
		NULL,			/* global_startup */
		&run_bbs,		bbs,
		&run_ftp,		ftp, 
		&run_web,		web,
		&run_mail,		mail, 
		&run_services,	services);

	/* read/default any sbbscon-specific .ini keys here */
#if defined(__unix__)
	{
		char	value[INI_MAX_VALUE_LEN];
		char*	section="UNIX";
		SAFECOPY(new_uid_name,iniReadString(fp,section,"User","",value));
		SAFECOPY(new_gid_name,iniReadString(fp,section,"Group","",value));
		if(!recycle)
			is_daemon=iniReadBool(fp,section,"Daemonize",FALSE);
		SAFECOPY(log_facility,iniReadString(fp,section,"LogFacility","U",value));
		SAFECOPY(log_ident,iniReadString(fp,section,"LogIdent","synchronet",value));
		SAFECOPY(pid_fname,iniReadString(fp,section,PidFile","/var/run/sbbs.pid",value));
		umask(iniReadInteger(fp,section,"umask",077));
	}
#endif
	/* close .ini file here */
	if(fp!=NULL)
		fclose(fp);
}

/* Server recycle callback (read relevant startup .ini file section)		*/
void recycle(void* cbdata)
{
	bbs_startup_t* bbs=NULL;
	ftp_startup_t* ftp=NULL;
	web_startup_t* web=NULL;
	mail_startup_t* mail=NULL;
	services_startup_t* services=NULL;

	if(cbdata==&bbs_startup)
		bbs=cbdata;
	else if(cbdata==&ftp_startup)
		ftp=cbdata;
	else if(cbdata==&web_startup)
		web=cbdata;
	else if(cbdata==&mail_startup)
		mail=cbdata;
	else if(cbdata==&services_startup)
		services=cbdata;

	read_startup_ini(/* recycle? */TRUE,bbs,ftp,web,mail,services);
}

void cleanup(void)
{
#ifdef __unix__
	unlink(pid_fname);
#endif
}

#if defined(_WIN32)
BOOL WINAPI ControlHandler(DWORD CtrlType)
{
	terminated=TRUE;
	return TRUE;
}
#endif


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

	lprintf(LOG_NOTICE,"     Got quit signal (%d)",sig);
	terminate();

    exit(0);
}

void _sighandler_rerun(int sig)
{

	lputs(LOG_NOTICE,"     Got HUP (rerun) signal");

	bbs_startup.recycle_now=TRUE;
	ftp_startup.recycle_now=TRUE;
	web_startup.recycle_now=TRUE;
	mail_startup.recycle_now=TRUE;
	services_startup.recycle_now=TRUE;
}

static void handle_sigs(void)
{
	int			i;
	int			sig=0;
	sigset_t	sigs;

	thread_up(NULL,TRUE,TRUE);

	if (is_daemon) {
		/* Write the standard .pid file if running as a daemon */
		/* Must be here so signals are sent to the correct thread */

		if(pidf!=NULL) {
			fprintf(pidf,"%d",getpid());
			fclose(pidf);
			pidf=NULL;
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
	/* sigaddset(&sigs,SIGPIPE); */
	pthread_sigmask(SIG_BLOCK,&sigs,NULL);
	while(1)  {
		if((i=sigwait(&sigs,&sig))!=0) {   /* wait here until signaled */
			lprintf(LOG_ERR,"     !sigwait FAILURE (%d)", i);
			continue;
		}
		lprintf(LOG_NOTICE,"     Got signal (%d)", sig);
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
				lputs(LOG_NOTICE,"     Signal has no handler (unexpected)");
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

#if SBBS_MAGIC_FILENAMES
static int command_is(char *cmdline, char *cmd)
{
	return(strnicmp(getfname(cmdline),cmd,strlen(cmd))==0);
}
#endif

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
	char	host_name[128]="";
	node_t	node;
#ifdef __unix__
	struct passwd*	pw_entry;
	struct group*	gr_entry;
	sigset_t		sigs;
#endif

#ifdef __QNX__
	setlocale( LC_ALL, "C-TRADITIONAL" );
#endif
#ifdef __unix__
	setsid();	/* Disassociate from controlling terminal */
	umask(077);
#endif
	printf("\nSynchronet Console for %s  Version %s%c  %s\n\n"
		,PLATFORM_DESC,VERSION,REVISION,COPYRIGHT_NOTICE);

	atexit(cleanup);

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

	sbbs_get_ini_fname(ini_file, ctrl_dir, host_name);

	/* Initialize BBS startup structure */
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);
	bbs_startup.cbdata=&bbs_startup;
	bbs_startup.log_mask=~0;
	bbs_startup.lputs=bbs_lputs;
	bbs_startup.event_lputs=event_lputs;
    bbs_startup.started=bbs_started;
	bbs_startup.recycle=recycle;
    bbs_startup.terminated=bbs_terminated;
    bbs_startup.thread_up=thread_up;
    bbs_startup.socket_open=socket_open;
    bbs_startup.client_on=client_on;
#ifdef __unix__
	bbs_startup.seteuid=do_seteuid;
	bbs_startup.setuid=do_setuid;
#endif
/*	These callbacks haven't been created yet
    bbs_startup.status=bbs_status;
    bbs_startup.clients=bbs_clients;
*/
    strcpy(bbs_startup.ctrl_dir,ctrl_dir);

	/* Initialize FTP startup structure */
    memset(&ftp_startup,0,sizeof(ftp_startup));
    ftp_startup.size=sizeof(ftp_startup);
	ftp_startup.cbdata=&ftp_startup;
	ftp_startup.log_mask=~0;
	ftp_startup.lputs=ftp_lputs;
    ftp_startup.started=ftp_started;
	ftp_startup.recycle=recycle;
    ftp_startup.terminated=ftp_terminated;
	ftp_startup.thread_up=thread_up;
    ftp_startup.socket_open=socket_open;
    ftp_startup.client_on=client_on;
#ifdef __unix__
	ftp_startup.seteuid=do_seteuid;
	ftp_startup.setuid=do_setuid;
#endif
    strcpy(ftp_startup.index_file_name,"00index");
    strcpy(ftp_startup.ctrl_dir,ctrl_dir);

	/* Initialize Web Server startup structure */
    memset(&web_startup,0,sizeof(web_startup));
    web_startup.size=sizeof(web_startup);
	web_startup.cbdata=&web_startup;
	web_startup.log_mask=~0;
	web_startup.lputs=web_lputs;
    web_startup.started=web_started;
	web_startup.recycle=recycle;
    web_startup.terminated=web_terminated;
	web_startup.thread_up=thread_up;
    web_startup.socket_open=socket_open;
	web_startup.client_on=client_on;
#ifdef __unix__
	web_startup.seteuid=do_seteuid;
	web_startup.setuid=do_setuid;
#endif
    strcpy(web_startup.ctrl_dir,ctrl_dir);

	/* Initialize Mail Server startup structure */
    memset(&mail_startup,0,sizeof(mail_startup));
    mail_startup.size=sizeof(mail_startup);
	mail_startup.cbdata=&mail_startup;
	mail_startup.log_mask=~0;
	mail_startup.lputs=mail_lputs;
    mail_startup.started=mail_started;
	mail_startup.recycle=recycle;
    mail_startup.terminated=mail_terminated;
	mail_startup.thread_up=thread_up;
    mail_startup.socket_open=socket_open;
    mail_startup.client_on=client_on;
#ifdef __unix__
	mail_startup.seteuid=do_seteuid;
	mail_startup.setuid=do_setuid;
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
	services_startup.cbdata=&services_startup;
	services_startup.log_mask=~0;
	services_startup.lputs=services_lputs;
    services_startup.started=services_started;
	services_startup.recycle=recycle;
    services_startup.terminated=services_terminated;
	services_startup.thread_up=thread_up;
    services_startup.socket_open=socket_open;
    services_startup.client_on=client_on;
#ifdef __unix__
	services_startup.seteuid=do_seteuid;
	services_startup.setuid=do_setuid;
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

	read_startup_ini(/* recycle? */FALSE
		,&bbs_startup, &ftp_startup, &web_startup, &mail_startup, &services_startup);

#if SBBS_MAGIC_FILENAMES	/* This stuff is just broken */

	if(!command_is(argv[0],"sbbs"))  {
		run_bbs=has_bbs=FALSE;
		run_ftp=has_ftp=FALSE;
		run_mail=has_mail=FALSE;
		run_services=has_services=FALSE;
		run_web=has_web=FALSE;
	}
	if(command_is(argv[0],"sbbs_ftp"))
		run_ftp=has_ftp=TRUE;
	else if(command_is(argv[0],"sbbs_mail"))
		run_mail=has_mail=TRUE;
	else if(command_is(argv[0],"sbbs_bbs"))
		run_bbs=has_bbs=TRUE;
#ifndef NO_SERVICES
	else if(command_is(argv[0],"sbbs_srvc"))
		run_services=has_services=TRUE;
#endif
#ifndef NO_WEB_SERVER
	else if(command_is(argv[0],"sbbs_web"))
		run_web=has_web=TRUE;
#endif
	else {
		run_bbs=has_bbs=TRUE;
		run_ftp=has_ftp=TRUE;
		run_mail=has_mail=TRUE;
#ifndef NO_SERVICES
		run_services=has_services=TRUE;
#endif
#ifndef NO_WEB_SERVER
		run_web=has_web=TRUE;
#endif
	}

#endif	/* Removed broken stuff */

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
					if(*arg)
						SAFECOPY(log_facility,arg++);
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
						return(1);
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
						return(1);
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
						return(1);
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
								return(1);
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
								return(1);
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
						return(1);
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
						return(1);
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
						return(1);
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
						return(1);
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
						return(1);
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
						return(1);
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
						return(1);
				}
				break;

			default:
				show_usage(argv[0]);
				return(1);
		}
	}

	/* Read in configuration files */
    memset(&scfg,0,sizeof(scfg));
    SAFECOPY(scfg.ctrl_dir,bbs_startup.ctrl_dir);

	if(chdir(scfg.ctrl_dir)!=0)
		lprintf(LOG_ERR,"!ERROR %d changing directory to: %s", errno, scfg.ctrl_dir);

    scfg.size=sizeof(scfg);
	SAFECOPY(error,UNKNOWN_LOAD_ERROR);
	lprintf(LOG_INFO,"Loading configuration files from %s", scfg.ctrl_dir);
	if(!load_cfg(&scfg, NULL /* text.dat */, TRUE /* prep */, error)) {
		lprintf(LOG_ERR,"!ERROR Loading Configuration Files: %s", error);
        return(-1);
    }

/* Daemonize / Set uid/gid */
#ifdef __unix__

	switch(toupper(log_facility[0])) {
		case '0':
			openlog(log_ident,LOG_CONS,LOG_LOCAL0);
			break;
		case '1':
			openlog(log_ident,LOG_CONS,LOG_LOCAL1);
			break;
		case '2':
			openlog(log_ident,LOG_CONS,LOG_LOCAL2);
			break;
		case '3':
			openlog(log_ident,LOG_CONS,LOG_LOCAL3);
			break;
		case '4':
			openlog(log_ident,LOG_CONS,LOG_LOCAL4);
			break;
		case '5':
			openlog(log_ident,LOG_CONS,LOG_LOCAL5);
			break;
		case '6':
			openlog(log_ident,LOG_CONS,LOG_LOCAL6);
			break;
		case '7':
			openlog(log_ident,LOG_CONS,LOG_LOCAL7);
			break;
		case 'F':	/* this is legacy */
		case 'S':
			/* Use standard facilities */
			openlog(log_ident,LOG_CONS,LOG_USER);
			std_facilities = TRUE;
			break;
		default:
			openlog(log_ident,LOG_CONS,LOG_USER);
	}
	if(is_daemon) {

		lprintf(LOG_INFO,"Running as daemon");
		if(daemon(TRUE,FALSE))  { /* Daemonize, DON'T switch to / and DO close descriptors */
			lprintf(LOG_ERR,"!ERROR %d running as daemon",errno);
			is_daemon=FALSE;
		}

		/* Open here to use startup permissions to create the file */
		pidf=fopen(pid_fname,"w");
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

	/* Install Ctrl-C/Break signal handler here */
#if defined(_WIN32)
	SetConsoleCtrlHandler(ControlHandler, TRUE /* Add */);
#elif defined(__unix__)
	/* Set up blocked signals */
	sigemptyset(&sigs);
	sigaddset(&sigs,SIGINT);
	sigaddset(&sigs,SIGQUIT);
	sigaddset(&sigs,SIGABRT);
	sigaddset(&sigs,SIGTERM);
	sigaddset(&sigs,SIGHUP);
	sigaddset(&sigs,SIGALRM);
	/* sigaddset(&sigs,SIGPIPE); */
	pthread_sigmask(SIG_BLOCK,&sigs,NULL);
    signal(SIGPIPE, SIG_IGN);       /* Ignore "Broken Pipe" signal (Also used for broken socket etc.) */
    signal(SIGALRM, SIG_IGN);       /* Ignore "Alarm" signal */
	_beginthread((void(*)(void*))handle_sigs,0,NULL);
	if(new_uid_name[0]!=0) {        /*  check the user arg, if we have uid 0 */
		/* Can't recycle servers (re-bind ports) as non-root user */
		/* If DONT_BLAME_SYNCHRONET is set, keeps root credentials laying around */
#if !defined(DONT_BLAME_SYNCHRONET) && !defined(_THREAD_SUID_BROKEN)
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
#endif
	}
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
		lprintf(LOG_WARNING
			,"!Started as non-root user.  Cannot bind() to ports below %u.", IPPORT_RESERVED);
	}
	
	else if(new_uid_name[0]==0)   /*  check the user arg, if we have uid 0 */
		lputs(LOG_WARNING,"WARNING: No user account specified, running as root.");
	
	else 
	{
		lputs(LOG_INFO,"Waiting for child threads to bind ports...");
		while((run_bbs && !(bbs_running || bbs_stopped)) 
				|| (run_ftp && !(ftp_running || ftp_stopped)) 
				|| (run_web && !(web_running || web_stopped)) 
				|| (run_mail && !(mail_running || mail_stopped)) 
				|| (run_services && !(services_running || services_stopped)))  {
			mswait(1000);
			if(run_bbs && !(bbs_running || bbs_stopped))
				lputs(LOG_INFO,"Waiting for BBS thread");
			if(run_web && !(web_running || web_stopped))
				lputs(LOG_INFO,"Waiting for Web thread");
			if(run_ftp && !(ftp_running || ftp_stopped))
				lputs(LOG_INFO,"Waiting for FTP thread");
			if(run_mail && !(mail_running || mail_stopped))
				lputs(LOG_INFO,"Waiting for Mail thread");
			if(run_services && !(services_running || services_stopped))
				lputs(LOG_INFO,"Waiting for Services thread");
		}

		if(!do_setuid(FALSE))
				/* actually try to change the uid of this process */
			lputs(LOG_ERR,"!Setting new user_id failed!  (Does the user exist?)");
	
		else {
			char str[256];
			struct passwd *pwent;

			pwent=getpwnam(new_uid_name);
			if(pwent != NULL) {
				char	uenv[128];
				char	henv[MAX_PATH+6];
				sprintf(uenv,"USER=%s",pwent->pw_name);
				putenv(uenv);
				sprintf(henv,"HOME=%s",pwent->pw_dir);
				putenv(henv);
			}
			if(new_gid_name[0]) {
				char	genv[128];
				sprintf(genv,"GROUP=%s",new_gid_name);
				putenv(genv);
			}
			lprintf(LOG_INFO,"Successfully changed user_id to %s", new_uid_name);
		}
	}

	if(!isatty(fileno(stdin)))  			/* redirected */
		select(0,NULL,NULL,NULL,NULL);	/* Sleep forever - Should this just exit the thread? */
	else 								/* interactive */
#endif
	{
		prompt = "[Threads: %d  Sockets: %d  Clients: %d  Served: %lu] (?=Help): ";
		lputs(LOG_INFO,NULL);	/* display prompt */

		while(!terminated) {
#ifdef __unix__
			if(!isatty(STDIN_FILENO))  {		/* Controlling terminal has left us *sniff* */
				int fd;
				openlog(log_ident,LOG_CONS,LOG_USER);
    			if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
        			(void)dup2(fd, STDIN_FILENO);
        			(void)dup2(fd, STDOUT_FILENO);
        			(void)dup2(fd, STDERR_FILENO);
        			if (fd > 2)
            			(void)close(fd);
    			}
				is_daemon=TRUE;
				lputs(LOG_WARNING, "STDIN is not a tty anymore... switching to syslog logging");
				select(0,NULL,NULL,NULL,NULL);	/* Sleep forever - Should this just exit the thread? */
			}
#endif

			if(!kbhit()) {
				YIELD();
				continue;
			}
			ch=getch();
			printf("%c\n",ch);
			switch(ch) {
				case 'q':
					terminated=TRUE;
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
				case 'r':	/* recycle */
				case 's':	/* shutdown */
				case 't':	/* terminate */
					printf("BBS, FTP, Web, Mail, Services, or [All] ? ");
					switch(toupper(getch())) {
						case 'B':
							printf("BBS\n");
							if(ch=='t')
								bbs_terminate();
							else if(ch=='s')
								bbs_startup.shutdown_now=TRUE;
							else
								bbs_startup.recycle_now=TRUE;
							break;
						case 'F':
							printf("FTP\n");
							if(ch=='t')
								ftp_terminate();
							else if(ch=='s')
								ftp_startup.shutdown_now=TRUE;
							else
								ftp_startup.recycle_now=TRUE;
							break;
						case 'W':
							printf("Web\n");
							if(ch=='t')
								web_terminate();
							else if(ch=='s')
								web_startup.shutdown_now=TRUE;
							else
								web_startup.recycle_now=TRUE;
							break;
						case 'M':
							printf("Mail\n");
							if(ch=='t')
								mail_terminate();
							else if(ch=='s')
								mail_startup.shutdown_now=TRUE;
							else
								mail_startup.recycle_now=TRUE;
							break;
						case 'S':
							printf("Services\n");
							if(ch=='t')
								services_terminate();
							else if(ch=='s')
								services_startup.shutdown_now=TRUE;
							else
								services_startup.recycle_now=TRUE;
							break;
						default:
							printf("All\n");
							if(ch=='t')
								terminate();
							else if(ch=='s') {
								bbs_startup.shutdown_now=TRUE;
								ftp_startup.shutdown_now=TRUE;
								web_startup.shutdown_now=TRUE;
								mail_startup.shutdown_now=TRUE;
								services_startup.shutdown_now=TRUE;
							}
							else {
								bbs_startup.recycle_now=TRUE;
								ftp_startup.recycle_now=TRUE;
								web_startup.recycle_now=TRUE;
								mail_startup.recycle_now=TRUE;
								services_startup.recycle_now=TRUE;							
							}
							break;
					}
					break;
				case '!':	/* execute */
					printf("Command line: ");
					fgets(str,sizeof(str),stdin);
					system(str);
					break;
				default:
					printf("\nSynchronet Console Version %s%c Help\n\n",VERSION,REVISION);
					printf("q   = quit\n");
					printf("n   = node list\n");
					printf("w   = who's online\n");
					printf("l   = lock node (toggle)\n");
					printf("d   = down node (toggle)\n");
					printf("i   = interrupt node (toggle)\n");
					printf("r   = recycle servers (when not in use)\n");
					printf("s   = shutdown servers (when not in use)\n");
					printf("t   = terminate servers (immediately)\n");
					printf("!   = execute external command\n");
#if 0	/* to do */	
					printf("c#  = chat with node #\n");
					printf("s#  = spy on node #\n");
#endif
					break;
			}
			lputs(LOG_INFO,"");	/* redisplay prompt */
		}
	}

	terminate();

	/* erase the prompt */
	printf("\r%*s\r",prompt_len,"");

	return(0);
}
