/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

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

//---------------------------------------------------------------------------
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
#include "MailFormUnit.h"
#include "NodeFormUnit.h"

#include "StatsFormUnit.h"
#include "ClientFormUnit.h"
#include "CtrlPathDialogUnit.h"
#include "TelnetCfgDlgUnit.h"
#include "MailCfgDlgUnit.h"
#include "FtpCfgDlgUnit.h"
#include "ServicesCfgDlgUnit.h"
#include "AboutBoxFormUnit.h"
#include "CodeInputFormUnit.h"
#include "TextFileEditUnit.h"
#include "UserListFormUnit.h"
#include "PropertiesDlgUnit.h"
#include "ConfigWizardUnit.h"
#include "PreviewFormUnit.h"

#include "sbbs.h"           // unixtodstr()
#include "sbbs_ini.h"		// sbbs_read_ini()
#include "userdat.h"		// lastuser()

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "trayicon"
#pragma link "Trayicon"
#pragma resource "*.dfm"
TMainForm *MainForm;

#define LOG_TIME_FMT "  m/d  hh:mm:ssa/p"

DWORD	MaxLogLen=20000;
int     threads=1;

static void thread_up(BOOL up, BOOL setuid)
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
    sprintf(str,"Threads: %d",threads);
    AnsiString Str=AnsiString(str);
    if(MainForm->StatusBar->Panels->Items[0]->Text!=Str)
		MainForm->StatusBar->Panels->Items[0]->Text=Str;
    ReleaseMutex(mutex);
}

int sockets=0;

void socket_open(BOOL open)
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
    sprintf(str,"Sockets: %d",sockets);
    AnsiString Str=AnsiString(str);
    if(MainForm->StatusBar->Panels->Items[1]->Text!=Str)
		MainForm->StatusBar->Panels->Items[1]->Text=Str;
    ReleaseMutex(mutex);
}

int clients=0;
int total_clients=0;

static void client_add(BOOL add)
{
	char 	str[128];

    if(add) {
	    clients++;
        total_clients++;
    } else if(clients>0)
    	clients--;
    sprintf(str,"Clients: %d",clients);
    AnsiString Str=AnsiString(str);
    if(MainForm->StatusBar->Panels->Items[2]->Text!=Str)
		MainForm->StatusBar->Panels->Items[2]->Text=Str;

    sprintf(str,"Served: %d",total_clients);
    Str=AnsiString(str);
    if(MainForm->StatusBar->Panels->Items[3]->Text!=Str)
		MainForm->StatusBar->Panels->Items[3]->Text=Str;
}

static void client_on(BOOL on, int sock, client_t* client, BOOL update)
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
		if(update)	/* Can't update a non-existing entry */
			return;
        i=-1;
	}

    if(on) {
	    if(!update)
	        client_add(TRUE);
    } else { // Off
        client_add(FALSE);
        if(i>=0)
            ClientForm->ListView->Items->Delete(i);
        ReleaseMutex(mutex);
        ReleaseMutex(ClientForm->ListMutex);
        return;
    }
    if(client!=NULL && client->size==sizeof(client_t)) {
        if(i>=0) {
            Item=ClientForm->ListView->Items->Item[i];
        } else {
            Item=ClientForm->ListView->Items->Add();
            Item->Data=(void*)client->time;
            Item->Caption=sock;
        }
        Item->SubItems->Clear();
        Item->SubItems->Add(client->protocol);
        Item->SubItems->Add(client->user);
        Item->SubItems->Add(client->addr);
        Item->SubItems->Add(client->host);
        Item->SubItems->Add(client->port);
        t=time(NULL)-(time_t)Item->Data;
        sprintf(str,"%d:%02d",t/60,t%60);
        Item->SubItems->Add(str);
    }
    ReleaseMutex(mutex);
    ReleaseMutex(ClientForm->ListMutex);
}

static int bbs_lputs(char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    while(MaxLogLen && TelnetForm->Log->Text.Length()>=MaxLogLen)
        TelnetForm->Log->Lines->Delete(0);

    AnsiString Line=Now().FormatString(LOG_TIME_FMT)+"  ";
    Line+=AnsiString(str).Trim();
	TelnetForm->Log->Lines->Add(Line);
    ReleaseMutex(mutex);
    return(Line.Length());
}

static void bbs_status(char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

	TelnetForm->Status->Caption=AnsiString(str);

    ReleaseMutex(mutex);
}

static void bbs_clients(int clients)
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

static void bbs_terminated(int code)
{
	Screen->Cursor=crDefault;
	MainForm->TelnetStart->Enabled=true;
	MainForm->TelnetStop->Enabled=false;
	MainForm->TelnetRecycle->Enabled=false;    
    Application->ProcessMessages();
}
static void bbs_started(void)
{
	Screen->Cursor=crDefault;
	MainForm->TelnetStart->Enabled=false;
    MainForm->TelnetStop->Enabled=true;
    MainForm->TelnetRecycle->Enabled=true;    
    Application->ProcessMessages();
}
static void bbs_start(void)
{
	Screen->Cursor=crAppStart;
    bbs_status("Starting");
    SAFECOPY(MainForm->bbs_startup.ctrl_dir
        ,MainForm->CtrlDirectory.c_str());
    SAFECOPY(MainForm->bbs_startup.temp_dir
        ,MainForm->TempDirectory.c_str());
    SAFECOPY(MainForm->bbs_startup.host_name
        ,MainForm->Hostname.c_str());
    MainForm->bbs_startup.sem_chk_freq=MainForm->SemFileCheckFrequency;
    MainForm->bbs_startup.js_max_bytes=MainForm->JS_MaxBytes;
	_beginthread((void(*)(void*))bbs_thread,0,&MainForm->bbs_startup);
    Application->ProcessMessages();
}

static int event_log(char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    while(MaxLogLen && EventsForm->Log->Text.Length()>=MaxLogLen)
        EventsForm->Log->Lines->Delete(0);

    AnsiString Line=Now().FormatString(LOG_TIME_FMT)+"  ";
    Line+=AnsiString(str).Trim();
	EventsForm->Log->Lines->Add(Line);
    ReleaseMutex(mutex);
    return(Line.Length());
}

static int service_log(char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    while(MaxLogLen && ServicesForm->Log->Text.Length()>=MaxLogLen)
        ServicesForm->Log->Lines->Delete(0);

    AnsiString Line=Now().FormatString(LOG_TIME_FMT)+"  ";
    Line+=AnsiString(str).Trim();
	ServicesForm->Log->Lines->Add(Line);
    ReleaseMutex(mutex);
    return(Line.Length());
}

static void services_status(char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

	ServicesForm->Status->Caption=AnsiString(str);

    ReleaseMutex(mutex);
}

static void services_terminated(int code)
{
	Screen->Cursor=crDefault;
	MainForm->ServicesStart->Enabled=true;
	MainForm->ServicesStop->Enabled=false;
    MainForm->ServicesRecycle->Enabled=false;
    Application->ProcessMessages();
}
static void services_started(void)
{
	Screen->Cursor=crDefault;
	MainForm->ServicesStart->Enabled=false;
    MainForm->ServicesStop->Enabled=true;
    MainForm->ServicesRecycle->Enabled=true;
    Application->ProcessMessages();
}

static void services_clients(int clients)
{
}

static int mail_lputs(char *str)
{
	static HANDLE mutex;
	static FILE* LogStream;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    if(str==NULL) {
        if(LogStream!=NULL)
            fclose(LogStream);
        LogStream=NULL;
        ReleaseMutex(mutex);
        return(0);
    }

    while(MaxLogLen && MailForm->Log->Text.Length()>=MaxLogLen)
        MailForm->Log->Lines->Delete(0);

    AnsiString Line=Now().FormatString(LOG_TIME_FMT)+"  ";
    Line+=AnsiString(str).Trim();
	MailForm->Log->Lines->Add(Line);

    if(MainForm->MailLogFile && MainForm->MailStop->Enabled) {
        AnsiString LogFileName
            =AnsiString(MainForm->cfg.data_dir)
            +"LOGS\\MS"
            +Now().FormatString("mmddyy")
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
			Line=Now().FormatString("hh:mm:ss")+"  ";
		    Line+=AnsiString(str).Trim();
	        Line+="\n";
        	fwrite(AnsiString(Line).c_str(),1,Line.Length(),LogStream);
        }
	}

    ReleaseMutex(mutex);
    return(Line.Length());
}

static void mail_status(char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

	MailForm->Status->Caption=AnsiString(str);

    ReleaseMutex(mutex);
}

static void mail_clients(int clients)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    MailForm->ProgressBar->Max=MainForm->mail_startup.max_clients;
	MailForm->ProgressBar->Position=clients;

    ReleaseMutex(mutex);
}

static void mail_terminated(int code)
{
	Screen->Cursor=crDefault;
	MainForm->MailStart->Enabled=true;
	MainForm->MailStop->Enabled=false;
    MainForm->MailRecycle->Enabled=false;
    Application->ProcessMessages();
}
static void mail_started(void)
{
	Screen->Cursor=crDefault;
	MainForm->MailStart->Enabled=false;
    MainForm->MailStop->Enabled=true;
    MainForm->MailRecycle->Enabled=true;
    Application->ProcessMessages();
}
static void mail_start(void)
{
	Screen->Cursor=crAppStart;
    mail_status("Starting");
    SAFECOPY(MainForm->mail_startup.ctrl_dir
        ,MainForm->CtrlDirectory.c_str());
    SAFECOPY(MainForm->mail_startup.host_name
        ,MainForm->Hostname.c_str());
    MainForm->mail_startup.sem_chk_freq=MainForm->SemFileCheckFrequency;
	_beginthread((void(*)(void*))mail_server,0,&MainForm->mail_startup);
    Application->ProcessMessages();
}

static int ftp_lputs(char *str)
{
	static HANDLE mutex;
	static FILE* LogStream;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    if(str==NULL) {
        if(LogStream!=NULL)
            fclose(LogStream);
        LogStream=NULL;
        ReleaseMutex(mutex);
        return(0);
    }

#if defined(_DEBUG)
    if(IsBadReadPtr(FtpForm,sizeof(void*))) {
        DebugBreak();
        return(0);
    }
    if(IsBadReadPtr(FtpForm->Log,sizeof(void*))) {
        DebugBreak();
        return(0);
    }
    if(IsBadReadPtr(FtpForm->Log->Lines,sizeof(void*))) {
        DebugBreak();
        return(0);
    }
#endif

    while(MaxLogLen && FtpForm->Log->Text.Length()>=MaxLogLen)
        FtpForm->Log->Lines->Delete(0);

    AnsiString Line=Now().FormatString(LOG_TIME_FMT)+"  ";
    Line+=AnsiString(str).Trim();
	FtpForm->Log->Lines->Add(Line);

    if(MainForm->FtpLogFile && MainForm->FtpStop->Enabled) {
        AnsiString LogFileName
            =AnsiString(MainForm->cfg.data_dir)
            +"LOGS\\FS"
            +Now().FormatString("mmddyy")
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
            Line=Now().FormatString("hh:mm:ss")+"  ";
            Line+=AnsiString(str).Trim();
            Line+="\n";
        	fwrite(AnsiString(Line).c_str(),1,Line.Length(),LogStream);
        }
	}

    ReleaseMutex(mutex);
    return(Line.Length());
}

static void ftp_status(char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

	FtpForm->Status->Caption=AnsiString(str);

    ReleaseMutex(mutex);
}

static void ftp_clients(int clients)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    FtpForm->ProgressBar->Max=MainForm->ftp_startup.max_clients;
	FtpForm->ProgressBar->Position=clients;

    ReleaseMutex(mutex);
}

static void ftp_terminated(int code)
{
	Screen->Cursor=crDefault;
	MainForm->FtpStart->Enabled=true;
	MainForm->FtpStop->Enabled=false;
    MainForm->FtpRecycle->Enabled=false;
    Application->ProcessMessages();
}
static void ftp_started(void)
{
	Screen->Cursor=crDefault;
	MainForm->FtpStart->Enabled=false;
    MainForm->FtpStop->Enabled=true;
    MainForm->FtpRecycle->Enabled=true;
    Application->ProcessMessages();
}
static void ftp_start(void)
{
	Screen->Cursor=crAppStart;
    ftp_status("Starting");
    SAFECOPY(MainForm->ftp_startup.ctrl_dir
        ,MainForm->CtrlDirectory.c_str());
    SAFECOPY(MainForm->ftp_startup.temp_dir
        ,MainForm->TempDirectory.c_str());
    SAFECOPY(MainForm->ftp_startup.host_name
        ,MainForm->Hostname.c_str());
    MainForm->ftp_startup.sem_chk_freq=MainForm->SemFileCheckFrequency;
    MainForm->ftp_startup.js_max_bytes=MainForm->JS_MaxBytes;
	_beginthread((void(*)(void*))ftp_server,0,&MainForm->ftp_startup);
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
__fastcall TMainForm::TMainForm(TComponent* Owner)
        : TForm(Owner)
{
    /* Defaults */
    CtrlDirectory="c:\\sbbs\\ctrl\\";
    LoginCommand="telnet://localhost";
    ConfigCommand="%sscfg.exe %s -l25";
    JS_MaxBytes=JAVASCRIPT_MAX_BYTES;
    MinimizeToSysTray=false;
    UndockableForms=false;
    NodeDisplayInterval=1;  	/* seconds */
    ClientDisplayInterval=5;    /* seconds */
    SemFileCheckFrequency=5;	/* seconds */
    Initialized=false;

    char* p;
    if((p=getenv("SBBSCTRL"))!=NULL)
        CtrlDirectory=p;
    char ch=*lastchar(CtrlDirectory.c_str());
    if(ch!='\\' && ch!='/')
        CtrlDirectory+="\\";

    memset(&bbs_startup,0,sizeof(bbs_startup));
    bbs_startup.size=sizeof(bbs_startup);
    bbs_startup.first_node=1;
    bbs_startup.last_node=4;
	bbs_startup.options=BBS_OPT_XTRN_MINIMIZED|BBS_OPT_SYSOP_AVAILABLE;
	bbs_startup.telnet_port=IPPORT_TELNET;
    bbs_startup.telnet_interface=INADDR_ANY;
    bbs_startup.rlogin_port=513;
    bbs_startup.rlogin_interface=INADDR_ANY;
	bbs_startup.lputs=bbs_lputs;
    bbs_startup.status=bbs_status;
    bbs_startup.clients=bbs_clients;
    bbs_startup.started=bbs_started;
    bbs_startup.terminated=bbs_terminated;
    bbs_startup.thread_up=thread_up;
    bbs_startup.client_on=client_on;
    bbs_startup.socket_open=socket_open;
    bbs_startup.event_log=event_log;

    memset(&mail_startup,0,sizeof(mail_startup));
    mail_startup.size=sizeof(mail_startup);
    mail_startup.smtp_port=IPPORT_SMTP;
    mail_startup.relay_port=IPPORT_SMTP;
    mail_startup.pop3_port=110;
    mail_startup.interface_addr=INADDR_ANY;
	mail_startup.lputs=mail_lputs;
    mail_startup.status=mail_status;
    mail_startup.clients=mail_clients;
    mail_startup.started=mail_started;
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

    memset(&ftp_startup,0,sizeof(ftp_startup));
    ftp_startup.size=sizeof(ftp_startup);
    ftp_startup.port=IPPORT_FTP;
    ftp_startup.interface_addr=INADDR_ANY;
	ftp_startup.lputs=ftp_lputs;
    ftp_startup.status=ftp_status;
    ftp_startup.clients=ftp_clients;
    ftp_startup.started=ftp_started;
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

    memset(&web_startup,0,sizeof(web_startup));
    web_startup.size=sizeof(web_startup);

    memset(&services_startup,0,sizeof(services_startup));
    services_startup.size=sizeof(services_startup);
    services_startup.interface_addr=INADDR_ANY;
    services_startup.lputs=service_log;
    services_startup.status=services_status;
    services_startup.clients=services_clients;
    services_startup.started=services_started;
    services_startup.terminated=services_terminated;
    services_startup.thread_up=thread_up;
    services_startup.client_on=client_on;
    services_startup.socket_open=socket_open;

    /* Default local "Spy Terminal" settings */
    SpyTerminalFont=new TFont;
    SpyTerminalFont->Name="Terminal";
    SpyTerminalFont->Size=9;
    SpyTerminalFont->Pitch=fpFixed;
    SpyTerminalWidth=434;
    SpyTerminalHeight=365;
    SpyTerminalKeyboardActive=true;

    Application->OnMinimize=FormMinimize;    
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
    if(bbs_ver < (0x31000 | 'M'-'A') || bbs_ver > (0x399<<8)) {
        char str[128];
        sprintf(str,"Incorrect SBBS.DLL Version (%lX)",bbs_ver);
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
	Toolbar->Visible=!ViewToolbarMenuItem->Checked;
    ViewToolbarMenuItem->Checked=Toolbar->Visible;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::FormClose(TObject *Sender, TCloseAction &Action)
{
    UpTimer->Enabled=false; /* Stop updating the status bar */

    if(TrayIcon->Visible)           /* minimized to tray? */
        TrayIcon->Visible=false;    /* restore to avoid crash */
        
    /* This is necessary to save form sizes/positions */
    if(Initialized) /* Don't overwrite registry settings with defaults */
        SaveSettings(Sender);

	StatusBar->Panels->Items[4]->Text="Closing...";
    time_t start=time(NULL);
	while(TelnetStop->Enabled || MailStop->Enabled || FtpStop->Enabled
    	|| ServicesStop->Enabled) {
        if(time(NULL)-start>30)
            break;
        Application->ProcessMessages();
        YIELD();
    }
    /* Extra time for callbacks to be called by child threads */
    start=time(NULL);
    while(time(NULL)<start+2) {
        Application->ProcessMessages();
        YIELD();
    }
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FormCloseQuery(TObject *Sender, bool &CanClose)
{
	CanClose=false;

    if(TelnetStop->Enabled) {
     	if(TelnetForm->ProgressBar->Position
	        && Application->MessageBox("Shut down the Telnet Server?"
        	,"Telnet Server In Use", MB_OKCANCEL)!=IDOK)
            return;
        TelnetStopExecute(Sender);
	}

    if(MailStop->Enabled) {
    	if(MailForm->ProgressBar->Position
    		&& Application->MessageBox("Shut down the Mail Server?"
        	,"Mail Server In Use", MB_OKCANCEL)!=IDOK)
            return;
        MailStopExecute(Sender);
    }

    if(FtpStop->Enabled) {
    	if(FtpForm->ProgressBar->Position
    		&& Application->MessageBox("Shut down the FTP Server?"
	       	,"FTP Server In Use", MB_OKCANCEL)!=IDOK)
            return;
        FtpStopExecute(Sender);
    }
    ServicesStopExecute(Sender);

    CanClose=true;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::TelnetStartExecute(TObject *Sender)
{
	bbs_start();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ServicesStartExecute(TObject *Sender)
{
	Screen->Cursor=crAppStart;
    services_status("Starting");

    SAFECOPY(MainForm->services_startup.ctrl_dir
        ,MainForm->CtrlDirectory.c_str());
    SAFECOPY(MainForm->services_startup.host_name
        ,MainForm->Hostname.c_str());
    MainForm->services_startup.sem_chk_freq=SemFileCheckFrequency;        
    MainForm->services_startup.js_max_bytes=MainForm->JS_MaxBytes;
	_beginthread((void(*)(void*))services_thread,0,&MainForm->services_startup);
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ServicesStopExecute(TObject *Sender)
{
	Screen->Cursor=crAppStart;
    services_status("Terminating");
	services_terminate();
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::TelnetStopExecute(TObject *Sender)
{
	Screen->Cursor=crAppStart;
    bbs_status("Terminating");
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
    mail_start();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::MailStopExecute(TObject *Sender)
{
	Screen->Cursor=crAppStart;
    mail_status("Terminating");
	mail_terminate();
    Application->ProcessMessages();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewTelnetExecute(TObject *Sender)
{
	TelnetForm->Visible=!TelnetForm->Visible;
    ViewTelnet->Checked=TelnetForm->Visible;
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ViewEventsExecute(TObject *Sender)
{
	EventsForm->Visible=!EventsForm->Visible;
    ViewEvents->Checked=EventsForm->Visible;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewNodesExecute(TObject *Sender)
{
	NodeForm->Visible=!NodeForm->Visible;
    ViewNodes->Checked=NodeForm->Visible;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewMailServerExecute(TObject *Sender)
{
	MailForm->Visible=!MailForm->Visible;
    ViewMailServer->Checked=MailForm->Visible;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewFtpServerExecute(TObject *Sender)
{
	FtpForm->Visible=!FtpForm->Visible;
    ViewFtpServer->Checked=FtpForm->Visible;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FtpStartExecute(TObject *Sender)
{
	ftp_start();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FtpStopExecute(TObject *Sender)
{
	Screen->Cursor=crAppStart;
    ftp_status("Terminating");
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


void __fastcall TMainForm::BBSConfigureMenuItemClick(TObject *Sender)
{
	char str[256];

    sprintf(str,ConfigCommand.c_str()
    	,cfg.exec_dir, cfg.ctrl_dir);
    STARTUPINFO startup_info={0};
    PROCESS_INFORMATION process_info;
    startup_info.cb=sizeof(startup_info);
    startup_info.lpTitle="Synchronet Configuration Utility";
	CreateProcess(
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
		);
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

	if(!StatsForm->Visible)
		return;

    getstats(&cfg,0,&stats);

	StatsForm->TotalLogons->Caption=AnsiString(stats.logons);
    StatsForm->LogonsToday->Caption=AnsiString(stats.ltoday);
    StatsForm->TotalTimeOn->Caption=AnsiString(stats.timeon);
    StatsForm->TimeToday->Caption=AnsiString(stats.ttoday);
    StatsForm->TotalEMail->Caption=AnsiString(getmail(&cfg,0,0));
	StatsForm->EMailToday->Caption=AnsiString(stats.etoday);
	StatsForm->TotalFeedback->Caption=AnsiString(getmail(&cfg,1,0));
	StatsForm->FeedbackToday->Caption=AnsiString(stats.ftoday);
    StatsForm->TotalUsers->Caption=AnsiString(total_users(&cfg));
    StatsForm->NewUsersToday->Caption=AnsiString(stats.nusers);
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
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewStatsExecute(TObject *Sender)
{
	StatsForm->Visible=!StatsForm->Visible;
    ViewStats->Checked=StatsForm->Visible;
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

    if(Registry->ValueExists("Style")) {
        int style=Registry->ReadInteger("Style");
        Font->Style=Font->Style.Clear();
        for(int i=fsBold;i<=fsStrikeOut;i++)
            if(style&(1<<i))
                Font->Style=Font->Style<<(TFontStyle)i;
    }
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

    int style=0;
    for(int i=fsBold;i<=fsStrikeOut;i++)
    	if(Font->Style.Contains((TFontStyle)i))
        	style|=(1<<i);
    Registry->WriteInteger("Style",style);

    Registry->CloseKey();
    delete Registry;
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
    int		NodeFormPage=PAGE_UPPERLEFT;
    int		StatsFormPage=PAGE_UPPERLEFT;
    int		ClientFormPage=PAGE_UPPERLEFT;
    int		TelnetFormPage=PAGE_LOWERLEFT;
    int		EventsFormPage=PAGE_LOWERLEFT;
    int		MailFormPage=PAGE_UPPERRIGHT;
    int		FtpFormPage=PAGE_LOWERRIGHT;
    int     ServicesFormPage=PAGE_LOWERRIGHT;

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
    }

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
    NodeForm->ListBox->Color=ReadColor(Registry,"NodeList");
    ReadFont("NodeList",NodeForm->ListBox->Font);
    ClientForm->ListView->Color=ReadColor(Registry,"ClientList");
    ReadFont("ClientList",ClientForm->ListView->Font);

	if(Registry->ValueExists("TelnetFormTop"))
    	TelnetForm->Top=Registry->ReadInteger("TelnetFormTop");
	if(Registry->ValueExists("TelnetFormLeft"))
    	TelnetForm->Left=Registry->ReadInteger("TelnetFormLeft");
	if(Registry->ValueExists("TelnetFormWidth"))
    	TelnetForm->Width=Registry->ReadInteger("TelnetFormWidth");
	if(Registry->ValueExists("TelnetFormHeight"))
    	TelnetForm->Height=Registry->ReadInteger("TelnetFormHeight");

	if(Registry->ValueExists("EventsFormTop"))
    	EventsForm->Top=Registry->ReadInteger("EventsFormTop");
	if(Registry->ValueExists("EventsFormLeft"))
    	EventsForm->Left=Registry->ReadInteger("EventsFormLeft");
	if(Registry->ValueExists("EventsFormWidth"))
    	EventsForm->Width=Registry->ReadInteger("EventsFormWidth");
	if(Registry->ValueExists("EventsFormHeight"))
    	EventsForm->Height=Registry->ReadInteger("EventsFormHeight");

	if(Registry->ValueExists("ServicesFormTop"))
    	ServicesForm->Top=Registry->ReadInteger("ServicesFormTop");
	if(Registry->ValueExists("ServicesFormLeft"))
    	ServicesForm->Left=Registry->ReadInteger("ServicesFormLeft");
	if(Registry->ValueExists("ServicesFormWidth"))
    	ServicesForm->Width=Registry->ReadInteger("ServicesFormWidth");
	if(Registry->ValueExists("ServicesFormHeight"))
    	ServicesForm->Height=Registry->ReadInteger("ServicesFormHeight");

	if(Registry->ValueExists("FtpFormTop"))
    	FtpForm->Top=Registry->ReadInteger("FtpFormTop");
	if(Registry->ValueExists("FtpFormLeft"))
    	FtpForm->Left=Registry->ReadInteger("FtpFormLeft");
	if(Registry->ValueExists("FtpFormWidth"))
    	FtpForm->Width=Registry->ReadInteger("FtpFormWidth");
	if(Registry->ValueExists("FtpFormHeight"))
    	FtpForm->Height=Registry->ReadInteger("FtpFormHeight");

	if(Registry->ValueExists("MailFormTop"))
    	MailForm->Top=Registry->ReadInteger("MailFormTop");
	if(Registry->ValueExists("MailFormLeft"))
    	MailForm->Left=Registry->ReadInteger("MailFormLeft");
	if(Registry->ValueExists("MailFormWidth"))
    	MailForm->Width=Registry->ReadInteger("MailFormWidth");
	if(Registry->ValueExists("MailFormHeight"))
    	MailForm->Height=Registry->ReadInteger("MailFormHeight");

	if(Registry->ValueExists("NodeFormTop"))
    	NodeForm->Top=Registry->ReadInteger("NodeFormTop");
	if(Registry->ValueExists("NodeFormLeft"))
    	NodeForm->Left=Registry->ReadInteger("NodeFormLeft");
	if(Registry->ValueExists("NodeFormWidth"))
    	NodeForm->Width=Registry->ReadInteger("NodeFormWidth");
	if(Registry->ValueExists("NodeFormHeight"))
    	NodeForm->Height=Registry->ReadInteger("NodeFormHeight");

	if(Registry->ValueExists("StatsFormTop"))
    	StatsForm->Top=Registry->ReadInteger("StatsFormTop");
	if(Registry->ValueExists("StatsFormLeft"))
    	StatsForm->Left=Registry->ReadInteger("StatsFormLeft");
	if(Registry->ValueExists("StatsFormWidth"))
    	StatsForm->Width=Registry->ReadInteger("StatsFormWidth");
	if(Registry->ValueExists("StatsFormHeight"))
    	StatsForm->Height=Registry->ReadInteger("StatsFormHeight");

	if(Registry->ValueExists("ClientFormTop"))
    	ClientForm->Top=Registry->ReadInteger("ClientFormTop");
	if(Registry->ValueExists("ClientFormLeft"))
    	ClientForm->Left=Registry->ReadInteger("ClientFormLeft");
	if(Registry->ValueExists("ClientFormWidth"))
    	ClientForm->Width=Registry->ReadInteger("ClientFormWidth");
	if(Registry->ValueExists("ClientFormHeight"))
    	ClientForm->Height=Registry->ReadInteger("ClientFormHeight");

    for(int i=0;i<ClientForm->ListView->Columns->Count;i++) {
        char str[128];
        sprintf(str,"ClientListColumn%dWidth",i);
        if(Registry->ValueExists(str))
            ClientForm->ListView->Columns->Items[i]->Width
                =Registry->ReadInteger(str);
    }

    if(Registry->ValueExists("ToolbarVisible"))
    	Toolbar->Visible=Registry->ReadBool("ToolbarVisible");

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

    if(Registry->ValueExists("ServicesAutoStart"))
    	ServicesAutoStart=Registry->ReadInteger("ServicesAutoStart");
    else
    	ServicesAutoStart=true;

    ViewToolbarMenuItem->Checked=Toolbar->Visible;
    ViewStatusBarMenuItem->Checked=StatusBar->Visible;

    if(Registry->ValueExists("Hostname"))
    	Hostname=Registry->ReadString("Hostname");
    if(Registry->ValueExists("CtrlDirectory"))
    	CtrlDirectory=Registry->ReadString("CtrlDirectory");
    if(Registry->ValueExists("TempDirectory"))
    	TempDirectory=Registry->ReadString("TempDirectory");

    if(Registry->ValueExists("MaxLogLen"))
    	MaxLogLen=Registry->ReadInteger("MaxLogLen");
    if(Registry->ValueExists("JS_MaxBytes"))
    	JS_MaxBytes=Registry->ReadInteger("JS_MaxBytes");
    if(JS_MaxBytes==0)
        JS_MaxBytes=JAVASCRIPT_MAX_BYTES;

    if(Registry->ValueExists("LoginCommand"))
    	LoginCommand=Registry->ReadString("LoginCommand");
    if(Registry->ValueExists("ConfigCommand"))
    	ConfigCommand=Registry->ReadString("ConfigCommand");
    if(Registry->ValueExists("Password"))
    	Password=Registry->ReadString("Password");
    if(Registry->ValueExists("MinimizeToSysTray"))
    	MinimizeToSysTray=Registry->ReadBool("MinimizeToSysTray");
	if(Registry->ValueExists("NodeDisplayInterval"))
    	NodeDisplayInterval=Registry->ReadInteger("NodeDisplayInterval");
	if(Registry->ValueExists("ClientDisplayInterval"))
    	ClientDisplayInterval=Registry->ReadInteger("ClientDisplayInterval");
	if(Registry->ValueExists("SemFileCheckFrequency"))
    	SemFileCheckFrequency=Registry->ReadInteger("SemFileCheckFrequency");

    if(Registry->ValueExists("MailLogFile"))
    	MailLogFile=Registry->ReadInteger("MailLogFile");
    else
    	MailLogFile=true;

    if(Registry->ValueExists("FtpLogFile"))
    	FtpLogFile=Registry->ReadInteger("FtpLogFile");
    else
    	FtpLogFile=true;

    if(Registry->ValueExists("TelnetInterface"))
    	bbs_startup.telnet_interface=Registry->ReadInteger("TelnetInterface");
    if(Registry->ValueExists("RLoginInterface"))
    	bbs_startup.rlogin_interface=Registry->ReadInteger("RLoginInterface");

	if(Registry->ValueExists("TelnetPort"))
    	bbs_startup.telnet_port=Registry->ReadInteger("TelnetPort");
	if(Registry->ValueExists("RLoginPort"))
    	bbs_startup.rlogin_port=Registry->ReadInteger("RLoginPort");

    if(Registry->ValueExists("FirstNode"))
    	bbs_startup.first_node=Registry->ReadInteger("FirstNode");

    if(Registry->ValueExists("LastNode"))
    	bbs_startup.last_node=Registry->ReadInteger("LastNode");

    if(Registry->ValueExists("ExternalYield"))
    	bbs_startup.xtrn_polls_before_yield=Registry->ReadInteger("ExternalYield");

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

    if(Registry->ValueExists("MailInterface"))
    	mail_startup.interface_addr=Registry->ReadInteger("MailInterface");

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

    if(Registry->ValueExists("FtpInterface"))
    	ftp_startup.interface_addr=Registry->ReadInteger("FtpInterface");

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

    if(Registry->ValueExists("ServicesInterface"))
    	services_startup.interface_addr
            =Registry->ReadInteger("ServicesInterface");

    if(Registry->ValueExists("ServicesAnswerSound"))
    	SAFECOPY(services_startup.answer_sound
        	,Registry->ReadString("ServicesAnswerSound").c_str());

    if(Registry->ValueExists("ServicesHangupSound"))
    	SAFECOPY(services_startup.hangup_sound
        	,Registry->ReadString("ServicesHangupSound").c_str());

    if(Registry->ValueExists("ServicesOptions"))
    	services_startup.options=Registry->ReadInteger("ServicesOptions");

    Registry->CloseKey();
    delete Registry;

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
    memset(&cfg,0,sizeof(cfg));
    strcpy(cfg.ctrl_dir,CtrlDirectory.c_str());
    cfg.size=sizeof(cfg);
    cfg.node_num=bbs_startup.first_node;
    char error[256];
	SAFECOPY(error,UNKNOWN_LOAD_ERROR);
	if(!load_cfg(&cfg, NULL, TRUE, error)) {
    	Application->MessageBox(error,"ERROR Loading Configuration"
	        ,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
        return;
    }

    if(!(cfg.sys_misc&SM_LOCAL_TZ)) {
    	if(putenv("TZ=UTC0")) {
        	Application->MessageBox("Error setting timezone"
            	,"ERROR",MB_OK|MB_ICONEXCLAMATION);
            Application->Terminate();
        }
    	tzset();
    }

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

    if(bbs_startup.options&BBS_OPT_SYSOP_AVAILABLE)
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

    NodeForm->Show();
    ClientForm->Show();
    StatsForm->Show();
    TelnetForm->Show();
    EventsForm->Show();
    FtpForm->Show();
    ServicesForm->Show();
    MailForm->Show();

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

    if(SysAutoStart)
        bbs_start();
    if(MailAutoStart)
        mail_start();
    if(FtpAutoStart)
        ftp_start();
    if(ServicesAutoStart)
        ServicesStartExecute(Sender);

    NodeForm->Timer->Interval=NodeDisplayInterval*1000;
    NodeForm->Timer->Enabled=true;
    ClientForm->Timer->Interval=ClientDisplayInterval*1000;
    ClientForm->Timer->Enabled=true;

    StatsTimer->Interval=cfg.node_stat_check*1000;
	StatsTimer->Enabled=true;
    Initialized=true;

    UpTimer->Enabled=true; /* Start updating the status bar */

    if(!Application->Active)	/* Starting up minimized? */
    	FormMinimize(Sender);   /* Put icon in systray */
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::SaveSettings(TObject* Sender)
{
	StatusBar->Panels->Items[4]->Text="Saving Settings...";

    NodeForm->Timer->Interval=NodeDisplayInterval*1000;
    ClientForm->Timer->Interval=ClientDisplayInterval*1000;

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

    Registry->WriteInteger("StatsFormTop",StatsForm->Top);
    Registry->WriteInteger("StatsFormLeft",StatsForm->Left);
    Registry->WriteInteger("StatsFormHeight",StatsForm->Height);
    Registry->WriteInteger("StatsFormWidth",StatsForm->Width);

    Registry->WriteInteger("ClientFormTop",ClientForm->Top);
    Registry->WriteInteger("ClientFormLeft",ClientForm->Left);
    Registry->WriteInteger("ClientFormHeight",ClientForm->Height);
    Registry->WriteInteger("ClientFormWidth",ClientForm->Width);

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

    Registry->WriteInteger("EventsFormTop",EventsForm->Top);
    Registry->WriteInteger("EventsFormLeft",EventsForm->Left);
    Registry->WriteInteger("EventsFormHeight",EventsForm->Height);
    Registry->WriteInteger("EventsFormWidth",EventsForm->Width);

    Registry->WriteInteger("ServicesFormTop",ServicesForm->Top);
    Registry->WriteInteger("ServicesFormLeft",ServicesForm->Left);
    Registry->WriteInteger("ServicesFormHeight",ServicesForm->Height);
    Registry->WriteInteger("ServicesFormWidth",ServicesForm->Width);

    Registry->WriteInteger("FtpFormTop",FtpForm->Top);
    Registry->WriteInteger("FtpFormLeft",FtpForm->Left);
    Registry->WriteInteger("FtpFormHeight",FtpForm->Height);
    Registry->WriteInteger("FtpFormWidth",FtpForm->Width);

    Registry->WriteInteger("MailFormTop",MailForm->Top);
    Registry->WriteInteger("MailFormLeft",MailForm->Left);
    Registry->WriteInteger("MailFormHeight",MailForm->Height);
    Registry->WriteInteger("MailFormWidth",MailForm->Width);

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
    Registry->WriteBool("MailFormFloating",MailForm->Floating);

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
    WriteColor(Registry,"NodeList",NodeForm->ListBox->Color);
    WriteFont("NodeList",NodeForm->ListBox->Font);
    WriteColor(Registry,"ClientList",ClientForm->ListView->Color);
    WriteFont("ClientList",ClientForm->ListView->Font);

    Registry->WriteBool("ToolBarVisible",Toolbar->Visible);
    Registry->WriteBool("StatusBarVisible",StatusBar->Visible);

    Registry->WriteString("Hostname",Hostname);
    Registry->WriteString("CtrlDirectory",CtrlDirectory);
    Registry->WriteString("TempDirectory",TempDirectory);
    Registry->WriteInteger("MaxLogLen",MaxLogLen);
    Registry->WriteInteger("JS_MaxBytes",JS_MaxBytes);
    Registry->WriteString("LoginCommand",LoginCommand);
    Registry->WriteString("ConfigCommand",ConfigCommand);
    Registry->WriteString("Password",Password);
    Registry->WriteBool("MinimizeToSysTray",MinimizeToSysTray);
    Registry->WriteInteger("NodeDisplayInterval",NodeDisplayInterval);
    Registry->WriteInteger("ClientDisplayInterval",ClientDisplayInterval);
    Registry->WriteInteger("SemFileCheckFrequency",SemFileCheckFrequency);

    Registry->WriteInteger("SysAutoStart",SysAutoStart);
    Registry->WriteInteger("MailAutoStart",MailAutoStart);
    Registry->WriteInteger("FtpAutoStart",FtpAutoStart);
    Registry->WriteInteger("ServicesAutoStart",ServicesAutoStart);
    Registry->WriteInteger("MailLogFile",MailLogFile);
    Registry->WriteInteger("FtpLogFile",FtpLogFile);

    Registry->WriteInteger("TelnetInterface",bbs_startup.telnet_interface);
    Registry->WriteInteger("RLoginInterface",bbs_startup.rlogin_interface);

	Registry->WriteInteger("TelnetPort",bbs_startup.telnet_port);
	Registry->WriteInteger("RLoginPort",bbs_startup.rlogin_port);
    Registry->WriteInteger("FirstNode",bbs_startup.first_node);
    Registry->WriteInteger("LastNode",bbs_startup.last_node);

    Registry->WriteInteger("ExternalYield",bbs_startup.xtrn_polls_before_yield);
    Registry->WriteString("AnswerSound",AnsiString(bbs_startup.answer_sound));
    Registry->WriteString("HangupSound",AnsiString(bbs_startup.hangup_sound));

    Registry->WriteInteger("StartupOptions",bbs_startup.options);

    Registry->WriteInteger("MailMaxClients",mail_startup.max_clients);
    Registry->WriteInteger("MailMaxInactivity",mail_startup.max_inactivity);
    Registry->WriteInteger("MailInterface",mail_startup.interface_addr);
    Registry->WriteInteger("MailMaxDeliveryAttempts"
        ,mail_startup.max_delivery_attempts);
    Registry->WriteInteger("MailRescanFrequency"
        ,mail_startup.rescan_frequency);
    Registry->WriteInteger("MailMaxRecipients"
        ,mail_startup.max_recipients);
    Registry->WriteInteger("MailMaxMsgSize"
        ,mail_startup.max_msg_size);
    Registry->WriteInteger("MailLinesPerYield"
        ,mail_startup.lines_per_yield);

    Registry->WriteInteger("MailSMTPPort",mail_startup.smtp_port);
    Registry->WriteInteger("MailPOP3Port",mail_startup.pop3_port);

    Registry->WriteString("MailDefaultUser",mail_startup.default_user);
	Registry->WriteString("MailDNSBlacklistHeader",mail_startup.dnsbl_hdr);
	Registry->WriteString("MailDNSBlacklistSubject",mail_startup.dnsbl_tag);

    Registry->WriteString("MailRelayServer",mail_startup.relay_server);
    Registry->WriteInteger("MailRelayPort",mail_startup.relay_port);
    Registry->WriteString("MailDNSServer",mail_startup.dns_server);

    Registry->WriteString("MailInboundSound",AnsiString(mail_startup.inbound_sound));
    Registry->WriteString("MailOutboundSound",AnsiString(mail_startup.outbound_sound));
    Registry->WriteInteger("MailOptions",mail_startup.options);

    Registry->WriteString("MailPOP3Sound",AnsiString(mail_startup.pop3_sound));

	Registry->WriteInteger("FtpPort",ftp_startup.port);
    Registry->WriteInteger("FtpMaxClients",ftp_startup.max_clients);
    Registry->WriteInteger("FtpMaxInactivity",ftp_startup.max_inactivity);
    Registry->WriteInteger("FtpQwkTimeout",ftp_startup.qwk_timeout);
    Registry->WriteInteger("FtpInterface",ftp_startup.interface_addr);
    Registry->WriteString("FtpAnswerSound",AnsiString(ftp_startup.answer_sound));
    Registry->WriteString("FtpHangupSound",AnsiString(ftp_startup.hangup_sound));
    Registry->WriteString("FtpHackAttemptSound",AnsiString(ftp_startup.hack_sound));

    Registry->WriteString("FtpIndexFileName"
    	,AnsiString(ftp_startup.index_file_name));
    Registry->WriteString("FtpHtmlIndexFile"
    	,AnsiString(ftp_startup.html_index_file));
    Registry->WriteString("FtpHtmlIndexScript"
    	,AnsiString(ftp_startup.html_index_script));

    Registry->WriteInteger("FtpOptions",ftp_startup.options);

    Registry->WriteInteger("ServicesInterface",services_startup.interface_addr);

    Registry->WriteString("ServicesAnswerSound"
        ,AnsiString(services_startup.answer_sound));
    Registry->WriteString("ServicesHangupSound"
        ,AnsiString(services_startup.hangup_sound));

    Registry->WriteInteger("ServicesOptions",services_startup.options);

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
void __fastcall TMainForm::ImportSettings(TObject* Sender)
{
    OpenDialog->Filter="Settings files (*.ini)|*.ini|All files|*.*";
    OpenDialog->FileName=CtrlDirectory+"sbbs.ini";
    if(!OpenDialog->Execute())
    	return;

    FILE* fp;

    if((fp=fopen(OpenDialog->FileName.c_str(),"r"))==NULL) {
    	char str[MAX_PATH*2];
        char err[MAX_PATH];
        SAFECOPY(err,truncsp(strerror(errno)));
    	sprintf(str,"ERROR (%s) opening %s"
        	,err
            ,OpenDialog->FileName.c_str());
        Application->MessageBox(str,"Import Error",MB_OK|MB_ICONEXCLAMATION);
    	return;
    }
    
	StatusBar->Panels->Items[4]->Text="Importing Settings...";

    sbbs_read_ini(fp
    	,(BOOL*)&SysAutoStart   		,&bbs_startup
    	,(BOOL*)&FtpAutoStart 			,&ftp_startup
    	,(BOOL*)&WebAutoStart 			,&web_startup
    	,(BOOL*)&MailAutoStart 	    	,&mail_startup
    	,(BOOL*)&ServicesAutoStart     	,&services_startup
        );
    fclose(fp);

	TMemIniFile* IniFile=new TMemIniFile(OpenDialog->FileName);

    const char* section = "sbbsctrl";

   	TopPanel->Height
    	=IniFile->ReadInteger(section,"TopPanelHeight"
    		,TopPanel->Height);
   	UpperLeftPageControl->Width
       	=IniFile->ReadInteger(section,"UpperLeftPageControlWidth"
        	,UpperLeftPageControl->Width);
   	LowerLeftPageControl->Width
       	=IniFile->ReadInteger(section,"LowerLeftPageControlWidth"
        	,LowerLeftPageControl->Width);
    UndockableForms=IniFile->ReadBool(section,"UndockableForms"
    	,UndockableForms);
#if 0
    if(UndockableForms) {
        TelnetFormFloating=IniFile->ReadBool(section,"TelnetFormFloating",false);
        EventsFormFloating=IniFile->ReadBool(section,"EventsFormFloating",false);
        ServicesFormFloating=IniFile->ReadBool(section,"ServicesFormFloating",false);
        NodeFormFloating=IniFile->ReadBool(section,"NodeFormFloating",false);
        StatsFormFloating=IniFile->ReadBool(section,"StatsFormFloating",false);
        ClientFormFloating=IniFile->ReadBool(section,"ClientFormFloating",false);
        MailFormFloating=IniFile->ReadBool(section,"MailFormFloating",false);
        FtpFormFloating=IniFile->ReadBool(section,"FtpFormFloating",false);
    }
#endif

#if 0
  	TelnetFormPage=IniFile->ReadInteger(section,"TelnetFormPage",TelnetFormPage);
  	EventsFormPage=IniFile->ReadInteger(section,"EventsFormPage",EventsFormPage);
   	ServicesFormPage=IniFile->ReadInteger(section,"ServicesFormPage",ServicesFormPage);
   	NodeFormPage=IniFile->ReadInteger(section,"NodeFormPage",NodeFormPage);
   	StatsFormPage=IniFile->ReadInteger(section,"StatsFormPage",StatsFormPage);
  	ClientFormPage=IniFile->ReadInteger(section,"ClientFormPage",ClientFormPage);
   	MailFormPage=IniFile->ReadInteger(section,"MailFormPage",MailFormPage);
   	FtpFormPage=IniFile->ReadInteger(section,"FtpFormPage",FtpFormPage);

#endif
    
#if 0
    TelnetForm->Log->Color=ReadColor(IniFile,"TelnetLog");
    ReadFont("TelnetLog",TelnetForm->Log->Font);
    EventsForm->Log->Color=ReadColor(IniFile,"EventsLog");
    ReadFont("EventsLog",EventsForm->Log->Font);
    ServicesForm->Log->Color=ReadColor(IniFile,"ServicesLog");
    ReadFont("ServicesLog",ServicesForm->Log->Font);
    MailForm->Log->Color=ReadColor(IniFile,"MailLog");
    ReadFont("MailLog",MailForm->Log->Font);
    FtpForm->Log->Color=ReadColor(IniFile,"FtpLog");
    ReadFont("FtpLog",FtpForm->Log->Font);
    NodeForm->ListBox->Color=ReadColor(IniFile,"NodeList");
    ReadFont("NodeList",NodeForm->ListBox->Font);
    ClientForm->ListView->Color=ReadColor(IniFile,"ClientList");
    ReadFont("ClientList",ClientForm->ListView->Font);
#endif

   	TelnetForm->Top=IniFile->ReadInteger(section,"TelnetFormTop"
    	,TelnetForm->Top);
   	TelnetForm->Left=IniFile->ReadInteger(section,"TelnetFormLeft"
    	,TelnetForm->Left);
   	TelnetForm->Width=IniFile->ReadInteger(section,"TelnetFormWidth"
    	,TelnetForm->Width);
   	TelnetForm->Height=IniFile->ReadInteger(section,"TelnetFormHeight"
    	,TelnetForm->Height);

   	EventsForm->Top=IniFile->ReadInteger(section,"EventsFormTop"
    	,EventsForm->Top);
   	EventsForm->Left=IniFile->ReadInteger(section,"EventsFormLeft"
    	,EventsForm->Left);
   	EventsForm->Width=IniFile->ReadInteger(section,"EventsFormWidth"
    	,EventsForm->Width);
   	EventsForm->Height=IniFile->ReadInteger(section,"EventsFormHeight"
    	,EventsForm->Height);

   	ServicesForm->Top=IniFile->ReadInteger(section,"ServicesFormTop"
    	,ServicesForm->Top);
   	ServicesForm->Left=IniFile->ReadInteger(section,"ServicesFormLeft"
    	,ServicesForm->Left);
   	ServicesForm->Width=IniFile->ReadInteger(section,"ServicesFormWidth"
    	,ServicesForm->Width);
   	ServicesForm->Height=IniFile->ReadInteger(section,"ServicesFormHeight"
    	,ServicesForm->Height);

   	FtpForm->Top=IniFile->ReadInteger(section,"FtpFormTop"
    	,FtpForm->Top);
   	FtpForm->Left=IniFile->ReadInteger(section,"FtpFormLeft"
    	,FtpForm->Left);
   	FtpForm->Width=IniFile->ReadInteger(section,"FtpFormWidth"
    	,FtpForm->Width);
   	FtpForm->Height=IniFile->ReadInteger(section,"FtpFormHeight"
    	,FtpForm->Height);

   	MailForm->Top=IniFile->ReadInteger(section,"MailFormTop"
    	,MailForm->Top);
   	MailForm->Left=IniFile->ReadInteger(section,"MailFormLeft"
    	,MailForm->Left);
   	MailForm->Width=IniFile->ReadInteger(section,"MailFormWidth"
    	,MailForm->Width);
   	MailForm->Height=IniFile->ReadInteger(section,"MailFormHeight"
    	,MailForm->Height);

   	NodeForm->Top=IniFile->ReadInteger(section,"NodeFormTop"
    	,NodeForm->Top);
   	NodeForm->Left=IniFile->ReadInteger(section,"NodeFormLeft"
    	,NodeForm->Left);
   	NodeForm->Width=IniFile->ReadInteger(section,"NodeFormWidth"
    	,NodeForm->Width);
   	NodeForm->Height=IniFile->ReadInteger(section,"NodeFormHeight"
    	,NodeForm->Height);

   	StatsForm->Top=IniFile->ReadInteger(section,"StatsFormTop"
    	,StatsForm->Top);
   	StatsForm->Left=IniFile->ReadInteger(section,"StatsFormLeft"
    	,StatsForm->Left);
   	StatsForm->Width=IniFile->ReadInteger(section,"StatsFormWidth"
    	,StatsForm->Width);
   	StatsForm->Height=IniFile->ReadInteger(section,"StatsFormHeight"
    	,StatsForm->Height);

   	ClientForm->Top=IniFile->ReadInteger(section,"ClientFormTop"
    	,ClientForm->Top);
   	ClientForm->Left=IniFile->ReadInteger(section,"ClientFormLeft"
    	,ClientForm->Left);
   	ClientForm->Width=IniFile->ReadInteger(section,"ClientFormWidth"
    	,ClientForm->Width);
   	ClientForm->Height=IniFile->ReadInteger(section,"ClientFormHeight"
    	,ClientForm->Height);

    for(int i=0;i<ClientForm->ListView->Columns->Count;i++) {
        char str[128];
        sprintf(str,"ClientListColumn%dWidth",i);
        if(IniFile->ValueExists(section,str))
            ClientForm->ListView->Columns->Items[i]->Width
                =IniFile->ReadInteger(section,str,0);
    }

   	Toolbar->Visible=IniFile->ReadBool(section,"ToolbarVisible"
    	,Toolbar->Visible);
    ViewToolbarMenuItem->Checked=Toolbar->Visible;
    ViewStatusBarMenuItem->Checked=StatusBar->Visible;

   	LoginCommand=IniFile->ReadString(section,"LoginCommand"
    	,LoginCommand);
   	ConfigCommand=IniFile->ReadString(section,"ConfigCommand"
    	,ConfigCommand);
   	Password=IniFile->ReadString(section,"Password"
    	,Password);
   	MinimizeToSysTray=IniFile->ReadBool(section,"MinimizeToSysTray"
    	,MinimizeToSysTray);
   	NodeDisplayInterval=IniFile->ReadInteger(section,"NodeDisplayInterval"
    	,NodeDisplayInterval);
   	ClientDisplayInterval=IniFile->ReadInteger(section,"ClientDisplayInterval"
    	,ClientDisplayInterval);
    SemFileCheckFrequency=IniFile->ReadInteger(section,"SemFileCheckFrequency"
    	,SemFileCheckFrequency);

   	MailLogFile=IniFile->ReadInteger(section,"MailLogFile",true);
   	FtpLogFile=IniFile->ReadInteger(section,"FtpLogFile",true);

    delete IniFile;

    Application->MessageBox(AnsiString("Successfully imported settings from "
    	+ OpenDialog->FileName).c_str(),"Successful Import",MB_OK);
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::ExportSettings(TObject* Sender)
{
	char str[128];

    SaveDialog->Filter="Settings files (*.ini)|*.ini|All files|*.*";
    SaveDialog->FileName=CtrlDirectory+"sbbs.ini";
    if(!SaveDialog->Execute())
    	return;

	TMemIniFile* IniFile=new TMemIniFile(SaveDialog->FileName);

	StatusBar->Panels->Items[4]->Text="Exporting Settings...";

    NodeForm->Timer->Interval=NodeDisplayInterval*1000;
    ClientForm->Timer->Interval=ClientDisplayInterval*1000;

    const char* section = "SBBSCTRL::Settings";

    IniFile->WriteString(section,"LoginCommand",LoginCommand);
    IniFile->WriteString(section,"ConfigCommand",ConfigCommand);
    IniFile->WriteString(section,"Password",Password);
    IniFile->WriteBool(section,"MinimizeToSysTray",MinimizeToSysTray);
    IniFile->WriteBool(section,"UndockableForms",UndockableForms);

    section = "SBBSCTRL::MainForm";
    IniFile->WriteInteger(section,"Top",Top);
    IniFile->WriteInteger(section,"Left",Left);
    IniFile->WriteInteger(section,"Height",Height);
    IniFile->WriteInteger(section,"Width",Width);

    IniFile->WriteInteger(section,"TopPanelHeight",TopPanel->Height);
 	IniFile->WriteInteger(section,"UpperLeftPageControlWidth"
    	,UpperLeftPageControl->Width);
    IniFile->WriteInteger(section,"LowerLeftPageControlWidth"
    	,LowerLeftPageControl->Width);
    IniFile->WriteBool(section,"ToolBarVisible",Toolbar->Visible);
    IniFile->WriteBool(section,"StatusBarVisible",StatusBar->Visible);

    section = "SBBSCTRL::NodeForm";
    IniFile->WriteInteger(section,"Top",NodeForm->Top);
    IniFile->WriteInteger(section,"Left",NodeForm->Left);
    IniFile->WriteInteger(section,"Height",NodeForm->Height);
    IniFile->WriteInteger(section,"Width",NodeForm->Width);
    IniFile->WriteInteger(section,"Page"
    	,PageNum((TPageControl*)NodeForm->HostDockSite));
    IniFile->WriteBool(section,"Floating",NodeForm->Floating);
    IniFile->WriteInteger(section,"DisplayInterval",NodeDisplayInterval);

    section = "SBBSCTRL::StatsForm";
    IniFile->WriteInteger(section,"Top",StatsForm->Top);
    IniFile->WriteInteger(section,"Left",StatsForm->Left);
    IniFile->WriteInteger(section,"Height",StatsForm->Height);
    IniFile->WriteInteger(section,"Width",StatsForm->Width);
    IniFile->WriteInteger(section,"Page"
    	,PageNum((TPageControl*)StatsForm->HostDockSite));
    IniFile->WriteBool(section,"Floating",StatsForm->Floating);

    section = "SBBSCTRL::ClientForm";
    IniFile->WriteInteger(section,"Top",ClientForm->Top);
    IniFile->WriteInteger(section,"Left",ClientForm->Left);
    IniFile->WriteInteger(section,"Height",ClientForm->Height);
    IniFile->WriteInteger(section,"Width",ClientForm->Width);
    IniFile->WriteInteger(section,"Page"
    	,PageNum((TPageControl*)ClientForm->HostDockSite));
    IniFile->WriteBool(section,"Floating",ClientForm->Floating);
    IniFile->WriteInteger(section,"DisplayInterval",ClientDisplayInterval);

    for(int i=0;i<ClientForm->ListView->Columns->Count;i++) {
        char str[128];
        sprintf(str,"Column%dWidth",i);
        IniFile->WriteInteger(section,str
            ,ClientForm->ListView->Columns->Items[i]->Width);
    }

    section = "SBBSCTRL::TelnetForm";
    IniFile->WriteInteger(section,"Top",TelnetForm->Top);
    IniFile->WriteInteger(section,"Left",TelnetForm->Left);
    IniFile->WriteInteger(section,"Height",TelnetForm->Height);
    IniFile->WriteInteger(section,"Width",TelnetForm->Width);
    IniFile->WriteInteger(section,"Page"
	    ,PageNum((TPageControl*)TelnetForm->HostDockSite));
    IniFile->WriteBool(section,"Floating",TelnetForm->Floating);

    section = "SBBSCTRL::EventForm";
    IniFile->WriteInteger(section,"Top",EventsForm->Top);
    IniFile->WriteInteger(section,"Left",EventsForm->Left);
    IniFile->WriteInteger(section,"Height",EventsForm->Height);
    IniFile->WriteInteger(section,"Width",EventsForm->Width);
    IniFile->WriteInteger(section,"Page"
	    ,PageNum((TPageControl*)EventsForm->HostDockSite));
    IniFile->WriteBool(section,"Floating",EventsForm->Floating);

    section = "SBBSCTRL::ServicesForm";
    IniFile->WriteInteger(section,"Top",ServicesForm->Top);
    IniFile->WriteInteger(section,"Left",ServicesForm->Left);
    IniFile->WriteInteger(section,"Height",ServicesForm->Height);
    IniFile->WriteInteger(section,"Width",ServicesForm->Width);
    IniFile->WriteInteger(section,"Page"
	    ,PageNum((TPageControl*)ServicesForm->HostDockSite));
    IniFile->WriteBool(section,"Floating",ServicesForm->Floating);

    section = "SBBSCTRL::FtpForm";
    IniFile->WriteInteger(section,"Top",FtpForm->Top);
    IniFile->WriteInteger(section,"Left",FtpForm->Left);
    IniFile->WriteInteger(section,"Height",FtpForm->Height);
    IniFile->WriteInteger(section,"Width",FtpForm->Width);
    IniFile->WriteInteger(section,"Page"
    	,PageNum((TPageControl*)FtpForm->HostDockSite));
    IniFile->WriteBool(section,"Floating",FtpForm->Floating);

    section = "SBBSCTRL::MailForm";
    IniFile->WriteInteger(section,"Top",MailForm->Top);
    IniFile->WriteInteger(section,"Left",MailForm->Left);
    IniFile->WriteInteger(section,"Height",MailForm->Height);
    IniFile->WriteInteger(section,"Width",MailForm->Width);
    IniFile->WriteInteger(section,"Page"
    	,PageNum((TPageControl*)MailForm->HostDockSite));
    IniFile->WriteBool(section,"Floating",MailForm->Floating);

    section = "SBBSCTRL::SpyTerminal";
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

#if 0
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
    WriteColor(Registry,"NodeList",NodeForm->ListBox->Color);
    WriteFont("NodeList",NodeForm->ListBox->Font);
    WriteColor(Registry,"ClientList",ClientForm->ListView->Color);
    WriteFont("ClientList",ClientForm->ListView->Font);
#endif

	/***********************************************************************/
    section = "Global";
    IniFile->WriteString(section,"Hostname",Hostname);
    IniFile->WriteString(section,"CtrlDirectory",CtrlDirectory);
    IniFile->WriteString(section,"TempDirectory",TempDirectory);
    IniFile->WriteInteger(section,"JavaScriptMaxBytes",JS_MaxBytes);

    /***********************************************************************/
	section = "BBS";
    IniFile->WriteInteger(section,"AutoStart",SysAutoStart);
    IniFile->WriteInteger(section,"TelnetInterface",bbs_startup.telnet_interface);
    IniFile->WriteInteger(section,"RLoginInterface",bbs_startup.rlogin_interface);

	IniFile->WriteInteger(section,"TelnetPort",bbs_startup.telnet_port);
	IniFile->WriteInteger(section,"RLoginPort",bbs_startup.rlogin_port);
    IniFile->WriteInteger(section,"FirstNode",bbs_startup.first_node);
    IniFile->WriteInteger(section,"LastNode",bbs_startup.last_node);

    IniFile->WriteInteger(section,"ExternalYield",bbs_startup.xtrn_polls_before_yield);
    IniFile->WriteString(section,"AnswerSound",bbs_startup.answer_sound);
    IniFile->WriteString(section,"HangupSound",bbs_startup.hangup_sound);

    sprintf(str,"0x%x",bbs_startup.options);
    IniFile->WriteString(section,"Options",str);

    /***********************************************************************/
    section = "Mail";
    IniFile->WriteInteger(section,"AutoStart",MailAutoStart);
    IniFile->WriteInteger(section,"LogFile",MailLogFile);
    IniFile->WriteInteger(section,"MaxClients",mail_startup.max_clients);
    IniFile->WriteInteger(section,"MaxInactivity",mail_startup.max_inactivity);
    IniFile->WriteInteger(section,"Interface",mail_startup.interface_addr);
    IniFile->WriteInteger(section,"MaxDeliveryAttempts"
        ,mail_startup.max_delivery_attempts);
    IniFile->WriteInteger(section,"RescanFrequency"
        ,mail_startup.rescan_frequency);
    IniFile->WriteInteger(section,"LinesPerYield"
        ,mail_startup.lines_per_yield);
    IniFile->WriteInteger(section,"MaxRecipients"
        ,mail_startup.max_recipients);
    IniFile->WriteInteger(section,"MaxMsgSize"
        ,mail_startup.max_msg_size);

    IniFile->WriteInteger(section,"SMTPPort",mail_startup.smtp_port);
    IniFile->WriteInteger(section,"POP3Port",mail_startup.pop3_port);

    IniFile->WriteString(section,"DefaultUser",mail_startup.default_user);
	IniFile->WriteString(section,"DNSBlacklistHeader"
    	,mail_startup.dnsbl_hdr);
	IniFile->WriteString(section,"DNSBlacklistSubject"
    	,mail_startup.dnsbl_tag);

    IniFile->WriteString(section,"RelayServer",mail_startup.relay_server);
    IniFile->WriteInteger(section,"RelayPort",mail_startup.relay_port);
    IniFile->WriteString(section,"DNSServer",mail_startup.dns_server);

    IniFile->WriteString(section,"POP3Sound",mail_startup.pop3_sound);
    IniFile->WriteString(section,"InboundSound",mail_startup.inbound_sound);
    IniFile->WriteString(section,"OutboundSound",mail_startup.outbound_sound);

	sprintf(str,"0x%x",mail_startup.options);
    IniFile->WriteString(section,"Options",str);

    /***********************************************************************/
	section = "FTP";
    IniFile->WriteInteger(section,"AutoStart",FtpAutoStart);
    IniFile->WriteInteger(section,"LogFile",FtpLogFile);
	IniFile->WriteInteger(section,"Port",ftp_startup.port);
    IniFile->WriteInteger(section,"MaxClients",ftp_startup.max_clients);
    IniFile->WriteInteger(section,"MaxInactivity",ftp_startup.max_inactivity);
    IniFile->WriteInteger(section,"QwkTimeout",ftp_startup.qwk_timeout);
    IniFile->WriteInteger(section,"Interface",ftp_startup.interface_addr);
    IniFile->WriteString(section,"AnswerSound",ftp_startup.answer_sound);
    IniFile->WriteString(section,"HangupSound",ftp_startup.hangup_sound);
    IniFile->WriteString(section,"HackAttemptSound",ftp_startup.hack_sound);

    IniFile->WriteString(section,"IndexFileName",ftp_startup.index_file_name);
    IniFile->WriteString(section,"HtmlIndexFile",ftp_startup.html_index_file);
    IniFile->WriteString(section,"HtmlIndexScript",ftp_startup.html_index_script);

    sprintf(str,"0x%x",ftp_startup.options);
    IniFile->WriteString(section,"Options",str);

    /***********************************************************************/
    section = "Services";
    IniFile->WriteInteger(section,"AutoStart",ServicesAutoStart);
    IniFile->WriteInteger(section,"Interface",services_startup.interface_addr);

    IniFile->WriteString(section,"AnswerSound",services_startup.answer_sound);
    IniFile->WriteString(section,"HangupSound",services_startup.hangup_sound);

    sprintf(str,"0x%x",services_startup.options);
    IniFile->WriteString(section,"Options",str);

    IniFile->UpdateFile();

    delete IniFile;

    Application->MessageBox(AnsiString("Successfully exported settings to "
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



void __fastcall TMainForm::SoundToggleExecute(TObject *Sender)
{
    SoundToggle->Checked=!SoundToggle->Checked;
    if(!SoundToggle->Checked) {
	    bbs_startup.options|=BBS_OPT_MUTE;
	    ftp_startup.options|=FTP_OPT_MUTE;
	    mail_startup.options|=MAIL_OPT_MUTE;
	} else {
	    bbs_startup.options&=~BBS_OPT_MUTE;
	    ftp_startup.options&=~FTP_OPT_MUTE;
	    mail_startup.options&=~MAIL_OPT_MUTE;
    }
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

	Application->CreateForm(__classid(TCodeInputForm), &CodeInputForm);
	CodeInputForm->Label->Caption="Event Internal Code";
    CodeInputForm->ComboBox->Items->Clear();
    for(i=0;i<cfg.total_events;i++)
    	CodeInputForm->ComboBox->Items->Add(
            AnsiString(cfg.event[i]->code).UpperCase());
    CodeInputForm->ComboBox->ItemIndex=0;
    if(CodeInputForm->ShowModal()==mrOk
       	&& CodeInputForm->ComboBox->Text.Length()) {
        for(i=0;i<cfg.total_events;i++) {
			if(!stricmp(CodeInputForm->ComboBox->Text.c_str(),cfg.event[i]->code)) {
				sprintf(str,"%s%s.now",cfg.data_dir,cfg.event[i]->code);
            	if((file=_sopen(str,O_CREAT|O_TRUNC|O_WRONLY
	                ,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
	                close(file);
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
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption=((TMenuItem*)Sender)->Caption;
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
}

//---------------------------------------------------------------------------
void __fastcall TMainForm::CtrlMenuItemEditClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%s%s"
    	,MainForm->cfg.ctrl_dir
        ,((TMenuItem*)Sender)->Hint.c_str());
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
   	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption=((TMenuItem*)Sender)->Caption;
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;

}
void __fastcall TMainForm::DataMenuItemClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%s%s"
    	,MainForm->cfg.data_dir
        ,((TMenuItem*)Sender)->Hint.c_str());
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
   	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption=((TMenuItem*)Sender)->Caption;
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------

void __fastcall TMainForm::UpTimerTick(TObject *Sender)
{
	char    str[128];
    char    days[64];
    static  time_t start;
    ulong   up;

    if(!start)
        start=time(NULL);
    up=time(NULL)-start;

    days[0]=0;
    if((up/(24*60*60))>=2) {
        sprintf(days,"%u days ",up/(24*60*60));
        up%=(24*60*60);
    }
    sprintf(str,"Up: %s%u:%02u"
        ,days
        ,up/(60*60)
        ,(up/60)%60
        );
    AnsiString Str=AnsiString(str);
    if(MainForm->StatusBar->Panels->Items[4]->Text!=Str)
		MainForm->StatusBar->Panels->Items[4]->Text=Str;
#if 0
    THeapStatus hp=GetHeapStatus();
    sprintf(str,"Mem Used: %lu bytes",hp.TotalAllocated);
    Str=AnsiString(str);
    if(MainForm->StatusBar->Panels->Items[5]->Text!=Str)
		MainForm->StatusBar->Panels->Items[5]->Text=Str;
#endif
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

void __fastcall TMainForm::BBSViewErrorLogMenuItemClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%sERROR.LOG"
    	,MainForm->cfg.logs_dir);
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
    TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption="Error Log";
    TextFileEditForm->Memo->ReadOnly=true;
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ChatToggleExecute(TObject *Sender)
{
    ChatToggle->Checked=!ChatToggle->Checked;
    if(ChatToggle->Checked)
	    bbs_startup.options|=BBS_OPT_SYSOP_AVAILABLE;
    else
        bbs_startup.options&=~BBS_OPT_SYSOP_AVAILABLE;

}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ViewClientsExecute(TObject *Sender)
{
	ClientForm->Visible=!ClientForm->Visible;
    ViewClients->Checked=ClientForm->Visible;
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
    mail_lputs(NULL);
    ftp_lputs(NULL);

    sprintf(filename,"%sLOGS\\%s%02d%02d%02d.LOG"
    	,MainForm->cfg.logs_dir
        ,((TMenuItem*)Sender)->Hint.c_str()
        ,tm->tm_mon+1
        ,tm->tm_mday
        ,tm->tm_year%100
        );
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption=((TMenuItem*)Sender)->Caption;
    TextFileEditForm->Memo->ReadOnly=true;
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
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
    static inside;
    if(inside) return;
    inside=true;

    Application->CreateForm(__classid(TPropertiesDlg), &PropertiesDlg);
    PropertiesDlg->LoginCmdEdit->Text=LoginCommand;
    PropertiesDlg->ConfigCmdEdit->Text=ConfigCommand;
    PropertiesDlg->HostnameEdit->Text=Hostname;
    PropertiesDlg->CtrlDirEdit->Text=CtrlDirectory;
    PropertiesDlg->TempDirEdit->Text=TempDirectory;    
    PropertiesDlg->NodeIntUpDown->Position=NodeDisplayInterval;
    PropertiesDlg->ClientIntUpDown->Position=ClientDisplayInterval;
    PropertiesDlg->SemFreqUpDown->Position=SemFileCheckFrequency;
    PropertiesDlg->TrayIconCheckBox->Checked=MinimizeToSysTray;
    PropertiesDlg->UndockableCheckBox->Checked=UndockableForms;
    PropertiesDlg->PasswordEdit->Text=Password;
    PropertiesDlg->JS_MaxBytesEdit->Text=IntToStr(JS_MaxBytes);
    if(MaxLogLen==0)
		PropertiesDlg->MaxLogLenEdit->Text="<unlimited>";
    else
	    PropertiesDlg->MaxLogLenEdit->Text=IntToStr(MaxLogLen);
	if(PropertiesDlg->ShowModal()==mrOk) {
        LoginCommand=PropertiesDlg->LoginCmdEdit->Text;
        ConfigCommand=PropertiesDlg->ConfigCmdEdit->Text;
        Hostname=PropertiesDlg->HostnameEdit->Text;
        CtrlDirectory=PropertiesDlg->CtrlDirEdit->Text;
        TempDirectory=PropertiesDlg->TempDirEdit->Text;
        Password=PropertiesDlg->PasswordEdit->Text;
        NodeDisplayInterval=PropertiesDlg->NodeIntUpDown->Position;
        ClientDisplayInterval=PropertiesDlg->ClientIntUpDown->Position;
        SemFileCheckFrequency=PropertiesDlg->SemFreqUpDown->Position;
        MinimizeToSysTray=PropertiesDlg->TrayIconCheckBox->Checked;
        UndockableForms=PropertiesDlg->UndockableCheckBox->Checked;
        JS_MaxBytes
        	=PropertiesDlg->JS_MaxBytesEdit->Text.ToIntDef(JAVASCRIPT_MAX_BYTES);
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
        ReloadConfigExecute(Sender);
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

void __fastcall TMainForm::ReloadConfigExecute(TObject *Sender)
{
	FtpRecycleExecute(Sender);
	MailRecycleExecute(Sender);
	ServicesRecycleExecute(Sender);

	char error[256];
	SAFECOPY(error,UNKNOWN_LOAD_ERROR);
	if(!load_cfg(&cfg, NULL, TRUE, error)) {
    	Application->MessageBox(error,"ERROR Re-loading Configuration"
	        ,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
    }

    node_t node;
    for(int i=0;i<cfg.sys_nodes;i++) {
    	int file;
       	if(NodeForm->getnodedat(i+1,&node,&file))
            break;
        node.misc|=NODE_RRUN;
        if(NodeForm->putnodedat(i+1,&node,file))
            break;
    }
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
//---------------------------------------------------------------------------


void __fastcall TMainForm::MailRecycleExecute(TObject *Sender)
{
	mail_startup.recycle_now=true;
    MailRecycle->Enabled=false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FtpRecycleExecute(TObject *Sender)
{
	ftp_startup.recycle_now=true;
    FtpRecycle->Enabled=false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::ServicesRecycleExecute(TObject *Sender)
{
	services_startup.recycle_now=true;
    ServicesRecycle->Enabled=false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::TelnetRecycleExecute(TObject *Sender)
{
	bbs_startup.recycle_now=true;
    TelnetRecycle->Enabled=false;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FileEditTextFilesClick(TObject *Sender)
{
	TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

    dlg->Options << ofNoChangeDir;
    dlg->Filter = 	"Text files (*.txt)|*.TXT"
    				"|All files|*.*";
    dlg->InitialDir=cfg.text_dir;
    if(dlg->Execute()==true) {
        Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
        TextFileEditForm->Filename=AnsiString(dlg->FileName);
        TextFileEditForm->Caption="Edit";
        TextFileEditForm->ShowModal();
        delete TextFileEditForm;
    }
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
    if(dlg->Execute()==true) {
        Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
        TextFileEditForm->Filename=AnsiString(dlg->FileName);
        TextFileEditForm->Caption="Edit";
        TextFileEditForm->ShowModal();
        delete TextFileEditForm;
    }
    delete dlg;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::FileEditConfigFilesClick(TObject *Sender)
{
	TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

    dlg->Options << ofNoChangeDir;
    dlg->Filter = "Configuration Files (*.cfg)|*.CFG";
    dlg->InitialDir=cfg.ctrl_dir;
    if(dlg->Execute()==true) {
        Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
        TextFileEditForm->Filename=AnsiString(dlg->FileName);
        TextFileEditForm->Caption="Edit";
        TextFileEditForm->ShowModal();
        delete TextFileEditForm;
    }
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

