/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2011 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#pragma hdrstop

#include "LoginAttemptsFormUnit.h"
#include "MainFormUnit.h"
#include "userdat.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TLoginAttemptsForm *LoginAttemptsForm;
extern link_list_t login_attempt_list;

//---------------------------------------------------------------------------
__fastcall TLoginAttemptsForm::TLoginAttemptsForm(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TLoginAttemptsForm::FormShow(TObject *Sender)
{
    char                str[128];
    TListItem*          Item;
    list_node_t*        node;
    struct tm			tm;
    login_attempt_t*    attempt;

    ColumnToSort=0;
    SortBackwards=false;
    Screen->Cursor=crAppStart;

    listLock(&login_attempt_list);

    ListView->AllocBy=listCountNodes(&login_attempt_list);

    ListView->Items->BeginUpdate();

    for(node=listFirstNode(&login_attempt_list); node!=NULL; node=listNextNode(node)) {
        attempt=(login_attempt_t*)node->data;
        if(attempt==NULL)
            continue;
        Item=ListView->Items->Add();
        Item->Caption=AnsiString(attempt->count);
        Item->Data=(void*)attempt->time;
        Item->SubItems->Add(attempt->dupes);        
        Item->SubItems->Add(inet_ntoa(attempt->addr));
        Item->SubItems->Add(attempt->prot);
        Item->SubItems->Add(attempt->user);
        Item->SubItems->Add(attempt->pass);
        localtime_r(&attempt->time,&tm);
        sprintf(str,"%u/%u %02u:%02u:%02u"
            ,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
        Item->SubItems->Add(str);
    }
    ListView->Items->EndUpdate();

    listUnlock(&login_attempt_list);
    Screen->Cursor=crDefault;
}
//---------------------------------------------------------------------------
void __fastcall TLoginAttemptsForm::FormClose(TObject *Sender,
      TCloseAction &Action)
{
    ListView->Items->BeginUpdate();
    ListView->Items->Clear();
    ListView->Items->EndUpdate();
}
//---------------------------------------------------------------------------
void __fastcall TLoginAttemptsForm::ListViewColumnClick(TObject *Sender,
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
void __fastcall TLoginAttemptsForm::ListViewCompare(TObject *Sender,
      TListItem *Item1, TListItem *Item2, int Data, int &Compare)
{
    /* Decimal compare */
    if (ColumnToSort < 2 || ColumnToSort == 6) {
        int num1, num2;
        if(ColumnToSort==6) { /* Date */
            num1=(ulong)Item1->Data;
            num2=(ulong)Item2->Data;
        } else {
            num1=Item1->Caption.ToIntDef(0);
            num2=Item2->Caption.ToIntDef(0);
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
