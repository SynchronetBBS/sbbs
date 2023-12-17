/* Synchronet client/content-filtering (trashcan) functions */

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

#include "trash.h"
#include "datewrap.h"
#include "ini_file.h"
#include "scfglib.h"
#include "findstr.h"

/****************************************************************************/
/* Searches the file <name>.can in the TEXT directory for matches			*/
/* Returns TRUE if found in list, FALSE if not.								*/
/****************************************************************************/
BOOL trashcan(scfg_t* cfg, const char* insearchof, const char* name)
{
	struct trash trash;
	return trashcan2(cfg, insearchof, NULL, name, &trash);
}

/****************************************************************************/
static void parse_trash_details(char* p, struct trash* trash)
{
	memset(trash, 0, sizeof(*trash));

	str_list_t list = strListSplit(NULL, p, "\t");
	if(list == NULL)
		return;

	trash->added = iniGetDateTime(list, ROOT_SECTION, "added", 0);
	trash->expires = iniGetDateTime(list, ROOT_SECTION, "expires", 0);
	if((p = iniGetValue(list, ROOT_SECTION, "prot", NULL, NULL)) != NULL)
		SAFECOPY(trash->prot, p);
	if((p = iniGetValue(list, ROOT_SECTION, "user", NULL, NULL)) != NULL)
		SAFECOPY(trash->user, p);
	if((p = iniGetValue(list, ROOT_SECTION, "reason", NULL, NULL)) != NULL)
		SAFECOPY(trash->reason, p);
	strListFree(&list);
}

/****************************************************************************/
char* trash_details(const struct trash* trash, char* str, size_t max)
{
	char tmp[64];
	char since[128] = "";
	*str = '\0';
	if(trash->added)
		snprintf(since, sizeof since, "since %.24s", ctime_r(&trash->added, tmp));
	snprintf(str, max, "%s%s%s", since, trash->reason[0] ? " for " : "", trash->reason);
	return str;
}

/****************************************************************************/
/* Searches the file <name>.can in the TEXT directory for 2 matches			*/
/* Returns TRUE if found in list, FALSE if not.								*/
/****************************************************************************/
BOOL trashcan2(scfg_t* cfg, const char* str1, const char* str2, const char* name, struct trash* trash)
{
	char fname[MAX_PATH+1];
	char details[FINDSTR_MAX_LINE_LEN + 1];

	if(!find2strs(str1, str2, trashcan_fname(cfg,name,fname,sizeof(fname)), details))
		return FALSE;
	parse_trash_details(details, trash);
	if(trash->expires && trash->expires >= time(NULL))
		return FALSE;
	return TRUE;
}

/****************************************************************************/
BOOL trash_in_list(const char* str1, const char* str2, str_list_t list, struct trash* trash)
{
	char details[FINDSTR_MAX_LINE_LEN + 1];

	if(!find2strs_in_list(str1, str2, list, details))
		return FALSE;
	parse_trash_details(details, trash);
	if(trash->expires && trash->expires >= time(NULL))
		return FALSE;
	return TRUE;
}

/****************************************************************************/
str_list_t trashcan_list(scfg_t* cfg, const char* name)
{
	char	fname[MAX_PATH+1];

	return findstr_list(trashcan_fname(cfg, name, fname, sizeof(fname)));
}
