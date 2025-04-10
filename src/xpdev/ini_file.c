/* Functions to create and parse .ini files */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include "ini_file.h"
#include <stdlib.h>     /* strtol */
#include <string.h>     /* strlen */
#include <math.h>       /* fmod */
#include "xpdatetime.h" /* isoDateTime_t */
#include "datewrap.h"   /* ctime_r */
#include "dirwrap.h"    /* fexist */
#include "filewrap.h"   /* chsize */
#include "netwrap.h"

/* Maximum length of entire line, includes '\0' */
#define INI_MAX_LINE_LEN        (INI_MAX_VALUE_LEN * 2)
#define INI_COMMENT_CHAR        ';'
#define INI_OPEN_SECTION_CHAR   '['
#define INI_CLOSE_SECTION_CHAR  ']'
#define INI_SECTION_NAME_SEP    "|"
#define INI_BIT_SEP             '|'
#define INI_NEW_SECTION         ((char*)~0)
#define INI_EOF_DIRECTIVE       "!eof"
#define INI_INCLUDE_DIRECTIVE   "!include "
#define INI_INCLUDE_MAX         10000

static ini_style_t default_style;

void iniSetDefaultStyle(ini_style_t style)
{
	default_style = style;
}

/* These correlate with the LOG_* definitions in syslog.h/gen_defs.h */
static char* logLevelStringList[]
    = {"Emergency", "Alert", "Critical", "Error", "Warning", "Notice", "Info", "Debug", NULL};

str_list_t iniLogLevelStringList(void)
{
	return logLevelStringList;
}

static bool is_eof(char* str)
{
	return *str == '!' && stricmp(truncsp(str), INI_EOF_DIRECTIVE) == 0;
}

static char* section_name(char* p)
{
	char* tp;

	SKIP_WHITESPACE(p);
	if (*p != INI_OPEN_SECTION_CHAR)
		return NULL;
	p++;
	SKIP_WHITESPACE(p);
	tp = strrchr(p, INI_CLOSE_SECTION_CHAR);
	if (tp == NULL)
		return NULL;
	*tp = 0;
	truncsp(p);

	return p;
}

static bool section_match(const char* name, const char* compare, bool case_sensitive)
{
	bool       found = false;
	str_list_t names = strListSplitCopy(NULL, name, INI_SECTION_NAME_SEP);
	str_list_t comps = strListSplitCopy(NULL, compare, INI_SECTION_NAME_SEP);
	size_t     i, j;
	char*      n;
	char*      c;

	/* Ignore trailing whitespace */
	for (i = 0; names[i] != NULL; i++)
		truncsp(names[i]);
	for (i = 0; comps[i] != NULL; i++)
		truncsp(comps[i]);

	/* Search for matches */
	for (i = 0; names[i] != NULL && !found; i++)
		for (j = 0; comps[j] != NULL && !found; j++) {
			n = names[i];
			SKIP_WHITESPACE(n);
			c = comps[j];
			SKIP_WHITESPACE(c);
			if (case_sensitive)
				found = strcmp(n, c) == 0;
			else
				found = stricmp(n, c) == 0;
		}

	strListFree(&names);
	strListFree(&comps);

	return found;
}

static bool seek_section(FILE* fp, const char* section)
{
	char* p;
	char  str[INI_MAX_LINE_LEN];

	rewind(fp);

	if (section == ROOT_SECTION)
		return true;

	/* Perform case-sensitive search first */
	while (!feof(fp)) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		if (is_eof(str))
			break;
		if ((p = section_name(str)) == NULL)
			continue;
		if (section_match(p, section, /* case-sensitive */ true))
			return true;
	}

	/* Then perform case-insensitive search */
	rewind(fp);
	while (!feof(fp)) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		if (is_eof(str))
			break;
		if ((p = section_name(str)) == NULL)
			continue;
		if (section_match(p, section, /* case-sensitive */ false))
			return true;
	}

	return false;
}

static size_t find_section_index(str_list_t list, const char* section)
{
	char*  p;
	char   str[INI_MAX_VALUE_LEN];
	size_t i;

	/* Perform case-sensitive search first */
	for (i = 0; list[i] != NULL; i++) {
		SAFECOPY(str, list[i]);
		if (is_eof(str))
			return strListCount(list);
		if ((p = section_name(str)) != NULL && section_match(p, section, /* case-sensitive */ true))
			return i;
	}

	/* Then perform case-insensitive search */
	for (i = 0; list[i] != NULL; i++) {
		SAFECOPY(str, list[i]);
		if (is_eof(str))
			return strListCount(list);
		if ((p = section_name(str)) != NULL && section_match(p, section, /* case-sensitive */ false))
			return i;
	}

	return i;
}

static size_t section_start(str_list_t list, size_t index)
{
	char* p = list[index];
	if (p != NULL) {
		SKIP_WHITESPACE(p);
		if (*p == INI_OPEN_SECTION_CHAR) // A new section starts immediately?
			return strListCount(list);
	}
	return index;
}

static size_t find_section(str_list_t list, const char* section)
{
	size_t i;

	if (section == ROOT_SECTION)
		return 0;

	i = find_section_index(list, section);
	if (list[i] != NULL)
		i++;
	return section_start(list, i);
}

static char* key_name(char* p, char** vp, bool literals_supported)
{
	char* equal;
	char* colon;
	char* tp;

	*vp = NULL;

	if (p == NULL)
		return NULL;

	/* Parse value name */
	SKIP_WHITESPACE(p);
	if (*p == INI_COMMENT_CHAR)
		return NULL;
	if (*p == INI_OPEN_SECTION_CHAR)
		return INI_NEW_SECTION;
	equal = strchr(p, '=');
	colon = strchr(p, ':');
	if (colon == NULL || (equal != NULL && equal < colon)) {
		*vp = equal;
		colon = NULL;
	} else
		*vp = colon;

	if (*vp == NULL)
		return NULL;

	*(*vp) = 0;
	truncsp(p);

	/* Parse value */
	(*vp)++;
	SKIP_WHITESPACE(*vp);
	if (literals_supported && colon != NULL) {     /* string literal value */
		truncnl(*vp);       /* "key : value" - truncate new-line chars only */
		if (*(*vp) == '"') { /* handled quoted-strings here */
			(*vp)++;
			tp = strrchr(*vp, '"');
			if (tp != NULL) {
				*tp = 0;
			}
		}
		c_unescape_str(*vp);
	} else
		truncsp(*vp);       /* "key = value" - truncate all white-space chars */

	return p;
}

static char* clean_value(char* str)
{
	char* p;

	if ((p = strchr(str, '\xFF')) != NULL)
		*p = '\0';

	return str;
}

static char* read_value(FILE* fp, const char* section, const char* key, char* value, bool literals_supported)
{
	char* p;
	char* vp = NULL;
	char  str[INI_MAX_LINE_LEN];

	if (fp == NULL)
		return NULL;

	if (!seek_section(fp, section))
		return NULL;

	while (!feof(fp)) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		if (is_eof(str))
			break;
		if ((p = key_name(str, &vp, literals_supported)) == NULL)
			continue;
		if (p == INI_NEW_SECTION)
			break;
		if (stricmp(p, key) != 0)
			continue;
		if (vp == NULL)
			break;
		/* key found */
		sprintf(value, "%.*s", INI_MAX_VALUE_LEN - 1, vp);
		return clean_value(value);
	}

	return NULL;
}

static size_t get_value(str_list_t list, const char* section, const char* key, char* value, char** vpp, bool literals_supported)
{
	char   str[INI_MAX_LINE_LEN];
	char*  p;
	char*  vp;
	size_t i;

	if (value != NULL)
		value[0] = 0;
	if (vpp != NULL)
		*vpp = NULL;
	if (list == NULL)
		return 0;

	for (i = find_section(list, section); list[i] != NULL; i++) {
		SAFECOPY(str, list[i]);
		if (is_eof(str))
			break;
		if ((p = key_name(str, &vp, literals_supported)) == NULL)
			continue;
		if (p == INI_NEW_SECTION)
			break;
		if (stricmp(p, key) != 0)
			continue;
		if (value != NULL) {
			sprintf(value, "%.*s", INI_MAX_VALUE_LEN - 1, vp);
			clean_value(value);
		}
		if (vpp != NULL)
			*vpp = list[i] + (vp - str);
		return i;
	}

	return i;
}

bool iniSectionExists(str_list_t list, const char* section)
{
	size_t i;

	if (list == NULL)
		return false;

	if (section == ROOT_SECTION)
		return true;

	i = find_section_index(list, section);
	return list[i] != NULL;
}

str_list_t iniGetSection(str_list_t list, const char *section)
{
	size_t     i;
	str_list_t retval;
	char *     p;

	if (list == NULL)
		return NULL;

	if ((retval = strListInit()) == NULL)
		return NULL;

	i = find_section(list, section);
	if (list[i] != NULL) {
		strListPush(&retval, list[i]);
		for (i++; list[i] != NULL; i++) {
			p = list[i];
			SKIP_WHITESPACE(p);
			if (*p == INI_OPEN_SECTION_CHAR)
				break;
			if (*p)
				strListPush(&retval, list[i]);
		}
	}
	return retval;
}

bool iniKeyExists(str_list_t list, const char* section, const char* key)
{
	size_t i;

	if (list == NULL)
		return false;

	i = get_value(list, section, key, NULL, NULL, /* literals_supported: */ false);

	if (list[i] == NULL || *(list[i]) == INI_OPEN_SECTION_CHAR)
		return false;

	return true;
}

bool iniValueExists(str_list_t list, const char* section, const char* key)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	return vp != NULL && *vp != 0;
}

bool iniRemoveKey(str_list_t* list, const char* section, const char* key)
{
	size_t i;
	char*  vp = NULL;
	bool   removed = false;

	while (1) {
		i = get_value(*list, section, key, NULL, &vp, /* literals_supported: */ false);

		if (vp == NULL)
			break;

		if (!strListDelete(list, i))
			return false;
		removed = true;
	}
	return removed;
}

bool iniRemoveValue(str_list_t* list, const char* section, const char* key)
{
	char* vp = NULL;

	get_value(*list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL)
		return false;

	while (*vp != '\0' && isspace(*(vp - 1)))
		--vp;
	*vp = 0;
	return true;
}

bool iniRemoveSection(str_list_t* list, const char* section)
{
	size_t i;

	i = find_section_index(*list, section);
	if ((*list)[i] == NULL)    /* not found */
		return false;
	do {
		strListDelete(list, i);
	} while ((*list)[i] != NULL && *(*list)[i] != INI_OPEN_SECTION_CHAR);

	return true;
}

bool iniRemoveSectionFast(str_list_t list, const char* section)
{
	size_t i;

	i = find_section_index(list, section);
	if (list[i] == NULL)   /* not found */
		return false;
	do {
		strListFastDelete(list, i, /* count: */ 1);
	} while (list[i] != NULL && *list[i] != INI_OPEN_SECTION_CHAR);

	return true;
}

str_list_t iniCutSection(str_list_t list, const char *section)
{
	str_list_t ini = iniGetSection(list, section);
	if (ini == NULL)
		return NULL;
	(void)iniRemoveSectionFast(list, section);
	return ini;
}

bool iniRemoveSections(str_list_t* list, const char* prefix)
{
	str_list_t  sections;
	const char* section;

	if (list == NULL)
		return false;
	sections = iniGetSectionList(*list, prefix);
	while ((section = strListPop(&sections)) != NULL)
		if (!iniRemoveSection(list, section))
			return false;

	strListFree(&sections);

	return true;
}

// This sorts comments too, so should not be used on human created/edited files
bool iniSortSections(str_list_t* list, const char* prefix, bool sort_keys)
{
	size_t     i;
	str_list_t root_keys = NULL;
	str_list_t new_list;
	str_list_t keys;
	str_list_t section_list = iniGetSectionList(*list, prefix);

	if (prefix == NULL)
		root_keys = iniGetSection(*list, ROOT_SECTION);
	if (section_list == NULL && root_keys == NULL)
		return true;

	if (sort_keys)
		strListSortAlphaCase(root_keys);

	if (section_list != NULL)
		strListSortAlphaCase(section_list);
	if (prefix == NULL)
		new_list = strListInit();
	else {
		new_list = strListDup(*list);
		iniRemoveSections(&new_list, prefix);
	}
	if (new_list == NULL) {
		strListFree(&section_list);
		strListFree(&root_keys);
		return false;
	}
	strListAppendList(&new_list, root_keys);
	strListFree(&root_keys);
	for (i = 0; section_list != NULL && section_list[i] != NULL; i++) {
		keys = iniGetSection(*list, section_list[i]);
		if (sort_keys)
			strListSortAlphaCase(keys);
		iniAppendSectionWithKeys(&new_list, section_list[i], keys, /* ini_style_t */ NULL);
		strListFree(&keys);
	}
	strListFree(&section_list);
	strListFree(list);
	*list = new_list;
	return true;
}

bool iniRenameSection(str_list_t* list, const char* section, const char* newname)
{
	char   str[INI_MAX_LINE_LEN];
	size_t i;

	if (section == ROOT_SECTION)
		return false;

	if (stricmp(section, newname) != 0) {
		i = find_section_index(*list, newname);
		if ((*list)[i] != NULL)    /* duplicate */
			return false;
	}

	i = find_section_index(*list, section);
	if ((*list)[i] == NULL)    /* not found */
		return false;

	SAFEPRINTF(str, "[%s]", newname);
	return strListReplace(*list, i, str) != NULL;
}

static size_t ini_add_section(str_list_t* list, const char* section
                              , ini_style_t* style, size_t index)
{
	char str[INI_MAX_LINE_LEN];

	if (section == ROOT_SECTION)
		return 0;

	if ((*list)[index] != NULL)
		return index;

	if (style == NULL)
		style = &default_style;
	if (index > 0 && style->section_separator != NULL)
		strListAppend(list, style->section_separator, index++);
	SAFEPRINTF(str, "[%s]", section);
	strListAppend(list, str, index);

	return index;
}

size_t iniAddSection(str_list_t* list, const char* section, ini_style_t* style)
{
	if (section == ROOT_SECTION)
		return 0;

	return ini_add_section(list, section, style, find_section_index(*list, section));
}

size_t iniAppendSection(str_list_t* list, const char* section, ini_style_t* style)
{
	if (section == ROOT_SECTION)
		return 0;

	return ini_add_section(list, section, style, strListCount(*list));
}

size_t iniAppendSectionWithKeys(str_list_t* list, const char* section, const str_list_t keys
                                , ini_style_t* style)
{
	if (section == ROOT_SECTION)
		return 0;

	ini_add_section(list, section, style, strListCount(*list));

	return strListAppendList(list, keys);
}

static bool str_contains_ctrl_char(const char* str)
{
	while (*str) {
		if (*(unsigned char*)str < ' ')
			return true;
		str++;
	}
	return false;
}

static char* ini_set_string(str_list_t* list, const char* section, const char* key, const char* value, bool literal
                            , ini_style_t* style)
{
	char        str[INI_MAX_LINE_LEN];
	char        litstr[INI_MAX_VALUE_LEN];
	char        curval[INI_MAX_VALUE_LEN];
	const char* key_prefix = "";
	const char* value_separator = "=";
	const char* literal_separator = ":";
	size_t      i;

	if (style == NULL)
		style = &default_style;

	iniAddSection(list, section, style);

	if (key == NULL)
		return NULL;
	if (style->key_prefix != NULL)
		key_prefix = style->key_prefix;
	if (style->value_separator != NULL)
		value_separator = style->value_separator;
	if (style->literal_separator != NULL)
		literal_separator = style->literal_separator;
	if (value == NULL)
		value = "";
	if (literal) {
		char cstr[INI_MAX_VALUE_LEN];
		SAFEPRINTF(litstr, "\"%s\"", c_escape_str(value, cstr, sizeof(cstr) - 1, /* ctrl_only: */ false));
		value = litstr;
		value_separator = literal_separator;
	}
	safe_snprintf(str, sizeof(str), "%s%-*s%s%s"
	              , key_prefix, style->key_len, key, value_separator, value);
	i = get_value(*list, section, key, curval, NULL, /* literals_supported: */ literal);
	if ((*list)[i] == NULL || *(*list)[i] == INI_OPEN_SECTION_CHAR) {
		while (i && *(*list)[i - 1] == 0) i--; /* Insert before blank lines, not after */
		return strListInsert(list, str, i);
	}

	if (strcmp(curval, value) == 0)
		return (*list)[i]; /* no change */

	return strListReplace(*list, i, str);
}

size_t iniAppendSectionWithNamedStrings(str_list_t* list, const char* section, const named_string_t** key
                                , ini_style_t* style)
{
	size_t     i;

	if (section == ROOT_SECTION)
		return 0;

	ini_add_section(list, section, style, strListCount(*list));

	for (i = 0; key[i] != NULL; ++i)
		if (ini_set_string(list, section, key[i]->name, key[i]->value, /* literal */false, style) == NULL)
			break;

	return i;
}

char* iniSetString(str_list_t* list, const char* section, const char* key, const char* value
                   , ini_style_t* style)
{
	bool literal = value != NULL && (str_contains_ctrl_char(value) || *value == ' ' || *lastchar(value) == ' ');

	return ini_set_string(list, section, key, value, literal, style);
}

char* iniSetStringLiteral(str_list_t* list, const char* section, const char* key, const char* value
                          , ini_style_t* style)
{
	return ini_set_string(list, section, key, value, /* literal: */ true, style);
}

char* iniSetValue(str_list_t* list, const char* section, const char* key, const char* value
                  , ini_style_t* style)
{
	return ini_set_string(list, section, key, value, /* literal: */ false, style);
}

char* iniSetInteger(str_list_t* list, const char* section, const char* key, int value
                    , ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str, "%d", value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetUInteger(str_list_t* list, const char* section, const char* key, uint value
                     , ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str, "%u", value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetShortInt(str_list_t* list, const char* section, const char* key, short value
                     , ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str, "%hd", value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetUShortInt(str_list_t* list, const char* section, const char* key, ushort value
                      , ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str, "%hu", value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetLongInt(str_list_t* list, const char* section, const char* key, long value
                    , ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str, "%ld", value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetULongInt(str_list_t* list, const char* section, const char* key, ulong value
                     , ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str, "%lu", value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetHexInt(str_list_t* list, const char* section, const char* key, uint value
                   , ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN] = "0";

	if (value) {
		if (value < 10)
			SAFEPRINTF(str, "%u", value);
		else
			SAFEPRINTF(str, "0x%x", value);
	}
	return iniSetString(list, section, key, str, style);
}

char* iniSetHexInt64(str_list_t* list, const char* section, const char* key, uint64_t value
                     , ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN] = "0";

	if (value) {
		if (value < 10)
			SAFEPRINTF(str, "%" PRIu64, value);
		else
			SAFEPRINTF(str, "0x%" PRIx64, value);
	}
	return iniSetString(list, section, key, str, style);
}

char* iniSetFloat(str_list_t* list, const char* section, const char* key, double value
                  , ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN];

	SAFEPRINTF(str, "%g", value);
	return iniSetString(list, section, key, str, style);
}

char* iniSetBytes(str_list_t* list, const char* section, const char* key, uint unit
                  , int64_t value, ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN];

	if (value == 0)
		SAFECOPY(str, "0");
	else
		switch (unit) {
			case 1024 * 1024 * 1024:
				SAFEPRINTF(str, "%" PRIi64 "G", value);
				break;
			case 1024 * 1024:
				SAFEPRINTF(str, "%" PRIi64 "M", value);
				break;
			case 1024:
				SAFEPRINTF(str, "%" PRIi64 "K", value);
				break;
			default:
				if (unit < 1)
					unit = 1;
				byte_count_to_str(value * unit, str, sizeof(str));
		}

	return iniSetString(list, section, key, str, style);
}

char* iniSetDuration(str_list_t* list, const char* section, const char* key
                     , double value, ini_style_t* style)
{
	char str[INI_MAX_VALUE_LEN] = "0";

	if (value)
		duration_to_str(value, str, sizeof(str));
	return iniSetString(list, section, key, str, style);
}


#if !defined(NO_SOCKET_SUPPORT)
char* iniSetIpAddress(str_list_t* list, const char* section, const char* key, uint32_t value
                      , ini_style_t* style)
{
	char buf[128];
	return iniSetString(list, section, key,
	                    IPv4AddressToStr(value, buf, sizeof(buf)),
	                    style);
}

char* iniSetIp6Address(str_list_t* list, const char* section, const char* key, struct in6_addr value
                       , ini_style_t* style)
{
	char              addrstr[INET6_ADDRSTRLEN];
	union xp_sockaddr addr = {{0}};

	addr.in6.sin6_addr = value;
	addr.in6.sin6_family = AF_INET6;
	inet_addrtop(&addr, addrstr, sizeof(addrstr));
	return iniSetString(list, section, key, addrstr, style);
}
#endif

char* iniSetBool(str_list_t* list, const char* section, const char* key, bool value
                 , ini_style_t* style)
{
	return iniSetString(list, section, key, value ? "true":"false", style);
}

char* iniSetDateTime(str_list_t* list, const char* section, const char* key
                     , bool include_time, time_t value, ini_style_t* style)
{
	char  str[INI_MAX_VALUE_LEN];
	char  tstr[32];
	char* p;

	if (value == 0)
		SAFECOPY(str, "Never");
	else if ((p = ctime_r(&value, tstr)) == NULL)
		SAFEPRINTF(str, "0x%llx", (long long)value);
	else if (!include_time)  /* reformat into "Mon DD YYYY" */
		safe_snprintf(str, sizeof(str), "%.3s %.2s %.4s", p + 4, p + 8, p + 20);
	else                    /* reformat into "Mon DD YYYY HH:MM:SS" */
		safe_snprintf(str, sizeof(str), "%.3s %.2s %.4s %.8s", p + 4, p + 8, p + 20, p + 11);

	return iniSetString(list, section, key, str, style);
}

char* iniSetEnum(str_list_t* list, const char* section, const char* key, str_list_t names, unsigned value
                 , ini_style_t* style)
{
	if (value < strListCount(names))
		return iniSetString(list, section, key, names[value], style);

	return iniSetUInteger(list, section, key, value, style);
}

char* iniSetEnumList(str_list_t* list, const char* section, const char* key
                     , const char* sep, str_list_t names, unsigned* val_list, unsigned count, ini_style_t* style)
{
	char   value[INI_MAX_VALUE_LEN];
	size_t i;
	size_t name_count;

	value[0] = 0;

	if (sep == NULL)
		sep = ",";

	if (val_list != NULL) {
		name_count = strListCount(names);
		for (i = 0; i < count; i++) {
			if (value[0])
				SAFECAT(value, sep);
			if (val_list[i] < name_count)
				SAFECAT(value, names[val_list[i]]);
			else
				sprintf(value + strlen(value), "%u", val_list[i]);
		}
	}

	return iniSetString(list, section, key, value, style);
}

char* iniSetNamedInt(str_list_t* list, const char* section, const char* key, named_int_t* names
                     , int value, ini_style_t* style)
{
	size_t i;

	for (i = 0; names[i].name != NULL; i++)
		if (names[i].value == value)
			return iniSetString(list, section, key, names[i].name, style);

	return iniSetInteger(list, section, key, value, style);
}

char* iniSetNamedHexInt(str_list_t* list, const char* section, const char* key, named_uint_t* names
                        , uint value, ini_style_t* style)
{
	size_t i;

	for (i = 0; names[i].name != NULL; i++)
		if (names[i].value == value)
			return iniSetString(list, section, key, names[i].name, style);

	return iniSetHexInt(list, section, key, value, style);
}

char* iniSetNamedLongInt(str_list_t* list, const char* section, const char* key, named_long_t* names
                         , long value, ini_style_t* style)
{
	size_t i;

	for (i = 0; names[i].name != NULL; i++)
		if (names[i].value == value)
			return iniSetString(list, section, key, names[i].name, style);

	return iniSetLongInt(list, section, key, value, style);
}

char* iniSetNamedFloat(str_list_t* list, const char* section, const char* key, named_double_t* names
                       , double value, ini_style_t* style)
{
	size_t i;

	for (i = 0; names[i].name != NULL; i++)
		if (names[i].value == value)
			return iniSetString(list, section, key, names[i].name, style);

	return iniSetFloat(list, section, key, value, style);
}

char* iniSetBitField(str_list_t* list, const char* section, const char* key
                     , ini_bitdesc_t* bitdesc, uint value, ini_style_t* style)
{
	char        str[INI_MAX_VALUE_LEN];
	int         i;
	const char* bit_separator = "|";

	if (style == NULL)
		style = &default_style;
	if (style->bit_separator != NULL)
		bit_separator = style->bit_separator;
	str[0] = 0;
	for (i = 0; bitdesc[i].name; i++) {
		if ((value & bitdesc[i].bit) == 0)
			continue;
		if (str[0])
			SAFECAT(str, bit_separator);
		SAFECAT(str, bitdesc[i].name);
		value &= ~bitdesc[i].bit;
	}
	if (value) { /* left over bits? */
		if (str[0])
			SAFECAT(str, bit_separator);
		sprintf(str + strlen(str), "0x%X", value);
	}
	return iniSetString(list, section, key, str, style);
}

char* iniSetStringList(str_list_t* list, const char* section, const char* key
                       , const char* sep, str_list_t val_list, ini_style_t* style)
{
	char value[INI_MAX_VALUE_LEN];

	if (sep == NULL)
		sep = ",";

	return iniSetString(list, section, key, strListCombine(val_list, value, sizeof(value), sep), style);
}

char* iniSetIntList(str_list_t* list, const char* section, const char* key
                    , const char* sep, int* val_list, unsigned count, ini_style_t* style)
{
	unsigned i;
	char     value[INI_MAX_VALUE_LEN];

	if (sep == NULL)
		sep = ",";
	for (i = 0; i < count; i++) {
		if (i) {
			int len = strlen(value);
			if (len > INI_MAX_VALUE_LEN - 20)
				return NULL;
			sprintf(value + len, "%s%d", sep, *(val_list + i));
		} else
			sprintf(value, "%d", *val_list);
	}

	return iniSetString(list, section, key, value, style);
}

static char* default_value(const char* deflt, char* value)
{
	if (deflt != NULL && deflt != value && value != NULL)
		sprintf(value, "%.*s", INI_MAX_VALUE_LEN - 1, deflt);

	return (char*)deflt;
}

/* Supports string literals: */
char* iniReadString(FILE* fp, const char* section, const char* key, const char* deflt, char* value)
{
	if (read_value(fp, section, key, value, /* literals_supported: */ true) == NULL || *value == 0 /* blank */)
		return default_value(deflt, value);

	return value;
}

char* iniReadSString(FILE* fp, const char* section, const char* key, const char* deflt, char* value, size_t sz)
{
	char   fval[INI_MAX_VALUE_LEN] = "";
	char * ret;
	size_t pos;

	ret = iniReadString(fp, section, key, deflt, fval);
	if (ret == NULL) {
		if (sz > 0 && value != NULL)
			value[0] = 0;
		return NULL;
	}
	if (sz < 1 || value == NULL)
		return value;
	for (pos = 0; ret[pos]; pos++) {
		if (pos == sz - 1)
			break;
		value[pos] = ret[pos];
	}
	value[pos] = 0;
	if (ret == deflt)
		return (char*)deflt;
	return value;
}

/* Does NOT support string literals: */
char* iniReadValue(FILE* fp, const char* section, const char* key, const char* deflt, char* value)
{
	if (read_value(fp, section, key, value, /* literals_supported: */ false) == NULL || *value == 0 /* blank */)
		return default_value(deflt, value);

	return value;
}

/* Supports string literals: */
char* iniGetString(str_list_t list, const char* section, const char* key, const char* deflt, char* value)
{
	char* vp = NULL;

	get_value(list, section, key, value, &vp, /* literals_supported: */ true);

	if (vp == NULL || *vp == 0 /* blank value or missing key */)
		return default_value(deflt, value);

	if (value != NULL)   /* return the modified (trimmed) value */
		return value;

	return vp;
}

char* iniGetSString(str_list_t list, const char* section, const char* key, const char* deflt, char* value, size_t sz)
{
	char   fval[INI_MAX_VALUE_LEN] = "";
	char * ret;
	size_t pos;

	ret = iniGetString(list, section, key, deflt, fval);
	if (ret == NULL) {
		if (sz > 0 && value != NULL)
			value[0] = 0;
		return NULL;
	}
	if (sz < 1 || value == NULL)
		return value;
	for (pos = 0; ret[pos]; pos++) {
		if (pos == sz - 1)
			break;
		value[pos] = ret[pos];
	}
	value[pos] = 0;
	if (ret == deflt)
		return (char*)deflt;
	return value;
}

/* Does NOT support string literals: */
char* iniGetValue(str_list_t list, const char* section, const char* key, const char* deflt, char* value)
{
	char* vp = NULL;

	get_value(list, section, key, value, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0 /* blank value or missing key */)
		return default_value(deflt, value);

	if (value != NULL)   /* return the modified (trimmed) value */
		return value;

	return vp;
}

/* Does NOT support string literals: */
char* iniPopKey(str_list_t* list, const char* section, const char* key, char* value)
{
	size_t i;

	if (list == NULL || *list == NULL)
		return NULL;

	i = get_value(*list, section, key, value, NULL, /* literals_supported: */ false);

	if ((*list)[i] == NULL)
		return NULL;

	strListDelete(list, i);

	return value;
}

/* Supports string literals: */
char* iniPopString(str_list_t* list, const char* section, const char* key, char* value)
{
	size_t i;

	if (list == NULL || *list == NULL)
		return NULL;

	i = get_value(*list, section, key, value, NULL, /* literals_supported: */ true);

	if ((*list)[i] == NULL)
		return NULL;

	strListDelete(list, i);

	return value;
}

char* iniReadExistingString(FILE* fp, const char* section, const char* key, const char* deflt, char* value)
{
	if (read_value(fp, section, key, value, /* literals_supported: */ true) == NULL)
		return NULL;

	if (*value == 0 /* blank */)
		return default_value(deflt, value);

	return value;
}

char* iniGetExistingString(str_list_t list, const char* section, const char* key, const char* deflt, char* value)
{
	if (!iniKeyExists(list, section, key))
		return NULL;

	return iniGetString(list, section, key, deflt, value);
}

char* iniReadExistingValue(FILE* fp, const char* section, const char* key, const char* deflt, char* value)
{
	if (read_value(fp, section, key, value, /* literals_supported: */ false) == NULL)
		return NULL;

	if (*value == 0 /* blank */)
		return default_value(deflt, value);

	return value;
}

char* iniGetExistingValue(str_list_t list, const char* section, const char* key, const char* deflt, char* value)
{
	if (!iniKeyExists(list, section, key))
		return NULL;

	return iniGetValue(list, section, key, deflt, value);
}


static str_list_t splitList(char* list, const char* sep)
{
	char*      token;
	char*      tmp;
	ulong      items = 0;
	str_list_t lp;

	if ((lp = strListInit()) == NULL)
		return NULL;

	if (sep == NULL)
		sep = ",";

	token = strtok_r(list, sep, &tmp);
	while (token != NULL) {
		SKIP_WHITESPACE(token);
		truncsp(token);
		if (strListAppend(&lp, token, items++) == NULL)
			break;
		token = strtok_r(NULL, sep, &tmp);
	}

	return lp;
}

str_list_t iniReadStringList(FILE* fp, const char* section, const char* key
                             , const char* sep, const char* deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];
	char  list[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ true)) == NULL || *value == 0 /* blank */)
		value = (char*)deflt;

	if (value == NULL)
		return NULL;

	SAFECOPY(list, value);

	return splitList(list, sep);
}

str_list_t iniGetStringList(str_list_t list, const char* section, const char* key
                            , const char* sep, const char* deflt)
{
	char value[INI_MAX_VALUE_LEN];

	get_value(list, section, key, value, NULL, /* literals_supported: */ true);

	if (*value == 0 /* blank value or missing key */) {
		if (deflt == NULL)
			return NULL;
		SAFECOPY(value, deflt);
	}

	return splitList(value, sep);
}

void* iniFreeStringList(str_list_t list)
{
	strListFree(&list);
	return list;
}

void* iniFreeNamedStringList(named_string_t** list)
{
	ulong i;

	if (list == NULL)
		return NULL;

	for (i = 0; list[i] != NULL; i++) {
		if (list[i]->name != NULL)
			free(list[i]->name);
		if (list[i]->value != NULL)
			free(list[i]->value);
		free(list[i]);
	}

	free(list);
	return NULL;
}

static str_list_t ini_read_section_list(FILE* fp, const char* prefix, bool include_dupes)
{
	char*      p;
	char       str[INI_MAX_LINE_LEN];
	ulong      items = 0;
	str_list_t lp;

	if ((lp = strListInit()) == NULL)
		return NULL;

	if (fp == NULL)
		return lp;

	rewind(fp);

	while (!feof(fp)) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		if (is_eof(str))
			break;
		if ((p = section_name(str)) == NULL)
			continue;
		if (prefix != NULL)
			if (strnicmp(p, prefix, strlen(prefix)) != 0)
				continue;
		if (!include_dupes && strListFind(lp, p, /* case_sensitive */ false) >= 0)
			continue;
		if (strListAppend(&lp, p, items++) == NULL)
			break;
	}

	return lp;
}

str_list_t iniReadSectionList(FILE* fp, const char* prefix)
{
	return ini_read_section_list(fp, prefix, /* include dupes: */ false);
}

str_list_t iniReadSectionListWithDupes(FILE* fp, const char* prefix)
{
	return ini_read_section_list(fp, prefix, /* include dupes: */ true);
}

static str_list_t ini_get_section_list(str_list_t list, const char* prefix, bool include_dupes)
{
	char*      p;
	char       str[INI_MAX_LINE_LEN];
	ulong      i, items = 0;
	str_list_t lp;

	if ((lp = strListInit()) == NULL)
		return NULL;

	if (list == NULL)
		return lp;

	for (i = 0; list[i] != NULL; i++) {
		SAFECOPY(str, list[i]);
		if (is_eof(str))
			break;
		if ((p = section_name(str)) == NULL)
			continue;
		if (prefix != NULL)
			if (strnicmp(p, prefix, strlen(prefix)) != 0)
				continue;
		if (include_dupes && strListFind(lp, p, /* case_sensitive */ false) >= 0)
			continue;
		if (strListAppend(&lp, p, items++) == NULL)
			break;
	}

	return lp;
}

str_list_t iniGetSectionList(str_list_t list, const char* prefix)
{
	return ini_get_section_list(list, prefix, /* include_dupes: */ false);
}

str_list_t iniGetSectionListWithDupes(str_list_t list, const char* prefix)
{
	return ini_get_section_list(list, prefix, /* include_dupes: */ true);
}

size_t iniGetSectionCount(str_list_t list, const char* prefix)
{
	char*  p;
	char   str[INI_MAX_LINE_LEN];
	size_t i, items = 0;

	if (list == NULL)
		return 0;

	for (i = 0; list[i] != NULL; i++) {
		SAFECOPY(str, list[i]);
		if (is_eof(str))
			break;
		if ((p = section_name(str)) == NULL)
			continue;
		if (prefix != NULL)
			if (strnicmp(p, prefix, strlen(prefix)) != 0)
				continue;
		items++;
	}

	return items;
}

size_t iniReadSectionCount(FILE* fp, const char* prefix)
{
	char* p;
	char  str[INI_MAX_LINE_LEN];
	ulong items = 0;

	if (fp == NULL)
		return 0;

	rewind(fp);

	while (!feof(fp)) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		if (is_eof(str))
			break;
		if ((p = section_name(str)) == NULL)
			continue;
		if (prefix != NULL)
			if (strnicmp(p, prefix, strlen(prefix)) != 0)
				continue;
		items++;
	}

	return items;
}


str_list_t iniReadKeyList(FILE* fp, const char* section)
{
	char*      p;
	char*      vp;
	char       str[INI_MAX_LINE_LEN];
	ulong      items = 0;
	str_list_t lp;

	if ((lp = strListInit()) == NULL)
		return NULL;

	if (fp == NULL)
		return lp;

	rewind(fp);

	if (!seek_section(fp, section))
		return lp;

	while (!feof(fp)) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		if (is_eof(str))
			break;
		if ((p = key_name(str, &vp, /* literals_supported: */ false)) == NULL)
			continue;
		if (p == INI_NEW_SECTION)
			break;
		if (strListAppend(&lp, p, items++) == NULL)
			break;
	}

	return lp;
}

str_list_t iniGetKeyList(str_list_t list, const char* section)
{
	char*      p;
	char*      vp;
	char       str[INI_MAX_LINE_LEN];
	ulong      i, items = 0;
	str_list_t lp;

	if ((lp = strListInit()) == NULL)
		return NULL;

	if (list == NULL)
		return lp;

	for (i = find_section(list, section); list[i] != NULL; i++) {
		SAFECOPY(str, list[i]);
		if (is_eof(str))
			break;
		if ((p = key_name(str, &vp, /* literals_supported: */ false)) == NULL)
			continue;
		if (p == INI_NEW_SECTION)
			break;
		if (strListAppend(&lp, p, items++) == NULL)
			break;
	}

	return lp;
}


named_string_t**
iniReadNamedStringList(FILE* fp, const char* section)
{
	char*            name;
	char*            value;
	char             str[INI_MAX_LINE_LEN];
	ulong            items = 0;
	named_string_t** lp;
	named_string_t** np;

	if (fp == NULL)
		return NULL;

	rewind(fp);

	if (!seek_section(fp, section))
		return NULL;

	/* New behavior, if section exists but is empty, return single element array (terminator only) */
	if ((lp = (named_string_t**)malloc(sizeof(named_string_t*))) == NULL)
		return NULL;

	while (!feof(fp)) {
		if (fgets(str, sizeof(str), fp) == NULL)
			break;
		if (is_eof(str))
			break;
		if ((name = key_name(str, &value, /* literals_supported: */ true)) == NULL)
			continue;
		if (name == INI_NEW_SECTION)
			break;
		if ((np = (named_string_t**)realloc(lp, sizeof(named_string_t*) * (items + 2))) == NULL)
			break;
		lp = np;
		if ((lp[items] = (named_string_t*)malloc(sizeof(named_string_t))) == NULL)
			break;
		if ((lp[items]->name = strdup(name)) == NULL)
			break;
		if ((lp[items]->value = strdup(value)) == NULL)
			break;
		items++;
	}

	lp[items] = NULL; /* terminate list */

	return lp;
}

named_string_t**
iniGetNamedStringList(str_list_t list, const char* section)
{
	char*            name;
	char*            value;
	char             str[INI_MAX_LINE_LEN];
	ulong            i, items = 0;
	named_string_t** lp;
	named_string_t** np;

	if (list == NULL)
		return NULL;

	i = find_section(list, section);
	if (section != ROOT_SECTION && list[i] == NULL)
		return NULL;

	/* New behavior, if section exists but is empty, return single element array (terminator only) */
	if ((lp = (named_string_t**)malloc(sizeof(named_string_t*))) == NULL)
		return NULL;

	for (; list[i] != NULL; i++) {
		SAFECOPY(str, list[i]);
		if (is_eof(str))
			break;
		if ((name = key_name(str, &value, /* literals_supported: */ true)) == NULL)
			continue;
		if (name == INI_NEW_SECTION)
			break;
		if ((np = (named_string_t**)realloc(lp, sizeof(named_string_t*) * (items + 2))) == NULL)
			break;
		lp = np;
		if ((lp[items] = (named_string_t*)malloc(sizeof(named_string_t))) == NULL)
			break;
		if ((lp[items]->name = strdup(name)) == NULL)
			break;
		if ((lp[items]->value = strdup(value)) == NULL)
			break;
		items++;
	}

	lp[items] = NULL; /* terminate list */

	return lp;
}

// the 'list' must remain allocated/valid through-out the life of the returned named_str_list
// as this function does not copy the key=value lines in the original list, it just references them
named_str_list_t** iniParseSections(const str_list_t list)
{
	char               str[INI_MAX_LINE_LEN];
	char*              p;
	size_t             i;
	size_t             sections = 0;
	size_t             keys = 0;
	named_str_list_t** lp;
	named_str_list_t** np;

	if (list == NULL)
		return NULL;

	// Find first section
	for (i = 0; list[i] != NULL; ++i) {
		p = list[i];
		SKIP_WHITESPACE(p);
		if (*p == INI_OPEN_SECTION_CHAR)
			break;
	}
	if (list[i] == NULL)
		return NULL;

	if ((lp = (named_str_list_t**)malloc(sizeof(named_str_list_t*))) == NULL)
		return NULL;

	for (; list[i] != NULL; ++i) {
		SAFECOPY(str, list[i]);
		p = section_name(str);
		if (p != NULL) {
			if ((np = (named_str_list_t**)realloc(lp, sizeof(named_str_list_t*) * (sections + 2))) == NULL)
				break;
			lp = np;
			if ((lp[sections] = (named_str_list_t*)malloc(sizeof(named_str_list_t))) == NULL)
				break;
			if ((lp[sections]->name = strdup(p)) == NULL)
				break;
			if ((lp[sections]->list = strListInit()) == NULL)
				break;
			++sections;
			keys = 0;
		} else {
			p = list[i];
			if (is_eof(p))
				break;
			if (sections > 0) {
				SKIP_WHITESPACE(p);
				if (*p == '\0' || *p == INI_COMMENT_CHAR)
					continue;
				strListAnnex(&lp[sections - 1]->list, p, keys++);
			}
		}
	}

	lp[sections] = NULL;    /* terminate list */

	return lp;
}

str_list_t iniGetParsedSectionList(named_str_list_t** list, const char* prefix)
{
	size_t            i;
	size_t            count = 0;
	str_list_t        result = strListInit();
	named_str_list_t* section;

	for (i = 0; list != NULL && list[i] != NULL; ++i) {
		section = list[i];
		if (section->name == NULL)
			continue;
		if (prefix != NULL) {
			if (strnicmp(section->name, prefix, strlen(prefix)) != 0)
				continue;
		}
		strListAppend(&result, section->name, count++);
	}
	return result;
}

str_list_t iniGetParsedSection(named_str_list_t** list, const char* name, bool cut)
{
	size_t            i;
	named_str_list_t* section;

	if (name == NULL) // Root section not supported
		return NULL;

	if (list == NULL)
		return NULL;

	for (i = 0; list[i] != NULL; ++i) {
		section = list[i];
		if (section->name == NULL)
			continue;
		if (stricmp(section->name, name) == 0) {
			if (cut) {
				free(section->name);
				section->name = NULL;
			}
			return section->list;
		}
	}
	return NULL;
}

void* iniFreeParsedSections(named_str_list_t** list)
{
	size_t i;

	if (list == NULL)
		return NULL;

	for (i = 0; list[i] != NULL; ++i) {
		free(list[i]->name);
		free(list[i]->list);
		free(list[i]);
	}

	free(list);
	return NULL;
}

/* These functions read a single key of the specified type */

static bool isTrue(const char* value)
{
	char* str;
	char* p;
	bool  is_true;

	if (!IS_ALPHA(*value))
		return false;

	if ((str = strdup(value)) == NULL)
		return false;

	/* Truncate value at first space, tab or semicolon for purposes of checking for special boolean words. */
	/* This allows comments or white-space to immediately follow a special boolean word: "True", "Yes", or "On" */
	p = str;
	FIND_CHARSET(p, "; \t");
	*p = 0;

	is_true = (stricmp(str, "true") == 0 || stricmp(str, "YES") == 0 || stricmp(str, "ON") == 0);
	free(str);
	return is_true;
}

static int parseInteger(const char* value)
{
	if (isTrue(value))
		return true;

	return (int)strtol(value, NULL, 0);
}

static uint parseUInteger(const char* value)
{
	if (isTrue(value))
		return true;

	return (uint)strtoul(value, NULL, 0);
}

static long parseLongInteger(const char* value)
{
	if (isTrue(value))
		return true;

	return strtol(value, NULL, 0);
}

static ulong parseULongInteger(const char* value)
{
	if (isTrue(value))
		return true;

	return strtoul(value, NULL, 0);
}

#if !defined __BORLANDC__
static int64_t parseInt64(const char* value)
{
	if (isTrue(value))
		return true;

	return strtoll(value, NULL, 0);
}

static uint64_t parseUInt64(const char* value)
{
	if (isTrue(value))
		return true;

	return strtoull(value, NULL, 0);
}
#endif

static bool parseBool(const char* value)
{
	return INT_TO_BOOL(parseInteger(value));
}

int iniReadInteger(FILE* fp, const char* section, const char* key, int deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseInteger(value);
}

uint iniReadUInteger(FILE* fp, const char* section, const char* key, uint deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseUInteger(value);
}

int iniGetInteger(str_list_t list, const char* section, const char* key, int deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	return parseInteger(vp);
}

/* Returns the default value if key value is out of range */
int iniGetIntInRange(str_list_t list, const char* section, const char* key, int min, int deflt, int max)
{
	char* vp = NULL;
	int   result;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	result = parseInteger(vp);
	if (result < min || result > max)
		return deflt;
	return result;
}

/* Returns the min or max value if key value is out of range */
int iniGetClampedInt(str_list_t list, const char* section, const char* key, int min, int deflt, int max)
{
	char* vp = NULL;
	int   result;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	result = parseInteger(vp);
	if (result < min)
		return min;
	if (result > max)
		return max;
	return result;
}

uint iniGetUInteger(str_list_t list, const char* section, const char* key, uint deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	return parseUInteger(vp);
}

short iniReadShortInt(FILE* fp, const char* section, const char* key, short deflt)
{
	return (short)iniReadInteger(fp, section, key, deflt);
}

ushort iniReadUShortInt(FILE* fp, const char* section, const char* key, ushort deflt)
{
	return (ushort)iniReadUInteger(fp, section, key, deflt);
}

short iniGetShortInt(str_list_t list, const char* section, const char* key, short deflt)
{
	return (short)iniGetInteger(list, section, key, deflt);
}

ushort iniGetUShortInt(str_list_t list, const char* section, const char* key, ushort deflt)
{
	return (ushort)iniGetUInteger(list, section, key, deflt);
}

long iniReadLongInt(FILE* fp, const char* section, const char* key, long deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseLongInteger(value);
}

ulong iniReadULongInt(FILE* fp, const char* section, const char* key, ulong deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseULongInteger(value);
}

long iniGetLongInt(str_list_t list, const char* section, const char* key, long deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	return parseLongInteger(vp);
}

ulong iniGetULongInt(str_list_t list, const char* section, const char* key, ulong deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	return parseULongInteger(vp);
}

#if !defined __BORLANDC__
int64_t iniReadInt64(FILE* fp, const char* section, const char* key, int64_t deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseInt64(value);
}

uint64_t iniReadUInt64(FILE* fp, const char* section, const char* key, uint64_t deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseUInt64(value);
}

int64_t iniGetInt64(str_list_t list, const char* section, const char* key, int64_t deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	return parseInt64(vp);
}

uint64_t iniGetUInt64(str_list_t list, const char* section, const char* key, uint64_t deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	return parseUInt64(vp);
}
#endif

int64_t iniReadBytes(FILE* fp, const char* section, const char* key, uint unit, int64_t deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parse_byte_count(value, unit);
}

int64_t iniGetBytes(str_list_t list, const char* section, const char* key, uint unit, int64_t deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	return parse_byte_count(vp, unit);
}

double iniReadDuration(FILE* fp, const char* section, const char* key, double deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parse_duration(value);
}

double iniGetDuration(str_list_t list, const char* section, const char* key, double deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)  /* blank value or missing key */
		return deflt;

	return parse_duration(vp);
}

#if !defined(NO_SOCKET_SUPPORT)

int iniGetSocketOptions(str_list_t list, const char* section, SOCKET sock
                        , char* error, size_t errlen)
{
	int               i;
	int               result;
	char*             name;
	char              err[128];
	BYTE*             vp;
	socklen_t         len;
	int               option;
	int               level;
	int               value;
	int               type = 0; // Assignment is to silence Valgrind.
	LINGER            linger;
	socket_option_t*  socket_options = getSocketOptionList();
#ifdef IPPROTO_IPV6
	union xp_sockaddr addr;
#endif

	len = sizeof(type);
	if ((result = getsockopt(sock, SOL_SOCKET, SO_TYPE, (char*)&type, &len)) != 0) {
		safe_snprintf(error, errlen, "%d (%s) getting socket type", SOCKET_ERRNO, SOCKET_STRERROR(err, sizeof(err)));
		return result;
	}
#ifdef IPPROTO_IPV6
	len = sizeof(addr);
	if ((result = getsockname(sock, &addr.addr, &len)) != 0) {
		safe_snprintf(error, errlen, "%d (%s) getting socket name", SOCKET_ERRNO, SOCKET_STRERROR(err, sizeof(err)));
		return result;
	}
#endif
	for (i = 0; socket_options[i].name != NULL; i++) {
		if (socket_options[i].type != 0
		    && socket_options[i].type != type)
			continue;
#ifdef IPPROTO_IPV6
		if (addr.addr.sa_family != AF_INET6 && socket_options[i].level == IPPROTO_IPV6)
			continue;
#endif
		name = socket_options[i].name;
		if (!iniValueExists(list, section, name))
			continue;
		value = iniGetInteger(list, section, name, 0);

		vp = (BYTE*)&value;
		len = sizeof(value);

		level   = socket_options[i].level;
		option  = socket_options[i].value;

		if (option == SO_LINGER) {
			if (value) {
				linger.l_onoff = true;
				linger.l_linger = value;
			} else {
				ZERO_VAR(linger);
			}
			vp = (BYTE*)&linger;
			len = sizeof(linger);
		}

		if ((result = setsockopt(sock, level, option, (const char *)vp, len)) != 0) {
			safe_snprintf(error, errlen, "%d (%s) setting socket option (%s, %d) to %d"
			              , SOCKET_ERRNO, SOCKET_STRERROR(err, sizeof(err)), name, option, value);
			return result;
		}
	}

	return 0;
}

uint32_t iniReadIpAddress(FILE* fp, const char* section, const char* key, uint32_t deflt)
{
	char  buf[INI_MAX_VALUE_LEN];
	char* value;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseIPv4Address(value);
}

struct in6_addr iniReadIp6Address(FILE* fp, const char* section, const char* key, struct in6_addr deflt)
{
	char  buf[INI_MAX_VALUE_LEN];
	char* value;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseIPv6Address(value);
}

uint32_t iniGetIpAddress(str_list_t list, const char* section, const char* key, uint32_t deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)      /* blank value or missing key */
		return deflt;

	return parseIPv4Address(vp);
}

struct in6_addr iniGetIp6Address(str_list_t list, const char* section, const char* key, struct in6_addr deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)      /* blank value or missing key */
		return deflt;

	return parseIPv6Address(vp);
}

#endif  /* !NO_SOCKET_SUPPORT */

char* iniFileName(char* dest, size_t maxlen, const char* indir, const char* infname)
{
	char  dir[MAX_PATH + 1];
	char  fname[MAX_PATH + 1];
	char  ext[MAX_PATH + 1];
	char* p;

	SAFECOPY(dir, indir);
	backslash(dir);
	SAFECOPY(fname, infname);
	ext[0] = 0;
	if ((p = getfext(fname)) != NULL) {
		SAFECOPY(ext, p);
		*p = 0;
	}

#if !defined(NO_SOCKET_SUPPORT)
	{
		char hostname[128];

		if (gethostname(hostname, sizeof(hostname)) == 0) {
			safe_snprintf(dest, maxlen, "%s%s.%s%s", dir, fname, hostname, ext);
			if (fexistcase(dest))        /* path/file.host.domain.ini */
				return dest;
			if ((p = strchr(hostname, '.')) != NULL) {
				*p = 0;
				safe_snprintf(dest, maxlen, "%s%s.%s%s", dir, fname, hostname, ext);
				if (fexistcase(dest))    /* path/file.host.ini */
					return dest;
			}
		}
	}
#endif

	safe_snprintf(dest, maxlen, "%s%s.%s%s", dir, fname, PLATFORM_DESC, ext);
	if (fexistcase(dest))    /* path/file.platform.ini */
		return dest;

	safe_snprintf(dest, maxlen, "%s%s%s", dir, fname, ext);
	fexistcase(dest);   /* path/file.ini */
	return dest;
}

double iniReadFloat(FILE* fp, const char* section, const char* key, double deflt)
{
	char  buf[INI_MAX_VALUE_LEN];
	char* value;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return atof(value);
}

double iniGetFloat(str_list_t list, const char* section, const char* key, double deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)      /* blank value or missing key */
		return deflt;

	return atof(vp);
}

bool iniReadBool(FILE* fp, const char* section, const char* key, bool deflt)
{
	char  buf[INI_MAX_VALUE_LEN];
	char* value;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseBool(value);
}

bool iniGetBool(str_list_t list, const char* section, const char* key, bool deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)      /* blank value or missing key */
		return deflt;

	return parseBool(vp);
}

static bool validDate(struct tm* tm)
{
	return tm->tm_mon && tm->tm_mon <= 12
	       && tm->tm_mday && tm->tm_mday <= 31;
}

static time_t fixedDateTime(struct tm* tm, const char* tstr, char pm)
{
	if (tm->tm_year < 70)
		tm->tm_year += 100; /* 05 == 2005 (not 1905) and 70 == 1970 (not 2070) */
	else if (tm->tm_year > 1900)
		tm->tm_year -= 1900;
	if (tm->tm_mon)
		tm->tm_mon--;       /* zero-based month field */

	/* hh:mm:ss [p] */
	sscanf(tstr, "%u:%u:%u", &tm->tm_hour, &tm->tm_min, &tm->tm_sec);
	if (tm->tm_hour < 12 && (toupper(pm) == 'P' || strchr(tstr, 'p') || strchr(tstr, 'P')))
		tm->tm_hour += 12;  /* pm, correct for 24 hour clock */

	tm->tm_isdst = -1;    /* auto-detect */

	return mktime(tm);
}

static int getMonth(const char* month)
{
	char *mon[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun"
		           , "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
	int   i;

	for (i = 0; mon[i] != NULL; i++)
		if (strnicmp(month, mon[i], 3) == 0)
			return i + 1;

	return atoi(month);
}

static time_t parseDateTime(const char* value)
{
	char      month[INI_MAX_VALUE_LEN];
	char      tstr[INI_MAX_VALUE_LEN];
	char      pm = 0;
	time_t    t;
	struct tm tm;
	struct tm curr_tm;
	isoDate_t isoDate;
	isoTime_t isoTime;

	ZERO_VAR(tm);
	tstr[0] = 0;

	/* Use current month and year as default */
	t = time(NULL);
	if (localtime_r(&t, &curr_tm) != NULL) {
		tm.tm_mon = curr_tm.tm_mon + 1; /* convert to one-based (reversed later) */
		tm.tm_year = curr_tm.tm_year;
	}

	/* CCYYMMDDTHHMMSS <--- ISO-8601 date and time format */
	if (sscanf(value, "%uT%u"
	           , &isoDate, &isoTime) >= 2)
		return isoDateTime_to_time(isoDate, isoTime);

	/* DD.MM.[CC]YY [time] [p] <-- Euro/Canadian numeric date format */
	if (sscanf(value, "%u.%u.%u %s %c"
	           , &tm.tm_mday, &tm.tm_mon, &tm.tm_year, tstr, &pm) >= 2
	    && validDate(&tm))
		return fixedDateTime(&tm, tstr, pm);

	/* MM/DD/[CC]YY [time] [p] <-- American numeric date format */
	if (sscanf(value, "%u%*c %u%*c %u %s %c"
	           , &tm.tm_mon, &tm.tm_mday, &tm.tm_year, tstr, &pm) >= 2
	    && validDate(&tm))
		return fixedDateTime(&tm, tstr, pm);

	/* DD[-]Mon [CC]YY [time] [p] <-- Perversion of RFC822 date format */
	if (sscanf(value, "%u%*c %s %u %s %c"
	           , &tm.tm_mday, month, &tm.tm_year, tstr, &pm) >= 2
	    && (tm.tm_mon = getMonth(month)) != 0
	    && validDate(&tm))
		return fixedDateTime(&tm, tstr, pm);

	/* Wday, DD Mon YYYY [time] <-- IETF standard (RFC2822) date format */
	if (sscanf(value, "%*s %u %s %u %s"
	           , &tm.tm_mday, month, &tm.tm_year, tstr) >= 2
	    && (tm.tm_mon = getMonth(month)) != 0
	    && validDate(&tm))
		return fixedDateTime(&tm, tstr, 0);

	/* Mon DD[,] [CC]YY [time] [p] <-- Preferred date format */
	if (sscanf(value, "%s %u%*c %u %s %c"
	           , month, &tm.tm_mday, &tm.tm_year, tstr, &pm) >= 2
	    && (tm.tm_mon = getMonth(month)) != 0
	    && validDate(&tm))
		return fixedDateTime(&tm, tstr, pm);

	/* Wday Mon DD YYYY [time] <-- JavaScript (SpiderMonkey) Date.toString() format */
	if (sscanf(value, "%*s %s %u %u %s"
	           , month, &tm.tm_mday, &tm.tm_year, tstr) >= 2
	    && (tm.tm_mon = getMonth(month)) != 0
	    && validDate(&tm))
		return fixedDateTime(&tm, tstr, 0);

	/* Wday Mon DD [time] YYYY <-- ctime() format */
	if (sscanf(value, "%*s %s %u %s %u"
	           , month, &tm.tm_mday, tstr, &tm.tm_year) >= 2
	    && (tm.tm_mon = getMonth(month)) != 0
	    && validDate(&tm))
		return fixedDateTime(&tm, tstr, 0);

	if ((t = xpDateTime_to_time(isoDateTimeStr_parse(value))) != INVALID_TIME)
		return t;

#if defined __BORLANDC__
	return (time_t)strtoul(value, NULL, 0);
#else
	return (time_t)strtoull(value, NULL, 0);
#endif
}

time_t iniReadDateTime(FILE* fp, const char* section, const char* key, time_t deflt)
{
	char  buf[INI_MAX_VALUE_LEN];
	char* value;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseDateTime(value);
}

time_t iniGetDateTime(str_list_t list, const char* section, const char* key, time_t deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)      /* blank value or missing key */
		return deflt;

	return parseDateTime(vp);
}

// Like stricmp(), but either string may be shorter than the other and still match
int partial_stricmp(const char* str1, const char* str2)
{
	int result = 0;

	while (result == 0 && *str1 != '\0' && *str2 != '\0') {
		result = toupper(*str1) - toupper(*str2);
		++str1;
		++str2;
	}

	return result;
}

static unsigned parseEnum(const char* value, str_list_t names, unsigned deflt)
{
	unsigned i, count;
	char     val[INI_MAX_VALUE_LEN];
	char*    p = val;
	char*    endptr;

	/* Strip trailing words (enums must be a single word with no white-space) */
	/* to support comments following enum values */
	SAFECOPY(val, value);
	FIND_WHITESPACE(p);
	*p = 0;

	if ((count = strListCount(names)) == 0)
		return 0;

	/* Look for exact matches first */
	for (i = 0; i < count; i++)
		if (stricmp(names[i], val) == 0)
			return i;

	/* Look for partial matches second */
	for (i = 0; i < count; i++)
		if (partial_stricmp(names[i], val) == 0)
			return i;

	i = strtoul(val, &endptr, 0);
	if (*endptr != 0 && !IS_WHITESPACE(*endptr))
		return deflt;
	if (i >= count)
		i = count - 1;
	return i;
}

unsigned* parseEnumList(const char* values, const char* sep, str_list_t names, unsigned* count)
{
	char*      vals;
	str_list_t list;
	unsigned*  enum_list;
	size_t     i;

	*count = 0;

	if (values == NULL)
		return NULL;

	if ((vals = strdup(values)) == NULL)
		return NULL;

	list = splitList(vals, sep);

	free(vals);

	if ((*count = strListCount(list)) < 1) {
		strListFree(&list);
		return NULL;
	}

	if ((enum_list = (unsigned *)malloc((*count) * sizeof(unsigned))) != NULL) {
		for (i = 0; i < *count; i++)
			enum_list[i] = parseEnum(list[i], names, /* default: */ 0);
	}

	strListFree(&list);

	return enum_list;
}

unsigned iniReadEnum(FILE* fp, const char* section, const char* key, str_list_t names, unsigned deflt)
{
	char  buf[INI_MAX_VALUE_LEN];
	char* value;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseEnum(value, names, deflt);
}

unsigned* iniReadEnumList(FILE* fp, const char* section, const char* key
                          , str_list_t names, unsigned* cp
                          , const char* sep, const char* deflt)
{
	char*    value;
	char     buf[INI_MAX_VALUE_LEN];
	unsigned count;

	if (cp == NULL)
		cp = &count;

	*cp = 0;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL || *value == 0 /* blank */)
		value = (char*)deflt;

	return parseEnumList(value, sep, names, cp);
}

unsigned iniGetEnum(str_list_t list, const char* section, const char* key, str_list_t names, unsigned deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)      /* blank value or missing key */
		return deflt;

	return parseEnum(vp, names, deflt);
}

unsigned* iniGetEnumList(str_list_t list, const char* section, const char* key
                         , str_list_t names, unsigned* cp, const char* sep, const char* deflt)
{
	char*    vp = NULL;
	unsigned count;

	if (cp == NULL)
		cp = &count;

	*cp = 0;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0 /* blank value or missing key */) {
		if (deflt == NULL)
			return NULL;
		vp = (char*)deflt;
	}
	return parseEnumList(vp, sep, names, cp);
}

static int parseNamedInt(const char* value, named_int_t* names)
{
	unsigned i;

	/* Look for exact matches first */
	for (i = 0; names[i].name != NULL; i++)
		if (stricmp(names[i].name, value) == 0)
			return names[i].value;

	/* Look for partial matches second */
	for (i = 0; names[i].name != NULL; i++)
		if (partial_stricmp(names[i].name, value) == 0)
			return names[i].value;

	return parseInteger(value);
}

int iniReadNamedInt(FILE* fp, const char* section, const char* key
                    , named_int_t* names, int deflt)
{
	char  buf[INI_MAX_VALUE_LEN];
	char* value;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseNamedInt(value, names);
}

int iniGetNamedInt(str_list_t list, const char* section, const char* key
                   , named_int_t* names, int deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)      /* blank value or missing key */
		return deflt;

	return parseNamedInt(vp, names);
}

static ulong parseNamedULongInt(const char* value, named_ulong_t* names)
{
	unsigned i;

	/* Look for exact matches first */
	for (i = 0; names[i].name != NULL; i++)
		if (stricmp(names[i].name, value) == 0)
			return names[i].value;

	/* Look for partial matches second */
	for (i = 0; names[i].name != NULL; i++)
		if (partial_stricmp(names[i].name, value) == 0)
			return names[i].value;

	return parseULongInteger(value);
}

ulong iniReadNamedULongInt(FILE* fp, const char* section, const char* key
                           , named_ulong_t* names, ulong deflt)
{
	char  buf[INI_MAX_VALUE_LEN];
	char* value;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseNamedULongInt(value, names);
}

ulong iniGetNamedULongInt(str_list_t list, const char* section, const char* key
                          , named_ulong_t* names, ulong deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)      /* blank value or missing key */
		return deflt;

	return parseNamedULongInt(vp, names);
}

static double parseNamedFloat(const char* value, named_double_t* names)
{
	unsigned i;

	/* Look for exact matches first */
	for (i = 0; names[i].name != NULL; i++)
		if (stricmp(names[i].name, value) == 0)
			return names[i].value;

	/* Look for partial matches second */
	for (i = 0; names[i].name != NULL; i++)
		if (partial_stricmp(names[i].name, value) == 0)
			return names[i].value;

	return atof(value);
}

double iniReadNamedFloat(FILE* fp, const char* section, const char* key
                         , named_double_t* names, double deflt)
{
	char  buf[INI_MAX_VALUE_LEN];
	char* value;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)
		return deflt;

	if (*value == 0)       /* blank value */
		return deflt;

	return parseNamedFloat(value, names);
}

double iniGetNamedFloat(str_list_t list, const char* section, const char* key
                        , named_double_t* names, double deflt)
{
	char* vp = NULL;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0)      /* blank value or missing key */
		return deflt;

	return parseNamedFloat(vp, names);
}

static ulong parseBitField(char* value, ini_bitdesc_t* bitdesc)
{
	int   i;
	char* p;
	char* tp;
	ulong v = 0;

	for (p = value; *p;) {
		tp = strchr(p, INI_BIT_SEP);
		if (tp != NULL)
			*tp = 0;
		truncsp(p);

		for (i = 0; bitdesc[i].name; i++)
			if (!stricmp(bitdesc[i].name, p))
				break;

		if (bitdesc[i].name)
			v |= bitdesc[i].bit;
		else
			v |= strtoul(p, NULL, 0);

		if (tp == NULL)
			break;

		p = tp + 1;
		SKIP_WHITESPACE(p);
	}

	return v;
}

uint iniReadBitField(FILE* fp, const char* section, const char* key,
                     ini_bitdesc_t* bitdesc, uint deflt)
{
	char* value;
	char  buf[INI_MAX_VALUE_LEN];

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL)    /* missing key */
		return deflt;

	return parseBitField(value, bitdesc);
}

uint iniGetBitField(str_list_t list, const char* section, const char* key,
                    ini_bitdesc_t* bitdesc, uint deflt)
{
	char* vp = NULL;;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL)        /* missing key */
		return deflt;

	return parseBitField(vp, bitdesc);
}

int* parseIntList(const char* values, const char* sep, unsigned* count)
{
	char*      vals;
	str_list_t list;
	int*       int_list;
	size_t     i;

	*count = 0;

	if (values == NULL)
		return NULL;

	if ((vals = strdup(values)) == NULL)
		return NULL;

	list = splitList(vals, sep);

	free(vals);

	if ((*count = strListCount(list)) < 1) {
		strListFree(&list);
		return NULL;
	}

	if ((int_list = malloc((*count) * sizeof(int))) != NULL) {
		for (i = 0; i < *count; i++)
			int_list[i] = atoi(list[i]);
	}

	strListFree(&list);

	return int_list;
}

int* iniGetIntList(str_list_t list, const char* section, const char* key
                   , unsigned* cp, const char* sep, const char* deflt)
{
	char*    vp = NULL;
	unsigned count;

	if (cp == NULL)
		cp = &count;

	*cp = 0;

	get_value(list, section, key, NULL, &vp, /* literals_supported: */ false);

	if (vp == NULL || *vp == 0 /* blank value or missing key */) {
		if (deflt == NULL)
			return NULL;
		vp = (char*)deflt;
	}
	return parseIntList(vp, sep, cp);
}

int* iniReadIntList(FILE* fp, const char* section, const char* key
                    , unsigned* cp, const char* sep, const char* deflt)
{
	char*    value;
	char     buf[INI_MAX_VALUE_LEN];
	unsigned count;

	if (cp == NULL)
		cp = &count;

	*cp = 0;

	if ((value = read_value(fp, section, key, buf, /* literals_supported: */ false)) == NULL || *value == 0 /* blank */)
		value = (char*)deflt;

	return parseIntList(value, sep, cp);
}

FILE* iniOpenFile(const char* fname, bool for_modify)
{
	char* mode = "r";

	if (for_modify)
		mode = fexist(fname) ? "r+" : "w+";

	return fopen(fname, mode);
}

bool iniCloseFile(FILE* fp)
{
	return fclose(fp) == 0;
}

str_list_t iniReadFile(FILE* fp)
{
	char       str[INI_MAX_LINE_LEN];
	char       err[512];
	char*      p;
	size_t     i;
	size_t     inc_len;
	size_t     inc_counter = 0;
	str_list_t list;
	FILE*      insert_fp = NULL;

	if (fp != NULL)
		rewind(fp);

	list = strListReadFile(fp, NULL, INI_MAX_LINE_LEN);
	if (list == NULL)
		return NULL;

	/* Look for !include directives */
	inc_len = strlen(INI_INCLUDE_DIRECTIVE);
	for (i = 0; list[i] != NULL; i++) {
		if (strnicmp(list[i], INI_INCLUDE_DIRECTIVE, inc_len) == 0) {
			glob_t gl = {0};
			size_t j;
			p = list[i] + inc_len;
			SKIP_WHITESPACE(p);
			truncsp(p);
			(void)glob(p, GLOB_MARK, NULL, &gl);
			safe_snprintf(str, sizeof(str), "; %s - %lu matches found", list[i], (ulong)gl.gl_pathc);
			strListReplace(list, i, str);
			for (j = 0; j < gl.gl_pathc; j++) {
				char* fname = gl.gl_pathv[j];
				if (*lastchar(fname) == '/')
					continue;
				if (inc_counter >= INI_INCLUDE_MAX)
					SAFEPRINTF2(str, "; %s - MAXIMUM INCLUDES REACHED: %u", fname, INI_INCLUDE_MAX);
				else if ((insert_fp = fopen(fname, "r")) == NULL)
					SAFEPRINTF2(str, "; %s - FAILURE: %s", fname, safe_strerror(errno, err, sizeof(err)));
				else
					SAFEPRINTF(str, "; %s", fname);
				strListInsert(&list, str, i + 1);
				if (insert_fp != NULL) {
					strListInsertFile(insert_fp, &list, i + 2, INI_MAX_LINE_LEN);
					fclose(insert_fp);
					insert_fp = NULL;
					inc_counter++;
				}
			}
			globfree(&gl);
		}
	}

	/* truncate new-line chars off end of strings */
	for (i = 0; list[i] != NULL; i++)
		truncnl(list[i]);

	return list;
}

bool iniHasInclude(const str_list_t list)
{
	size_t i;

	/* Look for !include directives */
	size_t inc_len = strlen(INI_INCLUDE_DIRECTIVE) + 1;
	for (i = 0; list[i] != NULL; i++) {
		if (strnicmp(list[i], ";" INI_INCLUDE_DIRECTIVE, inc_len) == 0)
			return true;
	}
	return false;
}

bool iniWriteFile(FILE* fp, const str_list_t list)
{
	size_t count;
	long   pos;

	rewind(fp);
	count = strListWriteFile(fp, list, "\n");
	fflush(fp);
	pos = ftell(fp);
	if (pos == -1)
		return false;
	if (chsize(fileno(fp), pos) != 0) {  /* truncate */
		fseek(fp, 0, SEEK_END);
		return false;
	}
	fseek(fp, 0, SEEK_END);

	return count == strListCount(list);
}

#ifdef INI_FILE_TEST
void main(int argc, char** argv)
{
	int        i;
	size_t     l;
	char       str[128];
	FILE*      fp;
	str_list_t list;

	for (i = 1; i < argc; i++) {
		if ((fp = iniOpenFile(argv[i], false)) == NULL) {
			perror(argv[i]);
			continue;
		}
		if ((list = iniReadFile(fp)) != NULL) {
			named_str_list_t** ini = iniParseSections(list);
			str_list_t         sections = iniGetParsedSectionList(ini, NULL);
			for (size_t j = 0; sections[j] != NULL; j++)
				printf("%s\n", sections[j]);
			strListFree(&sections);
		}
		fclose(fp);
	}
}
#endif
