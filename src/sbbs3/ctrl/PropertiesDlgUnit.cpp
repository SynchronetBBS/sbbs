/* $Id: PropertiesDlgUnit.cpp,v 1.10 2016/05/27 08:55:04 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include "NodeFormUnit.h"
#include "ClientFormUnit.h"
#include "TelnetFormUnit.h"
#include "EventsFormUnit.h"
#include "FtpFormUnit.h"
#include "WebFormUnit.h"
#include "MailFormUnit.h"
#include "ServicesFormUnit.h"
#include "PropertiesDlgUnit.h"
#include <mmsystem.h>		// sndPlaySound()
//---------------------------------------------------------------------
#pragma resource "*.dfm"
TPropertiesDlg *PropertiesDlg;
//---------------------------------------------------------------------
__fastcall TPropertiesDlg::TPropertiesDlg(TComponent* AOwner)
	: TForm(AOwner)
{
}
//---------------------------------------------------------------------
void __fastcall TPropertiesDlg::FormShow(TObject *Sender)
{
	TrayIconCheckBoxClick(Sender);
    PageControl->ActivePage=SettingsTabSheet;
    SourceComboBoxChange(Sender);
    LogLevelComboBoxChange(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TPropertiesDlg::TrayIconCheckBoxClick(TObject *Sender)
{
	PasswordEdit->Enabled=TrayIconCheckBox->Checked;
	PasswordLabel->Enabled=PasswordEdit->Enabled;
}
//---------------------------------------------------------------------------

void __fastcall TPropertiesDlg::SourceComboBoxChange(TObject *Sender)
{
	TargetComboBox->ItemIndex=SourceComboBox->ItemIndex;
	switch(SourceComboBox->ItemIndex) {
    	case 0:	/* Node List */
            ExampleEdit->Font=NodeForm->ListBox->Font;
        	ExampleEdit->Color=NodeForm->ListBox->Color;
            break;
        case 1: /* Client List */
            ExampleEdit->Font=ClientForm->ListView->Font;
        	ExampleEdit->Color=ClientForm->ListView->Color;
            break;
        case 2: /* Terminal Server Log */
            ExampleEdit->Font=TelnetForm->Log->Font;
        	ExampleEdit->Color=TelnetForm->Log->Color;
            break;
        case 3: /* Events Log */
            ExampleEdit->Font=EventsForm->Log->Font;
        	ExampleEdit->Color=EventsForm->Log->Color;
            break;
        case 4: /* FTP Server Log */
            ExampleEdit->Font=FtpForm->Log->Font;
        	ExampleEdit->Color=FtpForm->Log->Color;
            break;
        case 5: /* Mail Server Log */
            ExampleEdit->Font=MailForm->Log->Font;
        	ExampleEdit->Color=MailForm->Log->Color;
            break;
        case 6: /* Web Server Log */
            ExampleEdit->Font=WebForm->Log->Font;
        	ExampleEdit->Color=WebForm->Log->Color;
            break;
        case 7: /* Services Log */
            ExampleEdit->Font=ServicesForm->Log->Font;
        	ExampleEdit->Color=ServicesForm->Log->Color;
            break;
    }
}
//---------------------------------------------------------------------------

void __fastcall TPropertiesDlg::FontButtonClick(TObject *Sender)
{
	TFontDialog *FontDialog=new TFontDialog(this);

    FontDialog->Font=ExampleEdit->Font;
    if(FontDialog->Execute())
	    ExampleEdit->Font->Assign(FontDialog->Font);
    delete FontDialog;
}
//---------------------------------------------------------------------------

void __fastcall TPropertiesDlg::BackgroundButtonClick(TObject *Sender)
{
	TColorDialog* ColorDialog=new TColorDialog(this);

    ColorDialog->Options << cdPreventFullOpen;
    ColorDialog->Color=ExampleEdit->Color;
    if(ColorDialog->Execute())
    	ExampleEdit->Color=ColorDialog->Color;
    delete ColorDialog;
}
//---------------------------------------------------------------------------
void __fastcall TPropertiesDlg::ChangeScheme(int target)
{
	switch(target) {
    	case 0:	/* Node List */
            NodeForm->ListBox->Font=ExampleEdit->Font;
        	NodeForm->ListBox->Color=ExampleEdit->Color;
            break;
        case 1: /* Client List */
            ClientForm->ListView->Font=ExampleEdit->Font;
        	ClientForm->ListView->Color=ExampleEdit->Color;
            break;
        case 2: /* Terminal Server Log */
            TelnetForm->Log->Font=ExampleEdit->Font;
        	TelnetForm->Log->Color=ExampleEdit->Color;
            break;
        case 3: /* Events Log */
            EventsForm->Log->Font=ExampleEdit->Font;
        	EventsForm->Log->Color=ExampleEdit->Color;
            break;
        case 4: /* FTP Server Log */
            FtpForm->Log->Font=ExampleEdit->Font;
        	FtpForm->Log->Color=ExampleEdit->Color;
            break;
        case 5: /* Mail Server Log */
            MailForm->Log->Font=ExampleEdit->Font;
        	MailForm->Log->Color=ExampleEdit->Color;
            break;
        case 6: /* Web Server Log */
            WebForm->Log->Font=ExampleEdit->Font;
        	WebForm->Log->Color=ExampleEdit->Color;
            break;
        case 7: /* Services Log */
            ServicesForm->Log->Font=ExampleEdit->Font;
        	ServicesForm->Log->Color=ExampleEdit->Color;
            break;
    }
}
//---------------------------------------------------------------------------
void __fastcall TPropertiesDlg::ApplyButtonClick(TObject *Sender)
{
	if(TargetComboBox->ItemIndex==SourceComboBox->Items->Count) {
    	for(int i=0;i<SourceComboBox->Items->Count;i++)
        	ChangeScheme(i);
    } else
        ChangeScheme(TargetComboBox->ItemIndex);
}
//---------------------------------------------------------------------------
void __fastcall TPropertiesDlg::LogLevelComboBoxChange(TObject *Sender)
{
    LogFontExampleEdit->Font->Assign(MainForm->LogFont[LogLevelComboBox->ItemIndex]);
}
//---------------------------------------------------------------------------

void __fastcall TPropertiesDlg::LogFontButtonClick(TObject *Sender)
{
	TFontDialog *FontDialog=new TFontDialog(this);

    FontDialog->Font=LogFontExampleEdit->Font;
    if(FontDialog->Execute()) {
        LogFontExampleEdit->Font->Assign(FontDialog->Font);
	    MainForm->LogFont[LogLevelComboBox->ItemIndex]->Assign(FontDialog->Font);
    }
    delete FontDialog;
}
//---------------------------------------------------------------------------

void __fastcall TPropertiesDlg::ErrorSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=ErrorSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
        ErrorSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------


void __fastcall TPropertiesDlg::HelpBtnClick(TObject *Sender)
{
    AnsiString url = "http://wiki.synchro.net/monitor:sbbsctrl:properties#"
        + PageControl->ActivePage->Caption.LowerCase();
    ShellExecute(Handle, "open", url.c_str(), NULL, NULL, SW_SHOWDEFAULT);
}
//---------------------------------------------------------------------------

