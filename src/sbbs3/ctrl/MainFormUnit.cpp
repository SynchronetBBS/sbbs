/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

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

//---------------------------------------------------------------------------
#include <vcl.h>
#include <vcl\Registry.hpp>	/* TRegistry */
#pragma hdrstop
#include <winsock.h>		// IPPORT_TELNET, INADDR_ANY
#include <process.h> 		// _beginthread()
#include <io.h>
#include <stdio.h>
#include <sys\stat.h>
#include <sys\locking.h>
#include <fcntl.h>
#include <share.h>

#include "MainFormUnit.h"
#include "TelnetFormUnit.h"
#include "EventsFormUnit.h"
#include "FtpFormUnit.h"
#include "MailFormUnit.h"
#include "NodeFormUnit.h"
#include "StatsFormUnit.h"
#include "ClientFormUnit.h"
#include "CtrlPathDialogUnit.h"
#include "TelnetCfgDlgUnit.h"
#include "MailCfgDlgUnit.h"
#include "FtpCfgDlgUnit.h"
#include "AboutBoxFormUnit.h"
#include "CodeInputFormUnit.h"
#include "TextFileEditUnit.h"
#include "UserListFormUnit.h"
#include "PropertiesDlgUnit.h"
#include "ConfigWizardUnit.h"

#include "sbbs.h"           // unixtodstr()
#include "userdat.h"		// lastuser()

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "Trayicon"
#pragma resource "*.dfm"
TMainForm *MainForm;

#define MAX_LOGLEN 20000

#define LOG_TIME_FMT "  m/d  hh:mm:ssa/p"

int     threads=1;

static void thread_up(BOOL up)
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
	static  HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);
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
    ReleaseMutex(mutex);
}

static void client_on(BOOL on, int sock, client_t* client)
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
    if(i>=ClientForm->ListView->Items->Count)
        i=-1;

    if(on==FALSE) { // Off
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

    while(TelnetForm->Log->Text.Length()>=MAX_LOGLEN)
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

    if(clients>save_clients)
        client_add(TRUE);
    else if(clients<save_clients)
        client_add(FALSE);
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
    Application->ProcessMessages();
}
static void bbs_started(void)
{
	Screen->Cursor=crDefault;
	MainForm->TelnetStart->Enabled=false;
    MainForm->TelnetStop->Enabled=true;
    Application->ProcessMessages();
}
static void bbs_start(void)
{
	Screen->Cursor=crAppStart;
    bbs_status("Starting");
    strcpy(MainForm->bbs_startup.ctrl_dir,MainForm->CtrlDirectory.c_str());
	_beginthread((void(*)(void*))bbs_thread,0,&MainForm->bbs_startup);
    Application->ProcessMessages();
}

static int event_log(char *str)
{
	static HANDLE mutex;

    if(!mutex)
    	mutex=CreateMutex(NULL,false,NULL);
	WaitForSingleObject(mutex,INFINITE);

    while(EventsForm->Log->Text.Length()>=MAX_LOGLEN)
        EventsForm->Log->Lines->Delete(0);

    AnsiString Line=Now().FormatString(LOG_TIME_FMT)+"  ";
    Line+=AnsiString(str).Trim();
	EventsForm->Log->Lines->Add(Line);
    ReleaseMutex(mutex);
    return(Line.Length());
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

    while(MailForm->Log->Text.Length()>=MAX_LOGLEN)
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
            LogStream=fopen(LogFileName.c_str(),"a");

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
    static save_clients;

    if(clients>save_clients)
        client_add(TRUE);
    else if(clients<save_clients)
        client_add(FALSE);
    save_clients=clients;

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
    Application->ProcessMessages();
}
static void mail_started(void)
{
	Screen->Cursor=crDefault;
	MainForm->MailStart->Enabled=false;
    MainForm->MailStop->Enabled=true;
    Application->ProcessMessages();
}
static void mail_start(void)
{
	Screen->Cursor=crAppStart;
    mail_status("Starting");
    strcpy(MainForm->mail_startup.ctrl_dir,MainForm->CtrlDirectory.c_str());
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

    while(FtpForm->Log->Text.Length()>=MAX_LOGLEN)
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
            LogStream=fopen(LogFileName.c_str(),"a");

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
    static save_clients;

    if(clients>save_clients)
        client_add(TRUE);
    else if(clients<save_clients)
        client_add(FALSE);
    save_clients=clients;

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
    Application->ProcessMessages();
}
static void ftp_started(void)
{
	Screen->Cursor=crDefault;
	MainForm->FtpStart->Enabled=false;
    MainForm->FtpStop->Enabled=true;
    Application->ProcessMessages();
}
static void ftp_start(void)
{
	Screen->Cursor=crAppStart;
    ftp_status("Starting");
    strcpy(MainForm->ftp_startup.ctrl_dir,MainForm->CtrlDirectory.c_str());
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
    ConfigCommand="%sSCFG %s /T2";
    MinimizeToSysTray=false;
    NodeDisplayInterval=1;  /* seconds */
    ClientDisplayInterval=5;    /* seconds */
    Initialized=false;
    FirstRun=false;

    char* p;
    if((p=getenv("SBBSCTRL"))!=NULL)
        CtrlDirectory=p;

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
	ftp_startup.options=FTP_OPT_INDEX_FILE|FTP_OPT_ALLOW_QWK;
    strcpy(ftp_startup.index_file_name,"00index");

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
    if(bbs_ver < (0x300<<8) || bbs_ver > (0x399<<8)) {
        char str[128];
        sprintf(str,"Incorrect SBBS.DLL Version (%lX)",bbs_ver);
    	Application->MessageBox(str,"ERROR",MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
    }

	if(putenv("TZ=UCT0")) {
    	Application->MessageBox("Error settings timezone"
        	,"ERROR",MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
    }
	tzset();

    // Read Registry keys
	TRegistry* Registry=new TRegistry;
    if(!Registry->OpenKey(REG_KEY,true)) {
    	Application->MessageBox("Error opening registry key"
        	,REG_KEY,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
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

    if(Initialized) /* Don't overwrite registry settings with defaults */
        SaveSettings(Sender);

	StatusBar->Panels->Items[4]->Text="Closing...";
    time_t start=time(NULL);
	while(TelnetStop->Enabled || MailStop->Enabled || FtpStop->Enabled) {
        if(time(NULL)-start>30)
            break;
        Application->ProcessMessages();
        Sleep(1);
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
#if 0 // Moved to FormClose()
    time_t start=time(NULL);
	while(TelnetStop->Enabled || MailStop->Enabled || FtpStop->Enabled) {
        if(time(NULL)-start>15)
            break;
        Application->ProcessMessages();
        Sleep(1);
    }
#endif    

    CanClose=true;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::TelnetStartExecute(TObject *Sender)
{
	bbs_start();
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
	Application->CreateForm(__classid(TTelnetCfgDlg), &TelnetCfgDlg);
	TelnetCfgDlg->ShowModal();
    delete TelnetCfgDlg;
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
	Application->CreateForm(__classid(TMailCfgDlg), &MailCfgDlg);
	MailCfgDlg->ShowModal();
    delete MailCfgDlg;
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
	Application->CreateForm(__classid(TFtpCfgDlg), &FtpCfgDlg);
	FtpCfgDlg->ShowModal();
    delete FtpCfgDlg;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CloseTimerTimer(TObject *Sender)
{
	if(!TelnetStop->Enabled && !MailStop->Enabled && !FtpStop->Enabled)
   	    Close();
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
	CreateProcess(
		NULL,			// pointer to name of executable module
		str,  			// pointer to command line string
		NULL,  			// process security attributes
		NULL,   		// thread security attributes
		TRUE,  			// handle inheritance flag
		CREATE_NEW_CONSOLE|CREATE_SEPARATE_WOW_VDM, // creation flags
        NULL,  			// pointer to new environment block
		cfg.ctrl_dir,	// pointer to current directory name
		&startup_info,  // pointer to STARTUPINFO
		&process_info  	// pointer to PROCESS_INFORMATION
		);
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


#define STAT_DESC_LEN 23

void __fastcall TMainForm::StatsTimerTick(TObject *Sender)
{
	char 	str[128];
	int 	i;
	static stats_t stats;

    getstats(&cfg,0,&stats);

	StatsForm->TotalLogons->Caption=AnsiString(stats.logons);
    StatsForm->LogonsToday->Caption=AnsiString(stats.ltoday);
    StatsForm->TotalTimeOn->Caption=AnsiString(stats.timeon);
    StatsForm->TimeToday->Caption=AnsiString(stats.ttoday);
    StatsForm->TotalEMail->Caption=AnsiString(getmail(&cfg,0,0));
	StatsForm->EMailToday->Caption=AnsiString(stats.etoday);
	StatsForm->TotalFeedback->Caption=AnsiString(getmail(&cfg,1,0));
	StatsForm->FeedbackToday->Caption=AnsiString(stats.ftoday);
    StatsForm->TotalUsers->Caption=AnsiString(lastuser(&cfg));
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
void __fastcall TMainForm::StartupTimerTick(TObject *Sender)
{
    bool	TelnetFormFloating=false;
    bool	EventsFormFloating=false;
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

    AnsiString	Str;

    delete StartupTimer;

    // Read Registry keys
	TRegistry* Registry=new TRegistry;
    if(!Registry->OpenKey(REG_KEY,true)) {
    	Application->MessageBox("Error opening registry key"
        	,REG_KEY,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
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

    if(Registry->ValueExists("TelnetFormFloating"))
    	TelnetFormFloating=Registry->ReadBool("TelnetFormFloating");
    if(Registry->ValueExists("EventsFormFloating"))
    	EventsFormFloating=Registry->ReadBool("EventsFormFloating");
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

    if(Registry->ValueExists("TelnetFormPage"))
    	TelnetFormPage=Registry->ReadInteger("TelnetFormPage");
    if(Registry->ValueExists("EventsFormPage"))
    	EventsFormPage=Registry->ReadInteger("EventsFormPage");
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

    ViewToolbarMenuItem->Checked=Toolbar->Visible;
    ViewStatusBarMenuItem->Checked=StatusBar->Visible;

    if(Registry->ValueExists("CtrlDirectory"))
    	CtrlDirectory=Registry->ReadString("CtrlDirectory");
    if(Registry->ValueExists("LoginCommand"))
    	LoginCommand=Registry->ReadString("LoginCommand");
    if(Registry->ValueExists("ConfigCommand"))
    	ConfigCommand=Registry->ReadString("ConfigCommand");
    if(Registry->ValueExists("MinimizeToSysTray"))
    	MinimizeToSysTray=Registry->ReadBool("MinimizeToSysTray");
	if(Registry->ValueExists("NodeDisplayInterval"))
    	NodeDisplayInterval=Registry->ReadInteger("NodeDisplayInterval");
	if(Registry->ValueExists("ClientDisplayInterval"))
    	ClientDisplayInterval=Registry->ReadInteger("ClientDisplayInterval");

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

    if(Registry->ValueExists("AnswerSound"))
    	sprintf(bbs_startup.answer_sound,"%.*s"
        	,sizeof(bbs_startup.answer_sound)-1
        	,Registry->ReadString("AnswerSound").c_str());
    else
        FirstRun=true;

    if(Registry->ValueExists("HangupSound"))
    	sprintf(bbs_startup.hangup_sound,"%.*s"
        	,sizeof(bbs_startup.hangup_sound)-1
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

    if(Registry->ValueExists("MailSMTPPort"))
    	mail_startup.smtp_port=Registry->ReadInteger("MailSMTPPort");

    if(Registry->ValueExists("MailPOP3Port"))
    	mail_startup.pop3_port=Registry->ReadInteger("MailPOP3Port");

    if(Registry->ValueExists("MailRelayServer"))
        sprintf(mail_startup.relay_server,"%.*s"
            ,sizeof(mail_startup.relay_server)-1
            ,Registry->ReadString("MailRelayServer").c_str());

    if(Registry->ValueExists("MailDNSServer"))
        sprintf(mail_startup.dns_server,"%.*s"
            ,sizeof(mail_startup.dns_server)-1
            ,Registry->ReadString("MailDNSServer").c_str());

    if(Registry->ValueExists("MailInboundSound"))
    	sprintf(mail_startup.inbound_sound,"%.*s"
        	,sizeof(mail_startup.inbound_sound)-1
        	,Registry->ReadString("MailInboundSound").c_str());

    if(Registry->ValueExists("MailOutboundSound"))
    	sprintf(mail_startup.outbound_sound,"%.*s"
        	,sizeof(mail_startup.outbound_sound)-1
        	,Registry->ReadString("MailOutboundSound").c_str());

    if(Registry->ValueExists("MailPOP3Sound"))
    	sprintf(mail_startup.pop3_sound,"%.*s"
        	,sizeof(mail_startup.pop3_sound)-1
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
    	sprintf(ftp_startup.answer_sound,"%.*s"
        	,sizeof(ftp_startup.answer_sound)-1
        	,Registry->ReadString("FtpAnswerSound").c_str());

    if(Registry->ValueExists("FtpHangupSound"))
    	sprintf(ftp_startup.hangup_sound,"%.*s"
        	,sizeof(ftp_startup.hangup_sound)-1
        	,Registry->ReadString("FtpHangupSound").c_str());

    if(Registry->ValueExists("FtpHackAttemptSound"))
    	sprintf(ftp_startup.hack_sound,"%.*s"
        	,sizeof(ftp_startup.hack_sound)-1
        	,Registry->ReadString("FtpHackAttemptSound").c_str());

    if(Registry->ValueExists("FtpIndexFileName"))
    	sprintf(ftp_startup.index_file_name,"%.*s"
        	,sizeof(ftp_startup.index_file_name)-1
        	,Registry->ReadString("FtpIndexFileName").c_str());

    if(Registry->ValueExists("FtpHtmlIndexFile"))
    	sprintf(ftp_startup.html_index_file,"%.*s"
        	,sizeof(ftp_startup.html_index_file)-1
        	,Registry->ReadString("FtpHtmlIndexFile").c_str());

    if(Registry->ValueExists("FtpHtmlIndexScript"))
    	sprintf(ftp_startup.html_index_script,"%.*s"
        	,sizeof(ftp_startup.html_index_script)-1
        	,Registry->ReadString("FtpHtmlIndexScript").c_str());

    if(Registry->ValueExists("FtpOptions"))
    	ftp_startup.options=Registry->ReadInteger("FtpOptions");

    Registry->CloseKey();
    delete Registry;

    if(!FileExists(CtrlDirectory+"MAIN.CNF")) {
    	Sleep(3000);	// Let 'em see the logo for a bit
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
	if(!load_cfg(&cfg, NULL, TRUE)) {
    	Application->MessageBox("Failed to load configuration files.","ERROR"
	        ,MB_OK|MB_ICONEXCLAMATION);
        Application->Terminate();
        return;
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
	if(!FtpFormFloating)
    	FtpForm->ManualDock(PageControl(FtpFormPage),NULL,alClient);

    NodeForm->Show();
    ClientForm->Show();
    StatsForm->Show();
    TelnetForm->Show();
    EventsForm->Show();
    FtpForm->Show();
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

    if(SysAutoStart) {
        bbs_start();
    }
    if(MailAutoStart) {
        mail_start();
    }
    if(FtpAutoStart) {
        ftp_start();
    }
    NodeForm->Timer->Interval=NodeDisplayInterval*1000;
    NodeForm->Timer->Enabled=true;
    ClientForm->Timer->Interval=ClientDisplayInterval*1000;
    ClientForm->Timer->Enabled=true;

    StatsTimer->Interval=cfg.node_stat_check*1000;
	StatsTimer->Enabled=true;
    Initialized=true;

    if(FirstRun)
        BBSConfigWizardMenuItemClick(Sender);
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

    Registry->WriteInteger("TelnetFormTop",TelnetForm->Top);
    Registry->WriteInteger("TelnetFormLeft",TelnetForm->Left);
    Registry->WriteInteger("TelnetFormHeight",TelnetForm->Height);
    Registry->WriteInteger("TelnetFormWidth",TelnetForm->Width);

    Registry->WriteInteger("EventsFormTop",EventsForm->Top);
    Registry->WriteInteger("EventsFormLeft",EventsForm->Left);
    Registry->WriteInteger("EventsFormHeight",EventsForm->Height);
    Registry->WriteInteger("EventsFormWidth",EventsForm->Width);

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

    Registry->WriteBool("TelnetFormFloating",TelnetForm->Floating);
    Registry->WriteBool("EventsFormFloating",EventsForm->Floating);
    Registry->WriteBool("NodeFormFloating",NodeForm->Floating);
    Registry->WriteBool("StatsFormFloating",StatsForm->Floating);
    Registry->WriteBool("ClientFormFloating",ClientForm->Floating);
    Registry->WriteBool("FtpFormFloating",FtpForm->Floating);
    Registry->WriteBool("MailFormFloating",MailForm->Floating);

    Registry->WriteInteger("TelnetFormPage"
	    ,PageNum((TPageControl*)TelnetForm->HostDockSite));
    Registry->WriteInteger("EventsFormPage"
	    ,PageNum((TPageControl*)EventsForm->HostDockSite));
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

    Registry->WriteBool("ToolBarVisible",Toolbar->Visible);
    Registry->WriteBool("StatusBarVisible",StatusBar->Visible);

    Registry->WriteString("CtrlDirectory",CtrlDirectory);
    Registry->WriteString("LoginCommand",LoginCommand);
    Registry->WriteString("ConfigCommand",ConfigCommand);
    Registry->WriteBool("MinimizeToSysTray",MinimizeToSysTray);
    Registry->WriteInteger("NodeDisplayInterval",NodeDisplayInterval);
    Registry->WriteInteger("ClientDisplayInterval",ClientDisplayInterval);

    Registry->WriteInteger("SysAutoStart",SysAutoStart);
    Registry->WriteInteger("MailAutoStart",MailAutoStart);
    Registry->WriteInteger("FtpAutoStart",FtpAutoStart);
    Registry->WriteInteger("MailLogFile",MailLogFile);
    Registry->WriteInteger("FtpLogFile",FtpLogFile);

    Registry->WriteInteger("TelnetInterface",bbs_startup.telnet_interface);
    Registry->WriteInteger("RLoginInterface",bbs_startup.rlogin_interface);

	Registry->WriteInteger("TelnetPort",bbs_startup.telnet_port);
	Registry->WriteInteger("RLoginPort",bbs_startup.rlogin_port);
    Registry->WriteInteger("FirstNode",bbs_startup.first_node);
    Registry->WriteInteger("LastNode",bbs_startup.last_node);

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

    Registry->WriteInteger("MailSMTPPort",mail_startup.smtp_port);
    Registry->WriteInteger("MailPOP3Port",mail_startup.pop3_port);

    Registry->WriteString("MailRelayServer",mail_startup.relay_server);
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
    if(cfg.total_events)
    	CodeInputForm->Edit->Text=AnsiString(cfg.event[0]->code);
    if(CodeInputForm->ShowModal()==mrOk
       	&& CodeInputForm->Edit->Text.Length()) {
        for(i=0;i<cfg.total_events;i++) {
			if(!stricmp(CodeInputForm->Edit->Text.c_str(),cfg.event[i]->code)) {
				sprintf(str,"%s%s.now",cfg.data_dir,cfg.event[i]->code);
            	if((file=_sopen(str,O_CREAT|O_TRUNC|O_WRONLY
	                ,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
	                close(file);
                break;
	   		}
        }
        if(i>=cfg.total_events)
	    	Application->MessageBox("Event Code Not Found"
	        	,CodeInputForm->Edit->Text.c_str(),MB_OK|MB_ICONEXCLAMATION);
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
    if(cfg.total_qhubs)
    	CodeInputForm->Edit->Text=AnsiString(cfg.qhub[0]->id);
    if(CodeInputForm->ShowModal()==mrOk
    	&& CodeInputForm->Edit->Text.Length()) {
        for(i=0;i<cfg.total_qhubs;i++) {
			if(!stricmp(CodeInputForm->Edit->Text.c_str(),cfg.qhub[i]->id)) {
				sprintf(str,"%sqnet/%s.now",cfg.data_dir,cfg.qhub[i]->id);
            	if((file=_sopen(str,O_CREAT|O_TRUNC|O_WRONLY
                	,SH_DENYRW,S_IREAD|S_IWRITE))!=-1)
	                close(file);
                break;
	   		}
        }
        if(i>=cfg.total_qhubs)
	    	Application->MessageBox("QWKnet Hub ID Not Found"
	        	,CodeInputForm->Edit->Text.c_str(),MB_OK|MB_ICONEXCLAMATION);
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
        if(clients) {
            TrayIcon->IconIndex=(TrayIcon->IconIndex==4) ? 59 : 4;
            NumClients=" ("+AnsiString(clients)+" client";
            if(clients>1)
                NumClients+="s";
            NumClients+=")";
        } else if(TrayIcon->IconIndex!=4)
            TrayIcon->IconIndex=4;
        TrayIcon->Hint=AnsiString(APP_TITLE)+NumClients;
    }
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::BBSViewErrorLogMenuItemClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%sERROR.LOG"
    	,MainForm->cfg.data_dir);
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption="Error Log (See SYSOP.DOC for help)";
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

void __fastcall TMainForm::FileOpenMenuItemClick(TObject *Sender)
{
    TOpenDialog* dlg=new TOpenDialog((TComponent*)Sender);

    dlg->Filter = "Text files (*.txt)|*.TXT"
                "|Log files (*.log)|*.LOG"
                "|Config files (*.cfg)|*.CFG"
                "|Message files (*.msg; *.asc)|*.MSG;*.ASC"
                "|Baja Source (*.src)|*.SRC"
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


void __fastcall TMainForm::BBSLoginMenuItemClick(TObject *Sender)
{
    if(!strnicmp(LoginCommand.c_str(),"start ",6))
        WinExec(LoginCommand.c_str(),SW_SHOWMINNOACTIVE);
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
       	CodeInputForm->Edit->Text=AnsiString(unixtodstr(&cfg,time(NULL),str));
        mr=CodeInputForm->ShowModal();
        t=dstrtounix(&cfg,CodeInputForm->Edit->Text.c_str());
        delete CodeInputForm;
        if(mr!=mrOk)
            return;
    } else {
        t=time(NULL);
        t-=((TMenuItem*)Sender)->Tag*24*60*60;
    }
    tm=gmtime(&t);
    if(tm==NULL)
        return;

    /* Close Mail/FTP logs */
    mail_lputs(NULL);
    ftp_lputs(NULL);

    sprintf(filename,"%sLOGS\\%s%02d%02d%02d.LOG"
    	,MainForm->cfg.data_dir
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
    Application->CreateForm(__classid(TPropertiesDlg), &PropertiesDlg);
    PropertiesDlg->LoginCmdEdit->Text=LoginCommand;
    PropertiesDlg->ConfigCmdEdit->Text=ConfigCommand;
    PropertiesDlg->CtrlDirEdit->Text=CtrlDirectory;
    PropertiesDlg->NodeIntUpDown->Position=NodeDisplayInterval;
    PropertiesDlg->ClientIntUpDown->Position=ClientDisplayInterval;
    PropertiesDlg->TrayIconCheckBox->Checked=MinimizeToSysTray;
	if(PropertiesDlg->ShowModal()==mrOk) {
        LoginCommand=PropertiesDlg->LoginCmdEdit->Text;
        ConfigCommand=PropertiesDlg->ConfigCmdEdit->Text;
        CtrlDirectory=PropertiesDlg->CtrlDirEdit->Text;
        NodeDisplayInterval=PropertiesDlg->NodeIntUpDown->Position;
        ClientDisplayInterval=PropertiesDlg->ClientIntUpDown->Position;
        MinimizeToSysTray=PropertiesDlg->TrayIconCheckBox->Checked;
        SaveSettings(Sender);
    }
    delete PropertiesDlg;
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::CloseTrayMenuItemClick(TObject *Sender)
{
    Close();
}
//---------------------------------------------------------------------------

void __fastcall TMainForm::RestoreTrayMenuItemClick(TObject *Sender)
{
    TrayIcon->Visible=false;
    Application->Restore();
}
//---------------------------------------------------------------------------
void __fastcall TMainForm::BBSConfigWizardMenuItemClick(TObject *Sender)
{
#if 0
    char str[512];

    sprintf(str,"%sSCFGWIZ %s",cfg.exec_dir,cfg.ctrl_dir);
    WinExec(str,SW_SHOWNORMAL);
#else
    TConfigWizard* ConfigWizard;

    Application->CreateForm(__classid(TConfigWizard), &ConfigWizard);
	if(ConfigWizard->ShowModal()==mrOk) {
        SaveSettings(Sender);
    }
    delete ConfigWizard;

#endif
}
//---------------------------------------------------------------------------

