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

#ifndef ServicesFormUnitH
#define ServicesFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ComCtrls.hpp>
#include <ToolWin.hpp>
/* My Includes */
#include "MainFormUnit.h"
//---------------------------------------------------------------------------
class TServicesForm : public TForm
{
__published:	// IDE-managed Components
    TToolBar *ToolBar;
    TToolButton *StartButton;
    TToolButton *StopButton;
    TToolButton *ToolButton1;
    TToolButton *ConfigureButton;
    TToolButton *ToolButton2;
    TStaticText *Status;
	TToolButton *RecycleButton;
    TRichEdit *Log;
    TToolButton *ToolButton3;
    TStaticText *LogLevelText;
    TUpDown *LogLevelUpDown;
    TToolButton *LogPauseButton;
    void __fastcall FormHide(TObject *Sender);
    void __fastcall LogLevelUpDownChangingEx(TObject *Sender,
          bool &AllowChange, short NewValue, TUpDownDirection Direction);
private:	// User declarations
public:		// User declarations
    __fastcall TServicesForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TServicesForm *ServicesForm;
//---------------------------------------------------------------------------
#endif
