/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: NodeFormUnit.h,v 1.12 2019/08/31 22:25:08 rswindell Exp $ */

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
#ifndef NodeFormUnitH
#define NodeFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
/* My Includes */
#include "MainFormUnit.h"
#include <ComCtrls.hpp>
#include <ImgList.hpp>
#include <ToolWin.hpp>
#include <ExtCtrls.hpp>
#include <Menus.hpp>
//---------------------------------------------------------------------------
class TNodeForm : public TForm
{
__published:	// IDE-managed Components
	TTimer *Timer;
	TToolBar *Toolbar;
	TListBox *ListBox;
	TToolButton *InterruptNodeButton;
	TToolButton *LockNodeButton;
	TPopupMenu *PopupMenu;
	TMenuItem *InterruptMenuItem;
	TMenuItem *LockMenuItem;
	TMenuItem *N1;
	TMenuItem *SelectAllMenuItem;
	TToolButton *DownButton;
	TMenuItem *DownMenuItem;
    TToolButton *ClearErrorButton;
    TMenuItem *ClearErrorsMenuItem;
    TToolButton *ChatButton;
    TMenuItem *ChatMenuItem;
    TToolButton *SpyButton;
    TMenuItem *SpyMenuItem;
    TToolButton *RerunToolButton;
    TMenuItem *RerunMenuItem;
    TToolButton *UserEditButton;
    TMenuItem *EditUser1;
    TToolButton *UserMsgButton;
    TMenuItem *SendMsgMenuItem;
    TMenuItem *RefreshMenuItem;
	void __fastcall TimerTick(TObject *Sender);
    void __fastcall FormHide(TObject *Sender);
	void __fastcall InterruptNodeButtonClick(TObject *Sender);
	void __fastcall LockNodeButtonClick(TObject *Sender);
	void __fastcall SelectAllMenuItemClick(TObject *Sender);
	void __fastcall DownButtonClick(TObject *Sender);
    void __fastcall ClearErrorButtonClick(TObject *Sender);
    void __fastcall ChatButtonClick(TObject *Sender);
    void __fastcall SpyButtonClick(TObject *Sender);
    void __fastcall RerunNodeButtonClick(TObject *Sender);
    void __fastcall UserEditButtonClick(TObject *Sender);
    void __fastcall UserMsgButtonClick(TObject *Sender);
    void __fastcall RefreshMenuItemClick(TObject *Sender);
private:	// User declarations
	int file;
public:		// User declarations
     __fastcall TNodeForm(TComponent* Owner);
    int __fastcall getnodedat(int node_num, node_t* node, bool lockit);
    int __fastcall putnodedat(int node_num, node_t* node);

};
//---------------------------------------------------------------------------
extern PACKAGE TNodeForm *NodeForm;
//---------------------------------------------------------------------------
#endif
