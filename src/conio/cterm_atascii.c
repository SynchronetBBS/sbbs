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
 * cterm_atascii.c — Atari 8-bit ATASCII emulation (bitmap + handler table).
 *
 * Plugs into cterm_dispatch_byte() in cterm.c: the 256-bit
 * cterm_atascii_ctrl_bitmap marks which byte values are control codes;
 * cterm_atascii_handlers[byte] is the handler function for those bytes.
 * Printable characters (bitmap bit clear) use the generic print path.
 *
 * The ESC byte (27) enters one-shot "inverse" mode: the very next byte
 * is treated as a literal cell regardless of its control-character
 * value, then the mode resets.  This is handled by the ESC handler
 * installing a single-byte accumulator.
 *
 * Tab stops use the shared cterm->tabs[] / cterm_insert_tabstop /
 * cterm_delete_tabstop / cterm_do_tab API — ATASCII's SET TAB (159) /
 * CLEAR TAB (158) / TAB (127) are thin wrappers.
 */

#include <ctype.h>
#include <limits.h>
#include <string.h>

#include <genwrap.h>

#include "ciolib.h"
#include "cterm.h"
#include "cterm_internal.h"
#include "cterm_atascii.h"

/* ------------------------------------------------------------------------
 * Literal-byte accumulator for ATASCII's one-shot ESC-quote mode.
 * When the ESC handler fires it installs this accumulator; the next
 * byte is written as a screen-code-translated literal cell and the
 * accumulator clears itself.
 * ------------------------------------------------------------------------ */
static bool
atascii_literal_byte(struct cterminal *cterm, unsigned char byte, int *speed)
{
	int x, y, k, sx, sy;
	unsigned char ch[2] = {0, 0};
	(void)speed;

	if (byte == 155) {	/* Return — always a CR even when quoted */
		cterm_adjust_currpos(cterm, INT_MIN, +1, 1);
	}
	else {
		k = byte;
		if (k < 32)
			k += 64;
		else if (k < 96)
			k -= 32;
		else if (k < 128)
			;	/* no translation */
		else if (k < 160)
			k += 64;
		else if (k < 224)
			k -= 32;
		/* k < 256: no translation */
		ch[0] = k;
		ch[1] = cterm->attr;
		SCR_XY(&sx, &sy);
		puttext(sx, sy, sx, sy, ch);
		CURR_XY(&x, &y);
		cterm_advance_char(cterm, &x, &y, 1);
	}
	cterm->attr = 7;
	cterm->accumulator = NULL;
	return true;
}

/* ------------------------------------------------------------------------
 * Per-byte control handlers.  Each handler signature is
 *   void fn(struct cterminal *cterm);
 * — the byte value is implicit in which slot of cterm_atascii_handlers[]
 * holds the pointer.
 * ------------------------------------------------------------------------ */

static void
atascii_escape(struct cterminal *cterm)	/* 27 */
{
	cterm->attr = 1;
	cterm->accumulator = atascii_literal_byte;
}

static void
atascii_up(struct cterminal *cterm)	/* 28 — wraps to bottom of same column */
{
	int x, y;
	CURR_XY(&x, &y);
	y--;
	if (y < CURR_MINY)
		y = CURR_MAXY;
	gotoxy(x, y);
}

static void
atascii_down(struct cterminal *cterm)	/* 29 — wraps to top of same column */
{
	int x, y;
	CURR_XY(&x, &y);
	y++;
	if (y > CURR_MAXY)
		y = CURR_MINY;
	gotoxy(x, y);
}

static void
atascii_left(struct cterminal *cterm)	/* 30 — wraps to right side of same row */
{
	int x, y;
	CURR_XY(&x, &y);
	x--;
	if (x < CURR_MINX)
		x = CURR_MAXX;
	gotoxy(x, y);
}

static void
atascii_right(struct cterminal *cterm)	/* 31 — wraps to left side of same row */
{
	int x, y;
	CURR_XY(&x, &y);
	x++;
	if (x > CURR_MAXX)
		x = CURR_MINX;
	gotoxy(x, y);
}

static void
atascii_clear_screen(struct cterminal *cterm)	/* 125 */
{
	cterm_clearscreen(cterm, cterm->attr);
}

static void
atascii_backspace(struct cterminal *cterm)	/* 126 — erase, no wrap */
{
	int x, y;
	CURR_XY(&x, &y);
	x--;
	if (x < CURR_MINX)
		return;
	gotoxy(x, y);
	putch(32);
	gotoxy(x, y);
}

static void
atascii_tab(struct cterminal *cterm)	/* 127 — wraps to next line */
{
	cterm_do_tab(cterm);
}

static void
atascii_return(struct cterminal *cterm)	/* 155 */
{
	cterm_adjust_currpos(cterm, INT_MIN, +1, 1);
}

static void
atascii_delete_line(struct cterminal *cterm)	/* 156 */
{
	cterm_dellines(cterm, 1);
	cterm_adjust_currpos(cterm, INT_MIN, 0, 0);
}

static void
atascii_insert_line(struct cterminal *cterm)	/* 157 */
{
	int x, y, sx, sy, ex, ey;
	CURR_XY(&x, &y);
	if (y < CURR_MAXY) {
		sx = CURR_MINX;
		sy = y;
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
		ex = CURR_MAXX;
		ey = CURR_MAXY;
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &ex, &ey);
		movetext(sx, sy, ex, ey - 1, sx, sy + 1);
	}
	gotoxy(CURR_MINX, y);
	cterm_clreol(cterm);
}

static void
atascii_clear_tab(struct cterminal *cterm)	/* 158 */
{
	cterm_delete_tabstop(cterm, wherex());
}

static void
atascii_set_tab(struct cterminal *cterm)	/* 159 */
{
	cterm_insert_tabstop(cterm, wherex());
}

static void
atascii_bell(struct cterminal *cterm)	/* 253 */
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
atascii_delete_char(struct cterminal *cterm)	/* 254 */
{
	int x, y, sx, sy, ex;
	CURR_XY(&x, &y);
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
atascii_insert_char(struct cterminal *cterm)	/* 255 */
{
	int x, y, sx, sy, ex;
	CURR_XY(&x, &y);
	if (x < CURR_MAXX) {
		sx = x;
		sy = y;
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
		ex = CURR_MAXX;
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &ex, NULL);
		movetext(sx, sy, ex - 1, sy, sx + 1, sy);
	}
	putch(32);
	gotoxy(x, y);
}

/* ------------------------------------------------------------------------
 * Control bitmap: one bit per byte value.  A set bit means that byte
 * is a control character (look up handler in cterm_atascii_handlers[]);
 * a clear bit means "printable, draw via cterm_dispatch_byte's
 * default path".
 * ------------------------------------------------------------------------ */

#define BIT(b)	((uint8_t)1 << ((b) & 7))
const uint8_t cterm_atascii_ctrl_bitmap[32] = {
	/* 0x00-0x1F */
	0,
	0,
	0,
	/* byte 27 = ESC (bit 3 of byte 3) */
	BIT(27) | BIT(28) | BIT(29) | BIT(30) | BIT(31),
	/* 0x20-0x7F: only 125 (0x7D), 126 (0x7E), 127 (0x7F) */
	0, 0, 0, 0,	/* 0x20-0x3F */
	0, 0, 0, 0,	/* 0x40-0x5F */
	0, 0, 0,	/* 0x60-0x7B */
	BIT(125) | BIT(126) | BIT(127),
	/* 0x80-0x9F: 155 (0x9B), 156, 157, 158, 159 */
	0, 0, 0,	/* 0x80-0x97 */
	BIT(155) | BIT(156) | BIT(157) | BIT(158) | BIT(159),
	/* 0xA0-0xFF: only 253, 254, 255 */
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 0xA0-0xF7 */
	BIT(253) | BIT(254) | BIT(255),
};
#undef BIT

void (*const cterm_atascii_handlers[256])(struct cterminal *) = {
	[27]  = atascii_escape,
	[28]  = atascii_up,
	[29]  = atascii_down,
	[30]  = atascii_left,
	[31]  = atascii_right,
	[125] = atascii_clear_screen,
	[126] = atascii_backspace,
	[127] = atascii_tab,
	[155] = atascii_return,
	[156] = atascii_delete_line,
	[157] = atascii_insert_line,
	[158] = atascii_clear_tab,
	[159] = atascii_set_tab,
	[253] = atascii_bell,
	[254] = atascii_delete_char,
	[255] = atascii_insert_char,
};
