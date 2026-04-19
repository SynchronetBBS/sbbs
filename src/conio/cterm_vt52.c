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
 * cterm_vt52.c — DEC VT52 / Atari ST VT52 emulation.
 *
 * VT52 uses a simpler escape grammar than ECMA-48: ESC followed by a
 * single command byte, optionally followed by a fixed number of raw
 * trailing bytes.  Each command is a handler registered in
 * `cterm_st_vt52_dispatch[]`, keyed by the same 21-bit encoding used for
 * ANSI/ECMA-48: commands in the C1 Fe range (0x40-0x5F) use
 * SEQ_C1(ch); commands in the Fp/Fs ranges (0x30-0x3F, 0x60-0x7E)
 * use `SEQ_ESC | SEQ_FINAL(ch)`.  Trailing-byte commands (ESC Y, ESC
 * b, ESC c) set the entry's `trailing` field; the generic
 * cterm_accumulate_trailing accumulator in cterm.c collects the raw
 * bytes into seq_param_int[] before calling the handler.
 *
 * Handlers read their payload from cterm->seq_param_int[0..n-1] (for
 * ESC Y: row, col; for ESC b / c: color byte).
 *
 * Atari ST extensions (GEMDOS/TOS) layer on top of standard VT52 with
 * no conflicts — one shared table covers both.
 */

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include <genwrap.h>

#include "ciolib.h"
#include "cterm.h"
#include "cterm_internal.h"
#include "cterm_vt52.h"
#include "cterm_ecma48.h"	/* cterm_accumulate_ecma_seq installed on ESC */

/* ------------------------------------------------------------------------
 * VT52 command handlers
 * ------------------------------------------------------------------------ */

static void
cterm_vt52_alt_keypad_on(struct cterminal *cterm, int *speed)	/* ESC = */
{
	(void)speed;
	cterm->extattr |= CTERM_EXTATTR_ALTERNATE_KEYPAD;
}

static void
cterm_vt52_alt_keypad_off(struct cterminal *cterm, int *speed)	/* ESC > */
{
	(void)speed;
	cterm->extattr &= ~CTERM_EXTATTR_ALTERNATE_KEYPAD;
}

static void
cterm_vt52_cursor_up(struct cterminal *cterm, int *speed)	/* ESC A */
{
	(void)speed;
	cterm_adjust_currpos(cterm, 0, -1, 0);
}

static void
cterm_vt52_cursor_down(struct cterminal *cterm, int *speed)	/* ESC B */
{
	(void)speed;
	cterm_adjust_currpos(cterm, 0, 1, 0);
}

static void
cterm_vt52_cursor_right(struct cterminal *cterm, int *speed)	/* ESC C */
{
	(void)speed;
	cterm_adjust_currpos(cterm, 1, 0, 0);
}

static void
cterm_vt52_cursor_left(struct cterminal *cterm, int *speed)	/* ESC D */
{
	(void)speed;
	cterm_adjust_currpos(cterm, -1, 0, 0);
}

static void
cterm_vt52_noop(struct cterminal *cterm, int *speed)	/* ESC F, G, [, \ */
{
	(void)cterm;
	(void)speed;
	/* Graphics mode / hold-screen mode — not supported by Atari ST. */
}

static void
cterm_vt52_home(struct cterminal *cterm, int *speed)	/* ESC H */
{
	(void)cterm;
	(void)speed;
	gotoxy(CURR_MINX, CURR_MINY);
}

static void
cterm_vt52_reverse_lf(struct cterminal *cterm, int *speed)	/* ESC I */
{
	(void)speed;
	cterm_adjust_currpos(cterm, 0, -1, 1);
}

static void
cterm_vt52_erase_to_end(struct cterminal *cterm, int *speed)	/* ESC J */
{
	int x, y, i;
	(void)speed;
	clreol();
	CURR_XY(&x, &y);
	for (i = y + 1; i <= TERM_MAXY; i++) {
		cterm_gotoxy(cterm, TERM_MINY, i);
		clreol();
	}
	gotoxy(x, y);
}

static void
cterm_vt52_erase_eol(struct cterminal *cterm, int *speed)	/* ESC K */
{
	(void)cterm;
	(void)speed;
	clreol();
}

static void
cterm_vt52_goto_xy(struct cterminal *cterm, int *speed)	/* ESC Y row col */
{
	int x, y;
	(void)speed;
	y = (int)cterm->seq_param_int[0] - 32;
	if (y < 0)
		return;
	if (y > 23)
		CURR_XY(NULL, &y);
	x = (int)cterm->seq_param_int[1] - 32;
	if (x < 0)
		return;
	if (x > 79)
		x = 79;
	gotoxy(CURR_MINX + x, CURR_MINY + y);
}

/* ---- GEMDOS / Atari ST TOS extensions --------------------------------- */

static void
cterm_vt52_clear_screen(struct cterminal *cterm, int *speed)	/* ESC E */
{
	(void)speed;
	cterm_clearscreen(cterm, (char)cterm->attr);
	gotoxy(CURR_MINX, CURR_MINY);
}

static void
cterm_vt52_insert_line(struct cterminal *cterm, int *speed)	/* ESC L */
{
	int x, y, x2, y2, maxx, maxy;
	(void)speed;

	TERM_XY(&x, &y);
	if (y < TERM_MINY || y > TERM_MAXY || x < TERM_MINX || x > TERM_MAXX)
		return;
	x2 = TERM_MINX;
	y2 = y;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &x2, &y2);
	maxx = TERM_MAXX;
	maxy = TERM_MAXY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &maxx, &maxy);
	movetext(x2, y2, maxx, maxy - 1, x2, y2 + 1);
	cterm_gotoxy(cterm, TERM_MINX, y);
	cterm_clreol(cterm);
	cterm_gotoxy(cterm, x, y);
}

static void
cterm_vt52_delete_line(struct cterminal *cterm, int *speed)	/* ESC M */
{
	(void)speed;
	cterm_dellines(cterm, 1);
}

static void
cterm_vt52_do_attr(struct cterminal *cterm, char attr, bool bg)
{
	if (cterm->negative)
		bg = !bg;
	if (bg) {
		cterm->attr = (attr & 0x0F) << 4 | (cterm->attr & 0x0F);
		attr2palette(cterm->attr, NULL, &cterm->bg_color);
	}
	else {
		cterm->attr = (attr & 0x0F) | (cterm->attr & 0xF0);
		attr2palette(cterm->attr, &cterm->fg_color, NULL);
	}
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
}

static void
cterm_vt52_set_fg(struct cterminal *cterm, int *speed)	/* ESC b <color> */
{
	(void)speed;
	cterm_vt52_do_attr(cterm, (char)cterm->seq_param_int[0], false);
}

static void
cterm_vt52_set_bg(struct cterminal *cterm, int *speed)	/* ESC c <color> */
{
	(void)speed;
	cterm_vt52_do_attr(cterm, (char)cterm->seq_param_int[0], true);
}

static void
cterm_vt52_clear_to_begin(struct cterminal *cterm, int *speed)	/* ESC d */
{
	int x, y, i;
	(void)speed;
	cterm_clear2bol(cterm);
	CURR_XY(&x, &y);
	for (i = y - 1; i >= TERM_MINY; i--) {
		cterm_gotoxy(cterm, TERM_MINX, i);
		clreol();
	}
	gotoxy(x, y);
}

static void
cterm_vt52_cursor_on(struct cterminal *cterm, int *speed)	/* ESC e */
{
	(void)speed;
	cterm->cursor = _NORMALCURSOR;
	ciolib_setcursortype(cterm->cursor);
}

static void
cterm_vt52_cursor_off(struct cterminal *cterm, int *speed)	/* ESC f */
{
	(void)speed;
	cterm->cursor = _NOCURSOR;
	ciolib_setcursortype(cterm->cursor);
}

static void
cterm_vt52_save_cursor(struct cterminal *cterm, int *speed)	/* ESC j */
{
	(void)speed;
	CURR_XY(&cterm->save_xpos, &cterm->save_ypos);
}

static void
cterm_vt52_restore_cursor(struct cterminal *cterm, int *speed)	/* ESC k */
{
	(void)speed;
	if (cterm->save_ypos > 0 && cterm->save_ypos <= cterm->height
	    && cterm->save_xpos > 0 && cterm->save_xpos <= cterm->width) {
		if (cterm->save_ypos < CURR_MINY || cterm->save_ypos > CURR_MAXY
		    || cterm->save_xpos < CURR_MINX || cterm->save_xpos > CURR_MAXX)
			return;
		gotoxy(cterm->save_xpos, cterm->save_ypos);
	}
}

static void
cterm_vt52_clear_line(struct cterminal *cterm, int *speed)	/* ESC l */
{
	int x, y;
	(void)speed;
	CURR_XY(&x, &y);
	gotoxy(CURR_MINX, y);
	clreol();
	gotoxy(x, y);
}

static void
cterm_vt52_clear_to_bol(struct cterminal *cterm, int *speed)	/* ESC o */
{
	(void)speed;
	cterm_clear2bol(cterm);
}

static void
cterm_vt52_reverse_on(struct cterminal *cterm, int *speed)	/* ESC p */
{
	(void)speed;
	cterm_set_negative(cterm, true);
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
}

static void
cterm_vt52_reverse_off(struct cterminal *cterm, int *speed)	/* ESC q */
{
	(void)speed;
	cterm_set_negative(cterm, false);
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
}

static void
cterm_vt52_autowrap_on(struct cterminal *cterm, int *speed)	/* ESC v */
{
	(void)speed;
	cterm->extattr |= CTERM_EXTATTR_AUTOWRAP;
}

static void
cterm_vt52_autowrap_off(struct cterminal *cterm, int *speed)	/* ESC w */
{
	(void)speed;
	cterm->extattr &= ~CTERM_EXTATTR_AUTOWRAP;
}

/* ------------------------------------------------------------------------
 * cterm_dispatch_vt52 — VT52 single-byte dispatcher.
 *
 * VT52 has different C0 semantics than ECMA-48: byte 7 (BEL) beeps,
 * bytes 11 (VT) and 12 (FF) act like LF (down + scroll), ESC primes
 * the sequence parser, and the rest of the C0 range (0-6, 14-26,
 * 28-31) is silently dropped.  All other bytes are queued into the
 * print buffer.  Sequence parsing shares cterm_accumulate_ecma_seq()
 * via the cterm_st_vt52_dispatch[] table.
 * ------------------------------------------------------------------------ */
void
cterm_dispatch_vt52(struct cterminal *cterm, unsigned char byte, int *speed)
{
	(void)speed;

	switch (byte) {
		case 7:		/* BEL */
			cterm->lastch = 0;
			cterm_uctputs(cterm, cterm->print_buf);
			cterm->print_buf[0] = 0;
			cterm->print_pos = cterm->print_buf;
			if (cterm->log == CTERM_LOG_ASCII && cterm->logfile != NULL)
				fputs("\x07", cterm->logfile);
			if (!cterm->quiet) {
#ifdef __unix__
				putch(7);
#else
				MessageBeep(MB_OK);
#endif
			}
			break;
		case 11:	/* VT — treated as LF */
		case 12:	/* FF — treated as LF */
			cterm_uctputs(cterm, cterm->print_buf);
			cterm->print_buf[0] = 0;
			cterm->print_pos = cterm->print_buf;
			cterm_adjust_currpos(cterm, 0, 1, true);
			break;
		case 27:	/* ESC — start a sequence, prime seq_feed() */
			cterm_uctputs(cterm, cterm->print_buf);
			cterm->print_buf[0] = 0;
			cterm->print_pos = cterm->print_buf;
			cterm->accumulator = cterm_accumulate_ecma_seq;
			cterm->sequence = 1;
			cterm->seq_phase = SEQ_PHASE_C1;
			cterm->seq_key = SEQ_ESC;
			cterm->seq_param_len = 0;
			cterm->seq_params[0] = 0;
			cterm->seq_param_count = 0;
			cterm->seq_overflow = false;
			break;
		case 0:
		case 1: case 2: case 3: case 4: case 5: case 6:
		case 14: case 15: case 16: case 17: case 18: case 19:
		case 20: case 21: case 22: case 23: case 24: case 25:
		case 26:
		case 28: case 29: case 30: case 31:
			/* Silently dropped. */
			break;
		default:
			cterm->lastch = byte;
			*cterm->print_pos++ = byte;
			*cterm->print_pos = 0;
	}
}

/* ------------------------------------------------------------------------
 * VT52 dispatch table (sorted ascending by 21-bit key).
 * ------------------------------------------------------------------------ */
const struct seq_dispatch cterm_st_vt52_dispatch[] = {
	/* The table must be sorted ascending by `key` for bsearch().
	 * Keys are built from 6 intro bits (top) | FINAL | PRIV | INTERM.
	 * ESC + Fp/Fs (SEQ_ESC | SEQ_FINAL) all share the ESC intro, so
	 * their keys land in the SEQ_ESC band (0x0D8xxx).  ESC + Fe uses
	 * SEQ_C1(ch) which shifts (ch - 0x40 + 32) into the intro slot, so
	 * those keys land higher (0x108000+).  Fp/Fs entries therefore come
	 * before C1 entries. */
	/* --- ESC + Fp (0x30-0x3F) --- */
	{ cterm_vt52_alt_keypad_on,  SEQ_ESC | SEQ_FINAL('='), 0 },	/* 0x0D8E00 */
	{ cterm_vt52_alt_keypad_off, SEQ_ESC | SEQ_FINAL('>'), 0 },	/* 0x0D8F00 */
	/* --- ESC + Fs (0x60-0x7E) --- */
	{ cterm_vt52_set_fg,         SEQ_ESC | SEQ_FINAL('b'), 1 },	/* 0x0DB300 */
	{ cterm_vt52_set_bg,         SEQ_ESC | SEQ_FINAL('c'), 1 },	/* 0x0DB400 */
	{ cterm_vt52_clear_to_begin, SEQ_ESC | SEQ_FINAL('d'), 0 },
	{ cterm_vt52_cursor_on,      SEQ_ESC | SEQ_FINAL('e'), 0 },
	{ cterm_vt52_cursor_off,     SEQ_ESC | SEQ_FINAL('f'), 0 },
	{ cterm_vt52_save_cursor,    SEQ_ESC | SEQ_FINAL('j'), 0 },
	{ cterm_vt52_restore_cursor, SEQ_ESC | SEQ_FINAL('k'), 0 },
	{ cterm_vt52_clear_line,     SEQ_ESC | SEQ_FINAL('l'), 0 },
	{ cterm_vt52_clear_to_bol,   SEQ_ESC | SEQ_FINAL('o'), 0 },
	{ cterm_vt52_reverse_on,     SEQ_ESC | SEQ_FINAL('p'), 0 },
	{ cterm_vt52_reverse_off,    SEQ_ESC | SEQ_FINAL('q'), 0 },
	{ cterm_vt52_autowrap_on,    SEQ_ESC | SEQ_FINAL('v'), 0 },
	{ cterm_vt52_autowrap_off,   SEQ_ESC | SEQ_FINAL('w'), 0 },
	/* --- ESC + Fe (C1, 0x40-0x5F) --- */
	{ cterm_vt52_cursor_up,    SEQ_C1('A'), 0 },	/* 0x108000 */
	{ cterm_vt52_cursor_down,  SEQ_C1('B'), 0 },
	{ cterm_vt52_cursor_right, SEQ_C1('C'), 0 },
	{ cterm_vt52_cursor_left,  SEQ_C1('D'), 0 },
	{ cterm_vt52_clear_screen, SEQ_C1('E'), 0 },
	{ cterm_vt52_noop,         SEQ_C1('F'), 0 },	/* graphics */
	{ cterm_vt52_noop,         SEQ_C1('G'), 0 },	/* graphics */
	{ cterm_vt52_home,         SEQ_C1('H'), 0 },
	{ cterm_vt52_reverse_lf,   SEQ_C1('I'), 0 },
	{ cterm_vt52_erase_to_end, SEQ_C1('J'), 0 },
	{ cterm_vt52_erase_eol,    SEQ_C1('K'), 0 },
	{ cterm_vt52_insert_line,  SEQ_C1('L'), 0 },
	{ cterm_vt52_delete_line,  SEQ_C1('M'), 0 },
	{ cterm_vt52_goto_xy,      SEQ_C1('Y'), 2 },
	{ cterm_vt52_noop,         SEQ_C1('['), 0 },	/* hold screen */
	{ cterm_vt52_noop,         SEQ_C1('\\'), 0 },	/* hold screen off */
};
const size_t cterm_st_vt52_dispatch_len = SEQ_ARRAY_SIZE(cterm_st_vt52_dispatch);
