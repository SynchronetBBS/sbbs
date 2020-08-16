/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: LoginAttemptsFormUnit.cpp,v 1.9 2019/02/15 06:26:05 rswindell Exp $ */

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

#include "LoginAttemptsFormUnit.h"
#include "MainFormUnit.h"
#include "userdat.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TLoginAttemptsForm *LoginAttemptsForm;
extern link_list_t login_attempt_list;
extern bool clearLoginAttemptList;

//---------------------------------------------------------------------------
__fastcall TLoginAttemptsForm::TLoginAttemptsForm(TComponent* Owner)
    : TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TLoginAttemptsForm::FillListView(TObject *Sender)
{
    char                str[128];
    TListItem*          Item;
    list_node_t*        node;
    struct tm			tm;
    login_attempt_t*    attempt;

    Screen->Cursor=crAppStart;

    listLock(&login_attempt_list);

    ListView->AllocBy=listCountNodes(&login_attempt_list);

    ListView->Items->BeginUpdate();

    for(node=login_attempt_list.first; node!=NULL; node=node->next) {
        attempt=(login_attempt_t*)node->data;
        if(attempt==NULL)
            continue;
        Item=ListView->Items->Add();
        Item->Caption=AnsiString(attempt->count-attempt->dupes);
        Item->Data=(void*)attempt->time;
        Item->SubItems->Add(attempt->dupes);
		if(inet_addrtop(&attempt->addr, str, sizeof(str))==NULL)
			strcpy(str, "<invalid address>");
        Item->SubItems->Add(str);
        Item->SubItems->Add(attempt->prot);
        Item->SubItems->Add(attempt->user);
        Item->SubItems->Add(attempt->pass);
        localtime_r(&attempt->time,&tm);
        sprintf(str,"%u/%u/%u %02u:%02u:%02u"
            ,tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
        Item->SubItems->Add(str);
    }
    ListView->AlphaSort();
    ListView->Items->EndUpdate();

    listUnlock(&login_attempt_list);
    Screen->Cursor=crDefault;
}
//---------------------------------------------------------------------------
void __fastcall TLoginAttemptsForm::FormShow(TObject *Sender)
{
    ColumnToSort=0;
    SortBackwards=true;
    FillListView(Sender);
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
        } else if(ColumnToSort==0) {
            num1=Item1->Caption.ToIntDef(0);
            num2=Item2->Caption.ToIntDef(0);
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
static AnsiString ListItemString(TListItem* item)
{
    AnsiString str = item->Caption;
    int i;

    for(i=0;i<item->SubItems->Count;i++)
        str += "\t" + item->SubItems->Strings[i];

    return str + "\r\n";
}
//---------------------------------------------------------------------------
void __fastcall TLoginAttemptsForm::CopyPopupClick(TObject *Sender)
{
    if(ListView->Selected==NULL)
        return;
    Clipboard()->SetTextBuf(ListItemString(ListView->Selected).c_str());
}
//---------------------------------------------------------------------------

void __fastcall TLoginAttemptsForm::CopyAllPopupClick(TObject *Sender)
{
    AnsiString buf;
    int i;

    for(i=0;i<ListView->Items->Count;i++)
        buf += ListItemString(ListView->Items->Item[i]);

    Clipboard()->SetTextBuf(buf.c_str());
}
//---------------------------------------------------------------------------

void __fastcall TLoginAttemptsForm::RefreshPopupClick(TObject *Sender)
{
    ListView->Items->BeginUpdate();
    ListView->Items->Clear();
    ListView->Items->EndUpdate();    
    FillListView(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TLoginAttemptsForm::FilterIpMenuItemClick(TObject *Sender)
{
	char 	str[256];
    int		res;
    TListItem* ListItem;
    TItemStates State;

    ListItem=ListView->Selected;
    State << isSelected;

    while(ListItem!=NULL) {

    	struct in_addr addr;
    	HOSTENT*	h;

    	AnsiString ip_addr 	= ListItem->SubItems->Strings[1];
   	AnsiString prot 	= ListItem->SubItems->Strings[2];
	AnsiString username = ListItem->SubItems->Strings[3];

    	wsprintf(str,"Disallow future connections from %s"
        	,ip_addr);
    	res=Application->MessageBox(str,"Filter IP?"
        		,MB_YESNOCANCEL|MB_ICONQUESTION);
        if(res==IDCANCEL)
    		break;
    	if(res==IDYES) {
		char* hostname = NULL;

	    	addr.s_addr=inet_addr(ip_addr.c_str());
		Screen->Cursor=crHourGlass;
	    	h=gethostbyaddr((char *)&addr,sizeof(addr),AF_INET);
		Screen->Cursor=crDefault;
	        if(h!=NULL)
	            hostname = h->h_name;
		filter_ip(&MainForm->cfg, prot.c_str(), "abuse", hostname
				,ip_addr.c_str(), username.c_str(), NULL);
	}
        if(ListView->Selected == NULL)
        	break;
        ListItem=ListView->GetNextItem(ListItem,sdAll,State);
    }
}
//---------------------------------------------------------------------------

void __fastcall TLoginAttemptsForm::ResolveHostnameMenuItemClick(
      TObject *Sender)
{
	char 	str[256];
    int		res;
    TListItem* ListItem;
    TItemStates State;

    ListItem=ListView->Selected;
    State << isSelected;

    while(ListItem!=NULL) {

    	struct in_addr addr;
    	HOSTENT*	h;
        char*       ip_addr=ListItem->SubItems->Strings[1].c_str();
        char*       hostname="<unknown>";

       	addr.s_addr=inet_addr(ip_addr);
		Screen->Cursor=crHourGlass;
	    	h=gethostbyaddr((char *)&addr,sizeof(addr),AF_INET);
		Screen->Cursor=crDefault;
        if(h!=NULL)
            hostname = h->h_name;
    	wsprintf(str,"%s hostname is %s", ip_addr, hostname);

        Application->MessageBox(str, "Hostname Lookup", MB_OK|MB_ICONINFORMATION);

        if(ListView->Selected == NULL)
        	break;
        ListItem=ListView->GetNextItem(ListItem,sdAll,State);
    }
}
//---------------------------------------------------------------------------

void __fastcall TLoginAttemptsForm::ClearListMenuItemClick(TObject *Sender)
{
    clearLoginAttemptList = true;
    ListView->Items->BeginUpdate();
    ListView->Items->Clear();
    ListView->Items->EndUpdate();    
}
//---------------------------------------------------------------------------

