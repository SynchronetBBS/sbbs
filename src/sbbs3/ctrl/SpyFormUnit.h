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
#include <ActnList.hpp>
//---------------------------------------------------------------------------
class TSpyForm : public TForm
{
__published:	// IDE-managed Components
    TTimer *Timer;
    TImageList *ImageList;
    TMainMenu *SpyMenu;
    TMenuItem *SettingsMenuItem;
    TMenuItem *KeyboardActiveMenuItem;
    TMenuItem *FontMenuItem;
    TPopupMenu *PopupMenu;
    TMenuItem *KeyboardActivePopupMenuItem;
    TActionList *ActionList;
    TAction *KeyboardActive;
    TAction *ChangeFont;
    TMenuItem *FontPopupMenuItem;
    void __fastcall SpyTimerTick(TObject *Sender);
    void __fastcall FormShow(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall ChangeFontClick(TObject *Sender);
    void __fastcall FormKeyPress(TObject *Sender, char &Key);
    void __fastcall KeyboardActiveClick(TObject *Sender);
    void __fastcall FormMouseUp(TObject *Sender, TMouseButton Button,
          TShiftState Shift, int X, int Y);
private:	// User declarations
public:		// User declarations
    TEmulVT*    Terminal;
    RingBuf**   inbuf;
    RingBuf**   outbuf;
    __fastcall TSpyForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TSpyForm *SpyForms[];
//---------------------------------------------------------------------------
#endif
