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

#include <vcl.h>
#include <vcl/Clipbrd.hpp>
#pragma hdrstop

#include "LoginAttemptsFormUnit.h"
#include "MainFormUnit.h"
#include "CodeInputFormUnit.h"
#include "userdat.h"
#include "datewrap.h"
#include "trash.h"
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
        if(node->data == NULL)
            continue;
        if((attempt = (login_attempt_t*)malloc(sizeof(*attempt))) == NULL)
            continue;
        *attempt = *(login_attempt_t*)node->data;
        Item=ListView->Items->Add();
        Item->Caption=AnsiString(attempt->count-attempt->dupes);
        Item->Data=(void*)attempt;
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
        if(ColumnToSort==6 && Item1->Data && Item2->Data) { /* Date */
            num1 = ((login_attempt_t*)Item1->Data)->time;
            num2 = ((login_attempt_t*)Item2->Data)->time;
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
    AnsiString buf;
    TItemStates State;
    State << isSelected;
    TListItem* ListItem = ListView->Selected;

    while(ListItem != NULL) {
        buf += ListItemString(ListItem);
        ListItem = ListView->GetNextItem(ListItem, sdAll, State);
    }
    Clipboard()->SetTextBuf(buf.c_str());
}
//---------------------------------------------------------------------------

void __fastcall TLoginAttemptsForm::SelectAllPopupClick(TObject *Sender)
{
	ListView->SelectAll();
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
	char	fname[MAX_PATH + 1];
    int		res;
	bool	repeat = false;
    TListItem* ListItem;
    TItemStates State;
	static uint duration = MainForm->global.login_attempt.filter_duration;
	static bool silent;
	static bool apply_all;

    ListItem=ListView->Selected;
    State << isSelected;

    Screen->Cursor=crHourGlass;
    ListView->Items->BeginUpdate();

    while(ListItem!=NULL) {
        TListItem* NextItem = ListView->GetNextItem(ListItem,sdAll,State);
    	AnsiString ip_addr 	= ListItem->SubItems->Strings[1];
		if(!repeat) {
			Application->CreateForm(__classid(TCodeInputForm), &CodeInputForm);
			wsprintf(str,"Disallow future connections from %s?", ip_addr);
			CodeInputForm->Caption = str;
			CodeInputForm->Label->Caption = "Address Filter Duration";
			CodeInputForm->Edit->Visible = true;
			CodeInputForm->Edit->Text = duration ? duration_to_vstr(duration, str, sizeof str) : "Infinite";
			CodeInputForm->Edit->Hint = "'Infinite' or number of Seconds/Minutes/Hours/Days/Weeks/Years";
			CodeInputForm->Edit->ShowHint = true;
			CodeInputForm->RightCheckBox->Visible = true;
			CodeInputForm->RightCheckBox->Caption = "Silent Filter";
			CodeInputForm->RightCheckBox->Checked = silent;
			CodeInputForm->RightCheckBox->Hint = "No messages logged when blocking this client";
			CodeInputForm->RightCheckBox->ShowHint = true;
			if(ListView->SelCount > 1) {
				CodeInputForm->LeftCheckBox->Visible = true;
				CodeInputForm->LeftCheckBox->Caption = "All Selected";
				CodeInputForm->LeftCheckBox->Checked = apply_all;
				CodeInputForm->LeftCheckBox->Hint = "Filter all selected addresses";
				CodeInputForm->LeftCheckBox->ShowHint = true;
			}
			res = CodeInputForm->ShowModal();
			duration = parse_duration(CodeInputForm->Edit->Text.c_str());
			silent = CodeInputForm->RightCheckBox->Checked;
			repeat = CodeInputForm->LeftCheckBox->Checked;
			apply_all = repeat;
			delete CodeInputForm;
			if(res != mrOk)
				break;
		}
        login_attempt_t* attempt = (login_attempt_t*)ListItem->Data;
        if(attempt != NULL) {
            char* hostname = NULL;
            struct in_addr addr = {};
            addr.s_addr = inet_addr(ip_addr.c_str());
            HOSTENT* h = gethostbyaddr((char *)&addr,sizeof(addr),AF_INET);
            if(h!=NULL)
                hostname = h->h_name;
            char tmp[128];
            AnsiString reason = ListItem->Caption + AnsiString(" " STR_FAILED_LOGIN_ATTEMPTS " in ");
            reason += duration_estimate_to_str(attempt->time - attempt->first, tmp, sizeof tmp, 1, 1);
            if(filter_ip(&MainForm->cfg, attempt->prot, reason.c_str(), hostname
				,ip_addr.c_str(), attempt->user
				,trashcan_fname(&MainForm->cfg, silent ? "ip-silent" : "ip", fname, sizeof fname)
				,duration)) {
                loginSuccess(&login_attempt_list, &attempt->addr);
                ListItem->Delete();
            }
        }
        ListItem = NextItem;
    }
    ListView->Items->EndUpdate();
    Screen->Cursor=crDefault;
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

void __fastcall TLoginAttemptsForm::ListViewDeletion(TObject *Sender,
      TListItem *Item)
{
    free(Item->Data);
}
//---------------------------------------------------------------------------

void __fastcall TLoginAttemptsForm::Remove1Click(TObject *Sender)
{
    TItemStates State;

    TListItem* ListItem = ListView->Selected;
    State << isSelected;

    Screen->Cursor=crHourGlass;
    ListView->Items->BeginUpdate();

    while (ListItem != NULL) {
        TListItem* NextItem = ListView->GetNextItem(ListItem, sdAll, State);
        login_attempt_t* attempt = (login_attempt_t*)ListItem->Data;
        if(attempt != NULL) {
            loginSuccess(&login_attempt_list, &attempt->addr);
            ListItem->Delete();
        }
        ListItem = NextItem;
    }
    ListView->Items->EndUpdate();
    Screen->Cursor=crDefault;
}
//---------------------------------------------------------------------------

