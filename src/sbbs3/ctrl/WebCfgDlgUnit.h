//---------------------------------------------------------------------------

#ifndef WebCfgDlgUnitH
#define WebCfgDlgUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
//---------------------------------------------------------------------------
class TWebCfgDlg : public TForm
{
__published:	// IDE-managed Components
    TPageControl *PageControl;
    TTabSheet *GeneralTabSheet;
    TLabel *MaxClientesLabel;
    TLabel *MaxInactivityLabel;
    TLabel *PortLabel;
    TLabel *InterfaceLabel;
    TCheckBox *AutoStartCheckBox;
    TEdit *MaxClientsEdit;
    TEdit *MaxInactivityEdit;
    TEdit *PortEdit;
    TEdit *NetworkInterfaceEdit;
    TCheckBox *HostnameCheckBox;
    TTabSheet *HttpTabSheet;
    TLabel *HtmlDirLabel;
    TEdit *HtmlRootEdit;
    TEdit *ErrorSubDirEdit;
    TEdit *ServerSideJsExtEdit;
    TTabSheet *LogTabSheet;
    TCheckBox *DebugTxCheckBox;
    TCheckBox *DebugRxCheckBox;
    TCheckBox *AccessLogCheckBox;
    TTabSheet *SoundTabSheet;
    TLabel *AnswerSoundLabel;
    TLabel *HangupSoundLabel;
    TLabel *HackAttemptSoundLabel;
    TEdit *AnswerSoundEdit;
    TButton *AnswerSoundButton;
    TEdit *HangupSoundEdit;
    TButton *HangupSoundButton;
    TEdit *HackAttemptSoundEdit;
    TButton *HackAttemptSoundButton;
    TButton *OKBtn;
    TButton *CancelBtn;
    TButton *ApplyBtn;
    TLabel *ErrorSubDirLabel;
    TLabel *ServerSideJsExtLabel;
    TLabel *EmbeddedJsExtLabel;
    TEdit *EmbeddedJsExtEdit;
    TCheckBox *VirtualHostsCheckBox;
    TEdit *LogBaseNameEdit;
    TLabel *LogBaseLabel;
    TOpenDialog *OpenDialog;
    void __fastcall FormShow(TObject *Sender);
    void __fastcall AnswerSoundButtonClick(TObject *Sender);
    void __fastcall HangupSoundButtonClick(TObject *Sender);
    void __fastcall HackAttemptSoundButtonClick(TObject *Sender);
    void __fastcall OKBtnClick(TObject *Sender);
    void __fastcall AccessLogCheckBoxClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TWebCfgDlg(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TWebCfgDlg *WebCfgDlg;
//---------------------------------------------------------------------------
#endif
