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
#include "ClientFormUnit.h"

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
    for(i=0;i<ListView->Items->Count;i++) {
        t=time(NULL)-(ulong)ListView->Items->Item[i]->Data;
        if(t/(60*60))
            sprintf(str,"%d:%02d:%02d",t/(60*60),(t/60)%60,t%60);
        else
            sprintf(str,"%d:%02d",(t/60)%60,t%60);
        ListView->Items->Item[i]->SubItems->Strings[5]=str;

    }
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
    int		res;
    TListItem* ListItem;
    TItemStates State;

    ListItem=ListView->Selected;
    State << isSelected;

    while(ListItem!=NULL) {

   	    AnsiString prot 	= ListItem->SubItems->Strings[0];
	    AnsiString username = ListItem->SubItems->Strings[1];
    	AnsiString ip_addr 	= ListItem->SubItems->Strings[2];
		AnsiString hostname = ListItem->SubItems->Strings[3];

    	wsprintf(str,"Disallow future connections from %s"
        	,ip_addr);
    	res=Application->MessageBox(str,"Filter IP?"
        		,MB_YESNOCANCEL|MB_ICONQUESTION);
        if(res==IDCANCEL)
    		break;
    	if(res==IDYES)
			filter_ip(&MainForm->cfg,prot.c_str(),"abuse",hostname.c_str()
				,ip_addr.c_str(),username.c_str(), /* filename: */NULL, /* duration: */0);
        if(ListView->Selected == NULL)
        	break;
        ListItem=ListView->GetNextItem(ListItem,sdAll,State);
    }
}
//---------------------------------------------------------------------------

