/* chat.cpp */

/* Local sysop chat module (GUI Borland C++ Builder Project for Win32) */

/* $Id: chat.cpp,v 1.4 2018/07/24 01:11:20 rswindell Exp $ */

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
#include <stdio.h>      // sprintf
#pragma hdrstop
USERES("chat.res");
USEFORM("MainFormUnit.cpp", MainForm);
int     node_num=0;
char    node_number[32]="";
char    ctrl_dir[MAX_PATH+1]="";
char    node_dir[MAX_PATH+1]="";
char    user_name[128]="";
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR cmdline, int)
{
    char*   p;
    char*   argv=NULL;
    char    str[256];
    int     argc=0;
    int     len=0;

    wsprintf(str,"CHAT: %s\r\n",cmdline);
    OutputDebugString(str);

    for(p=cmdline;*p;p++) {
        switch(argc) {
            case 0:
                argv=ctrl_dir;
                break;
            case 1:
                argv=node_dir;
                break;
            case 2:
                argv=node_number;
                break;
            default:
                argv=user_name;
                break;
        }
        if(*p==' ' && argc<3) {
            argv[len]=0;
            argc++;
            len=0;
            continue;
        }
        argv[len++]=*p;
    }
    if(argv!=NULL)
        argv[len]=0;

    if(!user_name[0]) {
        char errmsg[512];
        sprintf(errmsg,"Invalid command-line: '%s'",cmdline);
	Application->MessageBox(errmsg
            ,"Synchronet Chat",MB_OK|MB_ICONEXCLAMATION);
        return 0;
    }

    node_num=(atoi(node_number));

    try
    {
         Application->Initialize();
         Application->Title = "Synchronet Sysop Chat";
		Application->CreateForm(__classid(TMainForm), &MainForm);
         Application->Run();
    }
    catch (Exception &exception)
    {
         Application->ShowException(&exception);
    }
    return 0;
}
//---------------------------------------------------------------------------
