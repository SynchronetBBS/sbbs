//---------------------------------------------------------------------------

#ifndef PreviewFormUnitH
#define PreviewFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "emulvt.hpp"	/* TEmulVT */
//---------------------------------------------------------------------------
class TPreviewForm : public TForm
{
__published:	// IDE-managed Components
	void __fastcall FormShow(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
private:	// User declarations
public:		// User declarations
	__fastcall TPreviewForm(TComponent* Owner);
    __fastcall ~TPreviewForm();
    TEmulVT*    Terminal;
    AnsiString 	Filename;
};
//---------------------------------------------------------------------------
extern PACKAGE TPreviewForm *PreviewForm;
//---------------------------------------------------------------------------
#endif
