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

#include <vcl.h>
#pragma hdrstop

#include "WebFormUnit.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TWebForm *WebForm;
//---------------------------------------------------------------------------
__fastcall TWebForm::TWebForm(TComponent* Owner)
    : TForm(Owner)
{
	MainForm=(TMainForm*)Application->MainForm;
}
//---------------------------------------------------------------------------


void __fastcall TWebForm::LogLevelUpDownChangingEx(TObject *Sender,
      bool &AllowChange, short NewValue, TUpDownDirection Direction)
{
    if(NewValue < 0 || NewValue > LOG_DEBUG)
        AllowChange = false;
    else {
        MainForm->web_startup.log_level = NewValue;
        LogLevelText->Caption = LogLevelDesc[NewValue];
    }
}
//---------------------------------------------------------------------------

