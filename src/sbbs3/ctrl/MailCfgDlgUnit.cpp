/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: MailCfgDlgUnit.cpp,v 1.34 2018/03/05 05:35:13 rswindell Exp $ */

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
    char str[256];

    if(MainForm->mail_startup.interfaces==NULL)
        NetworkInterfaceEdit->Text="<ANY>";
    else {
        strListCombine(MainForm->mail_startup.interfaces, str, sizeof(str)-1, ",");
        NetworkInterfaceEdit->Text=AnsiString(str);
    }
    MaxClientsEdit->Text=AnsiString(MainForm->mail_startup.max_clients);
    MaxInactivityEdit->Text=AnsiString(MainForm->mail_startup.max_inactivity);
    if(MainForm->mail_startup.max_recipients == 0)
        MaxRecipientsEdit->Text="N/A";
    else
    	MaxRecipientsEdit->Text=AnsiString(MainForm->mail_startup.max_recipients);
    if(MainForm->mail_startup.max_msg_size == 0)
        MaxMsgSizeEdit->Text="N/A";
    else
    	MaxMsgSizeEdit->Text=AnsiString(byte_count_to_str(MainForm->mail_startup.max_msg_size, str, sizeof(str)));
    if(MainForm->mail_startup.max_msgs_waiting == 0)
        MaxMsgsWaitingEdit->Text="N/A";
    else
        MaxMsgsWaitingEdit->Text=AnsiString(MainForm->mail_startup.max_msgs_waiting);
    if(MainForm->mail_startup.lines_per_yield == 0)
        LinesPerYieldEdit->Text="N/A";
    else
        LinesPerYieldEdit->Text=AnsiString(MainForm->mail_startup.lines_per_yield);

    AutoStartCheckBox->Checked=MainForm->MailAutoStart;
    LogFileCheckBox->Checked=MainForm->MailLogFile;
    HostnameCheckBox->Checked
        =!(MainForm->mail_startup.options&MAIL_OPT_NO_HOST_LOOKUP);
    UseSubPortCheckBox->Checked=MainForm->mail_startup.options&MAIL_OPT_USE_SUBMISSION_PORT;
    TLSSubPortCheckBox->Checked=MainForm->mail_startup.options&MAIL_OPT_TLS_SUBMISSION;

    DefCharsetEdit->Text=AnsiString(MainForm->mail_startup.default_charset);
    RelayServerEdit->Text=AnsiString(MainForm->mail_startup.relay_server);
    RelayAuthNameEdit->Text=AnsiString(MainForm->mail_startup.relay_user);
    RelayAuthPassEdit->Text=AnsiString(MainForm->mail_startup.relay_pass);
    SMTPPortEdit->Text=AnsiString(MainForm->mail_startup.smtp_port);
    SubPortEdit->Text=AnsiString(MainForm->mail_startup.submission_port);
    TLSSubPortEdit->Text=AnsiString(MainForm->mail_startup.submissions_port);
    POP3PortEdit->Text=AnsiString(MainForm->mail_startup.pop3_port);
    TLSPOP3PortEdit->Text=AnsiString(MainForm->mail_startup.pop3s_port);
    RelayPortEdit->Text=AnsiString(MainForm->mail_startup.relay_port);
    if(isalnum(MainForm->mail_startup.dns_server[0]))
        DNSServerEdit->Text=AnsiString(MainForm->mail_startup.dns_server);
    else
        DNSServerEdit->Text="<auto>";
    ConnectTimeoutEdit->Text=AnsiString(MainForm->mail_startup.connect_timeout);
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
    TLSPOP3EnabledCheckBox->Checked=MainForm->mail_startup.options
        &MAIL_OPT_TLS_POP3;
    POP3LogCheckBox->Checked=MainForm->mail_startup.options
        &MAIL_OPT_DEBUG_POP3;
    RelayRadioButton->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_RELAY_TX;
    RelayAuthPlainRadioButton->Checked=MainForm->mail_startup.options
        &MAIL_OPT_RELAY_AUTH_PLAIN;
    RelayAuthLoginRadioButton->Checked=MainForm->mail_startup.options
        &MAIL_OPT_RELAY_AUTH_LOGIN;
    RelayAuthCramMD5RadioButton->Checked=MainForm->mail_startup.options
        &MAIL_OPT_RELAY_AUTH_CRAM_MD5;

#if 0 /* this is a stupid option */
    UserNumberCheckBox->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_ALLOW_RX_BY_NUMBER;
#endif
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
    DNSBLSpamHashCheckBox->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_DNSBL_SPAMHASH;

    TcpDnsCheckBox->Checked=MainForm->mail_startup.options
    	&MAIL_OPT_USE_TCP_DNS;
    SendMailCheckBox->Checked=
        !(MainForm->mail_startup.options&MAIL_OPT_NO_SENDMAIL);

    int i=0;
    AdvancedCheckListBox->Checked[i++]
        =(MainForm->mail_startup.options&MAIL_OPT_SEND_INTRANSIT);
    AdvancedCheckListBox->Checked[i++]
        =(MainForm->mail_startup.options&MAIL_OPT_DEBUG_RX_BODY);
    AdvancedCheckListBox->Checked[i++]
        =(MainForm->mail_startup.options&MAIL_OPT_ALLOW_RX_BY_NUMBER);
    AdvancedCheckListBox->Checked[i++]
        =(MainForm->mail_startup.options&MAIL_OPT_ALLOW_SYSOP_ALIASES);
    AdvancedCheckListBox->Checked[i++]
        =(MainForm->mail_startup.options&MAIL_OPT_ALLOW_RELAY);

    AdvancedCheckListBox->Checked[i++]
        =(MainForm->mail_startup.options&MAIL_OPT_DNSBL_CHKRECVHDRS);
    AdvancedCheckListBox->Checked[i++]
        =(MainForm->mail_startup.options&MAIL_OPT_DNSBL_THROTTLE);
    AdvancedCheckListBox->Checked[i++]
        =!(MainForm->mail_startup.options&MAIL_OPT_NO_AUTO_EXEMPT);
    AdvancedCheckListBox->Checked[i++]
        =(MainForm->mail_startup.options&MAIL_OPT_KILL_READ_SPAM);

    DNSBLRadioButtonClick(Sender);
    DNSRadioButtonClick(Sender);
	POP3EnabledCheckBoxClick(Sender);
    SendMailCheckBoxClick(Sender);
    RelayAuthRadioButtonClick(Sender);
    UseSubPortCheckBoxClick(Sender);
    TLSSubPortCheckBoxClick(Sender);
    PageControl->ActivePage=GeneralTabSheet;
}
//---------------------------------------------------------------------------
static void setBit(unsigned long* l, long bit, bool yes)
{
    if(yes)
        *l|=bit;
    else
        *l&=~bit;
}
//---------------------------------------------------------------------------
void __fastcall TMailCfgDlg::OKBtnClick(TObject *Sender)
{
    iniFreeStringList(MainForm->mail_startup.interfaces);
    MainForm->mail_startup.interfaces = strListSplitCopy(NULL, NetworkInterfaceEdit->Text.c_str(), ",");

	MainForm->mail_startup.smtp_port=SMTPPortEdit->Text.ToIntDef(IPPORT_SMTP);
   	MainForm->mail_startup.submission_port=SubPortEdit->Text.ToIntDef(IPPORT_SUBMISSION);
    MainForm->mail_startup.submissions_port=TLSSubPortEdit->Text.ToIntDef(IPPORT_SUBMISSIONS);
    MainForm->mail_startup.pop3_port=POP3PortEdit->Text.ToIntDef(IPPORT_POP3);
    MainForm->mail_startup.pop3s_port=TLSPOP3PortEdit->Text.ToIntDef(IPPORT_POP3S);
    MainForm->mail_startup.relay_port=RelayPortEdit->Text.ToIntDef(IPPORT_SMTP);
    MainForm->mail_startup.max_clients=MaxClientsEdit->Text.ToIntDef(MAIL_DEFAULT_MAX_CLIENTS);
    MainForm->mail_startup.max_inactivity=MaxInactivityEdit->Text.ToIntDef(MAIL_DEFAULT_MAX_INACTIVITY);
    MainForm->mail_startup.max_recipients=MaxRecipientsEdit->Text.ToIntDef(MAIL_DEFAULT_MAX_RECIPIENTS);
    MainForm->mail_startup.max_msg_size=parse_byte_count(MaxMsgSizeEdit->Text.c_str(), 1);
    MainForm->mail_startup.max_msgs_waiting=MaxMsgsWaitingEdit->Text.ToIntDef(0);
    MainForm->mail_startup.max_delivery_attempts=DeliveryAttemptsEdit->Text.ToIntDef(MAIL_DEFAULT_MAX_DELIVERY_ATTEMPTS);
    MainForm->mail_startup.rescan_frequency=RescanFreqEdit->Text.ToIntDef(MAIL_DEFAULT_RESCAN_FREQUENCY);
    MainForm->mail_startup.lines_per_yield=LinesPerYieldEdit->Text.ToIntDef(0);

    SAFECOPY(MainForm->mail_startup.default_charset
        ,DefCharsetEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.default_user
        ,DefaultUserEdit->Text.c_str());
    if(isalnum(*DNSServerEdit->Text.c_str()))
        SAFECOPY(MainForm->mail_startup.dns_server
            ,DNSServerEdit->Text.c_str());
    else
        MainForm->mail_startup.dns_server[0]=0;
    MainForm->mail_startup.connect_timeout=ConnectTimeoutEdit->Text.ToIntDef(0);
    SAFECOPY(MainForm->mail_startup.relay_server
        ,RelayServerEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.relay_user
        ,RelayAuthNameEdit->Text.c_str());
    SAFECOPY(MainForm->mail_startup.relay_pass
        ,RelayAuthPassEdit->Text.c_str());
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
    MainForm->mail_startup.options&=~(MAIL_OPT_RELAY_AUTH_MASK);
    if(RelayAuthLoginRadioButton->Checked==true)
        MainForm->mail_startup.options|=MAIL_OPT_RELAY_AUTH_LOGIN;
    else if(RelayAuthPlainRadioButton->Checked==true)
        MainForm->mail_startup.options|=MAIL_OPT_RELAY_AUTH_PLAIN;
    else if(RelayAuthCramMD5RadioButton->Checked==true)
        MainForm->mail_startup.options|=MAIL_OPT_RELAY_AUTH_CRAM_MD5;
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
	if(TLSPOP3EnabledCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_TLS_POP3;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_TLS_POP3;
	if(POP3LogCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DEBUG_POP3;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_DEBUG_POP3;
	if(AuthViaIpCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_SMTP_AUTH_VIA_IP;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_SMTP_AUTH_VIA_IP;
 	if(UseSubPortCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_USE_SUBMISSION_PORT;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_USE_SUBMISSION_PORT;
 	if(TLSSubPortCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_TLS_SUBMISSION;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_TLS_SUBMISSION;

    /* DNSBL */
	MainForm->mail_startup.options&=
    	~(MAIL_OPT_DNSBL_IGNORE|MAIL_OPT_DNSBL_REFUSE|MAIL_OPT_DNSBL_BADUSER);
	if(BLIgnoreRadioButton->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DNSBL_IGNORE;
    else if(BLRefuseRadioButton->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DNSBL_REFUSE;
    else if(BLBadUserRadioButton->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DNSBL_BADUSER;
    if(DNSBLSpamHashCheckBox->Checked==true)
    	MainForm->mail_startup.options|=MAIL_OPT_DNSBL_SPAMHASH;
    else
	    MainForm->mail_startup.options&=~MAIL_OPT_DNSBL_SPAMHASH;

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

    int i=0;
    setBit(&MainForm->mail_startup.options
        ,MAIL_OPT_SEND_INTRANSIT
        ,AdvancedCheckListBox->Checked[i++]);
    setBit(&MainForm->mail_startup.options
        ,MAIL_OPT_DEBUG_RX_BODY
        ,AdvancedCheckListBox->Checked[i++]);
    setBit(&MainForm->mail_startup.options
        ,MAIL_OPT_ALLOW_RX_BY_NUMBER
        ,AdvancedCheckListBox->Checked[i++]);
    setBit(&MainForm->mail_startup.options
        ,MAIL_OPT_ALLOW_SYSOP_ALIASES
        ,AdvancedCheckListBox->Checked[i++]);
    setBit(&MainForm->mail_startup.options
        ,MAIL_OPT_ALLOW_RELAY
        ,AdvancedCheckListBox->Checked[i++]);
    setBit(&MainForm->mail_startup.options
        ,MAIL_OPT_DNSBL_CHKRECVHDRS
        ,AdvancedCheckListBox->Checked[i++]);
    setBit(&MainForm->mail_startup.options
        ,MAIL_OPT_DNSBL_THROTTLE
        ,AdvancedCheckListBox->Checked[i++]);
    setBit(&MainForm->mail_startup.options
        ,MAIL_OPT_NO_AUTO_EXEMPT
        ,!AdvancedCheckListBox->Checked[i++]);
    setBit(&MainForm->mail_startup.options
        ,MAIL_OPT_KILL_READ_SPAM
        ,AdvancedCheckListBox->Checked[i++]);

    MainForm->MailAutoStart=AutoStartCheckBox->Checked;
    MainForm->MailLogFile=LogFileCheckBox->Checked;

    MainForm->SaveIniSettings(Sender);
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
    bool checked = RelayRadioButton->Checked && SendMailCheckBox->Checked;
    RelayTabSheet->TabVisible=checked;
    RelayPortEdit->Enabled=checked;
    RelayPortLabel->Enabled=checked;
    checked = (!RelayRadioButton->Checked) && SendMailCheckBox->Checked;
    DNSServerEdit->Enabled=checked;
    DNSServerLabel->Enabled=checked;
    TcpDnsCheckBox->Enabled=checked;
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::POP3EnabledCheckBoxClick(TObject *Sender)
{
	POP3PortEdit->Enabled=POP3EnabledCheckBox->Checked;
    POP3PortLabel->Enabled=POP3EnabledCheckBox->Checked;
	TLSPOP3PortEdit->Enabled=TLSPOP3EnabledCheckBox->Checked;
    TLSPOP3PortLabel->Enabled=TLSPOP3EnabledCheckBox->Checked;

    bool enabled = POP3EnabledCheckBox->Checked || TLSPOP3EnabledCheckBox->Checked;
    
    POP3SoundEdit->Enabled = enabled;
    POP3SoundLabel->Enabled = enabled;
    POP3SoundButton->Enabled = enabled;
    POP3LogCheckBox->Enabled = enabled;
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
    OutboundSoundEdit->Enabled=checked;
    OutboundSoundLabel->Enabled=checked;
    OutboundSoundButton->Enabled=checked;
    DefCharsetLabel->Enabled=checked;
    DefCharsetEdit->Enabled=checked;
    ConnectTimeoutLabel->Enabled=checked;
    ConnectTimeoutEdit->Enabled=checked;

    DNSRadioButtonClick(Sender);
}
//---------------------------------------------------------------------------


void __fastcall TMailCfgDlg::DNSBLRadioButtonClick(TObject *Sender)
{
   	BLSubjectEdit->Enabled=BLTagRadioButton->Checked;
   	BLHeaderEdit->Enabled=BLTagRadioButton->Checked;
    BLSubjectLabel->Enabled=BLTagRadioButton->Checked;
   	BLHeaderLabel->Enabled=BLTagRadioButton->Checked;
    DNSBLSpamHashCheckBox->Enabled
        =(BLTagRadioButton->Checked ||  BLIgnoreRadioButton->Checked);
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::DNSBLServersButtonClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%sdns_blacklist.cfg",MainForm->cfg.ctrl_dir);
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption="DNS-Blacklist Services";
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
    TextFileEditForm->Caption="DNS-Blacklist Exemptions";
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
}
//---------------------------------------------------------------------------
void __fastcall TMailCfgDlg::RelayAuthRadioButtonClick(TObject *Sender)
{
    bool enabled = !RelayAuthNoneRadioButton->Checked;

    RelayAuthNameEdit->Enabled=enabled;
    RelayAuthPassEdit->Enabled=enabled;
    RelayAuthNameLabel->Enabled=enabled;
    RelayAuthPassLabel->Enabled=enabled;
}
//---------------------------------------------------------------------------

void __fastcall TMailCfgDlg::UseSubPortCheckBoxClick(TObject *Sender)
{
    bool enabled = UseSubPortCheckBox->Checked;

    SubPortLabel->Enabled = enabled;
    SubPortEdit->Enabled = enabled;
}
//---------------------------------------------------------------------------


void __fastcall TMailCfgDlg::TLSSubPortCheckBoxClick(TObject *Sender)
{
    bool enabled = TLSSubPortCheckBox->Checked;

    TLSSubPortLabel->Enabled = enabled;
    TLSSubPortEdit->Enabled = enabled;

}
//---------------------------------------------------------------------------


