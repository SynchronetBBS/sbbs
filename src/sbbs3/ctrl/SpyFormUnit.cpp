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

 #include <vcl.h>
#pragma hdrstop

#include "MainFormUnit.h"
#include "SpyFormUnit.h"
#include "telnet.h"
#include "str_util.h"

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
	IdleCount = 0;
    Width=MainForm->SpyTerminalWidth;
    Height=MainForm->SpyTerminalHeight;
    Terminal = new TEmulVT(this);
    Terminal->Parent=this;
    Terminal->Align=alClient;
    Terminal->OnKeyPress=FormKeyPress;
    Terminal->OnMouseUp=FormMouseUp;
    Terminal->AutoWrap=true;
    ActiveControl=Terminal;
}
//---------------------------------------------------------------------------
__fastcall TSpyForm::~TSpyForm()
{
	delete Terminal;
}
//---------------------------------------------------------------------------
int __fastcall TSpyForm::strip_telnet(uchar *buf, int len)
{
    int i;
    int telnet_cmd=0;
    int newlen=0;

    for(i=0;i<len;i++) {
		if(buf[i]==FF) {
			Terminal->Clear();
			continue;
		}
        if(buf[i]==TELNET_IAC || telnet_cmd) {
            if(telnet_cmd==1 && buf[i]==TELNET_IAC) {
                telnet_cmd=0;   /* escape IAC */
                continue;
            }
            if(telnet_cmd==1 && buf[i]<TELNET_WILL) {
                telnet_cmd=0;   /* single byte command */
                continue;
            }
            if(telnet_cmd>=2) {
                telnet_cmd=0;   /* two byte command */
                continue;
            }
            telnet_cmd++;
            continue;
        }
        buf[newlen++]=buf[i];
    }
    return(newlen);
}
//---------------------------------------------------------------------------
void __fastcall TSpyForm::SpyTimerTick(TObject *Sender)
{
    uchar   buf[8192];
    int     rd;

    if(*outbuf==NULL)
        return;

    rd=RingBufRead(*outbuf,buf,sizeof(buf) - 1);
    if(rd > 0) {
		if(IdleCount >= 2) {
			if(fdate(TerminalIniFile.c_str()) > terminal_fdate)
				ReadTerminalIniFile();
		}
        rd=strip_telnet(buf,rd);
		buf[rd] = 0;
		if(utf8)
			utf8_to_cp437_inplace(buf);
		Terminal->WriteBuffer(buf, strlen(buf));
		IdleCount = 0;
        Timer->Interval=1;
    } else {
		++IdleCount;
        Timer->Interval=250;
	}
}
//---------------------------------------------------------------------------
void __fastcall TSpyForm::ReadTerminalIniFile()
{
	FILE* fp = iniOpenFile(TerminalIniFile.c_str(), /* for_modify: */FALSE);
	if(fp != NULL) {
		char type[128] = {0};
		char chars[128] = {0};
		iniReadString(fp, ROOT_SECTION, "type", /* default: */NULL, type);
		iniReadString(fp, ROOT_SECTION, "chars", /* default: */NULL, chars);
		Terminal->Cols = iniReadInteger(fp, ROOT_SECTION, "cols", 80);
		Terminal->Rows = iniReadInteger(fp, ROOT_SECTION, "rows", 25);
		fclose(fp);
		utf8 = stricmp(chars, "UTF-8") == 0;
		Caption = "Node " + AnsiString(NodeNum) + " ("
			+ AnsiString(Terminal->Cols) + "x" + AnsiString(Terminal->Rows) + " "
			+ chars + " / " + type + ")";
	}
	terminal_fdate = fdate(TerminalIniFile.c_str());
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
	ReadTerminalIniFile();
	Terminal->WriteStr("ANSI Terminal Emulation:"+CopyRight+"\r\n\r\n");

    KeyboardActive->Checked=!MainForm->SpyTerminalKeyboardActive;
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


