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

//---------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "MainFormUnit.h"
#include "FtpCfgDlgUnit.h"
#include "TextFileEditUnit.h"
#include <stdio.h>			// sprintf()
#include <mmsystem.h>		// sndPlaySound()
//---------------------------------------------------------------------
#pragma resource "*.dfm"
TFtpCfgDlg *FtpCfgDlg;
//---------------------------------------------------------------------
__fastcall TFtpCfgDlg::TFtpCfgDlg(TComponent* AOwner)
	: TForm(AOwner)
{
}
//---------------------------------------------------------------------
void __fastcall TFtpCfgDlg::FormShow(TObject *Sender)
{
    char str[128];

    if(MainForm->ftp_startup.interface_addr==0)
        NetworkInterfaceEdit->Text="<ANY>";
    else {
        sprintf(str,"%d.%d.%d.%d"
            ,(MainForm->ftp_startup.interface_addr>>24)&0xff
            ,(MainForm->ftp_startup.interface_addr>>16)&0xff
            ,(MainForm->ftp_startup.interface_addr>>8)&0xff
            ,MainForm->ftp_startup.interface_addr&0xff
        );
        NetworkInterfaceEdit->Text=AnsiString(str);
    }
    MaxClientsEdit->Text=AnsiString((int)MainForm->ftp_startup.max_clients);
    MaxInactivityEdit->Text=AnsiString((int)MainForm->ftp_startup.max_inactivity);
	PortEdit->Text=AnsiString((int)MainForm->ftp_startup.port);
    AutoStartCheckBox->Checked=MainForm->FtpAutoStart;
    LogFileCheckBox->Checked=MainForm->FtpLogFile;

    IndexFileNameEdit->Text=AnsiString(MainForm->ftp_startup.index_file_name);
    AnswerSoundEdit->Text=AnsiString(MainForm->ftp_startup.answer_sound);
    HangupSoundEdit->Text=AnsiString(MainForm->ftp_startup.hangup_sound);
    CmdLogCheckBox->Checked=MainForm->ftp_startup.options&FTP_OPT_DEBUG_RX;
	DebugTxCheckBox->Checked=MainForm->ftp_startup.options&FTP_OPT_DEBUG_TX;
	DebugDataCheckBox->Checked=MainForm->ftp_startup.options&FTP_OPT_DEBUG_DATA;
    DirFilesCheckBox->Checked=MainForm->ftp_startup.options&FTP_OPT_DIR_FILES;
    AllowQWKCheckBox->Checked
        =MainForm->ftp_startup.options&FTP_OPT_ALLOW_QWK;
    LocalFileSysCheckBox->Checked
        =!(MainForm->ftp_startup.options&FTP_OPT_NO_LOCAL_FSYS);
    HostnameCheckBox->Checked
        =!(MainForm->ftp_startup.options&FTP_OPT_NO_HOST_LOOKUP);
	AutoIndexCheckBox->Checked=MainForm->ftp_startup.options&FTP_OPT_INDEX_FILE;
	AutoIndexCheckBoxClick(Sender);
    PageControl->ActivePage=GeneralTabSheet;
}
//---------------------------------------------------------------------------

void __fastcall TFtpCfgDlg::OKBtnClick(TObject *Sender)
{
    char    str[128],*p;
    DWORD   addr;

    sprintf(str,"%.*s",sizeof(str)-1
        ,NetworkInterfaceEdit->Text.c_str());
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
        MainForm->ftp_startup.interface_addr=addr;
    } else
        MainForm->ftp_startup.interface_addr=0;
    MainForm->ftp_startup.max_clients=MaxClientsEdit->Text.ToIntDef(10);
    MainForm->ftp_startup.max_inactivity=MaxInactivityEdit->Text.ToIntDef(300);
    MainForm->ftp_startup.port=PortEdit->Text.ToIntDef(23);
    MainForm->FtpAutoStart=AutoStartCheckBox->Checked;
    MainForm->FtpLogFile=LogFileCheckBox->Checked;

    sprintf(MainForm->ftp_startup.index_file_name,"%.*s"
	    ,sizeof(MainForm->ftp_startup.index_file_name)-1
        ,IndexFileNameEdit->Text.c_str());
    sprintf(MainForm->ftp_startup.answer_sound,"%.*s"
	    ,sizeof(MainForm->ftp_startup.answer_sound)-1
        ,AnswerSoundEdit->Text.c_str());
    sprintf(MainForm->ftp_startup.hangup_sound,"%.*s"
	    ,sizeof(MainForm->ftp_startup.hangup_sound)-1
        ,HangupSoundEdit->Text.c_str());
	if(DebugTxCheckBox->Checked==true)
    	MainForm->ftp_startup.options|=FTP_OPT_DEBUG_TX;
    else
	    MainForm->ftp_startup.options&=~FTP_OPT_DEBUG_TX;
	if(CmdLogCheckBox->Checked==true)
    	MainForm->ftp_startup.options|=FTP_OPT_DEBUG_RX;
    else
	    MainForm->ftp_startup.options&=~FTP_OPT_DEBUG_RX;
	if(DebugDataCheckBox->Checked==true)
    	MainForm->ftp_startup.options|=FTP_OPT_DEBUG_DATA;
    else
	    MainForm->ftp_startup.options&=~FTP_OPT_DEBUG_DATA;
	if(HostnameCheckBox->Checked==false)
    	MainForm->ftp_startup.options|=FTP_OPT_NO_HOST_LOOKUP;
    else
	    MainForm->ftp_startup.options&=~FTP_OPT_NO_HOST_LOOKUP;
	if(AllowQWKCheckBox->Checked==true)
    	MainForm->ftp_startup.options|=FTP_OPT_ALLOW_QWK;
    else
	    MainForm->ftp_startup.options&=~FTP_OPT_ALLOW_QWK;
	if(DirFilesCheckBox->Checked==true)
    	MainForm->ftp_startup.options|=FTP_OPT_DIR_FILES;
    else
	    MainForm->ftp_startup.options&=~FTP_OPT_DIR_FILES;
	if(LocalFileSysCheckBox->Checked==false)
    	MainForm->ftp_startup.options|=FTP_OPT_NO_LOCAL_FSYS;
    else
	    MainForm->ftp_startup.options&=~FTP_OPT_NO_LOCAL_FSYS;
	if(AutoIndexCheckBox->Checked==true)
    	MainForm->ftp_startup.options|=FTP_OPT_INDEX_FILE;
    else
	    MainForm->ftp_startup.options&=~FTP_OPT_INDEX_FILE;

    MainForm->SaveSettings(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TFtpCfgDlg::AnswerSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=AnswerSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	AnswerSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TFtpCfgDlg::HangupSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=HangupSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	HangupSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
	}
}
//---------------------------------------------------------------------------

void __fastcall TFtpCfgDlg::AutoIndexCheckBoxClick(TObject *Sender)
{
    IndexFileNameEdit->Enabled=AutoIndexCheckBox->Checked;
}
//---------------------------------------------------------------------------


