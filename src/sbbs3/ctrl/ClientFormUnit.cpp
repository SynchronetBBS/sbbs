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

#include <stdio.h>      // sprintf
#include <winsock.h>    // closesocket
#include "ClientFormUnit.h"

void socket_open(BOOL open);
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
void __fastcall TClientForm::FormShow(TObject *Sender)
{
    MainForm->ViewClients->Checked=true;
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
        if(closesocket(atoi(ListItem->Caption.c_str()))==0)
            socket_open(FALSE);
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

    	wsprintf(str,"Disallow future connections from %s"
        	,ListItem->SubItems->Strings[2].c_str());
    	res=Application->MessageBox(str,"Filter IP?"
        		,MB_YESNOCANCEL|MB_ICONQUESTION);
        if(res==IDCANCEL)
    		break;
    	if(res==IDOK)
	        MainForm->FilterIP(
    	     	 ListItem->SubItems->Strings[2].c_str() /* ip_addr */
	        	,ListItem->SubItems->Strings[0].c_str() /* protocol */
	        	,ListItem->SubItems->Strings[1].c_str() /* username */
	            );
        ListItem=ListView->GetNextItem(ListItem,sdAll,State);
    }
}
//---------------------------------------------------------------------------

