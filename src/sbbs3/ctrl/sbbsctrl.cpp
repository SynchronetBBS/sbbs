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
USEFORM("MainFormUnit.cpp", MainForm);
USEFORM("CtrlPathDialogUnit.cpp", CtrlPathDialog);
USEFORM("TextFileEditUnit.cpp", TextFileEditForm);
USEFORM("TelnetFormUnit.cpp", TelnetForm);
USEFORM("FtpFormUnit.cpp", FtpForm);
USEFORM("MailFormUnit.cpp", MailForm);
USEFORM("NodeFormUnit.cpp", NodeForm);
USEFORM("StatsFormUnit.cpp", StatsForm);
USEFORM("AboutBoxFormUnit.cpp", AboutBoxForm);
USEFORM("StatsLogFormUnit.cpp", StatsLogForm);
USEFORM("CodeInputFormUnit.cpp", CodeInputForm);
USEFORM("ClientFormUnit.cpp", ClientForm);
USEFORM("SpyFormUnit.cpp", SpyForm);
USEFORM("UserListFormUnit.cpp", UserListForm);
USEFORM("UserMsgFormUnit.cpp", UserMsgForm);
USEFORM("PropertiesDlgUnit.cpp", PropertiesDlg);
USEFORM("EventsFormUnit.cpp", EventsForm);
USEFORM("ConfigWizardUnit.cpp", ConfigWizardForm);
USEFORM("ServicesFormUnit.cpp", ServicesForm);
USEFORM("TelnetCfgDlgUnit.cpp", TelnetCfgDlg);
USEFORM("MailCfgDlgUnit.cpp", MailCfgDlg);
USEFORM("FtpCfgDlgUnit.cpp", FtpCfgDlg);
USEFORM("ServicesCfgDlgUnit.cpp", ServicesCfgDlg);
USEFORM("PreviewFormUnit.cpp", PreviewForm);
//---------------------------------------------------------------------------
#include "MainFormUnit.h"
#include "SpyFormUnit.h"
#include "CtrlPathDialogUnit.h"
TSpyForm *SpyForms[MAX_NODES];
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR cmd, int)
{
    memset(SpyForms,0,sizeof(SpyForms));
    try
    {
        Application->Initialize();
        Application->Title = "Synchronet Control Panel";
		Application->CreateForm(__classid(TMainForm), &MainForm);
		Application->CreateForm(__classid(TTelnetForm), &TelnetForm);
		Application->CreateForm(__classid(TFtpForm), &FtpForm);
		Application->CreateForm(__classid(TMailForm), &MailForm);
		Application->CreateForm(__classid(TNodeForm), &NodeForm);
		Application->CreateForm(__classid(TStatsForm), &StatsForm);
		Application->CreateForm(__classid(TClientForm), &ClientForm);
		Application->CreateForm(__classid(TUserListForm), &UserListForm);
		Application->CreateForm(__classid(TEventsForm), &EventsForm);
		Application->CreateForm(__classid(TServicesForm), &ServicesForm);
		Application->Run();
    }
    catch (Exception &exception)
    {
             Application->ShowException(&exception);
    }
    return 0;
}
//---------------------------------------------------------------------------
