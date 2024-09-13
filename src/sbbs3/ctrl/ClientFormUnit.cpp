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
#pragma hdrstop

#include <stdio.h>      // sprintf
#include "sockwrap.h"	// closesocket
#include "trash.h"		// filter_ip
#include "scfglib.h"	// trashcan_fname
#include "ClientFormUnit.h"
#include "CodeInputFormUnit.h"

void socket_open(void*, BOOL open);
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TClientForm *ClientForm;
//---------------------------------------------------------------------------
__fastcall TClientForm::TClientForm(TComponent* Owner)
    : TForm(Owner)
{
	MainForm=(TMainForm*)Application->MainForm;
    ListMutex=CreateMutex(NULL,false,NULL);
}
//---------------------------------------------------------------------------
void __fastcall TClientForm::FormHide(TObject *Sender)
{
    MainForm->ViewClients->Checked=false;
}
//---------------------------------------------------------------------------
void __fastcall TClientForm::TimerTimer(TObject *Sender)
{
    char    str[64];
    int     i;
    time_t  t;

    if(WaitForSingleObject(ListMutex,1)!=WAIT_OBJECT_0)
        return;
	ListView->Items->BeginUpdate();
    for(i=0;i<ListView->Items->Count;i++) {
        t=time(NULL)-(ulong)ListView->Items->Item[i]->Data;
        if(t/(60*60))
            sprintf(str,"%d:%02d:%02d",t/(60*60),(t/60)%60,t%60);
        else
            sprintf(str,"%d:%02d",(t/60)%60,t%60);
        ListView->Items->Item[i]->SubItems->Strings[5]=str;

    }
	ListView->Items->EndUpdate();
    ReleaseMutex(ListMutex);
}
//---------------------------------------------------------------------------

void __fastcall TClientForm::CloseSocketMenuItemClick(TObject *Sender)
{
    TListItem* ListItem;
    TItemStates State;

    ListItem=ListView->Selected;
    State << isSelected;

    while(ListItem!=NULL) {
        closesocket(atoi(ListItem->Caption.c_str()));
        ListItem=ListView->GetNextItem(ListItem,sdAll,State);
    }
}
//---------------------------------------------------------------------------

void __fastcall TClientForm::FilterIpMenuItemClick(TObject *Sender)
{
	char 	str[256];
	char	fname[MAX_PATH + 1];
    int		res;
	bool	repeat = false;
    TListItem* ListItem;
    TItemStates State;
	static uint duration = MainForm->global.login_attempt.filter_duration;
	static bool silent;
	static char reason[128] = "Abuse";

    ListItem=ListView->Selected;
    State << isSelected;

    while(ListItem!=NULL) {

   	    AnsiString prot 	= ListItem->SubItems->Strings[0];
	    AnsiString username = ListItem->SubItems->Strings[1];
    	AnsiString ip_addr 	= ListItem->SubItems->Strings[2];
		AnsiString hostname = ListItem->SubItems->Strings[3];
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
				CodeInputForm->LeftCheckBox->Hint = "Filter all selected clients";
				CodeInputForm->LeftCheckBox->ShowHint = true;
			}
			res = CodeInputForm->ShowModal();
			duration = parse_duration(CodeInputForm->Edit->Text.c_str());
			silent = CodeInputForm->RightCheckBox->Checked;
			repeat = CodeInputForm->LeftCheckBox->Checked;
			if(res != mrOk) {
				delete CodeInputForm;
				break;
			}
			CodeInputForm->Label->Caption = "Reason";
			CodeInputForm->Edit->Text = reason;
			CodeInputForm->Edit->Hint = "The cause or rationale for the filter";
			CodeInputForm->RightCheckBox->Visible = false;
			res = CodeInputForm->ShowModal();
			SAFECOPY(reason, CodeInputForm->Edit->Text.c_str());
			delete CodeInputForm;
			if(res != mrOk)
				break;
		}
		filter_ip(&MainForm->cfg,prot.c_str()
			,reason
			,hostname.c_str()
			,ip_addr.c_str(),username.c_str()
			,trashcan_fname(&MainForm->cfg, silent ? "ip-silent" : "ip", fname, sizeof fname)
			,duration);
        if(ListView->Selected == NULL)
        	break;
        ListItem=ListView->GetNextItem(ListItem,sdAll,State);
    }
}
//---------------------------------------------------------------------------

void __fastcall TClientForm::SelectAllMenuItemClick(TObject *Sender)
{
	for(int i = 0; i < ListView->Items->Count; ++i)
		ListView->Items->Item[i]->Selected = true;
}
//---------------------------------------------------------------------------

