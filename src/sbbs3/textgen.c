#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "genwrap.h"    // truncsp
#include "dirwrap.h"    // MAX_PATH
#include "gen_defs.h"
#include "str_util.h"
#include "getctrl.h"

/****************************************************************************/
/* Reads special TEXT.DAT printf style text lines, splicing multiple lines, */
/* replacing escaped characters, and allocating the memory					*/
/****************************************************************************/
/* Copied from load_cfg.c with comment pointer added */
char *readtext(FILE *stream, char **comment_ret)
{
	char buf[2048], str[2048], tmp[256], *p, *p2;
	char comment[2048], *cp;
	int  i, j, k;

	if (!fgets(buf, 256, stream))
		return NULL;
	if (buf[0] == '#')
		return NULL;
	p = strrchr(buf, '"');
	if (!p) {
		return NULL;
	}
	comment[0] = 0;
	if (*(p + 1) == '\\') {  /* merge multiple lines */
		for (cp = p + 2; *cp && IS_WHITESPACE(*cp); cp++);
		truncsp(cp);
		SAFECAT(comment, cp);
		while (strlen(buf) < 2000) {
			if (!fgets(str, 255, stream))
				return NULL;
			p2 = strchr(str, '"');
			if (!p2)
				continue;
			strcpy(p, p2 + 1);
			p = strrchr(p, '"');
			if (p && *(p + 1) == '\\') {
				for (cp = p + 2; *cp && IS_WHITESPACE(*cp); cp++);
				truncsp(cp);
				SAFECAT(comment, cp);
				continue;
			}
			break;
		}
	}
	else {
		for (cp = p + 2; *cp && IS_WHITESPACE(*cp); cp++);
		SAFECAT(comment, cp);
		truncsp(comment);
	}
	*(p) = 0;
	k = strlen(buf);
	for (i = 1, j = 0; i < k; j++) {
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
	if ((p = (char *)calloc(1, j + 2)) == NULL) { /* +1 for terminator, +1 for YNQX line */
		fprintf(stderr, "Error allocating %u bytes of memory from text.dat", j);
		return NULL;
	}
	strcpy(p, str);
	if (comment_ret)
		*comment_ret = strdup(comment);
	return p;
}

char *format_as_cstr(const char *orig)
{
	int         len = 0;
	char *      ret = NULL;
	const char *in;
	char        hex[32];
	int         outpos = 0;

	len = strlen(orig);
	len += ((len / 32) * 2);
	len *= 6;
	len += 32;  /* Only needs to be three, the extra is for luck  ;-) */

	ret = (char *)malloc(len);
	if (ret == NULL)
		return ret;
	strcpy(ret, "\"");
	for (in = orig; *in; in++) {
		sprintf(hex, "\\x%02x", (unsigned char)*in);
		strcat(ret, hex);
		if ((++outpos) % 32 == 0)
			strcat(ret, "\"\n\t\t\"");
	}
	strcat(ret, "\"");
	return ret;
}

int main(int argc, char **argv)
{
	FILE *        text_dat;
	char          path[MAX_PATH + 1];
	const char *  p;
	char *        cstr;
	char *        comment;
	char *        macro;
	unsigned long lno;
	int           i = 0;
	FILE *        text_h;
	FILE *        text_js;
	FILE *        text_id;
	FILE *        text_defaults_c;

	if (argc > 1)
		p = argv[1];
	else
		p = get_ctrl_dir(/* warn: */ TRUE);
	SAFEPRINTF(path, "%s/text.dat", p);
	if ((text_dat = fopen(path, "r")) == NULL) {
		perror(path);
		return __LINE__;
	}
	if ((text_h = fopen("text.h", "wb")) == NULL) {
		perror("text.h");
		return __LINE__;
	}
	fputs("/* text.h */\n", text_h);
	fputs("\n", text_h);
	fputs("/* Synchronet static text string constants */\n", text_h);
	fputs("\n", text_h);
	fputs("/****************************************************************************/\n", text_h);
	fputs("/* Macros for elements of the array of pointers (text[]) to static text		*/\n", text_h);
	fputs("/* Auto-generated from ctrl/text.dat 										*/\n", text_h);
	fputs("/****************************************************************************/\n", text_h);
	fputs("\n", text_h);
	fputs("#ifndef _TEXT_H\n", text_h);
	fputs("#define _TEXT_H\n", text_h);
	fputs("\n", text_h);
	fputs("extern\n", text_h);
	fputs("#ifdef __cplusplus\n", text_h);
	fputs("\"C\"\n", text_h);
	fputs("#endif\n", text_h);
	fputs("const char* const text_id[];\n\n", text_h);
	fputs("enum text {\n", text_h);

	if (argc > 2)
		p = argv[2];
	else
		p = getenv("SBBSEXEC");
	if (p == NULL)
		p = "/sbbs/exec";
	SAFEPRINTF(path, "%s/load/text.js", p);
	if ((text_js = fopen(path, "wb")) == NULL) {
		perror(path);
		return __LINE__;
	}
	fputs("/* Synchronet static text string constants */\n", text_js);
	fputs("\n", text_js);
	fputs("/* Automatically generated by textgen $ */\n", text_js);
	fputs("\n", text_js);
	fputs("/****************************************************************************/\n", text_js);
	fputs("/* Values for elements of the array of pointers (bbs.text()) to static text	*/\n", text_js);
	fputs("/* Auto-generated from ctrl/text.dat											*/\n", text_js);
	fputs("/****************************************************************************/\n", text_js);
	fputs("\n", text_js);
	if ((text_defaults_c = fopen("text_defaults.c", "wb")) == NULL) {
		fprintf(stderr, "Can't open text_defaults.c!\n");
		return __LINE__;
	}
	fputs("/* Synchronet default text strings */\n", text_defaults_c);
	fputs("\n", text_defaults_c);
	fputs("/* Automatically generated by textgen $ */\n", text_defaults_c);
	fputs("\n", text_defaults_c);
	fputs("#include \"text_defaults.h\"\n", text_defaults_c);
	fputs("\n", text_defaults_c);
	fputs("const char * const text_defaults[TOTAL_TEXT]={\n", text_defaults_c);
	if ((text_id = fopen("text_id.c", "wb")) == NULL) {
		fprintf(stderr, "Can't open text_id.c!\n");
		return __LINE__;
	}
	fprintf(text_id, "// Synchronet text.dat string identifiers\n\n");
	fprintf(text_id, "const char* const text_id[]={\n");
	do {
		i++;
		p = readtext(text_dat, &comment);
		if (p != NULL) {
			cstr = format_as_cstr(p);
			if (cstr == NULL) {
				fprintf(stderr, "Error creating C string! for %d\n", i + 1);
			}
			lno = strtoul(comment, &macro, 10);
			while (IS_WHITESPACE(*macro))
				macro++;
			if ((int)lno != i) {
				fprintf(stderr, "Mismatch! %s has %ld... should be %d\n", comment, lno, i);
			}
			fprintf(text_h, "\t%c%s\n", i == 1?' ':',', macro);
			truncstr(macro, " \t");
			fprintf(text_js, "var %s=%d;\n", macro, i);
			fprintf(text_id, "\t%c\"%s\"\n", i == 1 ? ' ' : ',', macro);
			fprintf(text_defaults_c, "\t%c%s // %s\n", i == 1?' ':',', cstr, comment);
		}
	} while (p != NULL);
	fclose(text_dat);

	fputs("\n", text_h);
	fputs("\t,TOTAL_TEXT\n", text_h);
	fputs("};\n", text_h);
	fputs("\n", text_h);
	fputs("#endif\n", text_h);
	fclose(text_h);
	fputs("\n", text_js);
	fprintf(text_js, "var TOTAL_TEXT=%d;\n", i - 1);
	fprintf(text_js, "\nthis;\n");
	fclose(text_js);
	fputs("};\n", text_defaults_c);
	fclose(text_defaults_c);
	fputs("};\n", text_id);
	fclose(text_id);

	return 0;
}
