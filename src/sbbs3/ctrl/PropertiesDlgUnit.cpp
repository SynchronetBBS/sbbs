//---------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

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
}
//---------------------------------------------------------------------------

void __fastcall TPropertiesDlg::TrayIconCheckBoxClick(TObject *Sender)
{
	PasswordEdit->Enabled=TrayIconCheckBox->Checked;
	PasswordLabel->Enabled=PasswordEdit->Enabled;
}
//---------------------------------------------------------------------------

