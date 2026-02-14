/* Synchronet Access Requirement String (ARS) functions */

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

#include "ars_defs.h"

static inline BOOL ar_string_arg(int artype)
{
	return ar_type(artype) == AR_STRING;
}

/* Converts ASCII ARS string into binary ARS buffer */

#ifdef __BORLANDC__ /* Eliminate warning when buildling Baja */
#pragma argsused
#endif
uchar* arstr(ushort* count, const char* str, scfg_t* cfg, uchar* ar_buf)
{
	const char* p;
	const char* np;
	char        ch;
	uchar       ar[1024];
	int         artype = AR_INVALID;
	uint        i, j, n, not = 0, equal = 0;
	uint        maxlen;
	BOOL        arg_expected = FALSE;

	if (str == NULL) {
		if (count)
			(*count) = 0;
		return NULL;
	}

	for (i = j = 0; str[i]; i++) {
		if (str[i] == ' ')
			continue;

		if (str[i] == '(') {
			if (not)
				ar[j++] = AR_NOT;
			not = equal = 0;
			ar[j++] = AR_BEGNEST;
			continue;
		}

		if (str[i] == ')') {
			ar[j++] = AR_ENDNEST;
			continue;
		}

		if (str[i] == '|') {
			ar[j++] = AR_OR;
			continue;
		}

		if (str[i] == '!') {
			not = 1;
			continue;
		}

		if (str[i] == '=') {
			equal = 1;
			continue;
		}

		if (str[i] == '&')
			continue;

		if (IS_ALPHA(str[i])) {
			p = np = str + i;
			SKIP_ALPHA(np);
			n = np - p;
			if (n == 2 && !strnicmp(p, "OR", 2)) {
				ar[j++] = AR_OR;
				i++;
				continue;
			}

			if (n == 3 && !strnicmp(p, "AND", 3)) {    /* AND is ignored */
				i += 2;
				continue;
			}

			if (n == 3 && !strnicmp(p, "NOT", 3)) {
				not = 1;
				i += 2;
				continue;
			}

			if (n == 2 && equal && !strnicmp(p, "TO", 2)) {  /* EQUAL TO */
				i++;
				continue;
			}

			if (n == 5 && !strnicmp(p, "EQUAL", 5)) {
				equal = 1;
				i += 4;
				continue;
			}

			if (n == 6 && !strnicmp(p, "EQUALS", 6)) {
				equal = 1;
				i += 5;
				continue;
			}
		}

		if (str[i] == '$') {
			arg_expected = TRUE;
			switch ((ch = toupper(str[++i]))) {
				case 'A':
					artype = AR_AGE;
					break;
				case 'B':
					artype = AR_BPS;
					break;
				case 'C':
					artype = AR_CREDIT;
					break;
				case 'D':
					artype = AR_UDFR;
					break;
				case 'E':
					artype = AR_EXPIRE;
					break;
				case 'F':
					artype = AR_FLAG1;
					break;
				case 'H':
					artype = AR_SUB;
					break;
				case 'I':
					artype = AR_LIB;
					break;
				case 'J':
					artype = AR_DIR;
					break;
				case 'K':
					artype = AR_UDR;
					break;
				case 'L':
					artype = AR_LEVEL;
					break;
				case 'M':
					artype = AR_GROUP;
					break;
				case 'N':
					artype = AR_NODE;
					break;
				case 'O':
					artype = AR_TUSED;
					break;
				case 'P':
					artype = AR_PCR;
					break;
				case 'Q':
					artype = AR_RANDOM;
					break;
				case 'R':
					artype = AR_TLEFT;
					break;
				case 'S':
					artype = AR_SEX;
					break;
				case 'T':
					artype = AR_TIME;
					break;
				case 'U':
					artype = AR_USER;
					break;
				case 'V':
					artype = AR_LOGONS;
					break;
				case 'W':
					artype = AR_DAY;
					break;
				case 'X':
					artype = AR_EXEMPT;
					break;
				case 'Y':   /* Days since last on */
					artype = AR_LASTON;
					break;
				case 'Z':
					artype = AR_REST;
				/* Boolean (no argument) symbols */
				case '0':
				case 'G':
				case '*':
				case '[':
					switch (ch) {
						case '0':
							artype = AR_NULL;
							break;
						case 'G':
							artype = AR_LOCAL;
							break;
						case '*':
							artype = AR_RIP;
							break;
						case '[':
							artype = AR_ANSI;
							break;
					}
					if (not)
						ar[j++] = AR_NOT;
					not = 0;
					ar[j++] = artype;
					artype = AR_INVALID;
					arg_expected = FALSE;
					break;
			}
			continue;
		}

		if (!arg_expected && IS_ALPHA(str[i])) {
			n = i;
			if (!strnicmp(str + i, "AGE", 3)) {
				artype = AR_AGE;
				i += 2;
			}
			else if (!strnicmp(str + i, "BPS", 3)) {
				artype = AR_BPS;
				i += 2;
			}
			else if (!strnicmp(str + i, "PCR", 3)) {
				artype = AR_PCR;
				i += 2;
			}
			else if (!strnicmp(str + i, "SEX", 3)) {
				artype = AR_SEX;
				i += 2;
			}
			else if (!strnicmp(str + i, "UDR", 3)) {
				artype = AR_UDR;
				i += 2;
			}
			else if (!strnicmp(str + i, "ULS", 3)) {
				artype = AR_ULS;
				i += 2;
			}
			else if (!strnicmp(str + i, "ULK", 3)) {
				artype = AR_ULK;
				i += 2;
			}
			else if (!strnicmp(str + i, "ULM", 3)) {
				artype = AR_ULM;
				i += 2;
			}
			else if (!strnicmp(str + i, "DLS", 3)) {
				artype = AR_DLS;
				i += 2;
			}
			else if (!strnicmp(str + i, "DLT", 3)) {
				artype = AR_DLT;
				i += 2;
			}
			else if (!strnicmp(str + i, "DLK", 3)) {
				artype = AR_DLK;
				i += 2;
			}
			else if (!strnicmp(str + i, "DLM", 3)) {
				artype = AR_DLM;
				i += 2;
			}
			else if (!strnicmp(str + i, "DAY", 3)) {
				artype = AR_DAY;
				i += 2;
			}
			else if (!strnicmp(str + i, "RIP", 3)) {
				artype = AR_RIP;
				i += 2;
			}
			else if (!strnicmp(str + i, "WIP", 3)) {
				artype = AR_WIP;
				i += 2;
			}
			else if (!strnicmp(str + i, "OS2", 3)) {
				artype = AR_OS2;
				i += 2;
			}
			else if (!strnicmp(str + i, "DOS", 3)) {
				artype = AR_DOS;
				i += 2;
			}
			else if (!strnicmp(str + i, "WIN32", 5)) {
				artype = AR_WIN32;
				i += 4;
			}
			else if (!strnicmp(str + i, "UNIX", 4)) {
				artype = AR_UNIX;
				i += 3;
			}
			else if (!strnicmp(str + i, "LINUX", 5)) {
				artype = AR_LINUX;
				i += 4;
			}
			else if (!strnicmp(str + i, "PROT", 4)) {
				artype = AR_PROT;
				i += 3;
			}
			else if (!strnicmp(str + i, "HOST", 4)) {
				artype = AR_HOST;
				i += 3;
			}
			else if (!strnicmp(str + i, "IP", 2)) {
				artype = AR_IP;
				i++;
			}
			else if (!strnicmp(str + i, "SUBCODE", 7)) {
				artype = AR_SUBCODE;
				i += 6;
			}
			else if (!strnicmp(str + i, "SUB", 3)) {
				artype = AR_SUB;
				i += 2;
			}
			else if (!strnicmp(str + i, "LIB", 3)) {
				artype = AR_LIB;
				i += 2;
			}
			else if (!strnicmp(str + i, "DIRCODE", 7)) {
				artype = AR_DIRCODE;
				i += 6;
			}
			else if (!strnicmp(str + i, "DIR", 3)) {
				artype = AR_DIR;
				i += 2;
			}
			else if (!strnicmp(str + i, "ANSI", 4)) {
				artype = AR_ANSI;
				i += 3;
			}
			else if (!strnicmp(str + i, "PETSCII", 7)) {
				artype = AR_PETSCII;
				i += 6;
			}
			else if (!strnicmp(str + i, "ASCII", 5)) {
				artype = AR_ASCII;
				i += 4;
			}
			else if (!strnicmp(str + i, "UTF8", 4)) {
				artype = AR_UTF8;
				i += 3;
			}
			else if (!strnicmp(str + i, "CP437", 5)) {
				artype = AR_CP437;
				i += 4;
			}
			else if (!strnicmp(str + i, "TERM", 4)) {
				artype = AR_TERM;
				i += 3;
			}
			else if (!strnicmp(str + i, "LANG", 4)) {
				artype = AR_LANG;
				i += 3;
			}
			else if (!strnicmp(str + i, "COLS", 4)) {
				artype = AR_COLS;
				i += 3;
			}
			else if (!strnicmp(str + i, "ROWS", 4)) {
				artype = AR_ROWS;
				i += 3;
			}
			else if (!strnicmp(str + i, "UDFR", 4)) {
				artype = AR_UDFR;
				i += 3;
			}
			else if (!strnicmp(str + i, "FLAG", 4)) {
				artype = AR_FLAG1;
				i += 3;
			}
			else if (!strnicmp(str + i, "NODE", 4)) {
				artype = AR_NODE;
				i += 3;
			}
			else if (!strnicmp(str + i, "NULL", 4)) {
				artype = AR_NULL;
				i += 3;
			}
			else if (!strnicmp(str + i, "USER", 4)) {
				artype = AR_USER;
				i += 3;
			}
			else if (!strnicmp(str + i, "TIME", 4)) {
				artype = AR_TIME;
				i += 3;
			}
			else if (!strnicmp(str + i, "REST", 4)) {
				artype = AR_REST;
				i += 3;
			}
			else if (!strnicmp(str + i, "LEVEL", 5)) {
				artype = AR_LEVEL;
				i += 4;
			}
			else if (!strnicmp(str + i, "TLEFT", 5)) {
				artype = AR_TLEFT;
				i += 4;
			}
			else if (!strnicmp(str + i, "TUSED", 5)) {
				artype = AR_TUSED;
				i += 4;
			}
			else if (!strnicmp(str + i, "LOCAL", 5)) {
				artype = AR_LOCAL;
				i += 4;
			}
			else if (!strnicmp(str + i, "GROUP", 5)) {
				artype = AR_GROUP;
				i += 4;
			}
			else if (!strnicmp(str + i, "EXPIRE", 6)) {
				artype = AR_EXPIRE;
				i += 5;
			}
			else if (!strnicmp(str + i, "ACTIVE", 6)) {
				artype = AR_ACTIVE;
				i += 5;
			}
			else if (!strnicmp(str + i, "INACTIVE", 8)) {
				artype = AR_INACTIVE;
				i += 7;
			}
			else if (!strnicmp(str + i, "DELETED", 7)) {
				artype = AR_DELETED;
				i += 6;
			}
			else if (!strnicmp(str + i, "EXPERT", 6)) {
				artype = AR_EXPERT;
				i += 5;
			}
			else if (!strnicmp(str + i, "SYSOP", 5)) {
				artype = AR_SYSOP;
				i += 4;
			}
			else if (!strnicmp(str + i, "GUEST", 5)) {
				artype = AR_GUEST;
				i += 4;
			}
			else if (!strnicmp(str + i, "QNODE", 5)) {
				artype = AR_QNODE;
				i += 4;
			}
			else if (!strnicmp(str + i, "QUIET", 5)) {
				artype = AR_QUIET;
				i += 4;
			}
			else if (!strnicmp(str + i, "EXEMPT", 6)) {
				artype = AR_EXEMPT;
				i += 5;
			}
			else if (!strnicmp(str + i, "RANDOM", 6)) {
				artype = AR_RANDOM;
				i += 5;
			}
			else if (!strnicmp(str + i, "LASTON", 6)) {
				artype = AR_LASTON;
				i += 5;
			}
			else if (!strnicmp(str + i, "LOGONS", 6)) {
				artype = AR_LOGONS;
				i += 5;
			}
			else if (!strnicmp(str + i, "CREDIT", 6)) {
				artype = AR_CREDIT;
				i += 5;
			}
			else if (!strnicmp(str + i, "MAIN_CMDS", 9)) {
				artype = AR_MAIN_CMDS;
				i += 8;
			}
			else if (!strnicmp(str + i, "FILE_CMDS", 9)) {
				artype = AR_FILE_CMDS;
				i += 8;
			}
			else if (!strnicmp(str + i, "SHELL", 5)) {
				artype = AR_SHELL;
				i += 4;
			}
			else if (!strnicmp(str + i, "PROP", 4)) {
				artype = AR_PROP;
				i += 3;
			}

			if (n != i)        /* one of the above */
			{
				if (ar_type(artype) == AR_BOOL) {
					/* Boolean (No arguments) */
					if (not)
						ar[j++] = AR_NOT;
					not = 0;
					ar[j++] = artype;
					artype = AR_INVALID;
					arg_expected = FALSE;
				}
				continue;
			}
		}
		if (not)
			ar[j++] = AR_NOT;
		if (equal && !ar_string_arg(artype))
			ar[j++] = AR_EQUAL;
		not = equal = 0;

		if (artype == AR_FLAG1 && IS_DIGIT(str[i])) {   /* flag set specified */
			switch (str[i]) {
				case '2':
					artype = AR_FLAG2;
					break;
				case '3':
					artype = AR_FLAG3;
					break;
				case '4':
					artype = AR_FLAG4;
					break;
			}
			continue;
		}

		arg_expected = FALSE;

		// Auto-detect AR keyword base on parameter class (numeric or alpha)
		switch (artype) {
			case AR_SUB:
			case AR_SUBCODE:
				artype = IS_DIGIT(str[i]) ? AR_SUB : AR_SUBCODE;
				break;
			case AR_DIR:
			case AR_DIRCODE:
				artype = IS_DIGIT(str[i]) ? AR_DIR : AR_DIRCODE;
				break;
			case AR_USER:
			case AR_USERNAME:
				artype = IS_DIGIT(str[i]) ? AR_USER : AR_USERNAME;
				break;
		}

		if (artype == AR_INVALID)
			artype = AR_LEVEL;
		ar[j++] = artype;
		if (IS_DIGIT(str[i]) && !ar_string_arg(artype)) {
			if (artype == AR_TIME) {
				n = atoi(str + i) * 60;
				p = strchr(str + i, ':');
				if (p)
					n += atoi(p + 1);
				ar[j++] = n & 0xff;
				ar[j++] = (n >> 8) & 0xff;
				while (IS_DIGIT(str[i + 1]) || str[i + 1] == ':') i++;
				continue;
			}
			n = atoi(str + i);
			switch (artype) {
				case AR_DAY:
					if (n > 6)     /* not past saturday */
						n = 6;
				// fall-through
				case AR_AGE:    /* byte operands */
				case AR_PCR:
				case AR_UDR:
				case AR_UDFR:
				case AR_ROWS:
				case AR_COLS:
				case AR_NODE:
				case AR_LEVEL:
				case AR_TLEFT:
				case AR_TUSED:
					ar[j++] = n;
					break;
				case AR_BPS:    /* int operands */
					if (n < 300)
						n *= 100;
				// fall-through
				case AR_MAIN_CMDS:
				case AR_FILE_CMDS:
				case AR_EXPIRE:
				case AR_CREDIT:
				case AR_USER:
				case AR_RANDOM:
				case AR_LASTON:
				case AR_LOGONS:
				case AR_ULS:
				case AR_ULK:
				case AR_ULM:
				case AR_DLS:
				case AR_DLT:
				case AR_DLK:
				case AR_DLM:
					ar[j++] = n & 0xff;
					ar[j++] = (n >> 8) & 0xff;
					break;
				case AR_GROUP:
				case AR_LIB:
				case AR_DIR:
				case AR_SUB:
					if (n > 0)
						n--;                    /* convert to 0 base */
					ar[j++] = n & 0xff;
					ar[j++] = (n >> 8) & 0xff;
					break;
				default:                    /* invalid numeric AR type */
					j--;
					break;
			}
			while (IS_DIGIT(str[i + 1])) i++;
			continue;
		}
		maxlen = 128;
		switch (artype) {
			case AR_SUBCODE:
			case AR_DIRCODE:
			case AR_SHELL:
				maxlen = LEN_EXTCODE;
			// fall-through
			case AR_PROT:
			case AR_HOST:
			case AR_IP:
			case AR_TERM:
			case AR_LANG:
			case AR_USERNAME:
			case AR_PROP:
				/* String argument */
				for (n = 0; n < maxlen
				     && str[i]
				     && str[i] != ' '
				     && str[i] != '('
				     && str[i] != ')'
				     && str[i] != '='
				     && str[i] != '|'
				     && str[i] != '&'
				     ; n++)
					ar[j++] = toupper(str[i++]);
				ar[j++] = 0;
				i--;
				break;
			case AR_FLAG1:
			case AR_FLAG2:
			case AR_FLAG3:
			case AR_FLAG4:
			case AR_EXEMPT:
			case AR_SEX:
			case AR_REST:
				ar[j++] = toupper(str[i]);
				break;
			case AR_SUB:
				for (n = 0; n < (uint)cfg->total_subs; n++)
					if (!strnicmp(str + i, cfg->sub[n]->code, strlen(cfg->sub[n]->code)))
						break;
				if (n < (uint)cfg->total_subs) {
					ar[j++] = n & 0xff;
					ar[j++] = (n >> 8) & 0xff;
				}
				else        /* Unknown sub-board */
					j--;
				while (IS_ALPHA(str[i + 1])) i++;
				break;
			case AR_DIR:
				for (n = 0; n < (uint)cfg->total_dirs; n++)
					if (!strnicmp(str + i, cfg->dir[n]->code, strlen(cfg->dir[n]->code)))
						break;
				if (n < (uint)cfg->total_dirs) {
					ar[j++] = n & 0xff;
					ar[j++] = (n >> 8) & 0xff;
				}
				else        /* Unknown directory */
					j--;
				while (IS_ALPHA(str[i + 1])) i++;
				break;
			case AR_DAY:
				if (toupper(str[i]) == 'S'
				    && toupper(str[i + 1]) == 'U')              /* Sunday */
					ar[j++] = 0;
				else if (toupper(str[i]) == 'M')               /* Monday */
					ar[j++] = 1;
				else if (toupper(str[i]) == 'T'
				         && toupper(str[i + 1]) == 'U')     /* Tuesday */
					ar[j++] = 2;
				else if (toupper(str[i]) == 'W')               /* Wednesday */
					ar[j++] = 3;
				else if (toupper(str[i]) == 'T'
				         && toupper(str[i + 1]) == 'H')     /* Thursday */
					ar[j++] = 4;
				else if (toupper(str[i]) == 'F')               /* Friday */
					ar[j++] = 5;
				else
					ar[j++] = 6;                            /* Saturday */
				while (IS_ALPHA(str[i + 1])) i++;
				break;
			default:    /* Badly formed ARS, digit expected */
				j--;
				break;
		}
	}

	ar[j++] = AR_NULL;
	if (ar_buf == NULL) {
		if ((ar_buf = (uchar *)calloc(j + 4, 1)) == NULL) { /* Padded for ushort dereferencing */
			if (count)
				(*count) = 0;
			return NULL;
		}
	}
	memcpy(ar_buf, ar, j);
	if (count)
		(*count) = j;
	return ar_buf;
}

#ifdef ARS_VERIFY   /* Verification for arstr() */

char *decompile_ars(uchar *ars, int len)
{
	static char buf[1024];
	char *      out;
	uchar *     in;
	uint        n;
	int         equals = 0;
	int         not = 0;

	out = buf;
	buf[0] = 0;
	for (in = ars; in < ars + len; in++) {
		switch (*in) {
			case AR_NULL:
				break;
			case AR_OR:
				*(out++) = '|';

				break;
			case AR_NOT:
				not = 1;
				break;
			case AR_EQUAL:
				equals = 1;
				break;
			case AR_BEGNEST:
				if (not)
					*(out++) = '!';
				not = 0;
				*(out++) = '(';

				break;
			case AR_ENDNEST:
				*(out++) = ')';

				break;
			case AR_LEVEL:
				*(out++) = '$';
				*(out++) = 'L';

				break;
			case AR_AGE:
				*(out++) = '$';
				*(out++) = 'A';

				break;
			case AR_BPS:
				*(out++) = '$';
				*(out++) = 'B';

				break;
			case AR_NODE:
				*(out++) = '$';
				*(out++) = 'N';

				break;
			case AR_TLEFT:
				*(out++) = '$';
				*(out++) = 'R';

				break;
			case AR_TUSED:
				*(out++) = '$';
				*(out++) = 'O';

				break;
			case AR_USER:
			case AR_USERNAME:
				*(out++) = '$';
				*(out++) = 'U';

				break;
			case AR_TIME:
				*(out++) = '$';
				*(out++) = 'T';

				break;
			case AR_PCR:
				*(out++) = '$';
				*(out++) = 'P';

				break;
			case AR_FLAG1:
				*(out++) = '$';
				*(out++) = 'F';
				*(out++) = '1';

				break;
			case AR_FLAG2:
				*(out++) = '$';
				*(out++) = 'F';
				*(out++) = '2';

				break;
			case AR_FLAG3:
				*(out++) = '$';
				*(out++) = 'F';
				*(out++) = '3';

				break;
			case AR_FLAG4:
				*(out++) = '$';
				*(out++) = 'F';
				*(out++) = '4';

				break;
			case AR_EXEMPT:
				*(out++) = '$';
				*(out++) = 'X';

				break;
			case AR_REST:
				*(out++) = '$';
				*(out++) = 'Z';

				break;
			case AR_SEX:
				*(out++) = '$';
				*(out++) = 'S';

				break;
			case AR_UDR:
				*(out++) = '$';
				*(out++) = 'K';

				break;
			case AR_UDFR:
				*(out++) = '$';
				*(out++) = 'D';

				break;
			case AR_EXPIRE:
				*(out++) = '$';
				*(out++) = 'E';

				break;
			case AR_CREDIT:
				*(out++) = '$';
				*(out++) = 'C';

				break;
			case AR_DAY:
				*(out++) = '$';
				*(out++) = 'W';

				break;
			case AR_ANSI:
				if (not)
					*(out++) = '!';
				not = 0;
				*(out++) = '$';
				*(out++) = '[';

				break;
			case AR_RIP:
				if (not)
					*(out++) = '!';
				not = 0;
				*(out++) = '$';
				*(out++) = '*';

				break;
			case AR_LOCAL:
				if (not)
					*(out++) = '!';
				not = 0;
				*(out++) = '$';
				*(out++) = 'G';

				break;
			case AR_GROUP:
				*(out++) = '$';
				*(out++) = 'M';

				break;
			case AR_SUB:
				*(out++) = '$';
				*(out++) = 'H';

				break;
			case AR_LIB:
				*(out++) = '$';
				*(out++) = 'I';

				break;
			case AR_DIR:
				*(out++) = '$';
				*(out++) = 'J';

				break;
			case AR_EXPERT:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "EXPERT");
				out = strchr(out, 0);

				break;
			case AR_SYSOP:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "SYSOP");
				out = strchr(out, 0);

				break;
			case AR_QUIET:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "QUIET");
				out = strchr(out, 0);

				break;
			case AR_MAIN_CMDS:
				*out = 0;
				strcat(out, "MAIN_CMDS");
				out = strchr(out, 0);

				break;
			case AR_FILE_CMDS:
				*out = 0;
				strcat(out, "FILE_CMDS");
				out = strchr(out, 0);

				break;
			case AR_RANDOM:
				*(out++) = '$';
				*(out++) = 'Q';

				break;
			case AR_LASTON:
				*(out++) = '$';
				*(out++) = 'Y';

				break;
			case AR_LOGONS:
				*(out++) = '$';
				*(out++) = 'V';

				break;
			case AR_WIP:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "WIP");
				out = strchr(out, 0);

				break;
			case AR_SUBCODE:
				*out = 0;
				strcat(out, "SUB ");
				out = strchr(out, 0);

				break;
			case AR_DIRCODE:
				*out = 0;
				strcat(out, "DIR ");
				out = strchr(out, 0);

				break;
			case AR_OS2:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "OS2");
				out = strchr(out, 0);

				break;
			case AR_DOS:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "DOS");
				out = strchr(out, 0);

				break;
			case AR_WIN32:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "WIN32");
				out = strchr(out, 0);

				break;
			case AR_UNIX:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "UNIX");
				out = strchr(out, 0);

				break;
			case AR_LINUX:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "LINUX");
				out = strchr(out, 0);

				break;
			case AR_SHELL:
				*out = 0;
				strcat(out, "SHELL ");
				out = strchr(out, 0);

				break;
			case AR_PROT:
				*out = 0;
				strcat(out, "PROT ");
				out = strchr(out, 0);

				break;
			case AR_HOST:
				*out = 0;
				strcat(out, "HOST ");
				out = strchr(out, 0);

				break;
			case AR_IP:
				*out = 0;
				strcat(out, "IP ");
				out = strchr(out, 0);

				break;
			case AR_GUEST:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "GUEST");
				out = strchr(out, 0);

				break;
			case AR_QNODE:
				if (not)
					*(out++) = '!';
				not = 0;
				*out = 0;
				strcat(out, "QNODE");
				out = strchr(out, 0);

				break;
			default:
				printf("Error decoding AR: %02Xh, offset: %u\n", *in, in - ars);
				return "Unknown ARS String";
		}
		switch (*in) {
			case AR_TIME:
				if (not)
					*(out++) = '!';
				if (equals)
					*(out++) = '=';
				not = equals = 0;
				in++;
				n = *((short *)in);
				in++;
				out += sprintf(out, "%02d:%02d", n / 60, n % 60);
				break;
			case AR_AGE:    /* byte operands */
			case AR_PCR:
			case AR_UDR:
			case AR_UDFR:
			case AR_NODE:
			case AR_ROWS:
			case AR_COLS:
			case AR_LEVEL:
			case AR_TLEFT:
			case AR_TUSED:
				if (not)
					*(out++) = '!';
				if (equals)
					*(out++) = '=';
				not = equals = 0;
				in++;
				out += sprintf(out, "%d", *in);
				break;
			case AR_BPS:    /* int operands */
			case AR_MAIN_CMDS:
			case AR_FILE_CMDS:
			case AR_EXPIRE:
			case AR_CREDIT:
			case AR_USER:
			case AR_RANDOM:
			case AR_LASTON:
			case AR_LOGONS:
				if (not)
					*(out++) = '!';
				if (equals)
					*(out++) = '=';
				not = equals = 0;
				in++;
				n = *((short *)in);
				in++;
				out += sprintf(out, "%d", n);
				break;
			case AR_GROUP:
			case AR_LIB:
			case AR_DIR:
			case AR_SUB:
				if (not)
					*(out++) = '!';
				if (equals)
					*(out++) = '=';
				not = equals = 0;
				in++;
				n = *((short *)in);
				n++;              /* convert from to 0 base */
				in++;
				out += sprintf(out, "%d", n);
				break;
			case AR_SUBCODE:
			case AR_DIRCODE:
			case AR_SHELL:
			case AR_PROT:
			case AR_HOST:
			case AR_IP:
			case AR_TERM:
			case AR_LANG:
			case AR_USERNAME:
				if (not)
					*(out++) = '!';
				if (equals)
					*(out++) = '=';
				not = equals = 0;
				in++;
				n = sprintf(out, "%s ", in);
				out += n;
				in += n - 1;
				break;
			case AR_FLAG1:
			case AR_FLAG2:
			case AR_FLAG3:
			case AR_FLAG4:
			case AR_EXEMPT:
			case AR_SEX:
			case AR_REST:
				if (not)
					*(out++) = '!';
				if (equals)
					*(out++) = '=';
				not = equals = 0;
				in++;
				*(out++) = *in;
				break;
		}
	}
	*out = 0;
	return buf;
}

void main(void)
{
	char*  example[] =
	{
		"LEVEL 60"
		, "LEVEL60"
		, "$L60"
		, "60"
		, "NOT LEVEL 60"
		, "LEVEL NOT 60"
		, "LEVEL !60"
		, "$L!60"
		, "!60"
		, "LEVEL EQUAL 60"
		, "LEVEL EQUALS 60"
		, "LEVEL EQUAL TO 60"
		, "LEVEL = 60"
		, "LEVEL=60"
		, "=60"
		, "LEVEL 60 AND FLAG 1A"
		, "LEVEL 60 FLAG 1A"
		, "LEVEL 60 AND FLAG A"
		, "LEVEL 60 AND FLAG1 A"
		, "LEVEL 60 AND FLAG 1 A"
		, "LEVEL 60 AND FLAG1A"
		, "LEVEL 60 & $F A"
		, "$L60 AND $FA"
		, "$L60$FA"
		, "60$FA"
		, "SEX F OR LEVEL 90"
		, "SEX F | LEVEL 90"
		, "SEXF|LEVEL90"
		, "$SF | $L90"
		, "$SF|$L90"
		, "USER NOT EQUAL TO 20"
		, "$U!=20"
		, "BPS 9600 OR NOT TIME 19:00"
		, "BPS 9600 OR NOT TIME 19"
		, "BPS 96 OR NOT TIME 19"
		, "$B 9600 OR NOT $T19"
		, "BPS9600|!TIME19"
		, "$B96|!$T19"
		, "BPS 9600 OR TIME NOT 18:00 OR TIME 21:30"
		, "BPS 9600 OR TIME NOT 18:00 OR 21:30"
		, "$B 9600 OR NOT $T 18 OR 21:30"
		, "$B96|$T!18|21:30"
		, "FLAG A OR FLAG B OR FLAG C OR LEVEL 90"
		, "FLAG A OR B OR C OR LEVEL 90"
		, "FLAG A|B|C OR LEVEL 90"
		, "$FA|B|C|$L90"
		, "USER EQUALS 145 OR LEVEL 90"
		, "USER=145 OR LEVEL 90"
		, "$U=145|$L90"
		, "LEVEL 60 AND FLAG X AND FLAG Y AND FLAG Z"
		, "LEVEL 60 AND FLAG X AND Y AND Z"
		, "LEVEL 60 AND FLAG X Y Z"
		, "LEVEL 60 FLAG XYZ"
		, "LEVEL60 FLAGXYZ"
		, "$L60 $FXYZ"
		, "60$FXYZ"
		, "FLAG 2A OR FLAG 2B OR FLAG 4Z"
		, "FLAG 2A OR B OR FLAG 4Z"
		, "FLAG 2A OR FLAG B OR FLAG 4Z"     /* not the same as above and below */
		, "FLAG2A|B OR FLAG4Z"
		, "$F2A|B|$F4Z"
		, "NOT FLAG 2G"
		, "FLAG NOT 2G"
		, "FLAG 2 NOT G"
		, "!$F2G"
		, "$F!2G"
		, "$F2!G"
		, "BPS 9600 OR (BPS 2400 AND TIME NOT 15:00)"
		, "$B9600|($B2400$T!15)"
		, "(SEX M AND AGE 21) OR (SEX F AND AGE 18)"
		, "($SM$A21)|($SF$A18)"
		, "AGE 21 OR (SEX F AND AGE 18)"
		, "(BPS 2400 AND PCR 20) OR LEVEL 90"
		, "(BPS 2400 AND PCR 20) OR 90"
		, "($B 2400 $P 20) | $L 90"
		, "($B2400$P20)|$L90"
		, "NOT (USER=1 OR USER=20)"
		, "NOT USER=1 AND NOT USER=20"
		, "LEVEL 90 OR (TIME 12:00 AND TIME NOT 18:00)"
		, "(TIME 12:00 AND TIME NOT 18:00) OR LEVEL 90"
		, "(TIME 12:00 AND NOT 18:00) OR LEVEL 90"
		, "($T12!18)|90"
		, "PROT NOTIFY"
		, "PROT NOT IFY"
		, "PROT NOT NOTIFY"
		, "NOT PROT NOTIFY"
		, "HOST NOT SEXY"
		, "HOST !*.YAHOO.COM"
		, "FLAGA SEXY"
		, "FLAGA SEX Y"
		, "FLAGA AND SEX Y"
		, "FLAG SEXY"
		, "FLAG SEX2Y"
		, "FLAG 2 SEXY"
		, "FLAG2SEXY"
		, "FLAG2ASEXY"
		, "IP192.168.1.*"
		, "HOST!=LOCALHOST&IP!=127.0.0.1"
		/* terminator */
		, NULL
	};
	int    i, j;
	uchar* ar;
	ushort cnt;

	for (i = 0; example[i] != NULL; i++) {
		printf("Example   : %s\n", example[i]);
		ar = arstr(&cnt, example[i], NULL);
		printf("Compiled  : ");
		for (j = 0; j < cnt; j++)
			printf("%02X ", ar[j]);
		printf("\n");
		printf("Decompiled: %s\n\n", decompile_ars(ar, cnt));
	}
}

#endif
