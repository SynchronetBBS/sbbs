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
#include "emulvt.hpp"
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
#include <ImgList.hpp>
#include <ToolWin.hpp>
#include <Menus.hpp>
//---------------------------------------------------------------------------
class TSpyForm : public TForm
{
__published:	// IDE-managed Components
    TTimer *Timer;
    TImageList *ImageList;
    TMainMenu *SpyMenu;
    TMenuItem *FontMenuItem;
    void __fastcall SpyTimerTick(TObject *Sender);
    void __fastcall FormShow(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall FontMenuItemClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    TEmulVT *Terminal;
    RingBuf** spybuf;
    __fastcall TSpyForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TSpyForm *SpyForms[];
//---------------------------------------------------------------------------
#endif
