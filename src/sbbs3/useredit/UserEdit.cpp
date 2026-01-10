/* Synchronet User Editor for Windows, C++ Builder */

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
#include "MainFormUnit.h"
#include "getctrl.h"
#include "scfglib.h"
#include "userdat.h"
#pragma hdrstop
//---------------------------------------------------------------------------
USEFORM("MainFormUnit.cpp", MainForm);
//---------------------------------------------------------------------------
WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR cmd, int)
{
    try
    {
         Application->Initialize();
         Application->Title = "Synchronet User Editor";
         Application->CreateForm(__classid(TMainForm), &MainForm);
         SAFECOPY(MainForm->cfg.ctrl_dir, get_ctrl_dir(/* warn: */FALSE));
         backslash(MainForm->cfg.ctrl_dir);
         char error[256];
         if(!read_main_cfg(&MainForm->cfg, error, sizeof(error))) {
             Application->MessageBox(error, "Error Loading Configuration"
                ,MB_OK | MB_ICONEXCLAMATION);
         } else {
            if(cmd[0]) {
                char* p = strchr(cmd, ' ');
                if(p != NULL) {
                    *(p++) = '\0';
                    SKIP_WHITESPACE(p);
                    MainForm->user.number = atoi(p);
                }
                SAFECOPY(MainForm->cfg.data_dir, cmd);
                backslash(MainForm->cfg.data_dir);
            } else
                prep_dir(MainForm->cfg.ctrl_dir, MainForm->cfg.data_dir, sizeof(MainForm->cfg.data_dir));
			char path[MAX_PATH + 1];
			snprintf(path, sizeof path, "%suser", MainForm->cfg.data_dir);
			if(!isdir(path) && mkdir(path) != 0) {
				snprintf(error, sizeof error, "%s does not exist and cannot create it", path);
				Application->MessageBox(error, "Error"
					,MB_OK | MB_ICONEXCLAMATION);
			} else {
				userdat_filename(&MainForm->cfg, MainForm->user_filename, sizeof MainForm->user_filename);
				Application->Run();
			}
         }
    }
    catch (Exception &exception)
    {
         Application->ShowException(&exception);
    }
    catch (...)
    {
         try
         {
             throw Exception("");
         }
         catch (Exception &exception)
         {
             Application->ShowException(&exception);
         }
    }
    return 0;
}
//---------------------------------------------------------------------------
