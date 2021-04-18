//---------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "SoundCfgDlgUnit.h"
#include <mmsystem.h>		// sndPlaySound()
//---------------------------------------------------------------------
#pragma resource "*.dfm"
TSoundCfgDlg *SoundCfgDlg;
//---------------------------------------------------------------------
__fastcall TSoundCfgDlg::TSoundCfgDlg(TComponent* AOwner)
	: TForm(AOwner)
{
}
//---------------------------------------------------------------------
void __fastcall TSoundCfgDlg::AnswerSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=AnswerSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	AnswerSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TSoundCfgDlg::HangupSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=HangupSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	HangupSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TSoundCfgDlg::LoginSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=LoginSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	LoginSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TSoundCfgDlg::LogoutSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=LogoutSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	LogoutSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TSoundCfgDlg::HackAttemptSoundButtonClick(TObject *Sender)
{
	OpenDialog->FileName=HackAttemptSoundEdit->Text;
	if(OpenDialog->Execute()==true) {
    	HackAttemptSoundEdit->Text=OpenDialog->FileName;
        sndPlaySound(OpenDialog->FileName.c_str(),SND_ASYNC);
    }
}
//---------------------------------------------------------------------------

void __fastcall TSoundCfgDlg::FormShow(TObject *Sender)
{
    AnswerSoundEdit->Text=AnsiString(sound->answer);
    HangupSoundEdit->Text=AnsiString(sound->hangup);
    LoginSoundEdit->Text=AnsiString(sound->login);
    LogoutSoundEdit->Text=AnsiString(sound->logout);
    HackAttemptSoundEdit->Text=AnsiString(sound->hack);
}
//---------------------------------------------------------------------------

void __fastcall TSoundCfgDlg::OKBtnClick(TObject *Sender)
{
    SAFECOPY(sound->answer, AnswerSoundEdit->Text.c_str());
    SAFECOPY(sound->hangup, HangupSoundEdit->Text.c_str());
    SAFECOPY(sound->login, LoginSoundEdit->Text.c_str());
    SAFECOPY(sound->logout, LogoutSoundEdit->Text.c_str());
    SAFECOPY(sound->hack, HackAttemptSoundEdit->Text.c_str());    
}
//---------------------------------------------------------------------------

