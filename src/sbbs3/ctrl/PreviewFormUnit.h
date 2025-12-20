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

#ifndef PreviewFormUnitH
#define PreviewFormUnitH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include "emulvt.hpp"   /* TEmulVT */
#include <Menus.hpp>
//---------------------------------------------------------------------------
class TPreviewForm : public TForm
{
__published:	// IDE-managed Components
    TPopupMenu *PopupMenu;
    TMenuItem *ChangeFontMenuItem;
	void __fastcall FormShow(TObject *Sender);
    void __fastcall FormClose(TObject *Sender, TCloseAction &Action);
    void __fastcall ChangeFontMenuItemClick(TObject *Sender);
private:	// User declarations
public:		// User declarations
	__fastcall TPreviewForm(TComponent* Owner);
    __fastcall ~TPreviewForm();
    TEmulVT*    Terminal;
    AnsiString 	Filename;
};
//---------------------------------------------------------------------------
extern PACKAGE TPreviewForm *PreviewForm;
//---------------------------------------------------------------------------
#endif
