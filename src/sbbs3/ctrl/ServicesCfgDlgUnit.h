//---------------------------------------------------------------------------

#ifndef ServicesCfgDlgUnitH
#define ServicesCfgDlgUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
#include "MainFormUnit.h"
//---------------------------------------------------------------------------
class TServicesCfgDlg : public TForm
{
__published:	// IDE-managed Components
    TPageControl *PageControl;
    TTabSheet *GeneralTabSheet;
    TLabel *InterfaceLabel;
    TCheckBox *AutoStartCheckBox;
    TEdit *NetworkInterfaceEdit;
    TCheckBox *HostnameCheckBox;
    TTabSheet *SoundTabSheet;
    TLabel *AnswerSoundLabel;
    TLabel *HangupSoundLabel;
    TEdit *AnswerSoundEdit;
    TButton *AnswerSoundButton;
    TEdit *HangupSoundEdit;
    TButton *HangupSoundButton;
    TButton *OKBtn;
    TButton *CancelBtn;
    TButton *ApplyBtn;
    TButton *OKButton;
    TButton *CancelButton;
    TButton *ApplyButton;
    TButton *ServicesCfgButton;
    TOpenDialog *OpenDialog;
	TButton *ServicesIniButton;
    void __fastcall ServicesCfgButtonClick(TObject *Sender);
    void __fastcall FormShow(TObject *Sender);
    void __fastcall OKButtonClick(TObject *Sender);
    void __fastcall AnswerSoundButtonClick(TObject *Sender);
    void __fastcall HangupSoundButtonClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TServicesCfgDlg(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TServicesCfgDlg *ServicesCfgDlg;
//---------------------------------------------------------------------------
#endif
