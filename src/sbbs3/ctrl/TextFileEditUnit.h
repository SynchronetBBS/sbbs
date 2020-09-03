/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: TextFileEditUnit.h,v 1.3 2018/07/24 01:11:30 rswindell Exp $ */

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

//---------------------------------------------------------------------------
#ifndef TextFileEditUnitH
#define TextFileEditUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <Buttons.hpp>
#include <ComCtrls.hpp>
#include <Dialogs.hpp>
#include <Menus.hpp>
#include <ActnList.hpp>
//---------------------------------------------------------------------------
class TTextFileEditForm : public TForm
{
__published:	// IDE-managed Components
	TPanel *Panel;
    TBitBtn *OkButton;
    TBitBtn *CancelButton;
    TRichEdit *Memo;
    TFindDialog *FindDialog;
    TReplaceDialog *ReplaceDialog;
    TMainMenu *Menu;
    TMenuItem *EditMenuItem;
    TMenuItem *EditFindMenuItem;
    TMenuItem *EditReplaceMenuItem;
    TMenuItem *FileMenuItem;
    TMenuItem *FileSaveMenuItem;
    TMenuItem *FileExitMenuItem;
    TPopupMenu *PopupMenu;
    TMenuItem *SelectAllPopupMenuItem;
    TMenuItem *FindPopupMenuItem;
    TMenuItem *ReplacePopupMenuItem;
    TMenuItem *N1;
    TMenuItem *N2;
    TMenuItem *CtrlAPopupMenuItem;
    TMenuItem *N3;
    TMenuItem *EditInsertCtrlA;
    TMenuItem *N4;
    TMenuItem *EditSelectAllMenuItem;
    TActionList *ActionList;
    TAction *InsertCtrlA;
    TAction *Find;
    TAction *Replace;
    TAction *SelectAll;
    TAction *Save;
    TMenuItem *EditUndoMenuItem;
    TMenuItem *N5;
    TMenuItem *EditCutMenuItem;
    TMenuItem *EditCopyMenuItem;
    TMenuItem *EditPasteMenuItem;
    TAction *Cut;
    TAction *Copy;
    TAction *Paste;
    TAction *Undo;
    TMenuItem *CutPopupMenuItem;
    TMenuItem *CopyPopupMenuItem;
    TMenuItem *PastePopupMenuItem;
    TFontDialog *FontDialog;
    TMenuItem *FontMenuItem;
    TAction *ChangeFont;
	void __fastcall FormShow(TObject *Sender);
    void __fastcall FontButtonClick(TObject *Sender);
    void __fastcall FindDialogFind(TObject *Sender);
    void __fastcall ReplaceDialogReplace(TObject *Sender);
    void __fastcall InsertCtrlAExecute(TObject *Sender);
    void __fastcall FindExecute(TObject *Sender);
    void __fastcall ReplaceExecute(TObject *Sender);
    void __fastcall SelectAllExecute(TObject *Sender);
    void __fastcall FileExitMenuItemClick(TObject *Sender);
    void __fastcall SaveExecute(TObject *Sender);
    void __fastcall CutExecute(TObject *Sender);
    void __fastcall CopyExecute(TObject *Sender);
    void __fastcall PasteExecute(TObject *Sender);
    void __fastcall UndoExecute(TObject *Sender);
    void __fastcall ChangeFontExecute(TObject *Sender);
    void __fastcall FontDialogApply(TObject *Sender, HWND Wnd);
private:	// User declarations
public:		// User declarations
	__fastcall TTextFileEditForm(TComponent* Owner);
    AnsiString Filename;
};
//---------------------------------------------------------------------------
extern PACKAGE TTextFileEditForm *TextFileEditForm;
//---------------------------------------------------------------------------
#endif
