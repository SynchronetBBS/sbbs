//----------------------------------------------------------------------------
#ifndef SoundCfgDlgUnitH
#define SoundCfgDlgUnitH
//----------------------------------------------------------------------------
#include <vcl\System.hpp>
#include <vcl\Windows.hpp>
#include <vcl\SysUtils.hpp>
#include <vcl\Classes.hpp>
#include <vcl\Graphics.hpp>
#include <vcl\StdCtrls.hpp>
#include <vcl\Forms.hpp>
#include <vcl\Controls.hpp>
#include <vcl\Buttons.hpp>
#include <vcl\ExtCtrls.hpp>
#include <Dialogs.hpp>
#include "startup.h"
//----------------------------------------------------------------------------
class TSoundCfgDlg : public TForm
{
__published:        
	TButton *OKBtn;
	TButton *CancelBtn;
    TLabel *AnswerSoundLabel;
    TEdit *AnswerSoundEdit;
    TLabel *HnagupSoundLabel;
    TEdit *HangupSoundEdit;
    TButton *AnswerSoundButton;
    TButton *HangupSoundButton;
    TButton *LoginSoundButton;
    TEdit *LoginSoundEdit;
    TLabel *LoginSoundLabel;
    TLabel *LogoutSoundLabel;
    TEdit *LogoutSoundEdit;
    TButton *LogoutSoundButton;
    TLabel *HackAttemptSoundLabel;
    TEdit *HackAttemptSoundEdit;
    TButton *HackAttemptSoundButton;
    TOpenDialog *OpenDialog;
    void __fastcall AnswerSoundButtonClick(TObject *Sender);
    void __fastcall HangupSoundButtonClick(TObject *Sender);
    void __fastcall LoginSoundButtonClick(TObject *Sender);
    void __fastcall LogoutSoundButtonClick(TObject *Sender);
    void __fastcall HackAttemptSoundButtonClick(TObject *Sender);
    void __fastcall FormShow(TObject *Sender);
    void __fastcall OKBtnClick(TObject *Sender);
private:
public:
	virtual __fastcall TSoundCfgDlg(TComponent* AOwner);
    struct startup_sound_settings* sound;
};
//----------------------------------------------------------------------------
extern PACKAGE TSoundCfgDlg *SoundCfgDlg;
//----------------------------------------------------------------------------
#endif    
