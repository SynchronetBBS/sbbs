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
#ifndef AboutBoxFormUnitH
#define AboutBoxFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <ExtCtrls.hpp>
#include <Graphics.hpp>
#include "MainFormUnit.h"
const char* ver(void);
//---------------------------------------------------------------------------
class TAboutBoxForm : public TForm
{
__published:	// IDE-managed Components
	TButton *OKButton;
    TBevel *Bevel;
	TImage *Logo;
    TStaticText *CopyrightLabel;
    TMemo *Credits;
    TStaticText *NameLabel;
    TStaticText *WebSiteLabel;
	void __fastcall FormShow(TObject *Sender);
    void __fastcall WebPageLabelClick(TObject *Sender);
	void __fastcall LogoClick(TObject *Sender);
    void __fastcall CreditsClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
	__fastcall TAboutBoxForm(TComponent* Owner);
};
//---------------------------------------------------------------------------
extern PACKAGE TAboutBoxForm *AboutBoxForm;
//---------------------------------------------------------------------------
#endif
