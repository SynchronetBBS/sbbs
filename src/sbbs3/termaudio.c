/* Pure helpers for terminal audio @-codes. See termaudio.h. */

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
 ****************************************************************************/

#include "termaudio.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float termaudio_db_from_pct(int pct)
{
	if (pct <= 0)
		return TERMAUDIO_DB_MUTE;
	if (pct >= 100)
		return TERMAUDIO_DB_UNITY;
	return 20.0f * log10f((float)pct / 100.0f);
}

bool termaudio_parse_volume(const char* arg, float* db)
{
	size_t len;
	bool   is_db = false;
	char   buf[32];
	char*  end;
	double val;

	if (arg == NULL || db == NULL || *arg == '\0')
		return false;
	len = strlen(arg);
	if (len >= sizeof(buf))
		return false;
	memcpy(buf, arg, len + 1);

	/* A "dB" suffix (either case) means the value is already decibels. A bare
	   number is a friendly percentage, converted here so that no percentage
	   ever reaches the wire: SyncTERM's mixer is dB-native, and its bare
	   percent path is clamped at unity and integer-only. */
	if (len > 2 && tolower((unsigned char)buf[len - 2]) == 'd'
	    && tolower((unsigned char)buf[len - 1]) == 'b') {
		is_db = true;
		buf[len - 2] = '\0';
	}
	if (buf[0] == '\0')
		return false;

	errno = 0;
	val = strtod(buf, &end);
	if (end == buf || *end != '\0')
		return false;
	if (errno == ERANGE || !isfinite(val))
		return false;

	if (is_db) {
		if (val < (double)TERMAUDIO_DB_MUTE)
			val = (double)TERMAUDIO_DB_MUTE;
		if (val > (double)TERMAUDIO_DB_MAX)
			val = (double)TERMAUDIO_DB_MAX;
		*db = (float)val;
		return true;
	}
	if (val > (double)INT_MAX)
		val = (double)INT_MAX;
	if (val < (double)INT_MIN)
		val = (double)INT_MIN;
	*db = termaudio_db_from_pct((int)val);
	return true;
}

bool termaudio_resolve_path(const char* text_dir, const char* arg
                            , bool allow_subdir, char* out, size_t outsz)
{
	size_t      dlen;
	size_t      alen;
	const char* p;

	if (text_dir == NULL || arg == NULL || out == NULL || outsz == 0)
		return false;
	if (*arg == '\0')
		return false;
	if (*arg == '/' || *arg == '\\')
		return false;
	if (strchr(arg, '\\') != NULL)
		return false;
	if (strstr(arg, "..") != NULL)
		return false;
	if (strchr(arg, ':') != NULL)       /* drive letters, and a stray arg split */
		return false;
	if (!allow_subdir && strchr(arg, '/') != NULL)
		return false;

	/* Reject a leading, doubled, or trailing separator even when subdirectories
	   are allowed, so the result is always an ordinary relative file path. */
	for (p = arg; *p != '\0'; p++) {
		if (*p == '/' && (p == arg || p[1] == '/' || p[1] == '\0'))
			return false;
	}

	dlen = strlen(text_dir);
	alen = strlen(arg);
	if (dlen + alen + 1 > outsz)
		return false;
	memcpy(out, text_dir, dlen);
	memcpy(out + dlen, arg, alen + 1);
	return true;
}

bool termaudio_resolve_sound(const char* text_dir, const char* arg
                             , char* out, size_t outsz)
{
	char rel[256];

	/* The first stage refuses any separator, so the prefix cannot be escaped;
	   the second joins the result to text_dir. */
	if (!termaudio_resolve_path("sound/", arg, /* allow_subdir: */ false, rel, sizeof(rel)))
		return false;
	return termaudio_resolve_path(text_dir, rel, /* allow_subdir: */ true, out, outsz);
}

void termaudio_cache_name(const uint8_t md5[16], const char* ext
                          , char* out, size_t outsz)
{
	static const char hex[] = "0123456789abcdef";
	char              tmp[TERMAUDIO_MAX_CACHE_NAME];
	size_t            n;
	int               i;

	if (out == NULL || outsz == 0)
		return;
	memcpy(tmp, "sbbs_", 5);
	n = 5;
	for (i = 0; i < 16 && n + 2 < sizeof(tmp); i++) {
		tmp[n++] = hex[(md5[i] >> 4) & 0x0f];
		tmp[n++] = hex[md5[i] & 0x0f];
	}
	if (ext != NULL && *ext != '\0' && n + 1 < sizeof(tmp)) {
		size_t e = 0;
		tmp[n++] = '.';
		while (ext[e] != '\0' && e < 8 && n + 1 < sizeof(tmp))
			tmp[n++] = ext[e++];
	}
	tmp[n] = '\0';
	if (n + 1 > outsz)
		n = outsz - 1;
	memcpy(out, tmp, n);
	out[n] = '\0';
}

/* The reply is CSI = 7 ; 100 ; <0|1> n, arriving here ESC-stripped as
   "[=7;100;1n". Report 7 is the audio state; feature 100 is libsndfile. */
int termaudio_parse_audio_state(const char* token)
{
	unsigned report = 0;
	unsigned feature = 0;
	unsigned value = 0;
	size_t   len;

	if (token == NULL)
		return -1;
	len = strlen(token);
	if (len < 2 || token[len - 1] != 'n')
		return -1;
	if (sscanf(token, "[=%u;%u;%u", &report, &feature, &value) != 3)
		return -1;
	if (report != 7 || feature != 100)
		return -1;
	return value ? 1 : 0;
}

size_t termaudio_parse_file_list(const char* body
                                 , char names[][TERMAUDIO_MAX_CACHE_NAME]
                                 , size_t maxnames)
{
	const char* p;
	size_t      count = 0;
	bool        first_line = true;

	if (body == NULL || names == NULL || maxnames == 0)
		return 0;

	for (p = body; *p != '\0' && count < maxnames; ) {
		const char* eol = strchr(p, '\n');
		const char* end = (eol != NULL) ? eol : (p + strlen(p));
		const char* tab = (const char*)memchr(p, '\t', (size_t)(end - p));

		if (first_line) {
			first_line = false;   /* the "SyncTERM:C;L" header */
		} else if (tab != NULL && tab > p) {
			size_t nlen = (size_t)(tab - p);
			if (nlen < TERMAUDIO_MAX_CACHE_NAME) {
				memcpy(names[count], p, nlen);
				names[count][nlen] = '\0';
				count++;
			}
		}
		if (eol == NULL)
			break;
		p = eol + 1;
	}
	return count;
}
