/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: SpyFormUnit.h,v 1.10 2018/07/24 01:11:29 rswindell Exp $ */

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
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
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
	int __fastcall strip_telnet(uchar *buf, int len);
public:		// User declarations
    TEmulVT*    Terminal;
    RingBuf**   inbuf;
    RingBuf**   outbuf;
    __fastcall TSpyForm(TComponent* Owner);
    __fastcall ~TSpyForm();
};
//---------------------------------------------------------------------------
extern PACKAGE TSpyForm *SpyForms[];
//---------------------------------------------------------------------------
#endif
