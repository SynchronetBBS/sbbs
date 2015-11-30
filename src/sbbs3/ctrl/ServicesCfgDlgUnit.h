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
#include <CheckLst.hpp>
#include <Menus.hpp>
#include <Grids.hpp>
#include <ValEdit.hpp>
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
    TOpenDialog *OpenDialog;
    TTabSheet *ServicesTabSheet;
    TCheckListBox *CheckListBox;
    TValueListEditor *ValueListEditor;
    TPopupMenu *ServicesCfgPopupMenu;
    TMenuItem *ServiceAdd;
    TMenuItem *ServiceRemove;
    TValueListEditor *GlobalValueListEditor;
    TLabel *GlobalSettingsLabel;
    void __fastcall FormShow(TObject *Sender);
    void __fastcall OKButtonClick(TObject *Sender);
    void __fastcall AnswerSoundButtonClick(TObject *Sender);
    void __fastcall HangupSoundButtonClick(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall CheckListBoxClick(TObject *Sender);
    void __fastcall ValueListEditorValidate(TObject *Sender, int ACol,
          int ARow, const AnsiString KeyName, const AnsiString KeyValue);
    void __fastcall ServiceAddClick(TObject *Sender);
    void __fastcall ServiceRemoveClick(TObject *Sender);
    void __fastcall CheckListBoxKeyDown(TObject *Sender, WORD &Key,
          TShiftState Shift);
    void __fastcall GlobalValueListEditorValidate(TObject *Sender,
          int ACol, int ARow, const AnsiString KeyName,
          const AnsiString KeyValue);
private:	// User declarations
	char iniFilename[MAX_PATH+1];
    str_list_t ini;
public:		// User declarations
    __fastcall TServicesCfgDlg(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TServicesCfgDlg *ServicesCfgDlg;
//---------------------------------------------------------------------------
#endif
