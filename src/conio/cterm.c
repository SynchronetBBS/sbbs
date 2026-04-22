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

/*
 * Coordinate systems in use here... text is 1-based.
 * "pixel"   the ciolib screen in pixels.  sixel commands happen here.
 * "screen"  the ciolib screen... gettext/puttext/movetext and mouse
 *           are in this space.
 * "absterm" position inside the full terminal area.  These coordinates
 *           are used when origin mode is off.
 * "term"    The scrolling region.  These coordinates are used when origin
 *           mode is on.
 * "curr"    Either absterm or term depending on the state of origin mode
 *           This is what's sent to and received from the remote, and
 *           matches the CTerm window.
 */

#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
 #include <malloc.h>	/* alloca() on Win32 */
 #include <xpprintf.h>	/* asprintf() on Win32 */
#endif

#include <genwrap.h>
#include <xpbeep.h>
#include <link_list.h>
#ifdef __unix__
	#include <xpsem.h>
#endif
#include <threadwrap.h>

#include "ciolib.h"

#include "cterm.h"
#include "cterm_internal.h"
#include "cterm_ecma48.h"
#include "cterm_dec.h"
#include "cterm_cterm.h"
#include "cterm_vt52.h"
#include "cterm_atascii.h"
#include "cterm_petscii.h"
#include "cterm_prestel.h"
#include "vidmodes.h"
#include "base64.h"
#include <crc16.h>

/*
 * Disable warnings for well-defined standard C operations
 * 4244 appears to be for implicit conversions
 * warning C4244: '=': conversion from 'uint64_t' to 'int', possible loss of data
 * 4267 is the same thing, but from size_t.
 * 4018 warns when comparing signed and unsigned if the signed is implicitly
 *      converted to unsigned (so more sane than -Wsign-compare)
 */
#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

// Hack for Visual Studio 2022
#ifndef UINT64_MAX
#define UINT64_MAX UINT64_C(-1)
#endif

#define	BUFSIZE	2048

/* cterm_accumulate_trailing is defined below and is installed from
 * dispatch handlers that consume N raw bytes (VT52 ESC Y / b / c, BEEB
 * VDU 23 / 28) — declared in cterm_internal.h so those handlers can
 * reach it. */

const int cterm_tabs[]={1,9,17,25,33,41,49,57,65,73,81,89,97,105,113,121,129,137,145};

/*
 * Send a response to the host.
 * If response_cb is set, calls it directly; otherwise the response is dropped.
 */
void
cterm_respond(struct cterminal *cterm, const char *data, size_t len)
{
	if (cterm->response_cb)
		cterm->response_cb(data, len, cterm->response_cbdata);
}

/* Printf-style wrapper for cterm_respond */
void
cterm_respond_printf(struct cterminal *cterm, const char *fmt, ...)
{
	char tmp[1024];
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	if (n > 0)
		cterm_respond(cterm, tmp, n);
}

/* Key-to-sequence mapping for emulation-specific input translation.
 * Tables are sorted by key value for bsearch(). */
struct key_mapping {
	int         key;
	const char *seq;
	int         len;
};

#define KEY_MAP_SIZE(table) (int)(sizeof(table) / sizeof((table)[0]))
#define LOOKUP_KEY(table, key, len) \
	lookup_key((table), KEY_MAP_SIZE(table), (key), (len))

static const struct key_mapping atascii_keys[] = {
	{253,                      "\x07",        1},	/* Undo Unicode: Atascii beep -> ^G */
	{CIO_KEY_UP,               "\x1c",        1},
	{CIO_KEY_LEFT,             "\x1e",        1},
	{CIO_KEY_RIGHT,            "\x1f",        1},
	{CIO_KEY_DOWN,             "\x1d",        1},
	{CIO_KEY_IC,               "\xff",        1},
	{CIO_KEY_DC,               "\x7e",        1},	/* "Delete" key */
};

static const struct key_mapping petscii_keys[] = {
	{8,                        "\x14",        1},
	{'\n',                     "\x0d",        1},
	{'\r',                     "\x0d",        1},
	{CIO_KEY_F(1),             "\x85",        1},
	{CIO_KEY_F(2),             "\x89",        1},
	{CIO_KEY_F(3),             "\x86",        1},
	{CIO_KEY_F(4),             "\x8a",        1},
	{CIO_KEY_F(5),             "\x87",        1},
	{CIO_KEY_F(6),             "\x8b",        1},
	{CIO_KEY_F(7),             "\x88",        1},
	{CIO_KEY_F(8),             "\x8c",        1},
	{CIO_KEY_HOME,             "\x13",        1},
	{CIO_KEY_UP,               "\x91",        1},
	{CIO_KEY_LEFT,             "\x9d",        1},
	{CIO_KEY_RIGHT,            "\x1d",        1},
	{CIO_KEY_END,              "\x93",        1},	/* Clear / Shift-Home */
	{CIO_KEY_DOWN,             "\x11",        1},
	{CIO_KEY_IC,               "\x94",        1},
	{CIO_KEY_DC,               "\x14",        1},
};

static const struct key_mapping prestel_keys[] = {
	{8,                        "\x7f",        1},
	{9,                        "\x09",        1},
	{10,                       "\x0d",        1},	/* LF sends CR */
	{13,                       "_",           1},	/* CR sends _ */
	{'#',                      "_",           1},
	{'_',                      "`",           1},
	{'`',                      "#",           1},
	{CIO_KEY_SHIFT_END,        "\x9b",        1},	/* Copy */
	{CIO_KEY_SHIFT_DOWN,       "\x9e",        1},
	{CIO_KEY_SHIFT_LEFT,       "\x9c",        1},
	{CIO_KEY_SHIFT_RIGHT,      "\x9d",        1},
	{CIO_KEY_SHIFT_UP,         "\x9f",        1},
	{CIO_KEY_F(7),             "\x1b",        1},
	{CIO_KEY_HOME,             "\x1f",        1},
	{CIO_KEY_UP,               "\x8f",        1},
	{CIO_KEY_LEFT,             "\x8c",        1},
	{CIO_KEY_RIGHT,            "\x8d",        1},
	{CIO_KEY_DOWN,             "\x8e",        1},
	{CIO_KEY_DC,               "\x7f",        1},
	{CIO_KEY_SHIFT_F(1),       "\x91",        1},
	{CIO_KEY_SHIFT_F(2),       "\x92",        1},
	{CIO_KEY_SHIFT_F(3),       "\x93",        1},
	{CIO_KEY_SHIFT_F(4),       "\x94",        1},
	{CIO_KEY_SHIFT_F(5),       "\x95",        1},
	{CIO_KEY_SHIFT_F(6),       "\x96",        1},
	{CIO_KEY_SHIFT_F(7),       "\x97",        1},
	{CIO_KEY_SHIFT_F(8),       "\x98",        1},
	{CIO_KEY_SHIFT_F(9),       "\x99",        1},
	{CIO_KEY_SHIFT_F(10),      "\x90",        1},	/* F0 */
	{CIO_KEY_CTRL_F(1),        "\xa1",        1},
	{CIO_KEY_CTRL_F(2),        "\xa2",        1},
	{CIO_KEY_CTRL_F(3),        "\xa3",        1},
	{CIO_KEY_CTRL_F(4),        "\xa4",        1},
	{CIO_KEY_CTRL_F(5),        "\xa5",        1},
	{CIO_KEY_CTRL_F(6),        "\xa6",        1},
	{CIO_KEY_CTRL_F(7),        "\xa7",        1},
	{CIO_KEY_CTRL_F(8),        "\xa8",        1},
	{CIO_KEY_CTRL_F(9),        "\xa9",        1},
	{CIO_KEY_CTRL_F(10),       "\xa0",        1},	/* F0 */
	{CIO_KEY_CTRL_LEFT,        "\xac",        1},
	{CIO_KEY_CTRL_RIGHT,       "\xad",        1},
	{CIO_KEY_CTRL_END,         "\xab",        1},	/* Copy */
	{CIO_KEY_CTRL_UP,          "\xaf",        1},
	{CIO_KEY_CTRL_DOWN,        "\xae",        1},
};

static const struct key_mapping beeb_keys[] = {
	{8,                        "\x7f",        1},
	{CIO_KEY_SHIFT_END,        "\x9b",        1},	/* Copy */
	{CIO_KEY_SHIFT_DOWN,       "\x9e",        1},
	{CIO_KEY_SHIFT_LEFT,       "\x9c",        1},
	{CIO_KEY_SHIFT_RIGHT,      "\x9d",        1},
	{CIO_KEY_SHIFT_UP,         "\x9f",        1},
	{CIO_KEY_F(7),             "\x1b",        1},
	{CIO_KEY_HOME,             "\x1f",        1},
	{CIO_KEY_UP,               "\x8f",        1},
	{CIO_KEY_LEFT,             "\x8c",        1},
	{CIO_KEY_RIGHT,            "\x8d",        1},
	{CIO_KEY_DOWN,             "\x8e",        1},
	{CIO_KEY_DC,               "\x7f",        1},
	{CIO_KEY_SHIFT_F(1),       "\x91",        1},
	{CIO_KEY_SHIFT_F(2),       "\x92",        1},
	{CIO_KEY_SHIFT_F(3),       "\x93",        1},
	{CIO_KEY_SHIFT_F(4),       "\x94",        1},
	{CIO_KEY_SHIFT_F(5),       "\x95",        1},
	{CIO_KEY_SHIFT_F(6),       "\x96",        1},
	{CIO_KEY_SHIFT_F(7),       "\x97",        1},
	{CIO_KEY_SHIFT_F(8),       "\x98",        1},
	{CIO_KEY_SHIFT_F(9),       "\x99",        1},
	{CIO_KEY_SHIFT_F(10),      "\x90",        1},	/* F0 */
	{CIO_KEY_CTRL_F(1),        "\xa1",        1},
	{CIO_KEY_CTRL_F(2),        "\xa2",        1},
	{CIO_KEY_CTRL_F(3),        "\xa3",        1},
	{CIO_KEY_CTRL_F(4),        "\xa4",        1},
	{CIO_KEY_CTRL_F(5),        "\xa5",        1},
	{CIO_KEY_CTRL_F(6),        "\xa6",        1},
	{CIO_KEY_CTRL_F(7),        "\xa7",        1},
	{CIO_KEY_CTRL_F(8),        "\xa8",        1},
	{CIO_KEY_CTRL_F(9),        "\xa9",        1},
	{CIO_KEY_CTRL_F(10),       "\xa0",        1},	/* F0 */
	{CIO_KEY_CTRL_LEFT,        "\xac",        1},
	{CIO_KEY_CTRL_RIGHT,       "\xad",        1},
	{CIO_KEY_CTRL_END,         "\xab",        1},	/* Copy */
	{CIO_KEY_CTRL_UP,          "\xaf",        1},
	{CIO_KEY_CTRL_DOWN,        "\xae",        1},
};

static const struct key_mapping vt52_keys[] = {
	{CIO_KEY_F(1),             "\033P",       2},
	{CIO_KEY_F(2),             "\033Q",       2},
	{CIO_KEY_F(3),             "\033R",       2},
	{CIO_KEY_UP,               "\033A",       2},
	{CIO_KEY_LEFT,             "\033D",       2},
	{CIO_KEY_RIGHT,            "\033C",       2},
	{CIO_KEY_DOWN,             "\033B",       2},
};

static const struct key_mapping ansi_keys[] = {
	{CIO_KEY_BACKTAB,          "\033[Z",      3},
	{CIO_KEY_F(1),             "\033[11~",    5},
	{CIO_KEY_F(2),             "\033[12~",    5},
	{CIO_KEY_F(3),             "\033[13~",    5},
	{CIO_KEY_F(4),             "\033[14~",    5},
	{CIO_KEY_F(5),             "\033[15~",    5},
	{CIO_KEY_F(6),             "\033[17~",    5},
	{CIO_KEY_F(7),             "\033[18~",    5},
	{CIO_KEY_F(8),             "\033[19~",    5},
	{CIO_KEY_F(9),             "\033[20~",    5},
	{CIO_KEY_F(10),            "\033[21~",    5},
	{CIO_KEY_HOME,             "\033[H",      3},
	{CIO_KEY_UP,               "\033[A",      3},
	{CIO_KEY_PPAGE,            "\033[V",      3},
	{CIO_KEY_LEFT,             "\033[D",      3},
	{CIO_KEY_RIGHT,            "\033[C",      3},
	{CIO_KEY_END,              "\033[K",      3},
#ifdef CIO_KEY_SELECT
	{CIO_KEY_SELECT,           "\033[K",      3},
#endif
	{CIO_KEY_DOWN,             "\033[B",      3},
	{CIO_KEY_NPAGE,            "\033[U",      3},
	{CIO_KEY_IC,               "\033[@",      3},
	{CIO_KEY_SHIFT_F(1),       "\033[11;2~",  7},
	{CIO_KEY_SHIFT_F(2),       "\033[12;2~",  7},
	{CIO_KEY_SHIFT_F(3),       "\033[13;2~",  7},
	{CIO_KEY_SHIFT_F(4),       "\033[14;2~",  7},
	{CIO_KEY_SHIFT_F(5),       "\033[15;2~",  7},
	{CIO_KEY_SHIFT_F(6),       "\033[17;2~",  7},
	{CIO_KEY_SHIFT_F(7),       "\033[18;2~",  7},
	{CIO_KEY_SHIFT_F(8),       "\033[19;2~",  7},
	{CIO_KEY_SHIFT_F(9),       "\033[20;2~",  7},
	{CIO_KEY_SHIFT_F(10),      "\033[21;2~",  7},
	{CIO_KEY_CTRL_F(1),        "\033[11;5~",  7},
	{CIO_KEY_CTRL_F(2),        "\033[12;5~",  7},
	{CIO_KEY_CTRL_F(3),        "\033[13;5~",  7},
	{CIO_KEY_CTRL_F(4),        "\033[14;5~",  7},
	{CIO_KEY_CTRL_F(5),        "\033[15;5~",  7},
	{CIO_KEY_CTRL_F(6),        "\033[17;5~",  7},
	{CIO_KEY_CTRL_F(7),        "\033[18;5~",  7},
	{CIO_KEY_CTRL_F(8),        "\033[19;5~",  7},
	{CIO_KEY_CTRL_F(9),        "\033[20;5~",  7},
	{CIO_KEY_CTRL_F(10),       "\033[21;5~",  7},
	{CIO_KEY_ALT_F(1),         "\033[11;3~",  7},
	{CIO_KEY_ALT_F(2),         "\033[12;3~",  7},
	{CIO_KEY_ALT_F(3),         "\033[13;3~",  7},
	{CIO_KEY_ALT_F(4),         "\033[14;3~",  7},
	{CIO_KEY_ALT_F(5),         "\033[15;3~",  7},
	{CIO_KEY_ALT_F(6),         "\033[17;3~",  7},
	{CIO_KEY_ALT_F(7),         "\033[18;3~",  7},
	{CIO_KEY_ALT_F(8),         "\033[19;3~",  7},
	{CIO_KEY_ALT_F(9),         "\033[20;3~",  7},
	{CIO_KEY_ALT_F(10),        "\033[21;3~",  7},
	{CIO_KEY_F(11),            "\033[23~",    5},
	{CIO_KEY_F(12),            "\033[24~",    5},
	{CIO_KEY_SHIFT_F(11),      "\033[23;2~",  7},
	{CIO_KEY_SHIFT_F(12),      "\033[24;2~",  7},
	{CIO_KEY_CTRL_F(11),       "\033[23;5~",  7},
	{CIO_KEY_CTRL_F(12),       "\033[24;5~",  7},
	{CIO_KEY_ALT_F(11),        "\033[23;3~",  7},
	{CIO_KEY_ALT_F(12),        "\033[24;3~",  7},
};

static int
key_mapping_cmp(const void *a, const void *b)
{
	return ((const struct key_mapping *)a)->key
	     - ((const struct key_mapping *)b)->key;
}

static const char *
lookup_key(const struct key_mapping *map, int count, int key, int *len)
{
	struct key_mapping needle = {key, NULL, 0};
	const struct key_mapping *found;

	found = bsearch(&needle, map, count, sizeof(*map), key_mapping_cmp);
	if (found) {
		*len = found->len;
		return found->seq;
	}
	return NULL;
}

static void
verify_key_tables(void)
{
#define CHECK_SORTED(table) do {					\
	for (int i_ = 1; i_ < KEY_MAP_SIZE(table); i_++)		\
		assert((table)[i_ - 1].key < (table)[i_].key);		\
} while (0)

	CHECK_SORTED(atascii_keys);
	CHECK_SORTED(petscii_keys);
	CHECK_SORTED(prestel_keys);
	CHECK_SORTED(beeb_keys);
	CHECK_SORTED(vt52_keys);
	CHECK_SORTED(ansi_keys);
#undef CHECK_SORTED
}

static void
toggle_prestel_reveal(void)
{
	cio_api.options ^= CONIO_OPT_PRESTEL_REVEAL;
	struct ciolib_screen *savscrn = savescreen();
	ciolib_vmem_puttext(1, 1, savscrn->text_info.screenwidth,
	    savscrn->text_info.screenheight, savscrn->vmem);
	freescreen(savscrn);
}

int
cterm_encode_key(struct cterminal *cterm, int key)
{
	unsigned char             ch[2];
	int                       slen;
	const char               *seq;
	const struct key_mapping *table;
	int                       table_count;
	int                       raw_lo = 0;
	int                       raw_hi = 256;

	/* Doorway mode: NUL + scancode.  Any high-bit scancode
	 * (key > 0xff with low byte 0) goes through. */
	if (cterm->doorway_mode && (key > 0xff) && ((key & 0xff) == 0)) {
		ch[0] = 0;
		ch[1] = key >> 8;
		cterm_respond(cterm, (const char *)ch, 2);
		return 2;
	}

	switch (cterm->emulation) {
		case CTERM_EMULATION_ATASCII:
			if (key == 96) {
				cterm->atascii_inverse = !cterm->atascii_inverse;
				return 0;
			}
			table = atascii_keys;
			table_count = KEY_MAP_SIZE(atascii_keys);
			break;
		case CTERM_EMULATION_PETASCII:
			table = petscii_keys;
			table_count = KEY_MAP_SIZE(petscii_keys);
			break;
		case CTERM_EMULATION_PRESTEL:
		case CTERM_EMULATION_BEEB:
			/* PRESTEL and BEEB: F3 or PgDn toggles reveal. */
			if (key == CIO_KEY_F(3) || key == CIO_KEY_NPAGE) {
				toggle_prestel_reveal();
				return 0;
			}
			switch (cterm->emulation) {
				case CTERM_EMULATION_PRESTEL:
					table = prestel_keys;
					table_count = KEY_MAP_SIZE(prestel_keys);
					raw_lo = 32;
					raw_hi = 128;
					break;
				default:
					table = beeb_keys;
					table_count = KEY_MAP_SIZE(beeb_keys);
					raw_hi = 128;
					break;
			}
			break;
		default:
			/* VT52 and ANSI-BBS: DEL and BS respect DECBKM. */
			if (key == CIO_KEY_DC) {
				if (cterm->extattr & CTERM_EXTATTR_DECBKM) {
					cterm_respond(cterm, "\x7f", 1);
					return 1;
				}
				cterm_respond(cterm, "\x1b[3~", 4);
				return 4;
			}
			if (key == '\b') {
				ch[0] = (cterm->extattr & CTERM_EXTATTR_DECBKM) ? '\b' : '\x7f';
				cterm_respond(cterm, (const char *)ch, 1);
				return 1;
			}
			switch (cterm->emulation) {
				case CTERM_EMULATION_ATARIST_VT52:
					table = vt52_keys;
					table_count = KEY_MAP_SIZE(vt52_keys);
					break;
				default: /* ANSI-BBS */
					table = ansi_keys;
					table_count = KEY_MAP_SIZE(ansi_keys);
					break;
			}
			break;
	}

	seq = lookup_key(table, table_count, key, &slen);
	if (seq) {
		cterm_respond(cterm, seq, slen);
		return slen;
	}
	if (key >= raw_lo && key < raw_hi) {
		ch[0] = key;
		cterm_respond(cterm, (const char *)ch, 1);
		return 1;
	}
	return 0;
}

bool
cterm_atascii_inverse(const struct cterminal *cterm)
{
	return cterm->atascii_inverse;
}

void
cterm_clear_selection(struct cterminal *cterm)
{
	cterm->ssa_row = 0;
	cterm->ssa_col = 0;
	cterm->esa_row = 0;
	cterm->esa_col = 0;
}

static void
cterm_hyperlink_gc(ciolib_hyperlink_mark_fn mark_live, int max_live, void *cbdata)
{
	struct cterminal *cterm = cbdata;
	int marked = 0;

	if (cterm->scrollback == NULL || cterm->backfilled == 0)
		return;

	uint64_t start = xp_timer64();
	int line = cterm->backpos;
	for (int row = 0; row < cterm->backfilled; row++) {
		line--;
		if (line < 0)
			line = cterm->backlines - 1;
		struct vmem_cell *cells = &cterm->scrollback[line * cterm->backwidth];
		for (int col = 0; col < cterm->backwidth; col++) {
			if (cells[col].hyperlink_id) {
				if (mark_live(cells[col].hyperlink_id))
					marked++;
				if (marked >= max_live)
					return;
			}
		}
		if (xp_timer64() - start > 3)
			return;
	}
}

void
cterm_coord_conv_xy(struct cterminal *cterm, enum cterm_coordinates from_coord,
    enum cterm_coordinates to_coord, int *x, int *y)
{
	if (from_coord == to_coord)
		return;

	if (x) {
		if (from_coord == CTERM_COORD_CURR) {
			if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
				from_coord = CTERM_COORD_TERM;
			else
				from_coord = CTERM_COORD_ABSTERM;
		}
		switch(from_coord) {
			case CTERM_COORD_SCREEN:
				break;
			case CTERM_COORD_TERM:
				*x += cterm->left_margin - 1;
				// Fall-through
			case CTERM_COORD_ABSTERM:
				*x += cterm->x - 1;
				break;
			default:
				// Silence warnings
				break;
		}
		if (to_coord == CTERM_COORD_CURR) {
			if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
				to_coord = CTERM_COORD_TERM;
			else
				to_coord = CTERM_COORD_ABSTERM;
		}
		switch(to_coord) {
			case CTERM_COORD_SCREEN:
				break;
			case CTERM_COORD_TERM:
				*x -= cterm->left_margin - 1;
				// Fall-through
			case CTERM_COORD_ABSTERM:
				*x -= cterm->x - 1;
				break;
			default:
				// Silence warnings
				break;
		}
	}
	if (y) {
		if (from_coord == CTERM_COORD_CURR) {
			if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
				from_coord = CTERM_COORD_TERM;
			else
				from_coord = CTERM_COORD_ABSTERM;
		}
		switch(from_coord) {
			case CTERM_COORD_SCREEN:
				break;
			case CTERM_COORD_TERM:
				*y += cterm->top_margin - 1;
				// Fall-through
			case CTERM_COORD_ABSTERM:
				*y += cterm->y - 1;
				break;
			default:
				// Silence warnings
				break;
		}
		if (to_coord == CTERM_COORD_CURR) {
			if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
				to_coord = CTERM_COORD_TERM;
			else
				to_coord = CTERM_COORD_ABSTERM;
		}
		switch(to_coord) {
			case CTERM_COORD_SCREEN:
				break;
			case CTERM_COORD_TERM:
				*y -= cterm->top_margin - 1;
				// Fall-through
			case CTERM_COORD_ABSTERM:
				*y -= cterm->y - 1;
				break;
			default:
				// Silence warnings
				break;
		}
	}
}

void
cterm_coord_get_xy(struct cterminal *cterm, enum cterm_coordinates coord, int *x, int *y)
{
	if (x)
		*x = wherex();
	if (y)
		*y = wherey();

	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, coord, x, y);
}

void
setwindow(struct cterminal *cterm)
{
	int col, row, max_col, max_row;

	col = CURR_MINX;
	row = CURR_MINY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &col, &row);
	max_col = CURR_MAXX;
	max_row = CURR_MAXY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &max_col, &max_row);
	window(col, row, max_col, max_row);
}

/* Adjust cterm->height in place without a full reinit.  Used by the
 * DECSSDT callback when toggling between a statusless display and one
 * with a reserved bottom row.  Preserves existing content; on growth
 * blanks the newly-exposed row(s) with the current attribute; on
 * shrink clamps the cursor to the new last row.  bottom_margin follows
 * the old implied bottom — a user-configured DECSTBM region narrower
 * than the screen is left alone. */
void
cterm_resize_rows(struct cterminal *cterm, int new_rows)
{
	int old_rows = cterm->height;
	int tx, ty;

	if (new_rows < 1 || new_rows == old_rows)
		return;

	/* Capture cursor in terminal coords before changing geometry; ciolib
	 * window() resets the window-relative cursor, so we always restore. */
	TERM_XY(&tx, &ty);
	if (ty > new_rows)
		ty = new_rows;
	if (tx > cterm->width)
		tx = cterm->width;

	if (cterm->bottom_margin == old_rows || cterm->bottom_margin > new_rows)
		cterm->bottom_margin = new_rows;
	if (cterm->top_margin > new_rows)
		cterm->top_margin = 1;

	cterm->height = new_rows;
	setwindow(cterm);
	cterm_gotoxy(cterm, tx, ty);

	if (new_rows > old_rows) {
		struct vmem_cell *buf = calloc(cterm->width, sizeof(*buf));
		int i, r, sx, sy, ex;

		if (buf) {
			buf[0].ch = ' ';
			buf[0].legacy_attr = cterm->attr;
			buf[0].fg = cterm->fg_color;
			buf[0].bg = cterm->bg_color;
			buf[0].font = ciolib_attrfont(cterm->attr);
			buf[0].hyperlink_id = 0;
			for (i = 1; i < cterm->width; i++)
				buf[i] = buf[0];
			for (r = old_rows + 1; r <= new_rows; r++) {
				sx = 1;
				sy = r;
				ex = cterm->width;
				cterm_coord_conv_xy(cterm, CTERM_COORD_TERM,
				    CTERM_COORD_SCREEN, &sx, &sy);
				cterm_coord_conv_xy(cterm, CTERM_COORD_TERM,
				    CTERM_COORD_SCREEN, &ex, NULL);
				vmem_puttext(sx, sy, ex, sy, buf);
			}
			free(buf);
		}
	}
}

static void
set_attr(struct cterminal *cterm, unsigned char colour, bool bg)
{
	if (bg) {
		cterm->attr &= 0x8F;
		cterm->attr |= ((colour & 0x07) << 4);
	}
	else {
		cterm->attr &= 0xF8;
		cterm->attr |= (colour & 0x07);
	}
	attr2palette(cterm->attr, bg ? NULL : &cterm->fg_color, bg ? &cterm->bg_color : NULL);
	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB) {
		if (cterm->extattr & CTERM_EXTATTR_PRESTEL_DOUBLE_HEIGHT)
			cterm->bg_color |= CIOLIB_BG_DOUBLE_HEIGHT;
		cterm->bg_color |= CIOLIB_BG_PRESTEL;
		if (cterm->emulation == CTERM_EMULATION_PRESTEL)
			cterm->bg_color |= CIOLIB_BG_PRESTEL_TERMINAL;
	}
	if (bg)
		FREE_AND_NULL(cterm->bg_tc_str);
	else
		FREE_AND_NULL(cterm->fg_tc_str);
}

void
cterm_set_fgattr(struct cterminal *cterm, unsigned char colour)
{
	set_attr(cterm, colour, false ^ cterm->negative);
}

void
cterm_set_bgattr(struct cterminal *cterm, unsigned char colour)
{
	set_attr(cterm, colour, true ^ cterm->negative);
}

void
cterm_gotoxy(struct cterminal *cterm, int x, int y)
{
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &x, &y);
	gotoxy(x, y);
	ABS_XY(&cterm->xpos, &cterm->ypos);
	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB)
		cterm_prestel_get_state(cterm);
}

/*
 * cterm_crpos() returns the ABSTERM column where a CR would place the
 * cursor for the current row.  The logic mirrors the CR path in
 * cterm_adjust_currpos() above: inside the scroll region CR lands on
 * left_margin; outside it clamps to left_margin only when the cursor
 * was already right of it, otherwise it stays at ABSTERM column 1.
 *
 * Called from ripper.c's cb_accept_introducer() as the cursor-position
 * fallback for the RIP `!` BOL predicate.
 */
CIOLIBEXPORT int
cterm_crpos(struct cterminal *cterm)
{
	int tx, ty, ax, ay;

	TERM_XY(&tx, &ty);
	ABS_XY(&ax, &ay);

	if (tx >= TERM_MINX && tx <= TERM_MAXX &&
	    ty >= TERM_MINY && ty <= TERM_MAXY)
		return cterm->left_margin;
	if (ax >= cterm->left_margin)
		return cterm->left_margin;
	return ABS_MINX;
}

void
cterm_insert_tabstop(struct cterminal *cterm, int pos)
{
	int i;
	int *new_stops;

	for (i = 0; i < cterm->tab_count && cterm->tabs[i] < pos; i++);
	if (cterm->tabs[i] == pos)
		return;

	new_stops = realloc(cterm->tabs, (cterm->tab_count + 1) * sizeof(cterm->tabs[0]));
	if (new_stops == NULL)
		return;
	cterm->tabs = new_stops;
	if (i < cterm->tab_count)
		memmove(&cterm->tabs[i + 1], &cterm->tabs[i], (cterm->tab_count - i) * sizeof(cterm->tabs[0]));
	cterm->tabs[i] = pos;
	cterm->tab_count++;
}

void
cterm_delete_tabstop(struct cterminal *cterm, int pos)
{
	int i;

	for (i = 0; i < cterm->tab_count && cterm->tabs[i] <= pos; i++) {
		if (cterm->tabs[i] == pos) {
			memmove(&cterm->tabs[i], &cterm->tabs[i+1], (cterm->tab_count - i - 1) * sizeof(cterm->tabs[0]));
			cterm->tab_count--;
			return;
		}
	}
	return;
}

void
cterm_insert_vtabstop(struct cterminal *cterm, int line)
{
	int i;
	int *new_stops;

	for (i = 0; i < cterm->vtab_count && cterm->vtabs[i] < line; i++);
	if (i < cterm->vtab_count && cterm->vtabs[i] == line)
		return;

	new_stops = realloc(cterm->vtabs, (cterm->vtab_count + 1) * sizeof(cterm->vtabs[0]));
	if (new_stops == NULL)
		return;
	cterm->vtabs = new_stops;
	if (i < cterm->vtab_count)
		memmove(&cterm->vtabs[i + 1], &cterm->vtabs[i], (cterm->vtab_count - i) * sizeof(cterm->vtabs[0]));
	cterm->vtabs[i] = line;
	cterm->vtab_count++;
}

void
cterm_delete_vtabstop(struct cterminal *cterm, int line)
{
	int i;

	for (i = 0; i < cterm->vtab_count && cterm->vtabs[i] <= line; i++) {
		if (cterm->vtabs[i] == line) {
			memmove(&cterm->vtabs[i], &cterm->vtabs[i+1], (cterm->vtab_count - i - 1) * sizeof(cterm->vtabs[0]));
			cterm->vtab_count--;
			return;
		}
	}
}

/*
 * Fill a rectangular area with a character using current attributes.
 * Coordinates are in SCREEN space.  Caller must validate and clamp.
 */
void
cterm_fill_rect(struct cterminal *cterm, int left, int top, int right, int bottom, uint8_t ch)
{
	int width = right - left + 1;
	int height = bottom - top + 1;
	struct vmem_cell *buf;
	int i, r;

	if (width <= 0 || height <= 0)
		return;
	buf = calloc(width, sizeof(*buf));
	if (buf == NULL)
		return;
	/* Initialize one row of cells with current attributes, no hyperlink */
	buf[0].ch = ch;
	buf[0].legacy_attr = cterm->attr;
	buf[0].fg = cterm->fg_color;
	buf[0].bg = cterm->bg_color;
	buf[0].font = ciolib_attrfont(cterm->attr);
	buf[0].hyperlink_id = 0;
	for (i = 1; i < width; i++)
		buf[i] = buf[0];
	/* Write row by row */
	for (r = top; r <= bottom; r++)
		vmem_puttext(left, r, right, r, buf);
	free(buf);
}

void
cterm_clreol(struct cterminal *cterm)
{
	int x, y;
	int rm;
	struct vmem_cell *buf;
	int i;
	int width;

	CURR_XY(&x, &y);
	rm = CURR_MAXX;

	width = rm - x + 1;
	if (width < 1)
		return;
	buf = calloc(width, sizeof(*buf));
	if (!buf)
		return;
	for (i = 0; i < width; i++) {
		if (i > 0)
			buf[i] = buf[0];
		else {
			buf[i].ch = ' ';
			buf[i].legacy_attr = cterm->attr;
			buf[i].fg = cterm->fg_color;
			buf[i].bg = cterm->bg_color;
			buf[i].font = ciolib_attrfont(cterm->attr);
			buf[i].hyperlink_id = 0;
		}
	}
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &x, &y);
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &rm, NULL);
	vmem_puttext(x, y, rm, y, buf);
	free(buf);
}

void
cterm_clrblk(struct cterminal *cterm, int sx, int sy, int ex, int ey)
{
	int i;
	struct vmem_cell *buf;
	int chars = (ex - sx + 1) * (ey - sy + 1);

	buf = calloc(chars, sizeof(*buf));
	if (!buf)
		return;
	for (i = 0; i < chars ; i++) {
		if (i > 0)
			buf[i] = buf[0];
		else {
			buf[i].ch = ' ';
			buf[i].legacy_attr = cterm->attr;
			buf[i].fg = cterm->fg_color;
			buf[i].bg = cterm->bg_color;
			buf[i].font = ciolib_attrfont(cterm->attr);
			buf[i].hyperlink_id = 0;
		}
	}
	vmem_puttext(sx, sy, ex, ey, buf);
	free(buf);
}

void
cterm_scrolldown(struct cterminal *cterm)
{
	int minx = TERM_MINX;
	int miny = TERM_MINY;
	int maxx = TERM_MAXX;
	int maxy = TERM_MAXY;
	int x,y;
	struct vmem_cell fill = {
		.ch = ' ',
		.legacy_attr = cterm->attr,
		.fg = cterm->fg_color,
		.bg = cterm->bg_color,
		.font = ciolib_attrfont(cterm->attr),
		.hyperlink_id = 0,
	};

	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &minx, &miny);
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &maxx, &maxy);
	CURR_XY(&x, &y);
	movetext_clear(minx, miny, maxx, maxy - 1, minx, miny + 1, &fill);
	gotoxy(x, y);
}

static void
cterm_line_to_scrollback(struct cterminal *cterm, int row)
{
	if(cterm->scrollback!=NULL) {
		int getw;

		cterm->backfilled++;
		if (cterm->backfilled > cterm->backlines) {
			cterm->backfilled--;
			cterm->backstart++;
			if (cterm->backstart == cterm->backlines)
				cterm->backstart = 0;
		}
		getw = cterm->backwidth;
		if (getw > cterm->width)
			getw = cterm->width;
		if (getw < cterm->backwidth) {
			memset(cterm->scrollback + cterm->backpos * cterm->backwidth, 0, sizeof(*cterm->scrollback) * cterm->backwidth);
		}
		vmem_gettext(cterm->x, row, cterm->x + getw - 1, row, cterm->scrollback + cterm->backpos * cterm->backwidth);

		cterm->backpos++;
		if (cterm->backpos == cterm->backlines)
			cterm->backpos = 0;
	}
}

void
cterm_scrollup(struct cterminal *cterm)
{
	int minx = TERM_MINX;
	int miny = TERM_MINY;
	int maxx = TERM_MAXX;
	int maxy = TERM_MAXY;
	int x,y;
	struct vmem_cell fill = {
		.ch = ' ',
		.legacy_attr = cterm->attr,
		.fg = cterm->fg_color,
		.bg = cterm->bg_color,
		.font = ciolib_attrfont(cterm->attr),
		.hyperlink_id = 0,
	};

	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &minx, &miny);
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &maxx, &maxy);
	cterm_line_to_scrollback(cterm, miny);
	CURR_XY(&x, &y);
	movetext_clear(minx, miny + 1, maxx, maxy, minx, miny, &fill);
	gotoxy(x, y);
}

void
cterm_cond_scrollup(struct cterminal *cterm)
{
	int x, y;

	TERM_XY(&x, &y);
	if (x >= TERM_MINX && x <= TERM_MAXX &&
	    y >= TERM_MINY && y <= TERM_MAXY)
		cterm_scrollup(cterm);
}

void
cterm_dellines(struct cterminal * cterm, int lines)
{
	int i;
	int minx = TERM_MINX;
	int miny = TERM_MINY;
	int maxx = TERM_MAXX;
	int maxy = TERM_MAXY;
	int sx,sy;
	int x,y;

	if(lines<1)
		return;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &minx, &miny);
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &maxx, &maxy);
	TERM_XY(&x, &y);
	// Can't delete lines outside of window
	if (x < TERM_MINX || x > TERM_MAXX || y < TERM_MINY || y > TERM_MAXY)
		return;
	SCR_XY(&sx, &sy);
	if (lines > maxy - sy + 1) // Delete all the lines... ie: clear screen...
		lines = maxy - sy + 1;
	if (sy + lines <= maxy)
		movetext(minx, sy + lines, maxx, maxy, minx, sy);
	for(i = TERM_MAXY - lines + 1; i <= TERM_MAXY; i++) {
		cterm_gotoxy(cterm, TERM_MINX, i);
		cterm_clreol(cterm);
	}
	cterm_gotoxy(cterm, x, y);
}

void
cterm_clear2bol(struct cterminal * cterm)
{
	struct vmem_cell *buf;
	int i;
	int x, y;
	int minx = CURR_MINX;

	CURR_XY(&x, &y);
	buf = calloc(x, sizeof(*buf));
	if (buf) {
		for(i = 0; i < x; i++) {
			if (i > 0)
				buf[i] = buf[0];
			else {
				buf[i].ch = ' ';
				buf[i].legacy_attr = cterm->attr;
				buf[i].fg = cterm->fg_color;
				buf[i].bg = cterm->bg_color;
				buf[i].font = ciolib_attrfont(cterm->attr);
				buf[i].hyperlink_id = 0;
			}
		}
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &x, &y);
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &minx, NULL);
		vmem_puttext(minx, y, x, y, buf);
		free(buf);
	}
	else {
		fprintf(stderr, "malloc() in cterm_clear2bol()\n");
	}
}

void
cterm_clearscreen(struct cterminal *cterm, char attr)
{
	if(!cterm->started)
		cterm_start(cterm);

	int minx = TERM_MINX;
	int miny = TERM_MINY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &minx, &miny);

	for (int i = 0; i < cterm->height; i++)
		cterm_line_to_scrollback(cterm, miny + i);
	clrscr();
	gotoxy(CURR_MINX, CURR_MINY);
}

/*
 * Parses an ESC sequence and returns it broken down
 */


bool cterm_parse_sub_parameters(struct sub_params *sub, struct cterminal *cterm, int param)
{
	int i;
	char *p;

	sub->param_count = 1;
	sub->param_int = NULL;

	if (param < 0 || param >= cterm->seq_param_count)
		return false;
	for (p = (char *)cterm->seq_param_strs[param]; *p; p++) {
		if (*p == ':') {
			if (sub->param_count == INT_MAX)
				return false;
			sub->param_count++;
		}
	}
	if (sub->param_count == 0)
		return true;
	sub->param_int = malloc(sub->param_count * sizeof(sub->param_int[0]));
	if (sub->param_int == NULL)
		return false;
	p = (char *)cterm->seq_param_strs[param];
	for (i = 0; i < sub->param_count; i++) {
		p++;
		sub->param_int[i] = strtoull(p, &p, 10);
		if (*p != ':' && *p != 0) {
			FREE_AND_NULL(sub->param_int);
			return false;
		}
	}
	return true;
}

void cterm_free_sub_parameters(struct sub_params *sub)
{
	FREE_AND_NULL(sub->param_int);
}

/* ------------------------------------------------------------------------
 * Dispatch-table helpers
 *
 * cterm_seq_dispatch_cmp is the bsearch() comparator used by every
 * accumulator that looks up a completed sequence in a per-emulation
 * dispatch table (cterm_ansi_dispatch[], cterm_st_vt52_dispatch[]).
 * Tables must be sorted ascending by the 21-bit key for this to work.
 *
 * cterm_seq_default fills in a default value for a parameter slot on
 * cterm->seq_param_int[].  seq_feed() stores UINT64_MAX in slots that
 * were explicitly empty (e.g. the first param of "CSI ;5H") and leaves
 * slots past seq_param_count untouched; handlers call this once per
 * parameter that has a non-zero default to promote those sentinel /
 * missing slots to the real default before reading them.
 * ------------------------------------------------------------------------ */

int
cterm_seq_dispatch_cmp(const void *key, const void *entry)
{
	uint32_t k = *(const uint32_t *)key;
	uint32_t e = ((const struct seq_dispatch *)entry)->key;
	return (k > e) - (k < e);
}

void
cterm_seq_default(struct cterminal *cterm, int index, uint64_t val)
{
	if (index < 0 || index >= MAX_SEQ_PARAMS)
		return;
	if (index >= cterm->seq_param_count) {
		for (int i = cterm->seq_param_count; i <= index; i++)
			cterm->seq_param_int[i] = UINT64_MAX;
		cterm->seq_param_count = index + 1;
	}
	if (cterm->seq_param_int[index] == UINT64_MAX)
		cterm->seq_param_int[index] = val;
}

int
cterm_setpalette(struct cterminal *cterm, uint32_t entry, uint16_t r, uint16_t g, uint16_t b)
{
	const uint32_t palette_offset = cio_api.options & CONIO_OPT_EXTENDED_PALETTE ? 16 : 0;
	if (entry == 7 + palette_offset)
		cterm->default_fg_palette = (uint32_t)r >> 8 << 16 | (uint32_t)g >> 8 << 8 | (uint32_t)b >> 8;
	else if (entry == palette_offset)
		cterm->default_bg_palette = (uint32_t)r >> 8 << 16 | (uint32_t)g >> 8 << 8 | (uint32_t)b >> 8;
	return setpalette(entry, r, g, b);
}

void
cterm_do_tab(struct cterminal *cterm)
{
	int i;
	int cx, cy, ox;
	int lm, rm, bm;

	rm = TERM_MAXX;
	bm = TERM_MAXY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &rm, &bm);
	lm = TERM_MINX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &lm, NULL);

	CURR_XY(&cx, &cy);
	ox = cx;
	for (i = 0; i < cterm->tab_count; i++) {
		if (cterm->tabs[i] > cx) {
			cx = cterm->tabs[i];
			break;
		}
	}
	if ((ox > rm && cx > CURR_MAXX) || (ox <= rm && cx > rm) || i == cterm->tab_count) {
		cx = lm;
		if (cy == bm) {
			cterm_cond_scrollup(cterm);
			cy = bm;
		}
		else if(cy < CURR_MAXY)
			cy++;
	}
	gotoxy(cx, cy);
}

void
cterm_do_backtab(struct cterminal *cterm)
{
	int i;
	int cx, cy, ox;
	int lm;

	lm = TERM_MINX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &lm, NULL);

	CURR_XY(&cx, &cy);
	ox = cx;
	for (i = cterm->tab_count - 1; i >= 0; i--) {
		if (cterm->tabs[i] < cx) {
			cx = cterm->tabs[i];
			break;
		}
	}
	if (ox >= lm && cx < lm)
		cx = lm;
	gotoxy(cx, cy);
}

void
cterm_do_vtab(struct cterminal *cterm)
{
	int i;
	int cx, cy;
	int bm;

	bm = TERM_MAXY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, NULL, &bm);

	CURR_XY(&cx, &cy);
	for (i = 0; i < cterm->vtab_count; i++) {
		if (cterm->vtabs[i] > cy) {
			cy = cterm->vtabs[i];
			break;
		}
	}
	if (i == cterm->vtab_count) {
		/* No next line tab stop — scroll up one, cursor on last row */
		cterm_cond_scrollup(cterm);
		cy = bm;
	}
	gotoxy(cx, cy);
}

void
cterm_adjust_currpos(struct cterminal *cterm, int xadj, int yadj, int scroll)
{
	int x, y;
	int tx, ty;

	/*
	 * If we're inside the scrolling margins, we simply move inside
	 * them, and scrolling works.
	 */
	TERM_XY(&tx, &ty);
	if (tx >= TERM_MINX && tx <= TERM_MAXX &&
	    ty >= TERM_MINY && ty <= TERM_MAXY) {
		if (xadj == INT_MIN)
			tx = TERM_MINX;
		else
			tx += xadj;
		if (yadj == INT_MIN)
			ty = TERM_MINX;
		else
			ty += yadj;
		if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB) {
			while (ty > TERM_MAXY) {
				ty -= TERM_MAXY;
			}
			while (ty < TERM_MINY) {
				ty += TERM_MAXY;
			}
		}
		if (scroll) {
			while(ty > TERM_MAXY) {
				cterm_scrollup(cterm);
				ty--;
			}
			while(ty < TERM_MINY) {
				cterm_scrolldown(cterm);
				ty++;
			}
		}
		else {
			if (ty > TERM_MAXY)
				ty = TERM_MAXY;
			if (ty < TERM_MINY)
				ty = TERM_MINY;
		}
		if (tx > TERM_MAXX)
			tx = TERM_MAXX;
		if (tx < TERM_MINX)
			tx = TERM_MINX;
		cterm_gotoxy(cterm, tx, ty);
		return;
	}
	/*
	 * Outside of the scrolling margins, we can cross a margin
	 * into the scrolling region, but can't cross going out.
	 */
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &tx, &ty);
	x = tx;
	y = ty;
	if (xadj == INT_MIN)
		tx = TERM_MINX;
	else
		tx += xadj;
	if (yadj == INT_MIN)
		ty = TERM_MINX;
	else
		ty += yadj;
	if (x <= cterm->right_margin && tx > cterm->right_margin)
		tx = cterm->right_margin;
	if (y <= cterm->bottom_margin && ty > cterm->bottom_margin)
		ty = cterm->bottom_margin;
	if (tx < cterm->left_margin && x >= cterm->left_margin)
		tx = cterm->left_margin;
	if (ty < cterm->top_margin && y >= cterm->top_margin)
		ty = cterm->top_margin;
	gotoxy(tx, ty);
	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB)
		cterm_prestel_get_state(cterm);
}

void
cterm_clear_lcf(struct cterminal *cterm)
{
	cterm->last_column_flag &= ~CTERM_LCF_SET;
}

/* ------------------------------------------------------------------------
 * cterm_skypix_color — SkyPix palette translation.
 *
 * SkyPix is its own graphics mode with a 16-colour palette in a
 * different order than the standard ANSI/IBM palette.  The ECMA-48
 * SGR colour cases (30-37, 40-47) call this inline so the same handler
 * paints the right index whether or not SkyPix is active: when
 * cterm->skypix is clear, this is a no-op pass-through.  Lives in
 * cterm.c rather than any emulation-specific file because SkyPix
 * interacts with the base attribute path rather than sitting cleanly
 * inside one standard.
 * ------------------------------------------------------------------------ */
int
cterm_skypix_color(struct cterminal *cterm, int color)
{
	if (!cterm->skypix)
		return color;
	switch (color) {
		case BLACK:        return 0;
		case RED:          return 3;
		case GREEN:        return 4;
		case BROWN:        return 6;
		case BLUE:         return 1;
		case MAGENTA:      return 7;
		case CYAN:         return 5;
		case LIGHTGRAY:    return 2;
		case DARKGRAY:     return 8;
		case LIGHTRED:     return 11;
		case LIGHTGREEN:   return 12;
		case YELLOW:       return 14;
		case LIGHTBLUE:    return 9;
		case LIGHTMAGENTA: return 15;
		case LIGHTCYAN:    return 13;
		case WHITE:        return 10;
	}
	return color;
}

void
cterm_set_negative(struct cterminal *cterm, bool on)
{
	unsigned char bg;
	unsigned char fg;
	uint32_t tmp_colour;

	// TODO: This one is great...
	if (on != cterm->negative) {
		if (cterm->emulation == CTERM_EMULATION_ATARIST_VT52) {
			bg = cterm->attr & 0xF0;
			fg = cterm->attr & 0x0F;
			cterm->attr = (bg >> 4) | (fg << 4);
			tmp_colour = cterm->fg_color;
			cterm->fg_color = cterm->bg_color;
			cterm->bg_color = tmp_colour;
			cterm->negative = on;
		}
		else {
			bg = cterm->attr & 0x70;
			fg = cterm->attr & 0x07;
			cterm->attr &= 0x88;
			cterm->attr |= (bg >> 4);
			cterm->attr |= (fg << 4);
			tmp_colour = cterm->fg_color;
			cterm->fg_color = cterm->bg_color;
			cterm->bg_color = tmp_colour;
			cterm->negative = on;
		}
	}
}

/* ------------------------------------------------------------------------
 * cterm_apply_sgr — chain-of-responsibility root for SGR parameter
 * application.  Each level (cterm -> DEC -> ECMA-48) handles the
 * parameters it owns and tail-calls the next level if the param
 * wasn't its to act on; ECMA-48 is the base case and silently ignores
 * anything nobody claimed.  ECMA-48 reserves unassigned SGR values,
 * so extensions (38/48 extended colour, 90-107 aixterm bright, any
 * future DEC additions) have to intercept before the ECMA-48 table
 * is consulted.
 *
 * Used from the CSI m handler (cterm_handle_sgr) and from DECCARA.
 * ------------------------------------------------------------------------ */
void
cterm_apply_sgr(struct cterminal *cterm, int *pi)
{
	cterm_seq_default(cterm, *pi, 0);
	cterm_cterm_apply_sgr(cterm, pi);
}

/* ------------------------------------------------------------------------
 * ANSI-BBS dispatch table (sorted ascending by key).
 *
 * New entries are added here as do_ansi()'s switch cases are migrated.
 * When the table covers every sequence that do_ansi() handles, the
 * legacy switch will be removed and seq_feed() becomes the parser.
 * ------------------------------------------------------------------------ */

const struct seq_dispatch cterm_ansi_dispatch[] = {
	/* Entries are sorted ascending by seq_key.  With the current bit
	 * layout (intro > final > priv > interm) that groups by introducer,
	 * then by final byte within each introducer.  Variants sharing a
	 * final byte (plain CSI, CSI = ..., CSI ? ..., CSI $ ..., CSI * ...)
	 * therefore appear consecutively. */

	/* ESC + Fp/Fs (intro = ESC = 0x1B) — by final byte */
	{ cterm_handle_decsc,     SEQ_DECSC, 0 },			/* ESC 7 */
	{ cterm_handle_decrc,     SEQ_DECRC, 0 },			/* ESC 8 */
	{ cterm_handle_ris,       SEQ_RIS,   0 },			/* ESC c */

	/* C1 bare controls with introducer < CSI (0x5B) */
	{ cterm_handle_nel,       SEQ_NEL,       0 },			/* ESC E */
	{ cterm_handle_ssa,       SEQ_C1('F'),   0 },			/* ESC F */
	{ cterm_handle_esa,       SEQ_C1('G'),   0 },			/* ESC G */
	{ cterm_handle_hts,       SEQ_HTS,       0 },			/* ESC H */
	{ cterm_handle_vts,       SEQ_C1('J'),   0 },			/* ESC J */
	{ cterm_handle_ri,        SEQ_RI,        0 },			/* ESC M */
	{ cterm_handle_dcs_start, SEQ_DCS,       0 },			/* ESC P */
	{ cterm_handle_sts,       SEQ_STS,       0 },			/* ESC S */
	{ cterm_handle_sos_start, SEQ_SOS,       0 },			/* ESC X */

	/* CSI family (intro = CSI) — sorted by final byte, then priv, then interm */
	{ cterm_handle_ich, SEQ_ICH, 0 },					/* CSI @ */
	{ cterm_handle_sl,
	  (SEQ_CSI | SEQ_INTERM(' ') | SEQ_FINAL('@')), 0 },			/* CSI SP @ */
	{ cterm_handle_cuu, SEQ_CUU, 0 },					/* CSI A */
	{ cterm_handle_sr,
	  (SEQ_CSI | SEQ_INTERM(' ') | SEQ_FINAL('A')), 0 },			/* CSI SP A */
	{ cterm_handle_cud, SEQ_CUD, 0 },					/* CSI B */
	{ cterm_handle_cuf, SEQ_CUF, 0 },					/* CSI C */
	{ cterm_handle_cub, SEQ_CUB, 0 },					/* CSI D */
	{ cterm_handle_fnt,
	  (SEQ_CSI | SEQ_INTERM(' ') | SEQ_FINAL('D')), 0 },			/* CSI SP D */
	{ cterm_handle_cnl, SEQ_CNL, 0 },					/* CSI E */
	{ cterm_handle_cpl, SEQ_CPL, 0 },					/* CSI F */
	{ cterm_handle_cha, SEQ_CHA, 0 },					/* CSI G */
	{ cterm_handle_cup, SEQ_CUP, 0 },					/* CSI H */
	{ cterm_handle_cht, SEQ_CHT, 0 },					/* CSI I */
	{ cterm_handle_ed,  SEQ_ED,  0 },					/* CSI J */
	{ cterm_handle_el,  SEQ_EL,  0 },					/* CSI K */
	{ cterm_handle_il,  SEQ_IL,  0 },					/* CSI L */
	{ cterm_handle_ansi_music, SEQ_DL, 0 },					/* CSI M (DL or ANSI music) */
	{ cterm_handle_ansi_music_setup,
	  (SEQ_CSI | SEQ_PRIV('=') | SEQ_FINAL('M')), 0 },			/* CSI = M */
	{ cterm_handle_bansi_music,
	  (SEQ_CSI | SEQ_FINAL('N')), 0 },					/* CSI N */
	{ cterm_handle_dch, SEQ_DCH, 0 },					/* CSI P */
	{ cterm_handle_su,  SEQ_SU,  0 },					/* CSI S */
	{ cterm_handle_xterm_gfx_query,
	  (SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('S')), 0 },			/* CSI ? S */
	{ cterm_handle_sd,  SEQ_SD,  0 },					/* CSI T */
	{ cterm_handle_ech, SEQ_ECH, 0 },					/* CSI X */
	{ cterm_handle_cvt, SEQ_CVT, 0 },					/* CSI Y */
	{ cterm_handle_cbt, SEQ_CBT, 0 },					/* CSI Z */
	{ cterm_handle_cha, SEQ_HPA, 0 },					/* CSI ` (HPA — alias of CHA) */
	{ cterm_handle_hpr, SEQ_HPR, 0 },					/* CSI a */
	{ cterm_handle_rep, SEQ_REP, 0 },					/* CSI b */
	{ cterm_handle_da,  SEQ_DA,  0 },					/* CSI c */
	{ cterm_handle_syncterm_ext_da,
	  (SEQ_CSI | SEQ_PRIV('<') | SEQ_FINAL('c')), 0 },			/* CSI < c */
	{ cterm_handle_vpa, SEQ_VPA, 0 },					/* CSI d */
	{ cterm_handle_tsr,
	  (SEQ_CSI | SEQ_INTERM(' ') | SEQ_FINAL('d')), 0 },			/* CSI SP d */
	{ cterm_handle_vpr, SEQ_VPR, 0 },					/* CSI e */
	{ cterm_handle_cup, SEQ_HVP, 0 },					/* CSI f (HVP — alias of CUP) */
	{ cterm_handle_tbc, SEQ_TBC, 0 },					/* CSI g */
	{ cterm_handle_sm,  SEQ_SM,  0 },					/* CSI h */
	{ cterm_handle_syncterm_sm_ext,
	  (SEQ_CSI | SEQ_PRIV('=') | SEQ_FINAL('h')), 0 },			/* CSI = h */
	{ cterm_handle_decsm_cterm, SEQ_DECSM, 0 },				/* CSI ? h (cascade: cterm -> dec) */
	{ cterm_handle_hpb, SEQ_HPB, 0 },					/* CSI j */
	{ cterm_handle_vpb, SEQ_VPB, 0 },					/* CSI k */
	{ cterm_handle_rm,  SEQ_RM,  0 },					/* CSI l */
	{ cterm_handle_syncterm_rm_ext,
	  (SEQ_CSI | SEQ_PRIV('=') | SEQ_FINAL('l')), 0 },			/* CSI = l */
	{ cterm_handle_decrm_cterm, SEQ_DECRM, 0 },				/* CSI ? l (cascade: cterm -> dec) */
	{ cterm_handle_sgr, SEQ_SGR, 0 },					/* CSI m */
	{ cterm_handle_dsr_cterm, SEQ_DSR, 0 },					/* CSI n (cascade: cterm -> ecma48) */
	{ cterm_handle_ext_state_query,
	  (SEQ_CSI | SEQ_PRIV('=') | SEQ_FINAL('n')), 0 },			/* CSI = n */
	{ cterm_handle_macro_query,
	  (SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('n')), 0 },			/* CSI ? n */
	{ cterm_handle_decrqm_public,
	  (SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('p')), 0 },			/* CSI $ p */
	{ cterm_handle_decrqm_syncterm,
	  (SEQ_CSI | SEQ_PRIV('=') | SEQ_INTERM('$') | SEQ_FINAL('p')), 0 },	/* CSI = $ p */
	{ cterm_handle_decrqm_dec,
	  (SEQ_CSI | SEQ_PRIV('?') | SEQ_INTERM('$') | SEQ_FINAL('p')), 0 },	/* CSI ? $ p */
	{ cterm_handle_decscusr, SEQ_DECSCUSR, 0 },				/* CSI SP q */
	{ cterm_handle_decstbm, SEQ_DECSTBM, 0 },				/* CSI r */
	{ cterm_handle_deccara,
	  (SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('r')), 0 },			/* CSI $ r */
	{ cterm_handle_decscs,
	  (SEQ_CSI | SEQ_INTERM('*') | SEQ_FINAL('r')), 0 },			/* CSI * r */
	{ cterm_handle_csi_s, SEQ_SCOSC, 0 },					/* CSI s (SCOSC or DECSLRM) */
	{ cterm_handle_save_dec_modes,
	  (SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('s')), 0 },			/* CSI ? s */
	{ cterm_handle_csi_t,
	  (SEQ_CSI | SEQ_FINAL('t')), 0 },					/* CSI t */
	{ cterm_handle_decrara,
	  (SEQ_CSI | SEQ_INTERM('$') | SEQ_FINAL('t')), 0 },			/* CSI $ t */
	{ cterm_handle_scorc, SEQ_SCORC, 0 },					/* CSI u */
	{ cterm_handle_restore_dec_modes,
	  (SEQ_CSI | SEQ_PRIV('?') | SEQ_FINAL('u')), 0 },			/* CSI ? u */
	{ cterm_handle_deccra, SEQ_DECCRA, 0 },					/* CSI $ v */
	{ cterm_handle_dectabsr, SEQ_DECTABSR, 0 },				/* CSI $ w */
	{ cterm_handle_decfra, SEQ_DECFRA, 0 },					/* CSI $ x */
	{ cterm_handle_decsace, SEQ_DECSACE, 0 },				/* CSI * x */
	{ cterm_handle_deccksr,
	  (SEQ_CSI | SEQ_INTERM('*') | SEQ_FINAL('y')), 0 },			/* CSI * y */
	{ cterm_handle_decera, SEQ_DECERA, 0 },					/* CSI $ z */
	{ cterm_handle_decmacro,
	  (SEQ_CSI | SEQ_INTERM('*') | SEQ_FINAL('z')), 0 },			/* CSI * z */
	{ cterm_handle_font_load,
	  (SEQ_CSI | SEQ_PRIV('=') | SEQ_FINAL('{')), 0 },			/* CSI = { */
	{ cterm_handle_syncterm_music,
	  (SEQ_CSI | SEQ_FINAL('|')), 0 },					/* CSI | */
	{ cterm_handle_decsasd, SEQ_DECSASD, 0 },				/* CSI $ } */
	{ cterm_handle_decic, SEQ_DECIC, 0 },					/* CSI ' } */
	{ cterm_handle_decssdt, SEQ_DECSSDT, 0 },				/* CSI $ ~ */
	{ cterm_handle_decdc, SEQ_DECDC, 0 },					/* CSI ' ~ */

	/* C1 bare controls with introducer > CSI */
	{ cterm_handle_st,        SEQ_ST,  0 },					/* ESC \ (ST) */
	{ cterm_handle_osc_start, SEQ_OSC, 0 },					/* ESC ] */
	{ cterm_handle_pm_start,  SEQ_PM,  0 },					/* ESC ^ */
	{ cterm_handle_apc_start, SEQ_APC, 0 },					/* ESC _ */
};
const size_t cterm_ansi_dispatch_len = SEQ_ARRAY_SIZE(cterm_ansi_dispatch);


void
cterm_reset(struct cterminal *cterm)
{
	int  i;
	struct text_info ti;

	cterm->altfont[0] = cterm->altfont[1] = cterm->altfont[2] = cterm->altfont[3] = getfont(1);
	cterm->top_margin=1;
	cterm->bottom_margin=cterm->height;
	cterm->left_margin=1;
	cterm->right_margin=cterm->width;
	cterm->save_xpos=0;
	cterm->save_ypos=0;
	cterm->ssa_row = 0;
	cterm->ssa_col = 0;
	cterm->esa_row = 0;
	cterm->esa_col = 0;
	cterm->fetm = 0;
	cterm->ttm = 0;
	cterm->escbuf[0]=0;
	cterm->sequence=0;
	cterm->string = 0;
	cterm->accumulator = NULL;
	cterm->saved_accumulator = NULL;
	FREE_AND_NULL(cterm->strbuf);
	cterm->strbuflen = 0;
	cterm->strbufsize = 0;
	cterm->musicbuf[0] = 0;
	cterm->music_enable=CTERM_MUSIC_BANSI;
	cterm->music=0;
	cterm->tempo=120;
	cterm->octave=4;
	cterm->notelen=4;
	cterm->noteshape=CTERM_MUSIC_NORMAL;
	cterm->musicfore=TRUE;
	cterm->backpos=0;
	cterm->xpos = TERM_MINX;
	cterm->ypos = TERM_MINY;
	cterm->cursor=_NORMALCURSOR;
	cterm->extattr = CTERM_EXTATTR_AUTOWRAP | CTERM_EXTATTR_SXSCROLL | CTERM_EXTATTR_DECBKM;
	if (cterm->emulation == CTERM_EMULATION_ATARIST_VT52)
		cterm->extattr &= ~CTERM_EXTATTR_AUTOWRAP;
	FREE_AND_NULL(cterm->tabs);
	cterm->tabs = malloc(sizeof(cterm_tabs));
	if (cterm->tabs) {
		memcpy(cterm->tabs, cterm_tabs, sizeof(cterm_tabs));
		cterm->tab_count = sizeof(cterm_tabs) / sizeof(cterm_tabs[0]);
	}
	else
		cterm->tab_count = 0;
	FREE_AND_NULL(cterm->vtabs);
	cterm->vtab_count = 0;
	cterm->decsace = 0;
	cterm->decscs_speed = 0;
	cterm->setfont_result = CTERM_NO_SETFONT_REQUESTED;
	cterm->saved_mode = 0;
	cterm->saved_mode_mask = 0;
	cterm->negative = false;
	cterm->c64reversemode = false;
	gettextinfo(&ti);
	switch (ti.currmode) {
		case C64_40X25:
		case C128_40X25:
		case C128_80X25:
			cterm->attr = 15;
			break;
		default:
			cterm->attr = ti.normattr;
			break;
	}
	attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
	if (cterm->last_column_flag & CTERM_LCF_FORCED)
		cterm->last_column_flag &= ~(CTERM_LCF_SET);
	else
		cterm->last_column_flag &= ~(CTERM_LCF_SET | CTERM_LCF_ENABLED);
	cterm->doorway_mode = 0;
	cterm->doorway_char = 0;
	FREE_AND_NULL(cterm->fg_tc_str);
	FREE_AND_NULL(cterm->bg_tc_str);
	cterm->sixel = SIXEL_INACTIVE;
	cterm->sx_iv = 0;
	cterm->sx_ih = 0;
	cterm->sx_trans = 0;
	cterm->sx_repeat = 0;
	cterm->sx_left = 0;
	cterm->sx_x = 0;
	cterm->sx_y = 0;
	cterm->sx_bg = 0;
	cterm->sx_fg = 0;
	cterm->sx_pixels_sent = 0;
	cterm->sx_first_pass = 0;
	cterm->sx_hold_update = 0;
	cterm->sx_start_x = 0;
	cterm->sx_start_y = 0;
	cterm->sx_row_max_x = 0;
	FREE_AND_NULL(cterm->sx_pixels);
	cterm->sx_width = 0;
	cterm->sx_height = 0;
	FREE_AND_NULL(cterm->sx_mask);
	ciolib_set_current_hyperlink(0);
	for (i = 0; i < (sizeof(cterm->macros) / sizeof(cterm->macros[0])); i++) {
		FREE_AND_NULL(cterm->macros[i]);
		cterm->macro_lens[i] = 0;
	}
	setwindow(cterm);

	/* Set up tabs for ATASCII */
	if(cterm->emulation == CTERM_EMULATION_ATASCII) {
		for(i=0; i<cterm->tab_count; i++)
			cterm->escbuf[cterm->tabs[i]]=1;
	}
	cterm->atascii_inverse = false;

	/* Set up a shadow palette */
	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE) {
		for (i=0; i < sizeof(dac_default)/sizeof(struct dac_colors); i++)
			cterm_setpalette(cterm, i + 16, dac_default[i].red << 8 | dac_default[i].red, dac_default[i].green << 8 | dac_default[i].green, dac_default[i].blue << 8 | dac_default[i].blue);
		if (cterm->has_palette_override) {
			uint32_t op[16];
			get_modepalette(op);
			for (i = 0; i < 16; i++) {
				uint32_t c = cterm->palette_override[i];
				cterm_setpalette(cterm, op[i] + 16,
					((c & 0x00FF0000) >> 8) | ((c & 0x00FF0000) >> 16),
					((c & 0x0000FF00)) | ((c & 0x0000FF00) >> 8),
					((c & 0x000000FF) << 8) | ((c & 0x000000FF)));
			}
		}
	}

	/* Reset mouse state */
	if (cterm->mouse_state_change) {
		cterm->mouse_state_change(9, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1000, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1001, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1002, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1003, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1004, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1005, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1006, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1007, 0, cterm->mouse_state_change_cbdata);
		cterm->mouse_state_change(1015, 0, cterm->mouse_state_change_cbdata);
	}
}

struct cterminal* cterm_init(int height, int width, int xpos, int ypos, int backlines, int backcols, struct vmem_cell *scrollback, int emulation)
{
	static bool key_tables_verified = false;
	char	*revision="$Revision: 1.327 $";
	char *in;
	char	*out;
	struct cterminal *cterm;
	int flags;

	if (!key_tables_verified) {
		verify_key_tables();
		key_tables_verified = true;
	}

	if((cterm=calloc(1, sizeof(struct cterminal)))==NULL)
		return cterm;

	cterm->x=xpos;
	cterm->y=ypos;
	cterm->height=height;
	cterm->width=width;
	cterm->backlines=backlines;
	cterm->backwidth = backcols;
	cterm->scrollback=scrollback;
	cterm->log=CTERM_LOG_NONE;
	cterm->logfile=NULL;
	cterm->emulation=emulation;
	cterm->last_column_flag = 0;
	cterm->music_stream = -1;  /* lazily opened when MML first arrives */
	cterm->fx_stream = -1;     /* lazily opened on first cterm_play_fx* call */
	cterm->ext_state_7_cb = NULL;  /* registered by syncterm after cterm_init */
	/* Install the per-emulation dispatcher and its dispatch table.
	 * The dispatcher handles single bytes outside sequence state; the
	 * dispatch table is searched (via bsearch) when a sequence or C1
	 * function completes. */
	cterm->dispatch = NULL;
	cterm->dispatch_table = NULL;
	cterm->dispatch_table_len = 0;
	cterm->ctrl_bitmap = NULL;
	cterm->byte_handlers = NULL;
	switch (emulation) {
		case CTERM_EMULATION_ANSI_BBS:
			cterm->dispatch = cterm_dispatch_ecma;
			cterm->dispatch_table = cterm_ansi_dispatch;
			cterm->dispatch_table_len = cterm_ansi_dispatch_len;
			break;
		case CTERM_EMULATION_ATARIST_VT52:
			cterm->dispatch = cterm_dispatch_vt52;
			cterm->dispatch_table = cterm_st_vt52_dispatch;
			cterm->dispatch_table_len = cterm_st_vt52_dispatch_len;
			break;
		case CTERM_EMULATION_ATASCII:
			cterm->dispatch = cterm_dispatch_byte;
			cterm->ctrl_bitmap = cterm_atascii_ctrl_bitmap;
			cterm->byte_handlers = cterm_atascii_handlers;
			break;
		case CTERM_EMULATION_PETASCII:
			cterm->dispatch = cterm_dispatch_byte;
			cterm->ctrl_bitmap = cterm_petscii_ctrl_bitmap;
			cterm->byte_handlers = cterm_petscii_handlers;
			break;
		case CTERM_EMULATION_PRESTEL:
			/* Prestel's ESC+byte handling is driven by a custom
			 * programmable-memory state machine (cterm_accumulate_
			 * prestel_seq); it does not benefit from a bsearch table,
			 * so the table stays NULL and the Prestel dispatcher
			 * handles the byte repertoire directly. */
			cterm->dispatch = cterm_dispatch_prestel;
			break;
		case CTERM_EMULATION_BEEB:
			/* BEEB uses cterm_accumulate_trailing for VDU 23/28 but
			 * otherwise shares Prestel's C1 handling; no dispatch
			 * table needed. */
			cterm->dispatch = cterm_dispatch_beeb;
			break;
	}
	cterm->accumulator = NULL;
	cterm->saved_accumulator = NULL;
	cterm_reset(cterm);
	if(cterm->scrollback!=NULL) {
		memset(cterm->scrollback, 0, cterm->backwidth * cterm->backlines * sizeof(*cterm->scrollback));
		ciolib_set_hyperlink_gc_callback(cterm_hyperlink_gc, cterm);
	}
	strcpy(cterm->DA,"\x1b[=67;84;101;114;109;");
	out=strchr(cterm->DA, 0);
	if(out != NULL) {
		for(in=revision; *in; in++) {
			if(isdigit(*in))
				*(out++)=*in;
			if(*in=='.')
				*(out++)=';';
		}
		*out=0;
	}
	strcat(cterm->DA,"c");
	if (cterm->emulation == CTERM_EMULATION_PRESTEL) {
		cterm->cursor = _NOCURSOR;
		ciolib_setcursortype(cterm->cursor);
		memcpy(cterm->prestel_data[0], "????????????????", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[1], ":2:?4967?89:1???", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[2], ":1632?123?456???", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[3], ";???????????????", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[4], ";???????????????", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[5], ";???????????????", PRESTEL_MEM_SLOT_SIZE);
		memcpy(cterm->prestel_data[6], ";???????????????", PRESTEL_MEM_SLOT_SIZE);
	}
	if (cterm->emulation == CTERM_EMULATION_ATARIST_VT52) {
		flags = getvideoflags();
		flags |= CIOLIB_VIDEO_BGBRIGHT;
		setvideoflags(flags);
	}

	return cterm;
}

void cterm_start(struct cterminal *cterm)
{
	struct text_info ti;

	if (!cterm->started) {
		gettextinfo(&ti);
		switch (ti.currmode) {
			case C64_40X25:
			case C128_40X25:
			case C128_80X25:
				cterm->attr = 15;
				break;
			default:
				cterm->attr = ti.normattr;
				break;
		}
		attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
		FREE_AND_NULL(cterm->fg_tc_str);
		FREE_AND_NULL(cterm->bg_tc_str);
		cterm->fg_color += 16;
		cterm->bg_color += 16;
		textattr(cterm->attr);
		ciolib_setcolour(cterm->fg_color, cterm->bg_color);
		ciolib_setcursortype(cterm->cursor);
		cterm->started=1;
		setwindow(cterm);
		cterm_clearscreen(cterm, cterm->attr);
		cterm_gotoxy(cterm, 1, 1);
	}
}

void
cterm_advance_char(struct cterminal *cterm, int *x, int *y, int move)
{
	int lm = cterm->left_margin;
	int rm = cterm->right_margin;
	int bm = cterm->bottom_margin;

	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB) {
		cterm_prestel_fix_line(cterm, *x, *y, true, false);
		textattr(cterm->attr);
		setcolour(cterm->fg_color, cterm->bg_color);
	}
	if((*x == rm || *x == CURR_MAXX) && (!(cterm->extattr & CTERM_EXTATTR_AUTOWRAP))) {
		gotoxy(*x, *y);
		return;
	}
	else {
		if((*x == rm || *x == CURR_MAXX) && ((cterm->last_column_flag & (CTERM_LCF_ENABLED | CTERM_LCF_SET)) == CTERM_LCF_ENABLED)) {
			cterm->last_column_flag |= CTERM_LCF_SET;
			gotoxy(*x, *y);
		}
		else if(*y == bm && (*x == rm || *x == CURR_MAXX)) {
			if (cterm->emulation == CTERM_EMULATION_PRESTEL) {
				*y = cterm->top_margin;
				move = 1;
				*x = lm;
				cterm_prestel_new_line(cterm);
			}
			else {
				cterm_cond_scrollup(cterm);
				move = 1;
				*x = lm;
				if (cterm->emulation == CTERM_EMULATION_BEEB)
					cterm_prestel_new_line(cterm);
			}
		}
		else {
			if(*x == rm || *x == CURR_MAXX) {
				if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB) {
					cterm_prestel_new_line(cterm);
				}
				*x=lm;
				if (*y < CURR_MAXY)
					(*y)++;
				if (move)
					gotoxy(*x, *y);
			}
			else {
				(*x)++;
			}
		}
	}
	if (move)
		gotoxy(*x, *y);
}

void
cterm_ctputs(struct cterminal *cterm, char *buf)
{
	char *outp;
	char *p;
	int		oldscroll;
	int cx, cy;
	int lm, rm, bm;

	if (cterm->font_render) {
		cterm->font_render(buf);
		return;
	}

	outp = buf;
	oldscroll = _wscroll;
	_wscroll = 0;
	CURR_XY(&cx, &cy);
	if (cterm->log == CTERM_LOG_ASCII && cterm->logfile != NULL)
		fputs(buf, cterm->logfile);
	lm = TERM_MINX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &lm, NULL);
	rm = TERM_MAXX;
	bm = TERM_MAXY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &rm, &bm);
	for (p = buf; *p; p++) {
		switch(*p) {
			case '\r':
				cterm_clear_lcf(cterm);
				*p = 0;
				cputs(outp);
				outp = p + 1;
				cterm_adjust_currpos(cterm, INT_MIN, 0, 0);
				CURR_XY(&cx, &cy);
				break;
			case '\n':
				cterm_clear_lcf(cterm);
				*p = 0;
				cputs(outp);
				outp = p + 1;
				cterm_adjust_currpos(cterm, 0, 1, 1);
				CURR_XY(&cx, &cy);
				break;
			case '\b':
				cterm_clear_lcf(cterm);
				*p=0;
				cputs(outp);
				outp = p + 1;
				cterm_adjust_currpos(cterm, -1, 0, 0);
				CURR_XY(&cx, &cy);
				break;
			case 7:		/* Bell */
				break;
			case '\t':
				cterm_clear_lcf(cterm);
				*p=0;
				cputs(outp);
				outp=p+1;
				cterm_do_tab(cterm);
				CURR_XY(&cx, &cy);
				break;
			default:
				if ((cterm->last_column_flag & (CTERM_LCF_ENABLED | CTERM_LCF_SET)) == (CTERM_LCF_ENABLED | CTERM_LCF_SET)) {
					if (cx == cterm->right_margin || cx == CURR_MAXX) {
						cterm_advance_char(cterm, &cx, &cy, 1);
						cterm_clear_lcf(cterm);
					}
				}
				if (cx == cterm->right_margin || cx == CURR_MAXX) {
					char ch;
					ch = *(p + 1);
					*(p + 1) = 0;
					cputs(outp);
					*(p+1) = ch;
					outp = p + 1;
				}
				cterm_advance_char(cterm, &cx, &cy, 0);
				break;
		}
	}
	cputs(outp);
	if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB)
		cterm_prestel_fix_line(cterm, cx, cy, true, false);
	_wscroll=oldscroll;
}

/* ustrlen / cterm_uctputs / ustrcat live in cterm_internal.h */

/* ------------------------------------------------------------------------
 * cterm_dispatch_byte — shared dispatcher for PETSCII / ATASCII
 *
 * PETSCII and ATASCII don't use ECMA-48 sequence state; every received
 * byte is either a control code or a printable cell.  Each emulation
 * installs a 256-bit bitmap marking which byte values are controls and
 * a handler table indexed by byte value.  Printable bytes go through
 * the shared C64 attribute path (cterm_c64_get_attr returns cterm->attr
 * when the reverse flag is clear, so the same path works for both
 * emulations).  Bytes whose bitmap bit is set but whose handler slot is
 * NULL are silently dropped (PETSCII uses this for its reserved/
 * non-printable byte ranges).
 * ------------------------------------------------------------------------ */
void
cterm_dispatch_byte(struct cterminal *cterm, unsigned char byte, int *speed)
{
	(void)speed;

	if (cterm->ctrl_bitmap != NULL
	    && (cterm->ctrl_bitmap[byte >> 3] & (1u << (byte & 7)))) {
		if (cterm->byte_handlers != NULL
		    && cterm->byte_handlers[byte] != NULL)
			cterm->byte_handlers[byte](cterm);
		/* else: control byte with no handler — drop silently */
	}
	else {
		unsigned char ch[2];
		int sx, sy, x, y;
		ch[0] = byte;
		ch[1] = cterm_c64_get_attr(cterm);
		SCR_XY(&sx, &sy);
		puttext(sx, sy, sx, sy, ch);
		CURR_XY(&x, &y);
		cterm_advance_char(cterm, &x, &y, 1);
	}
}

/* ------------------------------------------------------------------------
 * cterm_accumulate_trailing — generic "read N raw bytes, then dispatch"
 *
 * Activated by a dispatch-table entry with a non-zero `trailing` field.
 * When cterm_accumulate_ecma_seq matches such an entry, it stashes the
 * handler and byte count on cterm and returns without calling the
 * handler; subsequent bytes are funnelled here.  Each byte lands in
 * cterm->seq_param_int[] so handlers can read the payload the same way
 * they read ordinary integer parameters.  When the count reaches zero
 * the stored handler is invoked and the accumulator clears itself.
 * ------------------------------------------------------------------------ */
bool
cterm_accumulate_trailing(struct cterminal *cterm, unsigned char byte, int *speed)
{
	if (cterm->seq_param_count < MAX_SEQ_PARAMS) {
		cterm->seq_param_int[cterm->seq_param_count] = byte;
		cterm->seq_param_count++;
	}
	if (cterm->seq_trailing_remain > 0)
		cterm->seq_trailing_remain--;
	if (cterm->seq_trailing_remain == 0) {
		void (*handler)(struct cterminal *, int *) = cterm->seq_trailing_handler;
		cterm->seq_trailing_handler = NULL;
		cterm->accumulator = NULL;
		if (handler != NULL)
			handler(cterm, speed);
	}
	return true;
}

CIOLIBEXPORT size_t cterm_write(struct cterminal * cterm, const void *vbuf, int buflen, int *speed)
{
	const unsigned char *buf = (unsigned char *)vbuf;
	unsigned char ch[2];
	/* Alias the struct-resident print-batch buffer so the unified
	 * accumulator signature (which touches cterm->print_buf / print_pos)
	 * can share the batch with the still-inline normal-char path. */
	unsigned char *prn = cterm->print_buf;
	unsigned char *prnpos;
	const unsigned char *prnlast = &prn[sizeof(cterm->print_buf)-sizeof(cterm->escbuf)-1];
	int i, j, x, y;
	struct text_info	ti;
	int	olddmc;
	int oldptnm;
	uint32_t palette[16];
	int mpalette;
	int orig_fonts[4];
	/* lastch is now cterm->lastch (persists across cterm_write calls) */
 	int palette_offset = 0;

	/* DECSASD routing: when active display is the status line, forward
	 * writes to the sub-cterm.  The sub's parser handles its own sequences;
	 * status-display and terminal-global handlers bubble back via its
	 * parent pointer. */
	if (cterm->status_sub && cterm->status_display_active == 1)
		return cterm_write(cterm->status_sub, vbuf, buflen, speed);

	if(!cterm->started)
		cterm_start(cterm);

	/* Now rejigger the current modes palette... */
	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
		palette_offset = 16;
	mpalette = get_modepalette(palette);
	if (mpalette) {
		for (i=0; i < 16; i++)
			palette[i] += palette_offset;
		set_modepalette(palette);
	}

	/* Deedle up the fonts */
	orig_fonts[0] = getfont(1);
	orig_fonts[1] = getfont(2);
	orig_fonts[2] = getfont(3);
	orig_fonts[3] = getfont(4);
	setfont(cterm->altfont[0], FALSE, 1);
	setfont(cterm->altfont[1], FALSE, 2);
	setfont(cterm->altfont[2], FALSE, 3);
	setfont(cterm->altfont[3], FALSE, 4);

	oldptnm=puttext_can_move;
	puttext_can_move=1;
	olddmc=hold_update;
	hold_update=1;
	gettextinfo(&ti);
	setwindow(cterm);
	x = cterm->xpos;
	y = cterm->ypos;
	cterm_coord_conv_xy(cterm, CTERM_COORD_ABSTERM, CTERM_COORD_CURR, &x, &y);
	gotoxy(x, y);
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
	ciolib_setcursortype(cterm->cursor);
	ch[1]=0;
	if(buflen==-1)
		buflen=ustrlen(buf);
	switch(buflen) {
		case 0:
			break;
		default:
			if(cterm->log==CTERM_LOG_RAW && cterm->logfile != NULL)
				fwrite(buf, buflen, 1, cterm->logfile);
			prn[0]=0;
			prnpos = prn;
			if (cterm->emulation == CTERM_EMULATION_PRESTEL || cterm->emulation == CTERM_EMULATION_BEEB)
				cterm_prestel_get_state(cterm);
			for (j = 0; j < buflen; j++) {
				if (prnpos >= prnlast) {
					cterm_uctputs(cterm, prn);
					prn[0] = 0;
					prnpos = prn;
				}

				/* Canonical two-check byte-loop form.  The per-emulation
				 * dispatcher is installed by cterm_init(); the accumulator
				 * is installed at mode entry (ESC arrival, DCS/OSC start,
				 * font load, music terminator, etc.) and cleared at mode
				 * exit. */
				cterm->print_pos = prnpos;
				if (cterm->accumulator != NULL) {
					if (cterm->accumulator(cterm, buf[j], speed)) {
						prnpos = cterm->print_pos;
						continue;
					}
				}
				cterm->dispatch(cterm, buf[j], speed);
				prnpos = cterm->print_pos;
			}
			cterm_uctputs(cterm, prn);
			prn[0] = 0;
			prnpos = prn;
			break;
	}
	ABS_XY(&cterm->xpos, &cterm->ypos);

	hold_update=olddmc;
	puttext_can_move=oldptnm;
	CURR_XY(&x, &y);
	gotoxy(x, y);
	ciolib_setcursortype(cterm->cursor);

	/* Now rejigger the current modes palette... */
	if (mpalette) {
		for (i=0; i < 16; i++)
			palette[i] -= palette_offset;
		set_modepalette(palette);
	}

	/* De-doodle the fonts */
	setfont(orig_fonts[0], FALSE, 1);
	setfont(orig_fonts[1], FALSE, 2);
	setfont(orig_fonts[2], FALSE, 3);
	setfont(orig_fonts[3], FALSE, 4);

	return buflen > 0 ? (size_t)buflen : 0;
}

int cterm_openlog(struct cterminal *cterm, char *logfile, int logtype)
{
	if(!cterm->started)
		cterm_start(cterm);

	cterm->logfile=fopen(logfile, "ab");
	if(cterm->logfile==NULL)
		return(0);
	cterm->log=logtype;
	return(1);
}

void cterm_closelog(struct cterminal *cterm)
{
	if(!cterm->started)
		cterm_start(cterm);

	if(cterm->logfile != NULL)
		fclose(cterm->logfile);
	cterm->logfile=NULL;
	cterm->log=CTERM_LOG_NONE;
}

FILE *dbg;
void cterm_end(struct cterminal *cterm, int free_fonts)
{
	int i;

	if(cterm) {
		cterm_closelog(cterm);
		if (free_fonts) {
			for(i=CONIO_FIRST_FREE_FONT; i < 256; i++) {
				FREE_AND_NULL(conio_fontdata[i].eight_by_sixteen);
				FREE_AND_NULL(conio_fontdata[i].eight_by_fourteen);
				FREE_AND_NULL(conio_fontdata[i].eight_by_eight);
				FREE_AND_NULL(conio_fontdata[i].twelve_by_twenty);
				FREE_AND_NULL(conio_fontdata[i].desc);
			}
		}
		if (cterm->music_stream >= 0) {
			xp_audio_stop(cterm->music_stream);
			xp_audio_close(cterm->music_stream);
			cterm->music_stream = -1;
			xptone_close();
		}
		if (cterm->fx_stream >= 0) {
			xp_audio_stop(cterm->fx_stream);
			xp_audio_close(cterm->fx_stream);
			cterm->fx_stream = -1;
			xptone_close();
		}

		FREE_AND_NULL(cterm->strbuf);
		FREE_AND_NULL(cterm->tabs);
		FREE_AND_NULL(cterm->vtabs);
		FREE_AND_NULL(cterm->fg_tc_str);
		FREE_AND_NULL(cterm->bg_tc_str);
		FREE_AND_NULL(cterm->sx_pixels);
		FREE_AND_NULL(cterm->sx_mask);
		for (i = 0; i < (sizeof(cterm->macros) / sizeof(cterm->macros[0])); i++) {
			FREE_AND_NULL(cterm->macros[i]);
		}

		free(cterm);
	}
}

