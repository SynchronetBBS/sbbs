//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "MainFormUnit.h"
#include "TextFileEditUnit.h"
#include "ServicesCfgDlgUnit.h"
#include "CodeInputFormUnit.h"
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
    ini = NULL;
}
//---------------------------------------------------------------------------
void __fastcall TServicesCfgDlg::FormShow(TObject *Sender)
{
    char str[256];

    if(MainForm->services_startup.interfaces==NULL)
        NetworkInterfaceEdit->Text="<ANY>";
    else {
        strListCombine(MainForm->services_startup.interfaces, str, sizeof(str)-1, ",");
        NetworkInterfaceEdit->Text=AnsiString(str);
    }
    AutoStartCheckBox->Checked=MainForm->ServicesAutoStart;

    AnswerSoundEdit->Text=AnsiString(MainForm->services_startup.answer_sound);
    HangupSoundEdit->Text=AnsiString(MainForm->services_startup.hangup_sound);
    HostnameCheckBox->Checked
        =!(MainForm->services_startup.options&BBS_OPT_NO_HOST_LOOKUP);

    iniFileName(iniFilename,sizeof(iniFilename),MainForm->cfg.ctrl_dir,"services.ini");
    /* Populate the Enables tab */
    CheckListBox->Clear();
    FILE* fp;
    if((fp=iniOpenFile(iniFilename, /* Create: */false)) != NULL) {
        ini=iniReadFile(fp);
        iniCloseFile(fp);
    }
    str_list_t services = iniGetSectionList(ini, NULL);
    CheckListBox->Items->BeginUpdate();
    for(unsigned u=0; services!=NULL && services[u]!=NULL; u++) {
        CheckListBox->Items->Append(services[u]);
    }
    CheckListBox->Items->EndUpdate();

    for(unsigned u=0; u<CheckListBox->Items->Count; u++) {
        str_list_t section = iniGetSection(ini, CheckListBox->Items->Strings[u].c_str());
        CheckListBox->Checked[u]=iniGetBool(section, NULL, "Enabled", TRUE);
        iniFreeStringList(section);
    }
    iniFreeStringList(services);
    if(CheckListBox->Items->Count) {
        CheckListBox->ItemIndex=0;
        CheckListBoxClick(Sender);
    }

    str_list_t keys = iniGetKeyList(ini, ROOT_SECTION);
    for(unsigned u=0; keys!=NULL && keys[u]!=NULL; u++) {
        if(keys[u][0])
            GlobalValueListEditor->InsertRow(AnsiString(keys[u])
                ,AnsiString(iniGetString(ini,ROOT_SECTION,keys[u],"",NULL))
                ,/* append: */true);
    }
    iniFreeStringList(keys);

    PageControl->ActivePage=GeneralTabSheet;
}
//---------------------------------------------------------------------------
void __fastcall TServicesCfgDlg::OKButtonClick(TObject *Sender)
{
    iniFreeStringList(MainForm->services_startup.interfaces);
    MainForm->services_startup.interfaces = strListSplitCopy(NULL, NetworkInterfaceEdit->Text.c_str(), ",");

    MainForm->ServicesAutoStart=AutoStartCheckBox->Checked;

    SAFECOPY(MainForm->services_startup.answer_sound
        ,AnswerSoundEdit->Text.c_str());
    SAFECOPY(MainForm->services_startup.hangup_sound
        ,HangupSoundEdit->Text.c_str());

	if(HostnameCheckBox->Checked==false)
    	MainForm->services_startup.options|=BBS_OPT_NO_HOST_LOOKUP;
    else
	    MainForm->services_startup.options&=~BBS_OPT_NO_HOST_LOOKUP;

    FILE* fp;
    if((fp=iniOpenFile(iniFilename, /* Create: */true)) != NULL) {
        iniWriteFile(fp, ini);
        iniCloseFile(fp);
    }

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
void __fastcall TServicesCfgDlg::FormClose(TObject *Sender,
      TCloseAction &Action)
{
    iniFreeStringList(ini);
}
//---------------------------------------------------------------------------


void __fastcall TServicesCfgDlg::CheckListBoxClick(TObject *Sender)
{
    ValueListEditor->Strings->Clear();
    ValueListEditor->Visible=true;
    if(CheckListBox->ItemIndex < 0) {
        ValueListEditor->Visible=false;
        return;
    }
    if(iniGetBool(ini,CheckListBox->Items->Strings[CheckListBox->ItemIndex].c_str()
        ,"Enabled", true) != CheckListBox->Checked[CheckListBox->ItemIndex])
        iniSetBool(&ini
            ,CheckListBox->Items->Strings[CheckListBox->ItemIndex].c_str()
            ,"Enabled", CheckListBox->Checked[CheckListBox->ItemIndex]
            ,/* style: */NULL);

    char* service = strdup(CheckListBox->Items->Strings[CheckListBox->ItemIndex].c_str());
    str_list_t section = iniGetSection(ini, service);
    str_list_t keys = iniGetKeyList(section, NULL);
    for(unsigned u=0; keys!=NULL && keys[u]!=NULL; u++) {
        if(keys[u][0])
            ValueListEditor->InsertRow(AnsiString(keys[u]),
                AnsiString(iniGetString(section,NULL,keys[u],"",NULL)), /* append: */true);
    }
    iniFreeStringList(keys);
    iniFreeStringList(section);
    free(service);
}
//---------------------------------------------------------------------------


void __fastcall TServicesCfgDlg::ValueListEditorValidate(TObject *Sender,
      int ACol, int ARow, const AnsiString KeyName,
      const AnsiString KeyValue)
{
    if(CheckListBox->ItemIndex >= 0)
        iniSetString(&ini
            ,CheckListBox->Items->Strings[CheckListBox->ItemIndex].c_str()
            ,KeyName.c_str(), KeyValue.c_str(), /* style: */NULL);
}
//---------------------------------------------------------------------------

void __fastcall TServicesCfgDlg::ServiceAddClick(TObject *Sender)
{
	Application->CreateForm(__classid(TCodeInputForm), &CodeInputForm);
	CodeInputForm->Label->Caption="New Service Name";
   	CodeInputForm->Edit->Visible=true;
    if(CodeInputForm->ShowModal()==mrOk
       	&& CodeInputForm->Edit->Text.Length()
        && !iniSectionExists(ini,CodeInputForm->Edit->Text.c_str())
        && iniAppendSection(&ini,CodeInputForm->Edit->Text.c_str(),/* style: */NULL)) {
        CheckListBox->Items->Append(CodeInputForm->Edit->Text);
        CheckListBox->ItemIndex = CheckListBox->Items->Count-1;
        CheckListBoxClick(Sender);
    }
    delete CodeInputForm;
}
//---------------------------------------------------------------------------

void __fastcall TServicesCfgDlg::ServiceRemoveClick(TObject *Sender)
{
    if(CheckListBox->ItemIndex >= 0
        && iniRemoveSection(&ini,CheckListBox->Items->Strings[CheckListBox->ItemIndex].c_str())) {
        CheckListBox->DeleteSelected();
        CheckListBoxClick(Sender);
    }        
}
//---------------------------------------------------------------------------

void __fastcall TServicesCfgDlg::CheckListBoxKeyDown(TObject *Sender,
      WORD &Key, TShiftState Shift)
{
    if(Key==VK_INSERT)
        ServiceAddClick(Sender);
    else if(Key==VK_DELETE)
        ServiceRemoveClick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TServicesCfgDlg::GlobalValueListEditorValidate(
      TObject *Sender, int ACol, int ARow, const AnsiString KeyName,
      const AnsiString KeyValue)
{
    iniSetString(&ini
        ,ROOT_SECTION
        ,KeyName.c_str(), KeyValue.c_str(), /* style: */NULL);
}
//---------------------------------------------------------------------------



