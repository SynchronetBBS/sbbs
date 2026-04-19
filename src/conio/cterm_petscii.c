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
 ****************************************************************************/

/*
 * cterm_petscii.c — Commodore C64 / C128 PETSCII emulation (bitmap +
 * handler table).
 *
 * Plugs into cterm_dispatch_byte() in cterm.c via the same scheme used
 * by cterm_atascii.c: cterm_petscii_ctrl_bitmap marks which byte values
 * are control codes and cterm_petscii_handlers[byte] holds the handler
 * pointer.  Printable bytes (bitmap bit clear) go through the shared
 * print path which consults cterm_c64_get_attr() to apply the
 * reverse-video flag.
 *
 * The colour mapping differs between the 40-column (C64 / C128-40) and
 * 80-column (C128-80) modes: the same PETSCII colour byte produces a
 * different attribute nibble depending on the current video mode.  A
 * pair of lookup tables indexed by byte value keeps the handlers for
 * the 16 colour bytes short.
 *
 * The PETSCII byte ranges 0x00..0x1F and 0x80..0x9F contain "reserved"
 * / unhandled values alongside the actual controls.  Those byte values
 * have bitmap bit set but handler slot NULL so cterm_dispatch_byte
 * silently drops them (matches the legacy "drop" behaviour).
 */

#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <genwrap.h>

#include "ciolib.h"
#include "cterm.h"
#include "cterm_internal.h"
#include "cterm_petscii.h"
#include "vidmodes.h"

/* ------------------------------------------------------------------------
 * Colour lookup tables — index by PETSCII colour byte, value is the
 * low nibble (foreground) written into cterm->attr after clearing the
 * existing FG.  Bytes not in the colour set have UNMAPPED (0xff).
 * ------------------------------------------------------------------------ */
#define U 0xff	/* unmapped slot */

static const uint8_t petscii_colour_40[256] = {
	[5] = 1, [28] = 2, [30] = 5, [31] = 6,
	[129] = 8, [144] = 0, [149] = 9, [150] = 10,
	[151] = 11, [152] = 12, [153] = 13, [154] = 14,
	[155] = 15, [156] = 4, [158] = 7, [159] = 3,
	/* rest initialised to 0 by designated-initialiser semantics;
	 * the is_colour check uses the bitmap, so non-colour slots are
	 * never read. */
};

static const uint8_t petscii_colour_80[256] = {
	[5] = 15, [28] = 4, [30] = 2, [31] = 1,
	[129] = 5, [144] = 0, [149] = 6, [150] = 12,
	[151] = 3, [152] = 8, [153] = 10, [154] = 9,
	[155] = 7, [156] = 13, [158] = 14, [159] = 11,
};

#undef U

static void
petscii_colour(struct cterminal *cterm, unsigned char byte)
{
	struct text_info ti;
	uint8_t nibble;

	gettextinfo(&ti);
	if (ti.currmode == C64_40X25 || ti.currmode == C128_40X25)
		nibble = petscii_colour_40[byte];
	else
		nibble = petscii_colour_80[byte];
	cterm->attr &= 0xf0;
	cterm->attr |= (nibble & 0x0f);
	textattr(cterm->attr);
	attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
	FREE_AND_NULL(cterm->fg_tc_str);
	FREE_AND_NULL(cterm->bg_tc_str);
}

/* ------------------------------------------------------------------------
 * Per-byte control handlers.  Signature:
 *   static void fn(struct cterminal *cterm);
 * The byte value is implicit in which slot of cterm_petscii_handlers[]
 * holds the pointer.  Colour bytes dispatch through a single handler
 * per byte that re-discovers the byte via its slot number; keeping one
 * handler per byte is clearer than a trampoline.
 * ------------------------------------------------------------------------ */

static void petscii_colour_05(struct cterminal *c)  { petscii_colour(c, 5); }
static void petscii_colour_28(struct cterminal *c)  { petscii_colour(c, 28); }
static void petscii_colour_30(struct cterminal *c)  { petscii_colour(c, 30); }
static void petscii_colour_31(struct cterminal *c)  { petscii_colour(c, 31); }
static void petscii_colour_129(struct cterminal *c) { petscii_colour(c, 129); }
static void petscii_colour_144(struct cterminal *c) { petscii_colour(c, 144); }
static void petscii_colour_149(struct cterminal *c) { petscii_colour(c, 149); }
static void petscii_colour_150(struct cterminal *c) { petscii_colour(c, 150); }
static void petscii_colour_151(struct cterminal *c) { petscii_colour(c, 151); }
static void petscii_colour_152(struct cterminal *c) { petscii_colour(c, 152); }
static void petscii_colour_153(struct cterminal *c) { petscii_colour(c, 153); }
static void petscii_colour_154(struct cterminal *c) { petscii_colour(c, 154); }
static void petscii_colour_155(struct cterminal *c) { petscii_colour(c, 155); }
static void petscii_colour_156(struct cterminal *c) { petscii_colour(c, 156); }
static void petscii_colour_158(struct cterminal *c) { petscii_colour(c, 158); }
static void petscii_colour_159(struct cterminal *c) { petscii_colour(c, 159); }

static void
petscii_bell(struct cterminal *cterm)	/* 7 */
{
	if (!cterm->quiet) {
#ifdef __unix__
		putch(7);
#else
		(void)cterm;
		MessageBeep(MB_OK);
#endif
	}
}

static void
petscii_cr(struct cterminal *cterm)	/* 13 — "\r\n" + disable reverse */
{
	cterm_c64_set_reverse(cterm, false);
	cterm_adjust_currpos(cterm, INT_MIN, 0, 0);
	cterm_adjust_currpos(cterm, 0, 1, 1);
}

static void
petscii_lf(struct cterminal *cterm)	/* 141 — "\r\n" without reverse change */
{
	cterm_adjust_currpos(cterm, INT_MIN, 0, 0);
	cterm_adjust_currpos(cterm, 0, 1, 1);
}

static void
petscii_down(struct cterminal *cterm)	/* 17 — cursor down w/ scroll */
{
	cterm_adjust_currpos(cterm, 0, 1, 1);
}

static void
petscii_font_lower(struct cterminal *cterm)	/* 14 */
{
	struct text_info ti;

	gettextinfo(&ti);
	if (ti.currmode == C64_40X25) {
		setfont(33, FALSE, 1);
		cterm->altfont[0] = 33;
	}
	else {
		setfont(35, FALSE, 1);
		cterm->altfont[0] = 35;
	}
}

static void
petscii_font_upper(struct cterminal *cterm)	/* 142 */
{
	struct text_info ti;

	gettextinfo(&ti);
	if (ti.currmode == C64_40X25) {
		setfont(32, FALSE, 1);
		cterm->altfont[0] = 32;
	}
	else {
		setfont(34, FALSE, 1);
		cterm->altfont[0] = 34;
	}
}

static void
petscii_reverse_on(struct cterminal *cterm)	/* 18 */
{
	cterm_c64_set_reverse(cterm, true);
}

static void
petscii_reverse_off(struct cterminal *cterm)	/* 146 */
{
	cterm_c64_set_reverse(cterm, false);
}

static void
petscii_home(struct cterminal *cterm)	/* 19 */
{
	cterm_adjust_currpos(cterm, INT_MIN, INT_MIN, 0);
}

static void
petscii_clear(struct cterminal *cterm)	/* 147 — clear + home */
{
	cterm_clearscreen(cterm, cterm->attr);
	cterm_adjust_currpos(cterm, INT_MIN, INT_MIN, 0);
}

static void
petscii_delete(struct cterminal *cterm)	/* 20 — wrapping backspace */
{
	int x, y, sx, sy, ex;

	CURR_XY(&x, &y);
	if (x == CURR_MINX) {
		if (y == CURR_MINY)
			return;
		x = CURR_MINX;
		gotoxy(x, y - 1);
	}
	else
		gotoxy(--x, y);
	if (x < CURR_MAXX) {
		sx = x;
		sy = y;
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
		ex = CURR_MAXX;
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &ex, NULL);
		movetext(sx + 1, sy, ex, sy, sx, sy);
	}
	gotoxy(CURR_MAXX, y);
	cterm_clreol(cterm);
	gotoxy(x, y);
}

static void
petscii_left(struct cterminal *cterm)	/* 157 — cursor left (wraps) */
{
	int x, y;

	CURR_XY(&x, &y);
	if (x == CURR_MINX) {
		if (y > CURR_MINY)
			gotoxy(CURR_MAXX, y - 1);
	}
	else
		gotoxy(x - 1, y);
}

static void
petscii_right(struct cterminal *cterm)	/* 29 — cursor right (wraps + scroll) */
{
	int x, y;

	CURR_XY(&x, &y);
	if (x == CURR_MAXX) {
		if (y == CURR_MAXY) {
			cterm_cond_scrollup(cterm);
			gotoxy(CURR_MINX, y);
		}
		else
			gotoxy(CURR_MINX, y + 1);
	}
	else
		gotoxy(x + 1, y);
}

static void
petscii_up(struct cterminal *cterm)	/* 145 — cursor up (no scroll) */
{
	cterm_adjust_currpos(cterm, 0, -1, 0);
}

static void
petscii_insert(struct cterminal *cterm)	/* 148 — insert + erase under cursor */
{
	int x, y, sx, sy, ex;

	CURR_XY(&x, &y);
	if (x <= CURR_MAXX) {
		sx = x;
		sy = y;
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
		ex = CURR_MAXX;
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &ex, NULL);
		movetext(sx, sy, ex - 1, sy, sx + 1, sy);
	}
	putch(' ');
	gotoxy(x, y);
}

/* ------------------------------------------------------------------------
 * Control bitmap + handler table.
 *
 * "Control" for PETSCII includes every reserved/unhandled byte in the
 * 0x00..0x1F and 0x80..0x9F ranges so those bytes are dropped instead
 * of printed as garbage.  Reserved slots have a NULL handler pointer;
 * cterm_dispatch_byte treats that as a silent drop.
 * ------------------------------------------------------------------------ */

#define BIT(b)	((uint8_t)1 << ((b) & 7))
const uint8_t cterm_petscii_ctrl_bitmap[32] = {
	/* 0x00..0x1F — every byte is either a control or reserved/drop */
	0xff, 0xff, 0xff, 0xff,
	/* 0x20..0x7F — all printable */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	/* 0x80..0x9F — every byte is either a control or reserved/drop */
	0xff, 0xff, 0xff, 0xff,
	/* 0xA0..0xFF — all printable */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};
#undef BIT

void (*const cterm_petscii_handlers[256])(struct cterminal *) = {
	/* 0x00..0x1F */
	[5]   = petscii_colour_05,
	[7]   = petscii_bell,
	[13]  = petscii_cr,
	[14]  = petscii_font_lower,
	[17]  = petscii_down,
	[18]  = petscii_reverse_on,
	[19]  = petscii_home,
	[20]  = petscii_delete,
	[28]  = petscii_colour_28,
	[29]  = petscii_right,
	[30]  = petscii_colour_30,
	[31]  = petscii_colour_31,
	/* 0x80..0x9F */
	[129] = petscii_colour_129,
	[141] = petscii_lf,
	[142] = petscii_font_upper,
	[144] = petscii_colour_144,
	[145] = petscii_up,
	[146] = petscii_reverse_off,
	[147] = petscii_clear,
	[148] = petscii_insert,
	[149] = petscii_colour_149,
	[150] = petscii_colour_150,
	[151] = petscii_colour_151,
	[152] = petscii_colour_152,
	[153] = petscii_colour_153,
	[154] = petscii_colour_154,
	[155] = petscii_colour_155,
	[156] = petscii_colour_156,
	[157] = petscii_left,
	[158] = petscii_colour_158,
	[159] = petscii_colour_159,
};

void
cterm_c64_set_reverse(struct cterminal *cterm, bool on)
{
	if (on != cterm->c64reversemode)
		cterm->c64reversemode = on;
}

uint8_t
cterm_c64_get_attr(struct cterminal *cterm)
{
	if (cterm->c64reversemode)
		return (cterm->attr >> 4 | cterm->attr << 4);
	return cterm->attr;
}

