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

 #include <vcl.h>
#pragma hdrstop

#include "MainFormUnit.h"
#include "SpyFormUnit.h"

#define SPYBUF_LEN  10000
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "Emulvt"
#pragma resource "*.dfm"
TSpyForm *SpyForm;
//---------------------------------------------------------------------------
__fastcall TSpyForm::TSpyForm(TComponent* Owner)
    : TForm(Owner)
{
    Width=MainForm->SpyTerminalWidth;
    Height=MainForm->SpyTerminalHeight;
    KeyboardActive->Checked=MainForm->SpyTerminalKeyboardActive;
    Terminal = new TEmulVT(this);
    Terminal->Parent=this;
    Terminal->Align=alClient;
    Terminal->OnKeyPress=FormKeyPress;
    Terminal->OnMouseUp=FormMouseUp;
    ActiveControl=Terminal;
}
//---------------------------------------------------------------------------
void __fastcall TSpyForm::SpyTimerTick(TObject *Sender)
{
    char    buf[1024];
    int     rd;

    if(*outbuf==NULL)
        return;

    rd=RingBufRead(*outbuf,buf,sizeof(buf)-1);
    if(rd) {
        Terminal->WriteBuffer(buf,rd);
        Timer->Interval=1;
    } else
        Timer->Interval=250;
}
//---------------------------------------------------------------------------
void __fastcall TSpyForm::FormShow(TObject *Sender)
{
    if((*outbuf=(RingBuf*)malloc(sizeof(RingBuf)))==NULL) {
        Terminal->WriteStr("Malloc failure!");
        return;
    }
    RingBufInit(*outbuf,SPYBUF_LEN);

    Timer->Enabled=true;

    Terminal->Font=MainForm->SpyTerminalFont;
    Terminal->Clear();
    Terminal->WriteStr("*** Synchronet Local Spy ***\r\n\r\n");
    Terminal->WriteStr("ANSI Terminal Emulation:"+CopyRight+"\r\n\r\n");
    KeyboardActiveClick(Sender);
}
//---------------------------------------------------------------------------

void __fastcall TSpyForm::FormClose(TObject *Sender, TCloseAction &Action)
{
    Timer->Enabled=false;
    if(*outbuf!=NULL) {
        RingBufDispose(*outbuf);
        free(*outbuf);
        *outbuf=NULL;
    }
    MainForm->SpyTerminalWidth=Width;
    MainForm->SpyTerminalHeight=Height;
    MainForm->SpyTerminalKeyboardActive=KeyboardActive->Checked;
}
//---------------------------------------------------------------------------

void __fastcall TSpyForm::ChangeFontClick(TObject *Sender)
{
	TFontDialog *FontDialog=new TFontDialog(this);

    FontDialog->Font=MainForm->SpyTerminalFont;
    FontDialog->Execute();
    MainForm->SpyTerminalFont->Assign(FontDialog->Font);
    Terminal->Font=MainForm->SpyTerminalFont;
    delete FontDialog;
    Terminal->UpdateScreen();
}
//---------------------------------------------------------------------------

void __fastcall TSpyForm::FormKeyPress(TObject *Sender, char &Key)
{
    if(KeyboardActive->Checked && inbuf!=NULL && *inbuf!=NULL)
        RingBufWrite(*inbuf,&Key,1);

}
//---------------------------------------------------------------------------

void __fastcall TSpyForm::KeyboardActiveClick(TObject *Sender)
{
    KeyboardActive->Checked=!KeyboardActive->Checked;
    if(KeyboardActive->Checked)
        Terminal->Cursor=crIBeam;
    else
        Terminal->Cursor=crDefault;
}
//---------------------------------------------------------------------------

void __fastcall TSpyForm::FormMouseUp(TObject *Sender, TMouseButton Button,
      TShiftState Shift, int X, int Y)
{
    if(Button==mbRight)
        PopupMenu->Popup(ClientOrigin.x+X,ClientOrigin.y+Y);
}
//---------------------------------------------------------------------------


