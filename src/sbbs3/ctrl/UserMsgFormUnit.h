//---------------------------------------------------------------------------

#ifndef UserMsgFormUnitH
#define UserMsgFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Buttons.hpp>
#include <ExtCtrls.hpp>
//---------------------------------------------------------------------------
class TUserMsgForm : public TForm
{
__published:	// IDE-managed Components
    TMemo *Memo;
    TPanel *Panel;
    TBitBtn *OkButton;
    TBitBtn *CancelButton;
private:	// User declarations
public:		// User declarations
    __fastcall TUserMsgForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TUserMsgForm *UserMsgForm;
//---------------------------------------------------------------------------
#endif
