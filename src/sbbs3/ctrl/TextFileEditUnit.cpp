/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include <vcl.h>
#pragma hdrstop

#include <stdio.h>
#include "TextFileEditUnit.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TTextFileEditForm *TextFileEditForm;
//---------------------------------------------------------------------------
__fastcall TTextFileEditForm::TTextFileEditForm(TComponent* Owner)
	: TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TTextFileEditForm::FormShow(TObject *Sender)
{
	char str[256];

    if(Memo->ReadOnly) {
        Save->Enabled=false;
        Cut->Enabled=false;
        Undo->Enabled=false;
        Paste->Enabled=false;
        InsertCtrlA->Enabled=false;
        CancelButton->Enabled=false;
        Panel->Visible=false;
        EditReplaceMenuItem->Enabled=false;
        ReplacePopupMenuItem->Enabled=false;
    }
    Caption=Caption+AnsiString(" - ")+Filename;
    try {
        Memo->Lines->LoadFromFile(Filename);
    } catch(...) { }
    ActiveControl=Memo;
    Memo->SelStart=0;
}
//---------------------------------------------------------------------------
void __fastcall TTextFileEditForm::FontButtonClick(TObject *Sender)
{
	TFontDialog *FontDialog=new TFontDialog(this);

    FontDialog->Font->Assign(Memo->Font);
    FontDialog->Execute();
    Memo->Font->Assign(FontDialog->Font);
    delete FontDialog;
}
//---------------------------------------------------------------------------






void __fastcall TTextFileEditForm::FindDialogFind(TObject *Sender)
{
    TFindDialog* Dlg=(TFindDialog*)Sender;
    TSearchTypes stype;
    int result;
    int start;

    if(Dlg->Options.Contains(frMatchCase))
        stype << stMatchCase;
    if(Dlg->Options.Contains(frWholeWord))
        stype << stWholeWord;

    start=Memo->SelStart;

    if(Dlg->Tag)
        start++;
    result=Memo->FindText(Dlg->FindText
        ,start,Memo->Text.Length(),stype);
    if(result==-1)
        Beep();
    else {
        Memo->SelStart=result;
        Memo->SelLength=Dlg->FindText.Length();
    }
    Dlg->Tag=1;
}
//---------------------------------------------------------------------------



void __fastcall TTextFileEditForm::ReplaceDialogReplace(TObject *Sender)
{
    TSearchTypes stype;
    int result;
    int start;

    if(ReplaceDialog->Options.Contains(frMatchCase))
        stype << stMatchCase;
    if(ReplaceDialog->Options.Contains(frWholeWord))
        stype << stWholeWord;

    if(ReplaceDialog->Options.Contains(frReplaceAll)) {
        while((result=Memo->FindText(ReplaceDialog->FindText
            ,Memo->SelStart,Memo->Text.Length(),stype))!=-1) {
            Memo->SelStart=result;
            Memo->SelLength=ReplaceDialog->FindText.Length();
            Memo->SelText=ReplaceDialog->ReplaceText;
            Memo->SelStart++;
        }
        return;
    }

    start=Memo->SelStart;

    if(Memo->SelText==ReplaceDialog->FindText) {
        Memo->SelText=ReplaceDialog->ReplaceText;
        ReplaceDialog->Tag=1;
        return;
    }
    result=Memo->FindText(ReplaceDialog->FindText
        ,start,Memo->Text.Length(),stype);
    if(result==-1)
        Beep();
    else {
        Memo->SelStart=result;
        Memo->SelLength=ReplaceDialog->FindText.Length();
        Memo->SelText=ReplaceDialog->ReplaceText;
    }
}
//---------------------------------------------------------------------------


void __fastcall TTextFileEditForm::InsertCtrlAExecute(TObject *Sender)
{
    Memo->SelText="\1";
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::FindExecute(TObject *Sender)
{
    FindDialog->Options.Clear();
    FindDialog->Options << frHideUpDown;
    FindDialog->Tag=0;
    FindDialog->Execute();
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::ReplaceExecute(TObject *Sender)
{
    ReplaceDialog->Execute();
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::SelectAllExecute(TObject *Sender)
{
    Memo->SelectAll();
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::FileExitMenuItemClick(TObject *Sender)
{
    Close();
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::SaveExecute(TObject *Sender)
{
	int i;

    if(Memo->ReadOnly==true)
        return;

#if 0
	FILE* fp;
    if((fp=fopen(Filename.c_str(),"w"))!=NULL) {
		for(i=0;i<Memo->Lines->Count;i++)
        	fprintf(fp,"%s\n",Memo->Lines->Strings[i].c_str());
        fclose(fp);
    }
#else
    Memo->Lines->SaveToFile(Filename);
#endif
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::CutExecute(TObject *Sender)
{
    Memo->CutToClipboard();    
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::CopyExecute(TObject *Sender)
{
    Memo->CopyToClipboard();    
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::PasteExecute(TObject *Sender)
{
    Memo->PasteFromClipboard();
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::UndoExecute(TObject *Sender)
{
    Memo->Undo();
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::ChangeFontExecute(TObject *Sender)
{
    FontDialog->Font->Assign(Memo->Font);
    FontDialog->Execute();
    Memo->Font->Assign(FontDialog->Font);
}
//---------------------------------------------------------------------------

void __fastcall TTextFileEditForm::FontDialogApply(TObject *Sender,
      HWND Wnd)
{
    Memo->Font->Assign(FontDialog->Font);
}
//---------------------------------------------------------------------------




