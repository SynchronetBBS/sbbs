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
 * cterm_dec.c — DEC-private control-sequence handlers.
 *
 * Implements the CSI `?` and other DEC-extension sequences originating on
 * DEC VT-series terminals:
 *   - Cursor save/restore:  DECSC, DECRC
 *   - Private modes:        DECSM, DECRM (CSI ? h / l), plus the
 *                           save_dec_modes / restore_dec_modes helpers
 *                           driving XTerm-style mode stacks
 *   - Rectangular-area ops: DECERA (erase), DECFRA (fill), DECCRA (copy),
 *                           DECIC (insert col), DECDC (delete col),
 *                           DECCARA (change-attrs), DECRARA (reverse-attrs)
 *   - Area qualifiers:      DECSACE (select character edit extent)
 *   - Checksum / macros:    DECCKSR, DECMACRO
 *   - Mode queries:         DECRQM_ansi, DECRQM_dec
 *   - Tab report:           DECTABSR
 *   - Cursor style:         DECSCUSR
 *   - Left/right margins:   DECSLRM (CSI Pl ; Pr s when DECLRMM set)
 *
 * Handlers share the cterm_handle_* signature used by the dispatch table.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <genwrap.h>
#include <link_list.h>

#include "ciolib.h"
#include "cterm.h"
#include "cterm_internal.h"
#include "cterm_dec.h"
#include "cterm_ecma48.h"	/* cterm_handle_sm/rm chained from DECSM/DECRM */
#include "cterm_cterm.h"	/* SyncTERM DECSM extensions chained from DEC */
#include "vidmodes.h"
#include <crc16.h>

void
cterm_handle_decscusr(struct cterminal *cterm, int *speed)	/* CSI SP q */
{
	int s, e, r, b, v;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	switch (cterm->seq_param_int[0]) {
		case 0:
		case 1:
		default:
			cterm->cursor = _SOLIDCURSOR;
			_setcursortype(_SOLIDCURSOR);
			if (cio_api.options & CONIO_OPT_CUSTOM_CURSOR) {
				getcustomcursor(&s, &e, &r, &b, &v);
				b = true;
				v = true;
				setcustomcursor(s, e, r, b, v);
			}
			break;
		case 2:
			cterm->cursor = _SOLIDCURSOR;
			_setcursortype(_SOLIDCURSOR);
			if (cio_api.options & CONIO_OPT_CUSTOM_CURSOR) {
				getcustomcursor(&s, &e, &r, &b, &v);
				b = false;
				v = true;
				setcustomcursor(s, e, r, b, v);
			}
			break;
		case 3:
			cterm->cursor = _NORMALCURSOR;
			_setcursortype(_NORMALCURSOR);
			if (cio_api.options & CONIO_OPT_CUSTOM_CURSOR) {
				getcustomcursor(&s, &e, &r, &b, &v);
				b = true;
				v = true;
				setcustomcursor(s, e, r, b, v);
			}
			break;
		case 4:
			cterm->cursor = _NORMALCURSOR;
			_setcursortype(_NORMALCURSOR);
			if (cio_api.options & CONIO_OPT_CUSTOM_CURSOR) {
				getcustomcursor(&s, &e, &r, &b, &v);
				b = false;
				v = true;
				setcustomcursor(s, e, r, b, v);
			}
			break;
	}
}

void
cterm_handle_decrqm_public(struct cterminal *cterm, int *speed)	/* CSI Ps $ p */
{
	int pm = 0;
	(void)speed;
	if (cterm->seq_param_count != 1)
		return;
	cterm_seq_default(cterm, 0, 0);
	switch (cterm->seq_param_int[0]) {
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 15:
		case 17:
		case 18:
			pm = 4;	/* permanently set (see ECMA-48 modes) */
			break;
		case 14:
			pm = cterm->fetm ? 1 : 2;
			break;
		case 16:
			pm = cterm->ttm ? 1 : 2;
			break;
		case 21:
		case 22:
			pm = 3;	/* permanently reset */
			break;
	}
	cterm_respond_printf(cterm, "\x1b[%u;%d$y",
	    (unsigned)cterm->seq_param_int[0], pm);
}

void
cterm_handle_dectabsr(struct cterminal *cterm, int *speed)	/* CSI Ps $ w — tab report */
{
	int i;
	char tmp[3072];
	char *p2;
	(void)speed;
	cterm_seq_default(cterm, 0, 0);
	if (cterm->seq_param_int[0] != 2)
		return;
	strcpy(tmp, "\x1bP2$u");
	p2 = strchr(tmp, 0);
	for (i = 0; i < cterm->tab_count && cterm->tabs[i] <= cterm->width; i++) {
		if (i != 0)
			*(p2++) = '/';
		p2 += sprintf(p2, "%d", cterm->tabs[i]);
	}
	strcat(p2, "\x1b\\");
	if (*tmp)
		cterm_respond(cterm, tmp, strlen(tmp));
}

void
cterm_handle_decera(struct cterminal *cterm, int *speed)	/* CSI Pt;Pl;Pb;Pr $ z */
{
	int t, l, b, r;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	cterm_seq_default(cterm, 1, 1);
	cterm_seq_default(cterm, 2, cterm->height);
	cterm_seq_default(cterm, 3, cterm->width);
	t = (int)cterm->seq_param_int[0];
	l = (int)cterm->seq_param_int[1];
	b = (int)cterm->seq_param_int[2];
	r = (int)cterm->seq_param_int[3];
	if (t > b || l > r)
		return;
	if (t < CURR_MINY)
		t = CURR_MINY;
	if (l < CURR_MINX)
		l = CURR_MINX;
	if (b > CURR_MAXY)
		b = CURR_MAXY;
	if (r > CURR_MAXX)
		r = CURR_MAXX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &l, &t);
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &r, &b);
	cterm_fill_rect(cterm, l, t, r, b, ' ');
}

void
cterm_handle_decfra(struct cterminal *cterm, int *speed)	/* CSI Pch;Pt;Pl;Pb;Pr $ x */
{
	int t, l, b, r;
	uint8_t ch;
	(void)speed;
	cterm_seq_default(cterm, 0, 0);
	ch = (uint8_t)cterm->seq_param_int[0];
	if (ch < 0x20 || ch == 0x7F)
		return;
	cterm_seq_default(cterm, 1, 1);
	cterm_seq_default(cterm, 2, 1);
	cterm_seq_default(cterm, 3, cterm->height);
	cterm_seq_default(cterm, 4, cterm->width);
	t = (int)cterm->seq_param_int[1];
	l = (int)cterm->seq_param_int[2];
	b = (int)cterm->seq_param_int[3];
	r = (int)cterm->seq_param_int[4];
	if (t > b || l > r)
		return;
	if (t < CURR_MINY)
		t = CURR_MINY;
	if (l < CURR_MINX)
		l = CURR_MINX;
	if (b > CURR_MAXY)
		b = CURR_MAXY;
	if (r > CURR_MAXX)
		r = CURR_MAXX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &l, &t);
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &r, &b);
	cterm_fill_rect(cterm, l, t, r, b, ch);
}

void
cterm_handle_deccra(struct cterminal *cterm, int *speed)	/* CSI Pts;...;Ppd $ v */
{
	int st, sl, sb, sr, dt, dl;
	int sw, sh, i;
	struct vmem_cell *buf;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	cterm_seq_default(cterm, 1, 1);
	cterm_seq_default(cterm, 2, cterm->height);
	cterm_seq_default(cterm, 3, cterm->width);
	/* Pps (param 4) = source page, ignore */
	cterm_seq_default(cterm, 5, 1);
	cterm_seq_default(cterm, 6, 1);
	/* Ppd (param 7) = dest page, ignore */
	st = (int)cterm->seq_param_int[0];
	sl = (int)cterm->seq_param_int[1];
	sb = (int)cterm->seq_param_int[2];
	sr = (int)cterm->seq_param_int[3];
	dt = (int)cterm->seq_param_int[5];
	dl = (int)cterm->seq_param_int[6];
	if (st > sb || sl > sr)
		return;
	if (st < CURR_MINY)
		st = CURR_MINY;
	if (sl < CURR_MINX)
		sl = CURR_MINX;
	if (sb > CURR_MAXY)
		sb = CURR_MAXY;
	if (sr > CURR_MAXX)
		sr = CURR_MAXX;
	if (dt < CURR_MINY)
		dt = CURR_MINY;
	if (dl < CURR_MINX)
		dl = CURR_MINX;
	sw = sr - sl + 1;
	sh = sb - st + 1;
	if (dt + sh - 1 > CURR_MAXY)
		sh = CURR_MAXY - dt + 1;
	if (dl + sw - 1 > CURR_MAXX)
		sw = CURR_MAXX - dl + 1;
	if (sw <= 0 || sh <= 0)
		return;
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sl, &st);
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &dl, &dt);
	buf = calloc(sw, sizeof(*buf));
	if (buf) {
		for (i = 0; i < sh; i++) {
			vmem_gettext(sl, st + i, sl + sw - 1, st + i, buf);
			vmem_puttext(dl, dt + i, dl + sw - 1, dt + i, buf);
		}
		free(buf);
	}
}

void
cterm_handle_decic(struct cterminal *cterm, int *speed)	/* CSI Pn ' } */
{
	int cx, cy;
	int n, scx, scy, srx, sry, width, keep, c, r;
	struct vmem_cell *rowbuf;
	struct vmem_cell blank = {0};
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	TERM_XY(&cx, &cy);
	if (cx < TERM_MINX || cx > TERM_MAXX
	    || cy < TERM_MINY || cy > TERM_MAXY)
		return;
	n = (int)cterm->seq_param_int[0];
	scx = cx;
	scy = TERM_MINY;
	srx = TERM_MAXX;
	sry = TERM_MAXY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &scx, &scy);
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &srx, &sry);
	width = srx - scx + 1;
	if (n >= width) {
		cterm_fill_rect(cterm, scx, scy, srx, sry, ' ');
		return;
	}
	keep = width - n;
	rowbuf = calloc(width, sizeof(*rowbuf));
	if (rowbuf == NULL)
		return;
	blank.ch = ' ';
	blank.legacy_attr = cterm->attr;
	blank.fg = cterm->fg_color;
	blank.bg = cterm->bg_color;
	blank.font = ciolib_attrfont(cterm->attr);
	blank.hyperlink_id = 0;
	for (r = scy; r <= sry; r++) {
		vmem_gettext(scx, r, srx, r, rowbuf);
		memmove(&rowbuf[n], &rowbuf[0], keep * sizeof(*rowbuf));
		for (c = 0; c < n; c++)
			rowbuf[c] = blank;
		vmem_puttext(scx, r, srx, r, rowbuf);
	}
	free(rowbuf);
}

void
cterm_handle_decdc(struct cterminal *cterm, int *speed)	/* CSI Pn ' ~ */
{
	int cx, cy;
	int n, scx, scy, srx, sry, width, keep, c, r;
	struct vmem_cell *rowbuf;
	struct vmem_cell blank = {0};
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	TERM_XY(&cx, &cy);
	if (cx < TERM_MINX || cx > TERM_MAXX
	    || cy < TERM_MINY || cy > TERM_MAXY)
		return;
	n = (int)cterm->seq_param_int[0];
	scx = cx;
	scy = TERM_MINY;
	srx = TERM_MAXX;
	sry = TERM_MAXY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &scx, &scy);
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &srx, &sry);
	width = srx - scx + 1;
	if (n >= width) {
		cterm_fill_rect(cterm, scx, scy, srx, sry, ' ');
		return;
	}
	keep = width - n;
	rowbuf = calloc(width, sizeof(*rowbuf));
	if (rowbuf == NULL)
		return;
	blank.ch = ' ';
	blank.legacy_attr = cterm->attr;
	blank.fg = cterm->fg_color;
	blank.bg = cterm->bg_color;
	blank.font = ciolib_attrfont(cterm->attr);
	blank.hyperlink_id = 0;
	for (r = scy; r <= sry; r++) {
		vmem_gettext(scx, r, srx, r, rowbuf);
		memmove(&rowbuf[0], &rowbuf[n], keep * sizeof(*rowbuf));
		for (c = keep; c < width; c++)
			rowbuf[c] = blank;
		vmem_puttext(scx, r, srx, r, rowbuf);
	}
	free(rowbuf);
}

void
cterm_handle_decsace(struct cterminal *cterm, int *speed)	/* CSI Ps * x */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 0);
	cterm->decsace = (uint8_t)cterm->seq_param_int[0];
}

void
cterm_handle_deccara(struct cterminal *cterm, int *speed)	/* CSI Pt;Pl;Pb;Pr;...$ r */
{
	int top, left, bottom, right;
	int st, sl, sb, sr, bufsz;
	(void)speed;

	cterm_seq_default(cterm, 0, 1);
	cterm_seq_default(cterm, 1, 1);
	cterm_seq_default(cterm, 2, cterm->height);
	cterm_seq_default(cterm, 3, cterm->width);
	top = (int)cterm->seq_param_int[0];
	left = (int)cterm->seq_param_int[1];
	bottom = (int)cterm->seq_param_int[2];
	right = (int)cterm->seq_param_int[3];
	if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE) {
		top += cterm->top_margin - 1;
		bottom += cterm->top_margin - 1;
		left += cterm->left_margin - 1;
		right += cterm->left_margin - 1;
	}
	if (top < 1)
		top = 1;
	if (left < 1)
		left = 1;
	if (bottom > cterm->height)
		bottom = cterm->height;
	if (right > cterm->width)
		right = cterm->width;
	if (top > bottom || (cterm->decsace != 1 && left > right))
		return;

	{
		unsigned char save_attr = cterm->attr;
		uint32_t save_fg = cterm->fg_color;
		uint32_t save_bg = cterm->bg_color;
		bool save_neg = cterm->negative;
		char *save_fg_tc = cterm->fg_tc_str ? strdup(cterm->fg_tc_str) : NULL;
		char *save_bg_tc = cterm->bg_tc_str ? strdup(cterm->bg_tc_str) : NULL;
		struct vmem_cell *rowbuf;
		st = top; sl = left; sb = bottom; sr = right;
		cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &sl, &st);
		cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &sr, &sb);
		bufsz = (cterm->decsace == 1) ? cterm->width : (sr - sl + 1);
		rowbuf = calloc(bufsz, sizeof(*rowbuf));
		if (rowbuf) {
			for (int r = st; r <= sb; r++) {
				int c_start, c_end;
				if (cterm->decsace == 1) {
					c_start = (r == st) ? sl : cterm->x;
					c_end = (r == sb) ? sr : cterm->x + cterm->width - 1;
				}
				else {
					c_start = sl;
					c_end = sr;
				}
				vmem_gettext(c_start, r, c_end, r, rowbuf);
				int cnt = c_end - c_start + 1;
				for (int c = 0; c < cnt; c++) {
					cterm->attr = rowbuf[c].legacy_attr;
					cterm->fg_color = rowbuf[c].fg;
					cterm->bg_color = rowbuf[c].bg;
					cterm->negative = false;
					FREE_AND_NULL(cterm->fg_tc_str);
					FREE_AND_NULL(cterm->bg_tc_str);
					for (int p = 4; p < cterm->seq_param_count; p++)
						cterm_apply_sgr(cterm, &p);
					rowbuf[c].legacy_attr = cterm->attr;
					rowbuf[c].fg = cterm->fg_color;
					rowbuf[c].bg = cterm->bg_color;
					rowbuf[c].font = ciolib_attrfont(cterm->attr);
				}
				vmem_puttext(c_start, r, c_end, r, rowbuf);
			}
			free(rowbuf);
		}
		FREE_AND_NULL(cterm->fg_tc_str);
		FREE_AND_NULL(cterm->bg_tc_str);
		cterm->attr = save_attr;
		cterm->fg_color = save_fg;
		cterm->bg_color = save_bg;
		cterm->negative = save_neg;
		cterm->fg_tc_str = save_fg_tc;
		cterm->bg_tc_str = save_bg_tc;
	}
}

void
cterm_handle_decrara(struct cterminal *cterm, int *speed)	/* CSI Pt;Pl;Pb;Pr;...$ t */
{
	int top, left, bottom, right;
	int st, sl, sb, sr, bufsz;
	struct vmem_cell *rowbuf;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	cterm_seq_default(cterm, 1, 1);
	cterm_seq_default(cterm, 2, cterm->height);
	cterm_seq_default(cterm, 3, cterm->width);
	top = (int)cterm->seq_param_int[0];
	left = (int)cterm->seq_param_int[1];
	bottom = (int)cterm->seq_param_int[2];
	right = (int)cterm->seq_param_int[3];
	if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE) {
		top += cterm->top_margin - 1;
		bottom += cterm->top_margin - 1;
		left += cterm->left_margin - 1;
		right += cterm->left_margin - 1;
	}
	if (top < 1)
		top = 1;
	if (left < 1)
		left = 1;
	if (bottom > cterm->height)
		bottom = cterm->height;
	if (right > cterm->width)
		right = cterm->width;
	if (top > bottom)
		return;
	if (cterm->decsace != 1 && left > right)
		return;
	st = top;
	sl = left;
	sb = bottom;
	sr = right;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &sl, &st);
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &sr, &sb);
	bufsz = (cterm->decsace == 1) ? cterm->width : (sr - sl + 1);
	rowbuf = calloc(bufsz, sizeof(*rowbuf));
	if (rowbuf == NULL)
		return;
	for (int r = st; r <= sb; r++) {
		int c_start, c_end;
		if (cterm->decsace == 1) {
			c_start = (r == st) ? sl : cterm->x;
			c_end = (r == sb) ? sr : cterm->x + cterm->width - 1;
		}
		else {
			c_start = sl;
			c_end = sr;
		}
		vmem_gettext(c_start, r, c_end, r, rowbuf);
		int cnt = c_end - c_start + 1;
		for (int c = 0; c < cnt; c++) {
			for (int p = 4; p < cterm->seq_param_count; p++) {
				cterm_seq_default(cterm, p, 0);
				switch (cterm->seq_param_int[p]) {
					case 0:
						rowbuf[c].legacy_attr ^= 0x88;
						attr2palette(rowbuf[c].legacy_attr, &rowbuf[c].fg, NULL);
						attr2palette(rowbuf[c].legacy_attr, NULL, &rowbuf[c].bg);
						{
							uint32_t tfg = rowbuf[c].fg;
							rowbuf[c].fg = rowbuf[c].bg;
							rowbuf[c].bg = tfg;
						}
						break;
					case 1:
					case 22:
						rowbuf[c].legacy_attr ^= 0x08;
						attr2palette(rowbuf[c].legacy_attr, &rowbuf[c].fg, NULL);
						break;
					case 5:
					case 25:
						rowbuf[c].legacy_attr ^= 0x80;
						attr2palette(rowbuf[c].legacy_attr, NULL, &rowbuf[c].bg);
						break;
					case 7:
					case 27:
					{
						uint8_t a = rowbuf[c].legacy_attr;
						uint8_t fg_nibble = a & 0x0f;
						uint8_t bg_nibble = (a >> 4) & 0x0f;
						rowbuf[c].legacy_attr = (fg_nibble << 4) | bg_nibble;
						uint32_t tfg = rowbuf[c].fg;
						rowbuf[c].fg = rowbuf[c].bg;
						rowbuf[c].bg = tfg;
						break;
					}
				}
			}
			rowbuf[c].font = ciolib_attrfont(rowbuf[c].legacy_attr);
		}
		vmem_puttext(c_start, r, c_end, r, rowbuf);
	}
	free(rowbuf);
}

void
cterm_handle_decmacro(struct cterminal *cterm, int *speed)	/* CSI Pn * z */
{
	if (cterm->seq_param_count <= 0 || cterm->seq_param_int[0] > 63)
		return;
	uint64_t n = cterm->seq_param_int[0];
	if (!cterm->macros[n])
		return;
	if (cterm->in_macro & (UINT64_C(1) << n))
		return;
	cterm->escbuf[0] = 0;
	cterm->sequence = 0;
	cterm->accumulator = NULL;
	cterm->in_macro |= (UINT64_C(1) << n);
	cterm_write(cterm, cterm->macros[n], cterm->macro_lens[n], speed);
	cterm->in_macro &= ~(UINT64_C(1) << n);
}

void
cterm_handle_deccksr(struct cterminal *cterm, int *speed)	/* CSI Pid;1;Pt;Pl;Pb;Pr * y */
{
	struct text_info ti;
	(void)speed;
	if (cterm->seq_param_count < 6)
		return;
	if (cterm->seq_param_int[0] == UINT64_MAX
	    || cterm->seq_param_int[0] > UINT16_MAX
	    || cterm->seq_param_int[1] != 1
	    || cterm->seq_param_int[2] == UINT64_MAX
	    || cterm->seq_param_int[3] == UINT64_MAX
	    || cterm->seq_param_int[4] == UINT64_MAX
	    || cterm->seq_param_int[5] == UINT64_MAX
	    || cterm->seq_param_int[2] > cterm->seq_param_int[4]
	    || cterm->seq_param_int[3] > cterm->seq_param_int[5]
	    || cterm->seq_param_int[2] == 0
	    || cterm->seq_param_int[3] == 0
	    || cterm->seq_param_int[4] == 0
	    || cterm->seq_param_int[5] == 0)
		return;
	{
		struct ciolib_pixels *pix;
		uint16_t crc = 0;
		int good = 0;
		int vmode;
		gettextinfo(&ti);
		vmode = find_vmode(ti.currmode);
		if (vmode != -1
		    && (cterm->seq_param_int[3] < (uint64_t)(vparams[vmode].charwidth * cterm->width))
		    && (cterm->seq_param_int[2] < (uint64_t)(vparams[vmode].charheight * cterm->height))
		    && (cterm->seq_param_int[5] < (uint64_t)(vparams[vmode].charwidth * cterm->width))
		    && (cterm->seq_param_int[4] < (uint64_t)(vparams[vmode].charheight * cterm->height))
		    && (pix = getpixels(
		        (cterm->seq_param_int[3] - 1 + cterm->x - 1) * vparams[vmode].charwidth,
		        (cterm->seq_param_int[2] - 1 + cterm->y - 1) * vparams[vmode].charheight,
		        (cterm->seq_param_int[5] + cterm->x - 1) * vparams[vmode].charwidth - 1,
		        (cterm->seq_param_int[4] + cterm->y - 1) * vparams[vmode].charheight - 1,
		        true)) != NULL) {
			crc = crc16((void *)pix->pixels,
			    sizeof(pix->pixels[0]) * pix->width * pix->height);
			crc = icrc16(crc, (void *)pix->pixelsb,
			    sizeof(pix->pixelsb[0]) * pix->width * pix->height);
			good = 1;
			freepixels(pix);
		}
		else {
			size_t cnt = (cterm->seq_param_int[4] - cterm->seq_param_int[2] + 1)
			    * (cterm->seq_param_int[5] - cterm->seq_param_int[3] + 1);
			struct vmem_cell *vm = calloc(sizeof(struct vmem_cell), cnt);
			if (vm != NULL) {
				if (vmem_gettext(cterm->seq_param_int[3], cterm->seq_param_int[2],
				    cterm->seq_param_int[5], cterm->seq_param_int[4], vm)) {
					for (size_t cell = 0; cell < cnt; cell++) {
						struct vmem_cell vmc = vm[cell];
						memset(&vm[cell], 0, sizeof(vm[cell]));
						memcpy(&vm[cell].legacy_attr, &vmc.legacy_attr,
						    sizeof(vmc.legacy_attr));
						memcpy(&vm[cell].ch, &vmc.ch, sizeof(vmc.ch));
						memcpy(&vm[cell].font, &vmc.font, sizeof(vmc.font));
						memcpy(&vm[cell].fg, &vmc.fg, sizeof(vmc.fg));
						memcpy(&vm[cell].bg, &vmc.bg, sizeof(vmc.bg));
					}
					crc = crc16((void *)vm, sizeof(struct vmem_cell) * cnt);
					good = 1;
				}
				free(vm);
			}
		}
		if (good)
			cterm_respond_printf(cterm, "\x1bP%u!~%04X\x1b\\",
			    (unsigned)cterm->seq_param_int[0], crc);
	}
}

void
cterm_handle_decrqm_dec(struct cterminal *cterm, int *speed)	/* CSI ? $ p */
{
	int pm = 0;
	int vidflags;
	(void)speed;
	if (cterm->seq_param_count != 1)
		return;
	cterm_seq_default(cterm, 0, 0);
	vidflags = getvideoflags();
	switch (cterm->seq_param_int[0]) {
		case 6:
			pm = (cterm->extattr & CTERM_EXTATTR_ORIGINMODE) ? 1 : 2;
			break;
		case 7:
			pm = (cterm->extattr & CTERM_EXTATTR_AUTOWRAP) ? 1 : 2;
			break;
		case 25:
			pm = (cterm->cursor == _NORMALCURSOR) ? 1 : 2;
			break;
		case 31:
			pm = (vidflags & CIOLIB_VIDEO_ALTCHARS) ? 1 : 2;
			break;
		case 32:
			pm = (vidflags & CIOLIB_VIDEO_NOBRIGHT) ? 1 : 2;
			break;
		case 33:
			pm = (vidflags & CIOLIB_VIDEO_BGBRIGHT) ? 1 : 2;
			break;
		case 34:
			pm = (vidflags & CIOLIB_VIDEO_BLINKALTCHARS) ? 1 : 2;
			break;
		case 35:
			pm = (vidflags & CIOLIB_VIDEO_NOBLINK) ? 1 : 2;
			break;
		case 67:
			pm = (cterm->extattr & CTERM_EXTATTR_DECBKM) ? 1 : 2;
			break;
		case 69:
			pm = (cterm->extattr & CTERM_EXTATTR_DECLRMM) ? 1 : 2;
			break;
		case 80:
			pm = (cterm->extattr & CTERM_EXTATTR_SXSCROLL) ? 1 : 2;
			break;
		case 2004:
			pm = (cterm->extattr & CTERM_EXTATTR_BRACKETPASTE) ? 1 : 2;
			break;
		case 9:
		case 1000:
		case 1001:
		case 1002:
		case 1003:
		case 1004:
		case 1005:
		case 1006:
		case 1007:
		case 1015:
			if (cterm->mouse_state_query)
				pm = cterm->mouse_state_query((int)cterm->seq_param_int[0],
				    cterm->mouse_state_query_cbdata) ? 1 : 2;
			break;
	}
	cterm_respond_printf(cterm, "\x1b[?%u;%d$y",
	    (unsigned)cterm->seq_param_int[0], pm);
}

void
cterm_handle_save_dec_modes(struct cterminal *cterm, int *speed)	/* CSI ? s */
{
	struct text_info ti;
	int flags;
	int i;
	(void)speed;
	gettextinfo(&ti);
	flags = getvideoflags();
	if (cterm->seq_param_count == 0) {
		cterm->saved_mode_mask |= (CTERM_SAVEMODE_AUTOWRAP
		    | CTERM_SAVEMODE_CURSOR | CTERM_SAVEMODE_ALTCHARS
		    | CTERM_SAVEMODE_NOBRIGHT | CTERM_SAVEMODE_BGBRIGHT
		    | CTERM_SAVEMODE_ORIGIN | CTERM_SAVEMODE_SIXEL_SCROLL
		    | CTERM_SAVEMODE_MOUSE_X10 | CTERM_SAVEMODE_MOUSE_NORMAL
		    | CTERM_SAVEMODE_MOUSE_HIGHLIGHT
		    | CTERM_SAVEMODE_MOUSE_BUTTONTRACK
		    | CTERM_SAVEMODE_MOUSE_ANY | CTERM_SAVEMODE_MOUSE_FOCUS
		    | CTERM_SAVEMODE_MOUSE_UTF8 | CTERM_SAVEMODE_MOUSE_SGR
		    | CTERM_SAVEMODE_MOUSE_ALTSCROLL
		    | CTERM_SAVEMODE_MOUSE_URXVT
		    | CTERM_SAVEMODE_DECLRMM | CTERM_SAVEMODE_DECBKM);
		cterm->saved_mode &= ~cterm->saved_mode_mask;
		cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_AUTOWRAP) ? CTERM_SAVEMODE_AUTOWRAP : 0;
		cterm->saved_mode |= (cterm->cursor == _NORMALCURSOR) ? CTERM_SAVEMODE_CURSOR : 0;
		cterm->saved_mode |= (flags & CIOLIB_VIDEO_ALTCHARS) ? CTERM_SAVEMODE_ALTCHARS : 0;
		cterm->saved_mode |= (flags & CIOLIB_VIDEO_NOBRIGHT) ? CTERM_SAVEMODE_NOBRIGHT : 0;
		cterm->saved_mode |= (flags & CIOLIB_VIDEO_BGBRIGHT) ? CTERM_SAVEMODE_BGBRIGHT : 0;
		cterm->saved_mode |= (flags & CIOLIB_VIDEO_BLINKALTCHARS) ? CTERM_SAVEMODE_BLINKALTCHARS : 0;
		cterm->saved_mode |= (flags & CIOLIB_VIDEO_NOBLINK) ? CTERM_SAVEMODE_NOBLINK : 0;
		cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_ORIGINMODE) ? CTERM_SAVEMODE_ORIGIN : 0;
		cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_SXSCROLL) ? CTERM_SAVEMODE_SIXEL_SCROLL : 0;
		cterm->saved_mode |= (cterm->mouse_state_query(9, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_X10 : 0);
		cterm->saved_mode |= (cterm->mouse_state_query(1000, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_NORMAL : 0);
		cterm->saved_mode |= (cterm->mouse_state_query(1001, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_HIGHLIGHT : 0);
		cterm->saved_mode |= (cterm->mouse_state_query(1002, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_BUTTONTRACK : 0);
		cterm->saved_mode |= (cterm->mouse_state_query(1003, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_ANY : 0);
		cterm->saved_mode |= (cterm->mouse_state_query(1004, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_FOCUS : 0);
		cterm->saved_mode |= (cterm->mouse_state_query(1005, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_UTF8 : 0);
		cterm->saved_mode |= (cterm->mouse_state_query(1006, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_SGR : 0);
		cterm->saved_mode |= (cterm->mouse_state_query(1007, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_ALTSCROLL : 0);
		cterm->saved_mode |= (cterm->mouse_state_query(1015, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_URXVT : 0);
		cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_DECLRMM) ? CTERM_SAVEMODE_DECLRMM : 0;
		cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_DECBKM) ? CTERM_SAVEMODE_DECBKM : 0;
		setwindow(cterm);
		return;
	}
	for (i = 0; i < cterm->seq_param_count; i++) {
		cterm_seq_default(cterm, i, 0);
		switch (cterm->seq_param_int[i]) {
			case 6:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_ORIGIN;
				cterm->saved_mode &= ~CTERM_SAVEMODE_ORIGIN;
				cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_ORIGINMODE) ? CTERM_SAVEMODE_ORIGIN : 0;
				setwindow(cterm);
				break;
			case 7:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_AUTOWRAP;
				cterm->saved_mode &= ~CTERM_SAVEMODE_AUTOWRAP;
				cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_AUTOWRAP) ? CTERM_SAVEMODE_AUTOWRAP : 0;
				break;
			case 9:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_X10;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_X10;
				cterm->saved_mode |= (cterm->mouse_state_query(9, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_X10 : 0);
				break;
			case 25:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_CURSOR;
				cterm->saved_mode &= ~CTERM_SAVEMODE_CURSOR;
				cterm->saved_mode |= (cterm->cursor == _NORMALCURSOR) ? CTERM_SAVEMODE_CURSOR : 0;
				break;
			case 31:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_ALTCHARS;
				cterm->saved_mode &= ~CTERM_SAVEMODE_ALTCHARS;
				cterm->saved_mode |= (flags & CIOLIB_VIDEO_ALTCHARS) ? CTERM_SAVEMODE_ALTCHARS : 0;
				break;
			case 32:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_NOBRIGHT;
				cterm->saved_mode &= ~CTERM_SAVEMODE_NOBRIGHT;
				cterm->saved_mode |= (flags & CIOLIB_VIDEO_NOBRIGHT) ? CTERM_SAVEMODE_NOBRIGHT : 0;
				break;
			case 33:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_BGBRIGHT;
				cterm->saved_mode &= ~CTERM_SAVEMODE_BGBRIGHT;
				cterm->saved_mode |= (flags & CIOLIB_VIDEO_BGBRIGHT) ? CTERM_SAVEMODE_BGBRIGHT : 0;
				break;
			case 34:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_BLINKALTCHARS;
				cterm->saved_mode &= ~CTERM_SAVEMODE_BLINKALTCHARS;
				cterm->saved_mode |= (flags & CIOLIB_VIDEO_BLINKALTCHARS) ? CTERM_SAVEMODE_BLINKALTCHARS : 0;
				break;
			case 35:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_NOBLINK;
				cterm->saved_mode &= ~CTERM_SAVEMODE_NOBLINK;
				cterm->saved_mode |= (flags & CIOLIB_VIDEO_NOBLINK) ? CTERM_SAVEMODE_NOBLINK : 0;
				break;
			case 67:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_DECBKM;
				cterm->saved_mode &= ~CTERM_SAVEMODE_DECBKM;
				cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_DECBKM) ? CTERM_SAVEMODE_DECBKM : 0;
				break;
			case 69:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_DECLRMM;
				cterm->saved_mode &= ~CTERM_SAVEMODE_DECLRMM;
				cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_DECLRMM) ? CTERM_SAVEMODE_DECLRMM : 0;
				break;
			case 80:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_SIXEL_SCROLL;
				cterm->saved_mode &= ~CTERM_SAVEMODE_SIXEL_SCROLL;
				cterm->saved_mode |= (cterm->extattr & CTERM_EXTATTR_SXSCROLL) ? CTERM_SAVEMODE_SIXEL_SCROLL : 0;
				break;
			case 1000:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_NORMAL;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_NORMAL;
				cterm->saved_mode |= (cterm->mouse_state_query(1000, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_NORMAL : 0);
				break;
			case 1001:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_HIGHLIGHT;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_HIGHLIGHT;
				cterm->saved_mode |= (cterm->mouse_state_query(1001, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_HIGHLIGHT : 0);
				break;
			case 1002:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_BUTTONTRACK;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_BUTTONTRACK;
				cterm->saved_mode |= (cterm->mouse_state_query(1002, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_BUTTONTRACK : 0);
				break;
			case 1003:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_ANY;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_ANY;
				cterm->saved_mode |= (cterm->mouse_state_query(1003, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_ANY : 0);
				break;
			case 1004:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_FOCUS;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_FOCUS;
				cterm->saved_mode |= (cterm->mouse_state_query(1004, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_FOCUS : 0);
				break;
			case 1005:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_UTF8;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_UTF8;
				cterm->saved_mode |= (cterm->mouse_state_query(1005, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_UTF8 : 0);
				break;
			case 1006:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_SGR;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_SGR;
				cterm->saved_mode |= (cterm->mouse_state_query(1006, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_SGR : 0);
				break;
			case 1007:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_ALTSCROLL;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_ALTSCROLL;
				cterm->saved_mode |= (cterm->mouse_state_query(1007, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_ALTSCROLL : 0);
				break;
			case 1015:
				cterm->saved_mode_mask |= CTERM_SAVEMODE_MOUSE_URXVT;
				cterm->saved_mode &= ~CTERM_SAVEMODE_MOUSE_URXVT;
				cterm->saved_mode |= (cterm->mouse_state_query(1015, cterm->mouse_state_query_cbdata) ? CTERM_SAVEMODE_MOUSE_URXVT : 0);
				break;
		}
	}
}

void
cterm_handle_restore_dec_modes(struct cterminal *cterm, int *speed)	/* CSI ? u */
{
	struct text_info ti;
	int flags, i;
	uint32_t oldfg, oldbg;
	bool updfg, updbg;
	(void)speed;
	gettextinfo(&ti);
	flags = getvideoflags();
	attr2palette(cterm->attr, &oldfg, &oldbg);
	updfg = (oldfg == cterm->fg_color);
	updbg = (oldbg == cterm->bg_color);
	if (cterm->seq_param_count == 0) {
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_ORIGIN) {
			if (cterm->saved_mode & CTERM_SAVEMODE_ORIGIN)
				cterm->extattr |= CTERM_EXTATTR_ORIGINMODE;
			else
				cterm->extattr &= ~CTERM_EXTATTR_ORIGINMODE;
			setwindow(cterm);
		}
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_AUTOWRAP) {
			if (cterm->saved_mode & CTERM_SAVEMODE_AUTOWRAP)
				cterm->extattr |= CTERM_EXTATTR_AUTOWRAP;
			else
				cterm->extattr &= ~CTERM_EXTATTR_AUTOWRAP;
		}
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_SIXEL_SCROLL) {
			if (cterm->saved_mode & CTERM_SAVEMODE_SIXEL_SCROLL)
				cterm->extattr |= CTERM_EXTATTR_SXSCROLL;
			else
				cterm->extattr &= ~CTERM_EXTATTR_SXSCROLL;
		}
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_CURSOR) {
			cterm->cursor = (cterm->saved_mode & CTERM_SAVEMODE_CURSOR) ? _NORMALCURSOR : _NOCURSOR;
			ciolib_setcursortype(cterm->cursor);
		}
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_ALTCHARS) {
			if (cterm->saved_mode & CTERM_SAVEMODE_ALTCHARS)
				flags |= CIOLIB_VIDEO_ALTCHARS;
			else
				flags &= ~CIOLIB_VIDEO_ALTCHARS;
		}
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_BLINKALTCHARS) {
			if (cterm->saved_mode & CTERM_SAVEMODE_BLINKALTCHARS)
				flags |= CIOLIB_VIDEO_BLINKALTCHARS;
			else
				flags &= ~CIOLIB_VIDEO_BLINKALTCHARS;
		}
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_NOBRIGHT) {
			if (cterm->saved_mode & CTERM_SAVEMODE_NOBRIGHT)
				flags |= CIOLIB_VIDEO_NOBRIGHT;
			else
				flags &= ~CIOLIB_VIDEO_NOBRIGHT;
		}
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_NOBLINK) {
			if (cterm->saved_mode & CTERM_SAVEMODE_NOBLINK)
				flags |= CIOLIB_VIDEO_NOBLINK;
			else
				flags &= ~CIOLIB_VIDEO_NOBLINK;
		}
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_BGBRIGHT) {
			if (cterm->saved_mode & CTERM_SAVEMODE_BGBRIGHT)
				flags |= CIOLIB_VIDEO_BGBRIGHT;
			else
				flags &= ~CIOLIB_VIDEO_BGBRIGHT;
		}
		setvideoflags(flags);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_X10)
			cterm->mouse_state_change(9, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_X10, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_NORMAL)
			cterm->mouse_state_change(1000, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_NORMAL, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_HIGHLIGHT)
			cterm->mouse_state_change(1001, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_HIGHLIGHT, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_BUTTONTRACK)
			cterm->mouse_state_change(1002, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_BUTTONTRACK, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_ANY)
			cterm->mouse_state_change(1003, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_ANY, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_FOCUS)
			cterm->mouse_state_change(1004, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_FOCUS, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_UTF8)
			cterm->mouse_state_change(1005, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_UTF8, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_SGR)
			cterm->mouse_state_change(1006, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_SGR, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_ALTSCROLL)
			cterm->mouse_state_change(1007, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_ALTSCROLL, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_URXVT)
			cterm->mouse_state_change(1015, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_URXVT, cterm->mouse_state_change_cbdata);
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_DECBKM) {
			if (cterm->saved_mode & CTERM_SAVEMODE_DECBKM)
				cterm->extattr |= CTERM_EXTATTR_DECBKM;
			else
				cterm->extattr &= ~CTERM_EXTATTR_DECBKM;
		}
		if (cterm->saved_mode_mask & CTERM_SAVEMODE_DECLRMM) {
			if (cterm->saved_mode & CTERM_SAVEMODE_DECLRMM)
				cterm->extattr |= CTERM_EXTATTR_DECLRMM;
			else
				cterm->extattr &= ~CTERM_EXTATTR_DECLRMM;
		}
		goto finish;
	}
	for (i = 0; i < cterm->seq_param_count; i++) {
		cterm_seq_default(cterm, i, 0);
		switch (cterm->seq_param_int[i]) {
			case 6:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_ORIGIN) {
					if (cterm->saved_mode & CTERM_SAVEMODE_ORIGIN)
						cterm->extattr |= CTERM_EXTATTR_ORIGINMODE;
					else
						cterm->extattr &= ~CTERM_EXTATTR_ORIGINMODE;
				}
				setwindow(cterm);
				break;
			case 7:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_AUTOWRAP) {
					if (cterm->saved_mode & CTERM_SAVEMODE_AUTOWRAP)
						cterm->extattr |= CTERM_EXTATTR_AUTOWRAP;
					else
						cterm->extattr &= ~CTERM_EXTATTR_AUTOWRAP;
				}
				break;
			case 9:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_X10)
					cterm->mouse_state_change(9, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_X10, cterm->mouse_state_change_cbdata);
				break;
			case 25:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_CURSOR) {
					cterm->cursor = (cterm->saved_mode & CTERM_SAVEMODE_CURSOR) ? _NORMALCURSOR : _NOCURSOR;
					ciolib_setcursortype(cterm->cursor);
				}
				break;
			case 31:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_ALTCHARS) {
					if (cterm->saved_mode & CTERM_SAVEMODE_ALTCHARS)
						flags |= CIOLIB_VIDEO_ALTCHARS;
					else
						flags &= ~CIOLIB_VIDEO_ALTCHARS;
					setvideoflags(flags);
				}
				break;
			case 32:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_NOBRIGHT) {
					if (cterm->saved_mode & CTERM_SAVEMODE_NOBRIGHT)
						flags |= CIOLIB_VIDEO_NOBRIGHT;
					else
						flags &= ~CIOLIB_VIDEO_NOBRIGHT;
					setvideoflags(flags);
				}
				break;
			case 33:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_BGBRIGHT) {
					if (cterm->saved_mode & CTERM_SAVEMODE_BGBRIGHT)
						flags |= CIOLIB_VIDEO_BGBRIGHT;
					else
						flags &= ~CIOLIB_VIDEO_BGBRIGHT;
					setvideoflags(flags);
				}
				break;
			case 34:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_BLINKALTCHARS) {
					if (cterm->saved_mode & CTERM_SAVEMODE_BLINKALTCHARS)
						flags |= CIOLIB_VIDEO_BLINKALTCHARS;
					else
						flags &= ~CIOLIB_VIDEO_BLINKALTCHARS;
					setvideoflags(flags);
				}
				break;
			case 35:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_NOBLINK) {
					if (cterm->saved_mode & CTERM_SAVEMODE_NOBLINK)
						flags |= CIOLIB_VIDEO_NOBLINK;
					else
						flags &= ~CIOLIB_VIDEO_NOBLINK;
					setvideoflags(flags);
				}
				break;
			case 67:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_DECBKM) {
					if (cterm->saved_mode & CTERM_SAVEMODE_DECBKM)
						cterm->extattr |= CTERM_EXTATTR_DECBKM;
					else
						cterm->extattr &= ~CTERM_EXTATTR_DECBKM;
				}
				break;
			case 69:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_DECLRMM) {
					if (cterm->saved_mode & CTERM_SAVEMODE_DECLRMM)
						cterm->extattr |= CTERM_EXTATTR_DECLRMM;
					else
						cterm->extattr &= ~CTERM_EXTATTR_DECLRMM;
				}
				break;
			case 80:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_SIXEL_SCROLL) {
					if (cterm->saved_mode & CTERM_SAVEMODE_SIXEL_SCROLL)
						cterm->extattr |= CTERM_EXTATTR_SXSCROLL;
					else
						cterm->extattr &= ~CTERM_EXTATTR_SXSCROLL;
				}
				break;
			case 1000:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_NORMAL)
					cterm->mouse_state_change(1000, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_NORMAL, cterm->mouse_state_change_cbdata);
				break;
			case 1001:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_HIGHLIGHT)
					cterm->mouse_state_change(1001, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_HIGHLIGHT, cterm->mouse_state_change_cbdata);
				break;
			case 1002:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_BUTTONTRACK)
					cterm->mouse_state_change(1002, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_BUTTONTRACK, cterm->mouse_state_change_cbdata);
				break;
			case 1003:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_ANY)
					cterm->mouse_state_change(1003, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_ANY, cterm->mouse_state_change_cbdata);
				break;
			case 1004:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_FOCUS)
					cterm->mouse_state_change(1004, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_FOCUS, cterm->mouse_state_change_cbdata);
				break;
			case 1005:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_UTF8)
					cterm->mouse_state_change(1005, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_UTF8, cterm->mouse_state_change_cbdata);
				break;
			case 1006:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_SGR)
					cterm->mouse_state_change(1006, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_SGR, cterm->mouse_state_change_cbdata);
				break;
			case 1007:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_ALTSCROLL)
					cterm->mouse_state_change(1007, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_ALTSCROLL, cterm->mouse_state_change_cbdata);
				break;
			case 1015:
				if (cterm->saved_mode_mask & CTERM_SAVEMODE_MOUSE_URXVT)
					cterm->mouse_state_change(1015, cterm->saved_mode & CTERM_SAVEMODE_MOUSE_URXVT, cterm->mouse_state_change_cbdata);
				break;
		}
	}
finish:
	if (updfg || updbg) {
		attr2palette(cterm->attr, updfg ? &cterm->fg_color : NULL,
		    updbg ? &cterm->bg_color : NULL);
		if (updfg) {
			if (cterm->negative)
				FREE_AND_NULL(cterm->bg_tc_str);
			else
				FREE_AND_NULL(cterm->fg_tc_str);
		}
		if (updbg) {
			if (cterm->negative)
				FREE_AND_NULL(cterm->fg_tc_str);
			else
				FREE_AND_NULL(cterm->bg_tc_str);
		}
	}
}

void
cterm_handle_decsc(struct cterminal *cterm, int *speed)	/* ESC 7 */
{
	(void)speed;
	CURR_XY(&cterm->save_xpos, &cterm->save_ypos);
}

void
cterm_handle_decrc(struct cterminal *cterm, int *speed)	/* ESC 8 */
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

void
cterm_handle_decsm(struct cterminal *cterm, int *speed)	/* CSI ? h */
{
	int i;
	(void)speed;
	/* Process DEC / XTerm DECSET modes.  Parameters already consumed by a
	 * prior handler in the cascade are skipped. */
	for (i = 0; i < cterm->seq_param_count; i++) {
		if (cterm_seq_param_consumed(cterm, i))
			continue;
		cterm_seq_default(cterm, i, 0);
		switch (cterm->seq_param_int[i]) {
			case 6:
				cterm_clear_lcf(cterm);
				cterm->extattr |= CTERM_EXTATTR_ORIGINMODE;
				cterm_clear_selection(cterm);
				setwindow(cterm);
				break;
			case 7:
				cterm->extattr |= CTERM_EXTATTR_AUTOWRAP;
				break;
			case 25:
				cterm->cursor = _NORMALCURSOR;
				ciolib_setcursortype(cterm->cursor);
				break;
			case 67:
				cterm->extattr |= CTERM_EXTATTR_DECBKM;
				break;
			case 69:
				cterm->extattr |= CTERM_EXTATTR_DECLRMM;
				break;
			case 9:
			case 1000:
			case 1001:
			case 1002:
			case 1003:
			case 1004:
			case 1005:
			case 1006:
			case 1007:
			case 1015:
				if (cterm->mouse_state_change)
					cterm->mouse_state_change((int)cterm->seq_param_int[i], 1,
					    cterm->mouse_state_change_cbdata);
				break;
			case 2004:
				cterm->extattr |= CTERM_EXTATTR_BRACKETPASTE;
				break;
			default:
				/* Unknown to this layer; leave unmarked so future
				 * layers (if any) can claim it. */
				continue;
		}
		cterm_seq_consume_param(cterm, i);
	}
}

void
cterm_handle_decrm(struct cterminal *cterm, int *speed)	/* CSI ? l */
{
	int i;
	(void)speed;
	for (i = 0; i < cterm->seq_param_count; i++) {
		if (cterm_seq_param_consumed(cterm, i))
			continue;
		cterm_seq_default(cterm, i, 0);
		switch (cterm->seq_param_int[i]) {
			case 6:
				cterm_clear_lcf(cterm);
				cterm->extattr &= ~CTERM_EXTATTR_ORIGINMODE;
				cterm_clear_selection(cterm);
				setwindow(cterm);
				break;
			case 7:
				cterm_clear_lcf(cterm);
				cterm->extattr &= ~CTERM_EXTATTR_AUTOWRAP;
				break;
			case 25:
				cterm->cursor = _NOCURSOR;
				ciolib_setcursortype(cterm->cursor);
				break;
			case 67:
				cterm->extattr &= ~CTERM_EXTATTR_DECBKM;
				break;
			case 69:
				cterm->extattr &= ~CTERM_EXTATTR_DECLRMM;
				break;
			case 9:
			case 1000:
			case 1001:
			case 1002:
			case 1003:
			case 1004:
			case 1005:
			case 1006:
			case 1007:
			case 1015:
				if (cterm->mouse_state_change)
					cterm->mouse_state_change((int)cterm->seq_param_int[i], 0,
					    cterm->mouse_state_change_cbdata);
				break;
			case 2004:
				cterm->extattr &= ~CTERM_EXTATTR_BRACKETPASTE;
				break;
			default:
				continue;
		}
		cterm_seq_consume_param(cterm, i);
	}
}

void
cterm_handle_decslrm(struct cterminal *cterm, int *speed)	/* CSI Pl ; Pr s */
{
	int col, max_col;
	(void)speed;
	cterm_seq_default(cterm, 0, ABS_MINX);
	cterm_seq_default(cterm, 1, ABS_MAXX);
	col = (int)cterm->seq_param_int[0];
	if (col == 0)
		col = cterm->left_margin;
	max_col = (int)cterm->seq_param_int[1];
	if (max_col == 0)
		max_col = cterm->right_margin;
	if (col >= ABS_MINX && max_col > col && max_col <= ABS_MAXX) {
		cterm->left_margin = col;
		cterm->right_margin = max_col;
		cterm_clear_selection(cterm);
		setwindow(cterm);
		gotoxy(CURR_MINX, CURR_MINY);
	}
}

void
cterm_handle_decstbm(struct cterminal *cterm, int *speed)	/* CSI Pt ; Pb r */
{
	int row, max_row;
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	cterm_seq_default(cterm, 1, cterm->height);
	row = (int)cterm->seq_param_int[0];
	max_row = (int)cterm->seq_param_int[1];
	if (row >= ABS_MINY && max_row > row && max_row <= ABS_MAXY) {
		cterm->top_margin = row;
		cterm->bottom_margin = max_row;
		cterm_clear_selection(cterm);
		setwindow(cterm);
		gotoxy(CURR_MINX, CURR_MINY);
	}
}

/* ========================================================================
 * DCS string parsers — Sixel + DECDMAC
 *
 * Both are DEC extensions transported inside the ECMA-48 DCS framing
 * (ESC P … ESC \).  The generic string accumulator in cterm.c captures
 * the payload into cterm->strbuf; cterm_dec_dcs_byte() is invoked once
 * per byte so these parsers can work incrementally (sixel image data
 * can be megabytes; DECDMAC macro bodies similarly).  On ST the final
 * pass runs via cterm_dec_dcs_finish() below.
 * ======================================================================== */

/* Scan s for the length of a prefix of bytes in [min..max] excluding
 * `exclude`.  Used by the sixel/macro intro parsers to locate the final
 * byte past a run of parameter digits. */
static size_t
range_span_exclude(char exclude, const char *s, char min, char max)
{
	size_t ret = 0;

	while ((s[ret] >= min && s[ret] <= max) && s[ret] != exclude)
		ret++;
	return ret;
}

void cterm_parse_sixel_string(struct cterminal *cterm, bool finish)
{
	char *p = cterm->strbuf;
	char *end;
	int i, j, k;
	int vmode;
	int pos;
	struct text_info ti;
 	int palette_offset = 0;

	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
		palette_offset = 16;

	if (cterm->strbuflen == 0) {
		if (finish)
			goto all_done;
		return;
	}

	end = p+cterm->strbuflen-1;

	if ((*end < '?' || *end > '~') && !finish)
		return;

	/*
	 * TODO: Sixel interaction with scrolling margins...
	 * This one is interesting because there is no real terminal
	 * which supported left/right margins *and* sixel graphics.
	 */
	while (p <= end) {
		if (*p >= '?' && *p <= '~') {
			unsigned data = *p - '?';

			cterm->sx_pixels_sent = 1;
			gettextinfo(&ti);
			vmode = find_vmode(ti.currmode);
			if (vmode == -1)
				return;
			if (cterm->sx_pixels == NULL) {
				cterm->sx_pixels = malloc(sizeof(struct ciolib_pixels));
				if (cterm->sx_pixels != NULL) {
					cterm->sx_pixels->pixels = malloc(sizeof(cterm->sx_pixels->pixels[0]) * cterm->sx_iv * ti.screenwidth * vparams[vmode].charwidth * 6);
					if (cterm->sx_pixels->pixels != NULL) {
						cterm->sx_pixels->pixelsb = NULL;
						cterm->sx_pixels->width = ti.screenwidth * vparams[vmode].charwidth;
						cterm->sx_pixels->height = cterm->sx_iv * 6;
						cterm->sx_mask = malloc(sizeof(struct ciolib_mask));
						if (cterm->sx_mask != NULL) {
							cterm->sx_mask->width = cterm->sx_pixels->width;
							cterm->sx_mask->height = cterm->sx_pixels->height;
							cterm->sx_mask->bits = malloc((cterm->sx_iv * 6 * ti.screenwidth * vparams[vmode].charwidth + 7)/8);
							if (cterm->sx_mask->bits != NULL)
								memset(cterm->sx_mask->bits, 0, (cterm->sx_iv * 6 * ti.screenwidth * vparams[vmode].charwidth + 7)/8);
							else {
								FREE_AND_NULL(cterm->sx_mask);
								free(cterm->sx_pixels->pixels);
								FREE_AND_NULL(cterm->sx_pixels);
							}
						}
						else {
							free(cterm->sx_pixels->pixels);
							FREE_AND_NULL(cterm->sx_pixels);
						}
					}
					else {
						FREE_AND_NULL(cterm->sx_pixels);
					}
				}
			}
			if (cterm->sx_pixels == NULL) {
				fprintf(stderr, "Error allocating memory for sixel data\n");
				return;
			}
			if (cterm->sx_x == cterm->sx_left && cterm->sx_height && cterm->sx_width && cterm->sx_first_pass && cterm->sx_x + cterm->sx_ih <= cterm->sx_pixels->width) {
				/* Fill in the background of the line */
				for (i = 0; i < (cterm->sx_height > 6 ? 6 : cterm->sx_height); i++) {
					for (j = 0; j < cterm->sx_iv; j++) {
						for (k = 0; k < cterm->sx_ih; k++) {
							pos = i * cterm->sx_iv * cterm->sx_pixels->width + j * cterm->sx_pixels->width + cterm->sx_x + k;
							cterm->sx_pixels->pixels[pos] = cterm->sx_bg;
							cterm->sx_mask->bits[pos/8] |= (0x80 >> (pos % 8));
						}
					}
				}
			}
			if (cterm->sx_x + cterm->sx_ih <= ti.screenwidth * vparams[vmode].charwidth) {
				for (i=0; i<6; i++) {
					if (data & (1<<i)) {
						for (j = 0; j < cterm->sx_iv; j++) {
							for (k = 0; k < cterm->sx_ih; k++) {
								pos = i * cterm->sx_iv * cterm->sx_pixels->width + j * cterm->sx_pixels->width + cterm->sx_x + k;
								cterm->sx_pixels->pixels[pos] = cterm->sx_fg;
								cterm->sx_mask->bits[pos/8] |= (0x80 >> (pos % 8));
							}
						}
					}
					else {
						if (cterm->sx_first_pass && !cterm->sx_trans) {
							for (j = 0; j < cterm->sx_iv; j++) {
								for (k = 0; k < cterm->sx_ih; k++) {
									pos = i * cterm->sx_iv * cterm->sx_pixels->width + j * cterm->sx_pixels->width + cterm->sx_x + k;
									cterm->sx_pixels->pixels[pos] = cterm->sx_bg;
									cterm->sx_mask->bits[pos/8] |= (0x80 >> (pos % 8));
								}
							}
						}
						else {
							for (j = 0; j < cterm->sx_iv; j++) {
								for (k = 0; k < cterm->sx_ih; k++) {
									pos = i * cterm->sx_iv * cterm->sx_pixels->width + j * cterm->sx_pixels->width + cterm->sx_x + k;
									if (cterm->sx_first_pass)
										cterm->sx_mask->bits[pos/8] &= ~(0x80 >> (pos % 8));
								}
							}
						}
					}
				}
				if (cterm->sx_x > cterm->sx_row_max_x)
					cterm->sx_row_max_x = cterm->sx_x;
			}

			cterm->sx_x+=cterm->sx_ih;
			if (cterm->sx_repeat)
				cterm->sx_repeat--;
			if (!cterm->sx_repeat)
				p++;
		}
		else {
			switch(*p) {
				case '"':	// Raster Attributes
					if (!cterm->sx_pixels_sent) {
						p++;
						cterm->sx_iv = strtoul(p, &p, 10);
						cterm->sx_height = cterm->sx_width = 0;
						if (*p == ';') {
							p++;
							cterm->sx_ih = strtoul(p, &p, 10);
						}
						if (*p == ';') {
							p++;
							cterm->sx_width = strtoul(p, &p, 10);
						}
						if (*p == ';') {
							p++;
							cterm->sx_height = strtoul(p, &p, 10);
						}
						gettextinfo(&ti);
						vmode = find_vmode(ti.currmode);
						if (vmode != -1) {
							int max_w = ti.screenwidth * vparams[vmode].charwidth;
							int max_h = ti.screenheight * vparams[vmode].charheight;
							if (cterm->sx_iv < 1)
								cterm->sx_iv = 1;
							if (cterm->sx_iv > max_h)
								cterm->sx_iv = max_h;
							if (cterm->sx_ih < 1)
								cterm->sx_ih = 1;
							if (cterm->sx_ih > max_w)
								cterm->sx_ih = max_w;
						}
					}
					else
						p++;
					break;
				case '!':	// Repeat
					p++;
					if (!*p)
						continue;
					cterm->sx_repeat = strtoul(p, &p, 10);
					if (cterm->sx_repeat > 0x7fff)
						cterm->sx_repeat = 0x7fff;
					break;
				case '#':	// Colour Introducer
					p++;
					if (!*p)
						continue;
					cterm->sx_fg = strtoul(p, &p, 10) + TOTAL_DAC_SIZE + palette_offset;
					/* Do we want to redefine it while we're here? */
					if (*p == ';') {
						unsigned long t,r,g,b;

						p++;
						r=g=b=0;
						t = strtoul(p, &p, 10);
						if (*p == ';') {
							p++;
							r = strtoul(p, &p, 10);
						}
						if (*p == ';') {
							p++;
							g = strtoul(p, &p, 10);
						}
						if (*p == ';') {
							p++;
							b = strtoul(p, &p, 10);
						}
						if (t == 2)	// Only support RGB
							cterm_setpalette(cterm, cterm->sx_fg, UINT16_MAX * r / 100, UINT16_MAX * g / 100, UINT16_MAX * b / 100);
					}
					break;
				case '$':	// Graphics Carriage Return
					cterm->sx_x = cterm->sx_left;
					cterm->sx_first_pass = 0;
					p++;
					break;
				case '-':	// Graphics New Line
					{
						int max_row = TERM_MAXY;
						cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_CURR, &max_row, NULL);
						gettextinfo(&ti);
						vmode = find_vmode(ti.currmode);

						if (vmode == -1)
							return;
						setpixels(cterm->sx_left, cterm->sx_y, cterm->sx_row_max_x, cterm->sx_y + 6 * cterm->sx_iv - 1, cterm->sx_left, 0, cterm->sx_left, 0, cterm->sx_pixels, cterm->sx_mask);
						cterm->sx_row_max_x = 0;

						if ((!(cterm->extattr & CTERM_EXTATTR_SXSCROLL)) && (((cterm->sx_y + 6 * cterm->sx_iv) + 6*cterm->sx_iv - 1) >= (cterm->y + max_row - 1) * vparams[vmode].charheight)) {
							p++;
							break;
						}

						cterm->sx_x = cterm->sx_left;
						cterm->sx_y += 6*cterm->sx_iv;
						if (cterm->sx_height)
							cterm->sx_height -= cterm->sx_height > 6 ? 6 : cterm->sx_height;
						while ((cterm->sx_y + 6 * cterm->sx_iv - 1) >= (cterm->y + max_row - 1) * vparams[vmode].charheight) {
							cterm_cond_scrollup(cterm);
							cterm->sx_y -= vparams[vmode].charheight;
						}
						cterm->sx_first_pass = 1;
						p++;
					}
					break;
				default:
					p++;
			}
		}
	}
	cterm->strbuflen = 0;
	if (finish)
		goto all_done;
	return;

all_done:
	gettextinfo(&ti);
	vmode = find_vmode(ti.currmode);

	if (cterm->sx_row_max_x) {
		setpixels(cterm->sx_left, cterm->sx_y, cterm->sx_row_max_x, cterm->sx_y + 6 * cterm->sx_iv - 1, cterm->sx_left, 0, cterm->sx_left, 0, cterm->sx_pixels, cterm->sx_mask);
	}

	hold_update=cterm->sx_hold_update;

	/* Finish off the background */
	cterm->sx_x = cterm->sx_left;
	cterm->sx_y += 6 * cterm->sx_iv;
	if (cterm->sx_height)
		cterm->sx_height -= cterm->sx_height > 6 ? 6 : cterm->sx_height;

	if (cterm->sx_height && cterm->sx_width) {
		struct ciolib_pixels px;

		px.pixels = malloc(sizeof(px.pixels[0])*cterm->sx_width*cterm->sx_height*cterm->sx_iv*cterm->sx_ih);
		if (px.pixels) {
			px.height = cterm->sx_height;
			px.width = cterm->sx_width;
			for (i = 0; i<cterm->sx_height*cterm->sx_iv; i++) {
				for (j = 0; j < cterm->sx_width*cterm->sx_ih; j++)
					px.pixels[i*cterm->sx_width*cterm->sx_ih + j] = cterm->sx_bg;
			}
			setpixels(cterm->sx_x, cterm->sx_y, cterm->sx_x + cterm->sx_width - 1, cterm->sx_y + cterm->sx_height - 1, 0, 0, 0, 0, &px, NULL);
			free(px.pixels);
		}
		else {
			fprintf(stderr, "Error allocating memory for sixel data\n");
		}
	}

	if (cterm->extattr & CTERM_EXTATTR_SXSCROLL) {
		if (vmode != -1) {
			cterm->sx_x = cterm->sx_x / vparams[vmode].charwidth + 1;
			cterm->sx_x -= (cterm->x - 1);

			cterm->sx_y = (cterm->sx_y - 1) / vparams[vmode].charheight + 1;
			cterm->sx_y -= (cterm->y - 1);

			cterm_gotoxy(cterm, cterm->sx_x, cterm->sx_y);
		}
	}
	else {
		cterm_gotoxy(cterm, cterm->sx_start_x, cterm->sx_start_y);
	}
	cterm->cursor = cterm->sx_orig_cursor;
	ciolib_setcursortype(cterm->cursor);
	cterm->sixel = SIXEL_INACTIVE;
	if (cterm->sx_pixels)
		FREE_AND_NULL(cterm->sx_pixels->pixels);
	if (cterm->sx_mask)
		FREE_AND_NULL(cterm->sx_mask->bits);
	FREE_AND_NULL(cterm->sx_pixels);
	FREE_AND_NULL(cterm->sx_mask);
}

static int
is_hex(char ch)
{
	if ((ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F'))
		return 1;
	return 0;
}

static int
get_hexstrlen(char *str, char *end)
{
	int ret = 0;

	for (;str <= end; str++) {
		if (is_hex(*str)) {
			if (str == end)
				return -1;
			if (!is_hex(*(str+1)))
				return -1;
			ret++;
			str++;
		}
		else
			return ret;
	}
	return ret;
}

static int
nibble_val(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'a' && ch <= 'f')
		return (ch - 'a') + 10;
	if (ch >= 'A' && ch <= 'F')
		return (ch - 'A') + 10;
	return -1;
}

static void
get_hexstr(char *str, char *end, char *out)
{
	for (;str <= end; str++) {
		if (is_hex(*str)) {
			if (str == end)
				return;
			if (!is_hex(*(str+1)))
				return;
			*(out++) = nibble_val(*str) << 4 | nibble_val(*(str + 1));
			str++;
		}
		else
			return;
	}
}

#define MAX_MACRO_LEN 0xfffff
void
cterm_parse_macro_string(struct cterminal *cterm, bool finish)
{
	char *p = cterm->strbuf;
	char *end = NULL;
	int i;

	if (cterm->strbuflen == 0) {
		if (finish)
			goto all_done;
		return;
	}

	end = p+cterm->strbuflen-1;

	if (*end >= '\x08' && *end <= '\x0d') {
		cterm->strbuflen--;
		if (finish)
			goto all_done;
		return;
	}
	if (cterm->macro_encoding == MACRO_ENCODING_ASCII) {
		if ((*end >= ' ' && *end <= '\x7e') || (*end >= '\xa0' && *end <= '\xff')) {
			if (finish)
				goto all_done;
			return;
		}
	}
	if (cterm->macro_encoding == MACRO_ENCODING_HEX &&
	    (is_hex(*end) || (*end == '!') || (*end == ';'))) {
		if (finish)
			goto all_done;
		return;
	}

	cterm->macro = MACRO_INACTIVE;
	return;

all_done:
	if (cterm->macro_del == MACRO_DELETE_ALL) {
		for (i = 0; i < (sizeof(cterm->macros) / sizeof(cterm->macros[0])); i++) {
			FREE_AND_NULL(cterm->macros[i]);
			cterm->macro_lens[i] = 0;
		}
	}
	else {
		FREE_AND_NULL(cterm->macros[cterm->macro_num]);
		cterm->macro_lens[cterm->macro_num] = 0;
	}
	if (cterm->strbuflen == 0)
		return;
	if (cterm->strbuflen > MAX_MACRO_LEN)
		return;
	if (cterm->macro_encoding == MACRO_ENCODING_ASCII) {
		cterm->macros[cterm->macro_num] = malloc(cterm->strbuflen + 1);
		if (cterm->macros[cterm->macro_num]) {
			cterm->macro_lens[cterm->macro_num] = cterm->strbuflen;
			memcpy(cterm->macros[cterm->macro_num], cterm->strbuf, cterm->strbuflen);
			cterm->macros[cterm->macro_num][cterm->strbuflen] = 0;
		}
	}
	else {
		// Hex string...
		int plen;
		unsigned long ul;
		size_t mlen = 0;
		char *out;

		if (end == NULL)
			return;
		// First, calculate the required length...
		for (p = cterm->strbuf; p <= end;) {
			if (*p == '!') {
				if (p == end)
					return;
				p++;
				if (p == end)
					return;
				if (*p == ';')
					ul = 1;
				else {
					if (memchr(p, ';', cterm->strbuflen - (p - cterm->strbuf)) == NULL)
						return;
					ul = strtoul(p, &p, 10);
					if (*p != ';')
						return;
					if (ul > MAX_MACRO_LEN)
						return;
					p++;
				}
				plen = get_hexstrlen(p, end);
				if (plen < 0)
					return;
				if (plen > MAX_MACRO_LEN)
					return;
				p += plen * 2;
				mlen += ul * plen;
				if (mlen > MAX_MACRO_LEN)
					return;
				if (p <= end) {
					if (*p == ';')
						p++;
					else
						return;
				}
			}
			else {
				plen = get_hexstrlen(p, end);
				if (plen < 0)
					return;
				if (plen > MAX_MACRO_LEN)
					return;
				p += plen * 2;
				mlen += plen;
				if (mlen > MAX_MACRO_LEN)
					return;
			}
		}
		cterm->macros[cterm->macro_num] = malloc(mlen + 1);
		if (cterm->macros[cterm->macro_num] == NULL)
			return;
		cterm->macro_lens[cterm->macro_num] = mlen;
		out = cterm->macros[cterm->macro_num];
		for (p = cterm->strbuf; p <= end;) {
			if (*p == '!') {
				p++;
				if (*p == ';')
					ul = 1;
				else {
					ul = strtoul(p, &p, 10);
					if (ul > MAX_MACRO_LEN) {
						FREE_AND_NULL(cterm->macros[cterm->macro_num]);
						return;
					}
					p++;
				}
				plen = get_hexstrlen(p, end);
				if (plen < 0) {
					FREE_AND_NULL(cterm->macros[cterm->macro_num]);
					return;
				}
				if (plen > MAX_MACRO_LEN) {
					FREE_AND_NULL(cterm->macros[cterm->macro_num]);
					return;
				}
				for (i = 0; i < ul; i++) {
					get_hexstr(p, end, out);
					out += plen;
				}
				p += plen * 2;
				if (p <= end && *p == ';')
					p++;
			}
			else {
				plen = get_hexstrlen(p, end);
				if (plen < 0) {
					FREE_AND_NULL(cterm->macros[cterm->macro_num]);
					return;
				}
				if (plen > MAX_MACRO_LEN) {
					FREE_AND_NULL(cterm->macros[cterm->macro_num]);
					return;
				}
				get_hexstr(p, end, out);
				out += plen;
				p += plen * 2;
			}
		}
	}
}

void cterm_parse_sixel_intro(struct cterminal *cterm)
{
	size_t i;

	if (cterm->sixel != SIXEL_POSSIBLE)
		return;

	i = range_span_exclude(':', cterm->strbuf, '0', ';');

	if (i >= cterm->strbuflen)
		return;

	if (cterm->strbuf[i] == 'q') {
		int ratio, hgrid;
		int vmode;
		struct text_info ti;
		char *p;

		cterm->sixel = SIXEL_STARTED;
		cterm->sx_repeat = 0;
		cterm->sx_pixels_sent = 0;
		cterm->sx_first_pass = 1;
		cterm->sx_height = 0;
		cterm->sx_width = 0;
		cterm->sx_hold_update = hold_update;
		hold_update = 0;

		gettextinfo(&ti);
		vmode = find_vmode(ti.currmode);
		if (vmode == -1) {
			cterm->sixel = SIXEL_INACTIVE;
			return;
		}
		attr2palette(ti.attribute, &cterm->sx_fg, &cterm->sx_bg);
		if (cterm->extattr & CTERM_EXTATTR_SXSCROLL) {
			TERM_XY(&cterm->sx_start_x, &cterm->sx_start_y);
			cterm->sx_left = cterm->sx_x = (cterm->sx_start_x - 1) * vparams[vmode].charwidth;
			cterm->sx_y = (cterm->sx_start_y - 1) * vparams[vmode].charheight;
		}
		else {
			cterm->sx_x = cterm->sx_left = cterm->sx_y = 0;
			TERM_XY(&cterm->sx_start_x, &cterm->sx_start_y);
		}
		cterm->sx_orig_cursor = cterm->cursor;
		cterm->cursor = _NOCURSOR;
		ciolib_setcursortype(cterm->cursor);
		cterm_gotoxy(cterm, TERM_MINX, TERM_MINY);
		hold_update = 1;
		cterm->sx_trans = hgrid = 0;
		ratio = strtoul(cterm->strbuf, &p, 10);
		if (*p == ';') {
			p++;
			cterm->sx_trans = strtoul(p, &p, 10);
		}
		if (*p == ';') {
			p++;
			hgrid = strtoul(p, &p, 10);
		}
		switch (ratio) {
			default:
			case 0:
			case 1:
				cterm->sx_iv = 2;
				cterm->sx_ih = 1;
				break;
			case 2:
				cterm->sx_iv = 5;
				cterm->sx_ih = 1;
				break;
			case 3:
			case 4:
				cterm->sx_iv = 3;
				cterm->sx_ih = 1;
				break;
			case 5:
			case 6:
				cterm->sx_iv = 2;
				cterm->sx_ih = 1;
				break;
			case 7:
			case 8:
			case 9:
				cterm->sx_iv = 1;
				cterm->sx_ih = 1;
				break;
		}
		cterm->strbuflen = 0;
	}
	else if (cterm->strbuf[i] != 'q')
		cterm->sixel = SIXEL_INACTIVE;
}

void cterm_parse_macro_intro(struct cterminal *cterm)
{
	size_t i;

	if (cterm->macro != MACRO_POSSIBLE)
		return;

	i = range_span_exclude(':', cterm->strbuf, '0', ';');

	if (i >= cterm->strbuflen)
		return;

	if (cterm->strbuf[i] != '!') {
		cterm->macro = MACRO_INACTIVE;
		return;
	}
	i++;
	if (i >= cterm->strbuflen)
		return;

	if (cterm->strbuf[i] == 'z') {
		char *p;
		unsigned long res;

		// Parse parameters...
		cterm->macro_num = -1;
		cterm->macro_del = MACRO_DELETE_OLD;
		cterm->macro_encoding = MACRO_ENCODING_ASCII;
		res = strtoul(cterm->strbuf, &p, 10);
		if (res != ULONG_MAX)
			cterm->macro_num = res;
		if (*p == ';') {
			p++;
			res = strtoul(p, &p, 10);
			if (res != ULONG_MAX)
				cterm->macro_del = res;
			else
				cterm->macro_del = -1;
		}
		if (*p == ';') {
			p++;
			res = strtoul(p, &p, 10);
			if (res != ULONG_MAX)
				cterm->macro_encoding = res;
			else
				cterm->macro_encoding = -1;
		}
		if (cterm->macro_num < 0 || cterm->macro_num > 63)
			cterm->macro = MACRO_INACTIVE;
		else if (cterm->macro_del < 0 || cterm->macro_del > 1)
			cterm->macro = MACRO_INACTIVE;
		else if (cterm->macro_encoding < 0 || cterm->macro_encoding > 1)
			cterm->macro = MACRO_INACTIVE;
		else {
			cterm->macro = MACRO_STARTED;
			cterm->strbuflen = 0;
		}
	}
	else if (cterm->strbuf[i] != 'z')
		cterm->macro = MACRO_INACTIVE;
}

/* ------------------------------------------------------------------------
 * cterm_dec_dcs_byte — called by the generic command-string accumulator
 * in cterm.c after each byte is appended to cterm->strbuf.  Drives the
 * two incremental-parse state machines: sixel (SIXEL_POSSIBLE →
 * SIXEL_STARTED after the 'q' final byte, then streaming image data)
 * and DECDMAC (MACRO_POSSIBLE → MACRO_STARTED after "!z", then
 * streaming ASCII/hex body).  Buffered DCS strings (DECRQSS, CTerm:Font:)
 * need no per-byte work and fall through.
 * ------------------------------------------------------------------------ */
void
cterm_dec_dcs_byte(struct cterminal *cterm)
{
	switch (cterm->sixel) {
		case SIXEL_STARTED:
			cterm_parse_sixel_string(cterm, false);
			break;
		case SIXEL_POSSIBLE:
			cterm_parse_sixel_intro(cterm);
			break;
	}
	switch (cterm->macro) {
		case MACRO_STARTED:
			cterm_parse_macro_string(cterm, false);
			break;
		case MACRO_POSSIBLE:
			cterm_parse_macro_intro(cterm);
			break;
	}
}

/* ------------------------------------------------------------------------
 * cterm_dec_dcs_begin — DEC-specific setup at DCS start
 *
 * The DCS framing is ECMA-48, but everything we actually do inside it
 * is a DEC (or DEC-family) extension: sixel, DECDMAC, DECRQSS, plus
 * the SyncTERM CTerm:Font extension.  The generic cterm_handle_dcs_start
 * in cterm.c calls us so we can prime the sixel / macro detectors in
 * SIXEL_POSSIBLE / MACRO_POSSIBLE state; if the incoming DCS isn't one
 * of those they fall through to SIXEL_INACTIVE / MACRO_INACTIVE as
 * bytes arrive and the CTerm:Font / DECRQSS branches pick it up on ST.
 * ------------------------------------------------------------------------ */
void
cterm_dec_dcs_begin(struct cterminal *cterm)
{
	cterm->sixel = SIXEL_POSSIBLE;
	cterm->macro = MACRO_POSSIBLE;
}

/* ------------------------------------------------------------------------
 * cterm_dec_dcs_finish — DCS-content dispatcher invoked on ST.
 *
 * Called from the generic cterm_handle_st() in cterm.c when the active
 * string type is DCS.  Decides between sixel finalisation, DECDMAC
 * finalisation, DECRQSS (VT-420 status-string query), and the CTerm:Font
 * SyncTERM extension (delegated to cterm_cterm_handle_font_dcs).
 * ------------------------------------------------------------------------ */
void
cterm_dec_dcs_finish(struct cterminal *cterm)
{
	char	tmp[3072];

	if (cterm->sixel == SIXEL_STARTED) {
		cterm_parse_sixel_string(cterm, true);
	}
	else if (cterm->macro == MACRO_STARTED) {
		cterm_parse_macro_string(cterm, true);
	}
	else if (strncmp(cterm->strbuf, "CTerm:Font:", 11) == 0) {
		cterm_cterm_handle_font_dcs(cterm);
	}
	else if (strncmp(cterm->strbuf, "$q", 2) == 0) {
		/* DECRQSS — VT-420 Request Selection or Setting */
		switch (cterm->strbuf[2]) {
			case 'm':
				if (cterm->strbuf[3] == 0) {
					strcpy(tmp, "\x1bP1$r0");
					if (cterm->attr & 8)
						strcat(tmp, ";1");
					if (cterm->attr & 128)
						strcat(tmp, ";5");
					if (cterm->negative)
						strcat(tmp, ";7");
					if (cterm->fg_tc_str == NULL) {
						switch ((cterm->attr >> (cterm->negative ? 4 : 0)) & 7) {
							case 0: strcat(tmp, ";30"); break;
							case 1: strcat(tmp, ";34"); break;
							case 2: strcat(tmp, ";32"); break;
							case 3: strcat(tmp, ";36"); break;
							case 4: strcat(tmp, ";31"); break;
							case 5: strcat(tmp, ";35"); break;
							case 6: strcat(tmp, ";33"); break;
						}
					}
					if (cterm->bg_tc_str == NULL) {
						switch ((cterm->attr >> (cterm->negative ? 0 : 4)) & 7) {
							case 1: strcat(tmp, ";44"); break;
							case 2: strcat(tmp, ";42"); break;
							case 3: strcat(tmp, ";46"); break;
							case 4: strcat(tmp, ";41"); break;
							case 5: strcat(tmp, ";45"); break;
							case 6: strcat(tmp, ";43"); break;
							case 7: strcat(tmp, ";47"); break;
						}
					}
					if (cterm->fg_tc_str) {
						strcat(tmp, ";");
						strcat(tmp, cterm->fg_tc_str);
					}
					if (cterm->bg_tc_str) {
						strcat(tmp, ";");
						strcat(tmp, cterm->bg_tc_str);
					}
					strcat(tmp, "m\x1b\\");
					cterm_respond(cterm, tmp, strlen(tmp));
				}
				else {
					cterm_respond(cterm, "\x1bP0$r\x1b\\", 7);
				}
				break;
			case 'r':
				if (cterm->strbuf[3] == 0) {
					sprintf(tmp, "\x1bP1$r%d;%dr\x1b\\",
					    cterm->top_margin, cterm->bottom_margin);
					cterm_respond(cterm, tmp, strlen(tmp));
				}
				else {
					cterm_respond(cterm, "\x1bP0$r\x1b\\", 7);
				}
				break;
			case 's':
				if (cterm->strbuf[3] == 0) {
					sprintf(tmp, "\x1bP1$r%d;%ds\x1b\\",
					    cterm->left_margin, cterm->right_margin);
					cterm_respond(cterm, tmp, strlen(tmp));
				}
				else {
					cterm_respond(cterm, "\x1bP0$r\x1b\\", 7);
				}
				break;
			case 't':
				if (cterm->strbuf[3] == 0) {
					sprintf(tmp, "\x1bP1$r%dt\x1b\\", cterm->height);
					cterm_respond(cterm, tmp, strlen(tmp));
				}
				else {
					cterm_respond(cterm, "\x1bP0$r\x1b\\", 7);
				}
				break;
			case '$':
				if (cterm->strbuf[3] == '|' && cterm->strbuf[4] == 0) {
					sprintf(tmp, "\x1bP1$r%d$|\x1b\\", cterm->width);
					cterm_respond(cterm, tmp, strlen(tmp));
				}
				else {
					cterm_respond(cterm, "\x1bP0$r\x1b\\", 7);
				}
				break;
			case '*':
				if (cterm->strbuf[3] == '|' && cterm->strbuf[4] == 0) {
					sprintf(tmp, "\x1bP1$r%d$|\x1b\\", cterm->height);
					cterm_respond(cterm, tmp, strlen(tmp));
				}
				else if (cterm->strbuf[3] == 'x' && cterm->strbuf[4] == 0) {
					sprintf(tmp, "\x1bP1$r%d*x\x1b\\", cterm->decsace);
					cterm_respond(cterm, tmp, strlen(tmp));
				}
				else if (cterm->strbuf[3] == 'r' && cterm->strbuf[4] == 0) {
					int ps2 = 0;
					switch (cterm->decscs_speed) {
						case 300:    ps2 = 1; break;
						case 600:    ps2 = 2; break;
						case 1200:   ps2 = 3; break;
						case 2400:   ps2 = 4; break;
						case 4800:   ps2 = 5; break;
						case 9600:   ps2 = 6; break;
						case 19200:  ps2 = 7; break;
						case 38400:  ps2 = 8; break;
						case 57600:  ps2 = 9; break;
						case 76800:  ps2 = 10; break;
						case 115200: ps2 = 11; break;
					}
					sprintf(tmp, "\x1bP1$r;%d*r\x1b\\", ps2);
					cterm_respond(cterm, tmp, strlen(tmp));
				}
				else {
					cterm_respond(cterm, "\x1bP0$r\x1b\\", 7);
				}
				break;
			case ' ':
				if (cterm->strbuf[3] == 'q' && cterm->strbuf[4] == 0) {
					int style;
					int cs, ce, cr, cb, cv;
					if (cio_api.options & CONIO_OPT_CUSTOM_CURSOR) {
						getcustomcursor(&cs, &ce, &cr, &cb, &cv);
						if (cterm->cursor == _SOLIDCURSOR)
							style = cb ? 1 : 2;
						else
							style = cb ? 3 : 4;
					}
					else {
						style = (cterm->cursor == _SOLIDCURSOR) ? 2 : 4;
					}
					sprintf(tmp, "\x1bP1$r%d q\x1b\\", style);
					cterm_respond(cterm, tmp, strlen(tmp));
				}
				else {
					cterm_respond(cterm, "\x1bP0$r\x1b\\", 7);
				}
				break;
			default:
				cterm_respond(cterm, "\x1bP0$r\x1b\\", 7);
				break;
		}
	}
	cterm->sixel = SIXEL_INACTIVE;
}

/* ------------------------------------------------------------------------
 * cterm_dec_apply_sgr — DEC's slot in the SGR cascade.
 *
 * DEC has no currently-implemented SGR parameters of its own, so every
 * param falls through to the ECMA-48 base.  The placeholder keeps the
 * chain explicit and gives a home for future DEC-specific SGR values.
 * ------------------------------------------------------------------------ */
void
cterm_dec_apply_sgr(struct cterminal *cterm, int *pi)
{
	cterm_ecma48_apply_sgr(cterm, pi);
}

