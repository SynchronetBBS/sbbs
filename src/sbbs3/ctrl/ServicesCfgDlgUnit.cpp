//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "TextFileEditUnit.h"
#include "ServicesCfgDlgUnit.h"
#include <stdio.h>			// sprintf()
#include <mmsystem.h>		// sndPlaySound()
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TServicesCfgDlg *ServicesCfgDlg;
//---------------------------------------------------------------------------
__fastcall TServicesCfgDlg::TServicesCfgDlg(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TServicesCfgDlg::ServicesCfgButtonClick(TObject *Sender)
{
	char filename[MAX_PATH+1];

    sprintf(filename,"%s%s",MainForm->cfg.ctrl_dir,((TButton*)Sender)->Caption);
	Application->CreateForm(__classid(TTextFileEditForm), &TextFileEditForm);
	TextFileEditForm->Filename=AnsiString(filename);
    TextFileEditForm->Caption="Services Configuration";
	TextFileEditForm->ShowModal();
    delete TextFileEditForm;
}
//---------------------------------------------------------------------------
void __fastcall TServicesCfgDlg::FormShow(TObject *Sender)
{
    char str[128];

    if(MainForm->services_startup.interface_addr==0)
        NetworkInterfaceEdit->Text="<ANY>";
    else {
        sprintf(str,"%d.%d.%d.%d"
            ,(MainForm->services_startup.interface_addr>>24)&0xff
            ,(MainForm->services_startup.interface_addr>>16)&0xff
            ,(MainForm->services_startup.interface_addr>>8)&0xff
            ,MainForm->services_startup.interface_addr&0xff
        );
        NetworkInterfaceEdit->Text=AnsiString(str);
    }
    AutoStartCheckBox->Checked=MainForm->ServicesAutoStart;

    AnswerSoundEdit->Text=AnsiString(MainForm->services_startup.answer_sound);
    HangupSoundEdit->Text=AnsiString(MainForm->services_startup.hangup_sound);
    HostnameCheckBox->Checked
        =!(MainForm->services_startup.options&BBS_OPT_NO_HOST_LOOKUP);

    PageControl->ActivePage=GeneralTabSheet;

}
//---------------------------------------------------------------------------
void __fastcall TServicesCfgDlg::OKButtonClick(TObject *Sender)
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
        MainForm->services_startup.interface_addr=addr;
    } else
        MainForm->services_startup.interface_addr=0;
    MainForm->ServicesAutoStart=AutoStartCheckBox->Checked;


    SAFECOPY(MainForm->services_startup.answer_sound
        ,AnswerSoundEdit->Text.c_str());
    SAFECOPY(MainForm->services_startup.hangup_sound
        ,HangupSoundEdit->Text.c_str());

	if(HostnameCheckBox->Checked==false)
    	MainForm->services_startup.options|=BBS_OPT_NO_HOST_LOOKUP;
    else
	    MainForm->services_startup.options&=~BBS_OPT_NO_HOST_LOOKUP;

    MainForm->SaveSettings(Sender);

}
//---------------------------------------------------------------------------

void __fastcall TServicesCfgDlg::AnswerSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=AnswerSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	AnswerSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TServicesCfgDlg::HangupSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=HangupSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	HangupSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
	}
}

//---------------------------------------------------------------------------
