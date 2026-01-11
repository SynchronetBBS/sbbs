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

#include <stdio.h>		// sprintf()
#include <time.h>		// time_t
#include <share.h>
#include "MainFormUnit.h"
#include "StatsLogFormUnit.h"
#include "getstats.h"
#include "dat_file.h"
#include "xpdatetime.h"

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
	char ulbytes[32];
	char dlbytes[32];
	FILE* fp;
    time_t timestamp;
    struct tm* tm;
	stats_t stats = {0};

    SAFEPRINTF(path, "%scsts.tab", MainForm->global.ctrl_dir);
    if((fp = _fsopen(path, "rb", SH_DENYNO)) ==  NULL) {
        SAFEPRINTF(str, "!Error opening %s", path);
        Log->Lines->Add(AnsiString(str));
        return;
    }
	str_list_t columns = NULL;
	Screen->Cursor=crHourGlass;
	str_list_t* records = tabReadFile(fp, &columns);
	fclose(fp);
	long l;
	COUNT_LIST_ITEMS(records, l);
	for(--l; l >= 0; --l) {
		parse_cstats(records[l], &stats);
		timestamp=isoDateTime_to_time(strtoul(records[l][CSTATS_DATE], NULL, 10), 0);
        timestamp-=(24*60*60); /* 1 day less than stamp */
        tm=localtime(&timestamp);
        sprintf(str,"%u/%2.2d/%2.2d T:%5lu   L:%3lu   P:%3lu   "
            "E:%3lu   F:%3lu   U:%7s/%-5lu D:%7s/%-6lu N:%u"
            ,1900 + tm->tm_year,tm->tm_mon+1,tm->tm_mday
			,stats.ttoday,stats.ltoday,stats.ptoday,stats.etoday
            ,stats.ftoday
			,byte_estimate_to_str(stats.ulb, ulbytes, sizeof(ulbytes), 1024 * 1024, 1), stats.uls
			,byte_estimate_to_str(stats.dlb, dlbytes, sizeof(dlbytes), 1024 * 1024, 1), stats.dls
			,stats.nusers
			);
        Log->Lines->Add(AnsiString(str));
    }
	tabListFree(records);
	strListFree(&columns);
	Screen->Cursor=crDefault;
}
//---------------------------------------------------------------------------
