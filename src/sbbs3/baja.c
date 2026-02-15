/* Synchronet command shell/module compiler */

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

/* OS-specific */
#ifndef __unix__
	#include <io.h>
	#include <share.h>
#endif

/* ANSI */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>

/* Synchronet-specific */
#include "cmdshell.h"
#include "ars_defs.h"
#include "crc32.h"
#include "genwrap.h"    /* portability wrappers */
#include "dirwrap.h"    /* MAX_PATH */

#ifdef __BORLANDC__
unsigned _stklen = 20000; /* Set stack size in code, not header */
#endif

char **  label_name = NULL
, **goto_file = NULL
, **goto_label = NULL
, **call_file = NULL
, **call_label = NULL;

uint32_t *var_name = NULL, vars = 0;

char **   define_str = NULL
, **define_val = NULL;

char *    linestr = "%s %d: %s\n";
char      tmp[256];

uint *    label_indx = NULL
, *goto_indx = NULL
, *goto_line = NULL
, *call_indx = NULL
, *call_line = NULL;

char  bin_file[MAX_PATH + 1];
char  output_dir[MAX_PATH + 1];
char  include_dir[MAX_PATH + 1];

uint  display = 0, line = 0, labels = 0, gotos = 0, calls = 0, defines = 0, case_sens = 0;
BOOL  pause_on_error = FALSE;

FILE *out = NULL;

void bail(int retval)
{
	if (out)
		fclose(out);

	if (retval != 0) {
		if (bin_file[0] != 0)
			remove(bin_file);
		if (pause_on_error) {
			printf("\nHit enter to contiue...");
			getchar();
		}
	}

	exit(retval);
}

/****************************************************************************/
/* Converts an ASCII Hex string into an ulong								*/
/****************************************************************************/
uint32_t ahtoul(char *str)
{
	uint32_t l, val = 0;

	while ((l = (*str++) | 0x20) != 0x20)
		val = (l & 0xf) + (l >> 6 & 1) * 9 + val * 16;
	return val;
}

/* C Escape char */

uchar cesc(char ch)
{
	switch (ch) {
		case 'e':
			return ESC;
		case 'r':
			return CR;
		case 'n':
			return LF;
		case 't':
			return TAB;
		case 'b':
			return BS;
		case 'a':
			return BEL;
		case 'f':
			return FF;
		case 'v':
			return 11;
		default:
			return ch;
	}
}

int32_t val(char *src, char *p)
{
	static int inside;
	int32_t    l;

	if (IS_DIGIT(*p) || *p == '-')      /* Dec, Hex, or Oct */
		l = strtol(p, &p, 0);
	else if (*p == '\'') {  /* Char */
		p++;
		if (*p == '\\') {
			p++;
			l = cesc(*p);
		}
		else
			l = *p;
		p++;
	}
	else if (*p == '.')    /* Bit */
		l = 1L << strtol(p + 1, &p, 0);
	else {
		printf("!SYNTAX ERROR (expecting integer constant):\n");
		printf(linestr, src, line, *p ? p : "<end of line>");
		bail(1);
		return 0;
	}
	if (inside) {
		return l;
	}
	inside = 1;
	while (*p)
		switch (*(p++)) {
			case '+':
				l += val(src, p);
				break;
			case '-':
				l -= val(src, p);
				break;
			case '*':
				l *= val(src, p);
				break;
			case '/':
				l /= val(src, p);
				break;
			case '%':
				l %= val(src, p);
				break;
			case '&':
				l &= val(src, p);
				break;
			case '|':
				l |= val(src, p);
				break;
			case '~':
				l &= ~val(src, p);
				break;
			case '^':
				l ^= val(src, p);
				break;
			case '>':
				if (*p == '>') {
					p++;
					l >>= val(src, p);
				}
				break;
			case '<':
				if (*p == '<') {
					p++;
					l <<= val(src, p);
				}
				break;
			case ' ':
			case '#':
				inside = 0;
				return l;
		}
	inside = 0;
	return l;
}


void writecstr(char *p)
{
	char str[1024];
	int  j = 0, inquotes = 0;

	while (*p) {
		if (*p == '"') {   /* ignore quotes */
			if (inquotes)
				break;
			inquotes = 1;
			p++;
			continue;
		}
		if (*p == '\\')    { /* escape */
			p++;
			if (IS_DIGIT(*p)) {
				sprintf(tmp, "%.3s", p);
				str[j] = atoi(tmp);       /* decimal, NOT octal */
				if (IS_DIGIT(*(++p)))    /* skip up to 3 digits */
					if (IS_DIGIT(*(++p)))
						p++;
				j++;
				continue;
			}
			switch (*(p++)) {
				case 'x':
					tmp[0] = *(p++);
					tmp[1] = 0;
					if (isxdigit((uchar) * p)) {   /* if another hex digit, skip too */
						tmp[1] = *(p++);
						tmp[2] = 0;
					}
					str[j] = (char)ahtoul(tmp);
					break;
				case 'e':
					str[j] = ESC;
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
					str[j] = *(p - 1);
					break;
			}
			j++;
			continue;
		}
		str[j++] = *(p++);
	}
	str[j] = 0;
	fwrite(str, 1, j + 1, out);
}

void writestr(char *p)
{
	char str[1024];
	int  j = 0;

	while (*p) {
		if (*p == '"') {   /* ignore quotes */
			p++;
			continue;
		}
		if (*p == '\\' && *(p + 1) == '"' && *(p + 2))
			p++;
		str[j++] = *(p++);
	}
	str[j] = 0;
	fwrite(str, 1, j + 1, out);
}

void cvttab(char *str)
{
	int i;

	for (i = 0; str[i]; i++)
		if (str[i] == TAB)
			str[i] = ' ';
}

void newvar(char* src, char *in)
{
	char    name[128];
	int32_t i, l;

	if (IS_DIGIT(*in)) {
		printf("!SYNTAX ERROR (illegal variable name):\n");
		printf(linestr, src, line, (char*)in);
		bail(1);
	}

	sprintf(name, "%.80s", in);
	if (strncmp(name, "var_", 4) == 0)   /* decompiled source? */
		l = strtoul(name + 4, NULL, 16);
	else {
		if (!case_sens)
			strupr(name);
		l = crc32(name, 0);
		for (i = 0; i < vars; i++)
			if (var_name[i] == l)
				break;
		if (i < vars)
			return;
	}
	if ((var_name = (uint32_t *)realloc_or_free(var_name, sizeof(int32_t) * (vars + 1))) == NULL) {
		printf("Too many (%" PRIu32 ") variables!\n", vars);
		bail(1);
	}
	var_name[vars] = l;
	if (display)
		printf("newvar(%08" PRIX32 ")=%s\n", l, in);
	vars++;
}

void writecrc(char *src, char *in)
{
	char    name[128];
	char*   p;
	int32_t l;
	int     i;

	/* Automatically terminate variable name Oct-09-2000 rswindell */
	sprintf(name, "%.80s", in);
	p = strchr(name, ' ');
	if (p)
		*p = 0;

	if (!stricmp(name, "STR") || !name[0])
		l = 0;
	else if (strncmp(name, "var_", 4) == 0)  /* decompiled source? */
		l = strtoul(name + 4, NULL, 16);
	else {
		if (!case_sens)
			strupr(name);
		l = crc32(name, 0);

		for (i = 0; i < vars; i++)
			if (var_name[i] == l)
				break;
		if (i == vars) {
			printf("!SYNTAX ERROR (expecting variable name):\n");
			printf(linestr, src, line, *in ? (char*)in : "<end of line>");
			bail(1);
		}
	}
	fwrite(&l, 4, 1, out);
}

int32_t isvar(char *arg)
{
	char    name[128], *p;
	int32_t i, l;

	if (!arg || !(*arg) || IS_DIGIT(*arg))
		return 0;

	sprintf(name, "%.80s", arg);
	if ((p = strchr(name, ' ')) != NULL)  /* Truncate at first space */
		*p = 0;
	if (strncmp(name, "var_", 4) == 0)   /* decompiled source? */
		return strtoul(name + 4, NULL, 16);
	if (!case_sens)
		strupr(name);
	l = crc32(name, 0);

	for (i = 0; i < vars; i++)
		if (var_name[i] == l)
			break;
	if (i == vars)
		return 0;
	return l;
}

int str_cmp(char *s1, char *s2)
{
	if (case_sens)
		return strcmp(s1, s2);
	return stricmp(s1, s2);
}

void expdefs(char *line)
{
	char str[512], *p, *sp, sav[2] = {0};
	int  i;

	str[0] = 0;
	for (p = line; *p; p++) {
		if (*p == ' ') {
			strcat(str, " ");
			continue;
		}

		if (*p == '"') {               /* Skip quoted text */
			sp = strchr(p + 1, '"');
			if (sp)
				*sp = 0;
			strcat(str, p);
			if (!sp)
				break;
			strcat(str, "\"");
			p += strlen(p);
			continue;
		}

		for (sp = p; *sp; sp++)
			if (!IS_ALPHANUMERIC(*sp) && *sp != '_')
				break;
		sav[0] = *sp;         /* Save delimiter */
		sav[1] = 0;
		*sp = 0;
		for (i = 0; i < defines; i++)
			if (!str_cmp(define_str[i], p))
				break;
		if (i < defines)
			strcat(str, define_val[i]);
		else
			strcat(str, p);
		if (!sav[0])         /* Last argument */
			break;
		p += strlen(p);
		strcat(str, sav);    /* Restore delimiter */
	}
	strcpy(line, str);
}


#define SKIPCTRLSP(p) while (*(p) <= ' ' && *(p) > 0) (p) ++

void compile(char *src)
{
	char *   str, *save, *p, *sp, *tp, *arg, *arg2, *arg3, *arg4, ch;
	uchar *  ar;
	char     path[MAX_PATH + 1];
	uint16_t i;
	uint16_t j;
	int32_t  l;
	int      savline;
	FILE *   in;

	if ((in = fopen(src, "rb")) == NULL) {
		printf("error %d opening %s for read\n", errno, src);
		bail(1);
	}
	line = 0;

	if ((str = malloc(1024)) == NULL) {
		printf("malloc error\n");
		bail(1);
	}

	if ((save = malloc(1024)) == NULL) {
		printf("malloc error\n");
		bail(1);
	}

	while (!feof(in) && !ferror(in)) {
		if (!fgets(str, 1000, in))
			break;
		truncsp(str);
		cvttab(str);
		line++;
		strcpy(save, str);
		p = str;
		SKIPCTRLSP(p);   /* look for beginning of command */
		if ((*p) == 0)
			continue;
		if (*p == '#')             /* remarks start with # */
			continue;
		expdefs(p);             /* expand defines */
		if (display)
			printf("%s\n", p);
		sp = strchr(p, ' ');
		arg = arg2 = arg3 = arg4 = "";
		if (sp) {
			*sp = 0;
			arg = sp + 1;
			SKIPCTRLSP(arg);
			sp = strchr(arg, ' ');
			if (sp) {
				arg2 = sp + 1;
				SKIPCTRLSP(arg2);
				sp = strchr(arg2, ' ');
				if (sp) {
					arg3 = sp + 1;
					SKIPCTRLSP(arg3);
					sp = strchr(arg3, ' ');
					if (sp) {
						arg4 = sp + 1;
						SKIPCTRLSP(arg4);
					}
				}
			}
		}

		if (!stricmp(p, "!INCLUDE")) {
			savline = line;
			sp = strchr(arg, ' ');
			if (sp)
				*sp = 0;
			sprintf(path, "%s%s", include_dir, arg);
			fexistcase(path);
			compile(path);
			line = savline;
			continue;
		}

		if (!stricmp(p, "!DEFINE")) {                     /* define */
			sp = strchr(arg, ' ');
			if (sp)
				*sp = 0;
			else
				break;
			tp = strrchr(arg2, '\"');
			if (!tp)
				tp = arg2;
			sp = strchr(tp, '#');
			if (sp)
				*sp = 0;
			truncsp(arg2);
			if ((define_str = (char **)realloc_or_free(define_str, sizeof(char *) * (defines + 1)))
			    == NULL) {
				printf("Too many defines.\n");
				bail(1);
			}
			if ((define_str[defines] = (char *)malloc(strlen(arg) + 1)) == NULL) {
				printf("Too many defines.\n");
				bail(1);
			}
			if ((define_val = (char **)realloc_or_free(define_val, sizeof(char *) * (defines + 1)))
			    == NULL) {
				printf("Too many defines.\n");
				bail(1);
			}
			if ((define_val[defines] = (char *)malloc(strlen(arg2) + 1)) == NULL) {
				printf("Too many defines.\n");
				bail(1);
			}
			strcpy(define_str[defines], arg);
			strcpy(define_val[defines], arg2);
			defines++;
			continue;
		}

		if (!stricmp(p, "!GLOBAL")) {             /* declare global variables */
			if (!(*arg))
				break;
			for (p = arg; *p && *p != '#';) {
				sp = strchr(p, ' ');
				if (sp)
					*sp = 0;
				newvar(src, p);
				if (!sp)
					break;
				p = sp + 1;
				SKIPCTRLSP(p);
			}
			continue;
		}

		if (!stricmp(p, "PATCH")) {
			if (!(*arg))
				break;
			p = arg;
			while (*p) {
				SKIPCTRLSP(p);
				tmp[0] = *p++;
				tmp[1] = *p++;
				tmp[2] = 0;
				if (!tmp[0])
					break;
				ch = ahtoul(tmp);
				fputc(ch, out);
			}
			continue;
		}

		if (!stricmp(p, "SHOW_VARS")) {
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(SHOW_VARS, out);
			continue;
		}

		if (!stricmp(p, "EMAIL_SECTION")) {
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(EMAIL_SECTION, out);
			continue;
		}

		if (!stricmp(p, "COMPARE_ARS")) {
			if (!(*arg))
				break;
			strupr(arg);
			ar = arstr(&i, arg, NULL, NULL);
			if (ar != NULL) {
				fprintf(out, "%c%c", CS_COMPARE_ARS, (uchar)i);
				fwrite(ar, i, 1, out);
				free(ar);
			}
			continue;
		}

		if (!stricmp(p, "CHKSYSPASS")) {
			fprintf(out, "%c", CS_CHKSYSPASS);
			continue;
		}
		if (!stricmp(p, "INFO_SYSTEM")) {
			fprintf(out, "%c", CS_INFO_SYSTEM);
			continue;
		}
		if (!stricmp(p, "INFO_SUBBOARD")) {
			fprintf(out, "%c", CS_INFO_SUBBOARD);
			continue;
		}
		if (!stricmp(p, "INFO_DIRECTORY")) {
			fprintf(out, "%c", CS_INFO_DIRECTORY);
			continue;
		}
		if (!stricmp(p, "INFO_VERSION")) {
			fprintf(out, "%c", CS_INFO_VERSION);
			continue;
		}
		if (!stricmp(p, "INFO_USER")) {
			fprintf(out, "%c", CS_INFO_USER);
			continue;
		}
		if (!stricmp(p, "INFO_XFER_POLICY")) {
			fprintf(out, "%c", CS_INFO_XFER_POLICY);
			continue;
		}
		if (!stricmp(p, "LOGKEY")) {
			fprintf(out, "%c", CS_LOGKEY);
			continue;
		}
		if (!stricmp(p, "LOGKEY_COMMA")) {
			fprintf(out, "%c", CS_LOGKEY_COMMA);
			continue;
		}
		if (!stricmp(p, "LOGSTR")) {
			fprintf(out, "%c", CS_LOGSTR);
			continue;
		}

		if (!stricmp(p, "ONLINE")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_ONLINE);
			continue;
		}
		if (!stricmp(p, "OFFLINE")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_OFFLINE);
			continue;
		}
		if (!stricmp(p, "NEWUSER")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_NEWUSER);
			continue;
		}
		if (!stricmp(p, "LOGON")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_LOGON);
			continue;
		}
		if (!stricmp(p, "LOGOUT")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_LOGOUT);
			continue;
		}
		if (!stricmp(p, "EXIT")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_EXIT);
			continue;
		}

		if (!stricmp(p, "LOOP") || !stricmp(p, "LOOP_BEGIN")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_LOOP_BEGIN);
			continue;
		}
		if (!stricmp(p, "CONTINUE") || !stricmp(p, "CONTINUE_LOOP")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_CONTINUE_LOOP);
			continue;
		}
		if (!stricmp(p, "BREAK") || !stricmp(p, "BREAK_LOOP")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_BREAK_LOOP);
			continue;
		}
		if (!stricmp(p, "END_LOOP")) {
			fprintf(out, "%c%c", CS_ONE_MORE_BYTE, CS_END_LOOP);
			continue;
		}

		if (!stricmp(p, "USER_EVENT")) {
			if (!(*arg))
				break;
			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(2, out);       /* int offset */
				fputc(1, out);       /* int length */
				ch = 0;
			}                       /* place holder */
			else
				ch = val(src, arg);
			fprintf(out, "%c%c", CS_TWO_MORE_BYTES, CS_USER_EVENT);
			fwrite(&ch, 1, 1, out);
			continue;
		}

		if (!stricmp(p, "PUT_NODE")) {
			fprintf(out, "%c", CS_PUT_NODE);
			continue;
		}
		if (!stricmp(p, "SYNC")) {
			fprintf(out, "%c", CS_SYNC);
			continue;
		}
		if (!stricmp(p, "ASYNC")) {
			fprintf(out, "%c", CS_ASYNC);
			continue;
		}
		if (!stricmp(p, "RIOSYNC")) {     /* deprecated */
			fprintf(out, "%c", CS_SYNC);
			continue;
		}
		if (!stricmp(p, "GETTIMELEFT")) {
			fprintf(out, "%c", CS_GETTIMELEFT);
			continue;
		}
		if (!stricmp(p, "SAVELINE")) {
			fprintf(out, "%c", CS_SAVELINE);
			continue;
		}
		if (!stricmp(p, "RESTORELINE")) {
			fprintf(out, "%c", CS_RESTORELINE);
			continue;
		}
		if (!stricmp(p, "IF_TRUE") || !stricmp(p, "IF_EQUAL")) {
			fprintf(out, "%c", CS_IF_TRUE);
			continue;
		}
		if (!stricmp(p, "IF_FALSE") || !stricmp(p, "IF_NOT_EQUAL")) {
			fprintf(out, "%c", CS_IF_FALSE);
			continue;
		}
		if (!stricmp(p, "IF_GREATER")) {
			fprintf(out, "%c", CS_IF_GREATER);
			continue;
		}
		if (!stricmp(p, "IF_GREATER_OR_EQUAL")
		    || !stricmp(p, "IF_EQUAL_OR_GREATER")) {
			fprintf(out, "%c", CS_IF_GREATER_OR_EQUAL);
			continue;
		}
		if (!stricmp(p, "IF_LESS")) {
			fprintf(out, "%c", CS_IF_LESS);
			continue;
		}
		if (!stricmp(p, "IF_LESS_OR_EQUAL")
		    || !stricmp(p, "IF_EQUAL_OR_LESS")) {
			fprintf(out, "%c", CS_IF_LESS_OR_EQUAL);
			continue;
		}
		if (!stricmp(p, "ENDIF") || !stricmp(p, "END_IF")) {
			fprintf(out, "%c", CS_ENDIF);
			continue;
		}
		if (!stricmp(p, "ELSE")) {
			fprintf(out, "%c", CS_ELSE);
			continue;
		}
		if (p[0] == ':') {                     /* :label */
			p++;
			sp = strchr(p, ' ');
			if (sp)
				*sp = 0;
			for (i = 0; i < labels; i++)
				if (!stricmp(label_name[i], p))
					break;
			if (i < labels) {
				printf("!SYNTAX ERROR (duplicate label name):\n");
				printf(linestr, src, line, p);
				bail(1);
			}
			if ((label_name = (char **)realloc_or_free(label_name, sizeof(char *) * (labels + 1)))
			    == NULL) {
				printf("Too many labels.\n");
				bail(1);
			}
			if ((label_indx = (uint *)realloc_or_free(label_indx, sizeof(int) * (labels + 1)))
			    == NULL) {
				printf("Too many labels.\n");
				bail(1);
			}
			if ((label_name[labels] = (char *)malloc(strlen(p) + 1)) == NULL) {
				printf("Too many labels.\n");
				bail(1);
			}
			strcpy(label_name[labels], p);
			label_indx[labels] = ftell(out);
			labels++;
			continue;
		}
		if (!stricmp(p, "GOTO")) {           /* goto */
			if (!(*arg))
				break;
			sp = strchr(arg, ' ');
			if (sp)
				*sp = 0;
			if ((goto_label = (char **)realloc_or_free(goto_label, sizeof(char *) * (gotos + 1)))
			    == NULL) {
				printf("Too many gotos.\n");
				bail(1);
			}
			if ((goto_file = (char **)realloc_or_free(goto_file, sizeof(char *) * (gotos + 1)))
			    == NULL) {
				printf("Too many gotos.\n");
				bail(1);
			}
			if ((goto_indx = (uint *)realloc_or_free(goto_indx, sizeof(int) * (gotos + 1)))
			    == NULL) {
				printf("Too many gotos.\n");
				bail(1);
			}
			if ((goto_line = (uint *)realloc_or_free(goto_line, sizeof(int) * (gotos + 1)))
			    == NULL) {
				printf("Too many gotos.\n");
				bail(1);
			}
			if ((goto_label[gotos] = (char *)malloc(strlen(arg) + 1)) == NULL) {
				printf("Too many gotos.\n");
				bail(1);
			}
			if ((goto_file[gotos] = (char *)malloc(strlen(str) + 1)) == NULL) {
				printf("Too many gotos.\n");
				bail(1);
			}
			strcpy(goto_label[gotos], arg);
			strcpy(goto_file[gotos], str);
			goto_indx[gotos] = ftell(out);
			goto_line[gotos] = line;
			gotos++;
			fprintf(out, "%c%c%c", CS_GOTO, 0xff, 0xff);
			continue;
		}
		if (!stricmp(p, "CALL")) {          /* call */
			if (!(*arg))
				break;
			sp = strchr(arg, ' ');
			if (sp)
				*sp = 0;
			if ((call_label = (char **)realloc_or_free(call_label, sizeof(char *) * (calls + 1)))
			    == NULL) {
				printf("Too many calls.\n");
				bail(1);
			}
			if ((call_file = (char **)realloc_or_free(call_file, sizeof(char *) * (calls + 1)))
			    == NULL) {
				printf("Too many calls.\n");
				bail(1);
			}
			if ((call_indx = (uint *)realloc_or_free(call_indx, sizeof(int) * (calls + 1)))
			    == NULL) {
				printf("Too many calls.\n");
				bail(1);
			}
			if ((call_line = (uint *)realloc_or_free(call_line, sizeof(int) * (calls + 1)))
			    == NULL) {
				printf("Too many calls.\n");
				bail(1);
			}
			if ((call_label[calls] = (char *)malloc(strlen(arg) + 1)) == NULL) {
				printf("Too many calls.\n");
				bail(1);
			}
			if ((call_file[calls] = (char *)malloc(strlen(src) + 1)) == NULL) {
				printf("Too many calls.\n");
				bail(1);
			}

			strcpy(call_label[calls], arg);
			strcpy(call_file[calls], src);
			call_indx[calls] = ftell(out);
			call_line[calls] = line;
			calls++;
			fprintf(out, "%c%c%c", CS_CALL, 0xff, 0xff);
			continue;
		}

		if (!stricmp(p, "RETURN")) {
			fprintf(out, "%c", CS_RETURN);
			continue;
		}
		if (!stricmp(p, "CMD_HOME")) {
			fprintf(out, "%c", CS_CMD_HOME);
			continue;
		}
		if (!stricmp(p, "CMDKEY")) {
			if (!(*arg))
				break;
			if (!stricmp(arg, "DIGIT"))
				ch = CS_DIGIT;
			else if (!stricmp(arg, "EDIGIT"))
				ch = CS_EDIGIT;
			else
				ch = toupper(*arg);
			if (ch == '/')
				ch = *(arg + 1) | 0x80; /* high bit indicates slash required */
			else if (ch == '^' && (*(arg + 1) >= 0x40))
				ch = *(arg + 1) - 0x40; /* ctrl char */
			else if (ch == '\\')
				ch = cesc(*(arg + 1));
			else if (ch == '\'')
				ch = *(arg + 1);
			fprintf(out, "%c%c", CS_CMDKEY, ch);
			continue;
		}
		if (!stricmp(p, "CMDCHAR")) {
			if (!(*arg))
				break;
			fprintf(out, "%c%c", CS_CMDKEY, *arg);
			continue;
		}
		if (!stricmp(p, "SETLOGIC") || !stricmp(p, "SET_LOGIC")) {
			if (!(*arg))
				break;
			if (!stricmp(arg, "TRUE") || !stricmp(arg, "EQUAL"))
				ch = LOGIC_TRUE;
			else if (!stricmp(arg, "GREATER"))
				ch = LOGIC_GREATER;
			else if (!stricmp(arg, "LESS"))
				ch = LOGIC_LESS;
			else
				ch = LOGIC_FALSE;
			fprintf(out, "%c%c", CS_SETLOGIC, ch);
			continue;
		}

		if (!stricmp(p, "DEFINE_STR_VAR") || !stricmp(p, "STR")) {
			if (!(*arg))
				break;
			for (p = arg; *p && *p != '#';) {
				sp = strchr(p, ' ');
				if (sp)
					*sp = 0;
				fputc(CS_VAR_INSTRUCTION, out);
				fputc(DEFINE_STR_VAR, out);
				newvar(src, p);
				writecrc(src, p);
				if (!sp)
					break;
				p = sp + 1;
				SKIPCTRLSP(p);
			}
			continue;
		}
		if (!stricmp(p, "DEFINE_INT_VAR") || !stricmp(p, "INT")) {
			if (!(*arg))
				break;
			for (p = arg; *p && *p != '#';) {
				sp = strchr(p, ' ');
				if (sp)
					*sp = 0;
				fputc(CS_VAR_INSTRUCTION, out);
				fputc(DEFINE_INT_VAR, out);
				newvar(src, p);
				writecrc(src, p);
				if (!sp)
					break;
				p = sp + 1;
				SKIPCTRLSP(p);
			}
			continue;
		}
		if (!stricmp(p, "DEFINE_GLOBAL_STR_VAR") || !stricmp(p, "GLOBAL_STR")) {
			if (!(*arg))
				break;
			for (p = arg; *p && *p != '#';) {
				sp = strchr(p, ' ');
				if (sp)
					*sp = 0;
				fputc(CS_VAR_INSTRUCTION, out);
				fputc(DEFINE_GLOBAL_STR_VAR, out);
				newvar(src, p);
				writecrc(src, p);
				if (!sp)
					break;
				p = sp + 1;
				SKIPCTRLSP(p);
			}
			continue;
		}
		if (!stricmp(p, "DEFINE_GLOBAL_INT_VAR") || !stricmp(p, "GLOBAL_INT")) {
			if (!(*arg))
				break;
			for (p = arg; *p && *p != '#';) {
				sp = strchr(p, ' ');
				if (sp)
					*sp = 0;
				fputc(CS_VAR_INSTRUCTION, out);
				fputc(DEFINE_GLOBAL_INT_VAR, out);
				newvar(src, p);
				writecrc(src, p);
				if (!sp)
					break;
				p = sp + 1;
				SKIPCTRLSP(p);
			}
			continue;
		}

		if (!stricmp(p, "LOGIN")) {
			if (!(*arg))
				break;
			fputc(CS_STR_FUNCTION, out);
			fputc(CS_LOGIN, out);
			writecstr(arg);
			continue;
		}

		if (!stricmp(p, "LOAD_TEXT")) {
			if (!(*arg))
				break;
			fputc(CS_STR_FUNCTION, out);
			fputc(CS_LOAD_TEXT, out);
			writestr(arg);
			continue;
		}

		if (!stricmp(p, "SET_STR_VAR")
		    || (!stricmp(p, "SET") && strchr(arg, '"'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(SET_STR_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecstr(arg2);
			continue;
		}
		if (!stricmp(p, "CAT_STR_VAR")
		    || (!stricmp(p, "STRCAT") && strchr(arg, '"'))) {
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(CAT_STR_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecstr(arg2);
			continue;
		}
		if ((!stricmp(p, "STRSTR") || !stricmp(p, "COMPARE_SUBSTR"))
		    && strchr(arg, '"')) {
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(STRSTR_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecstr(arg2);
			continue;
		}
		if (!stricmp(p, "STRSTR") || !stricmp(p, "COMPARE_SUBSTR")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(STRSTR_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}
		if (!stricmp(p, "COPY_CHAR") || !stricmp(p, "COPY_KEY")) {
			if (!(*arg))
				break;
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, COPY_CHAR);
			writecrc(src, arg);
			continue;
		}
		if (!stricmp(p, "COPY_FIRST_CHAR")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(COPY_FIRST_CHAR, out);
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}
		if (!stricmp(p, "COMPARE_FIRST_CHAR")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(COMPARE_FIRST_CHAR, out);
			writecrc(src, arg);
			fputc((uchar)val(src, arg2), out);
			continue;
		}
		if (!stricmp(p, "CAT_STR_VARS") || !stricmp(p, "STRCAT")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(CAT_STR_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}
		if (!stricmp(p, "FORMAT") || !stricmp(p, "SPRINTF")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(FORMAT_STR_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);                  /* Write destination variable */
			p++;
			SKIPCTRLSP(p);
			arg = p;
			p = strrchr(arg, '"');
			if (!p)
				break;
			*p = 0;
			p++;
			SKIPCTRLSP(p);
			writecstr(arg);                 /* Write string */
			l = ftell(out);
			fputc(0, out);                   /* Write total number of args */
			i = 0;
			while (p && *p) {
				arg = p;
				p = strchr(arg, ' ');
				if (p) {
					*p = 0;
					p++;
				}
				writecrc(src, arg);
				i++;
			}
			fseek(out, l, SEEK_SET);
			fputc((char)i, out);
			fseek(out, i * 4, SEEK_CUR);
			continue;
		}

		if (!stricmp(p, "STRFTIME") || !stricmp(p, "FTIME_STR")) {
			if (!(*arg) || !(*arg2) || !(*arg3))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(FORMAT_TIME_STR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);                  /* Write destination variable */
			p++;
			SKIPCTRLSP(p);
			arg = p;
			p = strrchr(arg, '"');
			if (!p)
				break;
			*p = 0;
			writecstr(arg);                 /* Write string */
			p++;
			SKIPCTRLSP(p);
			writecrc(src, p);
			continue;
		}

		if (!stricmp(p, "TIME_STR")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(TIME_STR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "DATE_STR")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(DATE_STR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "SECOND_STR")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(SECOND_STR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}


		if (!stricmp(p, "SET_INT_VAR")
		    || (!stricmp(p, "SET") && *arg2 != '"')) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(SET_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			fwrite(&l, 4, 1, out);
			continue;
		}

		if (!stricmp(p, "COMPARE_STR_VAR") ||
		    (!stricmp(p, "COMPARE") && *arg2 == '"')) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(COMPARE_STR_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecstr(arg2);
			continue;
		}

		if (!stricmp(p, "COMPARE_STRN_VAR") ||
		    ((!stricmp(p, "STRNCMP") || !stricmp(p, "COMPARE_STRN"))
		     && *arg3 && strchr(arg3, '"'))) {
			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(2, out);       /* int offset */
				fputc(1, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg);
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(STRNCMP_VAR, out);
			fwrite(&i, 1, 1, out); /* Length */
			p = strchr(arg2, ' ');
			if (!p)
				break;
			*p = 0;
			p++;
			SKIPCTRLSP(p);
			writecrc(src, arg2);
			writecstr(p);
			continue;
		}

		if (!stricmp(p, "COMPARE_STRN_VARS") || !stricmp(p, "STRNCMP")
		    || !stricmp(p, "COMPARE_STRN")) {
			if (!(*arg) || !(*arg2) || !(*arg3))
				break;
			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(2, out);       /* int offset */
				fputc(1, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg);
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(STRNCMP_VARS, out);

			fwrite(&i, 1, 1, out); /* Length */
			p = strchr(arg2, ' ');
			if (!p)
				break;
			*p = 0;
			p++;
			SKIPCTRLSP(p);
			writecrc(src, arg2);
			writecrc(src, p);
			continue;
		}

		if (!stricmp(p, "COMPARE_INT_VAR") ||
		    (!stricmp(p, "COMPARE")
		     && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;

			fputc(CS_VAR_INSTRUCTION, out);
			fputc(COMPARE_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			fwrite(&l, 4, 1, out);
			continue;
		}

		if (!stricmp(p, "COMPARE")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(COMPARE_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}
		if (!stricmp(p, "COPY")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(COPY_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}
		if (!stricmp(p, "SWAP")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(SWAP_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "TIME")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(TIME_INT_VAR, out);
			writecrc(src, arg);
			continue;
		}

		if (!stricmp(p, "DATE_INT")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(DATE_STR_TO_INT, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "CRC16")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(CRC16_TO_INT, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "CRC32")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(CRC32_TO_INT, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "CHKSUM")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(CHKSUM_TO_INT, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "ADD_INT_VAR")
		    || (!stricmp(p, "ADD")
		        && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(ADD_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			if (!l)
				break;
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "ADD_INT_VARS") || !stricmp(p, "ADD")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(ADD_INT_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "SUB_INT_VAR")
		    || (!stricmp(p, "SUB")
		        && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(SUB_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			if (!l)
				break;
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "SUB_INT_VARS") || !stricmp(p, "SUB")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(SUB_INT_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "MUL_INT_VAR")
		    || (!stricmp(p, "MUL")
		        && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(MUL_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			if (!l)
				break;
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "MUL_INT_VARS") || !stricmp(p, "MUL")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(MUL_INT_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "DIV_INT_VAR")
		    || (!stricmp(p, "DIV")
		        && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(DIV_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			if (!l)
				break;
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "DIV_INT_VARS") || !stricmp(p, "DIV")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(DIV_INT_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "MOD_INT_VAR")
		    || (!stricmp(p, "MOD")
		        && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(MOD_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			if (!l)
				break;
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "MOD_INT_VARS") || !stricmp(p, "MOD")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(MOD_INT_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "AND_INT_VAR")
		    || (!stricmp(p, "AND")
		        && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(AND_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "AND_INT_VARS") || !stricmp(p, "AND")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(AND_INT_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "COMPARE_ANY_BITS") || !stricmp(p, "COMPARE_ALL_BITS")) {
			if (!(*arg) || !(*arg2))
				break;
			if ((l = isvar(arg2)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(6, out);       /* int offset */
				fputc(4, out);       /* int length */
				l = 0;
			}                       /* place holder */
			else
				l = val(src, arg2);
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION
			        , !stricmp(p, "COMPARE_ANY_BITS") ? COMPARE_ANY_BITS : COMPARE_ALL_BITS);
			writecrc(src, arg);
			fwrite(&l, sizeof(l), 1, out);
			continue;
		}

		if (!stricmp(p, "OR_INT_VAR")
		    || (!stricmp(p, "OR")
		        && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(OR_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "OR_INT_VARS") || !stricmp(p, "OR")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(OR_INT_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "NOT_INT_VAR")
		    || (!stricmp(p, "NOT")
		        && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(NOT_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "NOT_INT_VARS") || !stricmp(p, "NOT")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(NOT_INT_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "XOR_INT_VAR")
		    || (!stricmp(p, "XOR")
		        && (IS_DIGIT(*arg2) || atol(arg2) || *arg2 == '\'' || *arg2 == '.'))) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(XOR_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			l = val(src, arg2);
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "XOR_INT_VARS") || !stricmp(p, "XOR")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(XOR_INT_VARS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "RANDOM_INT_VAR") || !stricmp(p, "RANDOM")) {
			if (!(*arg))
				break;
			if ((l = isvar(arg2)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(6, out);       /* int offset */
				fputc(4, out);       /* int length */
				l = 0;
			}                       /* place holder */
			else
				l = val(src, arg2);
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(RANDOM_INT_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			fwrite(&l, 4, 1, out);
			continue;
		}

		if (!stricmp(p, "SWITCH")) {
			if (!(*arg))
				break;
			fputc(CS_SWITCH, out);
			writecrc(src, arg);
			continue;
		}
		if (!stricmp(p, "END_SWITCH")) {
			fputc(CS_END_SWITCH, out);
			continue;
		}
		if (!stricmp(p, "CASE")) {
			if (!(*arg))
				break;
			fputc(CS_CASE, out);
			l = val(src, arg);
			fwrite(&l, 4, 1, out);
			continue;
		}
		if (!stricmp(p, "DEFAULT")) {
			fputc(CS_DEFAULT, out);
			continue;
		}
		if (!stricmp(p, "END_CASE")) {
			fputc(CS_END_CASE, out);
			continue;
		}

		if (!stricmp(p, "PRINT") && !strchr(arg, '"') && !strchr(arg, '\\')
		    && !strchr(arg, ' ')) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			fputc(PRINT_VAR, out);
			writecrc(src, arg);
			continue;
		}

		if (!stricmp(p, "PRINTF") || !stricmp(p, "LPRINTF") || !stricmp(p, "PRINTF_LOCAL")) {
			if (!(*arg))
				break;
			fputc(CS_VAR_INSTRUCTION, out);
			if (!stricmp(p, "PRINTF"))
				fputc(VAR_PRINTF, out);
			else
				fputc(VAR_PRINTF_LOCAL, out);
			p = strrchr(arg, '"');
			if (!p)
				break;
			*p = 0;
			p++;
			SKIPCTRLSP(p);
			writecstr(arg);                 /* Write string */
			l = ftell(out);
			fputc(0, out);                   /* Write total number of args */
			i = 0;
			while (p && *p) {
				arg = p;
				p = strchr(arg, ' ');
				if (p) {
					*p = 0;
					p++;
				}
				writecrc(src, arg);
				i++;
			}
			fseek(out, l, SEEK_SET);
			fputc((char)i, out);
			fseek(out, i * 4, SEEK_CUR);
			continue;
		}

		if (!stricmp(p, "FOPEN")) {
			if (!(*arg) || !(*arg2) || !(*arg3))
				break;
			if ((l = isvar(arg2)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(6, out);       /* int offset */
				fputc(2, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg2);

			fputc(CS_FIO_FUNCTION, out);
			if (*arg3 == '"')
				fputc(FIO_OPEN, out);
			else
				fputc(FIO_OPEN_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			p = strchr(arg2, ' ');
			if (!p)
				break;
			*p = 0;
			p++;
			fwrite(&i, 2, 1, out);
			SKIPCTRLSP(p);
			if (*p == '"')
				writestr(p);
			else
				writecrc(src, p);
			continue;
		}
		if (!stricmp(p, "FCLOSE")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_CLOSE, out);
			writecrc(src, arg);
			continue;
		}
		if (!stricmp(p, "FFLUSH")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_FLUSH, out);
			writecrc(src, arg);
			continue;
		}
		if (!stricmp(p, "FREAD")) {
			if (!(*arg))
				break;

			fputc(CS_FIO_FUNCTION, out);
			if (!(*arg3) || IS_DIGIT(*arg3) || atoi(arg3))
				fputc(FIO_READ, out);
			else
				fputc(FIO_READ_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			p = strchr(arg2, ' ');
			if (p)
				*p = 0;
			writecrc(src, arg2);         /* Variable */
			if (IS_DIGIT(*arg3))
				i = val(src, arg3);  /* Length */
			else
				i = 0;
			if (i || !(*arg3))
				fwrite(&i, 2, 1, out);
			else
				writecrc(src, arg3);
			continue;
		}
		if (!stricmp(p, "FWRITE")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			if (!(*arg3) || IS_DIGIT(*arg3) || atoi(arg3))
				fputc(FIO_WRITE, out);
			else
				fputc(FIO_WRITE_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			p = strchr(arg2, ' ');
			if (p)
				*p = 0;
			writecrc(src, arg2);         /* Variable */
			if (IS_DIGIT(*arg3))
				i = val(src, arg3);  /* Length */
			else
				i = 0;
			if (i || !(*arg3))
				fwrite(&i, 2, 1, out);
			else
				writecrc(src, arg3);
			continue;
		}
		if (!stricmp(p, "FGET_LENGTH")
		    || !stricmp(p, "FGETLENGTH")
		    || !stricmp(p, "GETFLENGTH")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_GET_LENGTH, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			writecrc(src, arg2);         /* Variable */
			continue;
		}
		if (!stricmp(p, "FREAD_LINE")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_READ_LINE, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			writecrc(src, arg2);         /* Variable */
			continue;
		}
		if (!stricmp(p, "FEOF")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_EOF, out);
			writecrc(src, arg);
			continue;
		}
		if (!stricmp(p, "FGET_POS")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_GET_POS, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			writecrc(src, arg2);         /* Variable */
			continue;
		}
		if (!stricmp(p, "FSET_POS") || !stricmp(p, "FSEEK")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			if (IS_DIGIT(*arg2) || atol(arg2))
				fputc(FIO_SEEK, out);
			else
				fputc(FIO_SEEK_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			p = strchr(arg2, ' ');
			if (p)
				*p = 0;
			if (atol(arg2) || IS_DIGIT(*arg2)) {
				l = val(src, arg2);
				fwrite(&l, 4, 1, out);
			}
			else
				writecrc(src, arg2);        /* Offset variable */
			i = 0;
			if (p) {
				p++;
				SKIPCTRLSP(p);
				i = atoi(p);
				if (!stricmp(p, "CUR"))
					i = SEEK_CUR;
				else if (!stricmp(p, "END"))
					i = SEEK_END;
			}
			fwrite(&i, 2, 1, out);
			continue;
		}
		if (!stricmp(p, "FLOCK")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			if (IS_DIGIT(*arg2) || atol(arg2))
				fputc(FIO_LOCK, out);
			else
				fputc(FIO_LOCK_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			if (atol(arg2) || IS_DIGIT(*arg2)) {
				l = val(src, arg2);
				if (!l)
					break;
				fwrite(&l, 4, 1, out);
			}
			else
				writecrc(src, arg2);    /* Length variable */
			continue;
		}
		if (!stricmp(p, "FUNLOCK")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			if (IS_DIGIT(*arg2) || atol(arg2))
				fputc(FIO_UNLOCK, out);
			else
				fputc(FIO_UNLOCK_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			if (atol(arg2) || IS_DIGIT(*arg2)) {
				l = val(src, arg2);
				if (!l)
					break;
				fwrite(&l, 4, 1, out);
			}
			else
				writecrc(src, arg2);        /* Length variable */
			continue;
		}
		if (!stricmp(p, "FSET_LENGTH")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			if (IS_DIGIT(*arg2) || atol(arg2))
				fputc(FIO_SET_LENGTH, out);
			else
				fputc(FIO_SET_LENGTH_VAR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			if (atol(arg2) || IS_DIGIT(*arg2)) {
				l = val(src, arg2);
				fwrite(&l, 4, 1, out);
			}
			else
				writecrc(src, arg2);        /* Length variable */
			continue;
		}
		if (!stricmp(p, "FPRINTF")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_PRINTF, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);                  /* Write destination variable */
			p++;
			SKIPCTRLSP(p);
			arg = p;
			p = strrchr(arg, '"');
			if (!p)
				break;
			*p = 0;
			p++;
			SKIPCTRLSP(p);
			writecstr(arg);                 /* Write string */
			l = ftell(out);
			fputc(0, out);                   /* Write total number of args */
			i = 0;
			while (p && *p) {
				arg = p;
				p = strchr(arg, ' ');
				if (p) {
					*p = 0;
					p++;
				}
				writecrc(src, arg);
				i++;
			}
			fseek(out, l, SEEK_SET);
			fputc((char)i, out);
			fseek(out, i * 4, SEEK_CUR);
			continue;
		}
		if (!stricmp(p, "FSET_ETX")) {
			if (!(*arg))
				break;
			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(2, out);       /* int offset */
				fputc(1, out);       /* int length */
				ch = 0;
			}                       /* place holder */
			else
				ch = val(src, arg);

			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_SET_ETX, out);
			fwrite(&ch, 1, 1, out);
			continue;
		}
		if (!stricmp(p, "FGET_TIME")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_GET_TIME, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			writecrc(src, arg2);         /* Variable */
			continue;
		}
		if (!stricmp(p, "FSET_TIME")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(FIO_SET_TIME, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* File handle */
			writecrc(src, arg2);         /* Variable */
			continue;
		}
		if (!stricmp(p, "REMOVE_FILE")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(REMOVE_FILE, out);
			writecrc(src, arg);          /* Str var */
			continue;
		}
		if (!stricmp(p, "RENAME_FILE")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(RENAME_FILE, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* str var */
			writecrc(src, arg2);         /* str var */
			continue;
		}
		if (!stricmp(p, "COPY_FILE")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(COPY_FILE, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* str var */
			writecrc(src, arg2);         /* str var */
			continue;
		}
		if (!stricmp(p, "MOVE_FILE")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(MOVE_FILE, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* str var */
			writecrc(src, arg2);         /* str var */
			continue;
		}
		if (!stricmp(p, "GET_FILE_ATTRIB")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(GET_FILE_ATTRIB, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* str var */
			writecrc(src, arg2);         /* int var */
			continue;
		}
		if (!stricmp(p, "SET_FILE_ATTRIB")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(SET_FILE_ATTRIB, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* str var */
			writecrc(src, arg2);         /* int var */
			continue;
		}
		if (!stricmp(p, "RMDIR") || !stricmp(p, "REMOVE_DIR")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(REMOVE_DIR, out);
			writecrc(src, arg);          /* Str var */
			continue;
		}
		if (!stricmp(p, "MKDIR") || !stricmp(p, "MAKE_DIR")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(MAKE_DIR, out);
			writecrc(src, arg);          /* Str var */
			continue;
		}
		if (!stricmp(p, "CHDIR") || !stricmp(p, "CHANGE_DIR")) {
			if (!(*arg))
				break;
			printf("!WARNING: CHANGE_DIR deprecated in Synchronet v3+\n");
			printf(linestr, src, line, save);
			fputc(CS_FIO_FUNCTION, out);
			fputc(CHANGE_DIR, out);
			writecrc(src, arg);          /* Str var */
			continue;
		}
		if (!stricmp(p, "OPEN_DIR")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(OPEN_DIR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* int var */
			writecrc(src, arg2);         /* str var */
			continue;
		}
		if (!stricmp(p, "READ_DIR")) {
			if (!(*arg) || !(*arg2))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(READ_DIR, out);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);          /* int var */
			writecrc(src, arg2);         /* str var */
			continue;
		}
		if (!stricmp(p, "REWIND_DIR")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(REWIND_DIR, out);
			writecrc(src, arg);          /* int var */
			continue;
		}
		if (!stricmp(p, "CLOSE_DIR")) {
			if (!(*arg))
				break;
			fputc(CS_FIO_FUNCTION, out);
			fputc(CLOSE_DIR, out);
			writecrc(src, arg);          /* int var */
			continue;
		}

		/* NET_FUNCTIONS */

		if (!stricmp(p, "SOCKET_OPEN")) {
			if (!(*arg))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_SOCKET_OPEN);
			writecrc(src, arg);          /* int var (socket) */
			continue;
		}
		if (!stricmp(p, "SOCKET_CLOSE")) {
			if (!(*arg))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_SOCKET_CLOSE);
			writecrc(src, arg);          /* int var (socket) */
			continue;
		}
		if (!stricmp(p, "SOCKET_CHECK")) {
			if (!(*arg))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_SOCKET_CHECK);
			writecrc(src, arg);          /* int var (socket) */
			continue;
		}
		if (!stricmp(p, "SOCKET_CONNECT")) {
			if (!(*arg) || !(*arg2) || !(*arg3))
				break;

			/* TCP port */
			if ((l = isvar(arg3)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(10, out);      /* int offset */
				fputc(2, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg3);

			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_SOCKET_CONNECT);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* str var (address) */
			fwrite(&i, 2, 1, out);
			continue;
		}
		if (!stricmp(p, "SOCKET_ACCEPT")) {
			if (!(*arg))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_SOCKET_ACCEPT);
			writecrc(src, arg);          /* int var (socket) */
			continue;
		}
		if (!stricmp(p, "SOCKET_NREAD")) {
			if (!(*arg) || !(*arg2))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_SOCKET_NREAD);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* int var (nbytes) */
			continue;
		}
		if (!stricmp(p, "SOCKET_READ")
		    || !stricmp(p, "SOCKET_READLINE")
		    || !stricmp(p, "SOCKET_PEEK")) {
			if (!(*arg) || !(*arg2))
				break;

			/* length */
			if (!(*arg3))
				i = 0;
			else if ((l = isvar(arg3)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(10, out);      /* int offset */
				fputc(2, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg3);

			if (!stricmp(p, "SOCKET_READ"))
				ch = CS_SOCKET_READ;
			else if (!stricmp(p, "SOCKET_READLINE"))
				ch = CS_SOCKET_READLINE;
			else
				ch = CS_SOCKET_PEEK;
			fprintf(out, "%c%c", CS_NET_FUNCTION, ch);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* str var (buffer) */
			fwrite(&i, sizeof(i), 1, out); /* word (length) */
			continue;
		}
		if (!stricmp(p, "SOCKET_WRITE")) {
			if (!(*arg) || !(*arg2))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_SOCKET_WRITE);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* str var (buffer) */
			continue;
		}

		/* FTP functions */
		if (!stricmp(p, "FTP_LOGIN")) {
			if (!(*arg) || !(*arg2) || !(*arg3))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_FTP_LOGIN);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* int var (user) */
			writecrc(src, arg3);         /* int var (password) */
			continue;
		}
		if (!stricmp(p, "FTP_LOGOUT")) {
			if (!(*arg))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_FTP_LOGOUT);
			writecrc(src, arg);          /* int var (socket) */
			continue;
		}
		if (!stricmp(p, "FTP_PWD")) {
			if (!(*arg))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_FTP_PWD);
			writecrc(src, arg);          /* int var (socket) */
			continue;
		}
		if (!stricmp(p, "FTP_CWD")) {
			if (!(*arg) || !(*arg2))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_FTP_CWD);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* str var (path)	*/
			continue;
		}
		if (!stricmp(p, "FTP_DIR")) {
			if (!(*arg) || !(*arg2))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_FTP_DIR);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* str var (path)	*/
			continue;
		}
		if (!stricmp(p, "FTP_GET")) {
			if (!(*arg) || !(*arg2) || !(*arg3))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_FTP_GET);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* str var (src path) */
			writecrc(src, arg3);         /* str var (dest path) */
			continue;
		}
		if (!stricmp(p, "FTP_PUT")) {
			if (!(*arg) || !(*arg2) || !(*arg3))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_FTP_PUT);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* str var (src path) */
			writecrc(src, arg3);         /* str var (dest path) */
			continue;
		}
		if (!stricmp(p, "FTP_DELETE")) {
			if (!(*arg) || !(*arg2))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_FTP_DELETE);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* str var (path)	*/
			continue;
		}
		if (!stricmp(p, "FTP_RENAME")) {
			if (!(*arg) || !(*arg2) || !(*arg3))
				break;
			fprintf(out, "%c%c", CS_NET_FUNCTION, CS_FTP_RENAME);
			writecrc(src, arg);          /* int var (socket) */
			writecrc(src, arg2);         /* str var (org name) */
			writecrc(src, arg3);         /* str var (new name) */
			continue;
		}

		if (!stricmp(p, "NODE_ACTION")) {
			if (!(*arg))
				break;
			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(1, out);       /* int length */
				ch = 0;
			}                       /* place holder */
			else
				ch = val(src, arg);

			fprintf(out, "%c%c", CS_NODE_ACTION, ch);
			continue;
		}
		if (!stricmp(p, "NODE_STATUS")) {
			if (!(*arg))
				break;
			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(1, out);       /* int length */
				ch = 0;
			}                       /* place holder */
			else
				ch = val(src, arg);

			fprintf(out, "%c%c", CS_NODE_STATUS, ch);
			continue;
		}
		if (!stricmp(p, "END_CMD") || !stricmp(p, "ENDCMD")) {
			fprintf(out, "%c", CS_END_CMD);
			continue;
		}
		if (!stricmp(p, "CMD_POP") || !stricmp(p, "CMDPOP")) {
			fprintf(out, "%c", CS_CMD_POP);
			continue;
		}
		if (!stricmp(p, "CLS")) {
			fprintf(out, "%c", CS_CLS);
			continue;
		}
		if (!stricmp(p, "CRLF")) {
			fprintf(out, "%c", CS_CRLF);
			continue;
		}
		if (!stricmp(p, "PAUSE")) {
			fprintf(out, "%c", CS_PAUSE);
			continue;
		}
		if (!stricmp(p, "PAUSE_RESET")) {
			fprintf(out, "%c", CS_PAUSE_RESET);
			continue;
		}
		if (!stricmp(p, "CLEAR_ABORT")) {
			fprintf(out, "%c", CS_CLEAR_ABORT);
			continue;
		}
		if (!stricmp(p, "GETLINES")) {
			fprintf(out, "%c", CS_GETLINES);
			continue;
		}
		if (!stricmp(p, "GETFILESPEC")) {
			fprintf(out, "%c", CS_GETFILESPEC);
			continue;
		}
		if (!stricmp(p, "FINDUSER")) {
			fprintf(out, "%c", CS_FINDUSER);
			continue;
		}
		if (!stricmp(p, "MATCHUSER")) {
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, MATCHUSER);
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}

		if (!stricmp(p, "LOG")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_LOG);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "MNEMONICS")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_MNEMONICS);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "PRINT")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_PRINT);
			if (strstr(arg, "%s") != NULL) {
				printf("!WARNING: PRINT \"%%s\" is a security hole if STR contains unvalidated input\n");
				printf(linestr, src, line, save);
			}
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "PRINT_LOCAL")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_PRINT_LOCAL);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "PRINT_REMOTE")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_PRINT_REMOTE);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "PRINTFILE")) {
			if (!(*arg))
				break;
			if (*arg == '"') { /* NEED TO SUPPORT MODE HERE */
				fprintf(out, "%c", CS_PRINTFILE);
				writestr(arg);
			}
			else {
				if ((l = isvar(arg2)) != 0) {
					fputc(CS_USE_INT_VAR, out);
					fwrite(&l, 4, 1, out); /* variable */
					fputc(6, out);       /* int offset */
					fputc(2, out);       /* int length */
					i = 0;
				}                       /* place holder */
				else
					i = val(src, arg2);

				fprintf(out, "%c%c", CS_VAR_INSTRUCTION, PRINTFILE_VAR_MODE);
				p = strchr(arg, ' ');
				if (p)
					*p = 0;
				writecrc(src, arg);
				fwrite(&i, 2, 1, out);
			}
			continue;
		}
		if (!stricmp(p, "PRINTTAIL")) {
			if (!(*arg) || !(*arg2))
				break;
			if ((l = isvar(arg3)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(8, out);       /* int offset */
				fputc(1, out);       /* int length */
				j = 0;
			}                       /* place holder */
			else
				j = val(src, arg3);

			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, PRINTTAIL_VAR_MODE);
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			writecrc(src, arg);
			i = val(src, arg2);
			fwrite(&i, 2, 1, out);
			fwrite(&j, 1, 1, out);
			continue;
		}

		if (!stricmp(p, "PRINTFILE_STR")) {
			fprintf(out, "%c", CS_PRINTFILE_STR);
			continue;
		}
		if (!stricmp(p, "PRINTFILE_LOCAL")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_PRINTFILE_LOCAL);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "PRINTFILE_REMOTE")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_PRINTFILE_REMOTE);
			writestr(arg);
			continue;
		}

		if (!stricmp(p, "TELNET_GATE")) {
			if (!(*arg))
				break;
			if ((l = isvar(arg2)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(2, out);       /* int offset */
				fputc(4, out);       /* int length */
				l = 0;
			}                       /* place holder */
			else if (*arg2)
				l = val(src, arg2);
			else
				l = 0;

			if (*arg == '"') {
				fprintf(out, "%c%c", CS_VAR_INSTRUCTION, TELNET_GATE_STR);
				fwrite(&l, 4, 1, out);
				writestr(arg);
			} else {
				fprintf(out, "%c%c", CS_VAR_INSTRUCTION, TELNET_GATE_VAR);
				fwrite(&l, 4, 1, out);
				p = strchr(arg, ' ');
				if (p)
					*p = 0;
				writecrc(src, arg);
			}
			continue;
		}

		if (!stricmp(p, "EXEC")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_EXEC);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "EXEC_INT")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_EXEC_INT);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "EXEC_BIN")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_EXEC_BIN);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "EXEC_XTRN")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_EXEC_XTRN);
			writestr(arg);
			continue;
		}

		if (!stricmp(p, "SELECT_SHELL")) {
			fprintf(out, "%c", CS_SELECT_SHELL);
			continue;
		}
		if (!stricmp(p, "SET_SHELL")) {
			fprintf(out, "%c", CS_SET_SHELL);
			continue;
		}
		if (!stricmp(p, "SELECT_EDITOR")) {
			fprintf(out, "%c", CS_SELECT_EDITOR);
			continue;
		}
		if (!stricmp(p, "SET_EDITOR")) {
			fprintf(out, "%c", CS_SET_EDITOR);
			continue;
		}

		if (!stricmp(p, "YES_NO")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_YES_NO);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "NO_YES")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_NO_YES);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "MENU")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_MENU);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "SET_MENU_DIR")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_SET_MENU_DIR);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "SET_MENU_FILE")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_SET_MENU_FILE);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "SEND_FILE_VIA")) {
			if (!(*arg) || !(*arg2))
				break;
			if (*arg2 == '"') {
				fprintf(out, "%c%c%c", CS_VAR_INSTRUCTION, SEND_FILE_VIA, *arg);
				writestr(arg2);
			}
			else {
				fprintf(out, "%c%c%c", CS_VAR_INSTRUCTION, SEND_FILE_VIA_VAR, *arg);
				writecrc(src, arg2);
			}
			continue;
		}
		if (!stricmp(p, "RECEIVE_FILE_VIA")) {
			if (!(*arg) || !(*arg2))
				break;
			if (*arg2 == '"') {
				fprintf(out, "%c%c%c", CS_VAR_INSTRUCTION, RECEIVE_FILE_VIA, *arg);
				writestr(arg2);
			}
			else {
				fprintf(out, "%c%c%c", CS_VAR_INSTRUCTION, RECEIVE_FILE_VIA_VAR, *arg);
				writecrc(src, arg2);
			}
			continue;
		}
		if (!stricmp(p, "CHKFILE")) {
			if (!(*arg))
				break;
			if (*arg == '"') {
				fprintf(out, "%c", CS_CHKFILE);
				writestr(arg);
			}
			else {
				fprintf(out, "%c%c", CS_VAR_INSTRUCTION, CHKFILE_VAR);
				writecrc(src, arg);
			}
			continue;
		}
		if (!stricmp(p, "GET_FILE_LENGTH")) {
			if (!(*arg) || !(*arg2))
				break;
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, FLENGTH_TO_INT);
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}
		if (!stricmp(p, "GET_FILE_TIME")) {
			if (!(*arg) || !(*arg2))
				break;
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, FTIME_TO_INT);
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}
		if (!stricmp(p, "CHARVAL")) {
			if (!(*arg) || !(*arg2))
				break;
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, CHARVAL_TO_INT);
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}
		if (!stricmp(p, "SETSTR")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_SETSTR);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "COMPARE_STR")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_COMPARE_STR);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "GET_TEMPLATE")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_GET_TEMPLATE);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "READ_SIF")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_READ_SIF);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "CREATE_SIF")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_CREATE_SIF);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "TRASHCAN")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_TRASHCAN);
			writestr(arg);
			continue;
		}
		if (!stricmp(p, "CMDSTR")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_CMDSTR);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "CMDKEYS")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_CMDKEYS);
			for (p = arg; *p && *p != '#'; p++) {
				ch = *p;
				if (ch == '"')
					continue;
				if (ch == '/') {
					p++;
					ch = *p | 0x80;
				}                  /* high bit indicates slash required */
				else if (ch == '^' && *(p + 1) >= 0x40) {
					p++;
					ch = *p;
					ch -= 0x40;
				}
				else if (ch == '\\') {
					p++;
					ch = cesc(*p);
				}
				fputc(ch, out);
			}
			fputc(0, out);
			continue;
		}
		if (!stricmp(p, "COMPARE_WORD")) {
			if (!(*arg))
				break;
			fprintf(out, "%c", CS_COMPARE_WORD);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "GETSTR")) {
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			if ((!(*arg) || IS_DIGIT(*arg) || !stricmp(arg, "STR")) && !(*arg3))
				fprintf(out, "%c%c", CS_GETSTR, atoi(arg) ? atoi(arg)
				    : *arg2 ? atoi(arg2) : 128);
			else {
				if ((l = isvar(arg2)) != 0) {
					fputc(CS_USE_INT_VAR, out);
					fwrite(&l, 4, 1, out); /* variable */
					fputc(6, out);       /* int offset */
					fputc(1, out);       /* int length */
					i = 0;
				}                       /* place holder */
				else if (*arg2)
					i = val(src, arg2);
				else
					i = 0;

				fprintf(out, "%c%c", CS_VAR_INSTRUCTION
				        , *arg3 ? GETSTR_MODE : GETSTR_VAR);
				writecrc(src, arg);

				if (!i)
					i = 128;
				fwrite(&i, 1, 1, out);
				if (*arg3) {
					l = val(src, arg3);
					fwrite(&l, 4, 1, out);
				}
			}
			continue;
		}
		if (!stricmp(p, "GETNUM")) {
			if (!(*arg))
				break;
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			if (IS_DIGIT(*arg)) {
				i = val(src, arg);
				fprintf(out, "%c", CS_GETNUM);
				fwrite(&i, 2, 1, out);
			}
			else {
				if ((l = isvar(arg2)) != 0) {
					fputc(CS_USE_INT_VAR, out);
					fwrite(&l, 4, 1, out); /* variable */
					fputc(6, out);       /* int offset */
					fputc(2, out);       /* int length */
					i = 0;
				}                       /* place holder */
				else
					i = val(src, arg2);

				fprintf(out, "%c%c", CS_VAR_INSTRUCTION, GETNUM_VAR);
				writecrc(src, arg);
				fwrite(&i, 2, 1, out);
			}
			continue;
		}
		if (!stricmp(p, "MSWAIT")) {
			if (!(*arg))
				break;
			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(2, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg);

			fprintf(out, "%c", CS_MSWAIT);
			fwrite(&i, 2, 1, out);
			continue;
		}
		if (!stricmp(p, "GETLINE")) {
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			if (!(*arg) || IS_DIGIT(*arg))
				fprintf(out, "%c%c", CS_GETLINE, *arg ? atoi(arg) :128);
			else {
				if ((l = isvar(arg2)) != 0) {
					fputc(CS_USE_INT_VAR, out);
					fwrite(&l, 4, 1, out); /* variable */
					fputc(6, out);       /* int offset */
					fputc(1, out);       /* int length */
					i = 0;
				}                       /* place holder */
				else
					i = val(src, arg2);

				fprintf(out, "%c%c", CS_VAR_INSTRUCTION, GETLINE_VAR);
				writecrc(src, arg);
				if (!i)
					i = 128;
				fwrite(&i, 1, 1, out);
			}
			continue;
		}
		if (!stricmp(p, "GETSTRUPR")) {
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			if (!(*arg) || IS_DIGIT(*arg))
				fprintf(out, "%c%c", CS_GETSTRUPR, *arg ? atoi(arg) :128);
			else {
				if ((l = isvar(arg2)) != 0) {
					fputc(CS_USE_INT_VAR, out);
					fwrite(&l, 4, 1, out); /* variable */
					fputc(6, out);       /* int offset */
					fputc(1, out);       /* int length */
					i = 0;
				}                       /* place holder */
				else
					i = val(src, arg2);

				fprintf(out, "%c%c", CS_VAR_INSTRUCTION, GETSTRUPR_VAR);
				writecrc(src, arg);
				if (!i)
					i = 128;
				fwrite(&i, 1, 1, out);
			}
			continue;
		}
		if (!stricmp(p, "GETNAME")) {
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			if (!(*arg) || IS_DIGIT(*arg))
				fprintf(out, "%c%c", CS_GETNAME, *arg ? atoi(arg) :25);
			else {
				if ((l = isvar(arg2)) != 0) {
					fputc(CS_USE_INT_VAR, out);
					fwrite(&l, 4, 1, out); /* variable */
					fputc(6, out);       /* int offset */
					fputc(1, out);       /* int length */
					i = 0;
				}                       /* place holder */
				else
					i = atoi(arg2);

				fprintf(out, "%c%c", CS_VAR_INSTRUCTION, GETNAME_VAR);
				writecrc(src, arg);
				if (!i)
					i = 128;
				fwrite(&i, 1, 1, out);
			}
			continue;
		}
		if (!stricmp(p, "SHIFT_STR")) {
			if (!(*arg))
				break;
			p = strchr(arg, ' ');
			if (p)
				*p = 0;
			if (IS_DIGIT(*arg))
				fprintf(out, "%c%c", CS_SHIFT_STR, atoi(arg));
			else {
				if ((l = isvar(arg2)) != 0) {
					fputc(CS_USE_INT_VAR, out);
					fwrite(&l, 4, 1, out); /* variable */
					fputc(6, out);       /* int offset */
					fputc(1, out);       /* int length */
					i = 0;
				}                       /* place holder */
				else
					i = atoi(arg2);

				fprintf(out, "%c%c", CS_VAR_INSTRUCTION, SHIFT_STR_VAR);
				writecrc(src, arg);
				if (!i)
					i = 128;
				fwrite(&i, 1, 1, out);
			}
			continue;
		}
		if (!stricmp(p, "SHIFT_TO_FIRST_CHAR") || !stricmp(p, "SHIFT_TO_LAST_CHAR")) {
			if (!(*arg) || !(*arg2))
				break;
			if ((l = isvar(arg2)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(6, out);       /* int offset */
				fputc(1, out);       /* int length */
				ch = 0;
			}                       /* place holder */
			else
				ch = val(src, arg2);

			fprintf(out, "%c%c", CS_VAR_INSTRUCTION
			        , !stricmp(p, "SHIFT_TO_FIRST_CHAR") ? SHIFT_TO_FIRST_CHAR : SHIFT_TO_LAST_CHAR);
			writecrc(src, arg);
			fwrite(&ch, sizeof(ch), 1, out);
			continue;
		}
		if (!stricmp(p, "TRUNCSP")) {
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, TRUNCSP_STR_VAR);
			writecrc(src, arg);
			continue;
		}
		if (!stricmp(p, "STRIP_CTRL")) {
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, STRIP_CTRL_STR_VAR);
			writecrc(src, arg);
			continue;
		}
		if (!stricmp(p, "STRUPR")) {
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, STRUPR_VAR);
			writecrc(src, arg);
			continue;
		}
		if (!stricmp(p, "STRLWR")) {
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, STRLWR_VAR);
			writecrc(src, arg);
			continue;
		}
		if (!stricmp(p, "STRLEN")) {
			if (!(*arg))
				break;
			fprintf(out, "%c%c", CS_VAR_INSTRUCTION, STRLEN_INT_VAR);
			p = strchr(arg, ' ');
			if (!p)
				break;
			*p = 0;
			writecrc(src, arg);
			writecrc(src, arg2);
			continue;
		}
		if (!stricmp(p, "REPLACE_TEXT")) {
			if (!(*arg) || !(*arg2))
				break;
			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(2, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg);

			fprintf(out, "%c", CS_REPLACE_TEXT);
			fwrite(&i, 2, 1, out);
			writecstr(arg2);
			continue;
		}
		if (!stricmp(p, "REVERT_TEXT")) {
			if (!(*arg))
				break;
			if (!stricmp(arg, "ALL"))
				i = 0xffff;
			else {
				if ((l = isvar(arg)) != 0) {
					fputc(CS_USE_INT_VAR, out);
					fwrite(&l, 4, 1, out); /* variable */
					fputc(1, out);       /* int offset */
					fputc(2, out);       /* int length */
					i = 0;
				}                       /* place holder */
				else
					i = val(src, arg);
			}

			fprintf(out, "%c", CS_REVERT_TEXT);
			fwrite(&i, 2, 1, out);
			continue;
		}
		if (!stricmp(p, "TOGGLE_USER_MISC")
		    || !stricmp(p, "COMPARE_USER_MISC")) {
			if (!(*arg))
				break;

			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(4, out);       /* int length */
				l = 0;
			}                       /* place holder */
			else
				l = val(src, arg);

			if (!stricmp(p, "TOGGLE_USER_MISC"))
				fprintf(out, "%c", CS_TOGGLE_USER_MISC);
			else
				fprintf(out, "%c", CS_COMPARE_USER_MISC);
			fwrite(&l, 4, 1, out);
			continue;
		}

		if (!stricmp(p, "TOGGLE_USER_CHAT")
		    || !stricmp(p, "COMPARE_USER_CHAT")) {
			if (!(*arg))
				break;

			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(4, out);       /* int length */
				l = 0;
			}                       /* place holder */
			else
				l = val(src, arg);

			if (!stricmp(p, "TOGGLE_USER_CHAT"))
				fprintf(out, "%c", CS_TOGGLE_USER_CHAT);
			else
				fprintf(out, "%c", CS_COMPARE_USER_CHAT);
			fwrite(&l, 4, 1, out);
			continue;
		}

		if (!stricmp(p, "TOGGLE_USER_QWK")
		    || !stricmp(p, "COMPARE_USER_QWK")) {
			if (!(*arg))
				break;

			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(4, out);       /* int length */
				l = 0;
			}                       /* place holder */
			else
				l = val(src, arg);

			if (!stricmp(p, "TOGGLE_USER_QWK"))
				fprintf(out, "%c", CS_TOGGLE_USER_QWK);
			else
				fprintf(out, "%c", CS_COMPARE_USER_QWK);
			fwrite(&l, 4, 1, out);
			continue;
		}

		if (!stricmp(p, "TOGGLE_NODE_MISC")
		    || !stricmp(p, "COMPARE_NODE_MISC")) {
			if (!(*arg))
				break;

			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(2, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg);

			if (!stricmp(p, "TOGGLE_NODE_MISC"))
				fprintf(out, "%c", CS_TOGGLE_NODE_MISC);
			else
				fprintf(out, "%c", CS_COMPARE_NODE_MISC);
			fwrite(&i, 2, 1, out);
			continue;
		}

		if (!stricmp(p, "TOGGLE_USER_FLAG")) {
			if (!(*arg))
				break;
			p = arg;
			fprintf(out, "%c%c", CS_TOGGLE_USER_FLAG, toupper(*p++));
			SKIPCTRLSP(p);
			fprintf(out, "%c", toupper(*p));
			continue;
		}

		if (!stricmp(p, "SET_USER_LEVEL")) {
			if (!(*arg))
				break;

			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(1, out);       /* int length */
				ch = 0;
			}                       /* place holder */
			else
				ch = val(src, arg);

			fprintf(out, "%c%c", CS_SET_USER_LEVEL, ch);
			continue;
		}

		if (!stricmp(p, "SET_USER_STRING")) {
			if (!(*arg))
				break;

			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(1, out);       /* int length */
				ch = 0;
			}                       /* place holder */
			else
				ch = val(src, arg);

			fprintf(out, "%c%c", CS_SET_USER_STRING, ch);
			continue;
		}


		if (!stricmp(p, "ADJUST_USER_CREDITS")) {
			if (!(*arg))
				break;

			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(2, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg);

			fprintf(out, "%c", CS_ADJUST_USER_CREDITS);
			fwrite(&i, 2, 1, out);
			continue;
		}

		if (!stricmp(p, "ADJUST_USER_MINUTES")) {
			if (!(*arg))
				break;

			if ((l = isvar(arg)) != 0) {
				fputc(CS_USE_INT_VAR, out);
				fwrite(&l, 4, 1, out); /* variable */
				fputc(1, out);       /* int offset */
				fputc(2, out);       /* int length */
				i = 0;
			}                       /* place holder */
			else
				i = val(src, arg);

			fprintf(out, "%c", CS_ADJUST_USER_MINUTES);
			fwrite(&i, 2, 1, out);
			continue;
		}

		if (!stricmp(p, "SHOW_MEM")) {
			fprintf(out, "%c", CS_SHOW_MEM);
			continue;
		}
		if (!stricmp(p, "GURU_LOG")) {
			fprintf(out, "%c", CS_GURU_LOG);
			continue;
		}
		if (!stricmp(p, "ERROR_LOG")) {
			fprintf(out, "%c", CS_ERROR_LOG);
			continue;
		}
		if (!stricmp(p, "SYSTEM_LOG")) {
			fprintf(out, "%c", CS_SYSTEM_LOG);
			continue;
		}
		if (!stricmp(p, "SYSTEM_YLOG")) {
			fprintf(out, "%c", CS_SYSTEM_YLOG);
			continue;
		}
		if (!stricmp(p, "SYSTEM_STATS")) {
			fprintf(out, "%c", CS_SYSTEM_STATS);
			continue;
		}
		if (!stricmp(p, "NODE_STATS")) {
			fprintf(out, "%c", CS_NODE_STATS);
			continue;
		}
		if (!stricmp(p, "CHANGE_USER")) {
			fprintf(out, "%c", CS_CHANGE_USER);
			continue;
		}
		if (!stricmp(p, "ANSI_CAPTURE")) {
			fprintf(out, "%c", CS_ANSI_CAPTURE);
			continue;
		}
		if (!stricmp(p, "LIST_TEXT_FILE")) {
			fprintf(out, "%c", CS_LIST_TEXT_FILE);
			continue;
		}
		if (!stricmp(p, "EDIT_TEXT_FILE")) {
			fprintf(out, "%c", CS_EDIT_TEXT_FILE);
			continue;
		}


		if (!stricmp(p, "COMPARE_KEY")) {
			if (!stricmp(arg, "DIGIT"))
				ch = CS_DIGIT;
			else if (!stricmp(arg, "EDIGIT"))
				ch = CS_EDIGIT;
			else
				ch = toupper(*arg);
			if (ch == '/')
				ch = (*arg) | 0x80; /* high bit indicates slash required */
			else if (ch == '^' && (*(arg + 1)) >= 0x40)
				ch = (*(arg + 1)) - 0x40; /* ctrl char */
			else if (ch == '\\')
				ch = cesc(*(arg + 1));
			else if (ch == '\'')
				ch = *(arg + 1);
			fprintf(out, "%c%c", CS_COMPARE_KEY, ch);
			continue;
		}
		if (!stricmp(p, "COMPARE_CHAR")) {
			ch = *arg;
			fprintf(out, "%c%c", CS_COMPARE_CHAR, ch);
			continue;
		}
		if (!stricmp(p, "COMPARE_KEYS")) {
			fputc(CS_COMPARE_KEYS, out);
			for (p = arg; *p && *p != '#'; p++) {
				ch = *p;
				if (ch == '"')
					continue;
				if (ch == '/') {
					p++;
					ch = *p | 0x80;
				}                  /* high bit indicates slash required */
				else if (ch == '^' && *(p + 1) >= 0x40) {
					p++;
					ch = *p;
					ch -= 0x40;
				}
				else if (ch == '\\') {
					p++;
					ch = cesc(*p);
				}
				fputc(ch, out);
			}
			fputc(0, out);
			continue;
		}
		if (!stricmp(p, "GETCMD")) {
			fprintf(out, "%c", CS_GETCMD);
			writecstr(arg);
			continue;
		}
		if (!stricmp(p, "INKEY")) {
			fprintf(out, "%c", CS_INKEY);
			continue;
		}
		if (!stricmp(p, "INCHAR")) {
			fprintf(out, "%c", CS_INCHAR);
			continue;
		}
		if (!stricmp(p, "GETKEY")) {
			fprintf(out, "%c", CS_GETKEY);
			continue;
		}
		if (!stricmp(p, "GETCHAR")) {
			fprintf(out, "%c", CS_GETCHAR);
			continue;
		}
		if (!stricmp(p, "GETKEYE")) {
			fprintf(out, "%c", CS_GETKEYE);
			continue;
		}
		if (!stricmp(p, "UNGETKEY")) {
			fprintf(out, "%c", CS_UNGETKEY);
			continue;
		}
		if (!stricmp(p, "UNGETSTR")) {
			fprintf(out, "%c", CS_UNGETSTR);
			continue;
		}
		if (!stricmp(p, "PRINTKEY")) {
			fprintf(out, "%c", CS_PRINTKEY);
			continue;
		}
		if (!stricmp(p, "PRINTSTR")) {
			fprintf(out, "%c", CS_PRINTSTR);
			continue;
		}

		/* FUNCTIONS */

		if (!stricmp(p, "NODELIST_ALL")) {
			fprintf(out, "%c", CS_NODELIST_ALL);
			continue;
		}
		if (!stricmp(p, "NODELIST_USERS")) {
			fprintf(out, "%c", CS_NODELIST_USERS);
			continue;
		}

		if (!stricmp(p, "USERLIST_ALL")) {
			fprintf(out, "%c", CS_USERLIST_ALL);
			continue;
		}
		if (!stricmp(p, "USERLIST_SUB")) {
			fprintf(out, "%c", CS_USERLIST_SUB);
			continue;
		}
		if (!stricmp(p, "USERLIST_DIR")) {
			fprintf(out, "%c", CS_USERLIST_DIR);
			continue;
		}
		if (!stricmp(p, "USERLIST_LOGONS")) {
			fprintf(out, "%c", CS_USERLIST_LOGONS);
			continue;
		}

		if (!stricmp(p, "HANGUP")) {
			fprintf(out, "%c", CS_HANGUP);
			continue;
		}

		if (!stricmp(p, "LOGOFF")) {
			fprintf(out, "%c", CS_LOGOFF);
			continue;
		}

		if (!stricmp(p, "LOGOFF_FAST")) {
			fprintf(out, "%c", CS_LOGOFF_FAST);
			continue;
		}

		if (!stricmp(p, "AUTO_MESSAGE")) {
			fprintf(out, "%c", CS_AUTO_MESSAGE);
			continue;
		}

		if (!stricmp(p, "MINUTE_BANK")) {
			fprintf(out, "%c", CS_MINUTE_BANK);
			continue;
		}

		if (!stricmp(p, "USER_EDIT")) {
			fprintf(out, "%c", CS_USER_EDIT);
			continue;
		}

		if (!stricmp(p, "USER_DEFAULTS")) {
			fprintf(out, "%c", CS_USER_DEFAULTS);
			continue;
		}

		if (!stricmp(p, "PAGE_SYSOP")) {
			fprintf(out, "%c", CS_PAGE_SYSOP);
			continue;
		}
		if (!stricmp(p, "PAGE_GURU")) {
			fprintf(out, "%c", CS_PAGE_GURU);
			continue;
		}
		if (!stricmp(p, "SPY")) {
			fprintf(out, "%c", CS_SPY);
			continue;
		}


		if (!stricmp(p, "PRIVATE_CHAT")) {
			fprintf(out, "%c", CS_PRIVATE_CHAT);
			continue;
		}

		if (!stricmp(p, "PRIVATE_MESSAGE")) {
			fprintf(out, "%c", CS_PRIVATE_MESSAGE);
			continue;
		}

		if (!stricmp(p, "MULTINODE_CHAT")) {
			if (!(*arg))
				ch = 1;
			else {
				if ((l = isvar(arg)) != 0) {
					fputc(CS_USE_INT_VAR, out);
					fwrite(&l, 4, 1, out); /* variable */
					fputc(1, out);       /* int offset */
					fputc(1, out);       /* int length */
					ch = 0;
				}                       /* place holder */
				else
					ch = val(src, arg);
			}

			fprintf(out, "%c%c", CS_MULTINODE_CHAT, ch);
			continue;
		}

		if (!stricmp(p, "MAIL_READ")) {
			fprintf(out, "%c", CS_MAIL_READ);
			continue;
		}
		if (!stricmp(p, "MAIL_READ_SENT")) {       /* Kill/read sent mail */
			fprintf(out, "%c", CS_MAIL_READ_SENT);
			continue;
		}
		if (!stricmp(p, "MAIL_READ_ALL")) {
			fprintf(out, "%c", CS_MAIL_READ_ALL);
			continue;
		}
		if (!stricmp(p, "MAIL_SEND")) {       /* Send E-mail */
			fprintf(out, "%c", CS_MAIL_SEND);
			continue;
		}
		if (!stricmp(p, "MAIL_SEND_FEEDBACK")) {       /* Feedback */
			fprintf(out, "%c", CS_MAIL_SEND_FEEDBACK);
			continue;
		}
		if (!stricmp(p, "MAIL_SEND_NETMAIL")) {
			fprintf(out, "%c", CS_MAIL_SEND_NETMAIL);
			continue;
		}
		if (!stricmp(p, "MAIL_SEND_NETFILE")) {
			fprintf(out, "%c", CS_MAIL_SEND_NETFILE);
			continue;
		}
		if (!stricmp(p, "MAIL_SEND_FILE")) {   /* Upload Attached File to E-mail */
			fprintf(out, "%c", CS_MAIL_SEND_FILE);
			continue;
		}
		if (!stricmp(p, "MAIL_SEND_BULK")) {
			fprintf(out, "%c", CS_MAIL_SEND_BULK);
			continue;
		}


		if (!stricmp(p, "MSG_SET_AREA")) {
			fprintf(out, "%c", CS_MSG_SET_AREA);
			continue;
		}
		if (!stricmp(p, "MSG_SET_GROUP")) {
			fprintf(out, "%c", CS_MSG_SET_GROUP);
			continue;
		}
		if (!stricmp(p, "MSG_SELECT_AREA")) {
			fprintf(out, "%c", CS_MSG_SELECT_AREA);
			continue;
		}
		if (!stricmp(p, "MSG_SHOW_GROUPS")) {
			fprintf(out, "%c", CS_MSG_SHOW_GROUPS);
			continue;
		}
		if (!stricmp(p, "MSG_SHOW_SUBBOARDS")) {
			fprintf(out, "%c", CS_MSG_SHOW_SUBBOARDS);
			continue;
		}
		if (!stricmp(p, "MSG_GROUP_UP")) {
			fprintf(out, "%c", CS_MSG_GROUP_UP);
			continue;
		}
		if (!stricmp(p, "MSG_GROUP_DOWN")) {
			fprintf(out, "%c", CS_MSG_GROUP_DOWN);
			continue;
		}
		if (!stricmp(p, "MSG_SUBBOARD_UP")) {
			fprintf(out, "%c", CS_MSG_SUBBOARD_UP);
			continue;
		}
		if (!stricmp(p, "MSG_SUBBOARD_DOWN")) {
			fprintf(out, "%c", CS_MSG_SUBBOARD_DOWN);
			continue;
		}
		if (!stricmp(p, "MSG_GET_SUB_NUM")) {
			fprintf(out, "%c", CS_MSG_GET_SUB_NUM);
			continue;
		}
		if (!stricmp(p, "MSG_GET_GRP_NUM")) {
			fprintf(out, "%c", CS_MSG_GET_GRP_NUM);
			continue;
		}
		if (!stricmp(p, "MSG_READ")) {
			fprintf(out, "%c", CS_MSG_READ);
			continue;
		}
		if (!stricmp(p, "MSG_POST")) {
			fprintf(out, "%c", CS_MSG_POST);
			continue;
		}
		if (!stricmp(p, "MSG_QWK")) {
			fprintf(out, "%c", CS_MSG_QWK);
			continue;
		}
		if (!stricmp(p, "MSG_PTRS_CFG")) {
			fprintf(out, "%c", CS_MSG_PTRS_CFG);
			continue;
		}
		if (!stricmp(p, "MSG_PTRS_REINIT")) {
			fprintf(out, "%c", CS_MSG_PTRS_REINIT);
			continue;
		}
		if (!stricmp(p, "MSG_NEW_SCAN_CFG")) {
			fprintf(out, "%c", CS_MSG_NEW_SCAN_CFG);
			continue;
		}
		if (!stricmp(p, "MSG_NEW_SCAN")) {
			fprintf(out, "%c", CS_MSG_NEW_SCAN);
			continue;
		}
		if (!stricmp(p, "MSG_NEW_SCAN_SUB")) {
			fprintf(out, "%c", CS_MSG_NEW_SCAN_SUB);
			continue;
		}
		if (!stricmp(p, "MSG_NEW_SCAN_ALL")) {
			fprintf(out, "%c", CS_MSG_NEW_SCAN_ALL);
			continue;
		}
		if (!stricmp(p, "MSG_CONT_SCAN")) {
			fprintf(out, "%c", CS_MSG_CONT_SCAN);
			continue;
		}
		if (!stricmp(p, "MSG_CONT_SCAN_ALL")) {
			fprintf(out, "%c", CS_MSG_CONT_SCAN_ALL);
			continue;
		}
		if (!stricmp(p, "MSG_BROWSE_SCAN")) {
			fprintf(out, "%c", CS_MSG_BROWSE_SCAN);
			continue;
		}
		if (!stricmp(p, "MSG_BROWSE_SCAN_ALL")) {
			fprintf(out, "%c", CS_MSG_BROWSE_SCAN_ALL);
			continue;
		}
		if (!stricmp(p, "MSG_FIND_TEXT")) {
			fprintf(out, "%c", CS_MSG_FIND_TEXT);
			continue;
		}
		if (!stricmp(p, "MSG_FIND_TEXT_ALL")) {
			fprintf(out, "%c", CS_MSG_FIND_TEXT_ALL);
			continue;
		}
		if (!stricmp(p, "MSG_YOUR_SCAN_CFG")) {
			fprintf(out, "%c", CS_MSG_YOUR_SCAN_CFG);
			continue;
		}
		if (!stricmp(p, "MSG_YOUR_SCAN")) {
			fprintf(out, "%c", CS_MSG_YOUR_SCAN);
			continue;
		}
		if (!stricmp(p, "MSG_YOUR_SCAN_ALL")) {
			fprintf(out, "%c", CS_MSG_YOUR_SCAN_ALL);
			continue;
		}
		if (!stricmp(p, "MSG_LIST")) {
			fprintf(out, "%c", CS_MSG_LIST);
			continue;
		}
		if (!stricmp(p, "CHAT_SECTION")) {
			fprintf(out, "%c", CS_CHAT_SECTION);
			continue;
		}
		if (!stricmp(p, "TEXT_FILE_SECTION")) {
			fprintf(out, "%c", CS_TEXT_FILE_SECTION);
			continue;
		}
		if (!stricmp(p, "XTRN_EXEC")) {
			fprintf(out, "%c", CS_XTRN_EXEC);
			continue;
		}
		if (!stricmp(p, "XTRN_SECTION")) {
			fprintf(out, "%c", CS_XTRN_SECTION);
			continue;
		}

		if (!stricmp(p, "FILE_SET_AREA")) {
			fprintf(out, "%c", CS_FILE_SET_AREA);
			continue;
		}
		if (!stricmp(p, "FILE_SET_LIBRARY")) {
			fprintf(out, "%c", CS_FILE_SET_LIBRARY);
			continue;
		}
		if (!stricmp(p, "FILE_SELECT_AREA")) {
			fprintf(out, "%c", CS_FILE_SELECT_AREA);
			continue;
		}
		if (!stricmp(p, "FILE_SHOW_LIBRARIES")) {
			fprintf(out, "%c", CS_FILE_SHOW_LIBRARIES);
			continue;
		}
		if (!stricmp(p, "FILE_SHOW_DIRECTORIES")) {
			fprintf(out, "%c", CS_FILE_SHOW_DIRECTORIES);
			continue;
		}
		if (!stricmp(p, "FILE_LIBRARY_UP")) {
			fprintf(out, "%c", CS_FILE_LIBRARY_UP);
			continue;
		}
		if (!stricmp(p, "FILE_LIBRARY_DOWN")) {
			fprintf(out, "%c", CS_FILE_LIBRARY_DOWN);
			continue;
		}
		if (!stricmp(p, "FILE_DIRECTORY_UP")) {
			fprintf(out, "%c", CS_FILE_DIRECTORY_UP);
			continue;
		}
		if (!stricmp(p, "FILE_DIRECTORY_DOWN")) {
			fprintf(out, "%c", CS_FILE_DIRECTORY_DOWN);
			continue;
		}
		if (!stricmp(p, "FILE_GET_DIR_NUM")) {
			fprintf(out, "%c", CS_FILE_GET_DIR_NUM);
			continue;
		}
		if (!stricmp(p, "FILE_GET_LIB_NUM")) {
			fprintf(out, "%c", CS_FILE_GET_LIB_NUM);
			continue;
		}
		if (!stricmp(p, "FILE_UPLOAD")) {
			fprintf(out, "%c", CS_FILE_UPLOAD);
			continue;
		}
		if (!stricmp(p, "FILE_UPLOAD_USER")) {
			fprintf(out, "%c", CS_FILE_UPLOAD_USER);
			continue;
		}
		if (!stricmp(p, "FILE_UPLOAD_BULK")) {
			fprintf(out, "%c", CS_FILE_UPLOAD_BULK);
			continue;
		}
		if (!stricmp(p, "FILE_UPLOAD_SYSOP")) {
			fprintf(out, "%c", CS_FILE_UPLOAD_SYSOP);
			continue;
		}
		if (!stricmp(p, "FILE_RESORT_DIRECTORY")) {
			fprintf(out, "%c", CS_FILE_RESORT_DIRECTORY);
			continue;
		}
		if (!stricmp(p, "FILE_SET_ALT_PATH")) {
			fprintf(out, "%c", CS_FILE_SET_ALT_PATH);
			continue;
		}
		if (!stricmp(p, "FILE_GET")) {
			fprintf(out, "%c", CS_FILE_GET);
			continue;
		}
		if (!stricmp(p, "FILE_SEND")) {
			fprintf(out, "%c", CS_FILE_SEND);
			continue;
		}
		if (!stricmp(p, "FILE_PUT")) {
			fprintf(out, "%c", CS_FILE_PUT);
			continue;
		}
		if (!stricmp(p, "FILE_RECEIVE")) {
			fprintf(out, "%c", CS_FILE_RECEIVE);
			continue;
		}
		if (!stricmp(p, "FILE_FIND_OLD")) {
			fprintf(out, "%c", CS_FILE_FIND_OLD);
			continue;
		}
		if (!stricmp(p, "FILE_FIND_OPEN")) {
			fprintf(out, "%c", CS_FILE_FIND_OPEN);
			continue;
		}
		if (!stricmp(p, "FILE_FIND_OFFLINE")) {
			fprintf(out, "%c", CS_FILE_FIND_OFFLINE);
			continue;
		}
		if (!stricmp(p, "FILE_FIND_OLD_UPLOADS")) {
			fprintf(out, "%c", CS_FILE_FIND_OLD_UPLOADS);
			continue;
		}
		if (!stricmp(p, "FILE_DOWNLOAD")) {
			fprintf(out, "%c", CS_FILE_DOWNLOAD);
			continue;
		}
		if (!stricmp(p, "FILE_DOWNLOAD_USER")) {
			fprintf(out, "%c", CS_FILE_DOWNLOAD_USER);
			continue;
		}
		if (!stricmp(p, "FILE_DOWNLOAD_BATCH")) {
			fprintf(out, "%c", CS_FILE_DOWNLOAD_BATCH);
			continue;
		}
		if (!stricmp(p, "FILE_REMOVE")) {
			fprintf(out, "%c", CS_FILE_REMOVE);
			continue;
		}
		if (!stricmp(p, "FILE_LIST")) {
			fprintf(out, "%c", CS_FILE_LIST);
			continue;
		}
		if (!stricmp(p, "FILE_LIST_EXTENDED")) {
			fprintf(out, "%c", CS_FILE_LIST_EXTENDED);
			continue;
		}
		if (!stricmp(p, "FILE_VIEW")) {
			fprintf(out, "%c", CS_FILE_VIEW);
			continue;
		}
		if (!stricmp(p, "FILE_FIND_TEXT")) {
			fprintf(out, "%c", CS_FILE_FIND_TEXT);
			continue;
		}
		if (!stricmp(p, "FILE_FIND_TEXT_ALL")) {
			fprintf(out, "%c", CS_FILE_FIND_TEXT_ALL);
			continue;
		}
		if (!stricmp(p, "FILE_FIND_NAME")) {
			fprintf(out, "%c", CS_FILE_FIND_NAME);
			continue;
		}
		if (!stricmp(p, "FILE_FIND_NAME_ALL")) {
			fprintf(out, "%c", CS_FILE_FIND_NAME_ALL);
			continue;
		}
		if (!stricmp(p, "FILE_BATCH_SECTION")) {
			fprintf(out, "%c", CS_FILE_BATCH_SECTION);
			continue;
		}
		if (!stricmp(p, "FILE_TEMP_SECTION")) {
			fprintf(out, "%c", CS_FILE_TEMP_SECTION);
			continue;
		}
		if (!stricmp(p, "FILE_NEW_SCAN")) {
			fprintf(out, "%c", CS_FILE_NEW_SCAN);
			continue;
		}
		if (!stricmp(p, "FILE_NEW_SCAN_ALL")) {
			fprintf(out, "%c", CS_FILE_NEW_SCAN_ALL);
			continue;
		}
		if (!stricmp(p, "FILE_NEW_SCAN_CFG")) {
			fprintf(out, "%c", CS_FILE_NEW_SCAN_CFG);
			continue;
		}
		if (!stricmp(p, "FILE_PTRS_CFG")) {
			fprintf(out, "%c", CS_FILE_PTRS_CFG);
			continue;
		}
		if (!stricmp(p, "FILE_BATCH_ADD")) {
			fprintf(out, "%c", CS_FILE_BATCH_ADD);
			continue;
		}
		if (!stricmp(p, "FILE_BATCH_ADD_LIST")) {
			fprintf(out, "%c", CS_FILE_BATCH_ADD_LIST);
			continue;
		}
		if (!stricmp(p, "FILE_BATCH_CLEAR")) {
			fprintf(out, "%c", CS_FILE_BATCH_CLEAR);
			continue;
		}

		if (!stricmp(p, "INC_MAIN_CMDS")) {
			fprintf(out, "%c", CS_INC_MAIN_CMDS);
			continue;
		}
		if (!stricmp(p, "INC_FILE_CMDS")) {
			fprintf(out, "%c", CS_INC_FILE_CMDS);
			continue;
		}

		break;
	}


	if (!feof(in)) {
		printf("!SYNTAX ERROR:\n");
		printf(linestr, src, line, save);
		bail(1);
	}
	fclose(in);
	free(str);
	free(save);
}

char *banner =   "\n"
               "BAJA v2.34-%s (rev %s) - Synchronet Shell/Module Compiler\n";

char *usage =    "\n"
              "usage: baja [-opts] file[.src]\n"
              "\n"
              " opts: -d display debug during compile\n"
              "       -c case sensitive variables, labels, and macros\n"
              "       -o set output directory (e.g. -o/sbbs/exec)\n"
              "       -i set include directory (e.g. -i/sbbs/exec)\n"
              "       -q quiet mode (no banner)\n"
              "       -p pause on error"
;

int main(int argc, char **argv)
{
	char src[MAX_PATH + 1] = "", *p;
	char path[MAX_PATH + 1];
	int  i, j;
	int  show_banner = TRUE;

	p = getenv("BAJAINCLUDE");
	if (p != NULL) {
		SAFECOPY(include_dir, p);
		backslash(include_dir);
	}

	for (i = 1; i < argc; i++)
		if (argv[i][0] == '-'
#ifdef _WIN32
		    || argv[i][0] == '/'
#endif
		    )
			switch (toupper(argv[i][1])) {
				case 'D':
					display = 1;
					break;
				case 'C':
					case_sens = 1;
					break;
				case 'O':
					SAFECOPY(output_dir, argv[i] + 2);
					backslash(output_dir);
					break;
				case 'I':
					SAFECOPY(include_dir, argv[i] + 2);
					backslash(include_dir);
					break;
				case 'P':
					pause_on_error = TRUE;
					break;
				case 'Q':
					show_banner = 0;
					break;
				default:
					printf(banner, PLATFORM_DESC, VERSION);
					puts(usage);
					bail(1);
			}
		else
			SAFECOPY(src, argv[i]);

	if (show_banner)
		printf(banner, PLATFORM_DESC, VERSION);

	if (!src[0]) {
		puts(usage);
		bail(1);
	}

	if (include_dir[0] == 0) {             /* Include directory not specified */
		SAFECOPY(include_dir, src);      /* Default to same dir as src file */
		if ((p = getfname(include_dir)) != NULL)
			*p = 0;                     /* Truncate off the src filename */
	}
	if (getfext(src) == NULL)
		SAFECAT(src, ".src");

	SAFECOPY(bin_file, src);
	if ((p = getfext(bin_file)) != NULL)
		*p = 0;
	SAFECAT(bin_file, ".bin");

	if (output_dir[0]) {
		p = getfname(bin_file);
		SAFEPRINTF2(path, "%s%s", output_dir, p);
		SAFECOPY(bin_file, path);
	}

	if ((out = fopen(bin_file, "w+b")) == NULL) {
		printf("error %d opening %s for write\n", errno, bin_file);
		bail(1);
	}

	printf("\nCompiling %s...\n", src);

	compile(src);

	/****************************/
	/* Resolve GOTOS and CALLS */
	/****************************/

	printf("Resolving labels...\n");

	for (i = 0; i < gotos; i++) {
		for (j = 0; j < labels; j++)
			if (!stricmp(goto_label[i], label_name[j]))
				break;
		if (j >= labels) {
			printf("%s line %d: label (%s) not found.\n"
			       , goto_file[i], goto_line[i], goto_label[i]);
			bail(1);
		}
		fseek(out, (int32_t)(goto_indx[i] + 1), SEEK_SET);
		fwrite(&label_indx[j], 2, 1, out);
	}

	for (i = 0; i < calls; i++) {
		for (j = 0; j < labels; j++)
			if ((!case_sens
			     && !strnicmp(call_label[i], label_name[j], strlen(call_label[i])))
			    || (case_sens
			        && !strncmp(call_label[i], label_name[j], strlen(call_label[i]))))
				break;
		if (j >= labels) {
			printf("%s line %d: label (%s) not found.\n"
			       , call_file[i], call_line[i], call_label[i]);
			bail(1);
		}
		fseek(out, (int32_t)(call_indx[i] + 1), SEEK_SET);
		fwrite(&label_indx[j], 2, 1, out);
	}

	printf("\nDone.\n");
	return 0;
}



