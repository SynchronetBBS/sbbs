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
#ifdef USE_CRYPTLIB
#include "cryptlib.h"
#endif

#include <vcl.h>
#pragma hdrstop

#include "AboutBoxFormUnit.h"
#include "emulvt.hpp"
#include "mailsrvr.h"
#include "ftpsrvr.h"
#include "websrvr.h"
#include "services.h"
#include "git_branch.h"
#include "git_hash.h"

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TAboutBoxForm *AboutBoxForm;
//---------------------------------------------------------------------------
__fastcall TAboutBoxForm::TAboutBoxForm(TComponent* Owner)
	: TForm(Owner)
{
}

const char* ver(void)
{
    DWORD i;
    unsigned int len=GetFileVersionInfoSize(
         Application->ExeName.c_str()	// pointer to filename string
        ,&i 	        // pointer to variable to receive zero
        );
    BYTE* buf=(BYTE *)malloc(len);
    if(buf==NULL) {
        return "MALLOC ERROR!";
    }
    GetFileVersionInfo(
         Application->ExeName.c_str()	// pointer to filename string
        ,i	            // ignored
        ,len	        // size of buffer
        ,buf 	        // pointer to buffer to receive file-version info.
        );

    VS_FIXEDFILEINFO* Ver;
    len=sizeof(Ver);
    VerQueryValue(
         buf            // address of buffer for version resource
        ,"\\"	        // address of value to retrieve
        ,(void **)&Ver  // address of buffer for version pointer
        ,&len	        // address of version-value length buffer
        );
    free(buf);

    char compiler[32];
	wsprintf(compiler,"BCC %X.%02X"
		,__BORLANDC__>>8
		,__BORLANDC__&0xff);

    static char ver[256];
    wsprintf(ver,"Synchronet Control Panel v%u.%u.%u.%u%s%s  "
        "Compiled %s/%s %s with %s"
        ,Ver->dwFileVersionMS>>16
        ,Ver->dwFileVersionMS&0xffff
        ,Ver->dwFileVersionLS>>16
        ,Ver->dwFileVersionLS&0xffff
        ,Ver->dwFileFlags&VS_FF_DEBUG ?
            " Debug" : ""
        ,Ver->dwFileFlags&VS_FF_PRERELEASE ?
            " Pre-release" : ""
		,GIT_BRANCH, GIT_HASH
        ,GIT_DATE, compiler
        );

	return ver;
}

//---------------------------------------------------------------------------
void __fastcall TAboutBoxForm::FormShow(TObject *Sender)
{
    Credits->Lines->Clear();

    Credits->Lines->Add(bbs_ver());
    Credits->Lines->Add(mail_ver());
    Credits->Lines->Add(ftp_ver());
    Credits->Lines->Add(web_ver());    
    Credits->Lines->Add(services_ver());
    Credits->Lines->Add(ver());
    Credits->Lines->Add("Synchronet Local Spy ANSI Terminal Emulation"
        + CopyRight);
    Credits->Lines->Add(AnsiString(js_ver())
        + " (c) 1998 Netscape Communications Corp.");

#ifdef USE_CRYPTLIB
	char ver[256];
    wsprintf(ver,"Cryptlib v%u.%u.%u"
        ,(CRYPTLIB_VERSION/100)
        ,(CRYPTLIB_VERSION/10)%10
        ,CRYPTLIB_VERSION%10);
    Credits->Lines->Add(AnsiString(ver) +
        " Copyright 1992-2023 Peter Gutmann. All rights reserved.");
#endif
}
//---------------------------------------------------------------------------
void __fastcall TAboutBoxForm::WebPageLabelClick(TObject *Sender)
{
    ShellExecute(Handle, "open", ((TLabel*)Sender)->Hint.c_str(),
        NULL,NULL,SW_SHOWDEFAULT);
}
//---------------------------------------------------------------------------
void __fastcall TAboutBoxForm::LogoClick(TObject *Sender)
{
    ShellExecute(Handle, "open", ((TImage*)Sender)->Hint.c_str(),
        NULL,NULL,SW_SHOWDEFAULT);
}
//---------------------------------------------------------------------------
void __fastcall TAboutBoxForm::CreditsClick(TObject *Sender)
{
    ShellExecute(Handle, "open", ((TMemo*)Sender)->Hint.c_str(),
        NULL,NULL,SW_SHOWDEFAULT);
}
//---------------------------------------------------------------------------

