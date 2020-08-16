/* Synchronet Control Panel (GUI Borland C++ Builder Project for Win32) */

/* $Id: StatsLogFormUnit.cpp,v 1.4 2018/07/24 01:11:29 rswindell Exp $ */

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
#include <vcl.h>
#pragma hdrstop

#include <io.h>			// filelength()
#include <stdio.h>		// sprintf()
#include <time.h>		// time_t
#include <fcntl.h>		// O_RDONLY
#include <share.h>
#include "MainFormUnit.h"
#include "StatsLogFormUnit.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TStatsLogForm *StatsLogForm;
//---------------------------------------------------------------------------
__fastcall TStatsLogForm::TStatsLogForm(TComponent* Owner)
	: TForm(Owner)
{
}
//---------------------------------------------------------------------------
void __fastcall TStatsLogForm::FormShow(TObject *Sender)
{
	char str[256],path[256],*p;
    BYTE *buf;
	int i,file;
    time_t timestamp;
    struct tm* tm;
    long l;
    DWORD   length,
            logons,
            timeon,
            posts,
            emails,
            fbacks,
            ulb,
            uls,
            dlb,
            dls;

    sprintf(path,"%sCSTS.DAB",MainForm->global.ctrl_dir);
    if((file=_sopen(path,O_RDONLY|O_BINARY,SH_DENYNO))==-1) {
        sprintf(str,"!Error opening %s",path);
        Log->Lines->Add(AnsiString(str));
        return;
    }
    length=filelength(file);
    if(length<40) {
        close(file);
        return;
    }
    if((buf=(char *)malloc(length))==NULL) {
        close(file);
        sprintf(str,"!Error allocating %lu bytes",length);
        Log->Lines->Add(AnsiString(str));
        return;
    }
    read(file,buf,length);
    close(file);
    l=length-4;
    while(l>-1L) {
        fbacks=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        emails=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        posts=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        dlb=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        dls=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        ulb=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        uls=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        timeon=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        logons=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        timestamp=buf[l]|((long)buf[l+1]<<8)|((long)buf[l+2]<<16)
            |((long)buf[l+3]<<24);
        l-=4;
        timestamp-=(24*60*60); /* 1 day less than stamp */
        tm=localtime(&timestamp);
        sprintf(str,"%2.2d/%2.2d/%2.2d T:%5lu   L:%3lu   P:%3lu   "
            "E:%3lu   F:%3lu   U:%6luk %3lu  D:%6luk %3lu"
            ,tm->tm_mon+1,tm->tm_mday,tm->tm_year%100,timeon,logons,posts,emails
            ,fbacks,ulb/1024,uls,dlb/1024,dls);
        Log->Lines->Add(AnsiString(str));
    }
	free(buf);
}
//---------------------------------------------------------------------------
