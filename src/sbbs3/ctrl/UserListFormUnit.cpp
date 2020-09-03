/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: UserListFormUnit.cpp,v 1.10 2016/11/16 09:07:07 rswindell Exp $ */

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

#include "sbbs.h"
#include <vcl.h>
#include <vcl/Clipbrd.hpp>
#pragma hdrstop

#include "UserListFormUnit.h"
#include "MainFormUnit.h"
#include "userdat.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TUserListForm *UserListForm;
//---------------------------------------------------------------------------
__fastcall TUserListForm::TUserListForm(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TUserListForm::FormShow(TObject *Sender)
{
    char    str[128];
    int     i,last;
	int		file;
    user_t  user;
    TListItem*  Item;

    ColumnToSort=0;
    SortBackwards=false;
    Screen->Cursor=crAppStart;

	if((file = openuserdat(&MainForm->cfg, /* for modify: */FALSE)) < 0) {
		Screen->Cursor=crDefault;
		return;
	}
    last=lastuser(&MainForm->cfg);
    ListView->AllocBy=last;

    ListView->Items->BeginUpdate();
    for(i=0;i<last;i++) {
        user.number=i+1;
		if(fgetuserdat(&MainForm->cfg, &user, file)!=0)
			continue;
        if(user.misc&DELETED)
            continue;
        Item=ListView->Items->Add();
        Item->Caption=AnsiString(i+1);
        Item->SubItems->Add(user.alias);
        Item->SubItems->Add(user.name);
        Item->SubItems->Add(user.level);
        Item->SubItems->Add((int)getage(&MainForm->cfg,user.birth));
        if(user.sex<=' ' || user.sex&0x80)  /* garbage? */
            str[0]=0;
        else
            sprintf(str,"%c",user.sex);
        Item->SubItems->Add(str);
        Item->SubItems->Add(user.location);
        Item->SubItems->Add(user.modem);
        Item->SubItems->Add(user.note);
        Item->SubItems->Add(user.comp);
        Item->SubItems->Add(user.phone);
        Item->SubItems->Add(user.netmail);
        Item->SubItems->Add(user.logons);
        Item->SubItems->Add(unixtodstr(&MainForm->cfg,user.firston,str));
        Item->SubItems->Add(unixtodstr(&MainForm->cfg,user.laston,str));
    }
    ListView->Items->EndUpdate();
    
	close(file);
    Screen->Cursor=crDefault;
}
//---------------------------------------------------------------------------
void __fastcall TUserListForm::ListViewColumnClick(TObject *Sender,
      TListColumn *Column)
{
    if(Column->Index == ColumnToSort)
        SortBackwards=!SortBackwards;
    else
        SortBackwards=false;
    ColumnToSort = Column->Index;
    ((TCustomListView *)Sender)->AlphaSort();
}
//---------------------------------------------------------------------------
void __fastcall TUserListForm::ListViewCompare(TObject *Sender,
      TListItem *Item1, TListItem *Item2, int Data, int &Compare)
{
    /* Decimal compare */
    if (ColumnToSort == 0 || ColumnToSort==3 || ColumnToSort==4
    || ColumnToSort == 12   /* logons */
    || ColumnToSort == 13   /* First On */
    || ColumnToSort == 14   /* Last On */
    ) {
        int num1, num2;

        if(ColumnToSort==0) {
            num1=Item1->Caption.ToIntDef(0);
            num2=Item2->Caption.ToIntDef(0);
        } else if(ColumnToSort>12) {    /* Date */
            int ix = ColumnToSort - 1;
            num1=dstrtounix(&MainForm->cfg
                ,Item1->SubItems->Strings[ix].c_str());
            num2=dstrtounix(&MainForm->cfg
                ,Item2->SubItems->Strings[ix].c_str());
        } else {
            int ix = ColumnToSort - 1;
            num1=Item1->SubItems->Strings[ix].ToIntDef(0);
            num2=Item2->SubItems->Strings[ix].ToIntDef(0);
        }
        if(SortBackwards)
            Compare=(num2-num1);
        else
            Compare=(num1-num2);
    } else {
        int ix = ColumnToSort - 1;
        if(SortBackwards)
            Compare = CompareText(Item2->SubItems->Strings[ix]
                ,Item1->SubItems->Strings[ix]);
        else
            Compare = CompareText(Item1->SubItems->Strings[ix]
                ,Item2->SubItems->Strings[ix]);
    }
}
//---------------------------------------------------------------------------
void __fastcall TUserListForm::FormClose(TObject *Sender,
      TCloseAction &Action)
{
    ListView->Items->BeginUpdate();
    ListView->Items->Clear();
    ListView->Items->EndUpdate();
}
//---------------------------------------------------------------------------
void __fastcall TUserListForm::EditUserPopupClick(TObject *Sender)
{
    char str[256];

    if(ListView->Selected==NULL)
        return;
    sprintf(str,"%sUSEREDIT %s %s"
        ,MainForm->cfg.exec_dir,MainForm->cfg.data_dir
        ,ListView->Selected->Caption.c_str());
    WinExec(str,SW_SHOWNORMAL);
}
//---------------------------------------------------------------------------

void __fastcall TUserListForm::ListViewKeyPress(TObject *Sender, char &Key)
{
    if(Key=='\r')
        EditUserPopupClick(Sender);
}
//---------------------------------------------------------------------------
static AnsiString ListItemString(TListItem* item)
{
    AnsiString str = item->Caption;
    int i;

    for(i=0;i<item->SubItems->Count;i++)
        str += "\t" + item->SubItems->Strings[i];

    return str + "\r\n";
}
//---------------------------------------------------------------------------
void __fastcall TUserListForm::CopyPopupClick(TObject *Sender)
{
    if(ListView->Selected==NULL)
        return;
    Clipboard()->SetTextBuf(ListItemString(ListView->Selected).c_str());
}
//---------------------------------------------------------------------------
void __fastcall TUserListForm::CopyAllPopupClick(TObject *Sender)
{
    AnsiString buf;
    int i;

    for(i=0;i<ListView->Items->Count;i++)
        buf += ListItemString(ListView->Items->Item[i]);

    Clipboard()->SetTextBuf(buf.c_str());
}
//---------------------------------------------------------------------------

void __fastcall TUserListForm::RefreshPopupClick(TObject *Sender)
{
    ListView->Items->BeginUpdate();
    ListView->Items->Clear();
    ListView->Items->EndUpdate();    
    FormShow(Sender);
}
//---------------------------------------------------------------------------

