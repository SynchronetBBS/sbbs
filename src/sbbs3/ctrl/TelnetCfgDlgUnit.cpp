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
    char str[128];

    if(MainForm->bbs_startup.telnet_interface==0)
        TelnetInterfaceEdit->Text="<ANY>";
    else {
        sprintf(str,"%d.%d.%d.%d"
            ,(MainForm->bbs_startup.telnet_interface>>24)&0xff
            ,(MainForm->bbs_startup.telnet_interface>>16)&0xff
            ,(MainForm->bbs_startup.telnet_interface>>8)&0xff
            ,MainForm->bbs_startup.telnet_interface&0xff
        );
        TelnetInterfaceEdit->Text=AnsiString(str);
    }
    if(MainForm->bbs_startup.rlogin_interface==0)
        RLoginInterfaceEdit->Text="<ANY>";
    else {
        sprintf(str,"%d.%d.%d.%d"
            ,(MainForm->bbs_startup.rlogin_interface>>24)&0xff
            ,(MainForm->bbs_startup.rlogin_interface>>16)&0xff
            ,(MainForm->bbs_startup.rlogin_interface>>8)&0xff
            ,MainForm->bbs_startup.rlogin_interface&0xff
        );
        RLoginInterfaceEdit->Text=AnsiString(str);
    }
	TelnetPortEdit->Text=AnsiString((int)MainForm->bbs_startup.telnet_port);
	RLoginPortEdit->Text=AnsiString((int)MainForm->bbs_startup.rlogin_port);

	FirstNodeEdit->Text=AnsiString((int)MainForm->bbs_startup.first_node);
	LastNodeEdit->Text=AnsiString((int)MainForm->bbs_startup.last_node);
	XtrnYieldEdit->Text=AnsiString(
        (int)MainForm->bbs_startup.xtrn_polls_before_yield);
    AutoStartCheckBox->Checked=MainForm->SysAutoStart;
    AnswerSoundEdit->Text=AnsiString(MainForm->bbs_startup.answer_sound);
    HangupSoundEdit->Text=AnsiString(MainForm->bbs_startup.hangup_sound);
    CmdLogCheckBox->Checked=MainForm->bbs_startup.options&BBS_OPT_DEBUG_TELNET;
    TelnetNopCheckBox->Checked
    	=!(MainForm->bbs_startup.options&BBS_OPT_NO_TELNET_NOP);
	XtrnMinCheckBox->Checked=MainForm->bbs_startup.options&BBS_OPT_XTRN_MINIMIZED;
    AutoLogonCheckBox->Checked=MainForm->bbs_startup.options&BBS_OPT_AUTO_LOGON;
    HostnameCheckBox->Checked
        =!(MainForm->bbs_startup.options&BBS_OPT_NO_HOST_LOOKUP);
    IdentityCheckBox->Checked
        =MainForm->bbs_startup.options&BBS_OPT_GET_IDENT;

    RLoginEnabledCheckBox->Checked
        =MainForm->bbs_startup.options&BBS_OPT_ALLOW_RLOGIN;
    RLogin2ndNameCheckBox->Checked
        =MainForm->bbs_startup.options&BBS_OPT_USE_2ND_RLOGIN;
    QWKEventsCheckBox->Checked
        =!(MainForm->bbs_startup.options&BBS_OPT_NO_QWK_EVENTS);
    JavaScriptCheckBox->Checked
        =!(MainForm->bbs_startup.options&BBS_OPT_NO_JAVASCRIPT);

    RLoginEnabledCheckBoxClick(Sender);
    PageControl->ActivePage=GeneralTabSheet;
}
//---------------------------------------------------------------------------

void __fastcall TTelnetCfgDlg::OKBtnClick(TObject *Sender)
{
    char    str[128],*p;
    DWORD   addr;

    SAFECOPY(str,TelnetInterfaceEdit->Text.c_str());
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
        MainForm->bbs_startup.telnet_interface=addr;
    } else
        MainForm->bbs_startup.telnet_interface=0;

    SAFECOPY(str,RLoginInterfaceEdit->Text.c_str());
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
        MainForm->bbs_startup.rlogin_interface=addr;
    } else
        MainForm->bbs_startup.rlogin_interface=0;

    MainForm->bbs_startup.telnet_port=TelnetPortEdit->Text.ToIntDef(23);
    MainForm->bbs_startup.rlogin_port=RLoginPortEdit->Text.ToIntDef(513);

    MainForm->bbs_startup.first_node=FirstNodeEdit->Text.ToIntDef(1);
    MainForm->bbs_startup.last_node=LastNodeEdit->Text.ToIntDef(1);
    MainForm->bbs_startup.xtrn_polls_before_yield
        =XtrnYieldEdit->Text.ToIntDef(10);

    MainForm->SysAutoStart=AutoStartCheckBox->Checked;
    SAFECOPY(MainForm->bbs_startup.answer_sound
        ,AnswerSoundEdit->Text.c_str());
    SAFECOPY(MainForm->bbs_startup.hangup_sound
        ,HangupSoundEdit->Text.c_str());
	if(TelnetNopCheckBox->Checked==false)
    	MainForm->bbs_startup.options|=BBS_OPT_NO_TELNET_NOP;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_NO_TELNET_NOP;
    if(XtrnMinCheckBox->Checked==true)
    	MainForm->bbs_startup.options|=BBS_OPT_XTRN_MINIMIZED;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_XTRN_MINIMIZED;
    if(QWKEventsCheckBox->Checked==true)
        MainForm->bbs_startup.options&=~BBS_OPT_NO_QWK_EVENTS;
    else
        MainForm->bbs_startup.options|=BBS_OPT_NO_QWK_EVENTS;
    if(JavaScriptCheckBox->Checked==true)
        MainForm->bbs_startup.options&=~BBS_OPT_NO_JAVASCRIPT;
    else
        MainForm->bbs_startup.options|=BBS_OPT_NO_JAVASCRIPT;

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
	if(IdentityCheckBox->Checked==true)
    	MainForm->bbs_startup.options|=BBS_OPT_GET_IDENT;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_GET_IDENT;

	if(RLoginEnabledCheckBox->Checked==true)
    	MainForm->bbs_startup.options|=BBS_OPT_ALLOW_RLOGIN;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_ALLOW_RLOGIN;
	if(RLogin2ndNameCheckBox->Checked==true)
    	MainForm->bbs_startup.options|=BBS_OPT_USE_2ND_RLOGIN;
    else
	    MainForm->bbs_startup.options&=~BBS_OPT_USE_2ND_RLOGIN;

    MainForm->SaveSettings(Sender);
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
    RLogin2ndNameCheckBox->Enabled = RLoginEnabledCheckBox->Checked;
    RLoginPortLabel->Enabled = RLoginEnabledCheckBox->Checked;
    RLoginInterfaceLabel->Enabled = RLoginEnabledCheckBox->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TTelnetCfgDlg::RLoginIPallowButtonClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%sRLOGIN.CAN"
    	,MainForm->cfg.text_dir);
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption="Allowed IP addresses for RLogin";
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
}
//---------------------------------------------------------------------------

