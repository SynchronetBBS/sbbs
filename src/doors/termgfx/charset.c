// charset.c -- see charset.h.

#include "charset.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "genwrap.h"    // xpdev: stricmp()
#include "ini_file.h"   // xpdev: iniReadFile / iniGetString

/* The glyphs, spelled as hex escapes rather than literal bytes on purpose: a
 * literal 0xC9 in a source file is at the mercy of every editor and tool that
 * touches it (and has been mangled to a UTF-8 replacement char in this tree
 * before). An escape survives anything. */
static const termgfx_box_t box_double_cp437 = {
	"\xC9", "\xBB", "\xC8", "\xBC",     /* corners */
	"\xCD", "\xBA",                     /* h, v    */
	"\xCC", "\xB9"                      /* tees    */
};

static const termgfx_box_t box_single_cp437 = {
	"\xDA", "\xBF", "\xC0", "\xD9",
	"\xC4", "\xB3",
	"\xC3", "\xB4"
};

static const termgfx_box_t box_double_utf8 = {
	"\xE2\x95\x94", "\xE2\x95\x97", "\xE2\x95\x9A", "\xE2\x95\x9D",   /* U+2554 2557 255A 255D */
	"\xE2\x95\x90", "\xE2\x95\x91",                                   /* U+2550 2551 */
	"\xE2\x95\xA0", "\xE2\x95\xA3"                                    /* U+2560 2563 */
};

static const termgfx_box_t box_single_utf8 = {
	"\xE2\x94\x8C", "\xE2\x94\x90", "\xE2\x94\x94", "\xE2\x94\x98",   /* U+250C 2510 2514 2518 */
	"\xE2\x94\x80", "\xE2\x94\x82",                                   /* U+2500 2502 */
	"\xE2\x94\x9C", "\xE2\x94\xA4"                                    /* U+251C 2524 */
};

const termgfx_box_t *termgfx_box_double(termgfx_charset_t cs)
{
	return (cs == TERMGFX_UTF8) ? &box_double_utf8 : &box_double_cp437;
}

const termgfx_box_t *termgfx_box_single(termgfx_charset_t cs)
{
	return (cs == TERMGFX_UTF8) ? &box_single_utf8 : &box_single_cp437;
}

termgfx_charset_t termgfx_client_charset(void)
{
	static termgfx_charset_t cs;
	static int               known;

	const char *             node;
	char                     path[512];
	char                     chars[INI_MAX_VALUE_LEN] = "";
	FILE *                   f;
	str_list_t               ini;

	if (known)
		return cs;
	known = 1;
	cs    = TERMGFX_CP437;

	node = getenv("SBBSNODE");
	if (node == NULL || *node == '\0')
		return cs;
	snprintf(path, sizeof path, "%s/terminal.ini", node);
	if ((f = fopen(path, "r")) == NULL)
		return cs;

	ini = iniReadFile(f);
	fclose(f);
	iniGetString(ini, ROOT_SECTION, "chars", "", chars);
	strListFree(&ini);

	if (stricmp(chars, "utf-8") == 0 || stricmp(chars, "utf8") == 0)
		cs = TERMGFX_UTF8;
	return cs;
}
