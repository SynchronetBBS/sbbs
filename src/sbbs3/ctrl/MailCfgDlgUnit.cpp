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
#include "MailCfgDlgUnit.h"
#include "TextFileEditUnit.h"
#include <stdio.h>			// sprintf()
#include <mmsystem.h>		// sndPlaySound()
//---------------------------------------------------------------------
#pragma resource "*.dfm"
TMailCfgDlg *MailCfgDlg;
//---------------------------------------------------------------------
__fastcall TMailCfgDlg::TMailCfgDlg(TComponent* AOwner)
	: TForm(AOwner)
{
}
//---------------------------------------------------------------------
void __fastcall TMailCfgDlg::InboundSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=InboundSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	InboundSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::OutboundSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=OutboundSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	OutboundSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
	}
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::FormShow(TObject *Sender)
{
    char str[128];

    if(MainForm->mail_startup.interface_addr==0)
        NetworkInterfaceEdit->Text="<ANY>";
    else {
        sprintf(str,"%d.%d.%d.%d"
            ,(MainForm->mail_startup.interface_addr>>24)&0xff
            ,(MainForm->mail_startup.interface_addr>>16)&0xff
            ,(MainForm->mail_startup.interface_addr>>8)&0xff
            ,MainForm->mail_startup.interface_addr&0xff
        );
        NetworkInterfaceEdit->Text=AnsiString(str);
    }
    MaxClientsEdit->Text=AnsiString(MainForm->mail_startup.max_clients);
    MaxInactivityEdit->Text=AnsiString(MainForm->mail_startup.max_inactivity);
	MaxRecipientsEdit->Text=AnsiString(MainForm->mail_startup.max_recipients);
	MaxMsgSizeEdit->Text=AnsiString(MainForm->mail_startup.max_msg_size);
    LinesPerYieldEdit->Text=AnsiString(MainForm->mail_startup.lines_per_yield);

    AutoStartCheckBox->Checked=MainForm->MailAutoStart;
    LogFileCheckBox->Checked=MainForm->MailLogFile;
    HostnameCheckBox->Checked
        =!(MainForm->mail_startup.options&MAIL_OPT_NO_HOST_LOOKUP);

    RelayServerEdit->Text=AnsiString(MainForm->mail_startup.relay_server);
    SMTPPortEdit->Text=AnsiString(MainForm->mail_startup.smtp_port);
    POP3PortEdit->Text=AnsiString(MainForm->mail_startup.pop3_port);
    RelayPortEdit->Text=AnsiString(MainForm->mail_startup.relay_port);
    DNSServerEdit->Text=AnsiString(MainForm->mail_startup.dns_server);
    InboundSoundEdit->Text=AnsiString(MainForm->mail_startup.inbound_sound);
    OutboundSoundEdit->Text=AnsiString(MainForm->mail_startup.outbound_sound);
    POP3SoundEdit->Text=AnsiString(MainForm->mail_startup.pop3_sound);
    DeliveryAttemptsEdit->Text
        =AnsiString(MainForm->mail_startup.max_delivery_attempts);
    RescanFreqEdit->Text=AnsiString(MainForm->mail_startup.rescan_frequency);
    DefaultUserEdit->Text=AnsiString(MainForm->mail_startup.default_user);
    BLSubjectEdit->Text=AnsiString(MainForm->mail_startup.dnsbl_tag);
    BLHeaderEdit->Text=AnsiString(MainForm->mail_startup.dnsbl_hdr);

    DebugTXCheckBox->Checked=MainForm->mail_startup.options
        &MAIL_OPT_DEBUG_TX;
    DebugRXCheckBox->Checked=MainForm->mail_startup.options
        &MAIL_OPT_DEBUG_RX_RSP;
    DebugHeadersCheckBox->Checked=MainForm->mail_startup.options
        &MAIL_OPT_DEBUG_RX_HEADER;
    NotifyCheckBox->Checked
    	=!(MainForm->mail_startup.options&MAIL_OPT_NO_NOTIFY);
    POP3EnabledCheckBox->Checked=MainForm->mail_startup.options
        &MAIL_OPT_ALLOW_POP3;
    POP3LogCheckBox->Checked=MainForm->mail_startup.options
        &MAIL_OPT_DEBUG_POP3;
    RelayRadioButton->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_RELAY_TX;
#if 0 /* this is a stupid option */
    UserNumberCheckBox->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_ALLOW_RX_BY_NUMBER;
#endif
    AllowRelayCheckBox->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_ALLOW_RELAY;
    AuthViaIpCheckBox->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_SMTP_AUTH_VIA_IP;
    if(MainForm->mail_startup.options&MAIL_OPT_DNSBL_REFUSE)
	    BLRefuseRadioButton->Checked=true;
    else if(MainForm->mail_startup.options&MAIL_OPT_DNSBL_BADUSER)
	    BLBadUserRadioButton->Checked=true;
    else if(MainForm->mail_startup.options&MAIL_OPT_DNSBL_IGNORE)
	    BLIgnoreRadioButton->Checked=true;
	else
	    BLTagRadioButton->Checked=true;
    BLDebugCheckBox->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_DNSBL_DEBUG;

    TcpDnsCheckBox->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_USE_TCP_DNS;
    SendMailCheckBox->Checked=
        !(MainForm->mail_startup.options&MAIL_OPT_NO_SENDMAIL);

    DNSBLRadioButtonClick(Sender);
    DNSRadioButtonClick(Sender);
	POP3EnabledCheckBoxClick(Sender);
    SendMailCheckBoxClick(Sender);
    AllowRelayCheckBoxClick(Sender);
    PageControl->ActivePage=GeneralTabSheet;
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::OKBtnClick(TObject *Sender)
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
        MainForm->mail_startup.interface_addr=addr;
    } else
        MainForm->mail_startup.interface_addr=0;

	MainForm->mail_startup.smtp_port=SMTPPortEdit->Text.ToIntDef(25);
    MainForm->mail_startup.pop3_port=POP3PortEdit->Text.ToIntDef(110);
    MainForm->mail_startup.relay_port=RelayPortEdit->Text.ToIntDef(25);
    MainForm->mail_startup.max_clients=MaxClientsEdit->Text.ToIntDef(10);
    MainForm->mail_startup.max_inactivity=MaxInactivityEdit->Text.ToIntDef(120);
    MainForm->mail_startup.max_recipients=MaxRecipientsEdit->Text.ToIntDef(100);
    MainForm->mail_startup.max_msg_size
    	=MaxMsgSizeEdit->Text.ToIntDef(MainForm->mail_startup.max_msg_size);
    MainForm->mail_startup.max_delivery_attempts
        =DeliveryAttemptsEdit->Text.ToIntDef(10);
    MainForm->mail_startup.rescan_frequency=RescanFreqEdit->Text.ToIntDef(300);
    MainForm->mail_startup.lines_per_yield=LinesPerYieldEdit->Text.ToIntDef(10);

    SAFECOPY(MainForm->mail_startup.default_user
        ,DefaultUserEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.dns_server
        ,DNSServerEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.relay_server
        ,RelayServerEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.inbound_sound
        ,InboundSoundEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.outbound_sound
        ,OutboundSoundEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.pop3_sound
        ,POP3SoundEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.dnsbl_tag
    	,BLSubjectEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.dnsbl_hdr
    	,BLHeaderEdit->Text.c_str());

	if(RelayRadioButton->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_RELAY_TX;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_RELAY_TX;
	if(DebugTXCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DEBUG_TX;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_DEBUG_TX;
	if(DebugRXCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DEBUG_RX_RSP;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_DEBUG_RX_RSP;
	if(NotifyCheckBox->Checked==false)
    	MainForm->mail_startup.options|=MAIL_OPT_NO_NOTIFY;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_NO_NOTIFY;
	if(DebugHeadersCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DEBUG_RX_HEADER;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_DEBUG_RX_HEADER;
	if(POP3EnabledCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_ALLOW_POP3;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_ALLOW_POP3;
	if(POP3LogCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DEBUG_POP3;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_DEBUG_POP3;
#if 0 /* this is a stupid option */
	if(UserNumberCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_ALLOW_RX_BY_NUMBER;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_ALLOW_RX_BY_NUMBER;
#endif
	if(AllowRelayCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_ALLOW_RELAY;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_ALLOW_RELAY;
	if(AuthViaIpCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_SMTP_AUTH_VIA_IP;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_SMTP_AUTH_VIA_IP;

    /* DNSBL */
	MainForm->mail_startup.options&=
    	~(MAIL_OPT_DNSBL_IGNORE|MAIL_OPT_DNSBL_REFUSE|MAIL_OPT_DNSBL_BADUSER);
	if(BLIgnoreRadioButton->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DNSBL_IGNORE;
    else if(BLRefuseRadioButton->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DNSBL_REFUSE;
    else if(BLBadUserRadioButton->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DNSBL_BADUSER;
    if(BLDebugCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DNSBL_DEBUG;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_DNSBL_DEBUG;

	if(HostnameCheckBox->Checked==false)
    	MainForm->mail_startup.options|=MAIL_OPT_NO_HOST_LOOKUP;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_NO_HOST_LOOKUP;
	if(TcpDnsCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_USE_TCP_DNS;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_USE_TCP_DNS;
    if(SendMailCheckBox->Checked==false)
        MainForm->mail_startup.options|=MAIL_OPT_NO_SENDMAIL;
    else
        MainForm->mail_startup.options&=~MAIL_OPT_NO_SENDMAIL;

    MainForm->MailAutoStart=AutoStartCheckBox->Checked;
    MainForm->MailLogFile=LogFileCheckBox->Checked;

    MainForm->SaveSettings(Sender);
}
//---------------------------------------------------------------------------


void __fastcall TMailCfgDlg::POP3SoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=POP3SoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	POP3SoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::DNSRadioButtonClick(TObject *Sender)
{
    RelayServerEdit->Enabled=RelayRadioButton->Checked;
    RelayPortEdit->Enabled=RelayRadioButton->Checked;
    RelayPortLabel->Enabled=RelayRadioButton->Checked;
    DNSServerEdit->Enabled=!RelayRadioButton->Checked;
    TcpDnsCheckBox->Enabled=!RelayRadioButton->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::POP3EnabledCheckBoxClick(TObject *Sender)
{
	POP3PortEdit->Enabled=POP3EnabledCheckBox->Checked;
    POP3PortLabel->Enabled=POP3EnabledCheckBox->Checked;
    POP3SoundEdit->Enabled=POP3EnabledCheckBox->Checked;
    POP3SoundLabel->Enabled=POP3EnabledCheckBox->Checked;
    POP3SoundButton->Enabled=POP3EnabledCheckBox->Checked;
    POP3LogCheckBox->Enabled=POP3EnabledCheckBox->Checked;
}
//---------------------------------------------------------------------------


void __fastcall TMailCfgDlg::SendMailCheckBoxClick(TObject *Sender)
{
    bool checked=SendMailCheckBox->Checked;

    DeliveryAttemptsEdit->Enabled=checked;
    DeliveryAttemptsLabel->Enabled=checked;
    RescanFreqEdit->Enabled=checked;
    RescanFreqLabel->Enabled=checked;
    DNSRadioButton->Enabled=checked;
    DNSServerEdit->Enabled=checked;
    TcpDnsCheckBox->Enabled=checked;
    RelayRadioButton->Enabled=checked;
    RelayServerEdit->Enabled=checked;
    RelayPortEdit->Enabled=checked;
    RelayPortLabel->Enabled=checked;
    OutboundSoundEdit->Enabled=checked;
    OutboundSoundLabel->Enabled=checked;
    OutboundSoundButton->Enabled=checked;

    if(checked)
        DNSRadioButtonClick(Sender);
}
//---------------------------------------------------------------------------


void __fastcall TMailCfgDlg::DNSBLRadioButtonClick(TObject *Sender)
{
   	BLSubjectEdit->Enabled=BLTagRadioButton->Checked;
   	BLHeaderEdit->Enabled=BLTagRadioButton->Checked;
    BLSubjectLabel->Enabled=BLTagRadioButton->Checked;
   	BLHeaderLabel->Enabled=BLTagRadioButton->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::DNSBLServersButtonClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%sdns_blacklist.cfg",MainForm->cfg.ctrl_dir);
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption="Services Configuration";
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
}
//---------------------------------------------------------------------------


void __fastcall TMailCfgDlg::DNSBLExemptionsButtonClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%sdnsbl_exempt.cfg",MainForm->cfg.ctrl_dir);
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption="Services Configuration";
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::AllowRelayCheckBoxClick(TObject *Sender)
{
	AuthViaIpCheckBox->Enabled=AllowRelayCheckBox->Checked;
}
//---------------------------------------------------------------------------

