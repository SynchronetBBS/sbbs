/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

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

//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "MainFormUnit.h"
#include "WebCfgDlgUnit.h"
#include <stdio.h>			// sprintf()
#include <mmsystem.h>		// sndPlaySound()
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TWebCfgDlg *WebCfgDlg;
//---------------------------------------------------------------------------
__fastcall TWebCfgDlg::TWebCfgDlg(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TWebCfgDlg::FormShow(TObject *Sender)
{
    char str[128];
    char** p;

    if(MainForm->web_startup.interface_addr==0)
        NetworkInterfaceEdit->Text="<ANY>";
    else {
        sprintf(str,"%d.%d.%d.%d"
            ,(MainForm->web_startup.interface_addr>>24)&0xff
            ,(MainForm->web_startup.interface_addr>>16)&0xff
            ,(MainForm->web_startup.interface_addr>>8)&0xff
            ,MainForm->web_startup.interface_addr&0xff
        );
        NetworkInterfaceEdit->Text=AnsiString(str);
    }
    if(MainForm->web_startup.max_clients==0)
        MaxClientsEdit->Text="infinite";
    else
        MaxClientsEdit->Text=AnsiString((int)MainForm->web_startup.max_clients);
    MaxInactivityEdit->Text=AnsiString((int)MainForm->web_startup.max_inactivity);
	PortEdit->Text=AnsiString((int)MainForm->web_startup.port);
    AutoStartCheckBox->Checked=MainForm->WebAutoStart;

    HtmlRootEdit->Text=AnsiString(MainForm->web_startup.root_dir);
    ErrorSubDirEdit->Text=AnsiString(MainForm->web_startup.error_dir);
    EmbeddedJsExtEdit->Text=AnsiString(MainForm->web_startup.js_ext);
    ServerSideJsExtEdit->Text=AnsiString(MainForm->web_startup.ssjs_ext);

    IndexFileEdit->Text.SetLength(0);
    for(p=MainForm->web_startup.index_file_name;*p;p++) {
        if(p!=MainForm->web_startup.index_file_name)
            IndexFileEdit->Text=IndexFileEdit->Text+",";
        IndexFileEdit->Text=IndexFileEdit->Text+AnsiString(*p);
    }

    AnswerSoundEdit->Text=AnsiString(MainForm->web_startup.answer_sound);
    HangupSoundEdit->Text=AnsiString(MainForm->web_startup.hangup_sound);
    HackAttemptSoundEdit->Text=AnsiString(MainForm->web_startup.hack_sound);

	DebugTxCheckBox->Checked=MainForm->web_startup.options&WEB_OPT_DEBUG_TX;
	DebugRxCheckBox->Checked=MainForm->web_startup.options&WEB_OPT_DEBUG_RX;
    AccessLogCheckBox->Checked=MainForm->web_startup.options&WEB_OPT_HTTP_LOGGING;
    AccessLogCheckBoxClick(Sender);

    VirtualHostsCheckBox->Checked=MainForm->web_startup.options&WEB_OPT_VIRTUAL_HOSTS;

    HostnameCheckBox->Checked
        =!(MainForm->web_startup.options&BBS_OPT_NO_HOST_LOOKUP);

    PageControl->ActivePage=GeneralTabSheet;
}
//---------------------------------------------------------------------------
void __fastcall TWebCfgDlg::OKBtnClick(TObject *Sender)
{
    char    str[128],*p;
    DWORD   addr;

    SAFECOPY(str,NetworkInterfaceEdit->Text.c_str());
    p=str;
    while(*p && *p<=' ') p++;
    if(*p && isdigit(*p)) {
        addr=atoi(p)<<24;
        while(*p && *p!='.') p++;
        if(*p=='.') p++;
        addr|=atoi(p)<<16;
        while(*p && *p!='.') p++;
        if(*p=='.') p++;
        addr|=atoi(p)<<8;
        while(*p && *p!='.') p++;
        if(*p=='.') p++;
        addr|=atoi(p);
        MainForm->web_startup.interface_addr=addr;
    } else
        MainForm->web_startup.interface_addr=0;
    MainForm->web_startup.max_clients=MaxClientsEdit->Text.ToIntDef(10);
    MainForm->web_startup.max_inactivity=MaxInactivityEdit->Text.ToIntDef(300);
    MainForm->web_startup.port=PortEdit->Text.ToIntDef(23);
    MainForm->WebAutoStart=AutoStartCheckBox->Checked;

    SAFECOPY(MainForm->web_startup.root_dir
        ,HtmlRootEdit->Text.c_str());
    SAFECOPY(MainForm->web_startup.error_dir
        ,ErrorSubDirEdit->Text.c_str());
    SAFECOPY(MainForm->web_startup.js_ext
        ,EmbeddedJsExtEdit->Text.c_str());
    SAFECOPY(MainForm->web_startup.ssjs_ext
        ,ServerSideJsExtEdit->Text.c_str());

    strListFree(&MainForm->web_startup.index_file_name);
    strListSplitCopy(&MainForm->web_startup.index_file_name,
        IndexFileEdit->Text.c_str(),",");

    SAFECOPY(MainForm->web_startup.answer_sound
        ,AnswerSoundEdit->Text.c_str());
    SAFECOPY(MainForm->web_startup.hangup_sound
        ,HangupSoundEdit->Text.c_str());
    SAFECOPY(MainForm->web_startup.hack_sound
        ,HackAttemptSoundEdit->Text.c_str());

	if(DebugTxCheckBox->Checked==true)
    	MainForm->web_startup.options|=WEB_OPT_DEBUG_TX;
    else
	    MainForm->web_startup.options&=~WEB_OPT_DEBUG_TX;
	if(DebugRxCheckBox->Checked==true)
    	MainForm->web_startup.options|=WEB_OPT_DEBUG_RX;
    else
	    MainForm->web_startup.options&=~WEB_OPT_DEBUG_RX;
	if(AccessLogCheckBox->Checked==true)
    	MainForm->web_startup.options|=WEB_OPT_HTTP_LOGGING;
    else
	    MainForm->web_startup.options&=~WEB_OPT_HTTP_LOGGING;
	if(HostnameCheckBox->Checked==false)
    	MainForm->web_startup.options|=BBS_OPT_NO_HOST_LOOKUP;
    else
	    MainForm->web_startup.options&=~BBS_OPT_NO_HOST_LOOKUP;
	if(VirtualHostsCheckBox->Checked==true)
    	MainForm->web_startup.options|=WEB_OPT_VIRTUAL_HOSTS;
    else
	    MainForm->web_startup.options&=~WEB_OPT_VIRTUAL_HOSTS;

    MainForm->SaveIniSettings(Sender);
}
//---------------------------------------------------------------------------
void __fastcall TWebCfgDlg::AnswerSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=AnswerSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	AnswerSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------
void __fastcall TWebCfgDlg::HangupSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=HangupSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	HangupSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
	}
}
//---------------------------------------------------------------------------
void __fastcall TWebCfgDlg::HackAttemptSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=HackAttemptSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	HackAttemptSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
	}
}
//---------------------------------------------------------------------------

void __fastcall TWebCfgDlg::AccessLogCheckBoxClick(TObject *Sender)
{
    LogBaseLabel->Enabled=AccessLogCheckBox->Checked;
    LogBaseNameEdit->Enabled=AccessLogCheckBox->Checked;
}
//---------------------------------------------------------------------------

