/* Synchronet client/content-filtering (trashcan/twit) functions */

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
#include "xpdatetime.h"
#include "ini_file.h"
#include "scfglib.h"
#include "findstr.h"
#include "nopen.h"

/****************************************************************************/
char* trashcan_fname(scfg_t* cfg, const char* name, char* fname, size_t maxlen)
{
	safe_snprintf(fname, maxlen, "%s%s.can", cfg->text_dir, name);
	return fname;
}

/****************************************************************************/
char* twitlist_fname(scfg_t* cfg, char* fname, size_t maxlen)
{
	safe_snprintf(fname, maxlen, "%stwitlist.cfg", cfg->ctrl_dir);
	return fname;
}

/****************************************************************************/
/* Searches the file <name>.can in the TEXT directory for matches			*/
/* Returns true if found in list, false if not.								*/
/****************************************************************************/
bool trashcan(scfg_t* cfg, const char* insearchof, const char* name)
{
	struct trash trash;
	return trashcan2(cfg, insearchof, NULL, name, &trash);
}

/****************************************************************************/
bool trash_parse_details(const char* p, struct trash* trash, char* item, size_t size)
{
	memset(trash, 0, sizeof(*trash));

	str_list_t list = strListSplitCopy(NULL, p, "\t");
	if (list == NULL)
		return false;

	if (item != NULL && size > 0) {
		if (list[0] == NULL)
			*item = '\0';
		else
			strlcpy(item, list[0], size);
		strListFastDelete(list, /* index: */ 0, /* count: */ 1);
	}
	trash->quiet = iniGetBool(list, ROOT_SECTION, "q", false);
	trash->added = iniGetDateTime(list, ROOT_SECTION, "t", 0);
	trash->expires = iniGetDateTime(list, ROOT_SECTION, "e", 0);
	if ((p = iniGetValue(list, ROOT_SECTION, "p", NULL, NULL)) != NULL)
		SAFECOPY(trash->prot, p);
	if ((p = iniGetValue(list, ROOT_SECTION, "u", NULL, NULL)) != NULL)
		SAFECOPY(trash->user, p);
	if ((p = iniGetValue(list, ROOT_SECTION, "r", NULL, NULL)) != NULL)
		SAFECOPY(trash->reason, p);
	strListFree(&list);
	return true;
}

/****************************************************************************/
char* trash_details(const struct trash* trash, char* str, size_t max)
{
	char tmp[64];
	char since[128] = "";
	*str = '\0';
	if (trash->added) {
		char* p = ctime_r(&trash->added, tmp);
		if (p != NULL)
			snprintf(since, sizeof since, "since %.20s", p + 4);
	}
	snprintf(str, max, "%s%s%s%s%s"
	         , since
	         , trash->reason[0] ? " for " : "", trash->reason
	         , trash->prot[0] ? " using " : "", trash->prot);
	return str;
}

/****************************************************************************/
/* Searches the file <name>.can in the TEXT directory for 2 matches			*/
/* Returns true if found in list, false if not.								*/
/****************************************************************************/
bool trashcan2(scfg_t* cfg, const char* str1, const char* str2, const char* name, struct trash* trash)
{
	char fname[MAX_PATH + 1];
	char details[FINDSTR_MAX_LINE_LEN + 1];

	if (!find2strs(str1, str2, trashcan_fname(cfg, name, fname, sizeof(fname)), details))
		return false;
	if (trash != NULL) {
		if (trash_parse_details(details, trash, NULL, 0)
		    && trash->expires && trash->expires <= time(NULL))
			return false;
	}
	return true;
}

/****************************************************************************/
bool trash_in_list(const char* str1, const char* str2, str_list_t list, struct trash* trash)
{
	char details[FINDSTR_MAX_LINE_LEN + 1];

	if (!find2strs_in_list(str1, str2, list, details))
		return false;
	if (trash != NULL) {
		if (trash_parse_details(details, trash, NULL, 0)
		    && trash->expires && trash->expires <= time(NULL))
			return false;
	}
	return true;
}

/****************************************************************************/
str_list_t trashcan_list(scfg_t* cfg, const char* name)
{
	char fname[MAX_PATH + 1];

	return findstr_list(trashcan_fname(cfg, name, fname, sizeof(fname)));
}

/****************************************************************************/
bool host_is_exempt(scfg_t* cfg, const char* ip_addr, const char* host_name)
{
	char exempt[MAX_PATH + 1];

	SAFEPRINTF2(exempt, "%s%s", cfg->ctrl_dir, strIpFilterExemptConfigFile);
	return find2strs(ip_addr, host_name, exempt, NULL);
}

/****************************************************************************/
/* Add an IP address (with comment) to the IP filter/trashcan file			*/
/****************************************************************************/
bool filter_ip(scfg_t* cfg, const char* prot, const char* reason, const char* host
               , const char* ip_addr, const char* username, const char* fname
               , uint duration)
{
	char   ip_can[MAX_PATH + 1];
	char   exempt[MAX_PATH + 1];
	char   tstr[64];
	FILE*  fp;
	time_t now = time(NULL);

	if (ip_addr == NULL)
		return false;

	SAFEPRINTF2(exempt, "%s%s", cfg->ctrl_dir, strIpFilterExemptConfigFile);
	if (find2strs(ip_addr, host, exempt, NULL))
		return false;

	SAFEPRINTF(ip_can, "%sip.can", cfg->text_dir);
	if (fname == NULL)
		fname = ip_can;

	if (findstr(ip_addr, fname)) /* Already filtered? */
		return true;

	if ((fp = fnopen(NULL, fname, O_CREAT | O_APPEND | O_WRONLY)) == NULL)
		return false;

	fprintf(fp, "%s\tt=%s"
	        , ip_addr
	        , time_to_isoDateTimeStr(now, xpTimeZone_local(), tstr, sizeof tstr));
	if (prot != NULL && *prot != '\0')
		fprintf(fp, "\tp=%s", prot);
	if (reason != NULL && *reason != '\0')
		fprintf(fp, "\tr=%s", reason);
	if (duration > 0)
		fprintf(fp, "\te=%s", time_to_isoDateTimeStr(time(NULL) + duration, xpTimeZone_local(), tstr, sizeof tstr));
	if (username != NULL && *username != '\0')
		fprintf(fp, "\tu=%s", username);
	if (host != NULL && *host != '\0' && strcmp(host, STR_NO_HOSTNAME) != 0)
		fprintf(fp, "\th=%s", host);
	fputc('\n', fp);

	fclose(fp);
	return true;
}

bool name_is_twit(scfg_t* cfg, const char* name)
{
	char path[MAX_PATH + 1];
	return findstr(name, twitlist_fname(cfg, path, sizeof path));
}

/* Add a name to the global twit list */
bool list_twit(scfg_t* cfg, const char* name, const char* comment)
{
	char  path[MAX_PATH + 1];
	FILE* fp = fnopen(/* fd: */ NULL, twitlist_fname(cfg, path, sizeof path), O_WRONLY | O_APPEND);
	if (fp == NULL)
		return false;
	if (comment != NULL)
		fprintf(fp, "\n; %s", comment);
	bool result = fprintf(fp, "\n%s\n", name) > 0;
	fclose(fp);
	return result;
}

str_list_t list_of_twits(scfg_t* cfg)
{
	char path[MAX_PATH + 1];
	return findstr_list(twitlist_fname(cfg, path, sizeof path));
}
