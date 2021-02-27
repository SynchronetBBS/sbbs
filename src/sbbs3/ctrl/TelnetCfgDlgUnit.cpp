/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: TelnetCfgDlgUnit.cpp,v 1.26 2019/01/12 23:45:21 rswindell Exp $ */

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
#include "TextFileEditUnit.h"
#include "TelnetCfgDlgUnit.h"
#include <stdio.h>			// sprintf()
#include <mmsystem.h>		// sndPlaySound()
//---------------------------------------------------------------------
#pragma resource "*.dfm"
TTelnetCfgDlg *TelnetCfgDlg;
//--------------------------------------------------------------------- 
__fastcall TTelnetCfgDlg::TTelnetCfgDlg(TComponent* AOwner)
	: TForm(AOwner)
{
}
//---------------------------------------------------------------------
void __fastcall TTelnetCfgDlg::FormShow(TObject *Sender)
{
    char str[256];

    if(MainForm->bbs_startup.telnet_interfaces==NULL)
        TelnetInterfaceEdit->Text="<ANY>";
    else {
        strListCombine(MainForm->bbs_startup.telnet_interfaces, str, sizeof(str)-1, ",");
        TelnetInterfaceEdit->Text=AnsiString(str);
    }

    if(MainForm->bbs_startup.rlogin_interfaces==NULL)
        RLoginInterfaceEdit->Text="<ANY>";
    else {
        strListCombine(MainForm->bbs_startup.rlogin_interfaces, str, sizeof(str)-1, ",");
        RLoginInterfaceEdit->Text=AnsiString(str);
    }

    if(MainForm->bbs_startup.ssh_interfaces==NULL)
        SshInterfaceEdit->Text="<ANY>";
    else {
        strListCombine(MainForm->bbs_startup.ssh_interfaces, str, sizeof(str)-1, ",");
        SshInterfaceEdit->Text=AnsiString(str);
    }
    SshConnTimeoutEdit->Text=AnsiString((int)MainForm->bbs_startup.ssh_connect_timeout);

	TelnetPortEdit->Text=AnsiString((int)MainForm->bbs_startup.telnet_port);
	RLoginPortEdit->Text=AnsiString((int)MainForm->bbs_startup.rlogin_port);
	SshPortEdit->Text=AnsiString((int)MainForm->bbs_startup.ssh_port);

	FirstNodeEdit->Text=AnsiString((int)MainForm->bbs_startup.first_node);
	LastNodeEdit->Text=AnsiString((int)MainForm->bbs_startup.last_node);
    MaxConConEdit->Text=AnsiString((int)MainForm->bbs_startup.max_concurrent_connections);
    AutoStartCheckBox->Checked=MainForm->SysAutoStart;
    AnswerSoundEdit->Text=AnsiString(MainForm->bbs_startup.answer_sound);
    HangupSoundEdit->Text=AnsiString(MainForm->bbs_startup.hangup_sound);
    CmdLogCheckBox->Checked=MainForm->bbs_startup.options&BBS_OPT_DEBUG_TELNET;
    TelnetGaCheckBox->Checked
    	=!(MainForm->bbs_startup.options&BBS_OPT_NO_TELNET_GA);
	XtrnMinCheckBox->Checked=MainForm->bbs_startup.options&BBS_OPT_XTRN_MINIMIZED;
    AutoLogonCheckBox->Checked=MainForm->bbs_startup.options&BBS_OPT_AUTO_LOGON;
    HostnameCheckBox->Checked
        =!(MainForm->bbs_startup.options&BBS_OPT_NO_HOST_LOOKUP);
    DosSupportCheckBox->Checked
        =!(MainForm->bbs_startup.options&BBS_OPT_NO_DOS);

    RLoginEnabledCheckBox->Checked
        =MainForm->bbs_startup.options&BBS_OPT_ALLOW_RLOGIN;
    SshEnabledCheckBox->Checked
        =MainForm->bbs_startup.options&BBS_OPT_ALLOW_SSH;

    QWKEventsCheckBox->Checked
        =!(MainForm->bbs_startup.options&BBS_OPT_NO_QWK_EVENTS);
    EventsCheckBox->Checked
        =!(MainForm->bbs_startup.options&BBS_OPT_NO_EVENTS);

    RLoginEnabledCheckBoxClick(Sender);
    SshEnabledCheckBoxClick(Sender);
    PageControl->ActivePage=GeneralTabSheet;
}
//---------------------------------------------------------------------------

void __fastcall TTelnetCfgDlg::OKBtnClick(TObject *Sender)
{
    iniFreeStringList(MainForm->bbs_startup.telnet_interfaces);
    MainForm->bbs_startup.telnet_interfaces = strListSplitCopy(NULL, TelnetInterfaceEdit->Text.c_str(), ",");

    iniFreeStringList(MainForm->bbs_startup.rlogin_interfaces);
    MainForm->bbs_startup.rlogin_interfaces = strListSplitCopy(NULL, RLoginInterfaceEdit->Text.c_str(), ",");

    iniFreeStringList(MainForm->bbs_startup.ssh_interfaces);
    MainForm->bbs_startup.ssh_interfaces = strListSplitCopy(NULL, SshInterfaceEdit->Text.c_str(), ",");
    MainForm->bbs_startup.ssh_connect_timeout=SshConnTimeoutEdit->Text.ToIntDef(0);

    MainForm->bbs_startup.telnet_port=TelnetPortEdit->Text.ToIntDef(23);
    MainForm->bbs_startup.rlogin_port=RLoginPortEdit->Text.ToIntDef(513);
    MainForm->bbs_startup.ssh_port=SshPortEdit->Text.ToIntDef(22);

    MainForm->bbs_startup.first_node=FirstNodeEdit->Text.ToIntDef(1);
    MainForm->bbs_startup.last_node=LastNodeEdit->Text.ToIntDef(1);
    MainForm->bbs_startup.max_concurrent_connections=MaxConConEdit->Text.ToIntDef(0);

    MainForm->SysAutoStart=AutoStartCheckBox->Checked;
    SAFECOPY(MainForm->bbs_startup.answer_sound
        ,AnswerSoundEdit->Text.c_str());
    SAFECOPY(MainForm->bbs_startup.hangup_sound
        ,HangupSoundEdit->Text.c_str());
	if(TelnetGaCheckBox->Checked==false)
    	MainForm->bbs_startup.options|=BBS_OPT_NO_TELNET_GA;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_NO_TELNET_GA;
    if(XtrnMinCheckBox->Checked==true)
    	MainForm->bbs_startup.options|=BBS_OPT_XTRN_MINIMIZED;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_XTRN_MINIMIZED;
    if(QWKEventsCheckBox->Checked==true)
        MainForm->bbs_startup.options&=~BBS_OPT_NO_QWK_EVENTS;
    else
        MainForm->bbs_startup.options|=BBS_OPT_NO_QWK_EVENTS;
    if(EventsCheckBox->Checked==true)
        MainForm->bbs_startup.options&=~BBS_OPT_NO_EVENTS;
    else
        MainForm->bbs_startup.options|=BBS_OPT_NO_EVENTS;
    if(DosSupportCheckBox->Checked==true)
        MainForm->bbs_startup.options&=~BBS_OPT_NO_DOS;
    else
        MainForm->bbs_startup.options|=BBS_OPT_NO_DOS;

    if(AutoLogonCheckBox->Checked==true)
    	MainForm->bbs_startup.options|=BBS_OPT_AUTO_LOGON;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_AUTO_LOGON;
    if(CmdLogCheckBox->Checked==true)
    	MainForm->bbs_startup.options|=BBS_OPT_DEBUG_TELNET;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_DEBUG_TELNET;
	if(HostnameCheckBox->Checked==false)
    	MainForm->bbs_startup.options|=BBS_OPT_NO_HOST_LOOKUP;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_NO_HOST_LOOKUP;

	if(RLoginEnabledCheckBox->Checked==true)
    	MainForm->bbs_startup.options|=BBS_OPT_ALLOW_RLOGIN;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_ALLOW_RLOGIN;

	if(SshEnabledCheckBox->Checked==true)
    	MainForm->bbs_startup.options|=BBS_OPT_ALLOW_SSH;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_ALLOW_SSH;

    MainForm->SaveIniSettings(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TTelnetCfgDlg::AnswerSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=AnswerSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	AnswerSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TTelnetCfgDlg::HangupSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=HangupSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	HangupSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
	}
}
//---------------------------------------------------------------------------

void __fastcall TTelnetCfgDlg::RLoginEnabledCheckBoxClick(TObject *Sender)
{
    RLoginPortEdit->Enabled = RLoginEnabledCheckBox->Checked;
    RLoginInterfaceEdit->Enabled = RLoginEnabledCheckBox->Checked;
    RLoginIPallowButton->Enabled = RLoginEnabledCheckBox->Checked;
    RLoginPortLabel->Enabled = RLoginEnabledCheckBox->Checked;
    RLoginInterfaceLabel->Enabled = RLoginEnabledCheckBox->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TTelnetCfgDlg::RLoginIPallowButtonClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%srlogin.cfg"
    	,MainForm->cfg.ctrl_dir);
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption="Allowed IP addresses for RLogin";
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
}
//---------------------------------------------------------------------------

void __fastcall TTelnetCfgDlg::SshEnabledCheckBoxClick(TObject *Sender)
{
    SshPortEdit->Enabled = SshEnabledCheckBox->Checked;
    SshInterfaceEdit->Enabled = SshEnabledCheckBox->Checked;
    SshPortLabel->Enabled = SshEnabledCheckBox->Checked;
    SshInterfaceLabel->Enabled = SshEnabledCheckBox->Checked;
}
//---------------------------------------------------------------------------




