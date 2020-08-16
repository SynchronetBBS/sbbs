/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: MainFormUnit.cpp,v 1.213 2020/04/17 20:38:56 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html		    *
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

//---------------------------------------------------------------------------
#include "sbbs.h"           // unixtodstr()
#include <vcl.h>
#include <vcl/Registry.hpp>	/* TRegistry */
#pragma hdrstop
#include <winsock.h>		// IPPORT_TELNET, INADDR_ANY
#include <process.h> 		// _beginthread()
#include <io.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/locking.h>
#include <fcntl.h>
#include <share.h>

#include "MainFormUnit.h"
#include "TelnetFormUnit.h"
#include "EventsFormUnit.h"
#include "ServicesFormUnit.h"
#include "FtpFormUnit.h"
#include "WebFormUnit.h"
#include "MailFormUnit.h"
#include "NodeFormUnit.h"

#include "StatsFormUnit.h"
#include "ClientFormUnit.h"
#include "CtrlPathDialogUnit.h"
#include "TelnetCfgDlgUnit.h"
#include "MailCfgDlgUnit.h"
#include "FtpCfgDlgUnit.h"
#include "WebCfgDlgUnit.h"
#include "ServicesCfgDlgUnit.h"
#include "AboutBoxFormUnit.h"
#include "CodeInputFormUnit.h"
#include "TextFileEditUnit.h"
#include "UserListFormUnit.h"
#include "PropertiesDlgUnit.h"
#include "ConfigWizardUnit.h"
#include "PreviewFormUnit.h"
#include "LoginAttemptsFormUnit.h"

#include "sbbs_ini.h"		// sbbs_read_ini()
#include "userdat.h"		// lastuser()
#include "ntsvcs.h"			// NTSVC_NAME_*

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "trayicon"
#pragma link "Trayicon"
#pragma resource "*.dfm"
TMainForm *MainForm;

#define LOG_TIME_FMT "  m/d  hh:mm:ssa/p"
#define STATUSBAR_LAST_PANEL  6

/* Service functions are NT-only, must call dynamically :-( */
typedef WINADVAPI SC_HANDLE (WINAPI *OpenSCManager_t)(LPCTSTR,LPCTSTR,DWORD);
typedef WINADVAPI SC_HANDLE (WINAPI *OpenService_t)(SC_HANDLE,LPCTSTR,DWORD);
typedef WINADVAPI BOOL (WINAPI *StartService_t)(SC_HANDLE,DWORD,LPCTSTR*);
typedef WINADVAPI BOOL (WINAPI *ControlService_t)(SC_HANDLE,DWORD,LPSERVICE_STATUS);
typedef WINADVAPI BOOL (WINAPI *QueryServiceStatus_t)(SC_HANDLE,LPSERVICE_STATUS);
typedef WINADVAPI BOOL (WINAPI *QueryServiceConfig_t)(SC_HANDLE,LPQUERY_SERVICE_CONFIG,DWORD,LPDWORD);
typedef WINADVAPI BOOL (WINAPI *CloseServiceHandle_t)(SC_HANDLE);

OpenSCManager_t 		openSCManager;
OpenService_t 			openService;
StartService_t  		startService;
ControlService_t    	controlService;
QueryServiceStatus_t    queryServiceStatus;
QueryServiceConfig_t    queryServiceConfig;
CloseServiceHandle_t	closeServiceHandle;

SC_HANDLE       hSCManager;
SC_HANDLE	    bbs_svc;
SC_HANDLE	    ftp_svc;
SC_HANDLE	    web_svc;
SC_HANDLE	    mail_svc;
SC_HANDLE	    services_svc;
SERVICE_STATUS	bbs_svc_status;
SERVICE_STATUS	ftp_svc_status;
SERVICE_STATUS	web_svc_status;
SERVICE_STATUS	mail_svc_status;
SERVICE_STATUS	services_svc_status;
QUERY_SERVICE_CONFIG*	bbs_svc_config;
DWORD					bbs_svc_config_size;
QUERY_SERVICE_CONFIG*	ftp_svc_config;
DWORD					ftp_svc_config_size;
QUERY_SERVICE_CONFIG*	web_svc_config;
DWORD					web_svc_config_size;
QUERY_SERVICE_CONFIG*	mail_svc_config;
DWORD					mail_svc_config_size;
QUERY_SERVICE_CONFIG*	services_svc_config;
DWORD					services_svc_config_size;

const char* LogLevelDesc[] = {   "Emergency"
                                ,"Alert"
                                ,"Critical"
                                ,"Error"
                                ,"Warning"
                                ,"Notice"
                                ,"Normal"
                                ,"Debug"
                            };
const TColor LogLevelColor[] = {
                                 clRed
                                ,clRed
                                ,clRed
                                ,clRed
                                ,clFuchsia	
                                ,clBlue
                                ,clBlack    /* not used */
                                ,clGreen
                                };

link_list_t bbs_log_list;
link_list_t event_log_list;
link_list_t mail_log_list;
link_list_t ftp_log_list;
link_list_t web_log_list;
link_list_t services_log_list;
link_list_t	login_attempt_list;

bool clearLoginAttemptList = false;

DWORD	MaxLogLen=20000;
int     threads=1;
time_t  initialized=0;
static	str_list_t recycle_semfiles;
static  str_list_t shutdown_semfiles;
bool    terminating=false;
ulong   errors;
AnsiString ErrorSoundFile;

static void errormsg(void* p, int level, const char* msg)
{
    errors++;

    if(MainForm->SoundToggle->Checked)
        PlaySound(ErrorSoundFile.c_str(), NULL, SND_ASYNC|SND_FILENAME);
}

static void thread_up(void* p, BOOL up, BOOL setuid)
{
	char str[128];
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);
    if(up)
	    threads++;
    else if(threads>0)
    	threads--;
    ReleaseMutex(mutex);
}

int sockets=0;

void socket_open(void* p, BOOL open)
{
	char 	str[128];
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);
    if(open)
	    sockets++;
    else if(sockets>0)
    	sockets--;
    ReleaseMutex(mutex);
}

int clients=0;
int total_clients=0;

static void client_add(void* p, BOOL add)
{
	char 	str[128];

    if(add) {
	    clients++;
        total_clients++;
    } else if(clients>0)
    	clients--;
}

static void client_on(void* p, BOOL on, int sock, client_t* client, BOOL update)
{
    char    str[128];
    int     i,j;
    time_t  t;
	static  HANDLE mutex;
    TListItem*  Item;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);
    WaitForSingleObject(ClientForm->ListMutex,INFINITE);

    /* Search for exising entry for this socket */
    for(i=0;i<ClientForm->ListView->Items->Count;i++) {
        if(ClientForm->ListView->Items->Item[i]->Caption.ToIntDef(0)==sock)
            break;
    }
    if(i>=ClientForm->ListView->Items->Count) {
		if(update)	{ /* Can't update a non-existing entry */
			ReleaseMutex(mutex);
			ReleaseMutex(ClientForm->ListMutex);
			return;
		}
        i=-1;
	}

    if(on) {
	    if(!update)
	        client_add(NULL, TRUE);
    } else { // Off
        client_add(NULL, FALSE);
        if(i>=0)
            ClientForm->ListView->Items->Delete(i);
        ReleaseMutex(mutex);
        ReleaseMutex(ClientForm->ListMutex);
        return;
    }
    if(client!=NULL && client->size==sizeof(client_t)) {
        t=time(NULL);
        if(i>=0) {
            Item=ClientForm->ListView->Items->Item[i];
        } else {
            Item=ClientForm->ListView->Items->Add();
            Item->Data=(void*)t;
            Item->Caption=sock;
        }
        Item->SubItems->Clear();
        Item->SubItems->Add(client->protocol);
        Item->SubItems->Add(client->user);
        Item->SubItems->Add(client->addr);
        Item->SubItems->Add(client->host);
        Item->SubItems->Add(client->port);
        t-=(time_t)Item->Data;
        sprintf(str,"%d:%02d",t/60,t%60);
        Item->SubItems->Add(str);
    }
    ReleaseMutex(mutex);
    ReleaseMutex(ClientForm->ListMutex);
}

static int lputs(void* p, int level, const char *str)
{
    log_msg_t   msg;
	link_list_t* list = (link_list_t*)p;
	log_msg_t*	last;

	msg.repeated = 0;
    msg.level = level;
    GetLocalTime(&msg.time);
    SAFECOPY(msg.buf, str);
	listLock(list);
	BOOL unique = TRUE;
	if(list->last != NULL) {
		last = (log_msg_t*)list->last->data;
		if(strcmp(last->buf, msg.buf) == 0) {
			last->repeated++;
			unique = FALSE;
		}
	}
	if(unique)
		listPushNodeData(list, &msg, sizeof(msg));
	listUnlock(list);
    return strlen(msg.buf);
}

static void log_msg(TRichEdit* Log, log_msg_t* msg)
{
    while(MaxLogLen && Log->Lines->Count >= MaxLogLen)
        Log->Lines->Delete(0);

    AnsiString Line=SystemTimeToDateTime(msg->time).FormatString(LOG_TIME_FMT)+"  ";
    Line+=AnsiString(msg->buf).Trim();
	if(msg->repeated)
		Line += " [x" + AnsiString(msg->repeated + 1) + "]";
    Log->SelLength=0;
	Log->SelStart=-1;
    Log->SelAttributes->Assign(
        MainForm->LogAttributes(msg->level, Log->Color, Log->Font));
	Log->Lines->Add(Line);
    SendMessage(Log->Handle, WM_VSCROLL, SB_BOTTOM, NULL);
}

static void bbs_log_msg(log_msg_t* msg)
{
	log_msg(TelnetForm->Log, msg);
}

static void bbs_status(void* p, const char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

	TelnetForm->Status->Caption=AnsiString(str);

    ReleaseMutex(mutex);
}

static void bbs_clients(void* p, int clients)
{
	static HANDLE mutex;
    static save_clients;

    save_clients=clients;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    TelnetForm->ProgressBar->Max
    	=(MainForm->bbs_startup.last_node
	    -MainForm->bbs_startup.first_node)+1;
	TelnetForm->ProgressBar->Position=clients;

    ReleaseMutex(mutex);
}

static void bbs_terminated(void* p, int code)
{
	Screen->Cursor=crDefault;
	MainForm->TelnetStart->Enabled=true;
	MainForm->TelnetStop->Enabled=false;
	MainForm->TelnetRecycle->Enabled=false;
    MainForm->TelnetPause->Enabled=false;
    MainForm->TelnetPause->Checked=false;    
    Application->ProcessMessages();
}
static void bbs_started(void* p)
{
	Screen->Cursor=crDefault;
	MainForm->TelnetStart->Enabled=false;
    MainForm->TelnetStop->Enabled=true;
    MainForm->TelnetRecycle->Enabled=true;
    MainForm->TelnetPause->Enabled=true;
    MainForm->TelnetPause->Checked=false;        
    Application->ProcessMessages();
}
static void bbs_start(void)
{
	Screen->Cursor=crAppStart;
    bbs_status(NULL,"Starting");

    FILE* fp=fopen(MainForm->ini_file,"r");
    sbbs_read_ini(fp, MainForm->ini_file
        ,&MainForm->global
        ,NULL   ,&MainForm->bbs_startup
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,NULL
        );
    if(fp!=NULL)
        fclose(fp);

	_beginthread((void(*)(void*))bbs_thread,0,&MainForm->bbs_startup);
    Application->ProcessMessages();
}

static void event_log_msg(log_msg_t* msg)
{
	log_msg(EventsForm->Log, msg);
}

static void services_log_msg(log_msg_t* msg)
{
	log_msg(ServicesForm->Log, msg);
}

static void services_status(void* p, const char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

	ServicesForm->Status->Caption=AnsiString(str);

    ReleaseMutex(mutex);
}

static void services_terminated(void* p, int code)
{
	Screen->Cursor=crDefault;
	MainForm->ServicesStart->Enabled=true;
	MainForm->ServicesStop->Enabled=false;
    MainForm->ServicesRecycle->Enabled=false;
    MainForm->ServicesPause->Enabled=false;
    MainForm->ServicesPause->Checked=false;
    Application->ProcessMessages();
}
static void services_started(void* p)
{
	Screen->Cursor=crDefault;
	MainForm->ServicesStart->Enabled=false;
    MainForm->ServicesStop->Enabled=true;
    MainForm->ServicesRecycle->Enabled=true;
    MainForm->ServicesPause->Enabled=true;
    MainForm->ServicesPause->Checked=false;
    Application->ProcessMessages();
}

static void services_clients(void* p, int clients)
{
}

static void mail_log_msg(log_msg_t* msg)
{
	static FILE* LogStream;

    if(msg==NULL) {
        if(LogStream!=NULL)
            fclose(LogStream);
        LogStream=NULL;
        return;
    }

	log_msg(MailForm->Log, msg);

    if(MainForm->MailLogFile && MainForm->MailStop->Enabled) {
        AnsiString LogFileName
            =AnsiString(MainForm->cfg.logs_dir)
            +"LOGS\\MS"
            +SystemTimeToDateTime(msg->time).FormatString("mmddyy")
            +".LOG";

        if(!FileExists(LogFileName)) {
            if(LogStream!=NULL) {
            	fclose(LogStream);
                LogStream=NULL;
            }
        }
        if(LogStream==NULL)
            LogStream=_fsopen(LogFileName.c_str(),"a",SH_DENYNONE);

        if(LogStream!=NULL) {
			AnsiString Line=SystemTimeToDateTime(msg->time).FormatString("hh:mm:ss")+"  ";
		    Line+=AnsiString(msg->buf).Trim();
			if(msg->repeated)
				Line += " [x" + AnsiString(msg->repeated + 1) + "]";
	        Line+="\n";
        	fwrite(AnsiString(Line).c_str(),1,Line.Length(),LogStream);
        }
	}
}

static void mail_status(void* p, const char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

	MailForm->Status->Caption=AnsiString(str);

    ReleaseMutex(mutex);
}

static void mail_clients(void* p, int clients)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    MailForm->ProgressBar->Max=MainForm->mail_startup.max_clients;
	MailForm->ProgressBar->Position=clients;

    ReleaseMutex(mutex);
}

static void mail_terminated(void* p, int code)
{
	Screen->Cursor=crDefault;
	MainForm->MailStart->Enabled=true;
	MainForm->MailStop->Enabled=false;
    MainForm->MailRecycle->Enabled=false;
    MainForm->MailPause->Enabled=false;
    MainForm->MailPause->Checked=false;
    Application->ProcessMessages();
}
static void mail_started(void* p)
{
	Screen->Cursor=crDefault;
	MainForm->MailStart->Enabled=false;
    MainForm->MailStop->Enabled=true;
    MainForm->MailRecycle->Enabled=true;
    MainForm->MailPause->Enabled=true;
    MainForm->MailPause->Checked=false;    
    Application->ProcessMessages();
}
static void mail_start(void)
{
	Screen->Cursor=crAppStart;
    mail_status(NULL, "Starting");

    FILE* fp=fopen(MainForm->ini_file,"r");
    sbbs_read_ini(fp, MainForm->ini_file
        ,&MainForm->global
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,&MainForm->mail_startup
        ,NULL   ,NULL
        );
    if(fp!=NULL)
        fclose(fp);

	_beginthread((void(*)(void*))mail_server,0,&MainForm->mail_startup);
    Application->ProcessMessages();
}

static void ftp_log_msg(log_msg_t* msg)
{
	static FILE* LogStream;

    if(msg==NULL) {
        if(LogStream!=NULL)
            fclose(LogStream);
        LogStream=NULL;
        return;
    }

	log_msg(FtpForm->Log, msg);

    if(MainForm->FtpLogFile && MainForm->FtpStop->Enabled) {
        AnsiString LogFileName
            =AnsiString(MainForm->cfg.logs_dir)
            +"LOGS\\FS"
            +SystemTimeToDateTime(msg->time).FormatString("mmddyy")
            +".LOG";

        if(!FileExists(LogFileName)) {
            FileClose(FileCreate(LogFileName));
            if(LogStream!=NULL) {
                fclose(LogStream);
                LogStream=NULL;
            }
        }
        if(LogStream==NULL)
            LogStream=_fsopen(LogFileName.c_str(),"a",SH_DENYNONE);

        if(LogStream!=NULL) {
            AnsiString Line=SystemTimeToDateTime(msg->time).FormatString("hh:mm:ss")+"  ";
            Line+=AnsiString(msg->buf).Trim();
			if(msg->repeated)
				Line += " [x" + AnsiString(msg->repeated + 1) + "]";
            Line+="\n";
        	fwrite(AnsiString(Line).c_str(),1,Line.Length(),LogStream);
        }
	}
}

static void ftp_status(void* p, const char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

	FtpForm->Status->Caption=AnsiString(str);

    ReleaseMutex(mutex);
}

static void ftp_clients(void* p, int clients)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    FtpForm->ProgressBar->Max=MainForm->ftp_startup.max_clients;
	FtpForm->ProgressBar->Position=clients;

    ReleaseMutex(mutex);
}

static void ftp_terminated(void* p, int code)
{
	Screen->Cursor=crDefault;
	MainForm->FtpStart->Enabled=true;
	MainForm->FtpStop->Enabled=false;
    MainForm->FtpRecycle->Enabled=false;
    MainForm->FtpPause->Enabled=false;
    MainForm->FtpPause->Checked=false;
    Application->ProcessMessages();
}
static void ftp_started(void* p)
{
	Screen->Cursor=crDefault;
	MainForm->FtpStart->Enabled=false;
    MainForm->FtpStop->Enabled=true;
    MainForm->FtpRecycle->Enabled=true;
    MainForm->FtpPause->Enabled=true;
    MainForm->FtpPause->Checked=false;    
    Application->ProcessMessages();
}
static void ftp_start(void)
{
	Screen->Cursor=crAppStart;
    ftp_status(NULL, "Starting");

    FILE* fp=fopen(MainForm->ini_file,"r");
    sbbs_read_ini(fp, MainForm->ini_file
        ,&MainForm->global
        ,NULL   ,NULL
        ,NULL   ,&MainForm->ftp_startup
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,NULL
        );
    if(fp!=NULL)
        fclose(fp);

	_beginthread((void(*)(void*))ftp_server,0,&MainForm->ftp_startup);
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------
static void web_log_msg(log_msg_t* msg)
{
	static FILE* LogStream;

    if(msg==NULL) {
        if(LogStream!=NULL)
            fclose(LogStream);
        LogStream=NULL;
        return;
    }

	log_msg(WebForm->Log, msg);
}

static void web_status(void* p, const char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

	WebForm->Status->Caption=AnsiString(str);

    ReleaseMutex(mutex);
}

static void web_clients(void* p, int clients)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    WebForm->ProgressBar->Max=MainForm->web_startup.max_clients;
	WebForm->ProgressBar->Position=clients;

    ReleaseMutex(mutex);
}

static void web_terminated(void* p, int code)
{
	Screen->Cursor=crDefault;
	MainForm->WebStart->Enabled=true;
	MainForm->WebStop->Enabled=false;
    MainForm->WebRecycle->Enabled=false;
    MainForm->WebPause->Enabled=false;  // caused exception
    MainForm->WebPause->Checked=false;
    Application->ProcessMessages();
}
static void web_started(void* p)
{
	Screen->Cursor=crDefault;
	MainForm->WebStart->Enabled=false;
    MainForm->WebStop->Enabled=true;
    MainForm->WebRecycle->Enabled=true;
    MainForm->WebPause->Enabled=true;
    MainForm->WebPause->Checked=false;    
    Application->ProcessMessages();
}
static void web_start(void)
{
	Screen->Cursor=crAppStart;
    web_status(NULL, "Starting");

    FILE* fp=fopen(MainForm->ini_file,"r");
    sbbs_read_ini(fp, MainForm->ini_file
        ,&MainForm->global
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,&MainForm->web_startup
        ,NULL   ,NULL
        ,NULL   ,NULL
        );
    if(fp!=NULL)
        fclose(fp);

	_beginthread((void(*)(void*))web_server,0,&MainForm->web_startup);
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------
/* Server recycle callback (read relevant startup .ini file section)		*/
static void recycle(void* cbdata)
{
    char            str[MAX_PATH*2];
    FILE*           fp=NULL;
	bbs_startup_t*  bbs=NULL;
	ftp_startup_t*  ftp=NULL;
	web_startup_t*  web=NULL;
	mail_startup_t* mail=NULL;
	services_startup_t* services=NULL;

    SAFEPRINTF(str,"Reading %s",MainForm->ini_file);
	if(cbdata==(void*)&bbs_log_list) {
		bbs=&MainForm->bbs_startup;
        lputs(cbdata,LOG_INFO,str);
	} else if(cbdata==(void*)&ftp_log_list) {
		ftp=&MainForm->ftp_startup;
        lputs(cbdata,LOG_INFO,str);
    } else if(cbdata==(void*)&web_log_list) {
		web=&MainForm->web_startup;
        lputs(cbdata,LOG_INFO,str);
    } else if(cbdata==(void*)&mail_log_list) {
		mail=&MainForm->mail_startup;
        lputs(cbdata,LOG_INFO,str);
	} else if(cbdata==(void*)&services_log_list) {
		services=&MainForm->services_startup;
        lputs(cbdata,LOG_INFO,str);
    }

    fp=fopen(MainForm->ini_file,"r");
    sbbs_read_ini(fp, MainForm->ini_file
        ,&MainForm->global
        ,NULL   ,bbs
        ,NULL   ,ftp
        ,NULL   ,web
        ,NULL   ,mail
        ,NULL   ,services
        );
    if(fp!=NULL)
        fclose(fp);
	MainForm->SetControls();
}
//---------------------------------------------------------------------------
__fastcall TMainForm::TMainForm(TComponent* Owner)
        : TForm(Owner)
{
    /* Defaults */
    memset(&global,0,sizeof(global));
    SAFECOPY(global.ctrl_dir,"c:\\sbbs\\ctrl\\");
    global.js.max_bytes=JAVASCRIPT_MAX_BYTES;
    global.js.cx_stack=JAVASCRIPT_CONTEXT_STACK;
    global.js.time_limit=JAVASCRIPT_TIME_LIMIT;
    global.js.gc_interval=JAVASCRIPT_GC_INTERVAL;
    global.js.yield_interval=JAVASCRIPT_YIELD_INTERVAL;
    global.sem_chk_freq=DEFAULT_SEM_CHK_FREQ;		/* seconds */

    /* These are SBBSCTRL-specific */
    LoginCommand="telnet://127.0.0.1";
    ConfigCommand="%sscfg.exe %s -l25";
    MinimizeToSysTray=false;
    UndockableForms=false;
    UseFileAssociations=true;
    Initialized=false;

	loginAttemptListInit(&login_attempt_list);

    char* p;
    if((p=getenv("SBBSCTRL"))!=NULL)
        SAFECOPY(global.ctrl_dir,p);
   	p=lastchar(global.ctrl_dir);
	if(*p!='/' && *p!='\\')
    	strcat(global.ctrl_dir,"\\");
        
    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);
    bbs_startup.cbdata=&bbs_log_list;
    bbs_startup.event_cbdata=&event_log_list;    
    bbs_startup.first_node=1;
    bbs_startup.last_node=4;
	bbs_startup.options=BBS_OPT_XTRN_MINIMIZED;
	bbs_startup.telnet_port=IPPORT_TELNET;
    bbs_startup.rlogin_port=513;
	bbs_startup.lputs=lputs;
    bbs_startup.event_lputs=lputs;
    bbs_startup.errormsg=errormsg;
    bbs_startup.status=bbs_status;
    bbs_startup.clients=bbs_clients;
    bbs_startup.started=bbs_started;
    bbs_startup.recycle=recycle;
    bbs_startup.terminated=bbs_terminated;
    bbs_startup.thread_up=thread_up;
    bbs_startup.client_on=client_on;
    bbs_startup.socket_open=socket_open;
	bbs_startup.login_attempt_list=&login_attempt_list;

    memset(&mail_startup,0,sizeof(mail_startup));
    mail_startup.size=sizeof(mail_startup);
    mail_startup.cbdata=&mail_log_list;
    mail_startup.smtp_port=IPPORT_SMTP;
    mail_startup.relay_port=IPPORT_SMTP;
    mail_startup.pop3_port=110;
	mail_startup.lputs=lputs;
    mail_startup.errormsg=errormsg;
    mail_startup.status=mail_status;
    mail_startup.clients=mail_clients;
    mail_startup.started=mail_started;
    mail_startup.recycle=recycle;
    mail_startup.terminated=mail_terminated;
    mail_startup.options=MAIL_OPT_ALLOW_POP3;
    mail_startup.thread_up=thread_up;
    mail_startup.client_on=client_on;
    mail_startup.socket_open=socket_open;
    mail_startup.max_delivery_attempts=50;
    mail_startup.rescan_frequency=3600;  /* 60 minutes */
    mail_startup.lines_per_yield=10;
    mail_startup.max_clients=10;
    mail_startup.max_msg_size=10*1024*1024;
	mail_startup.login_attempt_list=&login_attempt_list;

    memset(&ftp_startup,0,sizeof(ftp_startup));
    ftp_startup.size=sizeof(ftp_startup);
    ftp_startup.cbdata=&ftp_log_list;
    ftp_startup.port=IPPORT_FTP;
	ftp_startup.lputs=lputs;
    ftp_startup.errormsg=errormsg;
    ftp_startup.status=ftp_status;
    ftp_startup.clients=ftp_clients;
    ftp_startup.started=ftp_started;
    ftp_startup.recycle=recycle;
    ftp_startup.terminated=ftp_terminated;
    ftp_startup.thread_up=thread_up;
    ftp_startup.client_on=client_on;
    ftp_startup.socket_open=socket_open;
	ftp_startup.options
        =FTP_OPT_INDEX_FILE|FTP_OPT_HTML_INDEX_FILE|FTP_OPT_ALLOW_QWK;
    ftp_startup.max_clients=10;
    strcpy(ftp_startup.index_file_name,"00index");
    strcpy(ftp_startup.html_index_file,"00index.html");
    strcpy(ftp_startup.html_index_script,"ftp-html.js");
	ftp_startup.login_attempt_list=&login_attempt_list;

    memset(&web_startup,0,sizeof(web_startup));
    web_startup.size=sizeof(web_startup);
    web_startup.cbdata=&web_log_list;
	web_startup.lputs=lputs;
    web_startup.errormsg=errormsg;
    web_startup.status=web_status;
    web_startup.clients=web_clients;
    web_startup.started=web_started;
    web_startup.recycle=recycle;
    web_startup.terminated=web_terminated;
    web_startup.thread_up=thread_up;
    web_startup.client_on=client_on;
    web_startup.socket_open=socket_open;
	web_startup.login_attempt_list=&login_attempt_list;

    memset(&services_startup,0,sizeof(services_startup));
    services_startup.size=sizeof(services_startup);
    services_startup.cbdata=&services_log_list;
    services_startup.lputs=lputs;
    services_startup.errormsg=errormsg;
    services_startup.status=services_status;
    services_startup.clients=services_clients;
    services_startup.started=services_started;
    services_startup.recycle=recycle;    
    services_startup.terminated=services_terminated;
    services_startup.thread_up=thread_up;
    services_startup.client_on=client_on;
    services_startup.socket_open=socket_open;
	services_startup.login_attempt_list=&login_attempt_list;

    bbs_log=INVALID_HANDLE_VALUE;
    event_log=INVALID_HANDLE_VALUE;
    ftp_log=INVALID_HANDLE_VALUE;
    web_log=INVALID_HANDLE_VALUE;    
    mail_log=INVALID_HANDLE_VALUE;
    services_log=INVALID_HANDLE_VALUE;

    /* Default local "Spy Terminal" settings */
    SpyTerminalFont=new TFont;
    SpyTerminalFont->Name="Terminal";
    SpyTerminalFont->Size=9;
    SpyTerminalFont->Pitch=fpFixed;
    SpyTerminalWidth=434;
    SpyTerminalHeight=365;
    SpyTerminalKeyboardActive=true;

    Application->OnMinimize=FormMinimize;

    /* Get pointers to NT Service functions */
	HANDLE hSCMlib = LoadLibrary("ADVAPI32.DLL");
	if(hSCMlib!=NULL) {
		openSCManager=(OpenSCManager_t)GetProcAddress(hSCMlib, "OpenSCManagerA");
		openService=(OpenService_t)GetProcAddress(hSCMlib,"OpenServiceA");
		startService=(StartService_t)GetProcAddress(hSCMlib,"StartServiceA");
		controlService=(ControlService_t)GetProcAddress(hSCMlib,"ControlService");
		queryServiceStatus=(QueryServiceStatus_t)GetProcAddress(hSCMlib,"QueryServiceStatus");
		queryServiceConfig=(QueryServiceConfig_t)GetProcAddress(hSCMlib,"QueryServiceConfigA");
		closeServiceHandle=(CloseServiceHandle_t)GetProcAddress(hSCMlib,"CloseServiceHandle");
		FreeLibrary(hSCMlib);
    }

    /* Open Service Manager */
    if(openSCManager!=NULL)
        hSCManager = openSCManager(
                            NULL,                   // machine (NULL == local)
                            NULL,                   // database (NULL == default)
                            SC_MANAGER_CONNECT		// access required
                            );

	/* Open Synchronet Services */
    if(hSCManager!=NULL) {
		bbs_svc =  openService(hSCManager, NTSVC_NAME_BBS, GENERIC_READ|GENERIC_EXECUTE);
		ftp_svc =  openService(hSCManager, NTSVC_NAME_FTP, GENERIC_READ|GENERIC_EXECUTE);
		web_svc =  openService(hSCManager, NTSVC_NAME_WEB, GENERIC_READ|GENERIC_EXECUTE);        
		mail_svc =  openService(hSCManager, NTSVC_NAME_MAIL, GENERIC_READ|GENERIC_EXECUTE);
		services_svc =  openService(hSCManager, NTSVC_NAME_SERVICES, GENERIC_READ|GENERIC_EXECUTE);
    }

    {
        int i;

        for(i=LOG_EMERG;i<=LOG_DEBUG;i++) {
            LogFont[i] = new TFont;
            LogFont[i]->Color=LogLevelColor[i];
            if(i<=LOG_ERR)
                LogFont[i]->Style = TFontStyles()<< fsBold;
        }
    }

    listInit(&bbs_log_list, LINK_LIST_MUTEX);
    listInit(&event_log_list, LINK_LIST_MUTEX);    
    listInit(&ftp_log_list, LINK_LIST_MUTEX);
    listInit(&web_log_list, LINK_LIST_MUTEX);
    listInit(&mail_log_list, LINK_LIST_MUTEX);
    listInit(&services_log_list, LINK_LIST_MUTEX);

    TelnetPause->DisableIfNoHandler=false;
    MailPause->DisableIfNoHandler=false;
    WebPause->DisableIfNoHandler=false;
    FtpPause->DisableIfNoHandler=false;
    ServicesPause->DisableIfNoHandler=false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FileExitMenuItemClick(TObject *Sender)
{
        Close();
}
void __fastcall TMainForm::FormCreate(TObject *Sender)
{
	Height=400;	// Just incase we mess it up in the IDE
    Width=700;

    // Verify SBBS.DLL version
    long bbs_ver = bbs_ver_num();
    if(bbs_ver != VERSION_HEX) {
        char str[128];
        sprintf(str,"Incorrect SBBS.DLL Version (%lX, expected %lx)", bbs_ver, VERSION_HEX);
    	Application->MessageBox(str,"ERROR",MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
    }

    // Read Registry keys
	TRegistry* Registry=new TRegistry;
    if(!Registry->OpenKey(REG_KEY,true)) {
    	Application->MessageBox("Error opening registry key"
        	,REG_KEY,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
        return;
    }
   	if(Registry->ValueExists("MainFormTop"))
	   	Top=Registry->ReadInteger("MainFormTop");
   	if(Registry->ValueExists("MainFormLeft"))
	   	Left=Registry->ReadInteger("MainFormLeft");
   	if(Registry->ValueExists("MainFormHeight"))
	   	Height=Registry->ReadInteger("MainFormHeight");
   	if(Registry->ValueExists("MainFormWidth"))
	   	Width=Registry->ReadInteger("MainFormWidth");
    else
        WindowState=wsMaximized;    // Default to fullscreen
   	if(Registry->ValueExists("SpyTerminalWidth"))
	   	SpyTerminalWidth=Registry->ReadInteger("SpyTerminalWidth");
   	if(Registry->ValueExists("SpyTerminalHeight"))
	   	SpyTerminalHeight=Registry->ReadInteger("SpyTerminalHeight");
   	if(Registry->ValueExists("SpyTerminalFontName"))
	   	SpyTerminalFont->Name=Registry->ReadString("SpyTerminalFontName");
   	if(Registry->ValueExists("SpyTerminalFontSize"))
	   	SpyTerminalFont->Size=Registry->ReadInteger("SpyTerminalFontSize");
   	if(Registry->ValueExists("SpyTerminalKeyboardActive"))
	   	SpyTerminalKeyboardActive
            =Registry->ReadBool("SpyTerminalKeyboardActive");

    Registry->CloseKey();
    delete Registry;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormShow(TObject *Sender)
{
	StartupTimer->Enabled=true;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewToolbarMenuItemClick(TObject *Sender)
{
	Toolbar->Visible=ViewToolbarMenuItem->Checked;
}
//---------------------------------------------------------------------------
BOOL NTsvcEnabled(SC_HANDLE svc, QUERY_SERVICE_CONFIG* config, DWORD config_size)
{
	if(svc==NULL || startService==NULL || queryServiceStatus==NULL || config==NULL)
    	return(FALSE);

	DWORD ret;
	if(!queryServiceConfig(svc,config,config_size,&ret))
		return(FALSE);
	if(config->dwStartType==SERVICE_DISABLED)
		return(FALSE);
	return(TRUE);
}
//---------------------------------------------------------------------------
BOOL __fastcall TMainForm::bbsServiceEnabled(void)
{
    return NTsvcEnabled(bbs_svc,bbs_svc_config,bbs_svc_config_size);
}
BOOL __fastcall TMainForm::mailServiceEnabled(void)
{
    return NTsvcEnabled(mail_svc,mail_svc_config,mail_svc_config_size);
}
BOOL __fastcall TMainForm::ftpServiceEnabled(void)
{
    return NTsvcEnabled(ftp_svc,ftp_svc_config,ftp_svc_config_size);
}
BOOL __fastcall TMainForm::webServiceEnabled(void)
{
    return NTsvcEnabled(web_svc,web_svc_config,web_svc_config_size);
}
BOOL __fastcall TMainForm::servicesServiceEnabled(void)
{
    return NTsvcEnabled(services_svc,services_svc_config,services_svc_config_size);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormClose(TObject *Sender, TCloseAction &Action)
{
    UpTimer->Enabled=false; /* Stop updating the status bar */
	StatsTimer->Enabled=false;

    if(TrayIcon->Visible)           /* minimized to tray? */
        TrayIcon->Visible=false;    /* restore to avoid crash */
        
    /* This is necessary to save form sizes/positions */
    if(Initialized) /* Don't overwrite registry and .ini settings with defaults */
        SaveSettings(Sender);

	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Terminating servers...";
    time_t start=time(NULL);
	while( (TelnetStop->Enabled     && !bbsServiceEnabled())
        || (MailStop->Enabled       && !mailServiceEnabled())
        || (FtpStop->Enabled        && !ftpServiceEnabled())
        || (WebStop->Enabled        && !webServiceEnabled())
    	|| (ServicesStop->Enabled   && !servicesServiceEnabled())) {
        if(time(NULL)-start>30)
            break;
        Application->ProcessMessages();
        YIELD();
    }
	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Closing...";
    Application->ProcessMessages();
    
	LogTimer->Enabled=false;

	ServiceStatusTimer->Enabled=false;
	NodeForm->Timer->Enabled=false;
	ClientForm->Timer->Enabled=false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormCloseQuery(TObject *Sender, bool &CanClose)
{
	CanClose=false;

    if(TelnetStop->Enabled && !bbsServiceEnabled()) {
     	if(!terminating && TelnetForm->ProgressBar->Position
	        && Application->MessageBox("Shut down the Terminal Server?"
        	,"Synchronet Terminal Server In Use", MB_OKCANCEL)!=IDOK)
            return;
        TelnetStopExecute(Sender);
	}

    if(MailStop->Enabled && !mailServiceEnabled()) {
    	if(!terminating && MailForm->ProgressBar->Position
    		&& Application->MessageBox("Shut down the Mail Server?"
        	,"Synchronet Mail Server In Use", MB_OKCANCEL)!=IDOK)
            return;
        MailStopExecute(Sender);
    }

    if(FtpStop->Enabled && !ftpServiceEnabled()) {
    	if(!terminating && FtpForm->ProgressBar->Position
    		&& Application->MessageBox("Shut down the FTP Server?"
	       	,"Synchronet FTP Server In Use", MB_OKCANCEL)!=IDOK)
            return;
        FtpStopExecute(Sender);
    }

    if(WebStop->Enabled && !webServiceEnabled()) {
    	if(!terminating && WebForm->ProgressBar->Position
    		&& Application->MessageBox("Shut down the Web Server?"
	       	,"Synchronet Web Server In Use", MB_OKCANCEL)!=IDOK)
            return;
        WebStopExecute(Sender);
    }

    if(ServicesStop->Enabled && !servicesServiceEnabled())
	    ServicesStopExecute(Sender);

    CanClose=true;
}

//---------------------------------------------------------------------------
BOOL StartNTsvc(SC_HANDLE svc, SERVICE_STATUS* status, QUERY_SERVICE_CONFIG* config, DWORD config_size)
{
	if(svc==NULL || startService==NULL || queryServiceStatus==NULL || config==NULL)
    	return(FALSE);

	DWORD ret;
	if(!queryServiceConfig(svc,config,config_size,&ret))
		return(FALSE);
	if(config->dwStartType==SERVICE_DISABLED)
		return(FALSE);
    if(!queryServiceStatus(svc,status))
    	return(FALSE);
    if(status->dwCurrentState!=SERVICE_STOPPED)
    	return(TRUE);
    if(!startService(svc,0,NULL))
       	Application->MessageBox(AnsiString("ERROR " + IntToStr(GetLastError()) +
            " starting " + config->lpDisplayName).c_str()
            ,"ERROR"
            ,MB_OK|MB_ICONEXCLAMATION);
    return(TRUE);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::TelnetStartExecute(TObject *Sender)
{
	if(!StartNTsvc(bbs_svc,&bbs_svc_status,bbs_svc_config,bbs_svc_config_size))
		bbs_start();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ServicesStartExecute(TObject *Sender)
{
	if(StartNTsvc(services_svc,&services_svc_status,services_svc_config,services_svc_config_size))
    	return;
	Screen->Cursor=crAppStart;
    services_status(NULL, "Starting");

    FILE* fp=fopen(ini_file,"r");
    sbbs_read_ini(fp, MainForm->ini_file
        ,&MainForm->global
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,&services_startup
        );
    if(fp!=NULL)
        fclose(fp);

	_beginthread((void(*)(void*))services_thread,0,&services_startup);
    Application->ProcessMessages();
}

//---------------------------------------------------------------------------
BOOL StopNTsvc(SC_HANDLE svc, SERVICE_STATUS* status)
{
	if(svc==NULL || controlService==NULL)
    	return(FALSE);

    return controlService(svc,SERVICE_CONTROL_STOP,status);
}

//---------------------------------------------------------------------------

void __fastcall TMainForm::ServicesStopExecute(TObject *Sender)
{
	if(StopNTsvc(services_svc,&services_svc_status))
    	return;
    Screen->Cursor=crAppStart;
    services_status(NULL, "Terminating");
    services_terminate();
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::TelnetStopExecute(TObject *Sender)
{
	if(StopNTsvc(bbs_svc,&bbs_svc_status))
    	return;
    Screen->Cursor=crAppStart;
    bbs_status(NULL, "Terminating");
    bbs_terminate();
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::TelnetConfigureExecute(TObject *Sender)
{
    static inside;
    if(inside) return;
    inside=true;

	Application->CreateForm(__classid(TTelnetCfgDlg), &TelnetCfgDlg);
	TelnetCfgDlg->ShowModal();
    delete TelnetCfgDlg;
    
    inside=false;
}
//---------------------------------------------------------------------------


void __fastcall TMainForm::NodeListStartExecute(TObject *Sender)
{
	NodeForm->Timer->Enabled=true;
    NodeListStart->Enabled=false;
    NodeListStop->Enabled=true;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::NodeListStopExecute(TObject *Sender)
{
    NodeForm->Timer->Enabled=false;
    NodeListStart->Enabled=true;
    NodeListStop->Enabled=false;
}
//---------------------------------------------------------------------------


void __fastcall TMainForm::MailConfigureExecute(TObject *Sender)
{
    static inside;
    if(inside) return;
    inside=true;

	Application->CreateForm(__classid(TMailCfgDlg), &MailCfgDlg);
	MailCfgDlg->ShowModal();
    delete MailCfgDlg;

    inside=false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::MailStartExecute(TObject *Sender)
{
	if(!StartNTsvc(mail_svc,&mail_svc_status,mail_svc_config,mail_svc_config_size))
	    mail_start();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::MailStopExecute(TObject *Sender)
{
	if(StopNTsvc(mail_svc,&mail_svc_status))
    	return;
    Screen->Cursor=crAppStart;
    mail_status(NULL, "Terminating");
    mail_terminate();
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewTelnetExecute(TObject *Sender)
{
	TelnetForm->Visible=ViewTelnet->Checked;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewEventsExecute(TObject *Sender)
{
	EventsForm->Visible=ViewEvents->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewNodesExecute(TObject *Sender)
{
	NodeForm->Visible=ViewNodes->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewMailServerExecute(TObject *Sender)
{
	MailForm->Visible=ViewMailServer->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewFtpServerExecute(TObject *Sender)
{
	FtpForm->Visible=ViewFtpServer->Checked;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewWebServerExecute(TObject *Sender)
{
    WebForm->Visible=ViewWebServer->Checked;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewServicesExecute(TObject *Sender)
{
    ServicesForm->Visible=ViewServices->Checked;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewStatsExecute(TObject *Sender)
{
	StatsForm->Visible=ViewStats->Checked;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewClientsExecute(TObject *Sender)
{
	ClientForm->Visible=ViewClients->Checked;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FtpStartExecute(TObject *Sender)
{
	if(!StartNTsvc(ftp_svc,&ftp_svc_status,ftp_svc_config,ftp_svc_config_size))
		ftp_start();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FtpStopExecute(TObject *Sender)
{
	if(StopNTsvc(ftp_svc,&ftp_svc_status))
    	return;
    Screen->Cursor=crAppStart;
    ftp_status(NULL, "Terminating");
    ftp_terminate();
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FtpConfigureExecute(TObject *Sender)
{
    static inside;
    if(inside) return;
    inside=true;

	Application->CreateForm(__classid(TFtpCfgDlg), &FtpCfgDlg);
	FtpCfgDlg->ShowModal();
    delete FtpCfgDlg;

    inside=false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::WebStartExecute(TObject *Sender)
{
	if(!StartNTsvc(web_svc,&web_svc_status,web_svc_config,web_svc_config_size))
		web_start();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::WebStopExecute(TObject *Sender)
{
	if(StopNTsvc(web_svc,&web_svc_status))
    	return;
    Screen->Cursor=crAppStart;
    web_status(NULL, "Terminating");
    web_terminate();
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::WebConfigureExecute(TObject *Sender)
{
    static inside;
    if(inside) return;
    inside=true;

	Application->CreateForm(__classid(TWebCfgDlg), &WebCfgDlg);
	WebCfgDlg->ShowModal();
    delete WebCfgDlg;

    inside=false;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BBSConfigureMenuItemClick(TObject *Sender)
{
	char str[256];

    sprintf(str,ConfigCommand.c_str()
    	,cfg.exec_dir, cfg.ctrl_dir);
    STARTUPINFO startup_info={0};
    PROCESS_INFORMATION process_info;
    startup_info.cb=sizeof(startup_info);
    startup_info.lpTitle="Synchronet Configuration Utility";
	if(!CreateProcess(
		NULL,			// pointer to name of executable module
		str,  			// pointer to command line string
		NULL,  			// process security attributes
		NULL,   		// thread security attributes
		FALSE, 			// handle inheritance flag
		CREATE_NEW_CONSOLE|CREATE_SEPARATE_WOW_VDM, // creation flags
        NULL,  			// pointer to new environment block
		cfg.ctrl_dir,	// pointer to current directory name
		&startup_info,  // pointer to STARTUPINFO
		&process_info  	// pointer to PROCESS_INFORMATION
		))
       	Application->MessageBox(AnsiString("ERROR " + IntToStr(GetLastError()) +
            " executing " + str).c_str()
            ,"ERROR"
            ,MB_OK|MB_ICONEXCLAMATION);
	// Resource leak if you don't close these:
	CloseHandle(process_info.hThread);
	CloseHandle(process_info.hProcess);
}
//---------------------------------------------------------------------------




void __fastcall TMainForm::NodeCloseButtonClick(TObject *Sender)
{
	ViewNodesExecute(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::TelnetCloseButtonClick(TObject *Sender)
{
	ViewTelnetExecute(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::MailCloseButtonClick(TObject *Sender)
{
	ViewMailServerExecute(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FtpCloseButtonClick(TObject *Sender)
{
	ViewFtpServerExecute(Sender);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::StatsTimerTick(TObject *Sender)
{
	char 	str[128];
	int 	i;
	static stats_t stats;
	static users;
	static newusers;
	static counter;

	if(!StatsForm->Visible)
		return;

    getstats(&cfg,0,&stats);

	StatsForm->TotalLogons->Caption=AnsiString(stats.logons);
    StatsForm->LogonsToday->Caption=AnsiString(stats.ltoday);
    StatsForm->TotalTimeOn->Caption=AnsiString(stats.timeon);
    StatsForm->TimeToday->Caption=AnsiString(stats.ttoday);
    StatsForm->TotalEMail->Caption=AnsiString(getmail(&cfg,0,0,0));
	StatsForm->EMailToday->Caption=AnsiString(stats.etoday);
	StatsForm->TotalFeedback->Caption=AnsiString(getmail(&cfg,1,0,0));
	StatsForm->FeedbackToday->Caption=AnsiString(stats.ftoday);
	/* Don't scan a large user database more often than necessary */
	if(!counter || users<100 || (counter%(users/100))==0 || stats.nusers!=newusers)
		users=total_users(&cfg);
    StatsForm->TotalUsers->Caption=AnsiString(users);
    StatsForm->NewUsersToday->Caption=AnsiString(newusers=stats.nusers);
    StatsForm->PostsToday->Caption=AnsiString(stats.ptoday);
    StatsForm->UploadedFiles->Caption=AnsiString(stats.uls);
	if(stats.ulb>=1024*1024)
    	sprintf(str,"%.1fM",stats.ulb/(1024.0*1024.0));
    else if(stats.ulb>=1024)
    	sprintf(str,"%luK",stats.ulb/1024);
    else
    	sprintf(str,"%lu",stats.ulb);
    StatsForm->UploadedBytes->Caption=AnsiString(str);
    StatsForm->DownloadedFiles->Caption=AnsiString(stats.dls);
	if(stats.dlb>=1024*1024)
    	sprintf(str,"%.1fM",stats.dlb/(1024.0*1024.0));
    else if(stats.dlb>=1024)
    	sprintf(str,"%luK",stats.dlb/1024);
    else
    	sprintf(str,"%lu",stats.dlb);
    StatsForm->DownloadedBytes->Caption=AnsiString(str);

	counter++;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::StatsCloseButtonClick(TObject *Sender)
{
	ViewStatsExecute(Sender);
}
//---------------------------------------------------------------------------
enum {
	 PAGE_UPPERLEFT
    ,PAGE_UPPERRIGHT
    ,PAGE_LOWERRIGHT
    ,PAGE_LOWERLEFT
    };
TPageControl* __fastcall TMainForm::PageControl(int num)
{
	switch(num)
    {
    	case PAGE_UPPERLEFT:
        	return(UpperLeftPageControl);
        case PAGE_UPPERRIGHT:
        	return(UpperRightPageControl);
        case PAGE_LOWERRIGHT:
        	return(LowerRightPageControl);
        case PAGE_LOWERLEFT:
        	return(LowerLeftPageControl);
    }
    return(NULL);
}
int __fastcall TMainForm::PageNum(TPageControl* obj)
{
    if(obj==UpperLeftPageControl)
       	return(PAGE_UPPERLEFT);
    if(obj==UpperRightPageControl)
       	return(PAGE_UPPERRIGHT);
    if(obj==LowerRightPageControl)
       	return(PAGE_LOWERRIGHT);
    if(obj==LowerLeftPageControl)
       	return(PAGE_LOWERLEFT);
	return(PAGE_LOWERRIGHT);
}
TColor __fastcall TMainForm::ReadColor(TRegistry* Registry
    ,AnsiString name)
{
    if(Registry->ValueExists(name + "Color"))
        return(StringToColor(Registry->ReadString(name + "Color")));
        
    return(clWindow);   // Default
}
void __fastcall TMainForm::WriteColor(TRegistry* Registry
    ,AnsiString name, TColor color)
{
    Registry->WriteString(name + "Color", ColorToString(color));
}

int FontStyleToInt(TFont* Font)
{
    int style=0;
    for(int i=fsBold;i<=fsStrikeOut;i++)
    	if(Font->Style.Contains((TFontStyle)i))
        	style|=(1<<i);
    return(style);
}

void IntToFontStyle(int style, TFont* Font)
{
    Font->Style=Font->Style.Clear();
    for(int i=fsBold;i<=fsStrikeOut;i++)
        if(style&(1<<i))
            Font->Style=Font->Style<<(TFontStyle)i;
}

void __fastcall TMainForm::ReadFont(AnsiString subkey, TFont* Font)
{
    // Read Registry keys
	TRegistry* Registry=new TRegistry;
    AnsiString key = REG_KEY + subkey + "Font";
    if(!Registry->OpenKey(key,true)) {
    	Application->MessageBox("Error opening registry key"
        	,key.c_str(),MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
        return;
    }
    if(Registry->ValueExists("Name"))
        Font->Name=Registry->ReadString("Name");
    if(Registry->ValueExists("Color"))
        Font->Color=StringToColor(Registry->ReadString("Color"));
    if(Registry->ValueExists("Height"))
        Font->Height=Registry->ReadInteger("Height");
    if(Registry->ValueExists("Size"))
        Font->Size=Registry->ReadInteger("Size");

    if(Registry->ValueExists("Style"))
        IntToFontStyle(Registry->ReadInteger("Style"),Font);

    Registry->CloseKey();
    delete Registry;
}
void __fastcall TMainForm::WriteFont(AnsiString subkey, TFont* Font)
{
    // Read Registry keys
	TRegistry* Registry=new TRegistry;
    AnsiString key = REG_KEY + subkey + "Font";
    if(!Registry->OpenKey(key,true)) {
    	Application->MessageBox("Error opening registry key"
        	,key.c_str(),MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
        return;
    }
    Registry->WriteString("Name",Font->Name);
    Registry->WriteString("Color",ColorToString(Font->Color));
    Registry->WriteInteger("Height",Font->Height);
    Registry->WriteInteger("Size",Font->Size);
    Registry->WriteInteger("Style",FontStyleToInt(Font));

    Registry->CloseKey();
    delete Registry;
}

void __fastcall TMainForm::SetControls(void)
{
    TelnetForm->LogLevelUpDown->Position=bbs_startup.log_level;
    TelnetForm->LogLevelText->Caption=LogLevelDesc[bbs_startup.log_level];
    FtpForm->LogLevelUpDown->Position=ftp_startup.log_level;
    FtpForm->LogLevelText->Caption=LogLevelDesc[ftp_startup.log_level];
    MailForm->LogLevelUpDown->Position=mail_startup.log_level;
    MailForm->LogLevelText->Caption=LogLevelDesc[mail_startup.log_level];
    WebForm->LogLevelUpDown->Position=web_startup.log_level;
    WebForm->LogLevelText->Caption=LogLevelDesc[web_startup.log_level];
    ServicesForm->LogLevelUpDown->Position=services_startup.log_level;
    ServicesForm->LogLevelText->Caption=LogLevelDesc[services_startup.log_level];

    if(cfg.total_faddrs)
        FidonetMenuItem->Visible = true;
    else
        FidonetMenuItem->Visible = false;
}

void __fastcall TMainForm::StartupTimerTick(TObject *Sender)
{
    bool	TelnetFormFloating=false;
    bool	EventsFormFloating=false;
    bool	ServicesFormFloating=false;
    bool 	NodeFormFloating=false;
    bool	StatsFormFloating=false;
    bool	ClientFormFloating=false;
    bool 	MailFormFloating=false;
    bool	FtpFormFloating=false;
    bool	WebFormFloating=false;
    int		NodeFormPage=PAGE_UPPERLEFT;
    int		StatsFormPage=PAGE_UPPERLEFT;
    int		ClientFormPage=PAGE_UPPERLEFT;
    int		TelnetFormPage=PAGE_LOWERLEFT;
    int		EventsFormPage=PAGE_LOWERLEFT;
    int		MailFormPage=PAGE_UPPERRIGHT;
    int		FtpFormPage=PAGE_LOWERRIGHT;
    int		WebFormPage=PAGE_LOWERRIGHT;
    int     ServicesFormPage=PAGE_LOWERRIGHT;
#if 0   /* not yet working */
    bool	TelnetFormVisible=true;
    bool	EventsFormVisible=true;
    bool	ServicesFormVisible=true;
    bool 	NodeFormVisible=true;
    bool	StatsFormVisible=true;
    bool	ClientFormVisible=true;
    bool 	MailFormVisible=true;
    bool	FtpFormVisible=true;
    bool	WebFormVisible=true;
#endif

    AnsiString	Str;

    delete StartupTimer;

    // Read Registry keys
	TRegistry* Registry=new TRegistry;
    if(!Registry->OpenKey(REG_KEY,true)) {
    	Application->MessageBox("Error opening registry key"
        	,REG_KEY,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
        return;
    }

    TopPanel->Height=Height/3;
    UpperLeftPageControl->Width=Width/2;
    LowerLeftPageControl->Width=Width/2;

    if(Registry->ValueExists("TopPanelHeight"))
    	TopPanel->Height=Registry->ReadInteger("TopPanelHeight");
    if(Registry->ValueExists("UpperLeftPageControlWidth"))
    	UpperLeftPageControl->Width
        	=Registry->ReadInteger("UpperLeftPageControlWidth");
    if(Registry->ValueExists("LowerLeftPageControlWidth"))
    	LowerLeftPageControl->Width
        	=Registry->ReadInteger("LowerLeftPageControlWidth");
    if(Registry->ValueExists("UndockableForms"))
        UndockableForms=Registry->ReadBool("UndockableForms");

    if(UndockableForms) {
        if(Registry->ValueExists("TelnetFormFloating"))
            TelnetFormFloating=Registry->ReadBool("TelnetFormFloating");
        if(Registry->ValueExists("EventsFormFloating"))
            EventsFormFloating=Registry->ReadBool("EventsFormFloating");
        if(Registry->ValueExists("ServicesFormFloating"))
            ServicesFormFloating=Registry->ReadBool("ServicesFormFloating");
        if(Registry->ValueExists("NodeFormFloating"))
            NodeFormFloating=Registry->ReadBool("NodeFormFloating");
        if(Registry->ValueExists("StatsFormFloating"))
            StatsFormFloating=Registry->ReadBool("StatsFormFloating");
        if(Registry->ValueExists("ClientFormFloating"))
            ClientFormFloating=Registry->ReadBool("ClientFormFloating");
        if(Registry->ValueExists("MailFormFloating"))
            MailFormFloating=Registry->ReadBool("MailFormFloating");
        if(Registry->ValueExists("FtpFormFloating"))
            FtpFormFloating=Registry->ReadBool("FtpFormFloating");
        if(Registry->ValueExists("WebFormFloating"))
            WebFormFloating=Registry->ReadBool("WebFormFloating");
    }
#if 0
    if(Registry->ValueExists("TelnetFormVisible"))
        TelnetFormVisible=Registry->ReadBool("TelnetFormVisible");
    if(Registry->ValueExists("EventsFormVisible"))
        EventsFormVisible=Registry->ReadBool("EventsFormVisible");
    if(Registry->ValueExists("ServicesFormVisible"))
        ServicesFormVisible=Registry->ReadBool("ServicesFormVisible");
    if(Registry->ValueExists("NodeFormVisible"))
        NodeFormVisible=Registry->ReadBool("NodeFormVisible");
    if(Registry->ValueExists("StatsFormVisible"))
        StatsFormVisible=Registry->ReadBool("StatsFormVisible");
    if(Registry->ValueExists("ClientFormVisible"))
        ClientFormVisible=Registry->ReadBool("ClientFormVisible");
    if(Registry->ValueExists("MailFormVisible"))
        MailFormVisible=Registry->ReadBool("MailFormVisible");
    if(Registry->ValueExists("FtpFormVisible"))
        FtpFormVisible=Registry->ReadBool("FtpFormVisible");
    if(Registry->ValueExists("WebFormVisible"))
        WebFormVisible=Registry->ReadBool("WebFormVisible");
#endif
    if(Registry->ValueExists("TelnetFormPage"))
    	TelnetFormPage=Registry->ReadInteger("TelnetFormPage");
    if(Registry->ValueExists("EventsFormPage"))
    	EventsFormPage=Registry->ReadInteger("EventsFormPage");
    if(Registry->ValueExists("ServicesFormPage"))
    	ServicesFormPage=Registry->ReadInteger("ServicesFormPage");
    if(Registry->ValueExists("NodeFormPage"))
    	NodeFormPage=Registry->ReadInteger("NodeFormPage");
    if(Registry->ValueExists("StatsFormPage"))
    	StatsFormPage=Registry->ReadInteger("StatsFormPage");
    if(Registry->ValueExists("ClientFormPage"))
    	ClientFormPage=Registry->ReadInteger("ClientFormPage");
    if(Registry->ValueExists("MailFormPage"))
    	MailFormPage=Registry->ReadInteger("MailFormPage");
    if(Registry->ValueExists("FtpFormPage"))
    	FtpFormPage=Registry->ReadInteger("FtpFormPage");
    if(Registry->ValueExists("WebFormPage"))
    	WebFormPage=Registry->ReadInteger("WebFormPage");

    TelnetForm->Log->Color=ReadColor(Registry,"TelnetLog");
    ReadFont("TelnetLog",TelnetForm->Log->Font);
    EventsForm->Log->Color=ReadColor(Registry,"EventsLog");
    ReadFont("EventsLog",EventsForm->Log->Font);
    ServicesForm->Log->Color=ReadColor(Registry,"ServicesLog");
    ReadFont("ServicesLog",ServicesForm->Log->Font);
    MailForm->Log->Color=ReadColor(Registry,"MailLog");
    ReadFont("MailLog",MailForm->Log->Font);
    FtpForm->Log->Color=ReadColor(Registry,"FtpLog");
    ReadFont("FtpLog",FtpForm->Log->Font);
    WebForm->Log->Color=ReadColor(Registry,"WebLog");
    ReadFont("WebLog",WebForm->Log->Font);
    NodeForm->ListBox->Color=ReadColor(Registry,"NodeList");
    ReadFont("NodeList",NodeForm->ListBox->Font);
    ClientForm->ListView->Color=ReadColor(Registry,"ClientList");
    ReadFont("ClientList",ClientForm->ListView->Font);

    {
        int i;

        for(i=LOG_EMERG; i<=LOG_DEBUG; i++)
            ReadFont("Log" + AnsiString(LogLevelDesc[i]), LogFont[i]);
    }

	if(Registry->ValueExists("TelnetFormTop"))
    	TelnetForm->Top=Registry->ReadInteger("TelnetFormTop");
	if(Registry->ValueExists("TelnetFormLeft"))
    	TelnetForm->Left=Registry->ReadInteger("TelnetFormLeft");
	if(Registry->ValueExists("TelnetFormWidth"))
    	TelnetForm->Width=Registry->ReadInteger("TelnetFormWidth");
	if(Registry->ValueExists("TelnetFormHeight"))
    	TelnetForm->Height=Registry->ReadInteger("TelnetFormHeight");
	if(Registry->ValueExists("TelnetFormMonitor"))
    	TelnetForm->DefaultMonitor=(TDefaultMonitor)Registry->ReadInteger("TelnetFormMonitor");

	if(Registry->ValueExists("EventsFormTop"))
    	EventsForm->Top=Registry->ReadInteger("EventsFormTop");
	if(Registry->ValueExists("EventsFormLeft"))
    	EventsForm->Left=Registry->ReadInteger("EventsFormLeft");
	if(Registry->ValueExists("EventsFormWidth"))
    	EventsForm->Width=Registry->ReadInteger("EventsFormWidth");
	if(Registry->ValueExists("EventsFormHeight"))
    	EventsForm->Height=Registry->ReadInteger("EventsFormHeight");
	if(Registry->ValueExists("EventsFormMonitor"))
    	EventsForm->DefaultMonitor=(TDefaultMonitor)Registry->ReadInteger("EventsFormMonitor");

	if(Registry->ValueExists("ServicesFormTop"))
    	ServicesForm->Top=Registry->ReadInteger("ServicesFormTop");
	if(Registry->ValueExists("ServicesFormLeft"))
    	ServicesForm->Left=Registry->ReadInteger("ServicesFormLeft");
	if(Registry->ValueExists("ServicesFormWidth"))
    	ServicesForm->Width=Registry->ReadInteger("ServicesFormWidth");
	if(Registry->ValueExists("ServicesFormHeight"))
    	ServicesForm->Height=Registry->ReadInteger("ServicesFormHeight");
	if(Registry->ValueExists("ServicesFormMonitor"))
    	ServicesForm->DefaultMonitor=(TDefaultMonitor)Registry->ReadInteger("ServicesFormMonitor");

	if(Registry->ValueExists("FtpFormTop"))
    	FtpForm->Top=Registry->ReadInteger("FtpFormTop");
	if(Registry->ValueExists("FtpFormLeft"))
    	FtpForm->Left=Registry->ReadInteger("FtpFormLeft");
	if(Registry->ValueExists("FtpFormWidth"))
    	FtpForm->Width=Registry->ReadInteger("FtpFormWidth");
	if(Registry->ValueExists("FtpFormHeight"))
    	FtpForm->Height=Registry->ReadInteger("FtpFormHeight");
	if(Registry->ValueExists("FtpFormMonitor"))
    	FtpForm->DefaultMonitor=(TDefaultMonitor)Registry->ReadInteger("FtpFormMonitor");

	if(Registry->ValueExists("WebFormTop"))
    	WebForm->Top=Registry->ReadInteger("WebFormTop");
	if(Registry->ValueExists("WebFormLeft"))
    	WebForm->Left=Registry->ReadInteger("WebFormLeft");
	if(Registry->ValueExists("WebFormWidth"))
    	WebForm->Width=Registry->ReadInteger("WebFormWidth");
	if(Registry->ValueExists("WebFormHeight"))
    	WebForm->Height=Registry->ReadInteger("WebFormHeight");
	if(Registry->ValueExists("WebFormMonitor"))
    	WebForm->DefaultMonitor=(TDefaultMonitor)Registry->ReadInteger("WebFormMonitor");

	if(Registry->ValueExists("MailFormTop"))
    	MailForm->Top=Registry->ReadInteger("MailFormTop");
	if(Registry->ValueExists("MailFormLeft"))
    	MailForm->Left=Registry->ReadInteger("MailFormLeft");
	if(Registry->ValueExists("MailFormWidth"))
    	MailForm->Width=Registry->ReadInteger("MailFormWidth");
	if(Registry->ValueExists("MailFormHeight"))
    	MailForm->Height=Registry->ReadInteger("MailFormHeight");
	if(Registry->ValueExists("MailFormMonitor"))
    	MailForm->DefaultMonitor=(TDefaultMonitor)Registry->ReadInteger("MailFormMonitor");

	if(Registry->ValueExists("NodeFormTop"))
    	NodeForm->Top=Registry->ReadInteger("NodeFormTop");
	if(Registry->ValueExists("NodeFormLeft"))
    	NodeForm->Left=Registry->ReadInteger("NodeFormLeft");
	if(Registry->ValueExists("NodeFormWidth"))
    	NodeForm->Width=Registry->ReadInteger("NodeFormWidth");
	if(Registry->ValueExists("NodeFormHeight"))
    	NodeForm->Height=Registry->ReadInteger("NodeFormHeight");
	if(Registry->ValueExists("NodeFormMonitor"))
    	NodeForm->DefaultMonitor=(TDefaultMonitor)Registry->ReadInteger("NodeFormMonitor");

	if(Registry->ValueExists("StatsFormTop"))
    	StatsForm->Top=Registry->ReadInteger("StatsFormTop");
	if(Registry->ValueExists("StatsFormLeft"))
    	StatsForm->Left=Registry->ReadInteger("StatsFormLeft");
	if(Registry->ValueExists("StatsFormWidth"))
    	StatsForm->Width=Registry->ReadInteger("StatsFormWidth");
	if(Registry->ValueExists("StatsFormHeight"))
    	StatsForm->Height=Registry->ReadInteger("StatsFormHeight");
	if(Registry->ValueExists("StatsFormMonitor"))
    	StatsForm->DefaultMonitor=(TDefaultMonitor)Registry->ReadInteger("StatsFormMonitor");

	if(Registry->ValueExists("ClientFormTop"))
    	ClientForm->Top=Registry->ReadInteger("ClientFormTop");
	if(Registry->ValueExists("ClientFormLeft"))
    	ClientForm->Left=Registry->ReadInteger("ClientFormLeft");
	if(Registry->ValueExists("ClientFormWidth"))
    	ClientForm->Width=Registry->ReadInteger("ClientFormWidth");
	if(Registry->ValueExists("ClientFormHeight"))
    	ClientForm->Height=Registry->ReadInteger("ClientFormHeight");
	if(Registry->ValueExists("ClientFormMonitor"))
    	ClientForm->DefaultMonitor=(TDefaultMonitor)Registry->ReadInteger("ClientFormMonitor");

    for(int i=0;i<ClientForm->ListView->Columns->Count;i++) {
        char str[128];
        sprintf(str,"ClientListColumn%dWidth",i);
        if(Registry->ValueExists(str))
            ClientForm->ListView->Columns->Items[i]->Width
                =Registry->ReadInteger(str);
    }

    if(Registry->ValueExists("ToolbarVisible"))
    	Toolbar->Visible=Registry->ReadBool("ToolbarVisible");

    ViewToolbarMenuItem->Checked=Toolbar->Visible;
    ViewStatusBarMenuItem->Checked=StatusBar->Visible;

    if(Registry->ValueExists("MaxLogLen"))
    	MaxLogLen=Registry->ReadInteger("MaxLogLen");

    if(Registry->ValueExists("LoginCommand"))
    	LoginCommand=Registry->ReadString("LoginCommand");
    if(Registry->ValueExists("ConfigCommand"))
    	ConfigCommand=Registry->ReadString("ConfigCommand");
    if(Registry->ValueExists("Password"))
    	Password=Registry->ReadString("Password");
    if(Registry->ValueExists("MinimizeToSysTray"))
    	MinimizeToSysTray=Registry->ReadBool("MinimizeToSysTray");
    if(Registry->ValueExists("UseFileAssociations"))
    	UseFileAssociations=Registry->ReadBool("UseFileAssociations");
	if(Registry->ValueExists("NodeDisplayInterval"))
    	NodeForm->Timer->Interval=Registry->ReadInteger("NodeDisplayInterval")*1000;
	if(Registry->ValueExists("ClientDisplayInterval"))
    	ClientForm->Timer->Interval=Registry->ReadInteger("ClientDisplayInterval")*1000;
    if(Registry->ValueExists("ErrorSoundFile"))
        ErrorSoundFile=Registry->ReadString("ErrorSoundFile");

    if(Registry->ValueExists("MailLogFile"))
    	MailLogFile=Registry->ReadInteger("MailLogFile");
    else
    	MailLogFile=true;

    if(Registry->ValueExists("FtpLogFile"))
    	FtpLogFile=Registry->ReadInteger("FtpLogFile");
    else
    	FtpLogFile=true;

    FILE* fp;
    if((!Registry->ValueExists("SysAutoStart")
        || (Registry->ValueExists("Imported") && Registry->ReadBool("Imported")))
        && ini_file[0]) {
        if((fp=fopen(ini_file,"r"))==NULL) {
            char err[MAX_PATH*2];
            sprintf(err,"Error %d opening initialization file: %s",errno,ini_file);
            Application->MessageBox(err,"ERROR",MB_OK|MB_ICONEXCLAMATION);
            Application->Terminate();
            return;
        }
        sbbs_read_ini(fp, MainForm->ini_file
            ,&global
            ,&SysAutoStart   		,&bbs_startup
            ,&FtpAutoStart 			,&ftp_startup
            ,&WebAutoStart 			,&web_startup
            ,&MailAutoStart 	    ,&mail_startup
            ,&ServicesAutoStart     ,&services_startup
            );
       	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Read " + AnsiString(ini_file);
        fclose(fp);

    } else {    /* Legacy (v3.10-3.11) */

        if(Registry->ValueExists("SysAutoStart"))
            SysAutoStart=Registry->ReadInteger("SysAutoStart");
        else
            SysAutoStart=true;

        if(Registry->ValueExists("MailAutoStart"))
            MailAutoStart=Registry->ReadInteger("MailAutoStart");
        else
            MailAutoStart=true;

        if(Registry->ValueExists("FtpAutoStart"))
            FtpAutoStart=Registry->ReadInteger("FtpAutoStart");
        else
            FtpAutoStart=true;

        if(Registry->ValueExists("WebAutoStart"))
            WebAutoStart=Registry->ReadInteger("WebAutoStart");
        else
            WebAutoStart=true;

        if(Registry->ValueExists("ServicesAutoStart"))
            ServicesAutoStart=Registry->ReadInteger("ServicesAutoStart");
        else
            ServicesAutoStart=true;

        if(Registry->ValueExists("Hostname"))
            SAFECOPY(global.host_name,Registry->ReadString("Hostname").c_str());
		if(Registry->ValueExists("CtrlDirectory"))
            SAFECOPY(global.ctrl_dir,Registry->ReadString("CtrlDirectory").c_str());
        if(Registry->ValueExists("TempDirectory"))
            SAFECOPY(global.temp_dir,Registry->ReadString("TempDirectory").c_str());

        if(Registry->ValueExists("SemFileCheckFrequency"))
            global.sem_chk_freq=Registry->ReadInteger("SemFileCheckFrequency");

        /* JavaScript Operating Parameters */
        if(Registry->ValueExists("JS_MaxBytes"))
            global.js.max_bytes=Registry->ReadInteger("JS_MaxBytes");
        if(global.js.max_bytes==0)
            global.js.max_bytes=JAVASCRIPT_MAX_BYTES;
        if(Registry->ValueExists("JS_ContextStack"))
            global.js.cx_stack=Registry->ReadInteger("JS_ContextStack");
        if(global.js.cx_stack==0)
            global.js.cx_stack=JAVASCRIPT_CONTEXT_STACK;
        if(Registry->ValueExists("JS_GcInterval"))
            global.js.gc_interval=Registry->ReadInteger("JS_GcInterval");
        if(Registry->ValueExists("JS_YieldInterval"))
            global.js.yield_interval=Registry->ReadInteger("JS_YieldInterval");

/*
        if(Registry->ValueExists("TelnetInterface"))
            bbs_startup.telnet_interface=Registry->ReadInteger("TelnetInterface");
        if(Registry->ValueExists("RLoginInterface"))
            bbs_startup.rlogin_interface=Registry->ReadInteger("RLoginInterface");
*/

        if(Registry->ValueExists("TelnetPort"))
            bbs_startup.telnet_port=Registry->ReadInteger("TelnetPort");
        if(Registry->ValueExists("RLoginPort"))
            bbs_startup.rlogin_port=Registry->ReadInteger("RLoginPort");

        if(Registry->ValueExists("FirstNode"))
            bbs_startup.first_node=Registry->ReadInteger("FirstNode");

        if(Registry->ValueExists("LastNode"))
            bbs_startup.last_node=Registry->ReadInteger("LastNode");

        if(Registry->ValueExists("OutbufHighwaterMark"))
            bbs_startup.outbuf_highwater_mark=Registry->ReadInteger("OutbufHighwaterMark");
        else
            bbs_startup.outbuf_highwater_mark=1024;
        if(Registry->ValueExists("OutbufDrainTimeout"))
            bbs_startup.outbuf_drain_timeout=Registry->ReadInteger("OutbufDrainTimeout");
        else
            bbs_startup.outbuf_drain_timeout=10;

        if(Registry->ValueExists("AnswerSound"))
            SAFECOPY(bbs_startup.answer_sound
                ,Registry->ReadString("AnswerSound").c_str());

        if(Registry->ValueExists("HangupSound"))
            SAFECOPY(bbs_startup.hangup_sound
                ,Registry->ReadString("HangupSound").c_str());

        if(Registry->ValueExists("StartupOptions"))
            bbs_startup.options=Registry->ReadInteger("StartupOptions");

        if(Registry->ValueExists("MailMaxClients"))
            mail_startup.max_clients=Registry->ReadInteger("MailMaxClients");

        if(Registry->ValueExists("MailMaxInactivity"))
            mail_startup.max_inactivity=Registry->ReadInteger("MailMaxInactivity");

/*
        if(Registry->ValueExists("MailInterface"))
            mail_startup.interface_addr=Registry->ReadInteger("MailInterface");
*/

        if(Registry->ValueExists("MailMaxDeliveryAttempts"))
            mail_startup.max_delivery_attempts
                =Registry->ReadInteger("MailMaxDeliveryAttempts");

        if(Registry->ValueExists("MailRescanFrequency"))
            mail_startup.rescan_frequency
                =Registry->ReadInteger("MailRescanFrequency");

        if(Registry->ValueExists("MailLinesPerYield"))
            mail_startup.lines_per_yield
                =Registry->ReadInteger("MailLinesPerYield");

        if(Registry->ValueExists("MailMaxRecipients"))
            mail_startup.max_recipients
                =Registry->ReadInteger("MailMaxRecipients");

        if(Registry->ValueExists("MailMaxMsgSize"))
            mail_startup.max_msg_size
                =Registry->ReadInteger("MailMaxMsgSize");

        if(Registry->ValueExists("MailSMTPPort"))
            mail_startup.smtp_port=Registry->ReadInteger("MailSMTPPort");

        if(Registry->ValueExists("MailPOP3Port"))
            mail_startup.pop3_port=Registry->ReadInteger("MailPOP3Port");

        if(Registry->ValueExists("MailRelayServer"))
            SAFECOPY(mail_startup.relay_server
                ,Registry->ReadString("MailRelayServer").c_str());

        if(Registry->ValueExists("MailRelayPort"))
            mail_startup.relay_port=Registry->ReadInteger("MailRelayPort");

        if(Registry->ValueExists("MailDefaultUser"))
            SAFECOPY(mail_startup.default_user
                ,Registry->ReadString("MailDefaultUser").c_str());

        if(Registry->ValueExists("MailDNSBlacklistSubject"))
            SAFECOPY(mail_startup.dnsbl_tag
                ,Registry->ReadString("MailDNSBlacklistSubject").c_str());
        else
            SAFECOPY(mail_startup.dnsbl_tag,"SPAM");

        if(Registry->ValueExists("MailDNSBlacklistHeader"))
            SAFECOPY(mail_startup.dnsbl_hdr
                ,Registry->ReadString("MailDNSBlacklistHeader").c_str());
        else
            SAFECOPY(mail_startup.dnsbl_hdr,"X-DNSBL");

        if(Registry->ValueExists("MailDNSServer"))
            SAFECOPY(mail_startup.dns_server
                ,Registry->ReadString("MailDNSServer").c_str());

        if(Registry->ValueExists("MailInboundSound"))
            SAFECOPY(mail_startup.inbound_sound
                ,Registry->ReadString("MailInboundSound").c_str());

        if(Registry->ValueExists("MailOutboundSound"))
            SAFECOPY(mail_startup.outbound_sound
                ,Registry->ReadString("MailOutboundSound").c_str());

        if(Registry->ValueExists("MailPOP3Sound"))
            SAFECOPY(mail_startup.pop3_sound
                ,Registry->ReadString("MailPOP3Sound").c_str());

        if(Registry->ValueExists("MailOptions"))
            mail_startup.options=Registry->ReadInteger("MailOptions");

        if(Registry->ValueExists("FtpMaxClients"))
            ftp_startup.max_clients=Registry->ReadInteger("FtpMaxClients");

        if(Registry->ValueExists("FtpMaxInactivity"))
            ftp_startup.max_inactivity=Registry->ReadInteger("FtpMaxInactivity");

        if(Registry->ValueExists("FtpQwkTimeout"))
            ftp_startup.qwk_timeout=Registry->ReadInteger("FtpQwkTimeout");

/*
        if(Registry->ValueExists("FtpInterface"))
            ftp_startup.interface_addr=Registry->ReadInteger("FtpInterface");
*/

        if(Registry->ValueExists("FtpPort"))
            ftp_startup.port=Registry->ReadInteger("FtpPort");

        if(Registry->ValueExists("FtpAnswerSound"))
            SAFECOPY(ftp_startup.answer_sound
                ,Registry->ReadString("FtpAnswerSound").c_str());

        if(Registry->ValueExists("FtpHangupSound"))
            SAFECOPY(ftp_startup.hangup_sound
                ,Registry->ReadString("FtpHangupSound").c_str());

        if(Registry->ValueExists("FtpHackAttemptSound"))
            SAFECOPY(ftp_startup.hack_sound
                ,Registry->ReadString("FtpHackAttemptSound").c_str());

        if(Registry->ValueExists("FtpIndexFileName"))
            SAFECOPY(ftp_startup.index_file_name
                ,Registry->ReadString("FtpIndexFileName").c_str());

        if(Registry->ValueExists("FtpHtmlIndexFile"))
            SAFECOPY(ftp_startup.html_index_file
                ,Registry->ReadString("FtpHtmlIndexFile").c_str());

        if(Registry->ValueExists("FtpHtmlIndexScript"))
            SAFECOPY(ftp_startup.html_index_script
                ,Registry->ReadString("FtpHtmlIndexScript").c_str());

        if(Registry->ValueExists("FtpOptions"))
            ftp_startup.options=Registry->ReadInteger("FtpOptions");

/*
        if(Registry->ValueExists("ServicesInterface"))
            services_startup.interface_addr
                =Registry->ReadInteger("ServicesInterface");
*/

        if(Registry->ValueExists("ServicesAnswerSound"))
            SAFECOPY(services_startup.answer_sound
                ,Registry->ReadString("ServicesAnswerSound").c_str());

        if(Registry->ValueExists("ServicesHangupSound"))
            SAFECOPY(services_startup.hangup_sound
                ,Registry->ReadString("ServicesHangupSound").c_str());

        if(Registry->ValueExists("ServicesOptions"))
            services_startup.options=Registry->ReadInteger("ServicesOptions");

		if(SaveIniSettings(Sender))
            Registry->WriteBool("Imported",true);   /* Use the .ini file for these settings from now on */
    }

    Registry->CloseKey();
    delete Registry;

    AnsiString CtrlDirectory = AnsiString(global.ctrl_dir);
    if(!FileExists(CtrlDirectory+"MAIN.CNF")) {
		Application->CreateForm(__classid(TCtrlPathDialog), &CtrlPathDialog);
    	if(CtrlPathDialog->ShowModal()!=mrOk) {
        	Application->Terminate();
            return;
        }
        CtrlDirectory=CtrlPathDialog->Edit->Text;
        delete CtrlPathDialog;
    }
    if(CtrlDirectory.UpperCase().AnsiPos("MAIN.CNF"))
		CtrlDirectory.SetLength(CtrlDirectory.Length()-8);
    SAFECOPY(global.ctrl_dir,CtrlDirectory.c_str());
    memset(&cfg,0,sizeof(cfg));
    SAFECOPY(cfg.ctrl_dir,global.ctrl_dir);
    cfg.size=sizeof(cfg);
    cfg.node_num=bbs_startup.first_node;
    char error[256];
	SAFECOPY(error,UNKNOWN_LOAD_ERROR);

   	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Loading configuration...";
	if(!load_cfg(&cfg, NULL, TRUE, error)) {
    	Application->MessageBox(error,"ERROR Loading Configuration"
	        ,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
        return;
    }
   	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Configuration loaded";

	recycle_semfiles=semfile_list_init(cfg.ctrl_dir,"recycle","ctrl");
    semfile_list_add(&recycle_semfiles,ini_file);
   	semfile_list_check(&initialized,recycle_semfiles);

	shutdown_semfiles=semfile_list_init(cfg.ctrl_dir,"shutdown","ctrl");
	semfile_list_check(&initialized,shutdown_semfiles);

    if(cfg.new_install) {
    	Application->BringToFront();
        for(int i=0;i<10;i++) {
	        Application->ProcessMessages();
	    	Sleep(300);	// Let 'em see the logo for a bit
        }
        BBSConfigWizardMenuItemClick(Sender);
    }

    if(bbs_startup.options&BBS_OPT_MUTE)
    	SoundToggle->Checked=false;
    else
    	SoundToggle->Checked=true;

    if(sysop_available(&cfg))
    	ChatToggle->Checked=true;
    else
    	ChatToggle->Checked=false;

   	if(!NodeFormFloating)
    	NodeForm->ManualDock(PageControl(NodeFormPage),NULL,alClient);
	if(!ClientFormFloating)
    	ClientForm->ManualDock(PageControl(ClientFormPage),NULL,alClient);
	if(!StatsFormFloating)
    	StatsForm->ManualDock(PageControl(StatsFormPage),NULL,alClient);
	if(!MailFormFloating)
    	MailForm->ManualDock(PageControl(MailFormPage),NULL,alClient);
	if(!TelnetFormFloating)
    	TelnetForm->ManualDock(PageControl(TelnetFormPage),NULL,alClient);
	if(!EventsFormFloating)
    	EventsForm->ManualDock(PageControl(EventsFormPage),NULL,alClient);
	if(!ServicesFormFloating)
    	ServicesForm->ManualDock(PageControl(ServicesFormPage),NULL,alClient);
	if(!FtpFormFloating)
    	FtpForm->ManualDock(PageControl(FtpFormPage),NULL,alClient);
	if(!WebFormFloating)
    	WebForm->ManualDock(PageControl(WebFormPage),NULL,alClient);

    NodeForm->Show();
//    ViewNodes->Checked=NodeFormVisible,     ViewNodesExecute(Sender);
    ClientForm->Show();
//    ViewClients->Checked=ClientFormVisible, ViewClientsExecute(Sender);
    StatsForm->Show();
//    ViewStats->Checked=StatsFormVisible,    ViewStatsExecute(Sender);
    TelnetForm->Show();
//    ViewTelnet->Checked=TelnetFormVisible,  ViewTelnetExecute(Sender);
    EventsForm->Show();
//    ViewEvents->Checked=EventsFormVisible,  ViewEventsExecute(Sender);
    FtpForm->Show();
//    ViewFtpServer->Checked=FtpFormVisible,  ViewFtpServerExecute(Sender);
    WebForm->Show();
//    ViewWebServer->Checked=WebFormVisible;
//    WebForm->Visible=ViewWebServer->Checked;
    MailForm->Show();
//    ViewMailServer->Checked=MailFormVisible,ViewMailServerExecute(Sender);
    ServicesForm->Show();
//    ViewServices->Checked=ServicesFormVisible,ViewServicesExecute(Sender);

	UpperLeftPageControl->Visible=true;
	UpperRightPageControl->Visible=true;
	LowerLeftPageControl->Visible=true;
	LowerRightPageControl->Visible=true;
	HorizontalSplitter->Visible=true;
    BottomPanel->Visible=true;
   	TopPanel->Visible=true;

    // Work-around for CB5 PageControl anomaly
    int i;

    for(i=1;i<UpperLeftPageControl->PageCount;i++)
        UpperLeftPageControl->ActivePageIndex=i;
    UpperLeftPageControl->ActivePageIndex=0;

    for(i=1;i<UpperRightPageControl->PageCount;i++)
        UpperRightPageControl->ActivePageIndex=i;
    UpperRightPageControl->ActivePageIndex=0;

    for(i=1;i<LowerRightPageControl->PageCount;i++)
        LowerRightPageControl->ActivePageIndex=i;
    LowerRightPageControl->ActivePageIndex=0;

    for(i=1;i<LowerLeftPageControl->PageCount;i++)
        LowerLeftPageControl->ActivePageIndex=i;
    LowerLeftPageControl->ActivePageIndex=0;

    /* Open Log Mailslots */
    LogTimerTick(Sender);

    /* Query service config lengths */
	ServiceStatusTimerTick(Sender);

    if(SysAutoStart)
       TelnetStartExecute(Sender);
    if(MailAutoStart)
        MailStartExecute(Sender);
    if(FtpAutoStart)
        FtpStartExecute(Sender);
    if(WebAutoStart)
        WebStartExecute(Sender);
    if(ServicesAutoStart)
        ServicesStartExecute(Sender);

    NodeForm->Timer->Enabled=true;
    ClientForm->Timer->Enabled=true;

    SemFileTimer->Interval=global.sem_chk_freq*1000;
    SemFileTimer->Enabled=true;

    StatsTimer->Interval=cfg.node_stat_check*1000;
	StatsTimer->Enabled=true;

    UpTimer->Enabled=true; /* Start updating the status bar */
    LogTimer->Enabled=true;
                
    ServiceStatusTimer->Enabled=true;

	SetControls();
	
    if(!Application->Active)	/* Starting up minimized? */
    	FormMinimize(Sender);   /* Put icon in systray */

    Initialized=true;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SaveRegistrySettings(TObject* Sender)
{
	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Saving Registry Settings...";

    // Write Registry keys
	TRegistry* Registry=new TRegistry;
    if(!Registry->OpenKey(REG_KEY,true)) {
    	Application->MessageBox("Error creating registry key"
        	,REG_KEY,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
        return;
    }
    Registry->WriteInteger("MainFormTop",Top);
    Registry->WriteInteger("MainFormLeft",Left);
    Registry->WriteInteger("MainFormHeight",Height);
    Registry->WriteInteger("MainFormWidth",Width);

    Registry->WriteInteger("NodeFormTop",NodeForm->Top);
    Registry->WriteInteger("NodeFormLeft",NodeForm->Left);
    Registry->WriteInteger("NodeFormHeight",NodeForm->Height);
    Registry->WriteInteger("NodeFormWidth",NodeForm->Width);
    Registry->WriteInteger("NodeFormMonitor",NodeForm->DefaultMonitor);

    Registry->WriteInteger("StatsFormTop",StatsForm->Top);
    Registry->WriteInteger("StatsFormLeft",StatsForm->Left);
    Registry->WriteInteger("StatsFormHeight",StatsForm->Height);
    Registry->WriteInteger("StatsFormWidth",StatsForm->Width);
    Registry->WriteInteger("StatsFormMonitor",StatsForm->DefaultMonitor);

    Registry->WriteInteger("ClientFormTop",ClientForm->Top);
    Registry->WriteInteger("ClientFormLeft",ClientForm->Left);
    Registry->WriteInteger("ClientFormHeight",ClientForm->Height);
    Registry->WriteInteger("ClientFormWidth",ClientForm->Width);
    Registry->WriteInteger("ClientFormMonitor",ClientForm->DefaultMonitor);

    for(int i=0;i<ClientForm->ListView->Columns->Count;i++) {
        char str[128];
        sprintf(str,"ClientListColumn%dWidth",i);
        Registry->WriteInteger(str
            ,ClientForm->ListView->Columns->Items[i]->Width);
    }
    
    Registry->WriteInteger("TelnetFormTop",TelnetForm->Top);
    Registry->WriteInteger("TelnetFormLeft",TelnetForm->Left);
    Registry->WriteInteger("TelnetFormHeight",TelnetForm->Height);
    Registry->WriteInteger("TelnetFormWidth",TelnetForm->Width);
    Registry->WriteInteger("TelnetFormMonitor",TelnetForm->DefaultMonitor);

    Registry->WriteInteger("EventsFormTop",EventsForm->Top);
    Registry->WriteInteger("EventsFormLeft",EventsForm->Left);
    Registry->WriteInteger("EventsFormHeight",EventsForm->Height);
    Registry->WriteInteger("EventsFormWidth",EventsForm->Width);
    Registry->WriteInteger("EventsFormMonitor",EventsForm->DefaultMonitor);

    Registry->WriteInteger("ServicesFormTop",ServicesForm->Top);
    Registry->WriteInteger("ServicesFormLeft",ServicesForm->Left);
    Registry->WriteInteger("ServicesFormHeight",ServicesForm->Height);
    Registry->WriteInteger("ServicesFormWidth",ServicesForm->Width);
    Registry->WriteInteger("ServicesFormMonitor",ServicesForm->DefaultMonitor);

    Registry->WriteInteger("FtpFormTop",FtpForm->Top);
    Registry->WriteInteger("FtpFormLeft",FtpForm->Left);
    Registry->WriteInteger("FtpFormHeight",FtpForm->Height);
    Registry->WriteInteger("FtpFormWidth",FtpForm->Width);
    Registry->WriteInteger("FtpFormMonitor",FtpForm->DefaultMonitor);

    Registry->WriteInteger("WebFormTop",WebForm->Top);
    Registry->WriteInteger("WebFormLeft",WebForm->Left);
    Registry->WriteInteger("WebFormHeight",WebForm->Height);
    Registry->WriteInteger("WebFormWidth",WebForm->Width);
    Registry->WriteInteger("WebFormMonitor",WebForm->DefaultMonitor);

    Registry->WriteInteger("MailFormTop",MailForm->Top);
    Registry->WriteInteger("MailFormLeft",MailForm->Left);
    Registry->WriteInteger("MailFormHeight",MailForm->Height);
    Registry->WriteInteger("MailFormWidth",MailForm->Width);
    Registry->WriteInteger("MailFormMonitor",MailForm->DefaultMonitor);    

    Registry->WriteInteger("TopPanelHeight",TopPanel->Height);
 	Registry->WriteInteger("UpperLeftPageControlWidth"
    	,UpperLeftPageControl->Width);
    Registry->WriteInteger("LowerLeftPageControlWidth"
    	,LowerLeftPageControl->Width);

    Registry->WriteBool("UndockableForms",UndockableForms);

    Registry->WriteBool("TelnetFormFloating",TelnetForm->Floating);
    Registry->WriteBool("EventsFormFloating",EventsForm->Floating);
    Registry->WriteBool("ServicesFormFloating",ServicesForm->Floating);
    Registry->WriteBool("NodeFormFloating",NodeForm->Floating);
    Registry->WriteBool("StatsFormFloating",StatsForm->Floating);
    Registry->WriteBool("ClientFormFloating",ClientForm->Floating);
    Registry->WriteBool("FtpFormFloating",FtpForm->Floating);
    Registry->WriteBool("WebFormFloating",WebForm->Floating);
    Registry->WriteBool("MailFormFloating",MailForm->Floating);

    Registry->WriteBool("TelnetFormVisible",TelnetForm->Visible);
    Registry->WriteBool("EventsFormVisible",EventsForm->Visible);
    Registry->WriteBool("ServicesFormVisible",ServicesForm->Visible);
    Registry->WriteBool("NodeFormVisible",NodeForm->Visible);
    Registry->WriteBool("StatsFormVisible",StatsForm->Visible);
    Registry->WriteBool("ClientFormVisible",ClientForm->Visible);
    Registry->WriteBool("FtpFormVisible",FtpForm->Visible);
    Registry->WriteBool("WebFormVisible",WebForm->Visible);
    Registry->WriteBool("MailFormVisible",MailForm->Visible);

    Registry->WriteInteger("TelnetFormPage"
	    ,PageNum((TPageControl*)TelnetForm->HostDockSite));
    Registry->WriteInteger("EventsFormPage"
	    ,PageNum((TPageControl*)EventsForm->HostDockSite));
    Registry->WriteInteger("ServicesFormPage"
	    ,PageNum((TPageControl*)ServicesForm->HostDockSite));
    Registry->WriteInteger("NodeFormPage"
    	,PageNum((TPageControl*)NodeForm->HostDockSite));
    Registry->WriteInteger("MailFormPage"
    	,PageNum((TPageControl*)MailForm->HostDockSite));
    Registry->WriteInteger("FtpFormPage"
    	,PageNum((TPageControl*)FtpForm->HostDockSite));
    Registry->WriteInteger("WebFormPage"
    	,PageNum((TPageControl*)WebForm->HostDockSite));
    Registry->WriteInteger("StatsFormPage"
    	,PageNum((TPageControl*)StatsForm->HostDockSite));
    Registry->WriteInteger("ClientFormPage"
    	,PageNum((TPageControl*)ClientForm->HostDockSite));

    WriteColor(Registry,"TelnetLog",TelnetForm->Log->Color);
    WriteFont("TelnetLog",TelnetForm->Log->Font);
    WriteColor(Registry,"EventsLog",EventsForm->Log->Color);
    WriteFont("EventsLog",EventsForm->Log->Font);
    WriteColor(Registry,"ServicesLog",ServicesForm->Log->Color);
    WriteFont("ServicesLog",ServicesForm->Log->Font);
    WriteColor(Registry,"MailLog",MailForm->Log->Color);
    WriteFont("MailLog",MailForm->Log->Font);
    WriteColor(Registry,"FtpLog",FtpForm->Log->Color);
    WriteFont("FtpLog",FtpForm->Log->Font);
    WriteColor(Registry,"WebLog",WebForm->Log->Color);
    WriteFont("WebLog",WebForm->Log->Font);
    WriteColor(Registry,"NodeList",NodeForm->ListBox->Color);
    WriteFont("NodeList",NodeForm->ListBox->Font);
    WriteColor(Registry,"ClientList",ClientForm->ListView->Color);
    WriteFont("ClientList",ClientForm->ListView->Font);

    {
        int i;

        for(i=LOG_EMERG;i<=LOG_DEBUG;i++)
            WriteFont("Log" + AnsiString(LogLevelDesc[i]), LogFont[i]);
    }

    Registry->WriteBool("ToolBarVisible",Toolbar->Visible);
    Registry->WriteBool("StatusBarVisible",StatusBar->Visible);

    Registry->WriteInteger("MaxLogLen",MaxLogLen);

    Registry->WriteString("LoginCommand",LoginCommand);
    Registry->WriteString("ConfigCommand",ConfigCommand);
    Registry->WriteString("Password",Password);
    Registry->WriteBool("MinimizeToSysTray",MinimizeToSysTray);
    Registry->WriteBool("UseFileAssociations",UseFileAssociations);
    Registry->WriteInteger("NodeDisplayInterval",NodeForm->Timer->Interval/1000);
    Registry->WriteInteger("ClientDisplayInterval",ClientForm->Timer->Interval/1000);
    Registry->WriteString("ErrorSoundFile",ErrorSoundFile);

	Registry->WriteInteger( "SpyTerminalWidth"
                            ,SpyTerminalWidth);
	Registry->WriteInteger( "SpyTerminalHeight"
                            ,SpyTerminalHeight);
   	Registry->WriteString(  "SpyTerminalFontName"
                            ,SpyTerminalFont->Name);
	Registry->WriteInteger( "SpyTerminalFontSize"
                            ,SpyTerminalFont->Size);
	Registry->WriteBool(    "SpyTerminalKeyboardActive"
                            ,SpyTerminalKeyboardActive);

    Registry->CloseKey();
    delete Registry;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SaveSettings(TObject* Sender)
{
    SaveIniSettings(Sender);
    SaveRegistrySettings(Sender);
}
//---------------------------------------------------------------------------
bool __fastcall TMainForm::SaveIniSettings(TObject* Sender)
{
    FILE* fp=NULL;
   	if(ini_file[0]==0 || !Initialized)
        return(false);

    if((fp=fopen(ini_file,"r+"))==NULL) {
        char err[MAX_PATH*2];
        SAFEPRINTF2(err,"Error %d opening initialization file: %s",errno,ini_file);
        Application->MessageBox(err,"ERROR",MB_OK|MB_ICONEXCLAMATION);
        return(false);
    }

	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Saving Settings to " + AnsiString(ini_file) + " ...";

    bool success = sbbs_write_ini(fp
        ,&cfg
        ,&global
        ,SysAutoStart		,&bbs_startup
        ,FtpAutoStart		,&ftp_startup
        ,WebAutoStart		,&web_startup
        ,MailAutoStart		,&mail_startup
        ,ServicesAutoStart	,&services_startup
        );
    fclose(fp);

	if(!success) {
		char err[MAX_PATH*2];
        SAFEPRINTF(err,"Failure writing initialization file: %s",ini_file);
        Application->MessageBox(err,"ERROR",MB_OK|MB_ICONEXCLAMATION);
	}

   	semfile_list_check(&initialized,recycle_semfiles);    
    return(success);
}

//---------------------------------------------------------------------------
void __fastcall TMainForm::ImportFormSettings(TMemIniFile* IniFile, const char* section, TForm* Form)
{
   	Form->Top=IniFile->ReadInteger(section,"Top",Form->Top);
   	Form->Left=IniFile->ReadInteger(section,"Left",Form->Left);
   	Form->Width=IniFile->ReadInteger(section,"Width",Form->Width);
   	Form->Height=IniFile->ReadInteger(section,"Height",Form->Height);
   	Form->DefaultMonitor=(TDefaultMonitor)IniFile->ReadInteger(section,"Monitor",Form->DefaultMonitor);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ExportFormSettings(TMemIniFile* IniFile, const char* section, TForm* Form)
{
    IniFile->WriteInteger(section,"Top",Form->Top);
    IniFile->WriteInteger(section,"Left",Form->Left);
    IniFile->WriteInteger(section,"Width",Form->Width);
    IniFile->WriteInteger(section,"Height",Form->Height);
    IniFile->WriteInteger(section,"Page",PageNum((TPageControl*)Form->HostDockSite));
    IniFile->WriteBool(section,"Floating",Form->Floating);
    IniFile->WriteInteger(section,"Monitor",Form->DefaultMonitor);    
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ImportFont(TMemIniFile* IniFile, const char* section, AnsiString prefix, TFont* Font)
{
    Font->Name=IniFile->ReadString(section,prefix + "Name",Font->Name);
    Font->Color=StringToColor(IniFile->ReadString(section,prefix + "Color",ColorToString(Font->Color)));
    Font->Height=IniFile->ReadInteger(section,prefix + "Height",Font->Height);
    Font->Size=IniFile->ReadInteger(section,prefix + "Size", Font->Size);
    IntToFontStyle(IniFile->ReadInteger(section,prefix + "Style",FontStyleToInt(Font)),Font);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ExportFont(TMemIniFile* IniFile, const char* section, AnsiString prefix, TFont* Font)
{
    IniFile->WriteString(section,prefix+"Name",Font->Name);
    IniFile->WriteString(section,prefix+"Color",ColorToString(Font->Color));
    IniFile->WriteInteger(section,prefix+"Height",Font->Height);
    IniFile->WriteInteger(section,prefix+"Size",Font->Size);
    IniFile->WriteInteger(section,prefix+"Style",FontStyleToInt(Font));
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ImportSettings(TObject* Sender)
{
    OpenDialog->Filter="Settings files (*.ini)|*.ini|All files|*.*";
    OpenDialog->FileName=AnsiString(global.ctrl_dir)+"sbbsctrl.ini";
    if(!OpenDialog->Execute())
    	return;

	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Importing Settings...";

	TMemIniFile* IniFile=new TMemIniFile(OpenDialog->FileName);

    const char* section = "Properties";

   	LoginCommand=IniFile->ReadString(section,"LoginCommand",LoginCommand);
    ConfigCommand=IniFile->ReadString(section,"ConfigCommand",ConfigCommand);
   	Password=IniFile->ReadString(section,"Password",Password);
   	MinimizeToSysTray=IniFile->ReadBool(section,"MinimizeToSysTray",MinimizeToSysTray);
   	UseFileAssociations=IniFile->ReadBool(section,"UseFileAssociations"	,UseFileAssociations);
    UndockableForms=IniFile->ReadBool(section,"UndockableForms",UndockableForms);

    ImportFormSettings(IniFile,section="MainForm",MainForm);
   	TopPanel->Height=IniFile->ReadInteger(section,"TopPanelHeight",TopPanel->Height);
   	UpperLeftPageControl->Width=IniFile->ReadInteger(section,"UpperLeftPageControlWidth",UpperLeftPageControl->Width);
   	LowerLeftPageControl->Width=IniFile->ReadInteger(section,"LowerLeftPageControlWidth",LowerLeftPageControl->Width);
    Toolbar->Visible=IniFile->ReadBool(section,"ToolBarVisible",Toolbar->Visible);
    StatusBar->Visible=IniFile->ReadBool(section,"StatusBarVisible",StatusBar->Visible);

    ImportFormSettings(IniFile,section="TelnetForm",TelnetForm);
    ImportFont(IniFile,section,"LogFont",TelnetForm->Log->Font);
    TelnetForm->Log->Color=StringToColor(IniFile->ReadString(section,"LogColor",clWindow));

    ImportFormSettings(IniFile,section="EventsForm",EventsForm);
    ImportFont(IniFile,section,"LogFont",EventsForm->Log->Font);
    EventsForm->Log->Color=StringToColor(IniFile->ReadString(section,"LogColor",clWindow));

    ImportFormSettings(IniFile,section="ServicesForm",ServicesForm);
    ImportFont(IniFile,section,"LogFont",ServicesForm->Log->Font);
    ServicesForm->Log->Color=StringToColor(IniFile->ReadString(section,"LogColor",clWindow));

    ImportFormSettings(IniFile,section="FtpForm",FtpForm);
    ImportFont(IniFile,section,"LogFont",FtpForm->Log->Font);
   	FtpLogFile=IniFile->ReadInteger(section,"LogFile",true);
    FtpForm->Log->Color=StringToColor(IniFile->ReadString(section,"LogColor",clWindow));

    ImportFormSettings(IniFile,section="WebForm",WebForm);
    ImportFont(IniFile,section,"LogFont",WebForm->Log->Font);
    WebForm->Log->Color=StringToColor(IniFile->ReadString(section,"LogColor",clWindow));

    ImportFormSettings(IniFile,section="MailForm",MailForm);
    ImportFont(IniFile,section,"LogFont",MailForm->Log->Font);
   	MailLogFile=IniFile->ReadInteger(section,"LogFile",true);
    MailForm->Log->Color=StringToColor(IniFile->ReadString(section,"LogColor",clWindow));

    ImportFormSettings(IniFile,section="NodeForm",NodeForm);
    ImportFont(IniFile,section,"ListFont",NodeForm->ListBox->Font);
    NodeForm->Timer->Interval=IniFile->ReadInteger(section,"DisplayInterval"
        ,NodeForm->Timer->Interval/1000)*1000;
    NodeForm->ListBox->Color=StringToColor(IniFile->ReadString(section,"ListColor",clWindow));

    ImportFormSettings(IniFile,section="StatsForm",StatsForm);

    ImportFormSettings(IniFile,section="ClientForm",ClientForm);
    ImportFont(IniFile,section,"ListFont",ClientForm->ListView->Font);
    ClientForm->ListView->Color=StringToColor(IniFile->ReadString(section,"ListColor",clWindow));
    ClientForm->Timer->Interval=IniFile->ReadInteger(section,"DisplayInterval"
        ,ClientForm->Timer->Interval/1000)*1000;
    for(int i=0;i<ClientForm->ListView->Columns->Count;i++) {
        char str[128];
        sprintf(str,"Column%dWidth",i);
        if(IniFile->ValueExists(section,str))
            ClientForm->ListView->Columns->Items[i]->Width
                =IniFile->ReadInteger(section,str,0);
    }

    section = "SpyTerminal";
	SpyTerminalWidth=IniFile->ReadInteger(section, "Width", SpyTerminalWidth);
	SpyTerminalHeight=IniFile->ReadInteger(section, "Height", SpyTerminalHeight);
   	SpyTerminalFont->Name=IniFile->ReadString(section, "FontName", SpyTerminalFont->Name);
	SpyTerminalFont->Size=IniFile->ReadInteger(section, "FontSize", SpyTerminalFont->Size);
	SpyTerminalKeyboardActive=IniFile->ReadBool(section, "KeyboardActive", SpyTerminalKeyboardActive);

    delete IniFile;

    Application->MessageBox(AnsiString("Successfully imported SBBSCTRL settings from "
    	+ OpenDialog->FileName).c_str(),"Successful Import",MB_OK);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ExportSettings(TObject* Sender)
{
	char str[128];

    SaveDialog->Filter="Settings files (*.ini)|*.ini|All files|*.*";
    SaveDialog->FileName=AnsiString(global.ctrl_dir)+"sbbsctrl.ini";
    if(!SaveDialog->Execute())
    	return;

	TMemIniFile* IniFile=new TMemIniFile(SaveDialog->FileName);

	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Exporting Settings...";

    const char* section = "Properties";

    IniFile->WriteString(section,"LoginCommand",LoginCommand);
    IniFile->WriteString(section,"ConfigCommand",ConfigCommand);
    IniFile->WriteString(section,"Password",Password);
    IniFile->WriteBool(section,"MinimizeToSysTray",MinimizeToSysTray);
    IniFile->WriteBool(section,"UseFileAssociations",UseFileAssociations);
    IniFile->WriteBool(section,"UndockableForms",UndockableForms);

    ExportFormSettings(IniFile,section="MainForm",MainForm);
    IniFile->WriteInteger(section,"TopPanelHeight",TopPanel->Height);
 	IniFile->WriteInteger(section,"UpperLeftPageControlWidth",UpperLeftPageControl->Width);
    IniFile->WriteInteger(section,"LowerLeftPageControlWidth",LowerLeftPageControl->Width);
    IniFile->WriteBool(section,"ToolBarVisible",Toolbar->Visible);
    IniFile->WriteBool(section,"StatusBarVisible",StatusBar->Visible);

    ExportFormSettings(IniFile,section = "NodeForm",NodeForm);
    ExportFont(IniFile,section,"ListFont",NodeForm->ListBox->Font);
    IniFile->WriteString(section,"ListColor",ColorToString(NodeForm->ListBox->Color));
    IniFile->WriteInteger(section,"DisplayInterval",NodeForm->Timer->Interval/1000);

    ExportFormSettings(IniFile,section = "StatsForm",StatsForm);

    ExportFormSettings(IniFile,section = "ClientForm",ClientForm);
    ExportFont(IniFile,section,"ListFont",ClientForm->ListView->Font);
    IniFile->WriteString(section,"ListColor",ColorToString(ClientForm->ListView->Color));
    IniFile->WriteInteger(section,"DisplayInterval",ClientForm->Timer->Interval/1000);
    for(int i=0;i<ClientForm->ListView->Columns->Count;i++) {
        char str[128];
        sprintf(str,"Column%dWidth",i);
        IniFile->WriteInteger(section,str
            ,ClientForm->ListView->Columns->Items[i]->Width);
    }

    ExportFormSettings(IniFile,section = "TelnetForm",TelnetForm);
    ExportFont(IniFile,section,"LogFont",TelnetForm->Log->Font);
    IniFile->WriteString(section,"LogColor",ColorToString(TelnetForm->Log->Color));

    ExportFormSettings(IniFile,section = "EventsForm",EventsForm);
    ExportFont(IniFile,section,"LogFont",EventsForm->Log->Font);
    IniFile->WriteString(section,"LogColor",ColorToString(EventsForm->Log->Color));

    ExportFormSettings(IniFile,section = "ServicesForm",ServicesForm);
    ExportFont(IniFile,section,"LogFont",ServicesForm->Log->Font);
    IniFile->WriteString(section,"LogColor",ColorToString(ServicesForm->Log->Color));

    ExportFormSettings(IniFile,section = "FtpForm",FtpForm);
    ExportFont(IniFile,section,"LogFont",FtpForm->Log->Font);
    IniFile->WriteString(section,"LogColor",ColorToString(FtpForm->Log->Color));

    ExportFormSettings(IniFile,section = "MailForm",MailForm);
    ExportFont(IniFile,section,"LogFont",MailForm->Log->Font);
    IniFile->WriteString(section,"LogColor",ColorToString(MailForm->Log->Color));

    ExportFormSettings(IniFile,section = "WebForm",WebForm);
    ExportFont(IniFile,section,"LogFont",WebForm->Log->Font);
    IniFile->WriteString(section,"LogColor",ColorToString(WebForm->Log->Color));

    section = "SpyTerminal";
	IniFile->WriteInteger(section, "Width"
                            ,SpyTerminalWidth);
	IniFile->WriteInteger(section, "Height"
                            ,SpyTerminalHeight);
   	IniFile->WriteString(section,  "FontName"
                            ,SpyTerminalFont->Name);
	IniFile->WriteInteger(section, "FontSize"
                            ,SpyTerminalFont->Size);
	IniFile->WriteBool(section,    "KeyboardActive"
                            ,SpyTerminalKeyboardActive);

    IniFile->UpdateFile();

    delete IniFile;

    Application->MessageBox(AnsiString("Successfully exported SBBSCTRL settings to "
    	+ SaveDialog->FileName).c_str(),"Successful Export",MB_OK);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewStatusBarMenuItemClick(TObject *Sender)
{
	StatusBar->Visible=!StatusBar->Visible;
    ViewStatusBarMenuItem->Checked=StatusBar->Visible;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::HelpAboutMenuItemClick(TObject *Sender)
{
	Application->CreateForm(__classid(TAboutBoxForm), &AboutBoxForm);
    AboutBoxForm->ShowModal();
    delete AboutBoxForm;
}
//---------------------------------------------------------------------------
BOOL MuteService(SC_HANDLE svc, SERVICE_STATUS* status, BOOL mute)
{
	if(svc==NULL || controlService==NULL)
    	return(FALSE);

	return controlService(svc
        ,mute ? SERVICE_CONTROL_MUTE:SERVICE_CONTROL_UNMUTE, status);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SoundToggleExecute(TObject *Sender)
{
    SoundToggle->Checked=!SoundToggle->Checked;

    if(!SoundToggle->Checked) {
	    bbs_startup.options|=BBS_OPT_MUTE;
	    ftp_startup.options|=FTP_OPT_MUTE;
	    web_startup.options|=FTP_OPT_MUTE;
	    mail_startup.options|=MAIL_OPT_MUTE;
	    services_startup.options|=MAIL_OPT_MUTE;
	} else {
	    bbs_startup.options&=~BBS_OPT_MUTE;
	    ftp_startup.options&=~FTP_OPT_MUTE;
	    web_startup.options&=~FTP_OPT_MUTE;
	    mail_startup.options&=~MAIL_OPT_MUTE;
	    services_startup.options&=~MAIL_OPT_MUTE;
    }
    MuteService(bbs_svc,&bbs_svc_status,!SoundToggle->Checked);
    MuteService(ftp_svc,&ftp_svc_status,!SoundToggle->Checked);
    MuteService(web_svc,&web_svc_status,!SoundToggle->Checked);
    MuteService(mail_svc,&mail_svc_status,!SoundToggle->Checked);
    MuteService(services_svc,&services_svc_status,!SoundToggle->Checked);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BBSStatisticsLogMenuItemClick(TObject *Sender)
{
	StatsForm->LogButtonClick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ForceTimedEventMenuItemClick(TObject *Sender)
{
	int i,file;
	char str[MAX_PATH+1];
	static int selection;

	Application->CreateForm(__classid(TCodeInputForm), &CodeInputForm);
	CodeInputForm->Label->Caption="Event Internal Code";
    CodeInputForm->ComboBox->Items->Clear();
    for(i=0;i<cfg.total_events;i++)
    	CodeInputForm->ComboBox->Items->Add(
            AnsiString(cfg.event[i]->code).UpperCase());
    CodeInputForm->ComboBox->ItemIndex=selection;
    if(CodeInputForm->ShowModal()==mrOk
       	&& CodeInputForm->ComboBox->Text.Length()) {
        for(i=0;i<cfg.total_events;i++) {
			if(!stricmp(CodeInputForm->ComboBox->Text.c_str(),cfg.event[i]->code)) {
				sprintf(str,"%s%s.now",cfg.data_dir,cfg.event[i]->code);
            	if((file=_sopen(str,O_CREAT|O_TRUNC|O_WRONLY
	                ,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
	                close(file);
				selection = CodeInputForm->ComboBox->ItemIndex;
                break;
	   		}
        }
        if(i>=cfg.total_events)
	    	Application->MessageBox(CodeInputForm->ComboBox->Text.c_str()
                ,"Event Code Not Found",MB_OK|MB_ICONEXCLAMATION);
    }
    delete CodeInputForm;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ForceNetworkCalloutMenuItemClick(
      TObject *Sender)
{
	int i,file;
	char str[MAX_PATH+1];

	Application->CreateForm(__classid(TCodeInputForm), &CodeInputForm);
	CodeInputForm->Label->Caption="Hub QWK-ID";
    CodeInputForm->ComboBox->Items->Clear();
    for(i=0;i<cfg.total_qhubs;i++)
    	CodeInputForm->ComboBox->Items->Add(
            AnsiString(cfg.qhub[i]->id).UpperCase());
    CodeInputForm->ComboBox->ItemIndex=0;
    if(CodeInputForm->ShowModal()==mrOk
    	&& CodeInputForm->ComboBox->Text.Length()) {
        for(i=0;i<cfg.total_qhubs;i++) {
			if(!stricmp(CodeInputForm->ComboBox->Text.c_str(),cfg.qhub[i]->id)) {
				sprintf(str,"%sqnet/%s.now",cfg.data_dir,cfg.qhub[i]->id);
            	if((file=_sopen(str,O_CREAT|O_TRUNC|O_WRONLY
                	,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
	                close(file);
                break;
	   		}
        }
        if(i>=cfg.total_qhubs)
	    	Application->MessageBox(CodeInputForm->ComboBox->Text.c_str()
                ,"QWKnet Hub ID Not Found",MB_OK|MB_ICONEXCLAMATION);
    }
    delete CodeInputForm;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::TextMenuItemEditClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%s%s"
    	,MainForm->cfg.text_dir
        ,((TMenuItem*)Sender)->Hint.c_str());
    EditFile(filename,((TMenuItem*)Sender)->Caption);
}

//---------------------------------------------------------------------------
void __fastcall TMainForm::CtrlMenuItemEditClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    iniFileName(filename,sizeof(filename)
    	,MainForm->cfg.ctrl_dir
        ,((TMenuItem*)Sender)->Hint.c_str());
    EditFile(filename,((TMenuItem*)Sender)->Caption);
}
void __fastcall TMainForm::DataMenuItemClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%s%s"
    	,MainForm->cfg.data_dir
        ,((TMenuItem*)Sender)->Hint.c_str());
    EditFile(filename,((TMenuItem*)Sender)->Caption);
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

void __fastcall TMainForm::UpTimerTick(TObject *Sender)
{
	char    str[128];
    char    days[64];
    static  time_t start;
    ulong   up;
    static  bool sysop_available;

    if(ChatToggle->Checked != sysop_available) {
        sysop_available = ChatToggle->Checked;
        set_sysop_availability(&cfg, sysop_available);
    }
	
	if(clearLoginAttemptList) {
		loginAttemptListClear(&login_attempt_list);
		clearLoginAttemptList = false;
	}

    if(!start)
        start=time(NULL);
    up=time(NULL)-start;

    days[0]=0;
    if((up/(24*60*60))>=2) {
        sprintf(days,"%u days ",up/(24*60*60));
        up%=(24*60*60);
    }
		
	for(int i = 0; i <= STATUSBAR_LAST_PANEL; i++) {
		switch(i) {
			case 0:
				sprintf(str,"Threads: %u",threads);
				break;
			case 1:
				sprintf(str,"Sockets: %u",sockets);
				break;
			case 2:
				sprintf(str,"Clients: %u",clients);
				break;
			case 3:
			    sprintf(str,"Served: %u",total_clients);
				break;
			case 4:
				sprintf(str,"Failed: %u",loginAttemptListCount(&login_attempt_list));
				break;
			case 5:
				sprintf(str,"Errors: %u",errors);
				break;
			default:
				sprintf(str,"Up: %s%u:%02u"
					,days
					,up/(60*60)
					,(up/60)%60
					);
		}
		TStatusPanel* panel = MainForm->StatusBar->Panels->Items[i];	
		
		AnsiString Str = AnsiString(str);
		if(panel->Text != Str) {
			panel->Text = Str;
//			panel->Bevel = pbRaised;
		} else {
//			panel->Bevel = pbLowered;
		}
	}
	
    if(TrayIcon->Visible) {
        /* Animate TrayIcon when in use */
        AnsiString NumClients;
        try {
            if(clients) {
                TrayIcon->IconIndex=(TrayIcon->IconIndex==4) ? 59 : 4;
                NumClients=" ("+AnsiString(clients)+" client";
                if(clients>1)
                    NumClients+="s";
                NumClients+=")";
            } else if(TrayIcon->IconIndex!=4)
                TrayIcon->IconIndex=4;
            TrayIcon->Hint=AnsiString(APP_TITLE)+NumClients;
        } catch(...) { /* ignore exceptions here */ };
    }
}
//---------------------------------------------------------------------------


void __fastcall TMainForm::ChatToggleExecute(TObject *Sender)
{
    ChatToggle->Checked=!ChatToggle->Checked;
}

//---------------------------------------------------------------------------
void __fastcall TMainForm::UserEditExecute(TObject *Sender)
{
    char str[256];

    sprintf(str,"%sUSEREDIT %s",cfg.exec_dir,cfg.data_dir);
    WinExec(str,SW_SHOWNORMAL);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::BBSPreviewMenuItemClick(TObject *Sender)
{
    TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

    dlg->Options << ofNoChangeDir;
    dlg->Filter = "ANSI/Ctrl-A files (*.asc; *.msg; *.ans)|*.ASC;*.MSG;*.ANS"
                "|All files|*.*";
    dlg->InitialDir=cfg.text_dir;
    if(dlg->Execute()==true) {
        Application->CreateForm(__classid(TPreviewForm), &PreviewForm);
        PreviewForm->Filename=AnsiString(dlg->FileName);
        PreviewForm->ShowModal();
        delete PreviewForm;
    }
    delete dlg;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BBSLoginMenuItemClick(TObject *Sender)
{
    if(!strnicmp(LoginCommand.c_str(),"start ",6)) /* Doesn't work on NT */
        ShellExecute(Handle, "open", LoginCommand.c_str()+6,
            NULL,NULL,SW_SHOWDEFAULT);
    else if(!strnicmp(LoginCommand.c_str(),"telnet:",7))
        ShellExecute(Handle, "open", LoginCommand.c_str(),
            NULL,NULL,SW_SHOWDEFAULT);
    else
        WinExec(LoginCommand.c_str(),SW_SHOWNORMAL);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewLogClick(TObject *Sender)
{
    char    str[128];
	char    filename[MAX_PATH+1];
    struct  tm* tm;
    time_t  t;
    TModalResult mr;

    if(((TMenuItem*)Sender)->Tag==-1) {
    	Application->CreateForm(__classid(TCodeInputForm), &CodeInputForm);
    	CodeInputForm->Label->Caption="Date";
        CodeInputForm->ComboBox->Items->Clear();
       	CodeInputForm->ComboBox->Text=AnsiString(unixtodstr(&cfg,time(NULL),str));
        mr=CodeInputForm->ShowModal();
        t=dstrtounix(&cfg,CodeInputForm->ComboBox->Text.c_str());
        delete CodeInputForm;
        if(mr!=mrOk)
            return;
    } else {
        t=time(NULL);
        t-=((TMenuItem*)Sender)->Tag*24*60*60;
    }
    tm=localtime(&t);
    if(tm==NULL)
        return;

    /* Close Mail/FTP logs */
    mail_log_msg(NULL);
    ftp_log_msg(NULL);

    if(strchr(((TMenuItem*)Sender)->Hint.c_str(),'.')==NULL)
        sprintf(filename,"%sLOGS\\%s%02d%02d%02d.LOG"
            ,MainForm->cfg.logs_dir
            ,((TMenuItem*)Sender)->Hint.c_str()
            ,tm->tm_mon+1
            ,tm->tm_mday
            ,tm->tm_year%100
            );
    else
        sprintf(filename,"%s\\%s"
            ,MainForm->cfg.logs_dir
            ,((TMenuItem*)Sender)->Hint.c_str()
        );
    ViewFile(filename,((TMenuItem*)Sender)->Caption);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::RunJSClick(TObject *Sender)
{
	char    cmdline[MAX_PATH+1];

    SAFEPRINTF2(cmdline,"%sjsexec.exe -p %s"
        ,MainForm->cfg.exec_dir
        ,((TMenuItem*)Sender)->Hint.c_str()
        );
    STARTUPINFO startup_info={0};
    PROCESS_INFORMATION process_info;
    startup_info.cb=sizeof(startup_info);
    startup_info.lpTitle = cmdline;
	if(!CreateProcess(
		NULL,			// pointer to name of executable module
		cmdline,        // pointer to command line string
		NULL,  			// process security attributes
		NULL,   		// thread security attributes
		FALSE, 			// handle inheritance flag
		0,              // creation flags
        NULL,  			// pointer to new environment block
		cfg.ctrl_dir,	// pointer to current directory name
		&startup_info,  // pointer to STARTUPINFO
		&process_info  	// pointer to PROCESS_INFORMATION
		))
       	Application->MessageBox(AnsiString("ERROR " + IntToStr(GetLastError()) +
            " executing " + cmdline).c_str()
            ,"ERROR"
            ,MB_OK|MB_ICONEXCLAMATION);
	// Resource leak if you don't close these:
	CloseHandle(process_info.hThread);
	CloseHandle(process_info.hProcess);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::UserListExecute(TObject *Sender)
{
    UserListForm->Show();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::WebPageMenuItemClick(TObject *Sender)
{
    ShellExecute(Handle, "open", ((TMenuItem*)Sender)->Hint.c_str(),
        NULL,NULL,SW_SHOWDEFAULT);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormMinimize(TObject *Sender)
{
    if(MinimizeToSysTray) {
    	if(Password.Length()) {
        	TrayIcon->RestoreOn=imNone;
            CloseTrayMenuItem->Enabled=false;
            ConfigureTrayMenuItem->Enabled=false;
            UserEditTrayMenuItem->Enabled=false;
        } else {
        	TrayIcon->RestoreOn=imLeftClickUp;
            CloseTrayMenuItem->Enabled=true;
            ConfigureTrayMenuItem->Enabled=true;
            UserEditTrayMenuItem->Enabled=true;
        }
        if(!TrayIcon->Visible)
	        TrayIcon->Visible=true;
   	    TrayIcon->Minimize();
    }
}

void __fastcall TMainForm::TrayIconRestore(TObject *Sender)
{
    TrayIcon->Visible=false;
}

//---------------------------------------------------------------------------

void __fastcall TMainForm::PropertiesExecute(TObject *Sender)
{
    char str[128];
    static inside;
    if(inside) return;
    inside=true;

    Application->CreateForm(__classid(TPropertiesDlg), &PropertiesDlg);
    PropertiesDlg->LoginCmdEdit->Text=LoginCommand;
    PropertiesDlg->ConfigCmdEdit->Text=ConfigCommand;
    PropertiesDlg->HostnameEdit->Text=global.host_name;
    PropertiesDlg->CtrlDirEdit->Text=global.ctrl_dir;
    PropertiesDlg->TempDirEdit->Text=global.temp_dir;
    PropertiesDlg->NodeIntUpDown->Position=NodeForm->Timer->Interval/1000;
    PropertiesDlg->ClientIntUpDown->Position=ClientForm->Timer->Interval/1000;
    PropertiesDlg->SemFreqUpDown->Position=global.sem_chk_freq;
    PropertiesDlg->TrayIconCheckBox->Checked=MinimizeToSysTray;
    PropertiesDlg->UndockableCheckBox->Checked=UndockableForms;
    PropertiesDlg->FileAssociationsCheckBox->Checked=UseFileAssociations;
    PropertiesDlg->PasswordEdit->Text=Password;
    PropertiesDlg->JS_MaxBytesEdit->Text=byte_count_to_str(global.js.max_bytes, str, sizeof(str));
    PropertiesDlg->JS_ContextStackEdit->Text=byte_count_to_str(global.js.cx_stack, str, sizeof(str));
    PropertiesDlg->JS_TimeLimitEdit->Text=IntToStr(global.js.time_limit);
    PropertiesDlg->JS_GcIntervalEdit->Text=IntToStr(global.js.gc_interval);
    PropertiesDlg->JS_YieldIntervalEdit->Text=IntToStr(global.js.yield_interval);
    PropertiesDlg->JS_LoadPathEdit->Text=global.js.load_path;
    PropertiesDlg->ErrorSoundEdit->Text=ErrorSoundFile;
    PropertiesDlg->LoginAttemptDelayEdit->Text=IntToStr(global.login_attempt.delay);
    PropertiesDlg->LoginAttemptThrottleEdit->Text=IntToStr(global.login_attempt.throttle);
    PropertiesDlg->LoginAttemptHackThresholdEdit->Text
        =global.login_attempt.hack_threshold ? IntToStr(global.login_attempt.hack_threshold) : AnsiString("<disabled>");
    PropertiesDlg->LoginAttemptFilterThresholdEdit->Text
        =global.login_attempt.filter_threshold ? IntToStr(global.login_attempt.filter_threshold) : AnsiString("<disabled>");
    PropertiesDlg->LoginAttemptTempBanThresholdEdit->Text
        =global.login_attempt.tempban_threshold ? IntToStr(global.login_attempt.tempban_threshold) : AnsiString("<disabled>");
    PropertiesDlg->LoginAttemptTempBanDurationEdit->Text
        =global.login_attempt.tempban_duration ? AnsiString(duration_to_str(global.login_attempt.tempban_duration, str, sizeof(str)))
            : AnsiString("<disabled>");

    if(MaxLogLen==0)
		PropertiesDlg->MaxLogLenEdit->Text="<unlimited>";
    else
	    PropertiesDlg->MaxLogLenEdit->Text=IntToStr(MaxLogLen);
	if(PropertiesDlg->ShowModal()==mrOk) {
        LoginCommand=PropertiesDlg->LoginCmdEdit->Text;
        ConfigCommand=PropertiesDlg->ConfigCmdEdit->Text;
        SAFECOPY(global.host_name,PropertiesDlg->HostnameEdit->Text.c_str());
        SAFECOPY(global.ctrl_dir,PropertiesDlg->CtrlDirEdit->Text.c_str());
        SAFECOPY(global.temp_dir,PropertiesDlg->TempDirEdit->Text.c_str());
        global.sem_chk_freq=PropertiesDlg->SemFreqUpDown->Position;
        SemFileTimer->Interval=global.sem_chk_freq;

        /* Copy global values to server startup structs */
        /* We don't support per-server unique values here (yet) */
        SAFECOPY(bbs_startup.host_name,global.host_name);
        SAFECOPY(bbs_startup.ctrl_dir,global.ctrl_dir);
        SAFECOPY(bbs_startup.temp_dir,global.temp_dir);
        bbs_startup.sem_chk_freq=global.sem_chk_freq;

        SAFECOPY(ftp_startup.host_name,global.host_name);
        SAFECOPY(ftp_startup.ctrl_dir,global.ctrl_dir);
        SAFECOPY(ftp_startup.temp_dir,global.temp_dir);
        ftp_startup.sem_chk_freq=global.sem_chk_freq;

        SAFECOPY(web_startup.host_name,global.host_name);
        SAFECOPY(web_startup.ctrl_dir,global.ctrl_dir);
        SAFECOPY(web_startup.temp_dir,global.temp_dir);
        web_startup.sem_chk_freq=global.sem_chk_freq;

        SAFECOPY(mail_startup.host_name,global.host_name);
        SAFECOPY(mail_startup.ctrl_dir,global.ctrl_dir);
        SAFECOPY(mail_startup.temp_dir,global.temp_dir);
        mail_startup.sem_chk_freq=global.sem_chk_freq;

        SAFECOPY(services_startup.host_name,global.host_name);
        SAFECOPY(services_startup.ctrl_dir,global.ctrl_dir);
        SAFECOPY(services_startup.temp_dir,global.temp_dir);
        services_startup.sem_chk_freq=global.sem_chk_freq ;

        Password=PropertiesDlg->PasswordEdit->Text;
        NodeForm->Timer->Interval=PropertiesDlg->NodeIntUpDown->Position*1000;
        ClientForm->Timer->Interval=PropertiesDlg->ClientIntUpDown->Position*1000;
        MinimizeToSysTray=PropertiesDlg->TrayIconCheckBox->Checked;
        UndockableForms=PropertiesDlg->UndockableCheckBox->Checked;
        UseFileAssociations=PropertiesDlg->FileAssociationsCheckBox->Checked;
        ErrorSoundFile=PropertiesDlg->ErrorSoundEdit->Text;

        /* JavaScript operating parameters */
        js_startup_t js=global.js; // save for later comparison
        global.js.max_bytes
        	=parse_byte_count(PropertiesDlg->JS_MaxBytesEdit->Text.c_str(), 1);
        global.js.cx_stack
        	=parse_byte_count(PropertiesDlg->JS_ContextStackEdit->Text.c_str(), 1);
        global.js.time_limit
        	=PropertiesDlg->JS_TimeLimitEdit->Text.ToIntDef(JAVASCRIPT_TIME_LIMIT);
        global.js.gc_interval
        	=PropertiesDlg->JS_GcIntervalEdit->Text.ToIntDef(JAVASCRIPT_GC_INTERVAL);
        global.js.yield_interval
        	=PropertiesDlg->JS_YieldIntervalEdit->Text.ToIntDef(JAVASCRIPT_YIELD_INTERVAL);
        SAFECOPY(global.js.load_path, PropertiesDlg->JS_LoadPathEdit->Text.c_str());

        /* Copy global settings, if appropriate (not unique) */
        if(memcmp(&bbs_startup.js,&js,sizeof(js))==0)       bbs_startup.js=global.js;
        if(memcmp(&ftp_startup.js,&js,sizeof(js))==0)       ftp_startup.js=global.js;
        if(memcmp(&web_startup.js,&js,sizeof(js))==0)       web_startup.js=global.js;
        if(memcmp(&mail_startup.js,&js,sizeof(js))==0)      mail_startup.js=global.js;
        if(memcmp(&services_startup.js,&js,sizeof(js))==0)  services_startup.js=global.js;

        /* Security parameters */
        global.login_attempt.delay = PropertiesDlg->LoginAttemptDelayEdit->Text.ToIntDef(0);
        global.login_attempt.throttle = PropertiesDlg->LoginAttemptThrottleEdit->Text.ToIntDef(0);
        global.login_attempt.hack_threshold = PropertiesDlg->LoginAttemptHackThresholdEdit->Text.ToIntDef(0);
        global.login_attempt.filter_threshold = PropertiesDlg->LoginAttemptFilterThresholdEdit->Text.ToIntDef(0);
        global.login_attempt.tempban_threshold = PropertiesDlg->LoginAttemptTempBanThresholdEdit->Text.ToIntDef(0);
        global.login_attempt.tempban_duration = parse_duration(PropertiesDlg->LoginAttemptTempBanDurationEdit->Text.c_str());

        MaxLogLen
        	=PropertiesDlg->MaxLogLenEdit->Text.ToIntDef(0);
        SaveSettings(Sender);
    }
    delete PropertiesDlg;

    inside=false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CloseTrayMenuItemClick(TObject *Sender)
{
    Close();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::RestoreTrayMenuItemClick(TObject *Sender)
{
#if 0
    TrayIcon->Visible=false;
    Application->Restore();
#else
	static inside;
    if(inside)
    	return;
    inside=true;
    
	if(Password.Length()) {
    	Application->CreateForm(__classid(TCodeInputForm), &CodeInputForm);
    	CodeInputForm->Label->Caption="Password";
        CodeInputForm->Edit->Visible=true;
        CodeInputForm->Edit->PasswordChar='*';
	    if(CodeInputForm->ShowModal()==mrOk
        	&& CodeInputForm->Edit->Text.AnsiCompareIC(Password)==0)
            TrayIcon->Restore();
        delete CodeInputForm;
    } else
        TrayIcon->Restore();

    inside=false;
#endif
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BBSConfigWizardMenuItemClick(TObject *Sender)
{
    TConfigWizard* ConfigWizard;
    static inside;
    if(inside) return;
    inside=true;

    Application->CreateForm(__classid(TConfigWizard), &ConfigWizard);
	if(ConfigWizard->ShowModal()==mrOk) {
        SaveSettings(Sender);
//        ReloadConfigExecute(Sender);  /* unnecessary since refresh_cfg() is already called */
    }
    delete ConfigWizard;

    inside=false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::PageControlUnDock(TObject *Sender,
      TControl *Client, TWinControl *NewTarget, bool &Allow)
{
    if(NewTarget==NULL) /* Desktop */
        Allow=UndockableForms;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::reload_config(void)
{
	char error[256];
	SAFECOPY(error,UNKNOWN_LOAD_ERROR);
   	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Reloading configuration...";
	if(!load_cfg(&cfg, NULL, TRUE, error)) {
    	Application->MessageBox(error,"ERROR Re-loading Configuration"
	        ,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
    }
    FILE* fp=fopen(MainForm->ini_file,"r");
    sbbs_read_ini(fp, MainForm->ini_file
        ,&MainForm->global
        ,NULL   ,&MainForm->bbs_startup
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,NULL
        ,NULL   ,NULL
        );
    if(fp!=NULL)
        fclose(fp);
   	StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text="Configuration reloaded";
   	semfile_list_check(&initialized,recycle_semfiles);

    if(sysop_available(&cfg))
    	ChatToggle->Checked=true;
    else
    	ChatToggle->Checked=false;
        
    if(bbs_startup.options&BBS_OPT_MUTE)
    	SoundToggle->Checked=false;
    else
    	SoundToggle->Checked=true;
	SetControls();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ReloadConfigExecute(TObject *Sender)
{
	FtpRecycleExecute(Sender);
	WebRecycleExecute(Sender);
	MailRecycleExecute(Sender);
    TelnetRecycleExecute(Sender);
	ServicesRecycleExecute(Sender);

    reload_config();
#if 0   /* This appears to be redundant */
    node_t node;
    for(int i=0;i<cfg.sys_nodes;i++) {
    	int file;
       	if(NodeForm->getnodedat(i+1,&node,&file))
            break;
        node.misc|=NODE_RRUN;
        if(NodeForm->putnodedat(i+1,&node,file))
            break;
    }
#endif
}

//---------------------------------------------------------------------------


void __fastcall TMainForm::ServicesConfigureExecute(TObject *Sender)
{
    static inside;
    if(inside) return;
    inside=true;

	Application->CreateForm(__classid(TServicesCfgDlg), &ServicesCfgDlg);
	ServicesCfgDlg->ShowModal();
    delete ServicesCfgDlg;

    inside=false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::UserTruncateMenuItemClick(TObject *Sender)
{
    int usernumber;
    int deleted=0;
    user_t user;

    Screen->Cursor=crHourGlass;
    while((user.number=lastuser(&cfg))!=0) {
        if(getuserdat(&cfg,&user)!=0)
            break;
        if(!(user.misc&DELETED))
            break;
        if(!del_lastuser(&cfg))
            break;
        deleted++;
    }
    Screen->Cursor=crDefault;
    char str[128];
    sprintf(str,"%u Deleted User Records Removed",deleted);
   	Application->MessageBox(str,"Users Truncated",MB_OK);
}

BOOL RecycleService(SC_HANDLE svc, SERVICE_STATUS* status)
{
	if(svc==NULL || controlService==NULL)
    	return(FALSE);

	return controlService(svc, SERVICE_CONTROL_RECYCLE, status);
}

//---------------------------------------------------------------------------
void __fastcall TMainForm::MailRecycleExecute(TObject *Sender)
{
	if(!RecycleService(mail_svc,&mail_svc_status)) {
		mail_startup.recycle_now=true;
	    MailRecycle->Enabled=false;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FtpRecycleExecute(TObject *Sender)
{
	if(!RecycleService(ftp_svc,&ftp_svc_status)) {
		ftp_startup.recycle_now=true;
	    FtpRecycle->Enabled=false;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::WebRecycleExecute(TObject *Sender)
{
	if(!RecycleService(web_svc,&web_svc_status)) {
		web_startup.recycle_now=true;
	    WebRecycle->Enabled=false;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ServicesRecycleExecute(TObject *Sender)
{
	if(!RecycleService(services_svc,&services_svc_status)) {
		services_startup.recycle_now=true;
	    ServicesRecycle->Enabled=false;
    }
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::TelnetRecycleExecute(TObject *Sender)
{
    if(!RecycleService(bbs_svc,&bbs_svc_status)) {
		bbs_startup.recycle_now=true;
	    TelnetRecycle->Enabled=false;
    }
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FileEditTextFilesClick(TObject *Sender)
{
	TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

    dlg->Options << ofNoChangeDir;
    dlg->Filter = 	"Text files (*.txt)|*.TXT"
    				"|All files|*.*";
    dlg->InitialDir=cfg.text_dir;
    if(dlg->Execute()==true)
        EditFile(dlg->FileName.c_str());
    delete dlg;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::BBSEditBajaMenuItemClick(TObject *Sender)
{
	TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

    dlg->Options << ofNoChangeDir;
    dlg->Filter = "Baja Source Code (*.src)|*.SRC";
    dlg->InitialDir=cfg.exec_dir;
    if(dlg->Execute()==true) {
        Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
        TextFileEditForm->Filename=AnsiString(dlg->FileName);
        TextFileEditForm->Caption="Edit";
        if(TextFileEditForm->ShowModal()==mrOk) {
        	/* Compile Baja Source File (requires Baja v2.33+) */
        	char cmdline[512];
            sprintf(cmdline,"%sbaja -p %s",cfg.exec_dir,dlg->FileName);
            WinExec(cmdline,SW_SHOW);
		}
        delete TextFileEditForm;
    }
    delete dlg;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FileEditJavaScriptClick(TObject *Sender)
{
	TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

    dlg->Options << ofNoChangeDir;
    dlg->Filter = "JavaScript Files (*.js)|*.JS";
    dlg->InitialDir=cfg.exec_dir;
    if(dlg->Execute()==true)
        EditFile(dlg->FileName.c_str());
    delete dlg;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FileEditConfigFilesClick(TObject *Sender)
{
	TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

    dlg->Options << ofNoChangeDir;
    dlg->Filter = "Configuration Files (*.cfg; *.ini; *.conf)|*.cfg;*.ini;*.conf";
    dlg->InitialDir=cfg.ctrl_dir;
    if(dlg->Execute()==true)
        EditFile(dlg->FileName.c_str());
    delete dlg;
}

void __fastcall TMainForm::BBSEditFileClick(TObject *Sender)
{
   TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

   dlg->Options << ofNoChangeDir;
    dlg->Filter = "ANSI/Ctrl-A files (*.asc; *.msg; *.ans)|*.ASC;*.MSG;*.ANS"
                "|All files|*.*";
    dlg->InitialDir=cfg.text_dir;
    if(dlg->Execute()==true) {
        Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
        TextFileEditForm->Filename=AnsiString(dlg->FileName);
        TextFileEditForm->Caption="Edit";
        if(TextFileEditForm->ShowModal()==mrOk) {
	        Application->CreateForm(__classid(TPreviewForm), &PreviewForm);
    	    PreviewForm->Filename=AnsiString(dlg->FileName);
            PreviewForm->ShowModal();
	        delete PreviewForm;
        }
        delete TextFileEditForm;
    }
    delete dlg;
}
//---------------------------------------------------------------------------
bool GetServerLogLine(HANDLE& log, const char* name, log_msg_t* msg)
{
	char fname[256];

	if(log==INVALID_HANDLE_VALUE) {
		sprintf(fname,"\\\\.\\mailslot\\%s.log",name);
        log = CreateMailslot(
            fname,					// pointer to string for mailslot name
            0,						// maximum message size
            0,						// milliseconds before read time-out
            NULL);                  // pointer to security structure
        if(log==INVALID_HANDLE_VALUE)
        	return(false);
    }
    DWORD msgs=0;
    if(!GetMailslotInfo(
        log,   // mailslot handle
        NULL,  // address of maximum message size
        NULL,  // address of size of next message
        &msgs, // address of number of messages
        NULL   // address of read time-out
        ) || !msgs)
        return(false);

	memset(msg, 0, sizeof(*msg));
    DWORD rd=0;
    if(!ReadFile(
        log,				// handle of file to read
        msg,		        // pointer to buffer that receives data
        sizeof(*msg),	   	// number of bytes to read
        &rd,				// pointer to number of bytes read
        NULL				// pointer to structure for data
        ) || !rd)
		return(false);

	return(true);
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::LogTimerTick(TObject *Sender)
{
    log_msg_t   msg;
    log_msg_t*  pmsg;

    if(!TelnetPause->Checked) {
        while(GetServerLogLine(bbs_log,NTSVC_NAME_BBS,&msg))
            bbs_log_msg(&msg);

        while(GetServerLogLine(event_log,NTSVC_NAME_EVENT,&msg))
            event_log_msg(&msg);

        while((pmsg=(log_msg_t*)listShiftNode(&bbs_log_list)) != NULL) {
            bbs_log_msg(pmsg);
            free(pmsg);
        }

        while((pmsg=(log_msg_t*)listShiftNode(&event_log_list)) != NULL) {
            event_log_msg(pmsg);
            free(pmsg);
        }
    }

    if(!FtpPause->Checked) {
        while(GetServerLogLine(ftp_log,NTSVC_NAME_FTP,&msg))
            ftp_log_msg(&msg);

        while((pmsg=(log_msg_t*)listShiftNode(&ftp_log_list)) != NULL) {
            ftp_log_msg(pmsg);
            free(pmsg);
        }
    }
    if(!MailPause->Checked) {
        while(GetServerLogLine(mail_log,NTSVC_NAME_MAIL,&msg))
            mail_log_msg(&msg);

        while((pmsg=(log_msg_t*)listShiftNode(&mail_log_list)) != NULL) {
            mail_log_msg(pmsg);
            free(pmsg);
        }
    }
    if(!WebPause->Checked) {
        while(GetServerLogLine(web_log,NTSVC_NAME_WEB,&msg))
            web_log_msg(&msg);

        while((pmsg=(log_msg_t*)listShiftNode(&web_log_list)) != NULL) {
            web_log_msg(pmsg);
            free(pmsg);
        }
    }
    if(!ServicesPause->Checked) {
        while(GetServerLogLine(services_log,NTSVC_NAME_SERVICES,&msg))
            services_log_msg(&msg);

        while((pmsg=(log_msg_t*)listShiftNode(&services_log_list)) != NULL) {
            services_log_msg(pmsg);
            free(pmsg);
        }
    }
}
//---------------------------------------------------------------------------
void CheckServiceStatus(
	 SC_HANDLE svc
	,SERVICE_STATUS* status
	,QUERY_SERVICE_CONFIG* &config
	,DWORD &config_size
	,TStaticText* text
    ,TAction* start
    ,TAction* stop
    ,TAction* recycle
    ,TProgressBar* bar
    )
{
	if(svc==NULL)
    	return;

	DWORD ret;

	if(!queryServiceConfig(svc,config,config_size,&ret)) {
		if(GetLastError()==ERROR_INSUFFICIENT_BUFFER) {
			config_size=ret;
			if(config!=NULL)
				free(config);
			config = (QUERY_SERVICE_CONFIG*)malloc(config_size);
		}
		return;
	}

	if(config->dwStartType==SERVICE_DISABLED)
		return;

	if(!queryServiceStatus(svc,status)) {
    	text->Caption="QueryServiceStatus Error "; // + GetLastError());
        return;
	}

    bool running=false;

	switch(status->dwCurrentState) {
    	case SERVICE_STOPPED:
        	text->Caption="Stopped NT Service";
            break;
        case SERVICE_STOP_PENDING:
        	text->Caption="Stopping NT Service";
            break;
        case SERVICE_RUNNING:
        	text->Caption="Running NT Service";
            running=true;
            break;
        case SERVICE_START_PENDING:
        	text->Caption="Starting NT Service";
            running=true;
            break;
        default:
        	text->Caption="!UNKNOWN: " +  status->dwCurrentState;
            break;
    }

    start->Enabled=!running;
    stop->Enabled=running;
    recycle->Enabled=running;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ServiceStatusTimerTick(TObject *Sender)
{
	if(queryServiceStatus==NULL || queryServiceConfig==NULL
    	|| (bbs_svc==NULL
        && ftp_svc==NULL
		&& web_svc==NULL
        && mail_svc==NULL
        && services_svc==NULL)) {
    	ServiceStatusTimer->Enabled=false;
        return;
    }

    CheckServiceStatus(
    	 bbs_svc
    	,&bbs_svc_status
		,bbs_svc_config
		,bbs_svc_config_size
        ,TelnetForm->Status
        ,TelnetStart
        ,TelnetStop
        ,TelnetRecycle
        ,TelnetForm->ProgressBar
        );
    CheckServiceStatus(
    	 ftp_svc
    	,&ftp_svc_status
		,ftp_svc_config
		,ftp_svc_config_size
        ,FtpForm->Status
        ,FtpStart
        ,FtpStop
        ,FtpRecycle
        ,FtpForm->ProgressBar
        );
    CheckServiceStatus(
    	 web_svc
    	,&web_svc_status
		,web_svc_config
		,web_svc_config_size
        ,WebForm->Status
        ,WebStart
        ,WebStop
        ,WebRecycle
        ,WebForm->ProgressBar
        );
    CheckServiceStatus(
    	 mail_svc
    	,&mail_svc_status
		,mail_svc_config
		,mail_svc_config_size
        ,MailForm->Status
        ,MailStart
        ,MailStop
        ,MailRecycle
        ,MailForm->ProgressBar
        );
    CheckServiceStatus(
    	 services_svc
    	,&services_svc_status
		,services_svc_config
		,services_svc_config_size
        ,ServicesForm->Status
        ,ServicesStart
        ,ServicesStop
        ,ServicesRecycle
        ,NULL
        );

}
//---------------------------------------------------------------------------
void __fastcall TMainForm::EditFile(AnsiString filename, AnsiString Caption)
{
   if(!UseFileAssociations
        || (int)ShellExecute(Handle, "edit", filename.c_str(), NULL,NULL,SW_SHOWDEFAULT)<=32) {
        Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
        TextFileEditForm->Filename=filename;
        TextFileEditForm->Caption=Caption;
        TextFileEditForm->ShowModal();
        delete TextFileEditForm;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewFile(AnsiString filename, AnsiString Caption)
{
   if(!UseFileAssociations
        || (int)ShellExecute(Handle, "open", filename.c_str(), NULL,NULL,SW_SHOWDEFAULT)<=32) {
        Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
        TextFileEditForm->Filename=filename;
        TextFileEditForm->Caption=Caption;
        TextFileEditForm->Memo->ReadOnly=true;
        TextFileEditForm->ShowModal();
        delete TextFileEditForm;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SemFileTimerTick(TObject *Sender)
{
    char* p;

    if((p=semfile_list_check(&initialized,shutdown_semfiles))!=NULL) {
	    StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text=AnsiString(p) + " signaled";
        terminating=true;
        Close();
    }
    else if((p=semfile_list_check(&initialized,recycle_semfiles))!=NULL) {
	    StatusBar->Panels->Items[STATUSBAR_LAST_PANEL]->Text=AnsiString(p) + " signaled";
        reload_config();
    }
}
//---------------------------------------------------------------------------
TFont* __fastcall TMainForm::LogAttributes(int log_level, TColor Color, TFont* Font)
{
    if(log_level==LOG_INFO || LogFont[log_level]->Color==Color
        || log_level > LOG_DEBUG)
        return Font;

    return LogFont[log_level];
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ClearErrorsExecute(TObject *Sender)
{
    errors=0;

    node_t node;
    for(int i=0;i<cfg.sys_nodes;i++) {
    	int file;
       	if(NodeForm->getnodedat(i+1,&node, /*lockit: */true))
            break;
        node.errors=0;
        if(NodeForm->putnodedat(i+1,&node))
            break;
    }
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewErrorLogExecute(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%sERROR.LOG"
    	,MainForm->cfg.logs_dir);
    ViewFile(filename,"Error Log");
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewLoginAttemptsMenuItemClick(TObject *Sender)
{
    LoginAttemptsForm->Show();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::LogPopupPauseClick(TObject *Sender)
{
    if(/*(TRichEdit*)*/Sender == TelnetForm->Log) {
        TelnetPause->Execute();
    }
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::LogPopupCopyAllClick(TObject *Sender)
{
    TRichEdit* Log = (TRichEdit*)LogPopupMenu->PopupComponent;
    Log->SelectAll();
    Log->CopyToClipboard();
    Log->SelLength=0;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::LogPopupCopyClick(TObject *Sender)
{
    TRichEdit* Log = (TRichEdit*)LogPopupMenu->PopupComponent;
    Log->CopyToClipboard();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ClearFailedLoginsPopupMenuItemClick(
      TObject *Sender)
{
    clearLoginAttemptList = true;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::RefreshLogClick(TObject *Sender)
{
    TRichEdit* Log = (TRichEdit*)LogPopupMenu->PopupComponent;
    Log->Refresh();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FidonetConfigureMenuItemClick(TObject *Sender)
{
	char str[MAX_PATH + 1];

    SAFEPRINTF(str, "%sechocfg.exe", cfg.exec_dir);
    STARTUPINFO startup_info={0};
    PROCESS_INFORMATION process_info;
    startup_info.cb=sizeof(startup_info);
    startup_info.lpTitle="Fidonet Configuration";
	if(!CreateProcess(
		NULL,			// pointer to name of executable module
		str,  			// pointer to command line string
		NULL,  			// process security attributes
		NULL,   		// thread security attributes
		FALSE, 			// handle inheritance flag
		0,              // creation flags
        NULL,  			// pointer to new environment block
		cfg.ctrl_dir,	// pointer to current directory name
		&startup_info,  // pointer to STARTUPINFO
		&process_info  	// pointer to PROCESS_INFORMATION
		))
       	Application->MessageBox(AnsiString("ERROR " + IntToStr(GetLastError()) +
            " executing " + str).c_str()
            ,"ERROR"
            ,MB_OK|MB_ICONEXCLAMATION);
	// Resource leak if you don't close these:
	CloseHandle(process_info.hThread);
	CloseHandle(process_info.hProcess);
}
//---------------------------------------------------------------------------



void __fastcall TMainForm::FidonetPollMenuItemClick(TObject *Sender)
{
	char path[MAX_PATH + 1];

    SAFEPRINTF(path, "%sbinkpoll.now", cfg.data_dir);
    int file=_sopen(path,O_CREAT|O_TRUNC|O_WRONLY
	                ,SH_DENYRW,S_IREAD|S_IWRITE);
    if (file!=-1)
        close(file);
}
//---------------------------------------------------------------------------


void __fastcall TMainForm::FileRunJSMenuItemClick(TObject *Sender)
{
	TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

    dlg->Options << ofNoChangeDir;
    dlg->Filter = 	"JavaScript files (*.js)|*.js";
    dlg->InitialDir=cfg.exec_dir;
    if(dlg->Execute()==true) {
    	char    cmdline[MAX_PATH+1];
        SAFEPRINTF2(cmdline,"%sjsexec.exe -p %s"
            ,MainForm->cfg.exec_dir
            ,dlg->FileName.c_str()
            );
        STARTUPINFO startup_info={0};
        PROCESS_INFORMATION process_info;
        startup_info.cb=sizeof(startup_info);
        startup_info.lpTitle = cmdline;
    	if(!CreateProcess(
    		NULL,			// pointer to name of executable module
    		cmdline,        // pointer to command line string
    		NULL,  			// process security attributes
    		NULL,   		// thread security attributes
    		FALSE, 			// handle inheritance flag
    		0,              // creation flags
            NULL,  			// pointer to new environment block
    		cfg.ctrl_dir,	// pointer to current directory name
    		&startup_info,  // pointer to STARTUPINFO
    		&process_info  	// pointer to PROCESS_INFORMATION
    		))
           	Application->MessageBox(AnsiString("ERROR " + IntToStr(GetLastError()) +
                " executing " + cmdline).c_str()
                ,"ERROR"
                ,MB_OK|MB_ICONEXCLAMATION);
    	// Resource leak if you don't close these:
    	CloseHandle(process_info.hThread);
    	CloseHandle(process_info.hProcess);
    }
    delete dlg;

}
//---------------------------------------------------------------------------

