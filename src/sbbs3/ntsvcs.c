/* ntsrvcs.c */

/* Synchronet BBS as a set of Windows NT Services */

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

/* Synchronet-specific headers */
#include "sbbs.h"		/* various */
#include "sbbs_ini.h"	/* sbbs_read_ini() */
#include "ftpsrvr.h"	/* ftp_startup_t, ftp_server */
#include "websrvr.h"	/* web_startup_t, web_server */
#include "mailsrvr.h"	/* mail_startup_t, mail_server */
#include "services.h"	/* services_startup_t, services_thread */

/* Windows-specific headers */
#include <winsvc.h>

/* Temporary: Do not include web server in 3.1x-Win32 release build */
#if defined(_MSC_VER)
	#define NO_WEB_SERVER
#endif

/* Services doesn't work without JavaScript support */
#if !defined(JAVASCRIPT)
	#define	NO_SERVICES
#endif

#define SBBS_NTSVC_BBS_NAME		"SynchronetBBS"
#define SBBS_NTSVC_BBS_DISP		"Synchronet Telnet/RLogin Server"
#define SBBS_NTSVC_BBS_DESC		"Provides support for Telnet and RLogin clients and executes timed events. " \
								"This service provides the critical functions of your Synchronet BBS."

#define SBBS_NTSVC_FTP_NAME		"SynchronetFTP"
#define SBBS_NTSVC_FTP_DISP		"Synchronet FTP Server"
#define SBBS_NTSVC_FTP_DESC		"Provides support for FTP clients (including web browsers) for file transfers."

#define SBBS_NTSVC_WEB_NAME		"SynchronetWeb"
#define SBBS_NTSVC_WEB_DISP		"Synchronet Web Server"
#define SBBS_NTSVC_WEB_DESC		"Provides support for Web (HTML/HTTP) clients (browsers)."

#define SBBS_NTSVC_MAIL_NAME	"SynchronetMail"
#define SBBS_NTSVC_MAIL_DISP	"Synchronet SMTP/POP3 Mail Server"
#define SBBS_NTSVC_MAIL_DESC	"Sends and receives Internet e-mail (using SMTP) and allows users to remotely " \
								"access their e-mail using an Internet mail client (using POP3)."

#define SBBS_NTSVC_SERV_NAME	"SynchronetServices"
#define SBBS_NTSVC_SERV_DISP	"Synchronet Services"
#define SBBS_NTSVC_SERV_DESC	"Plugin servers (usually in JavaScript) for any TCP/UDP protocol. " \
								"Stock services include Finger, Gopher, NNTP, and IRC. Edit your ctrl/services.ini " \
								"file for configuration of individual Synchronet Services."
							
/* Global variables */

SERVICE_STATUS			bbs_status;
SERVICE_STATUS_HANDLE	bbs_status_handle;
SERVICE_STATUS			ftp_status;
SERVICE_STATUS_HANDLE	ftp_status_handle;
SERVICE_STATUS			web_status;
SERVICE_STATUS_HANDLE	web_status_handle;
SERVICE_STATUS			mail_status;
SERVICE_STATUS_HANDLE	mail_status_handle;
SERVICE_STATUS			services_status;
SERVICE_STATUS_HANDLE	services_status_handle;

bbs_startup_t		bbs_startup;
ftp_startup_t		ftp_startup;
mail_startup_t		mail_startup;
services_startup_t	services_startup;
web_startup_t		web_startup;

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

/****************************************************************************/
/* BBS local/log print routine												*/
/****************************************************************************/
static int bbs_lputs(char *str)
{
    return(0);
}

/****************************************************************************/
/* Event thread local/log print routine										*/
/****************************************************************************/
static int event_lputs(char *str)
{
    return 0;
}

static void bbs_started(void)
{
	bbs_status.dwCurrentState=SERVICE_RUNNING;
	bbs_status.dwControlsAccepted=SERVICE_ACCEPT_STOP;
	SetServiceStatus(bbs_status_handle, &bbs_status);
}

static void bbs_terminated(int code)
{
	if(code) {
		bbs_status.dwWin32ExitCode=ERROR_SERVICE_SPECIFIC_ERROR;
		bbs_status.dwServiceSpecificExitCode=code;
		SetServiceStatus(bbs_status_handle, &bbs_status);
	}
}

static void WINAPI bbs_ctrl_handler(DWORD dwCtrlCode)
{
	switch(dwCtrlCode) {
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			bbs_terminate();
			bbs_status.dwCurrentState=SERVICE_STOP_PENDING;
			break;
	}
	SetServiceStatus(bbs_status_handle, &bbs_status);
}

static void WINAPI bbs_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
    if((bbs_status_handle = RegisterServiceCtrlHandler(SBBS_NTSVC_BBS_NAME, bbs_ctrl_handler))==0) {
		fprintf(stderr,"!ERROR %d registering service control handler\n",GetLastError());
		return;
	}

	memset(&bbs_status,0,sizeof(bbs_status));
	bbs_status.dwServiceType=SERVICE_WIN32_SHARE_PROCESS;
	bbs_status.dwWaitHint=30000;	/* milliseconds */

	bbs_status.dwCurrentState=SERVICE_START_PENDING;
	SetServiceStatus(bbs_status_handle, &bbs_status);

	bbs_thread(&bbs_startup);

	bbs_status.dwCurrentState=SERVICE_STOPPED;
	SetServiceStatus(bbs_status_handle, &bbs_status);
}


/****************************************************************************/
/* FTP local/log print routine												*/
/****************************************************************************/
static int ftp_lputs(char *str)
{
	return 0;
}

static void ftp_started(void)
{
	ftp_status.dwCurrentState=SERVICE_RUNNING;
	ftp_status.dwControlsAccepted=SERVICE_ACCEPT_STOP;
	SetServiceStatus(ftp_status_handle, &ftp_status);
}

static void ftp_terminated(int code)
{
	if(code) {
		ftp_status.dwWin32ExitCode=ERROR_SERVICE_SPECIFIC_ERROR;
		ftp_status.dwServiceSpecificExitCode=code;
		SetServiceStatus(ftp_status_handle, &ftp_status);
	}
}

static void WINAPI ftp_ctrl_handler(DWORD dwCtrlCode)
{
	switch(dwCtrlCode) {
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			ftp_terminate();
			ftp_status.dwCurrentState=SERVICE_STOP_PENDING;
			break;
	}
	SetServiceStatus(ftp_status_handle, &ftp_status);
}

static void WINAPI
ftp_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
    if((ftp_status_handle = RegisterServiceCtrlHandler(SBBS_NTSVC_FTP_NAME, ftp_ctrl_handler))==0) {
		fprintf(stderr,"!ERROR %d registering service control handler\n",GetLastError());
		return;
	}

	memset(&ftp_status,0,sizeof(ftp_status));
	ftp_status.dwServiceType=SERVICE_WIN32_SHARE_PROCESS;
	ftp_status.dwWaitHint=30000;	/* milliseconds */

	ftp_status.dwCurrentState=SERVICE_START_PENDING;
	SetServiceStatus(ftp_status_handle, &ftp_status);

	ftp_server(&ftp_startup);

	ftp_status.dwCurrentState=SERVICE_STOPPED;
	SetServiceStatus(ftp_status_handle, &ftp_status);
}

/****************************************************************************/
/* Web local/log print routine												*/
/****************************************************************************/
static int web_lputs(char *str)
{
    return 0;
}

static void web_started(void)
{
	web_status.dwCurrentState=SERVICE_RUNNING;
	web_status.dwControlsAccepted=SERVICE_ACCEPT_STOP;
	SetServiceStatus(web_status_handle, &web_status);
}

static void web_terminated(int code)
{
	if(code) {
		web_status.dwWin32ExitCode=ERROR_SERVICE_SPECIFIC_ERROR;
		web_status.dwServiceSpecificExitCode=code;
		SetServiceStatus(web_status_handle, &web_status);
	}
}

static void WINAPI web_ctrl_handler(DWORD dwCtrlCode)
{
	switch(dwCtrlCode) {
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			web_terminate();
			web_status.dwCurrentState=SERVICE_STOP_PENDING;
			break;
	}
	SetServiceStatus(web_status_handle, &web_status);
}

static void WINAPI
web_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
    if((web_status_handle = RegisterServiceCtrlHandler(SBBS_NTSVC_WEB_NAME, web_ctrl_handler))==0) {
		fprintf(stderr,"!ERROR %d registering service control handler\n",GetLastError());
		return;
	}

	memset(&web_status,0,sizeof(web_status));
	web_status.dwServiceType=SERVICE_WIN32_SHARE_PROCESS;
	web_status.dwWaitHint=30000;	/* milliseconds */

	web_status.dwCurrentState=SERVICE_START_PENDING;
	SetServiceStatus(web_status_handle, &web_status);

#if !defined(NO_WEB_SERVER)
	web_server(&web_startup);
#endif

	web_status.dwCurrentState=SERVICE_STOPPED;
	SetServiceStatus(web_status_handle, &web_status);
}

/****************************************************************************/
/* Mail Server local/log print routine										*/
/****************************************************************************/
static int mail_lputs(char *str)
{
	return 0;
}

static void mail_started(void)
{
	mail_status.dwCurrentState=SERVICE_RUNNING;
	mail_status.dwControlsAccepted=SERVICE_ACCEPT_STOP;
	SetServiceStatus(mail_status_handle, &mail_status);
}

static void mail_terminated(int code)
{
	if(code) {
		mail_status.dwWin32ExitCode=ERROR_SERVICE_SPECIFIC_ERROR;
		mail_status.dwServiceSpecificExitCode=code;
		SetServiceStatus(mail_status_handle, &mail_status);
	}
}

static void WINAPI mail_ctrl_handler(DWORD dwCtrlCode)
{
	switch(dwCtrlCode) {
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			mail_terminate();
			mail_status.dwCurrentState=SERVICE_STOP_PENDING;
			break;
	}
	SetServiceStatus(mail_status_handle, &mail_status);
}

static void WINAPI
mail_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
    if((mail_status_handle = RegisterServiceCtrlHandler(SBBS_NTSVC_MAIL_NAME, mail_ctrl_handler))==0) {
		fprintf(stderr,"!ERROR %d registering service control handler\n",GetLastError());
		return;
	}

	memset(&mail_status,0,sizeof(mail_status));
	mail_status.dwServiceType=SERVICE_WIN32_SHARE_PROCESS;
	mail_status.dwWaitHint=30000;	/* milliseconds */

	mail_status.dwCurrentState=SERVICE_START_PENDING;
	SetServiceStatus(mail_status_handle, &mail_status);

	mail_server(&mail_startup);

	mail_status.dwCurrentState=SERVICE_STOPPED;
	SetServiceStatus(mail_status_handle, &mail_status);
}


/****************************************************************************/
/* Services local/log print routine											*/
/****************************************************************************/
static int services_lputs(char *str)
{
	return 0;
}

static void services_started(void)
{
	services_status.dwCurrentState=SERVICE_RUNNING;
	services_status.dwControlsAccepted=SERVICE_ACCEPT_STOP;
	SetServiceStatus(services_status_handle, &services_status);
}

static void services_terminated(int code)
{
	if(code) {
		services_status.dwWin32ExitCode=ERROR_SERVICE_SPECIFIC_ERROR;
		services_status.dwServiceSpecificExitCode=code;
		SetServiceStatus(services_status_handle, &services_status);
	}
}

static void WINAPI services_ctrl_handler(DWORD dwCtrlCode)
{
	switch(dwCtrlCode) {
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			services_terminate();
			services_status.dwCurrentState=SERVICE_STOP_PENDING;
			break;
	}
	SetServiceStatus(services_status_handle, &services_status);
}

static void WINAPI 
services_start(DWORD dwArgc, LPTSTR *lpszArgv)
{
    if((services_status_handle = RegisterServiceCtrlHandler(SBBS_NTSVC_SERV_NAME, services_ctrl_handler))==0) {
		fprintf(stderr,"!ERROR %d registering service control handler\n",GetLastError());
		return;
	}

	memset(&services_status,0,sizeof(services_status));
	services_status.dwServiceType=SERVICE_WIN32_SHARE_PROCESS;
	services_status.dwWaitHint=30000;	/* milliseconds */

	services_status.dwCurrentState=SERVICE_START_PENDING;
	SetServiceStatus(services_status_handle, &services_status);

	services_thread(&services_startup);

	services_status.dwCurrentState=SERVICE_STOPPED;
	SetServiceStatus(services_status_handle, &services_status);
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

static SC_HANDLE create_service(HANDLE hSCMlib, SC_HANDLE hSCManager
								,char* name, char* DISP_name, char* description, char* path)
{
    SC_HANDLE   hService;

	printf("Installing service: %-40s ... ", DISP_name);

    hService = CreateService(
        hSCManager,					// SCManager database
        name,						// name of service
        DISP_name,				// name to display
        SERVICE_ALL_ACCESS,         // desired access
        SERVICE_WIN32_OWN_PROCESS,  // service type
		SERVICE_AUTO_START,			// start type (auto or manual)
        SERVICE_ERROR_NORMAL,       // error control type
        path,						// service's binary
        NULL,                       // no load ordering group
        NULL,                       // no tag identifier
        "",							// dependencies
        NULL,                       // LocalSystem account
        NULL);                      // no password

	if(hService==NULL)
		printf("!ERROR %d\n",GetLastError());
	else {
		describe_service(hSCMlib, hService,description);
		CloseServiceHandle(hService);
		printf("Successful\n");
	}

	return(hService);
}


static int install(void)
{
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
                        NULL,                   // machine (NULL == local)
                        NULL,                   // database (NULL == default)
                        SC_MANAGER_ALL_ACCESS   // access required
                        );
    if(hSCManager==NULL) {
		fprintf(stderr,"!ERROR %d opening SC manager\n",GetLastError());
		return(-1);
	}

	create_service(hSCMlib, hSCManager, SBBS_NTSVC_BBS_NAME,	SBBS_NTSVC_BBS_DISP,	SBBS_NTSVC_BBS_DESC, path);
	create_service(hSCMlib, hSCManager, SBBS_NTSVC_FTP_NAME,	SBBS_NTSVC_FTP_DISP,	SBBS_NTSVC_FTP_DESC, path);
#if !defined(NO_WEB_SERVER)
	create_service(hSCMlib, hSCManager, SBBS_NTSVC_WEB_NAME,	SBBS_NTSVC_WEB_DISP,	SBBS_NTSVC_WEB_DESC, path);
#endif
	create_service(hSCMlib, hSCManager, SBBS_NTSVC_MAIL_NAME,	SBBS_NTSVC_MAIL_DISP,	SBBS_NTSVC_MAIL_DESC, path);
	create_service(hSCMlib, hSCManager, SBBS_NTSVC_SERV_NAME,	SBBS_NTSVC_SERV_DISP,	SBBS_NTSVC_SERV_DESC, path);

	if(hSCMlib!=NULL)
		FreeLibrary(hSCMlib);

	CloseServiceHandle(hSCManager);

	return(0);
}

static void remove_service(SC_HANDLE hSCManager, char* name, char* DISP_name)
{
    SC_HANDLE		hService;
	SERVICE_STATUS	status;

	printf("Removing service: %-40s ... ", DISP_name);

    hService = OpenService(hSCManager, name, SERVICE_ALL_ACCESS);

	if(hService==NULL) {
		printf("\n!ERROR %d opening service: %s\n",GetLastError(),name);
		return;
	}

    // try to stop the service
    if(ControlService( hService, SERVICE_CONTROL_STOP, &status))
    {
        printf("\nStopping: %s ... ",name);

        while(QueryServiceStatus(hService, &status) && status.dwCurrentState == SERVICE_STOP_PENDING)
			Sleep(1000);

        if(status.dwCurrentState == SERVICE_STOPPED)
            printf("Stopped.\n");
        else
            printf("FAILED!\n");
    }

    // now remove the service
    if(DeleteService(hService))
		printf("Successful\n");
	else
		printf("!ERROR %d\n",GetLastError());
    CloseServiceHandle(hService);
}


static int uninstall(void)
{
    SC_HANDLE   hSCManager;

    hSCManager = OpenSCManager(
                        NULL,                   // machine (NULL == local)
                        NULL,                   // database (NULL == default)
                        SC_MANAGER_ALL_ACCESS   // access required
                        );
    if(hSCManager==NULL) {
		fprintf(stderr,"!ERROR %d opening SC manager\n",GetLastError());
		return(-1);
	}

	remove_service(hSCManager, SBBS_NTSVC_BBS_NAME,		SBBS_NTSVC_BBS_DISP);
	remove_service(hSCManager, SBBS_NTSVC_FTP_NAME,		SBBS_NTSVC_FTP_DISP);
#if !defined(NO_WEB_SERVER)
	remove_service(hSCManager, SBBS_NTSVC_WEB_NAME,		SBBS_NTSVC_WEB_DISP);
#endif
	remove_service(hSCManager, SBBS_NTSVC_MAIL_NAME,	SBBS_NTSVC_MAIL_DISP);
	remove_service(hSCManager, SBBS_NTSVC_SERV_NAME,	SBBS_NTSVC_SERV_DISP);

	CloseServiceHandle(hSCManager);

	return(0);
}


/****************************************************************************/
/* Main Entry Point															*/
/****************************************************************************/
int main(int argc, char** argv)
{
	char*	ctrl_dir;
	char	str[MAX_PATH+1];
	char	ini_file[MAX_PATH+1];
	char	host_name[128]="";
	FILE*	fp=NULL;

	SERVICE_TABLE_ENTRY  ServiceDispatchTable[] = 
    { 
        { SBBS_NTSVC_BBS_NAME,	bbs_start		}, 
		{ SBBS_NTSVC_FTP_NAME,	ftp_start		},
#if !defined(NO_WEB_SERVER)
		{ SBBS_NTSVC_WEB_NAME,	web_start		},
#endif
		{ SBBS_NTSVC_MAIL_NAME,	mail_start		},
		{ SBBS_NTSVC_SERV_NAME,	services_start	},
        { NULL,					NULL			}	/* Terminator */
    }; 

	printf("\nSynchronet NT Services  Version %s%c  %s\n\n"
		,VERSION,REVISION,COPYRIGHT_NOTICE);

	if(argc>1 && !stricmp(argv[1],"-install"))
		return install();

	if(argc>1 && !stricmp(argv[1],"-remove"))
		return uninstall();

    printf("%s -install          to install the services\n", getfname(argv[0]) );
    printf("%s -remove           to remove the services\n", getfname(argv[0]) );

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
	if(!fexistcase(ini_file))
		sprintf(ini_file,"%s%csbbs.ini",ctrl_dir,PATH_DELIM);

	/* Initialize BBS startup structure */
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);

	bbs_startup.lputs=bbs_lputs;
	bbs_startup.event_log=event_lputs;
    bbs_startup.started=bbs_started;
    bbs_startup.terminated=bbs_terminated;
    strcpy(bbs_startup.ctrl_dir,ctrl_dir);

	/* Initialize FTP startup structure */
    memset(&ftp_startup,0,sizeof(ftp_startup));
    ftp_startup.size=sizeof(ftp_startup);
	ftp_startup.lputs=ftp_lputs;
    ftp_startup.started=ftp_started;
    ftp_startup.terminated=ftp_terminated;
    strcpy(ftp_startup.ctrl_dir,ctrl_dir);

	/* Initialize Web Server startup structure */
    memset(&web_startup,0,sizeof(web_startup));
    web_startup.size=sizeof(web_startup);
	web_startup.lputs=web_lputs;
    web_startup.started=web_started;
    web_startup.terminated=web_terminated;
    strcpy(web_startup.ctrl_dir,ctrl_dir);

	/* Initialize Mail Server startup structure */
    memset(&mail_startup,0,sizeof(mail_startup));
    mail_startup.size=sizeof(mail_startup);
	mail_startup.lputs=mail_lputs;
    mail_startup.started=mail_started;
    mail_startup.terminated=mail_terminated;
    strcpy(mail_startup.ctrl_dir,ctrl_dir);

	/* Initialize Services startup structure */
    memset(&services_startup,0,sizeof(services_startup));
    services_startup.size=sizeof(services_startup);
	services_startup.lputs=services_lputs;
    services_startup.started=services_started;
    services_startup.terminated=services_terminated;
    strcpy(services_startup.ctrl_dir,ctrl_dir);

	/* Read .ini file here */
	if(ini_file[0]!=0 && (fp=fopen(ini_file,"r"))!=NULL) {
		sprintf(str,"Reading %s",ini_file);
		bbs_lputs(str);
	}

	/* We call this function to set defaults, even if there's no .ini file */
	sbbs_read_ini(fp, 
		NULL,	&bbs_startup,
		NULL,	&ftp_startup, 
		NULL,	&web_startup,
		NULL,	&mail_startup, 
		NULL,	&services_startup);

	/* close .ini file here */
	if(fp!=NULL)
		fclose(fp);

    printf("\nStartServiceCtrlDispatcher being called.\n" );
    printf("This may take several seconds.  Please wait.\n" );

	if(!StartServiceCtrlDispatcher(ServiceDispatchTable)) 
    { 
		sprintf(str,"!Synchronet StartServiceCtrlDispatcher ERROR %d",GetLastError());
		printf("%s\n",str);
        OutputDebugString(str); 
    } 

	return(0);
}
