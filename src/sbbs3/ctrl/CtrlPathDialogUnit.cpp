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

//---------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "CtrlPathDialogUnit.h"
//---------------------------------------------------------------------
#pragma resource "*.dfm"
TCtrlPathDialog *CtrlPathDialog;
//---------------------------------------------------------------------
__fastcall TCtrlPathDialog::TCtrlPathDialog(TComponent* AOwner)
	: TForm(AOwner)
{
}
//---------------------------------------------------------------------
void __fastcall TCtrlPathDialog::BrowseButtonClick(TObject *Sender)
{
	OpenDialog->InitialDir=Edit->Text;
    OpenDialog->Options << ofFileMustExist;
	if(OpenDialog->Execute()==true)
    	Edit->Text=OpenDialog->FileName;
}
//---------------------------------------------------------------------------
void __fastcall TCtrlPathDialog::FormShow(TObject *Sender)
{
   	Application->BringToFront();
}
//---------------------------------------------------------------------------

