//---------------------------------------------------------------------------

#ifndef SpyFormUnitH
#define SpyFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include "ringbuf.h"
//---------------------------------------------------------------------------
class TSpyForm : public TForm
{
__published:	// IDE-managed Components
    TMemo *Log;
    TTimer *Timer;
    void __fastcall SpyTimerTick(TObject *Sender);
    void __fastcall FormDestroy(TObject *Sender);
    void __fastcall FormShow(TObject *Sender);
private:	// User declarations
public:		// User declarations
    RingBuf** spybuf;
    __fastcall TSpyForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TSpyForm *SpyForm;
//---------------------------------------------------------------------------
#endif
