//---------------------------------------------------------------------------

#ifndef ServicesFormUnitH
#define ServicesFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
//---------------------------------------------------------------------------
class TServicesForm : public TForm
{
__published:	// IDE-managed Components
    TMemo *Log;
private:	// User declarations
public:		// User declarations
    __fastcall TServicesForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TServicesForm *ServicesForm;
//---------------------------------------------------------------------------
#endif
