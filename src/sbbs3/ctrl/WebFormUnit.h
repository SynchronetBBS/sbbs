//---------------------------------------------------------------------------

#ifndef WebFormUnitH
#define WebFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <ToolWin.hpp>
#include "MainFormUnit.h"
//---------------------------------------------------------------------------
class TWebForm : public TForm
{
__published:	// IDE-managed Components
    TToolBar *ToolBar;
    TToolButton *StartButton;
    TToolButton *StopButton;
    TToolButton *RecycleButton;
    TToolButton *ToolButton1;
    TToolButton *ConfigureButton;
    TToolButton *ToolButton2;
    TStaticText *Status;
    TToolButton *ToolButton3;
    TProgressBar *ProgressBar;
    TMemo *Log;
    void __fastcall FormShow(TObject *Sender);
    void __fastcall FormHide(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TWebForm(TComponent* Owner);
    TMainForm* 		MainForm;
 
};
//---------------------------------------------------------------------------
extern PACKAGE TWebForm *WebForm;
//---------------------------------------------------------------------------
#endif
