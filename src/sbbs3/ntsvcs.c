/* Synchronet BBS as a set of Windows NT Services */

/* $Id: ntsvcs.c,v 1.51 2020/01/03 20:59:58 rswindell Exp $ */

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

/* Synchronet-specific headers */
#include "sbbs.h"		/* various */
#include "sbbs_ini.h"	/* sbbs_read_ini() */
#include "ftpsrvr.h"	/* ftp_startup_t, ftp_server */
#include "websrvr.h"	/* web_startup_t, web_server */
#include "mailsrvr.h"	/* mail_startup_t, mail_server */
#include "services.h"	/* services_startup_t, services_thread */
#include "ntsvcs.h"		/* NT service names */

/* Windows-specific headers */
#include <winsvc.h>

#define NTSVC_TIMEOUT_STARTUP	30000	/* Milliseconds */
#define NTSVC_TIMEOUT_TERMINATE	30000	/* Milliseconds */

#define STRLEN_MAX_DISPLAY_NAME	35

#define STRLEN_SYNCHRONET		10		/* this number ain't change'n */

#define SERVICE_NOT_INSTALLED	(SERVICE_DISABLED+1)

static void WINAPI bbs_ctrl_handler(DWORD dwCtrlCode);
static void WINAPI ftp_ctrl_handler(DWORD dwCtrlCode);
static void WINAPI web_ctrl_handler(DWORD dwCtrlCode);
static void WINAPI mail_ctrl_handler(DWORD dwCtrlCode);
static void WINAPI services_ctrl_handler(DWORD dwCtrlCode);

/* Global variables */

char				ini_file[MAX_PATH+1];
bbs_startup_t		bbs_startup;
ftp_startup_t		ftp_startup;
mail_startup_t		mail_startup;
services_startup_t	services_startup;
web_startup_t		web_startup;
link_list_t			login_attempt_list;

typedef struct {
	char*					name;
	char*					display_name;
	char*					description;
	void*					startup;
	DWORD*					options;
	BOOL*					recycle_now;
	int*					log_level;
	void DLLCALL			(*thread)(void* arg);
	void DLLCALL			(*terminate)(void);
	void					(WINAPI *ctrl_handler)(DWORD);
	HANDLE					log_handle;
	HANDLE					event_handle;
	BOOL					autostart;
	BOOL					debug;
	SERVICE_STATUS			status;
	SERVICE_STATUS_HANDLE	status_handle;
} sbbs_ntsvc_t;

sbbs_ntsvc_t bbs ={	
	NTSVC_NAME_BBS,
	"Synchronet Terminal Server",
	"Provides support for Telnet, RLogin, and SSH clients and executes timed events. " \
		"This service provides the critical functions of your Synchronet BBS.",
	&bbs_startup,
	&bbs_startup.options,
	&bbs_startup.recycle_now,
	&bbs_startup.log_level,
	bbs_thread,
	bbs_terminate,
	bbs_ctrl_handler,
	INVALID_HANDLE_VALUE
};

/* This is not (currently) a separate service, use this for logging only */
sbbs_ntsvc_t event ={	
	NTSVC_NAME_EVENT,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	&bbs_startup.log_level,
	NULL,
	NULL,
	NULL,
	INVALID_HANDLE_VALUE
};

sbbs_ntsvc_t ftp = {
	NTSVC_NAME_FTP,
	"Synchronet FTP Server",
	"Provides support for FTP clients (including web browsers) for file transfers.",
	&ftp_startup,
	&ftp_startup.options,
	&ftp_startup.recycle_now,
	&ftp_startup.log_level,
	ftp_server,
	ftp_terminate,
	ftp_ctrl_handler,
	INVALID_HANDLE_VALUE
};

sbbs_ntsvc_t web = {
	NTSVC_NAME_WEB,
	"Synchronet Web Server",
	"Provides support for Web (HTML/HTTP) clients (browsers).",
	&web_startup,
	&web_startup.options,
	&web_startup.recycle_now,
	&web_startup.log_level,
	web_server,
	web_terminate,
	web_ctrl_handler,
	INVALID_HANDLE_VALUE
};

sbbs_ntsvc_t mail = {
	NTSVC_NAME_MAIL,
	"Synchronet SMTP/POP3 Mail Server",
	"Sends and receives Internet e-mail (using SMTP) and allows users to remotely " \
		"access their e-mail using an Internet mail client (using POP3).",
	&mail_startup,
	&mail_startup.options,
	&mail_startup.recycle_now,
	&mail_startup.log_level,
	mail_server,
	mail_terminate,
	mail_ctrl_handler,
	INVALID_HANDLE_VALUE
};

sbbs_ntsvc_t services = {
	NTSVC_NAME_SERVICES,
	"Synchronet Services",
	"Plugin servers (usually in JavaScript) for any TCP/UDP protocol. " \
		"Stock services include Finger, Gopher, NNTP, and IRC. Edit your ctrl/services.ini " \
		"file for configuration of individual Synchronet Services.",
	&services_startup,
	&services_startup.options,
	&services_startup.recycle_now,
	&services_startup.log_level,
	services_thread,
	services_terminate,
	services_ctrl_handler,
	INVALID_HANDLE_VALUE
};

/* This list is used for enumerating all services */
sbbs_ntsvc_t* ntsvc_list[] = {
	&bbs,
	&ftp,
	&web,
	&mail,
	&services,
	NULL
};
							
/****************************************/
/* Service Control Handlers (Callbacks) */
/****************************************/

/* Common control handler for all services */
static void svc_ctrl_handler(sbbs_ntsvc_t* svc, DWORD dwCtrlCode)
{
	switch(dwCtrlCode) {
		case SERVICE_CONTROL_RECYCLE:
			*svc->recycle_now=TRUE;
			break;
		case SERVICE_CONTROL_MUTE:
			*svc->options|=BBS_OPT_MUTE;
			break;
		case SERVICE_CONTROL_UNMUTE:
			*svc->options&=~BBS_OPT_MUTE;
			break;
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			svc->terminate();
			svc->status.dwWaitHint=NTSVC_TIMEOUT_TERMINATE;
			svc->status.dwCurrentState=SERVICE_STOP_PENDING;
			break;
	}
	SetServiceStatus(svc->status_handle, &svc->status);
}

/* Service-specific control handler stub functions */
static void WINAPI bbs_ctrl_handler(DWORD dwCtrlCode)
{
	svc_ctrl_handler(&bbs, dwCtrlCode);
}

static void WINAPI ftp_ctrl_handler(DWORD dwCtrlCode)
{
	svc_ctrl_handler(&ftp, dwCtrlCode);
}

static void WINAPI web_ctrl_handler(DWORD dwCtrlCode)
{
	svc_ctrl_handler(&web, dwCtrlCode);
}

static void WINAPI mail_ctrl_handler(DWORD dwCtrlCode)
{
	svc_ctrl_handler(&mail, dwCtrlCode);
}

static void WINAPI services_ctrl_handler(DWORD dwCtrlCode)
{
	svc_ctrl_handler(&services, dwCtrlCode);
}

static WORD event_type(int level)
{
	switch(level) {
		case LOG_WARNING:
			return(EVENTLOG_WARNING_TYPE);
		case LOG_NOTICE:
		case LOG_INFO:
		case LOG_DEBUG:
			return(EVENTLOG_INFORMATION_TYPE);	/* same as EVENT_LOG_SUCCESS */
	}
/*
	LOG_EMERG
	LOG_ALERT
	LOG_CRIT
	LOG_ERR
*/
	return(EVENTLOG_ERROR_TYPE);
}

/**************************************/
/* Common Service Log Ouptut Function */
/**************************************/
static int svc_lputs(void* p, int level, const char* str)
{
	char	debug[1024];
	char	fname[256];
	DWORD	len;
	DWORD	wr;
	log_msg_t	msg;
	sbbs_ntsvc_t* svc = (sbbs_ntsvc_t*)p;

	/* Debug Logging */
	if(svc==NULL || svc->debug) {
		snprintf(debug,sizeof(debug),"%s: %s",svc==NULL ? "Synchronet" : svc->name, str);
		OutputDebugString(debug);
	}
	if(svc==NULL)
		return(0);

	len = strlen(str);
	SAFECOPY(msg.buf, str);
	msg.level = level;
	msg.repeated = 0;
	GetLocalTime(&msg.time);

	/* Mailslot Logging (for sbbsctrl) */
	if(svc->log_handle != INVALID_HANDLE_VALUE /* Invalid log handle? */
		&& !GetMailslotInfo(svc->log_handle, NULL, NULL, NULL, NULL)) {
		/* Close and try to re-open */
		CloseHandle(svc->log_handle);
		svc->log_handle=INVALID_HANDLE_VALUE;
	}

	if(svc->log_handle == INVALID_HANDLE_VALUE) {
		sprintf(fname,"\\\\.\\mailslot\\%s.log",svc->name);
		svc->log_handle = CreateFile(
			fname,					/* pointer to name of the file */
			GENERIC_WRITE,			/* access (read-write) mode */
			FILE_SHARE_READ,		/* share mode */
			NULL,					/* pointer to security attributes */
			OPEN_EXISTING,			/* how to create */
			FILE_ATTRIBUTE_NORMAL,  /* file attributes */
			NULL					/* handle to file with attributes to copy */
			);
	}
	if(svc->log_handle != INVALID_HANDLE_VALUE) {
		len=sizeof(msg);
		if(!WriteFile(svc->log_handle,&msg,len,&wr,NULL) || wr!=len) {
			/* This most likely indicates the server closed the mailslot */
			sprintf(debug,"!ERROR %d writing %u bytes to %s pipe (wr=%d)"
				,GetLastError(),len,svc->name,wr);
			OutputDebugString(debug);
			/* So close the handle and attempt re-open next time */
			CloseHandle(svc->log_handle);
			svc->log_handle=INVALID_HANDLE_VALUE;
		}
	}
	
	/* Event Logging */
	if(level <= (*svc->log_level)) {
		if(svc->event_handle == NULL)
			svc->event_handle = RegisterEventSource(
				NULL,		/* server name for source (NULL = local computer) */
				svc->name   /* source name for registered handle */
				);
		if(svc->event_handle != NULL)
			ReportEvent(svc->event_handle,  /* event log handle */
				event_type(level),		/* event type */
				0,						/* category zero */
				0,						/* event identifier */
				NULL,					/* no user security identifier */
				1,						/* one string */
				0,						/* no data */
				(LPCSTR*)&str,			/* pointer to string array */
				NULL);					/* pointer to data */
	}

    return(0);
}

/************************************/
/* Shared Service Callback Routines */
/************************************/

static void svc_started(void* p)
{
	sbbs_ntsvc_t* svc = (sbbs_ntsvc_t*)p;

	svc->status.dwCurrentState=SERVICE_RUNNING;
	svc->status.dwControlsAccepted|=SERVICE_ACCEPT_STOP;
	SetServiceStatus(svc->status_handle, &svc->status);
}

static void read_ini(sbbs_ntsvc_t* svc)
{
	char	str[MAX_PATH*2];
	FILE*	fp;
	bbs_startup_t*		bbs_startup=NULL;
	ftp_startup_t*		ftp_startup=NULL;
	mail_startup_t*		mail_startup=NULL;
	services_startup_t*	services_startup=NULL;
	web_startup_t*		web_startup=NULL;

	if(svc==&bbs)
		bbs_startup=svc->startup;
	else if(svc==&ftp)
		ftp_startup=svc->startup;
	else if(svc==&web)
		web_startup=svc->startup;
	else if(svc==&mail)
		mail_startup=svc->startup;
	else if(svc==&services)
		services_startup=svc->startup;

	if((fp=fopen(ini_file,"r"))!=NULL) {
		sprintf(str,"Reading %s",ini_file);
		svc_lputs(NULL,LOG_INFO,str);
	}

	/* We call this function to set defaults, even if there's no .ini file */
	sbbs_read_ini(fp, ini_file
		,NULL	/* global_startup */
		,NULL	,bbs_startup
		,NULL	,ftp_startup 
		,NULL	,web_startup
		,NULL	,mail_startup 
		,NULL	,services_startup
		);

	/* close .ini file here */
	if(fp!=NULL)
		fclose(fp);
}

static void svc_recycle(void *p)
{
	read_ini((sbbs_ntsvc_t*)p);
}

static void svc_terminated(void* p, int code)
{
	sbbs_ntsvc_t* svc = (sbbs_ntsvc_t*)p;

	if(code) {
		svc->status.dwWin32ExitCode=ERROR_SERVICE_SPECIFIC_ERROR;
		svc->status.dwServiceSpecificExitCode=code;
		SetServiceStatus(svc->status_handle, &svc->status);
	}
}

static void svc_clients(void* p, int active)
{
	sbbs_ntsvc_t* svc = (sbbs_ntsvc_t*)p;
}

/***************/
/* ServiceMain */
/***************/

/* Common ServiceMain for all services */
static void WINAPI svc_main(sbbs_ntsvc_t* svc, DWORD argc, LPTSTR *argv)
{
	char	str[256];
	DWORD	i;
	char*	arg;

	for(i=0;i<argc;i++) {
		arg=argv[i];
		if(*arg=='-' || *arg=='/')
			arg++;
		if(!stricmp(arg,"debug"))
			svc->debug=TRUE;
		if(!stricmp(arg,"loglevel") && i+1<argc)
			(*svc->log_level)=strtol(argv[++i],NULL,0);
	}

	sprintf(str,"Starting NT Service: %s",svc->display_name);
	svc_lputs(svc,LOG_INFO,str);

    if((svc->status_handle = RegisterServiceCtrlHandler(svc->name, svc->ctrl_handler))==0) {
		sprintf(str,"!ERROR %d registering service control handler",GetLastError());
		svc_lputs(NULL,LOG_ERR,str);
		return;
	}

	memset(&svc->status,0,sizeof(SERVICE_STATUS));
	svc->status.dwServiceType=SERVICE_WIN32_SHARE_PROCESS;
	svc->status.dwControlsAccepted=SERVICE_ACCEPT_SHUTDOWN;
	svc->status.dwWaitHint=NTSVC_TIMEOUT_STARTUP;

	svc->status.dwCurrentState=SERVICE_START_PENDING;
	SetServiceStatus(svc->status_handle, &svc->status);

	read_ini(svc);

	svc->thread(svc->startup);

	svc->status.dwCurrentState=SERVICE_STOPPED;
	SetServiceStatus(svc->status_handle, &svc->status);

	if(svc->log_handle!=INVALID_HANDLE_VALUE) {
		CloseHandle(svc->log_handle);
		svc->log_handle=INVALID_HANDLE_VALUE;
	}

	if(svc->event_handle!=NULL) {
		DeregisterEventSource(svc->event_handle);
		svc->event_handle=NULL;
	}
}

/* Service-specific ServiceMain stub functions */

static void WINAPI bbs_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
	svc_main(&bbs, dwArgc, lpszArgv);

	/* Events are (currently) part of the BBS service */
	if(event.log_handle!=INVALID_HANDLE_VALUE) {
		CloseHandle(event.log_handle);
		event.log_handle=INVALID_HANDLE_VALUE;
	}
}

static void WINAPI ftp_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
	svc_main(&ftp, dwArgc, lpszArgv);
}

static void WINAPI web_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
	svc_main(&web, dwArgc, lpszArgv);
}

static void WINAPI mail_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
	svc_main(&mail, dwArgc, lpszArgv);
}

static void WINAPI services_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
	svc_main(&services, dwArgc, lpszArgv);
}

/******************************************/
/* NT Serivce Install/Uninstall Functions */
/******************************************/

/* ChangeServiceConfig2 is a Win2K+ API function, must call dynamically */
typedef WINADVAPI BOOL (WINAPI *ChangeServiceConfig2_t)(SC_HANDLE, DWORD, LPCVOID);

static void describe_service(HANDLE hSCMlib, SC_HANDLE hService, char* description)
{
	ChangeServiceConfig2_t changeServiceConfig2;
	static SERVICE_DESCRIPTION service_desc;
  
	if(hSCMlib==NULL)
		return;

	service_desc.lpDescription=description;

	if((changeServiceConfig2 = (ChangeServiceConfig2_t)GetProcAddress(hSCMlib, "ChangeServiceConfig2A"))!=NULL)
		changeServiceConfig2(hService, SERVICE_CONFIG_DESCRIPTION, &service_desc);
}

static BOOL register_event_source(char* name, char* path)
{
	char	keyname[256];
	HKEY	hKey;
	DWORD	type;
	DWORD	disp;
	LONG	retval;
	char*	value;

	sprintf(keyname,"system\\CurrentControlSet\\services\\eventlog\\application\\%s",name);

	retval=RegCreateKeyEx(
		HKEY_LOCAL_MACHINE,			/* handle to an open key */
		keyname,			/* address of subkey name */
		0,				/* reserved */
		"",				/* address of class string */
		0,				/* special options flag */
		KEY_ALL_ACCESS, /* desired security access */
		NULL,           /* address of key security structure */
		&hKey,			/* address of buffer for opened handle */
		&disp			/* address of disposition value buffer */
		);

	if(retval!=ERROR_SUCCESS) {
		fprintf(stderr,"!Error %d creating/opening registry key (HKLM\\%s)\n"
			,retval,keyname);
		return(FALSE);
	}

	value="EventMessageFile";
	retval=RegSetValueEx(
		hKey,			/* handle to key to set value for */
		value,			/* name of the value to set */
		0,				/* reserved */
		REG_SZ,			/* flag for value type */
		path,			/* address of value data */
		strlen(path)	/* size of value data */
		);

	if(retval!=ERROR_SUCCESS) {
		RegCloseKey(hKey);
		fprintf(stderr,"!Error %d setting registry key value (%s)\n"
			,retval,value);
		return(FALSE);
	}

	value="TypesSupported";
	type=EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
	retval=RegSetValueEx(
		hKey,			/* handle to key to set value for */
		value,			/* name of the value to set */
		0,				/* reserved */
		REG_DWORD,		/* flag for value type */
		(BYTE*)&type,	/* address of value data */
		sizeof(type)	/* size of value data */
		);

	RegCloseKey(hKey);

	if(retval!=ERROR_SUCCESS) {
		fprintf(stderr,"!Error %d setting registry key value (%s)\n"
			,retval,value);
		return(FALSE);
	}

	return(TRUE);
}

static const char* start_type_desc(DWORD start_type)
{
	static char str[128];

	switch(start_type) {
		case SERVICE_AUTO_START:			return("Startup: Automatic");
		case SERVICE_DEMAND_START:			return("Startup: Manual");
		case SERVICE_DISABLED:				return("Disabled");
		case SERVICE_NOT_INSTALLED:			return("Not installed");
	}

	sprintf(str,"Start_type: %d", start_type);
	return(str);
}

static const char* state_desc(DWORD state)
{
	static char str[128];

	switch(state) {
		case SERVICE_STOPPED:				return("Stopped");
		case SERVICE_START_PENDING:			return("Start Pending");
		case SERVICE_STOP_PENDING:			return("Stop Pending");
		case SERVICE_RUNNING:				return("Running");
		case SERVICE_CONTINUE_PENDING:		return("Continue Pending");
		case SERVICE_PAUSE_PENDING:			return("Pause Pending");
		case SERVICE_PAUSED:				return("Paused");
	}

	sprintf(str,"State: %d", state);
	return(str);
}

static const char* control_desc(DWORD ctrl)
{
	static char str[128];

	switch(ctrl) {
		case SERVICE_CONTROL_STOP:
			return("Stopping");
		case SERVICE_CONTROL_PAUSE:
			return("Pausing");
		case SERVICE_CONTROL_CONTINUE:
			return("Continuing");
		case SERVICE_CONTROL_INTERROGATE:
			return("Interrogating");
		case SERVICE_CONTROL_SHUTDOWN:
			return("Shutting-down");

		/* Synchronet-specific */
		case SERVICE_CONTROL_RECYCLE:
			return("Recycling");
		case SERVICE_CONTROL_MUTE:
			return("Muting");
		case SERVICE_CONTROL_UNMUTE:
			return("Un-muting");
		case SERVICE_CONTROL_SYSOP_AVAILABLE:
			return("Sysop Available");
		case SERVICE_CONTROL_SYSOP_UNAVAILABLE:	
			return("Sysop Unavailable");
	}
	sprintf(str,"Control: %d", ctrl);
	return(str);
}

/****************************************************************************/
/* Utility function to detect if a service is currently disabled			*/
/****************************************************************************/
static DWORD get_service_info(SC_HANDLE hSCManager, char* name, DWORD* state)
{
    SC_HANDLE		hService;
	DWORD			size;
	DWORD			err;
	DWORD			ret;
	SERVICE_STATUS	status;
	LPQUERY_SERVICE_CONFIG service_config;

	if(state!=NULL)
		*state=SERVICE_STOPPED;

	if((hService = OpenService(hSCManager, name, SERVICE_ALL_ACCESS))==NULL) {
		if((err=GetLastError())==ERROR_SERVICE_DOES_NOT_EXIST)
			return(SERVICE_NOT_INSTALLED);
		printf("\n!ERROR %d opening service: %s\n",err,name);
		return(-1);
	}

	if(QueryServiceConfig(
		hService,		/* handle of service */
		NULL,			/* address of service config. structure */
		0,				/* size of service configuration buffer */
		&size			/* address of variable for bytes needed */
		) || GetLastError()!=ERROR_INSUFFICIENT_BUFFER) {
		printf("\n!Unexpected QueryServiceConfig ERROR %u\n",err=GetLastError());
		return(-1);
	}

	if(state!=NULL && QueryServiceStatus(hService,&status))
		*state=status.dwCurrentState;

	if((service_config=malloc(size))==NULL) {
		printf("\n!ERROR allocating %u bytes of memory\n", size);
		return(-1);
	}

	if(!QueryServiceConfig(
		hService,		/* handle of service */
		service_config,	/* address of service config. structure */
		size,			/* size of service configuration buffer */
		&size			/* address of variable for bytes needed */
		)) {
		printf("\n!QueryServiceConfig ERROR %u\n",GetLastError());
		free(service_config);
		return(-1);
	}
    CloseServiceHandle(hService);
	ret=service_config->dwStartType;
	free(service_config);

	return ret;
}

/****************************************************************************/
/* Utility function to create a service with description (on Win2K+)		*/
/****************************************************************************/
static SC_HANDLE create_service(HANDLE hSCMlib, SC_HANDLE hSCManager
								,char* name, char* display_name, char* description, char* path
								,BOOL autostart)
{
    SC_HANDLE   hService;
	DWORD		err;
	DWORD		start_type = autostart ? SERVICE_AUTO_START : SERVICE_DEMAND_START;

	printf("Installing service: %-*s ... ", STRLEN_MAX_DISPLAY_NAME, display_name);

    hService = CreateService(
        hSCManager,						/* SCManager database */
        name,							/* name of service */
        display_name,					/* name to display */
        SERVICE_ALL_ACCESS,				/* desired access */
        SERVICE_WIN32_SHARE_PROCESS,	/* service type */
		start_type,						/* start type (auto or manual) */
        SERVICE_ERROR_NORMAL,			/* error control type */
        path,							/* service's binary */
        NULL,							/* no load ordering group */
        NULL,							/* no tag identifier */
        "",								/* dependencies */
        NULL,							/* LocalSystem account */
        NULL);							/* no password */

	if(hService==NULL) {
		if((err=GetLastError())==ERROR_SERVICE_EXISTS)
			printf("Already installed\n");
		else
			printf("!ERROR %d\n",err);
	}
	else {
		describe_service(hSCMlib, hService,description);
		CloseServiceHandle(hService);
		printf("%s\n", start_type_desc(start_type));

		register_event_source(name,path);
		register_event_source(NTSVC_NAME_EVENT,path);	/* Create SynchronetEvent event source */
	}

	return(hService);
}

/****************************************************************************/
/* Install one or all services												*/
/****************************************************************************/
static int install(const char* svc_name)
{
	int			i;
	HANDLE		hSCMlib;
    SC_HANDLE   hSCManager;
    char		path[MAX_PATH+1];

	printf("Installing Synchronet NT Services...\n");

	hSCMlib = LoadLibrary("ADVAPI32.DLL");

    if(GetModuleFileName(NULL,path,sizeof(path))==0)
    {
        fprintf(stderr,"!ERROR %d getting module file name\n",GetLastError());
        return(-1);
    }

    hSCManager = OpenSCManager(
                        NULL,                   /* machine (NULL == local) */
                        NULL,                   /* database (NULL == default) */
                        SC_MANAGER_ALL_ACCESS   /* access required */
                        );
    if(hSCManager==NULL) {
		fprintf(stderr,"!ERROR %d opening SC manager\n",GetLastError());
		return(-1);
	}

	for(i=0;ntsvc_list[i]!=NULL;i++)
		if(svc_name==NULL	/* All? */
			|| !stricmp(ntsvc_list[i]->name, svc_name)
			|| !stricmp(ntsvc_list[i]->name+STRLEN_SYNCHRONET, svc_name))
			create_service(hSCMlib
				,hSCManager
				,ntsvc_list[i]->name
				,ntsvc_list[i]->display_name
				,ntsvc_list[i]->description
				,path
				,ntsvc_list[i]->autostart);

	if(hSCMlib!=NULL)
		FreeLibrary(hSCMlib);

	CloseServiceHandle(hSCManager);

	return(0);
}

static SC_HANDLE open_service(SC_HANDLE hSCManager, char* name)
{
	SC_HANDLE	hService;
	DWORD		err;

	if((hService = OpenService(hSCManager, name, SERVICE_ALL_ACCESS))==NULL) {
		if((err=GetLastError())==ERROR_SERVICE_DOES_NOT_EXIST)
			printf("Not installed\n");
		else
			printf("\n!ERROR %d opening service: %s\n",err,name);
		return(NULL);
	}

	return(hService);
}

/****************************************************************************/
/* Utility function to remove a service cleanly (stopping if necessary)		*/
/****************************************************************************/
static void remove_service(SC_HANDLE hSCManager, char* name, char* disp_name)
{
    SC_HANDLE		hService;
	SERVICE_STATUS	status;

	printf("Removing: %-*s ... ", STRLEN_MAX_DISPLAY_NAME, disp_name);

    if((hService=open_service(hSCManager, name))==NULL)
		return;

    /* try to stop the service */
    if(ControlService( hService, SERVICE_CONTROL_STOP, &status))
    {
        printf("\nStopping: %-*s ... ", STRLEN_MAX_DISPLAY_NAME, disp_name);

        while(QueryServiceStatus(hService, &status) && status.dwCurrentState == SERVICE_STOP_PENDING)
			Sleep(1000);

        if(status.dwCurrentState == SERVICE_STOPPED)
            printf("Stopped, ");
        else
            printf("FAILED!, ");
    }

    /* now remove the service */
    if(DeleteService(hService))
		printf("Removed\n");
	else
		printf("!ERROR %d\n",GetLastError());
    CloseServiceHandle(hService);
}

/****************************************************************************/
/* Utility function to stop a service										*/
/****************************************************************************/
static void stop_service(SC_HANDLE hSCManager, char* name, char* disp_name)
{
    SC_HANDLE		hService;
	SERVICE_STATUS	status;

    printf("Stopping service: %-*s ... ", STRLEN_MAX_DISPLAY_NAME, disp_name);
    if((hService=open_service(hSCManager, name))==NULL)
		return;

    /* try to stop the service */
    if(ControlService( hService, SERVICE_CONTROL_STOP, &status))
    {
        while(QueryServiceStatus(hService, &status) && status.dwCurrentState == SERVICE_STOP_PENDING)
			Sleep(1000);

        if(status.dwCurrentState == SERVICE_STOPPED)
            printf("Stopped\n");
        else
            printf("FAILED!\n");
    } else
		printf("Already stopped\n");

    CloseServiceHandle(hService);
}

/****************************************************************************/
/* Utility function to stop a service										*/
/****************************************************************************/
static void control_service(SC_HANDLE hSCManager, char* name, char* disp_name, DWORD ctrl)
{
    SC_HANDLE		hService;
	SERVICE_STATUS	status;
	DWORD			err;

    printf("%s service: %-*s ... ", control_desc(ctrl), STRLEN_MAX_DISPLAY_NAME, disp_name);
    if((hService=open_service(hSCManager, name))==NULL)
		return;

    /* try to stop the service */
    if(!ControlService( hService, ctrl, &status)) {
		if((err=GetLastError())==ERROR_SERVICE_NOT_ACTIVE)
			printf("Not active\n");
		else
			printf("!ERROR %d\n",err);
    } else
		printf("Successful\n");

    CloseServiceHandle(hService);
}



/****************************************************************************/
/* Control one or all services												*/
/****************************************************************************/
static int control(const char* svc_name, DWORD ctrl)
{
	int			i;
    SC_HANDLE   hSCManager;

    hSCManager = OpenSCManager(
                        NULL,                   /* machine (NULL == local) */
                        NULL,                   /* database (NULL == default) */
                        SC_MANAGER_ALL_ACCESS   /* access required */
                        );
    if(hSCManager==NULL) {
		fprintf(stderr,"!ERROR %d opening SC manager\n",GetLastError());
		return(-1);
	}

	for(i=0;ntsvc_list[i]!=NULL;i++) {
#if 0
		if(svc_name==NULL 
			&& get_service_info(hSCManager, ntsvc_list[i]->name,NULL)==SERVICE_DISABLED)
			continue;
#endif
		if(svc_name==NULL	/* All? */
			|| !stricmp(ntsvc_list[i]->name, svc_name)
			|| !stricmp(ntsvc_list[i]->name+STRLEN_SYNCHRONET, svc_name))
			switch(ctrl) {
				case SERVICE_CONTROL_STOP:
					stop_service(hSCManager
						,ntsvc_list[i]->name
						,ntsvc_list[i]->display_name);
					break;
				default:
					control_service(hSCManager
						,ntsvc_list[i]->name
						,ntsvc_list[i]->display_name
						,ctrl);
					break;
		}
	}

	CloseServiceHandle(hSCManager);

	return(0);
}

/****************************************************************************/
/* Utility function to start a service										*/
/****************************************************************************/
static void start_service(SC_HANDLE hSCManager, char* name, char* disp_name
						  ,int argc, char** argv)
{
    SC_HANDLE		hService;
	SERVICE_STATUS	status;

    printf("Starting service: %-*s ... ", STRLEN_MAX_DISPLAY_NAME, disp_name);
    if((hService=open_service(hSCManager, name))==NULL)
		return;

	if(QueryServiceStatus(hService, &status) && status.dwCurrentState == SERVICE_RUNNING)
		printf("Already running\n");
	else {
		/* Start the service */
		if(StartService(hService, argc, (LPCTSTR*)argv))
		{
			while(QueryServiceStatus(hService, &status) && status.dwCurrentState == SERVICE_START_PENDING)
				Sleep(1000);

			if(status.dwCurrentState == SERVICE_RUNNING)
				printf("Started\n");
			else
				printf("FAILED!\n");
		} else
			printf("!ERROR %u\n", GetLastError());
	}

    CloseServiceHandle(hService);
}

/****************************************************************************/
/* Uninstall one or all services											*/
/****************************************************************************/
static int uninstall(const char* svc_name)
{
	int			i;
    SC_HANDLE   hSCManager;

    hSCManager = OpenSCManager(
                        NULL,                   /* machine (NULL == local) */
                        NULL,                   /* database (NULL == default) */
                        SC_MANAGER_ALL_ACCESS   /* access required */
                        );
    if(hSCManager==NULL) {
		fprintf(stderr,"!ERROR %d opening SC manager\n",GetLastError());
		return(-1);
	}

	for(i=0;ntsvc_list[i]!=NULL;i++)
		if(svc_name==NULL	/* All? */
			|| !stricmp(ntsvc_list[i]->name, svc_name)
			|| !stricmp(ntsvc_list[i]->name+STRLEN_SYNCHRONET, svc_name))
			remove_service(hSCManager
				,ntsvc_list[i]->name
				,ntsvc_list[i]->display_name);

	CloseServiceHandle(hSCManager);

	return(0);
}

/****************************************************************************/
/* Utility function to disable a service									*/
/****************************************************************************/
static void set_service_start_type(SC_HANDLE hSCManager, char* name
								   ,char* disp_name, DWORD start_type)
{
    SC_HANDLE		hService;

	printf("%s service: %-*s ... "
		,start_type==SERVICE_DISABLED ? "Disabling" : "Enabling"
		,STRLEN_MAX_DISPLAY_NAME, disp_name);

    if((hService=open_service(hSCManager, name))==NULL)
		return;

	if(!ChangeServiceConfig(
		hService,				/* handle to service */
		SERVICE_NO_CHANGE,		/* type of service */
		start_type,				/* when to start service */
		SERVICE_NO_CHANGE,		/* severity if service fails to start */
		NULL,					/* pointer to service binary file name */
		NULL,					/* pointer to load ordering group name */
		NULL,					/* pointer to variable to get tag identifier */
		NULL,					/* pointer to array of dependency names */
		NULL,					/* pointer to account name of service */
		NULL,					/* pointer to password for service account */
		NULL					/* pointer to display name */
		))
		printf("\n!ERROR %d changing service config for: %s\n",GetLastError(),name);
	else
		printf("%s\n", start_type_desc(start_type));

    CloseServiceHandle(hService);
}

/****************************************************************************/
/* Enable (set to auto-start) or disable one or all services				*/
/****************************************************************************/
static int enable(const char* svc_name, BOOL enabled)
{
	int			i;
    SC_HANDLE   hSCManager;

    hSCManager = OpenSCManager(
                        NULL,                   /* machine (NULL == local) */
                        NULL,                   /* database (NULL == default) */
                        SC_MANAGER_ALL_ACCESS   /* access required */
                        );
    if(hSCManager==NULL) {
		fprintf(stderr,"!ERROR %d opening SC manager\n",GetLastError());
		return(-1);
	}

	for(i=0;ntsvc_list[i]!=NULL;i++)
		if(svc_name==NULL	/* All? */
			|| !stricmp(ntsvc_list[i]->name, svc_name)
			|| !stricmp(ntsvc_list[i]->name+STRLEN_SYNCHRONET, svc_name))
			set_service_start_type(hSCManager
				,ntsvc_list[i]->name
				,ntsvc_list[i]->display_name
				,enabled ? (ntsvc_list[i]->autostart ? SERVICE_AUTO_START : SERVICE_DEMAND_START)
					: SERVICE_DISABLED);

	CloseServiceHandle(hSCManager);

	return(0);
}

/****************************************************************************/
/* List one or all services													*/
/****************************************************************************/
static int list(const char* svc_name)
{
	int			i;
    SC_HANDLE   hSCManager;
	DWORD		state;
	DWORD		start_type;

    hSCManager = OpenSCManager(
                        NULL,                   /* machine (NULL == local) */
                        NULL,                   /* database (NULL == default) */
                        SC_MANAGER_ALL_ACCESS   /* access required */
                        );
    if(hSCManager==NULL) {
		fprintf(stderr,"!ERROR %d opening SC manager\n",GetLastError());
		return(-1);
	}

	for(i=0;ntsvc_list[i]!=NULL;i++) {
		if(svc_name==NULL 
			|| !stricmp(ntsvc_list[i]->name, svc_name)
			|| !stricmp(ntsvc_list[i]->name+STRLEN_SYNCHRONET, svc_name)) {
			start_type = get_service_info(hSCManager, ntsvc_list[i]->name, &state);
			printf("%-*s ... %s, %s\n"
				,STRLEN_MAX_DISPLAY_NAME, ntsvc_list[i]->display_name
				,start_type_desc(start_type)
				,state_desc(state));
		}
	}

	CloseServiceHandle(hSCManager);

	return(0);
}

/****************************************************************************/
/* Start one or all services												*/
/****************************************************************************/
static int start(const char* svc_name, int argc, char** argv)
{
	int			i;
    SC_HANDLE   hSCManager;

    hSCManager = OpenSCManager(
                        NULL,                   /* machine (NULL == local) */
                        NULL,                   /* database (NULL == default) */
                        SC_MANAGER_ALL_ACCESS   /* access required */
                        );
    if(hSCManager==NULL) {
		fprintf(stderr,"!ERROR %d opening SC manager\n",GetLastError());
		return(-1);
	}

	for(i=0;ntsvc_list[i]!=NULL;i++) {
		if(svc_name==NULL 
			&& get_service_info(hSCManager, ntsvc_list[i]->name,NULL)==SERVICE_DISABLED)
			continue;
		if(svc_name==NULL 
			|| !stricmp(ntsvc_list[i]->name, svc_name)
			|| !stricmp(ntsvc_list[i]->name+STRLEN_SYNCHRONET, svc_name))
			start_service(hSCManager,ntsvc_list[i]->name,ntsvc_list[i]->display_name
				,argc,argv);
	}

	CloseServiceHandle(hSCManager);

	return(0);
}

/****************************************************************************/
/* Main Entry Point															*/
/****************************************************************************/
int main(int argc, char** argv)
{
	char*	ctrl_dir;
	char*	arg;
	char*	p;
	char	str[MAX_PATH+1];
	int		i;
	FILE*	fp=NULL;
	BOOL	start_services=TRUE;

	SERVICE_TABLE_ENTRY  ServiceDispatchTable[] = 
    { 
        { NTSVC_NAME_BBS,		bbs_start		}, 
		{ NTSVC_NAME_FTP,		ftp_start		},
		{ NTSVC_NAME_WEB,		web_start		},
		{ NTSVC_NAME_MAIL,		mail_start		},
		{ NTSVC_NAME_SERVICES,	services_start	},
        { NULL,					NULL			}	/* Terminator */
    }; 

	printf("\nSynchronet NT Services  Version %s%c  %s\n\n"
		,VERSION,REVISION,COPYRIGHT_NOTICE);

	loginAttemptListInit(&login_attempt_list);

	ctrl_dir = get_ctrl_dir();

	sbbs_get_ini_fname(ini_file, ctrl_dir, NULL /* auto-host_name */);

	/* Initialize BBS startup structure */
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);
	bbs_startup.cbdata=&bbs;
	bbs_startup.lputs=svc_lputs;
	bbs_startup.event_cbdata=&event;
	bbs_startup.event_lputs=svc_lputs;
    bbs_startup.started=svc_started;
	bbs_startup.recycle=svc_recycle;
    bbs_startup.terminated=svc_terminated;
	bbs_startup.clients=svc_clients;
	bbs_startup.login_attempt_list=&login_attempt_list;
    strcpy(bbs_startup.ctrl_dir,ctrl_dir);

	/* Initialize FTP startup structure */
    memset(&ftp_startup,0,sizeof(ftp_startup));
	ftp_startup.cbdata=&ftp;
    ftp_startup.size=sizeof(ftp_startup);
	ftp_startup.lputs=svc_lputs;
    ftp_startup.started=svc_started;
	ftp_startup.recycle=svc_recycle;
    ftp_startup.terminated=svc_terminated;
	ftp_startup.clients=svc_clients;
	ftp_startup.login_attempt_list=&login_attempt_list;
    strcpy(ftp_startup.ctrl_dir,ctrl_dir);

	/* Initialize Web Server startup structure */
    memset(&web_startup,0,sizeof(web_startup));
	web_startup.cbdata=&web;
    web_startup.size=sizeof(web_startup);
	web_startup.lputs=svc_lputs;
    web_startup.started=svc_started;
	web_startup.recycle=svc_recycle;
    web_startup.terminated=svc_terminated;
	web_startup.clients=svc_clients;
	web_startup.login_attempt_list=&login_attempt_list;
    strcpy(web_startup.ctrl_dir,ctrl_dir);

	/* Initialize Mail Server startup structure */
    memset(&mail_startup,0,sizeof(mail_startup));
	mail_startup.cbdata=&mail;
    mail_startup.size=sizeof(mail_startup);
	mail_startup.lputs=svc_lputs;
    mail_startup.started=svc_started;
	mail_startup.recycle=svc_recycle;
    mail_startup.terminated=svc_terminated;
	mail_startup.clients=svc_clients;
	mail_startup.login_attempt_list=&login_attempt_list;
    strcpy(mail_startup.ctrl_dir,ctrl_dir);

	/* Initialize Services startup structure */
    memset(&services_startup,0,sizeof(services_startup));
	services_startup.cbdata=&services;
    services_startup.size=sizeof(services_startup);
	services_startup.lputs=svc_lputs;
    services_startup.started=svc_started;
	services_startup.recycle=svc_recycle;
    services_startup.terminated=svc_terminated;
	services_startup.clients=svc_clients;
	services_startup.login_attempt_list=&login_attempt_list;
    strcpy(services_startup.ctrl_dir,ctrl_dir);

	/* Read .ini file here */
	if((fp=fopen(ini_file,"r"))!=NULL) {
		sprintf(str,"Reading %s",ini_file);
		svc_lputs(NULL,LOG_INFO,str);
	}

	/* We call this function to set defaults, even if there's no .ini file */
	sbbs_read_ini(fp, ini_file
		,NULL	/* global_startup */
		,&bbs.autostart			,NULL
		,&ftp.autostart			,NULL
		,&web.autostart			,NULL
		,&mail.autostart		,NULL
		,&services.autostart	,NULL
		);

	/* close .ini file here */
	if(fp!=NULL)
		fclose(fp);

	if(chdir(ctrl_dir)!=0) {
		sprintf(str,"!ERROR %d (%s) changing directory to: %s", errno, strerror(errno), ctrl_dir);
		svc_lputs(NULL,LOG_ERR,str);
	}

	for(i=1;i<argc;i++) {
		arg=argv[i];
		while(*arg=='-' || *arg=='/')
			arg++;
		if(!stricmp(arg,"help") || *arg=='?')
			start_services=FALSE;

		if(!stricmp(arg,"list"))
			return list(argv[i+1]);

		if(!stricmp(arg,"install"))
			return install(argv[i+1]);

		if(!stricmp(arg,"remove"))
			return uninstall(argv[i+1]);

		if(!stricmp(arg,"disable"))
			return enable(argv[i+1], FALSE);

		if(!stricmp(arg,"enable"))
			return enable(argv[i+1], TRUE);

		if(!stricmp(arg,"stop"))
			return control(argv[i+1],SERVICE_CONTROL_STOP);

		if(!stricmp(arg,"start"))
			return start(argv[i+1],argc,argv);

		if(!stricmp(arg,"recycle"))
			return control(argv[i+1],SERVICE_CONTROL_RECYCLE);

		if(!stricmp(arg,"mute"))
			return control(argv[i+1],SERVICE_CONTROL_MUTE);

		if(!stricmp(arg,"unmute"))
			return control(argv[i+1],SERVICE_CONTROL_UNMUTE);
	}

	if(start_services) {
		printf("Starting service control dispatcher.\n" );
		printf("This may take several seconds.  Please wait.\n" );

		if(StartServiceCtrlDispatcher(ServiceDispatchTable))
			return(0);

		sprintf(str,"!ERROR %u starting service control dispatcher",GetLastError());
		printf("%s\n\n",str);
		OutputDebugString(str); 
	}

	SAFECOPY(str,getfname(argv[0]));
	if((p=getfext(str))!=NULL)
		*p=0;

	printf("Usage: %s [command] [service]\n", str);

	printf("\nIf service name not specified, default is ALL services.\n");

	printf("\nAvailable Commands:\n\n");
	printf("%-20s %s\n","list","to list services");
    printf("%-20s %s\n","install","to install services");
    printf("%-20s %s\n","remove","to remove services");
    printf("%-20s %s\n","disable","to disable services");
    printf("%-20s %s\n","enable","to re-enable disabled services");
    printf("%-20s %s\n","start","to start services");
    printf("%-20s %s\n","stop","to stop services");
	printf("%-20s %s\n","recycle","to recycle services");
	printf("%-20s %s\n","mute","to mute (sounds of) services");
	printf("%-20s %s\n","unmute","to unmute (sounds of) services");

	printf("\nAvailable Services:\n\n");
	printf("%-20s %s\n","Name","Description");
	printf("%-20s %s\n","----","-----------");
	for(i=0;ntsvc_list[i]!=NULL;i++)
		printf("%-20s %s\n",ntsvc_list[i]->name+STRLEN_SYNCHRONET,ntsvc_list[i]->display_name);

	return(0);
}
