/* Read a string (item) from ctrl/text.dat */

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

#include "sbbsdefs.h"
#include "str_util.h"
#include "readtext.h"
#include "text_defaults.h"  /* text_defaults */

int lprintf(int level, const char *fmt, ...);       /* log output */
/****************************************************************************/
/* Reads special TEXT.DAT printf style text lines, splicing multiple lines, */
/* replacing escaped characters, and allocating the memory					*/
/****************************************************************************/
char *readtext(int *line, FILE *stream, int dflt, named_string_t** substr_list)
{
	char buf[MAX_TEXTDAT_ITEM_LEN + 256], str[MAX_TEXTDAT_ITEM_LEN + 1], tmp[256], *p, *p2;
	int  i, j, k;

	if (!fgets(buf, 256, stream)) {
		/* Hide the EOF */
		if (feof(stream))
			clearerr(stream);
		goto use_default;
	}
	if (line)
		(*line)++;
	if (buf[0] == '#')
		goto use_default;
	p = strrchr(buf, '"');
	if (!p) {
		if (line)
			lprintf(LOG_WARNING, "No quotation marks in line %d of text.dat", *line);
		goto use_default;
	}
	if (*(p + 1) == '\\')    /* merge multiple lines */
		while (strlen(buf) < MAX_TEXTDAT_ITEM_LEN) {
			if (!fgets(str, 255, stream)) {
				/* Hide the EOF */
				if (feof(stream))
					clearerr(stream);
				goto use_default;
			}
			if (line)
				(*line)++;
			p2 = strchr(str, '"');
			if (!p2)
				continue;
			strcpy(p, p2 + 1);
			p = strrchr(p, '"');
			if (p && *(p + 1) == '\\')
				continue;
			break;
		}
	if (p != NULL)
		*p = '\0';
	k = strlen(buf);
	for (i = 1, j = 0; i < k && j < sizeof(str) - 1; j++) {
		if (buf[i] == '\\')    { /* escape */
			i++;
			if (IS_DIGIT(buf[i])) {
				str[j] = atoi(buf + i);     /* decimal, NOT octal */
				if (IS_DIGIT(buf[++i]))  /* skip up to 3 digits */
					if (IS_DIGIT(buf[++i]))
						i++;
				continue;
			}
			switch (buf[i++]) {
				case '\\':
					str[j] = '\\';
					break;
				case '?':
					str[j] = '?';
					break;
				case 'x':
					tmp[0] = buf[i++];        /* skip next character */
					tmp[1] = 0;
					if (isxdigit(buf[i])) {  /* if another hex digit, skip too */
						tmp[1] = buf[i++];
						tmp[2] = 0;
					}
					str[j] = (char)ahtoul(tmp);
					break;
				case '\'':
					str[j] = '\'';
					break;
				case '"':
					str[j] = '"';
					break;
				case 'r':
					str[j] = CR;
					break;
				case 'n':
					str[j] = LF;
					break;
				case 't':
					str[j] = TAB;
					break;
				case 'b':
					str[j] = BS;
					break;
				case 'a':
					str[j] = BEL;
					break;
				case 'f':
					str[j] = FF;
					break;
				case 'v':
					str[j] = 11;  /* VT */
					break;
				default:
					str[j] = buf[i];
					break;
			}
			continue;
		}
		str[j] = buf[i++];
	}
	str[j] = 0;
	if (substr_list)
		replace_named_values(str, buf, sizeof buf, /* escape_seq */ NULL, substr_list, /* str_list: */NULL, /* int_list : */ NULL, /* case_sensitive: */ true);
	else
		SAFECOPY(buf, str);
	if ((p = strdup(buf)) == NULL) {
		lprintf(LOG_CRIT, "Error allocating %u bytes of memory from text.dat", j);
		goto use_default;
	}
	return p;
use_default:
	if (dflt < TOTAL_TEXT) {
		p = strdup(text_defaults[dflt]);
		if (p == NULL)
			lprintf(LOG_CRIT, "Error duplicating %s text defaults", text_defaults[dflt]);
		return p;
	}
	lprintf(LOG_CRIT, "Text defaults missing %d", dflt);
	return NULL;
}
