/* sexpots.c */

/* Synchronet External Plain Old Telephone System (POTS) support */

/* $Id: sexpots.c,v 1.32 2019/05/05 22:48:33 rswindell Exp $ */

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

/* ANSI C */
#include <stdarg.h>
#include <stdio.h>

/* xpdev lib */
#include "dirwrap.h"
#include "datewrap.h"
#include "sockwrap.h"
#include "ini_file.h"

/* comio lib */
#include "comio.h"

/* sbbs */
#include "telnet.h"

/* constants */
#define NAME					"SEXPOTS"
#define TITLE					"Synchronet External POTS Support"
#define DESCRIPTION				"Connects a communications port (e.g. COM1) to a TCP port (e.g. Telnet)"
#define MDM_TILDE_DELAY			500	/* milliseconds */

/* global vars */
BOOL	daemonize=FALSE;
char	termtype[INI_MAX_VALUE_LEN+1]	= NAME;
char	termspeed[INI_MAX_VALUE_LEN+1]	= "28800,28800";	/* "tx,rx", max length not defined */
char	revision[16];

char	mdm_init[INI_MAX_VALUE_LEN]		= "AT&F";
char	mdm_autoans[INI_MAX_VALUE_LEN]	= "ATS0=1";
char	mdm_cid[INI_MAX_VALUE_LEN]		= "AT+VCID=1";
char	mdm_cleanup[INI_MAX_VALUE_LEN]	= "ATS0=0";
char	mdm_answer[INI_MAX_VALUE_LEN]	= "ATA";
char	mdm_ring[INI_MAX_VALUE_LEN]		= "RING";
BOOL	mdm_null=FALSE;
BOOL	mdm_manswer=FALSE;		/* Manual Answer */

int		mdm_timeout=5;			/* seconds */
int		mdm_reinit=60;			/* minutes */
int		mdm_cmdretry=2;			/* command retries (total attempts-1) */

#ifdef _WIN32
char	com_dev[MAX_PATH+1]				= "COM1";
#else
char	com_dev[MAX_PATH+1]				= "/dev/ttyd0";
#endif
COM_HANDLE	com_handle=COM_HANDLE_INVALID;
BOOL	com_handle_passed=FALSE;
BOOL	com_alreadyconnected=FALSE;
BOOL	com_hangup=TRUE;
ulong	com_baudrate=0;
BOOL	dcd_ignore=FALSE;
int		dcd_timeout=10;	/* seconds */
ulong	dtr_delay=100;	/* milliseconds */
int		hangup_attempts=10;

BOOL	terminated=FALSE;
BOOL	terminate_after_one_call=FALSE;

SOCKET	sock=INVALID_SOCKET;
char	host[MAX_PATH+1]				= "localhost";
ushort	port=IPPORT_TELNET;

/* stats */
ulong	total_calls=0;
ulong	bytes_sent=0;
ulong	bytes_received=0;

/* .ini over-rideable */
int		log_level=LOG_INFO;
BOOL	pause_on_exit=FALSE;
BOOL	tcp_nodelay=TRUE;

/* telnet stuff */
BOOL	telnet=TRUE;
BOOL	debug_telnet=FALSE;
uchar	telnet_local_option[0x100];
uchar	telnet_remote_option[0x100];
BYTE	telnet_cmd[64];
int		telnet_cmdlen;
BOOL	telnet_advertise_cid=FALSE;

/* ident (RFC1413) server stuff */
BOOL	ident=FALSE;
ushort	ident_port=IPPORT_IDENT;
ulong	ident_interface=INADDR_ANY;
char	ident_response[INI_MAX_VALUE_LEN]	= "CALLERID:SEXPOTS";

/* Caller-ID stuff */
char	cid_name[64];
char	cid_number[64];

/****************************************************************************/
/****************************************************************************/
int usage(const char* fname)
{
	printf("usage: %s [ini file] [options]\n"
		"\nOptions:"
		"\n"
		"\n-null                 No 'AT commands' sent to modem"
		"\n-com <device>         Specify communications port device"
		"\n-baud <rate>          Specify baud rate for communications port"
		"\n-live [handle]        Communications port is already open/connected"
		"\n-nohangup             Do not hangup (drop DTR) after call"
		"\n-host <addr | name>   Specify TCP server hostname or IP address"
		"\n-port <number>        Specify TCP port number"
		"\n-debug                Enable debug log output"
#if defined(_WIN32)
		"\n\nNT Service\n"
		"\n-install              install and enable NT service (%s)"
		"\n-service              run as an NT service (background execution)"
		"\n-remove               remove NT service"
		"\n-enable               enable NT service (auto-start during boot)"
		"\n-disable              disable NT service"
#endif
		"\n"
		,getfname(fname)
		,NAME);

	return 0;
}

#if defined(_WIN32)

static WORD event_type(int level)
{
	switch(level) {
		case LOG_WARNING:
			return(EVENTLOG_WARNING_TYPE);
		case LOG_NOTICE:
		case LOG_INFO:
		case LOG_DEBUG:
			return(EVENTLOG_INFORMATION_TYPE);
	}
/*
	LOG_EMERG
	LOG_ALERT
	LOG_CRIT
	LOG_ERR
*/
	return(EVENTLOG_ERROR_TYPE);
}

static int syslog(int level, char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];
	char* p=sbuf;
	int	retval;
	static HANDLE event_handle;

    va_start(argptr,fmt);
    retval=vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);

	if(event_handle == NULL)
		event_handle = RegisterEventSource(
			NULL,		// server name for source (NULL = local computer)
			TITLE);		// source name for registered handle

	if(event_handle != NULL)
		ReportEvent(event_handle,	// event log handle
			event_type(level),		// event type
			0,						// category zero
			0,						// event identifier
			NULL,					// no user security identifier
			1,						// one string
			0,						// no data
			&p,						// pointer to string array
			NULL);					// pointer to data

    return(retval);
}

#endif

/****************************************************************************/
/****************************************************************************/
int lputs(int level, const char* str)
{
	time_t		t;
	struct tm	tm;
	char		tstr[32];
#if defined(_WIN32)
	char dbgmsg[1024];
	_snprintf(dbgmsg,sizeof(dbgmsg),"%s %s", NAME, str);
	if(log_level==LOG_DEBUG)
		OutputDebugString(dbgmsg);
#else
	char dbgmsg[1024];
	snprintf(dbgmsg,sizeof(dbgmsg),"%s %s", NAME, str);
	if(log_level==LOG_DEBUG)
		fputs(dbgmsg, stderr);
#endif

	if(level>log_level)
		return 0;

	if(daemonize) {
#if defined(_WIN32)
		return syslog(level,"%s", str);
#else
		/* syslog() is
		 * void syslog(int priority, const char *message, ...);
		 */

		syslog(level,"%s", str);
		return strlen(str);
#endif
	}

	t=time(NULL);
	if(localtime_r(&t,&tm)==NULL)
		tstr[0]=0;
	else
		sprintf(tstr,"%d/%d %02d:%02d:%02d "
			,tm.tm_mon+1,tm.tm_mday
			,tm.tm_hour,tm.tm_min,tm.tm_sec);

	return fprintf(level>=LOG_NOTICE ? stdout:stderr, "%s %s\n", tstr, str);
}

/****************************************************************************/
/****************************************************************************/
int lprintf(int level, char *fmt, ...)
{
	va_list argptr;
	char sbuf[1024];

    va_start(argptr,fmt);
    vsnprintf(sbuf,sizeof(sbuf),fmt,argptr);
	sbuf[sizeof(sbuf)-1]=0;
    va_end(argptr);
    return(lputs(level,sbuf));
}


#if defined(_WIN32)

#define NTSVC_TIMEOUT_STARTUP	30000	/* Milliseconds */
#define NTSVC_TIMEOUT_SHUTDOWN	30000	/* Milliseconds */

SERVICE_STATUS_HANDLE	svc_status_handle;
SERVICE_STATUS			svc_status;

void WINAPI ServiceControlHandler(DWORD dwCtrlCode)
{
	switch(dwCtrlCode) {
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			lprintf(LOG_NOTICE,"Received termination control signal: %d", dwCtrlCode);
			svc_status.dwWaitHint=NTSVC_TIMEOUT_SHUTDOWN;
			svc_status.dwCurrentState=SERVICE_STOP_PENDING;
			SetServiceStatus(svc_status_handle, &svc_status);
			terminated=TRUE;
//			SetEvent(service_event);
			break;
		case SERVICE_CONTROL_INTERROGATE:
			lprintf(LOG_DEBUG,"Ignoring service control signal: SERVICE_CONTROL_INTERROGATE");
			break;
		default:
			lprintf(LOG_WARNING,"Received unsupported service control signal: %d", dwCtrlCode);
			break;
	}
}

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


/****************************************************************************/
/* Install NT service														*/
/****************************************************************************/
static int install(void)
{
	HANDLE		hSCMlib;
    SC_HANDLE   hSCManager;
    SC_HANDLE   hService;
    char		path[MAX_PATH+1];
	char		cmdline[MAX_PATH+1];

	printf("Installing service: %-40s ... ", TITLE);

	hSCMlib = LoadLibrary("ADVAPI32.DLL");

    if(GetModuleFileName(NULL,path,sizeof(path))==0)
    {
        fprintf(stderr,"\n!ERROR %d getting module file name\n",GetLastError());
        return(-1);
    }

    hSCManager = OpenSCManager(
                        NULL,                   // machine (NULL == local)
                        NULL,                   // database (NULL == default)
                        SC_MANAGER_ALL_ACCESS   // access required
                        );
    if(hSCManager==NULL) {
		fprintf(stderr,"\n!ERROR %d opening SC manager\n",GetLastError());
		return(-1);
	}

	/* Install new service */
	wsprintf(cmdline,"%s service", path);
    hService = CreateService(
        hSCManager,						// SCManager database
        NAME,							// name of service
        TITLE,							// name to display
        SERVICE_ALL_ACCESS,				// desired access
        SERVICE_WIN32_OWN_PROCESS,		// service type
		SERVICE_AUTO_START,				// start type (auto or manual)
        SERVICE_ERROR_NORMAL,			// error control type
        cmdline,						// service's binary
        NULL,							// no load ordering group
        NULL,							// no tag identifier
        "",								// dependencies
        NULL,							// LocalSystem account
        NULL);							// no password

	if(hService==NULL)
		fprintf(stderr,"\n!ERROR %d creating service\n",GetLastError());
	else {
		describe_service(hSCMlib, hService, DESCRIPTION);
		CloseServiceHandle(hService);
		printf("Successful\n");

		register_event_source(TITLE,path);
	}


	if(hSCMlib!=NULL)
		FreeLibrary(hSCMlib);

	CloseServiceHandle(hSCManager);

	return(0);
}

/****************************************************************************/
/* Utility function to remove a service cleanly (stopping if necessary)		*/
/****************************************************************************/
static void remove_service(SC_HANDLE hSCManager, SC_HANDLE hService, char* name, char* disp_name)
{
	SERVICE_STATUS	status;

	printf("Removing service: %-40s ... ", disp_name);

	if(hService==NULL) {

		hService = OpenService(hSCManager, name, SERVICE_ALL_ACCESS);

		if(hService==NULL) {
			printf("\n!ERROR %d opening service: %s\n",GetLastError(),name);
			return;
		}
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

/****************************************************************************/
/* Uninstall one or all services											*/
/****************************************************************************/
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

	remove_service(hSCManager,NULL,NAME,TITLE);

	CloseServiceHandle(hSCManager);

	return(0);
}

/****************************************************************************/
/* Utility function to disable a service									*/
/****************************************************************************/
static void set_service_start_type(SC_HANDLE hSCManager, DWORD start_type)
{
    SC_HANDLE		hService;

	printf("%s service: %-40s ... "
		,start_type==SERVICE_DISABLED ? "Disabling" : "Enabling", TITLE);

    hService = OpenService(hSCManager, NAME, SERVICE_ALL_ACCESS);

	if(hService==NULL) {
		printf("\n!ERROR %d opening service: %s\n",GetLastError(),NAME);
		return;
	}

	if(!ChangeServiceConfig(
		hService,				// handle to service
		SERVICE_NO_CHANGE,		// type of service
		start_type,				// when to start service
		SERVICE_NO_CHANGE,		// severity if service fails to start
		NULL,					// pointer to service binary file name
		NULL,					// pointer to load ordering group name
		NULL,					// pointer to variable to get tag identifier
		NULL,					// pointer to array of dependency names
		NULL,					// pointer to account name of service
		NULL,					// pointer to password for service account
		NULL					// pointer to display name
		))
		printf("\n!ERROR %d changing service config for: %s\n",GetLastError(),NAME);
	else
		printf("Successful\n");

    CloseServiceHandle(hService);
}

/****************************************************************************/
/* Enable (set to auto-start) or disable one or all services				*/
/****************************************************************************/
static int enable(BOOL enabled)
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

	set_service_start_type(hSCManager
		,enabled ? SERVICE_AUTO_START : SERVICE_DISABLED);

	CloseServiceHandle(hSCManager);

	return(0);
}

#endif

#ifdef _WINSOCKAPI_

/* Note: Don't call WSACleanup() or TCP session will close! */
WSADATA WSAData;	

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		lprintf(LOG_INFO,"%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		return(TRUE);
	}

    lprintf(LOG_ERR,"WinSock startup ERROR %d", status);
	return(FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)	

#endif

/****************************************************************************/
/****************************************************************************/
BOOL modem_send(COM_HANDLE com_handle, const char* str)
{
	const char* p;
	char		ch;

	lprintf(LOG_INFO,"Modem Command: %s", str);
	for(p=str; *p; p++) {
		ch=*p;
		if(ch=='~') {
			SLEEP(MDM_TILDE_DELAY);
			continue;
		}
		if(ch=='^' && *(p+1)) {	/* Support ^X for control characters embedded in modem command strings */
			p++;
			ch=*p;
			if(ch!='^' && ch>='@')	/* ^^ to send an '^' char to the modem */
				ch-='@';
		}
		if(!comWriteByte(com_handle,ch))
			return FALSE;
	}
	SLEEP(100);
	comPurgeInput(com_handle);
	return comWriteByte(com_handle, '\r');
}

/****************************************************************************/
/****************************************************************************/
BOOL modem_response(COM_HANDLE com_handle, char *str, size_t maxlen)
{
	BYTE	ch;
	size_t	len=0;
	time_t	start;

	lprintf(LOG_DEBUG,"Waiting for Modem Response ...");
	start=time(NULL);
	while(1){
		if(time(NULL)-start >= mdm_timeout) {
			lprintf(LOG_WARNING,"Modem Response TIMEOUT (%lu seconds) on %s", mdm_timeout, com_dev);
			return FALSE;
		}
		if(len >= maxlen) {
			lprintf(LOG_WARNING,"Modem Response too long (%u >= %u) from %s"
				,len, maxlen, com_dev);
			return FALSE;
		}
		if(!comReadByte(com_handle, &ch)) {
			YIELD();
			continue;
		}
		if(ch<' ' && len==0)	/* ignore prepended control chars */
			continue;

		if(ch=='\r') {
			while(comReadByte(com_handle,&ch))	/* eat trailing ctrl chars (e.g. 'LF') */
#if 0
				lprintf(LOG_DEBUG, "eating ch=%02X", ch)
#endif
				;
			break;
		}
		str[len++]=ch;
	}
	str[len]=0;

	return TRUE;
}

/****************************************************************************/
/****************************************************************************/
BOOL modem_command(COM_HANDLE com_handle, const char* cmd)
{
	char	resp[128];
	int		i;

	for(i=0;i<=mdm_cmdretry;i++) {
		if(terminated)
			return FALSE;
		if(i) {
			lprintf(LOG_WARNING,"Retry #%u: sending modem command (%s) on %s", i, cmd, com_dev);
			lprintf(LOG_DEBUG,"Dropping DTR on %s", com_dev);
			if(!comLowerDTR(com_handle))
				lprintf(LOG_ERR,"ERROR %u lowering DTR on %s", COM_ERROR_VALUE, com_dev);
			SLEEP(dtr_delay);
			lprintf(LOG_DEBUG,"Raising DTR on %s", com_dev);
			if(!comRaiseDTR(com_handle))
				lprintf(LOG_ERR,"ERROR %u raising DTR on %s", COM_ERROR_VALUE, com_dev);
		}
		if(!modem_send(com_handle, cmd)) {
			lprintf(LOG_ERR,"ERROR %u sending modem command (%s) on %s"
				,COM_ERROR_VALUE, cmd, com_dev);
			continue;
		}

		if(modem_response(com_handle, resp, sizeof(resp)))
			break;
	}

	if(i<=mdm_cmdretry) {
		lprintf(LOG_INFO,"Modem Response: %s", resp);
		return TRUE;
	}
	lprintf(LOG_ERR,"Modem command (%s) failure on %s (%u attempts)", cmd, com_dev, i);
	return FALSE;
}

/****************************************************************************/
/****************************************************************************/
void close_socket(SOCKET* sock)
{
	if(*sock != INVALID_SOCKET) {
		shutdown(*sock,SHUT_RDWR);	/* required on Unix */
		closesocket(*sock);
		*sock=INVALID_SOCKET;
	}
}

/****************************************************************************/
/****************************************************************************/
void cleanup(void)
{
	terminated=TRUE;

	lprintf(LOG_INFO,"Cleaning up ...");


	if(com_handle!=COM_HANDLE_INVALID) {
		if(!mdm_null && mdm_cleanup[0])
			modem_command(com_handle, mdm_cleanup);
		if(!com_handle_passed)
			comClose(com_handle);
	}

	close_socket(&sock);

#ifdef _WINSOCKAPI_
	WSACleanup();
#endif

	lprintf(LOG_INFO,"Done (handled %lu calls).", total_calls);

#if defined(_WIN32)
	if(daemonize && svc_status_handle!=0) {
		svc_status.dwCurrentState=SERVICE_STOPPED;
		SetServiceStatus(svc_status_handle, &svc_status);
	} else
#endif
	if(pause_on_exit) {
		printf("Hit enter to continue...");
		getchar();
	}
}

/* Returns 0 on error, Modem status bit-map value otherwise */
int modem_status(COM_HANDLE com_handle)
{
	int	mdm_status;

	if((mdm_status=comGetModemStatus(com_handle)) == COM_ERROR) {
		lprintf(LOG_ERR,"ERROR %u getting modem status"
			,COM_ERROR_VALUE);
		return 0;
	}
	return mdm_status;
}

/* Returns TRUE if DCD (Data Carrier Detect) is high */
BOOL carrier_detect(COM_HANDLE com_handle)
{
	return (modem_status(com_handle)&COM_DCD) ? TRUE:FALSE;
}

/****************************************************************************/
/****************************************************************************/
BOOL wait_for_call(COM_HANDLE com_handle)
{
	char		str[128];
	char*		p;
	BOOL		result=TRUE;
	DWORD		events=0;
	time_t		start=time(NULL);

	ZERO_VAR(cid_name);
	ZERO_VAR(cid_number);

	if(!comRaiseDTR(com_handle))
		lprintf(LOG_ERR,"ERROR %u raising DTR", COM_ERROR_VALUE);

	if(com_alreadyconnected)
		return TRUE;

	if(!mdm_null) {
		if(mdm_init[0]) {
			lprintf(LOG_INFO,"Initializing modem:");
			if(!modem_command(com_handle, mdm_init))
				return FALSE;
		}
		if(!mdm_manswer && mdm_autoans[0]) {
			lprintf(LOG_INFO,"Setting modem to auto-answer:");
			if(!modem_command(com_handle, mdm_autoans))
				return FALSE;
		}
		if(mdm_cid[0]) {
			lprintf(LOG_INFO,"Enabling modem Caller-ID:");
			if(!modem_command(com_handle, mdm_cid))
				return FALSE;
		}
	}

	lprintf(LOG_INFO,"Waiting for incoming call (%s) ...", mdm_manswer ? "Ring Indication" : "Carrier Detect");
	while(1) {
		if(terminated)
			return FALSE;
		if(comReadLine(com_handle, str, sizeof(str), /* timeout (ms): */250) > 0) {
			truncsp(str);
			if(str[0]==0)
				continue;
			lprintf(LOG_DEBUG,"Received from modem: '%s'", str);
			p=str;
			SKIP_WHITESPACE(p);
			if(*p) {
				lprintf(LOG_INFO, "Modem Message: %s", p);
				if(strncmp(p,"CONNECT ",8)==0) {
					long rate=atoi(p+8);
					if(rate)
						SAFEPRINTF2(termspeed,"%u,%u", rate, rate);
				}
				else if(strncmp(p,"NMBR",4)==0 || strncmp(p,"MESG",4)==0) {
					p+=4;
					FIND_CHAR(p,'=');
					SKIP_CHAR(p,'=');
					SKIP_WHITESPACE(p);
					if(cid_number[0]==0)	/* Don't overwrite, if multiple messages received */
						SAFECOPY(cid_number, p);
				}
				else if(strncmp(p,"NAME",4)==0) {
					p+=4;
					FIND_CHAR(p,'=');
					SKIP_CHAR(p,'=');
					SKIP_WHITESPACE(p);
					SAFECOPY(cid_name, p);
				}
				else if(strcmp(p,"NO CARRIER")==0) {
					ZERO_VAR(cid_name);
					ZERO_VAR(cid_number);
				}
				else if(mdm_ring[0] && strcmp(p,mdm_ring)==0 && mdm_manswer && mdm_answer[0]) {
					if(!modem_send(com_handle, mdm_answer)) {
						lprintf(LOG_ERR,"ERROR %u sending modem command (%s) on %s"
							,COM_ERROR_VALUE, mdm_answer, com_dev);
					}
				}
			}
			continue;	/* don't check DCD until we've received all the modem msgs */
		}
		if(carrier_detect(com_handle))
			break;
		if(mdm_reinit && (time(NULL)-start)/60 >= mdm_reinit) {
			lprintf(LOG_INFO,"Re-initialization timer elapsed: %u minutes", mdm_reinit);
			return TRUE;
		}
	}

	if(strcmp(cid_name,"P")==0)
		SAFECOPY(cid_name,"Private");
	else if(strcmp(cid_name,"O")==0)
		SAFECOPY(cid_name,"Out-of-area");

	if(strcmp(cid_number,"P")==0)
		SAFECOPY(cid_number,"Private");
	else if(strcmp(cid_number,"O")==0)
		SAFECOPY(cid_number,"Out-of-area");

	lprintf(LOG_INFO,"Carrier detected");
	return TRUE;
}

/****************************************************************************/
/****************************************************************************/
u_long resolve_ip(const char *addr)
{
	HOSTENT*	host;
	const char*	p;

	if(*addr==0)
		return((u_long)INADDR_NONE);

	for(p=addr;*p;p++)
		if(*p!='.' && !isdigit(*p))
			break;
	if(!(*p))
		return(inet_addr(addr));
	if((host=gethostbyname(addr))==NULL) 
		return((u_long)INADDR_NONE);
	return(*((ulong*)host->h_addr_list[0]));
}

/****************************************************************************/
/****************************************************************************/
SOCKET connect_socket(const char* host, ushort port)
{
	SOCKET		sock;
	SOCKADDR_IN	addr;

	if((sock=socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
		lprintf(LOG_ERR,"ERROR %u creating socket", ERROR_VALUE);
		return INVALID_SOCKET;
	}

	lprintf(LOG_DEBUG,"Setting TCP_NODELAY to %d",tcp_nodelay);
	setsockopt(sock,IPPROTO_TCP,TCP_NODELAY,(char*)&tcp_nodelay,sizeof(tcp_nodelay));

	ZERO_VAR(addr);
	addr.sin_addr.s_addr = resolve_ip(host);
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(port);

	lprintf(LOG_INFO,"Connecting to %s port %u", host, port);
	if(connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
		lprintf(LOG_INFO,"Connected from COM Port (handle %u) to %s TCP port %u using socket descriptor %u"
			,com_handle, host, port, sock);
		return sock;
	}

	lprintf(LOG_ERR,"SOCKET ERROR %u connecting to %s port %u", ERROR_VALUE, host, port);
	closesocket(sock);

	return INVALID_SOCKET;
}

BOOL call_terminated;
BOOL input_thread_terminated;

/****************************************************************************/
/****************************************************************************/
void input_thread(void* arg)
{
	BYTE		ch;

	lprintf(LOG_DEBUG,"Input thread started");
	while(!call_terminated) {
		if(!comReadByte(com_handle, &ch)) {
			YIELD();
			continue;
		}
		if(telnet && ch==TELNET_IAC)
			sendsocket(sock, &ch, sizeof(ch));	/* escape Telnet IAC char (255) when in telnet mode */
		sendsocket(sock, &ch, sizeof(ch));
		bytes_received++;
	}
	lprintf(LOG_DEBUG,"Input thread terminated");
	input_thread_terminated=TRUE;
}

void ident_server_thread(void* arg)
{
	int				result;
	SOCKET			sock;
	SOCKADDR_IN		server_addr={0};
	fd_set			socket_set;
	struct timeval tv;

	lprintf(LOG_DEBUG,"Ident server thread started");

	if((sock=socket(AF_INET, SOCK_STREAM, IPPROTO_IP)) == INVALID_SOCKET) {
		lprintf(LOG_ERR,"ERROR %u creating socket", ERROR_VALUE);
		return;
	}
	
    memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_addr.s_addr = htonl(ident_interface);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(ident_port);

    if(bind(sock,(struct sockaddr *)&server_addr,sizeof(server_addr))!=0) {
		lprintf(LOG_ERR,"ERROR %u binding ident server socket", ERROR_VALUE);
		close_socket(&sock);
		return;
	}

    if(listen(sock, 1)) {
		lprintf(LOG_ERR,"!ERROR %u listening on ident server socket", ERROR_VALUE);
		close_socket(&sock);
		return;
	}

	while(!terminated) {
		/* now wait for connection */

		FD_ZERO(&socket_set);
		FD_SET(sock,&socket_set);

		tv.tv_sec=5;
		tv.tv_usec=0;

		if((result=select(sock+1,&socket_set,NULL,NULL,&tv))<1) {
			if(result==0)
				continue;
			if(ERROR_VALUE==EINTR)
				lprintf(LOG_DEBUG,"Ident Server listening interrupted");
			else if(ERROR_VALUE == ENOTSOCK)
            	lprintf(LOG_NOTICE,"Ident Server sockets closed");
			else
				lprintf(LOG_WARNING,"!ERROR %d selecting ident socket",ERROR_VALUE);
			continue;
		}

		if(FD_ISSET(sock,&socket_set)) {
			SOCKADDR_IN		client_addr;
			socklen_t		client_addr_len;
			SOCKET			client_socket=INVALID_SOCKET;
			char			request[128];
			char			response[256];
			int				rd;

			client_addr_len = sizeof(client_addr);
			client_socket = accept(sock, (struct sockaddr *)&client_addr, &client_addr_len);
			if(client_socket != INVALID_SOCKET) {
				lprintf(LOG_INFO,"Ident request from %s : %u"
					,inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port));
				FD_ZERO(&socket_set);
				FD_SET(client_socket,&socket_set);
				tv.tv_sec=5;
				tv.tv_usec=0;
				if(select(client_socket+1,&socket_set,NULL,NULL,&tv)==1) {
					lprintf(LOG_DEBUG,"Ident select");
					if((rd=recv(client_socket, request, sizeof(request), 0)) > 0) {
						request[rd]=0;
						truncsp(request);
						lprintf(LOG_INFO,"Ident request: %s", request);
						/* example response: "40931,23:USERID:UNIX:cyan" */
						SAFEPRINTF4(response,"%s:%s:%s %s\r\n"
							,request, ident_response, cid_number, cid_name);
						sendsocket(client_socket,response,strlen(response));
					} else
						lprintf(LOG_DEBUG,"ident recv=%d %d", rd, ERROR_VALUE);
				}
				close_socket(&client_socket);
			}
		}
	}

	close_socket(&sock);
	lprintf(LOG_DEBUG,"Ident server thread terminated");
}

static void send_telnet_cmd(uchar cmd, uchar opt)
{
	char buf[16];
	
	if(cmd<TELNET_WILL) {
		if(debug_telnet)
			lprintf(LOG_INFO,"TX Telnet command: %s"
				,telnet_cmd_desc(cmd));
		sprintf(buf,"%c%c",TELNET_IAC,cmd);
		sendsocket(sock,buf,2);
	} else {
		if(debug_telnet)
			lprintf(LOG_INFO,"TX Telnet command: %s %s"
				,telnet_cmd_desc(cmd), telnet_opt_desc(opt));
		sprintf(buf,"%c%c%c",TELNET_IAC,cmd,opt);
		sendsocket(sock,buf,3);
	}
}

void request_telnet_opt(uchar cmd, uchar opt)
{
	if(cmd==TELNET_DO || cmd==TELNET_DONT) {	/* remote option */
		if(telnet_remote_option[opt]==telnet_opt_ack(cmd))
			return;	/* already set in this mode, do nothing */
		telnet_remote_option[opt]=telnet_opt_ack(cmd);
	} else {	/* local option */
		if(telnet_local_option[opt]==telnet_opt_ack(cmd))
			return;	/* already set in this mode, do nothing */
		telnet_local_option[opt]=telnet_opt_ack(cmd);
	}
	send_telnet_cmd(cmd,opt);
}

/****************************************************************************/
/****************************************************************************/
BYTE* telnet_interpret(BYTE* inbuf, int inlen, BYTE* outbuf, int *outlen)
{
	BYTE	command;
	BYTE	option;
	BYTE*   first_iac;
	int 	i;

	if(inlen<1) {
		*outlen=0;
		return(inbuf);	/* no length? No interpretation */
	}

    first_iac=(BYTE*)memchr(inbuf, TELNET_IAC, inlen);

    if(!telnet_cmdlen && first_iac==NULL) {
        *outlen=inlen;
        return(inbuf);	/* no interpretation needed */
    }

    if(first_iac!=NULL) {
   		*outlen=first_iac-inbuf;
	    memcpy(outbuf, inbuf, *outlen);
    } else
    	*outlen=0;

    for(i=*outlen;i<inlen;i++) {
        if(inbuf[i]==TELNET_IAC && telnet_cmdlen==1) { /* escaped 255 */
            telnet_cmdlen=0;
            outbuf[(*outlen)++]=TELNET_IAC;
            continue;
        }
        if(inbuf[i]==TELNET_IAC || telnet_cmdlen) {

			if(telnet_cmdlen<sizeof(telnet_cmd))
				telnet_cmd[telnet_cmdlen++]=inbuf[i];

			command	= telnet_cmd[1];
			option	= telnet_cmd[2];

			if(telnet_cmdlen>=2 && command==TELNET_SB) {
				if(inbuf[i]==TELNET_SE 
					&& telnet_cmd[telnet_cmdlen-2]==TELNET_IAC) {
					if(debug_telnet)
						lprintf(LOG_INFO,"RX Telnet sub-negotiation command: %s"
							,telnet_opt_desc(option));
					/* sub-option terminated */
					if(option==TELNET_TERM_TYPE && telnet_cmd[3]==TELNET_TERM_SEND) {
						BYTE buf[32];
						int len=sprintf(buf,"%c%c%c%c%s%c%c"
							,TELNET_IAC,TELNET_SB
							,TELNET_TERM_TYPE,TELNET_TERM_IS
							,termtype
							,TELNET_IAC,TELNET_SE);
						if(debug_telnet)
							lprintf(LOG_INFO,"TX Telnet command: Terminal Type is %s", termtype);
						sendsocket(sock,buf,len);
/*						request_telnet_opt(TELNET_WILL, TELNET_TERM_SPEED); */
					} else if(option==TELNET_TERM_SPEED && telnet_cmd[3]==TELNET_TERM_SEND) {
						BYTE buf[32];
						int len=sprintf(buf,"%c%c%c%c%s%c%c"
							,TELNET_IAC,TELNET_SB
							,TELNET_TERM_SPEED,TELNET_TERM_IS
							,termspeed
							,TELNET_IAC,TELNET_SE);
						if(debug_telnet)
							lprintf(LOG_INFO,"TX Telnet command: Terminal Speed is %s", termspeed);
						sendsocket(sock,buf,len);
					}

					telnet_cmdlen=0;
				}
			}
            else if(telnet_cmdlen==2 && inbuf[i]<TELNET_WILL) {
                telnet_cmdlen=0;
            }
            else if(telnet_cmdlen>=3) {	/* telnet option negotiation */

				if(debug_telnet)
					lprintf(LOG_INFO,"RX Telnet command: %s %s"
						,telnet_cmd_desc(command),telnet_opt_desc(option));

				if(command==TELNET_DO || command==TELNET_DONT) {	/* local options */
					if(telnet_local_option[option]!=command) {
						switch(option) {
							case TELNET_BINARY_TX:
							case TELNET_ECHO:
							case TELNET_TERM_TYPE:
							case TELNET_TERM_SPEED:
							case TELNET_SUP_GA:
								telnet_local_option[option]=command;
								send_telnet_cmd(telnet_opt_ack(command),option);
								break;
							case TELNET_SEND_LOCATION:
								if(command==TELNET_DO) {
									BYTE buf[128];
									int len=safe_snprintf(buf,sizeof(buf),"%c%c%c%s %s%c%c"
										,TELNET_IAC,TELNET_SB
										,TELNET_SEND_LOCATION
										,cid_number, cid_name
										,TELNET_IAC,TELNET_SE);
									if(debug_telnet)
										lprintf(LOG_INFO,"TX Telnet command: Location is %s %s", cid_number, cid_name);
									sendsocket(sock,buf,len);
								} else
									send_telnet_cmd(telnet_opt_ack(command),option);
								break;
		
							default: /* unsupported local options */
								if(command==TELNET_DO) /* NAK */
									send_telnet_cmd(telnet_opt_nak(command),option);
								break;
						}
					}
				} else { /* WILL/WONT (remote options) */ 
					if(telnet_remote_option[option]!=command) {	

						switch(option) {
							case TELNET_BINARY_TX:
							case TELNET_ECHO:
							case TELNET_TERM_TYPE:
							case TELNET_SUP_GA:
								telnet_remote_option[option]=command;
								send_telnet_cmd(telnet_opt_ack(command),option);
								break;
							default: /* unsupported remote options */
								if(command==TELNET_WILL) /* NAK */
									send_telnet_cmd(telnet_opt_nak(command),option);
								break;
						}
					}
				}

                telnet_cmdlen=0;

            }
        } else
        	outbuf[(*outlen)++]=inbuf[i];
    }
    return(outbuf);
}

/****************************************************************************/
/****************************************************************************/
BOOL handle_call(void)
{
	BYTE		buf[4096];
	BYTE		telnet_buf[sizeof(buf)];
	BYTE*		p;
	int			result;
	int			rd;
	int			wr;
	fd_set		socket_set;
	struct		timeval tv = {0, 0};

	bytes_sent=0;
	bytes_received=0;
	call_terminated=FALSE;

	/* Reset Telnet state information */
	telnet_cmdlen=0;
	ZERO_VAR(telnet_local_option);
	ZERO_VAR(telnet_remote_option);

	if(telnet && telnet_advertise_cid && (cid_number[0] || cid_name[0]))	/* advertise the ability to send our location */
		send_telnet_cmd(TELNET_WILL, TELNET_SEND_LOCATION);

	input_thread_terminated=FALSE;
	_beginthread(input_thread, 0, NULL);

	while(!terminated) {

		if(!dcd_ignore && !carrier_detect(com_handle)) {
			lprintf(LOG_WARNING,"Loss of Carrier Detect (DCD) detected");
			break;
		}
#if 0	/* single-threaded mode: */
		if(comReadByte(com_handle, &ch)) {
			lprintf(LOG_DEBUG,"read byte: %c", ch);
			send(sock, &ch, sizeof(ch), 0);
		}
#endif

		FD_ZERO(&socket_set);
		FD_SET(sock,&socket_set);
		if((result = select(sock+1,&socket_set,NULL,NULL,&tv)) == 0) {
			YIELD();
			continue;
		}
		if(result == SOCKET_ERROR) {
			lprintf(LOG_ERR,"SOCKET ERROR %u on select", ERROR_VALUE);
			break;
		}

		rd=recv(sock,buf,sizeof(buf),0);

		if(rd < 1) {
			if(rd==0) {
				lprintf(LOG_WARNING,"Socket Disconnected");
				break;
			}
			if(ERROR_VALUE == EAGAIN)
				continue;
        	else if(ERROR_VALUE == ENOTSOCK)
   	            lprintf(LOG_WARNING,"Socket closed by peer on receive");
       	    else if(ERROR_VALUE==ECONNRESET) 
				lprintf(LOG_WARNING,"Connection reset by peer on receive");
			else if(ERROR_VALUE==ESHUTDOWN)
				lprintf(LOG_WARNING,"Socket shutdown on receive");
       	    else if(ERROR_VALUE==ECONNABORTED) 
				lprintf(LOG_WARNING,"Connection aborted by peer on receive");
			else
				lprintf(LOG_ERR,"SOCKET RECV ERROR %d",ERROR_VALUE);
			break;
		}

		if(telnet)
			p=telnet_interpret(buf,rd,telnet_buf,&rd);
		else
			p=buf;

		if((wr=comWriteBuf(com_handle, p, rd)) != COM_ERROR)
			bytes_sent += wr;
	}

	call_terminated=TRUE;	/* terminate input_thread() */
	while(!input_thread_terminated) {
		YIELD();
	}

	lprintf(LOG_INFO,"Bytes sent-to, received-from COM Port: %lu, %lu"
		,bytes_sent, bytes_received);

	return TRUE;
}

/****************************************************************************/
/****************************************************************************/
BOOL hangup_call(COM_HANDLE com_handle)
{
	time_t	start;
	int		attempt;
	int		mdm_status;

	if(!carrier_detect(com_handle))/* DCD already low */
		return TRUE;

	lprintf(LOG_DEBUG,"Waiting for transmit buffer to empty");
	SLEEP(dtr_delay);
	for(attempt=0; attempt<hangup_attempts; attempt++) {
		lprintf(LOG_INFO,"Dropping DTR (attempt #%d)", attempt+1);
		if(!comLowerDTR(com_handle)) {
			lprintf(LOG_ERR,"ERROR %u lowering DTR", COM_ERROR);
			continue;
		}
		lprintf(LOG_DEBUG,"Waiting for loss of Carrier Detect (DCD)");
		start=time(NULL);
		while(time(NULL)-start <= dcd_timeout) {
			if(((mdm_status=modem_status(com_handle))&COM_DCD)==0)
				return TRUE;
			SLEEP(1000); 
		}
		lprintf(LOG_ERR,"TIMEOUT waiting for DCD to drop (attempt #%d of %d)"
			,attempt+1, hangup_attempts);
		lprintf(LOG_DEBUG,"Modem status: 0x%lX", mdm_status);
	}

	return FALSE;
}

/****************************************************************************/
/****************************************************************************/
void break_handler(int type)
{
	lprintf(LOG_NOTICE,"-> Terminated Locally (signal: %d)",type);
	terminated=TRUE;
}

#if defined(_WIN32)
BOOL WINAPI ControlHandler(DWORD CtrlType)
{
	break_handler((int)CtrlType);
	return TRUE;
}
#endif

/****************************************************************************/
/****************************************************************************/
char* iniGetExistingWord(str_list_t list, const char* section, const char* key
					,const char* deflt, char* value)
{
	char* p;

	if((p=iniGetExistingString(list, section, key, deflt, value)) !=NULL) {
		FIND_WHITESPACE(value);
		*value=0;
	}
	return p;
}

void parse_ini_file(const char* ini_fname)
{
	FILE* fp;
	char* section;
	str_list_t	list=NULL;

	if((fp=fopen(ini_fname,"r"))!=NULL) {
		lprintf(LOG_INFO,"Reading %s",ini_fname);
		list=iniReadFile(fp);
		fclose(fp);
	}

	/* Root section */
	pause_on_exit			= iniGetBool(list,ROOT_SECTION,"PauseOnExit",FALSE);
	log_level				= iniGetLogLevel(list,ROOT_SECTION,"LogLevel",log_level);

	if(iniGetBool(list,ROOT_SECTION,"Debug",FALSE))
		log_level=LOG_DEBUG;
	
	/* [COM] Section */
	section="COM";
	iniGetExistingWord(list, section, "Device", NULL, com_dev);
	com_baudrate    = iniGetLongInt(list, section, "BaudRate", com_baudrate);
	com_hangup	    = iniGetBool(list, section, "Hangup", com_hangup);
	hangup_attempts = iniGetInteger(list, section, "HangupAttempts", hangup_attempts);
	dcd_timeout     = iniGetInteger(list, section, "DCDTimeout", dcd_timeout);
	dcd_ignore      = iniGetBool(list, section, "IgnoreDCD", dcd_ignore);
	dtr_delay		= iniGetLongInt(list, section, "DTRDelay", dtr_delay);
	mdm_null	    = iniGetBool(list, section, "NullModem", mdm_null);

	/* [Modem] Section */
	section="Modem";
	iniGetExistingWord(list, section, "Init", "", mdm_init);
	iniGetExistingWord(list, section, "AutoAnswer", "", mdm_autoans);
	iniGetExistingWord(list, section, "Cleanup", "", mdm_cleanup);
	iniGetExistingWord(list, section, "EnableCallerID", "", mdm_cid);
	iniGetExistingWord(list, section, "Answer", "", mdm_answer);
	iniGetExistingWord(list, section, "Ring", "", mdm_ring);
	mdm_timeout     = iniGetInteger(list, section, "Timeout", mdm_timeout);
	mdm_reinit		= iniGetInteger(list, section, "ReInit", mdm_reinit);
	mdm_cmdretry	= iniGetInteger(list, section, "CmdRetry", mdm_cmdretry);
	mdm_manswer		= iniGetBool(list,section,"ManualAnswer", mdm_manswer);

	/* [TCP] Section */
	section="TCP";
	iniGetExistingWord(list, section, "Host", NULL, host);
	port					= iniGetShortInt(list, section, "Port", port);
	tcp_nodelay				= iniGetBool(list,section,"NODELAY", tcp_nodelay);

	/* [Telnet] Section */
	section="Telnet";
	telnet					= iniGetBool(list,section,"Enabled", telnet);
	debug_telnet			= iniGetBool(list,section,"Debug", debug_telnet);
	telnet_advertise_cid	= iniGetBool(list,section,"AdvertiseLocation", telnet_advertise_cid);
	iniGetExistingWord(list, section, "TermType", NULL, termtype);
	iniGetExistingWord(list, section, "TermSpeed", NULL, termspeed);

	/* [Ident] Section */
	section="Ident";
	ident					= iniGetBool(list,section,"Enabled", ident);
	ident_port				= iniGetShortInt(list, section, "Port", ident_port);
	ident_interface			= iniGetIpAddress(list, section, "Interface", ident_interface);
	iniGetExistingWord(list, section, "Response", NULL, ident_response);

}

char	banner[128];

static void 
#if defined(_WIN32)
	WINAPI
#endif
service_loop(int argc, char** argv)
{
	int		argn;
	char*	arg;
	char	str[128];
	char	compiler[128];

	for(argn=1; argn<(int)argc; argn++) {
		arg=argv[argn];
		if(*arg!='-') {	/* .ini file specified */
			if(!fexist(arg)) {
				lprintf(LOG_ERR,"Initialization file does not exist: %s", arg);
				exit(usage(argv[0]));
			}
			parse_ini_file(arg);
			continue;
		}
		while(*arg=='-') 
			arg++;
		if(stricmp(arg,"null")==0)
			mdm_null=TRUE;
		else if(stricmp(arg,"com")==0 && argc > argn+1)
			SAFECOPY(com_dev, argv[++argn]);
		else if(stricmp(arg,"baud")==0 && argc > argn+1)
			com_baudrate = (ulong)strtol(argv[++argn],NULL,0);
		else if(stricmp(arg,"host")==0 && argc > argn+1)
			SAFECOPY(host, argv[++argn]);
		else if(stricmp(arg,"port")==0 && argc > argn+1)
			port = (ushort)strtol(argv[++argn], NULL, 0);
		else if(stricmp(arg,"live")==0) {
			if(argc > argn+1 &&
				(com_handle = (COM_HANDLE)strtol(argv[argn+1], NULL, 0)) != 0) {
				argn++;
				com_handle_passed=TRUE;
			}
			com_alreadyconnected=TRUE;
			terminate_after_one_call=TRUE;
			mdm_null=TRUE;
		}
		else if(stricmp(arg,"nohangup")==0) {
			com_hangup=FALSE;
		}
		else if(stricmp(arg,"debug")==0) {
			log_level=LOG_DEBUG;
		}
		else if(stricmp(arg,"help")==0 || *arg=='?')
			exit(usage(argv[0]));
		else {
			fprintf(stderr,"Invalid option: %s\n", arg);
			exit(usage(argv[0]));
		}
	}

#if defined(_WIN32)
	/* Convert "1" to "COM1" for Windows */
	{
		int i;

		if((i=atoi(com_dev)) != 0)
			SAFEPRINTF(com_dev, "COM%d", i);
	}

	if(daemonize) {

		if((svc_status_handle = RegisterServiceCtrlHandler(NAME, ServiceControlHandler))==0) {
			lprintf(LOG_ERR,"!ERROR %d registering service control handler",GetLastError());
			return;
		}

		svc_status.dwServiceType=SERVICE_WIN32_OWN_PROCESS;
		svc_status.dwControlsAccepted=SERVICE_ACCEPT_SHUTDOWN;
		svc_status.dwWaitHint=NTSVC_TIMEOUT_STARTUP;

		svc_status.dwCurrentState=SERVICE_START_PENDING;
		SetServiceStatus(svc_status_handle, &svc_status);
	}

#endif

	lprintf(LOG_INFO,"%s", comVersion(str,sizeof(str)));
	DESCRIBE_COMPILER(compiler);
	lprintf(LOG_INFO,"Build %s %s %s", __DATE__, __TIME__, compiler);

	/************************************/
	/* Inititalize WinSock and COM Port */
	/************************************/

	if(!winsock_startup())
		exit(1);

	/* Install clean-up callback */
	atexit(cleanup);

	lprintf(LOG_INFO,"TCP Host: %s", host);
	lprintf(LOG_INFO,"TCP Port: %u", port);
	
	if(!com_handle_passed) {
		lprintf(LOG_INFO,"Opening Communications Device (COM Port): %s", com_dev);
		if((com_handle=comOpen(com_dev)) == COM_HANDLE_INVALID) {
			lprintf(LOG_ERR,"ERROR %u opening communications device/port: '%s'", COM_ERROR_VALUE, com_dev);
			exit(1);
		}
	}
	lprintf(LOG_INFO,"COM Port device handle: %u", com_handle);

	if(com_baudrate!=0) {
		if(!comSetBaudRate(com_handle,com_baudrate))
			lprintf(LOG_ERR,"ERROR %u setting DTE rate to %lu bps"
				,COM_ERROR_VALUE, com_baudrate);
	}

	lprintf(LOG_INFO,"COM Port DTE rate: %ld bps", comGetBaudRate(com_handle));

	if(ident)
		_beginthread(ident_server_thread, 0, NULL);

#if defined(_WIN32)
	if(daemonize) {
		svc_status.dwCurrentState=SERVICE_RUNNING;
		svc_status.dwControlsAccepted|=SERVICE_ACCEPT_STOP;
		SetServiceStatus(svc_status_handle, &svc_status);
	}
#endif

	/***************************/
	/* Initialization Complete */
	/***************************/

	/* Main service loop: */
	while(!terminated && wait_for_call(com_handle)) {
		if(!carrier_detect(com_handle))	/* re-initialization timer time-out? */
			continue;
		comWriteByte(com_handle,'\r');
		comWriteString(com_handle, banner);
		comWriteString(com_handle, "\r\n");
		if((sock=connect_socket(host, port)) == INVALID_SOCKET) {
			comWriteString(com_handle,"\7\r\n!ERROR connecting to TCP port\r\n");
		} else {
			handle_call();
			close_socket(&sock);
			total_calls++;
			lprintf(LOG_INFO,"Call completed (%lu total)", total_calls);
		}
		if(com_hangup && !hangup_call(com_handle))
			break;
		if(terminate_after_one_call)
			break;
	}

	exit(0);
}

/****************************************************************************/
/****************************************************************************/
int main(int argc, char** argv)
{
	int		argn;
	char*	arg;
	char*	p;
	char	path[MAX_PATH+1];
	char	fname[MAX_PATH+1];
	char	ini_fname[MAX_PATH+1];

	/*******************************/
	/* Generate and display banner */
	/*******************************/
	sscanf("$Revision: 1.32 $", "%*s %s", revision);

	sprintf(banner,"\n%s v%s-%s"
		" Copyright %s Rob Swindell"
		,TITLE
		,revision
		,PLATFORM_DESC
		,__DATE__+7
		);

	fprintf(stdout,"%s\n\n", banner);

	/**********************/
	/* Parse command-line */
	/**********************/

	for(argn=1; argn<argc; argn++) {
		arg=argv[argn];
		while(*arg=='-') 
			arg++;
		if(stricmp(arg,"help")==0 || *arg=='?')
			return usage(argv[0]);
#ifdef _WIN32
		else if(stricmp(arg,"service")==0)
			daemonize=TRUE;
		else if(stricmp(arg,"install")==0)
			return install();
		else if(stricmp(arg,"remove")==0)
			return uninstall();
		else if(stricmp(arg,"disable")==0)
			return enable(FALSE);
		else if(stricmp(arg,"enable")==0)
			return enable(TRUE);
#endif
	}

	/******************/
	/* Read .ini file */
	/******************/
	/* Generate path/sexpots[.host].ini from path/sexpots[.exe] */
	SAFECOPY(path,argv[0]);
	p=getfname(path);
	SAFECOPY(fname,p);
	*p=0;
	if((p=getfext(fname))!=NULL) 
		*p=0;
	strcat(fname,".ini");

	iniFileName(ini_fname,sizeof(ini_fname),path,fname);
	parse_ini_file(ini_fname);

#if defined(_WIN32)
	if(daemonize) {

		SERVICE_TABLE_ENTRY  ServiceDispatchTable[] = 
		{ 
			{ NAME,	(void(WINAPI*)(DWORD, char**))service_loop	}, 
			{ NULL,			NULL										}	/* Terminator */
		};

		printf("Starting service control dispatcher.\n" );
		printf("This may take several seconds.  Please wait.\n" );

		if(!StartServiceCtrlDispatcher(ServiceDispatchTable)) {
			lprintf(LOG_ERR,"StartServiceCtrlDispatcher ERROR %d",GetLastError());
			return -1;
		}
		return 0;
	}
	SetConsoleCtrlHandler(ControlHandler, TRUE /* Add */);

#endif

	service_loop(argc,argv);

	return 0;
}
