//---------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "NodeFormUnit.h"
#include "ClientFormUnit.h"
#include "TelnetFormUnit.h"
#include "EventsFormUnit.h"
#include "FtpFormUnit.h"
#include "MailFormUnit.h"
#include "ServicesFormUnit.h"
#include "PropertiesDlgUnit.h"
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
        case 2: /* Telnet Server Log */
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
        case 6: /* Services Log */
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
        case 2: /* Telnet Server Log */
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
        case 6: /* Services Log */
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

