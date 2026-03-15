/*
 * scripteng.c -- Shared script engine for qtest/gtest
 *
 * Line-based script consumption and hook callbacks for driving game
 * code via scripted input.  Script format: one Type=value per line,
 * consumed in order as the game code requests input.
 *
 * Supported types:
 *   Choice=X     GetAnswer (single char from allowable set)
 *   Key=X        GetKey (single char, or ENTER/BS/SPACE)
 *   String=text  DosGetStr (free text)
 *   Number=N     DosGetLong (numeric)
 *   Topic=word   GetStringChoice (exact or prefix match)
 *   Random=N     my_random (caller-provided, not hooked here)
 *   End          End-of-script marker
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"

#include "console.h"
#include "input.h"
#include "scripteng.h"
#include "video.h"

static FILE  *script_fp   = NULL;
static int    script_ln   = 0;
static const char *tool   = "script";

/* ----------------------------------------------------------------------- */

void script_set_tool_name(const char *name)
{
	tool = name;
}

bool script_open(const char *path)
{
	script_fp = fopen(path, "r");
	script_ln = 0;
	return (script_fp != NULL);
}

void script_close(void)
{
	if (script_fp) {
		fclose(script_fp);
		script_fp = NULL;
	}
}

bool script_is_active(void)
{
	return (script_fp != NULL);
}

int script_line_number(void)
{
	return script_ln;
}

/* ----------------------------------------------------------------------- */

bool script_read_line(char *buf, size_t sz)
{
	while (fgets(buf, (int)sz, script_fp)) {
		script_ln++;
		size_t len = strlen(buf);
		while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
			buf[--len] = '\0';
		if (len > 0)
			return true;
	}
	return false;
}

const char *script_consume(const char *expected_type)
{
	static char line[512];

	if (!script_read_line(line, sizeof(line))) {
		fprintf(stderr, "%s: line %d: script ended unexpectedly, "
		        "expected %s\n", tool, script_ln + 1, expected_type);
		exit(2);
	}

	if (strcmp(line, "End") == 0) {
		fprintf(stderr, "%s: line %d: script ended (End marker) while "
		        "expecting %s\n", tool, script_ln, expected_type);
		exit(2);
	}

	char *eq = strchr(line, '=');
	if (!eq) {
		fprintf(stderr, "%s: line %d: malformed line (no '='): %s\n",
		        tool, script_ln, line);
		exit(2);
	}

	size_t tlen = strlen(expected_type);
	size_t alen = (size_t)(eq - line);
	if (alen != tlen || strncasecmp(line, expected_type, tlen) != 0) {
		char actual[64] = "(unknown)";
		if (alen < sizeof(actual)) {
			memcpy(actual, line, alen);
			actual[alen] = '\0';
		}
		fprintf(stderr, "%s: line %d: type mismatch: "
		        "expected %s, got %s\n",
		        tool, script_ln, expected_type, actual);
		exit(2);
	}

	return eq + 1;
}

void script_expect_end(void)
{
	char line[512];

	if (!script_read_line(line, sizeof(line))) {
		fprintf(stderr, "%s: line %d: expected End but reached "
		        "end of file\n", tool, script_ln + 1);
		exit(3);
	}
	if (strcmp(line, "End") != 0) {
		fprintf(stderr, "%s: line %d: expected End, got: %s\n",
		        tool, script_ln, line);
		exit(3);
	}
}

/* ----------------------------------------------------------------------- */
/* Hook callbacks                                                          */
/* ----------------------------------------------------------------------- */

char hook_get_answer(const char *szAllowableChars)
{
	const char *val = script_consume("Choice");
	if (val[0] == '\0') {
		fprintf(stderr, "%s: line %d: Choice= has no value\n",
		        tool, script_ln);
		exit(2);
	}
	char c = (char)toupper((unsigned char)val[0]);
	const char *p = szAllowableChars;
	while (*p && toupper((unsigned char)*p) != c)
		p++;
	if (!*p) {
		fprintf(stderr, "%s: line %d: Choice='%c' not in "
		        "allowable set \"%s\"\n",
		        tool, script_ln, c, szAllowableChars);
		exit(2);
	}
	return c;
}

char hook_get_key(void)
{
	const char *val = script_consume("Key");
	if (val[0] == '\0') {
		fprintf(stderr, "%s: line %d: Key= has no value\n",
		        tool, script_ln);
		exit(2);
	}
	if (plat_stricmp(val, "ENTER") == 0 || plat_stricmp(val, "CR") == 0)
		return '\r';
	if (plat_stricmp(val, "BS") == 0)
		return '\b';
	if (plat_stricmp(val, "SPACE") == 0)
		return ' ';
	return val[0];
}

void hook_dos_get_str(char *InputStr, int16_t MaxChars, bool HiBit)
{
	(void)HiBit;
	const char *val = script_consume("String");
	strlcpy(InputStr, val, (size_t)MaxChars + 1);
}

long hook_dos_get_long(const char *Prompt, long DefaultVal, long Maximum)
{
	(void)Prompt;
	(void)DefaultVal;
	const char *val = script_consume("Number");
	long n = atol(val);
	if (n > Maximum)
		n = Maximum;
	return n;
}

int16_t hook_get_string_choice(const char **apszChoices,
                               int16_t NumChoices,
                               bool AllowBlank)
{
	const char *val = script_consume("Topic");

	if (val[0] == '\0') {
		if (!AllowBlank) {
			fprintf(stderr, "%s: line %d: empty Topic= but "
			        "AllowBlank is false\n", tool, script_ln);
			exit(2);
		}
		return -1;
	}

	/* exact match first */
	for (int i = 0; i < NumChoices; i++) {
		if (plat_stricmp(val, apszChoices[i]) == 0)
			return (int16_t)i;
	}

	/* unambiguous prefix */
	size_t vlen = strlen(val);
	int found  = -1;
	int matches = 0;
	for (int i = 0; i < NumChoices; i++) {
		if (strncasecmp(val, apszChoices[i], vlen) == 0) {
			found = i;
			matches++;
		}
	}
	if (matches == 0) {
		fprintf(stderr, "%s: line %d: Topic=\"%s\" not found\n",
		        tool, script_ln, val);
		exit(2);
	}
	if (matches > 1) {
		fprintf(stderr, "%s: line %d: Topic=\"%s\" is ambiguous\n",
		        tool, script_ln, val);
		exit(2);
	}
	return (int16_t)found;
}

/* ----------------------------------------------------------------------- */

void script_install_hooks(void)
{
	Console_SetScriptMode(true);
	Console_SetGetAnswerHook(hook_get_answer);
	Console_SetGetKeyHook(hook_get_key);
	Video_SetScriptMode(true);
	Video_SetDosGetStrHook(hook_dos_get_str);
	Video_SetDosGetLongHook(hook_dos_get_long);
	Input_SetGetStringChoiceHook(hook_get_string_choice);
}
