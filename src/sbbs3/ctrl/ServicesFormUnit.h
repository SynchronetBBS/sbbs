//---------------------------------------------------------------------------

#ifndef ServicesFormUnitH
#define ServicesFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <ToolWin.hpp>
//---------------------------------------------------------------------------
class TServicesForm : public TForm
{
__published:	// IDE-managed Components
    TMemo *Log;
    TToolBar *ToolBar;
    TToolButton *StartButton;
    TToolButton *StopButton;
    TToolButton *ToolButton1;
    TToolButton *ConfigureButton;
    TToolButton *ToolButton2;
    TStaticText *Status;
private:	// User declarations
public:		// User declarations
    __fastcall TServicesForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TServicesForm *ServicesForm;
//---------------------------------------------------------------------------
#endif
