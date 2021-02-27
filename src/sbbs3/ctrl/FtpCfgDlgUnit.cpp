/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: FtpCfgDlgUnit.cpp,v 1.13 2016/05/27 08:55:02 rswindell Exp $ */

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
    char str[256];

    if(MainForm->ftp_startup.interfaces==NULL)
        NetworkInterfaceEdit->Text="<ANY>";
    else {
        strListCombine(MainForm->ftp_startup.interfaces, str, sizeof(str)-1, ",");
        NetworkInterfaceEdit->Text=AnsiString(str);
    }
    if(MainForm->ftp_startup.pasv_ip_addr.s_addr==0)
        PasvIPv4AddrEdit->Text="<unspecified>";
    else {
        sprintf(str,"%d.%d.%d.%d"
            ,(MainForm->ftp_startup.pasv_ip_addr.s_addr>>24)&0xff
            ,(MainForm->ftp_startup.pasv_ip_addr.s_addr>>16)&0xff
            ,(MainForm->ftp_startup.pasv_ip_addr.s_addr>>8)&0xff
            ,MainForm->ftp_startup.pasv_ip_addr.s_addr&0xff
        );
        PasvIPv4AddrEdit->Text=AnsiString(str);
    }

    MaxClientsEdit->Text=AnsiString((int)MainForm->ftp_startup.max_clients);
    MaxInactivityEdit->Text=AnsiString((int)MainForm->ftp_startup.max_inactivity);
    QwkTimeoutEdit->Text=AnsiString((int)MainForm->ftp_startup.qwk_timeout);
	PortEdit->Text=AnsiString((int)MainForm->ftp_startup.port);
    AutoStartCheckBox->Checked=MainForm->FtpAutoStart;
    LogFileCheckBox->Checked=MainForm->FtpLogFile;

    PasvIpLookupCheckBox->Checked=MainForm->ftp_startup.options&FTP_OPT_LOOKUP_PASV_IP;
	PasvPortLowEdit->Text=AnsiString((int)MainForm->ftp_startup.pasv_port_low);
  	PasvPortHighEdit->Text=AnsiString((int)MainForm->ftp_startup.pasv_port_high);

    IndexFileNameEdit->Text=AnsiString(MainForm->ftp_startup.index_file_name);
    HtmlFileNameEdit->Text=AnsiString(MainForm->ftp_startup.html_index_file);
    HtmlJavaScriptEdit->Text=AnsiString(MainForm->ftp_startup.html_index_script);
    AnswerSoundEdit->Text=AnsiString(MainForm->ftp_startup.answer_sound);
    HangupSoundEdit->Text=AnsiString(MainForm->ftp_startup.hangup_sound);
    HackAttemptSoundEdit->Text=AnsiString(MainForm->ftp_startup.hack_sound);    
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
	HtmlIndexCheckBox->Checked
        =MainForm->ftp_startup.options&FTP_OPT_HTML_INDEX_FILE;
	HtmlIndexCheckBoxClick(Sender);
    PasvIpLookupCheckBoxClick(Sender);

    PageControl->ActivePage=GeneralTabSheet;
}
//---------------------------------------------------------------------------

void __fastcall TFtpCfgDlg::OKBtnClick(TObject *Sender)
{
    char    str[128],*p;
    DWORD   addr;

    iniFreeStringList(MainForm->ftp_startup.interfaces);
    MainForm->ftp_startup.interfaces = strListSplitCopy(NULL, NetworkInterfaceEdit->Text.c_str(), ",");

    SAFECOPY(str,PasvIPv4AddrEdit->Text.c_str());
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
        MainForm->ftp_startup.pasv_ip_addr.s_addr=addr;
    } else
        MainForm->ftp_startup.pasv_ip_addr.s_addr=0;

    MainForm->ftp_startup.max_clients=MaxClientsEdit->Text.ToIntDef(FTP_DEFAULT_MAX_CLIENTS);
    MainForm->ftp_startup.max_inactivity=MaxInactivityEdit->Text.ToIntDef(FTP_DEFAULT_MAX_INACTIVITY);
    MainForm->ftp_startup.qwk_timeout=QwkTimeoutEdit->Text.ToIntDef(FTP_DEFAULT_QWK_TIMEOUT);
    MainForm->ftp_startup.port=PortEdit->Text.ToIntDef(IPPORT_FTP);
    MainForm->FtpAutoStart=AutoStartCheckBox->Checked;
    MainForm->FtpLogFile=LogFileCheckBox->Checked;

    MainForm->ftp_startup.pasv_port_low=PasvPortLowEdit->Text.ToIntDef(IPPORT_RESERVED);
    MainForm->ftp_startup.pasv_port_high=PasvPortHighEdit->Text.ToIntDef(0xffff);

    SAFECOPY(MainForm->ftp_startup.index_file_name
        ,IndexFileNameEdit->Text.c_str());
    SAFECOPY(MainForm->ftp_startup.html_index_file
        ,HtmlFileNameEdit->Text.c_str());
    SAFECOPY(MainForm->ftp_startup.html_index_script
        ,HtmlJavaScriptEdit->Text.c_str());

    SAFECOPY(MainForm->ftp_startup.answer_sound
        ,AnswerSoundEdit->Text.c_str());
    SAFECOPY(MainForm->ftp_startup.hangup_sound
        ,HangupSoundEdit->Text.c_str());
    SAFECOPY(MainForm->ftp_startup.hack_sound
        ,HackAttemptSoundEdit->Text.c_str());

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
	if(HtmlIndexCheckBox->Checked==true)
    	MainForm->ftp_startup.options|=FTP_OPT_HTML_INDEX_FILE;
    else
	    MainForm->ftp_startup.options&=~FTP_OPT_HTML_INDEX_FILE;
    if(PasvIpLookupCheckBox->Checked==true)
        MainForm->ftp_startup.options|=FTP_OPT_LOOKUP_PASV_IP;
    else
        MainForm->ftp_startup.options&=~FTP_OPT_LOOKUP_PASV_IP;

    MainForm->SaveIniSettings(Sender);
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
void __fastcall TFtpCfgDlg::HackAttemptSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=HackAttemptSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	HackAttemptSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
	}
}
//---------------------------------------------------------------------------

void __fastcall TFtpCfgDlg::AutoIndexCheckBoxClick(TObject *Sender)
{
    IndexFileNameEdit->Enabled=AutoIndexCheckBox->Checked;
}
//---------------------------------------------------------------------------


void __fastcall TFtpCfgDlg::HtmlJavaScriptButtonClick(TObject *Sender)
{
	OpenDialog->FileName=HtmlJavaScriptEdit->Text;
	if(OpenDialog->Execute()==true) {
    	HtmlJavaScriptEdit->Text=OpenDialog->FileName;
	}
}
//---------------------------------------------------------------------------

void __fastcall TFtpCfgDlg::HtmlIndexCheckBoxClick(TObject *Sender)
{
    HtmlFileNameEdit->Enabled=HtmlIndexCheckBox->Checked;
    HtmlJavaScriptEdit->Enabled=HtmlIndexCheckBox->Checked;
    HtmlJavaScriptLabel->Enabled=HtmlIndexCheckBox->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TFtpCfgDlg::PasvIpLookupCheckBoxClick(TObject *Sender)
{
    PasvIPv4AddrEdit->Enabled = !PasvIpLookupCheckBox->Checked;
}
//---------------------------------------------------------------------------

