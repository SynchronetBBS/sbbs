/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

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
#include <Buttons.hpp>
//---------------------------------------------------------------------------
class TSpyForm : public TForm
{
__published:	// IDE-managed Components
    TTimer *Timer;
    TImageList *ImageList;
    TPopupMenu *PopupMenu;
    TMenuItem *KeyboardActivePopupMenuItem;
    TActionList *ActionList;
    TAction *KeyboardActive;
    TAction *ChangeFont;
    TMenuItem *FontPopupMenuItem;
    TPanel *ToolBar;
    TSpeedButton *FontButton;
    TCheckBox *KeyboardActiveCheckBox;
    void __fastcall SpyTimerTick(TObject *Sender);
    void __fastcall FormShow(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall ChangeFontClick(TObject *Sender);
    void __fastcall FormKeyPress(TObject *Sender, char &Key);
    void __fastcall KeyboardActiveClick(TObject *Sender);
    void __fastcall FormMouseUp(TObject *Sender, TMouseButton Button,
          TShiftState Shift, int X, int Y);
private:	// User declarations
	int __fastcall ParseOutput(uchar *buf, int len);
	time_t terminal_fdate;
	void __fastcall ReadTerminalIniFile();
	bool utf8;
public:		// User declarations
    TEmulVT*    Terminal;
    RingBuf**   inbuf;
    RingBuf**   outbuf;
	AnsiString TerminalIniFile;
	int NodeNum;
	unsigned long IdleCount;
    __fastcall TSpyForm(TComponent* Owner);
    __fastcall ~TSpyForm();
};
//---------------------------------------------------------------------------
extern PACKAGE TSpyForm *SpyForms[];
//---------------------------------------------------------------------------
#endif
