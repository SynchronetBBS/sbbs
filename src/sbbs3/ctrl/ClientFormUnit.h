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

//---------------------------------------------------------------------------

#ifndef ClientFormUnitH
#define ClientFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "MainFormUnit.h"
#include <ExtCtrls.hpp>
#include <ComCtrls.hpp>
#include <Menus.hpp>
//---------------------------------------------------------------------------
class TClientForm : public TForm
{
__published:	// IDE-managed Components
    TTimer *Timer;
    TListView *ListView;
    TPopupMenu *PopupMenu;
    TMenuItem *CloseSocketMenuItem;
	TMenuItem *FilterIpMenuItem;
    TMenuItem *N1;
    TMenuItem *SelectAllMenuItem;
    void __fastcall FormHide(TObject *Sender);
    void __fastcall TimerTimer(TObject *Sender);
    void __fastcall CloseSocketMenuItemClick(TObject *Sender);
	void __fastcall FilterIpMenuItemClick(TObject *Sender);
    void __fastcall SelectAllMenuItemClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
    __fastcall TClientForm(TComponent* Owner);
    HANDLE          ListMutex;
};
//---------------------------------------------------------------------------
extern PACKAGE TClientForm *ClientForm;
//---------------------------------------------------------------------------
#endif
