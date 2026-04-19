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
 * cterm_ecma48.c — Standard ECMA-48 control-sequence handlers.
 *
 * Holds the standardised C1 / CSI handlers defined by ECMA-48 (equivalent
 * to ISO/IEC 6429):
 *   - Cursor motion: CUU, CUD, CUF, CUB, CNL, CPL, CHA, CUP, CHT, CBT,
 *     HPA, HPR, HPB, VPA, VPR, VPB, HVP
 *   - Editing:       ED, EL, IL, DL, DCH, ECH, ICH, SU, SD, SL, SR
 *   - Attributes:    SGR (including extended colour + sub-parameters)
 *   - Modes:         SM, RM
 *   - Reports:       DSR, DA (primary), REP, DAQ, CTC, TBC
 *   - Tab/line ctl:  NEL, SSA, ESA, HTS, VTS, RI, STS
 *   - String intro:  DCS, SOS, OSC, PM, APC and the ST terminator
 *   - Reset:         RIS (ESC c — reset to initial state)
 *
 * Each handler has the signature
 *   void cterm_handle_<name>(struct cterminal *, int *speed);
 * and is reached via bsearch() on the sorted cterm_ansi_dispatch[] table built
 * from the SEQ_* keys declared in cterm_internal.h.
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
#include "cterm_ecma48.h"
#include "cterm_cterm.h"	/* ANSI-music / SyncTERM DA hooks called from here */
#include "cterm_dec.h"	/* DECSC saved state restored by RIS */
#include "vidmodes.h"
#include "base64.h"
#include <crc16.h>

/* Forward declarations for the incremental CSI parser + param-string
 * splitter — defined further down in this file and used by
 * cterm_accumulate_ecma_seq. */
static enum sequence_state seq_feed(struct cterminal *cterm, unsigned char ch);
static void cterm_build_param_strs(struct cterminal *cterm);

void
cterm_handle_cuu(struct cterminal *cterm, int *speed)	/* CSI A */
{
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, 0, -(int)cterm->seq_param_int[0], 0);
}

void
cterm_handle_cud(struct cterminal *cterm, int *speed)	/* CSI B */
{
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, 0, (int)cterm->seq_param_int[0], 0);
}

void
cterm_handle_cuf(struct cterminal *cterm, int *speed)	/* CSI C */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, (int)cterm->seq_param_int[0], 0, 0);
}

void
cterm_handle_cub(struct cterminal *cterm, int *speed)	/* CSI D */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, -(int)cterm->seq_param_int[0], 0, 0);
}

void
cterm_handle_cnl(struct cterminal *cterm, int *speed)	/* CSI E */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, INT_MIN, (int)cterm->seq_param_int[0], 0);
}

void
cterm_handle_cpl(struct cterminal *cterm, int *speed)	/* CSI F */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, INT_MIN, -(int)cterm->seq_param_int[0], 0);
}

void
cterm_handle_cha(struct cterminal *cterm, int *speed)
{
	int col, row;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	CURR_XY(NULL, &row);
	col = (int)cterm->seq_param_int[0];
	if (col >= CURR_MINX && col <= CURR_MAXX)
		gotoxy(col, row);
}

void
cterm_handle_cup(struct cterminal *cterm, int *speed)
{
	int row, col;
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	cterm_seq_default(cterm, 1, 1);
	row = (int)cterm->seq_param_int[0];
	col = (int)cterm->seq_param_int[1];
	if (row < CURR_MINY)
		row = CURR_MINY;
	if (col < CURR_MINX)
		col = CURR_MINX;
	if (row > CURR_MAXY)
		row = CURR_MAXY;
	if (col > CURR_MAXX)
		col = CURR_MAXX;
	gotoxy(col, row);
}

void
cterm_handle_cht(struct cterminal *cterm, int *speed)	/* CSI I */
{
	int i, j;
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	i = (int)cterm->seq_param_int[0];
	if (i > cterm->width * cterm->height)
		i = cterm->width * cterm->height;
	for (j = 0; j < i; j++)
		cterm_do_tab(cterm);
}

void
cterm_handle_cvt(struct cterminal *cterm, int *speed)	/* CSI Y */
{
	int i, j;
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	i = (int)cterm->seq_param_int[0];
	if (i > cterm->height)
		i = cterm->height;
	for (j = 0; j < i; j++)
		cterm_do_vtab(cterm);
}

void
cterm_handle_cbt(struct cterminal *cterm, int *speed)	/* CSI Z */
{
	int i, j;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	i = (int)cterm->seq_param_int[0];
	if (i > cterm->width * cterm->height)
		i = cterm->width * cterm->height;
	for (j = 0; j < i; j++)
		cterm_do_backtab(cterm);
}

void
cterm_handle_hpr(struct cterminal *cterm, int *speed)	/* CSI a */
{
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, (int)cterm->seq_param_int[0], 0, 0);
}

void
cterm_handle_vpa(struct cterminal *cterm, int *speed)	/* CSI d */
{
	int row, col;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	CURR_XY(&col, NULL);
	row = (int)cterm->seq_param_int[0];
	if (row < CURR_MINY)
		row = CURR_MINY;
	if (row > CURR_MAXY)
		row = CURR_MAXY;
	gotoxy(col, row);
}

void
cterm_handle_vpr(struct cterminal *cterm, int *speed)	/* CSI e */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, 0, (int)cterm->seq_param_int[0], 0);
}

void
cterm_handle_hpb(struct cterminal *cterm, int *speed)	/* CSI j */
{
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, -(int)cterm->seq_param_int[0], 0, 0);
}

void
cterm_handle_vpb(struct cterminal *cterm, int *speed)	/* CSI k */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		return;
	cterm_adjust_currpos(cterm, 0, -(int)cterm->seq_param_int[0], 0);
}

void
cterm_handle_ich(struct cterminal *cterm, int *speed)	/* CSI @ */
{
	int i, j, l, col, row, max_col;
	(void)speed;
	cterm_clear_lcf(cterm);
	TERM_XY(&i, &j);
	if (i < TERM_MINX || i > TERM_MAXX || j < TERM_MINY || j > TERM_MAXY)
		return;
	col = i;
	row = j;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col, &row);
	max_col = TERM_MAXX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, NULL);
	cterm_seq_default(cterm, 0, 1);
	if (cterm->seq_param_int[0] < 1)
		cterm->seq_param_int[0] = 1;
	if (cterm->seq_param_int[0] > (uint64_t)(TERM_MAXX - i + 1))
		cterm->seq_param_int[0] = TERM_MAXX - i + 1;
	movetext(col, row, max_col - (int)cterm->seq_param_int[0], row,
	    col + (int)cterm->seq_param_int[0], row);
	for (l = 0; l < (int)cterm->seq_param_int[0]; l++)
		putch(' ');
	cterm_gotoxy(cterm, i, j);
}

void
cterm_handle_ed(struct cterminal *cterm, int *speed)	/* CSI J */
{
	int i, col, row;
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 0);
	switch (cterm->seq_param_int[0]) {
		case 0:
			clreol();
			CURR_XY(&col, &row);
			for (i = row + 1; i <= TERM_MAXY; i++) {
				cterm_gotoxy(cterm, TERM_MINY, i);
				clreol();
			}
			gotoxy(col, row);
			break;
		case 1:
			cterm_clear2bol(cterm);
			CURR_XY(&col, &row);
			for (i = row - 1; i >= TERM_MINY; i--) {
				cterm_gotoxy(cterm, TERM_MINX, i);
				clreol();
			}
			gotoxy(col, row);
			break;
		case 2:
			cterm_clearscreen(cterm, (char)cterm->attr);
			gotoxy(CURR_MINX, CURR_MINY);
			break;
	}
}

void
cterm_handle_el(struct cterminal *cterm, int *speed)	/* CSI K */
{
	int col, row;
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 0);
	switch (cterm->seq_param_int[0]) {
		case 0:
			clreol();
			break;
		case 1:
			cterm_clear2bol(cterm);
			break;
		case 2:
			CURR_XY(&col, &row);
			gotoxy(CURR_MINX, row);
			clreol();
			gotoxy(col, row);
			break;
	}
}

void
cterm_handle_il(struct cterminal *cterm, int *speed)	/* CSI L */
{
	int i, j, col, row, col2, row2, max_col, max_row;
	(void)speed;
	TERM_XY(&col, &row);
	if (row < TERM_MINY || row > TERM_MAXY || col < TERM_MINX || col > TERM_MAXX)
		return;
	cterm_seq_default(cterm, 0, 1);
	i = (int)cterm->seq_param_int[0];
	if (i < 1)
		return;
	if (i > TERM_MAXY - row)
		i = TERM_MAXY - row;
	col2 = TERM_MINX;
	row2 = row;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col2, &row2);
	max_col = TERM_MAXX;
	max_row = TERM_MAXY;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, &max_row);
	if (i)
		movetext(col2, row2, max_col, max_row - i, col2, row2 + i);
	for (j = 0; j < i; j++) {
		cterm_gotoxy(cterm, TERM_MINX, row + j);
		cterm_clreol(cterm);
	}
	cterm_gotoxy(cterm, col, row);
}

void
cterm_handle_dl(struct cterminal *cterm, int *speed)	/* CSI M */
{
	int i, col, row;
	(void)speed;
	TERM_XY(&col, &row);
	if (col < TERM_MINX || col > TERM_MAXX
	    || row < TERM_MINY || row > TERM_MAXY)
		return;
	cterm_seq_default(cterm, 0, 1);
	i = (int)cterm->seq_param_int[0];
	cterm_dellines(cterm, i);
}

void
cterm_handle_dch(struct cterminal *cterm, int *speed)	/* CSI P */
{
	int i, col, row, col2, row2, max_col;
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	TERM_XY(&col, &row);
	if (col < TERM_MINX || col > TERM_MAXX
	    || row < TERM_MINY || row > TERM_MAXY)
		return;
	i = (int)cterm->seq_param_int[0];
	if (i < 1)
		return;
	if (i > TERM_MAXX - col + 1)
		i = TERM_MAXX - col + 1;
	max_col = TERM_MAXX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, NULL);
	col2 = col;
	row2 = row;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col2, &row2);
	movetext(col2 + i, row2, max_col, row2, col2, row2);
	cterm_gotoxy(cterm, TERM_MAXX - i, row);
	cterm_clreol(cterm);
	cterm_gotoxy(cterm, col, row);
}

void
cterm_handle_su(struct cterminal *cterm, int *speed)	/* CSI S */
{
	int j, row;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	row = (int)cterm->seq_param_int[0];
	if (row > TERM_MAXY)
		row = TERM_MAXY;
	for (j = 0; j < row; j++)
		cterm_scrollup(cterm);
}

void
cterm_handle_sd(struct cterminal *cterm, int *speed)	/* CSI T */
{
	int j, row;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	row = (int)cterm->seq_param_int[0];
	if (row > TERM_MAXY)
		row = TERM_MAXY;
	for (j = 0; j < row; j++)
		cterm_scrolldown(cterm);
}

void
cterm_handle_ech(struct cterminal *cterm, int *speed)	/* CSI X */
{
	int i, k, col, row, col2, row2, max_col;
	struct vmem_cell *vc;
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_seq_default(cterm, 0, 1);
	i = (int)cterm->seq_param_int[0];
	if (i < 1)
		return;
	CURR_XY(&col, &row);
	if (i > CURR_MAXX - col)
		i = CURR_MAXX - col;
	vc = malloc(i * sizeof(*vc));
	if (vc == NULL)
		return;
	for (k = 0; k < i; k++) {
		vc[k].ch = ' ';
		vc[k].legacy_attr = cterm->attr;
		vc[k].fg = cterm->fg_color;
		vc[k].bg = cterm->bg_color;
		vc[k].font = ciolib_attrfont(cterm->attr);
		vc[k].hyperlink_id = 0;
	}
	col2 = col;
	row2 = row;
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &col2, &row2);
	max_col = CURR_MAXX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &max_col, NULL);
	(void)max_col;
	vmem_puttext(col2, row2, col2 + i - 1, row2, vc);
	free(vc);
}

void
cterm_handle_rep(struct cterminal *cterm, int *speed)	/* CSI b */
{
	int i;
	char *p;
	(void)speed;
	if (cterm->lastch == 0)
		return;
	cterm_seq_default(cterm, 0, 1);
	i = (int)cterm->seq_param_int[0];
	if (i > 0) {
		p = malloc(i + 1);
		if (p) {
			memset(p, cterm->lastch, i);
			p[i] = 0;
			cterm_ctputs(cterm, p);
			free(p);
		}
	}
}

void
cterm_handle_da(struct cterminal *cterm, int *speed)	/* CSI c */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 0);
	if (!cterm->seq_param_int[0])
		cterm_respond(cterm, cterm->DA, strlen(cterm->DA));
}

void
cterm_handle_tbc(struct cterminal *cterm, int *speed)	/* CSI g */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 0);
	switch (cterm->seq_param_int[0]) {
		case 0:
			cterm_delete_tabstop(cterm, wherex());
			break;
		case 1:
			cterm_delete_vtabstop(cterm, wherey());
			break;
		case 3:
			cterm->tab_count = 0;
			break;
		case 4:
			cterm->vtab_count = 0;
			break;
		case 5:
			cterm->tab_count = 0;
			cterm->vtab_count = 0;
			break;
	}
}

void
cterm_handle_sm(struct cterminal *cterm, int *speed)	/* CSI h */
{
	int i;
	(void)speed;
	for (i = 0; i < cterm->seq_param_count; i++) {
		cterm_seq_default(cterm, i, 0);
		switch (cterm->seq_param_int[i]) {
			case 14:	/* FETM = EXCLUDE */
				cterm->fetm = 1;
				break;
			case 16:	/* TTM = ALL */
				cterm->ttm = 1;
				break;
		}
	}
}

void
cterm_handle_rm(struct cterminal *cterm, int *speed)	/* CSI l */
{
	int i;
	(void)speed;
	for (i = 0; i < cterm->seq_param_count; i++) {
		cterm_seq_default(cterm, i, 0);
		switch (cterm->seq_param_int[i]) {
			case 14:	/* FETM = INSERT */
				cterm->fetm = 0;
				break;
			case 16:	/* TTM = CURSOR */
				cterm->ttm = 0;
				break;
		}
	}
}

void
cterm_handle_dsr(struct cterminal *cterm, int *speed)	/* CSI n */
{
	int col, row;
	(void)speed;
	if (cterm_seq_param_consumed(cterm, 0))
		return;
	cterm_seq_default(cterm, 0, 0);
	switch (cterm->seq_param_int[0]) {
		case 5:
			cterm_respond(cterm, "\x1b[0n", 4);
			break;
		case 6:
			CURR_XY(&col, &row);
			cterm_respond_printf(cterm, "\x1b[%d;%dR", row, col);
			break;
		default:
			return;
	}
	cterm_seq_consume_param(cterm, 0);
}

void
cterm_handle_sl(struct cterminal *cterm, int *speed)	/* CSI SP @ */
{
	int i, j, k, row, col, max_row, max_col;
	struct vmem_cell *vc;
	(void)speed;
	row = TERM_MINY;
	col = TERM_MINX;
	max_row = TERM_MAXY;
	max_col = TERM_MAXX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col, &row);
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, &max_row);
	cterm_seq_default(cterm, 0, 1);
	i = (int)cterm->seq_param_int[0];
	if (i < 1)
		return;
	if (i > TERM_MAXX)
		i = TERM_MAXX;
	movetext(col + i, row, max_col, max_row, col, row);
	j = i * TERM_MAXY;
	vc = malloc(j * sizeof(*vc));
	if (vc == NULL)
		return;
	for (k = 0; k < j; k++) {
		vc[k].ch = ' ';
		vc[k].legacy_attr = cterm->attr;
		vc[k].fg = cterm->fg_color;
		vc[k].bg = cterm->bg_color;
		vc[k].font = ciolib_attrfont(cterm->attr);
		vc[k].hyperlink_id = 0;
	}
	vmem_puttext(max_col - i + 1, row, max_col, max_row, vc);
	free(vc);
}

void
cterm_handle_sr(struct cterminal *cterm, int *speed)	/* CSI SP A */
{
	int i, j, k, row, col, max_row, max_col;
	struct vmem_cell *vc;
	(void)speed;
	row = TERM_MINY;
	col = TERM_MINX;
	max_row = TERM_MAXY;
	max_col = TERM_MAXX;
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &col, &row);
	cterm_coord_conv_xy(cterm, CTERM_COORD_TERM, CTERM_COORD_SCREEN, &max_col, &max_row);
	cterm_seq_default(cterm, 0, 1);
	i = (int)cterm->seq_param_int[0];
	if (i < 1)
		return;
	if (i > cterm->width)
		i = cterm->width;
	movetext(col, row, max_col - i, max_row, col + i, row);
	j = i * TERM_MAXY;
	vc = malloc(j * sizeof(*vc));
	if (vc == NULL)
		return;
	for (k = 0; k < j; k++) {
		vc[k].ch = ' ';
		vc[k].legacy_attr = cterm->attr;
		vc[k].fg = cterm->fg_color;
		vc[k].bg = cterm->bg_color;
		vc[k].font = ciolib_attrfont(cterm->attr);
		vc[k].hyperlink_id = 0;
	}
	vmem_puttext(col, row, col + i - 1, max_row, vc);
	free(vc);
}

void
cterm_handle_sgr(struct cterminal *cterm, int *speed)	/* CSI m — Select Graphic Rendition */
{
	int i;
	(void)speed;
	/* Inject the SGR 0 default ONLY when the sequence arrived empty
	 * and nothing has been consumed by an upstream cascade layer.
	 * Otherwise a cascade consumer that drained the list would cause
	 * the ECMA-48 reset default to clobber the attributes it set. */
	if (cterm_seq_defaults_apply(cterm))
		cterm_seq_default(cterm, 0, 0);
	for (i = 0; i < cterm->seq_param_count; i++) {
		if (cterm_seq_param_consumed(cterm, i))
			continue;
		cterm_apply_sgr(cterm, &i);
	}
	textattr(cterm->attr);
	setcolour(cterm->fg_color, cterm->bg_color);
}

void
cterm_handle_nel(struct cterminal *cterm, int *speed)	/* ESC E */
{
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_adjust_currpos(cterm, INT_MIN, 1, 1);
}

void
cterm_handle_ssa(struct cterminal *cterm, int *speed)	/* ESC F */
{
	int sx, sy;
	(void)speed;
	CURR_XY(&sx, &sy);
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
	cterm->ssa_col = sx;
	cterm->ssa_row = sy;
}

void
cterm_handle_esa(struct cterminal *cterm, int *speed)	/* ESC G */
{
	int sx, sy;
	(void)speed;
	CURR_XY(&sx, &sy);
	cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &sx, &sy);
	cterm->esa_col = sx;
	cterm->esa_row = sy;
}

void
cterm_handle_hts(struct cterminal *cterm, int *speed)	/* ESC H */
{
	(void)speed;
	cterm_insert_tabstop(cterm, wherex());
}

void
cterm_handle_vts(struct cterminal *cterm, int *speed)	/* ESC J */
{
	(void)speed;
	cterm_insert_vtabstop(cterm, wherey());
}

void
cterm_handle_ri(struct cterminal *cterm, int *speed)	/* ESC M */
{
	(void)speed;
	cterm_clear_lcf(cterm);
	cterm_adjust_currpos(cterm, 0, -1, 1);
}

void
cterm_handle_sts(struct cterminal *cterm, int *speed)	/* ESC S */
{
	(void)speed;
	/* Flush any pending retbuf content before transmitting. */
	if (cterm->response_cb && cterm->response_buf) {
		size_t pending = strlen(cterm->response_buf);
		if (pending > 0) {
			cterm->response_cb(cterm->response_buf, pending,
			    cterm->response_cbdata);
			cterm->response_buf[0] = '\0';
		}
	}
	cterm_transmit_selected(cterm);
}

void
cterm_handle_ris(struct cterminal *cterm, int *speed)	/* ESC c — Reset to Initial State */
{
	(void)speed;
	clrscr();
	cterm_reset(cterm);
	cterm_gotoxy(cterm, CURR_MINX, CURR_MINY);
}

/* ------------------------------------------------------------------------
 * Command-string framing (DCS / SOS / OSC / PM / APC) + ST dispatch.
 *
 * ECMA-48 defines the framing for the five command-string introducers
 * and the ST terminator, but leaves the string CONTENTS to individual
 * standards.  The start handlers here only set cterm->string and
 * allocate the accumulation buffer; DCS additionally calls a DEC-side
 * hook because sixel / DECDMAC state has to be armed at that point.
 *
 * On ST, cterm_handle_st() hands the buffered payload to the
 * appropriate per-standard finisher:
 *    DCS -> cterm_dec_dcs_finish     (sixel, DECDMAC, DECRQSS,
 *                                     CTerm:Font via cterm_cterm)
 *    OSC -> cterm_cterm_osc_finish   (XTerm palette, OSC 8 hyperlinks)
 *    APC -> user-registered cterm->apc_handler
 *    SOS -> nothing (ECMA-48 defines no registered semantics)
 *    PM  -> nothing
 * ------------------------------------------------------------------------ */
static void
cterm_string_alloc(struct cterminal *cterm, int type, cterm_accumulator_fn acc)
{
	cterm->string = type;
	cterm->accumulator = acc;
	FREE_AND_NULL(cterm->strbuf);
	cterm->strbuf = malloc(1024);
	cterm->strbufsize = 1024;
	cterm->strbuflen = 0;
}

void
cterm_handle_dcs_start(struct cterminal *cterm, int *speed)	/* ESC P */
{
	(void)speed;
	cterm_string_alloc(cterm, CTERM_STRING_DCS, cterm_accumulate_command_string);
	cterm_dec_dcs_begin(cterm);
}

void
cterm_handle_sos_start(struct cterminal *cterm, int *speed)	/* ESC X */
{
	(void)speed;
	cterm_string_alloc(cterm, CTERM_STRING_SOS, cterm_accumulate_sos);
}

void
cterm_handle_osc_start(struct cterminal *cterm, int *speed)	/* ESC ] */
{
	(void)speed;
	cterm_string_alloc(cterm, CTERM_STRING_OSC, cterm_accumulate_command_string);
}

void
cterm_handle_pm_start(struct cterminal *cterm, int *speed)	/* ESC ^ */
{
	(void)speed;
	cterm_string_alloc(cterm, CTERM_STRING_PM, cterm_accumulate_command_string);
}

void
cterm_handle_apc_start(struct cterminal *cterm, int *speed)	/* ESC _ */
{
	(void)speed;
	cterm_string_alloc(cterm, CTERM_STRING_APC, cterm_accumulate_command_string);
}

void
cterm_handle_st(struct cterminal *cterm, int *speed)	/* ESC \ (String Terminator) */
{
	char *p;
	(void)speed;

	if (cterm->strbuf == NULL) {
		cterm->strbufsize = cterm->strbuflen = 0;
		cterm->string = 0;
		cterm->saved_accumulator = NULL;
		return;
	}
	if (cterm->strbufsize == cterm->strbuflen - 1) {
		p = realloc(cterm->strbuf, cterm->strbufsize + 1);
		if (p == NULL) {
			cterm->string = 0;
		}
		else {
			cterm->strbuf = p;
			cterm->strbufsize++;
		}
	}
	cterm->strbuf[cterm->strbuflen] = 0;
	switch (cterm->string) {
		case CTERM_STRING_APC:
			if (cterm->apc_handler)
				cterm->apc_handler(cterm->strbuf, cterm->strbuflen,
				    cterm->response_buf, cterm->response_buf_size,
				    cterm->apc_handler_data);
			break;
		case CTERM_STRING_DCS:
			cterm_dec_dcs_finish(cterm);
			break;
		case CTERM_STRING_OSC:
			cterm_cterm_osc_finish(cterm);
			break;
		/* SOS, PM: no registered semantics — buffer is dropped below. */
	}
	FREE_AND_NULL(cterm->strbuf);
	cterm->strbufsize = cterm->strbuflen = 0;
	cterm->string = 0;
	/* The string is done; the caller (cterm_accumulate_ecma_seq) will
	 * fall back to saved_accumulator once we return — clearing it here
	 * tells it to clear instead of restoring. */
	cterm->saved_accumulator = NULL;
}

/* ------------------------------------------------------------------------
 * cterm_accumulate_command_string — DCS / APC / OSC / PM accumulator
 *
 * Collects string bytes into cterm->strbuf, feeds them through the
 * sixel / macro introspection hooks, and hands control to seq_feed()
 * when an ESC arrives (possibly starting the ST that terminates the
 * string).  Returns true for consumed bytes; returns false when the
 * byte is an out-of-range control that should be re-dispatched as
 * normal text (the old `j--` semantics).
 * ------------------------------------------------------------------------ */
bool
cterm_accumulate_command_string(struct cterminal *cterm, unsigned char byte,
    int *speed)
{
	(void)speed;

	if (byte == 0)
		return true;	/* media-fill; ignore */

	if (byte < 8 || (byte > 0x0d && byte < 0x20) || byte > 0x7e) {
		if (byte == 27) {
			/* ESC: may begin ST (ESC \) that closes the string.
			 * Stash the string accumulator so a broken nested
			 * sequence falls back to it; a clean ST will have
			 * cterm_handle_st clear saved_accumulator. */
			cterm_uctputs(cterm, cterm->print_buf);
			cterm->print_buf[0] = 0;
			cterm->print_pos = cterm->print_buf;
			cterm->saved_accumulator = cterm->accumulator;
			cterm->accumulator = cterm_accumulate_ecma_seq;
			cterm->sequence = 1;
			cterm->seq_phase = SEQ_PHASE_C1;
			cterm->seq_key = SEQ_ESC;
			cterm->seq_param_len = 0;
			cterm->seq_params[0] = 0;
			cterm->seq_param_count = 0;
			cterm->seq_overflow = false;
			return true;
		}
		/* Malformed byte mid-string: abandon the string, let the caller
		 * re-dispatch this byte through the normal path. */
		cterm->string = 0;
		cterm->accumulator = NULL;
		FREE_AND_NULL(cterm->strbuf);
		cterm->strbuflen = cterm->strbufsize = 0;
		cterm->sixel = SIXEL_INACTIVE;
		return false;
	}

	if (cterm->strbuf == NULL)
		return true;

	cterm->strbuf[cterm->strbuflen++] = byte;
	if (cterm->strbuflen == cterm->strbufsize) {
		char *p;
		cterm->strbufsize *= 2;
		if (cterm->strbufsize > 1024 * 1024 * 512) {
			FREE_AND_NULL(cterm->strbuf);
			cterm->strbuflen = cterm->strbufsize = 0;
			return true;
		}
		p = realloc(cterm->strbuf, cterm->strbufsize);
		if (p == NULL) {
			FREE_AND_NULL(cterm->strbuf);
			cterm->strbuflen = cterm->strbufsize = 0;
			return true;
		}
		cterm->strbuf = p;
	}
	cterm->strbuf[cterm->strbuflen] = 0;
	/* DCS is the only string type that needs incremental parsing
	 * (sixel image data / DECDMAC macro body may be arbitrarily
	 * large).  Hand the newly-appended byte to the DEC parser; other
	 * string types buffer the whole payload and dispatch on ST. */
	if (cterm->string == CTERM_STRING_DCS)
		cterm_dec_dcs_byte(cterm);
	return true;
}

/* ------------------------------------------------------------------------
 * cterm_accumulate_sos — SOS string accumulator
 *
 * ECMA-48 SOS captures any bytes until an ST (ESC \) or a matching SOS
 * (ESC X) reappears.  When the terminator fires, any pending ESC in the
 * buffer is replayed through cterm_write so it is parsed in its new
 * (non-string) context.  Always returns true.
 * ------------------------------------------------------------------------ */
bool
cterm_accumulate_sos(struct cterminal *cterm, unsigned char byte, int *speed)
{
	unsigned char ch_byte = byte;

	if ((byte == 'X' || byte == '\\') &&
	    cterm->strbuf && cterm->strbuflen &&
	    cterm->strbuf[cterm->strbuflen - 1] == '\x1b') {
		cterm->strbuflen--;
		cterm->string = 0;
		cterm->accumulator = NULL;
		FREE_AND_NULL(cterm->strbuf);
		cterm->strbuflen = cterm->strbufsize = 0;
		cterm_write(cterm, "\x1b", 1, NULL, 0, speed);
		cterm_write(cterm, &ch_byte, 1, NULL, 0, speed);
		return true;
	}
	if (cterm->strbuf == NULL) {
		cterm->string = 0;
		cterm->accumulator = NULL;
		cterm->strbuflen = cterm->strbufsize = 0;
		return true;
	}
	cterm->strbuf[cterm->strbuflen++] = byte;
	if (cterm->strbuflen == cterm->strbufsize) {
		char *p;
		cterm->strbufsize *= 2;
		if (cterm->strbufsize > 1024 * 1024 * 512) {
			FREE_AND_NULL(cterm->strbuf);
			cterm->string = 0;
			cterm->accumulator = NULL;
			cterm->strbuflen = cterm->strbufsize = 0;
			return true;
		}
		p = realloc(cterm->strbuf, cterm->strbufsize);
		if (p == NULL) {
			cterm->string = 0;
			cterm->accumulator = NULL;
			FREE_AND_NULL(cterm->strbuf);
			cterm->strbuflen = cterm->strbufsize = 0;
			return true;
		}
		cterm->strbuf = p;
	}
	cterm->strbuf[cterm->strbuflen] = 0;
	return true;
}

/* ========================================================================
 * ECMA-48 byte-loop plumbing
 *
 * cterm_dispatch_ecma() — single-byte dispatcher for bytes outside a
 *   multi-byte mode.  Handles the C0 controls we act on (BEL, FF, ESC,
 *   NUL) and routes printable bytes into the print-batch buffer.
 *
 * cterm_accumulate_ecma_seq() — CSI/C1 accumulator that drives the
 *   incremental seq_feed() state machine and calls the per-emulation
 *   dispatch table handler when a sequence completes.
 * ======================================================================== */
/* ------------------------------------------------------------------------
 * cterm_dispatch_ecma — ANSI-BBS / ECMA-48 / CTerm single-byte dispatcher
 *
 * Runs for bytes that aren't inside a multi-byte mode (no active
 * accumulator).  Handles the ECMA-48 C0 control bytes we act on (BEL,
 * FF, ESC) and routes anything else to the print-batch buffer.  NUL is
 * the trigger for doorway one-shot when doorway_mode is on.  An ESC
 * flushes the print buffer and primes seq_feed() so the following
 * bytes are parsed by the CSI state machine.
 * ------------------------------------------------------------------------ */
void
cterm_dispatch_ecma(struct cterminal *cterm, unsigned char byte, int *speed)
{
	(void)speed;

	switch (byte) {
		case 0:
			cterm->lastch = 0;
			if (cterm->doorway_mode) {
				cterm->doorway_char = 1;
				cterm->accumulator = cterm_accumulate_doorway;
			}
			break;
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
		case 12:	/* FF — clear screen (CTerm legacy) */
			cterm->lastch = 0;
			cterm_uctputs(cterm, cterm->print_buf);
			cterm->print_buf[0] = 0;
			cterm->print_pos = cterm->print_buf;
			if (cterm->log == CTERM_LOG_ASCII && cterm->logfile != NULL)
				fputs("\x0c", cterm->logfile);
			cterm_clearscreen(cterm, (char)cterm->attr);
			gotoxy(CURR_MINX, CURR_MINY);
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
		default:
			cterm->lastch = byte;
			*cterm->print_pos++ = byte;
			*cterm->print_pos = 0;
	}
}

/* ------------------------------------------------------------------------
 * cterm_accumulate_ecma_seq — ANSI-BBS / ECMA-48 CSI accumulator
 *
 * Feeds one byte to seq_feed().  On SEQ_INCOMPLETE, the byte is held for
 * more input.  On SEQ_COMPLETE, the handler is bsearched and invoked.
 * On SEQ_BROKEN the parser state is reset; the ESC and broken byte are
 * emitted as literal text via the caller's print-batch buffer (*prnpos),
 * mirroring the legacy escbuf fallback.
 *
 * Returns true when the byte was absorbed (keep in sequence mode or the
 * sequence just completed / broke here).  Returns false only when
 * SEQ_BROKEN occurred AND the byte itself was ESC (broken-then-legal):
 * in that case the caller should re-feed the byte to start a fresh
 * sequence from the ESC.  (The current byte-loop handles this inline by
 * restarting itself; exposing it via a return value matches the same
 * semantics once the loop is unified.)
 * ------------------------------------------------------------------------ */
bool
cterm_accumulate_ecma_seq(struct cterminal *cterm, unsigned char byte, int *speed)
{
	enum sequence_state s;

	if (byte == 0)
		return true;	/* NUL: media-fill, ignore (ECMA-48 §8.3.88) */

	s = seq_feed(cterm, byte);
	if (s == SEQ_INCOMPLETE)
		return true;

	if (s == SEQ_BROKEN) {
		*cterm->print_pos++ = '\033';
		*cterm->print_pos++ = byte;
		*cterm->print_pos = 0;
		cterm->seq_phase = SEQ_PHASE_IDLE;
		cterm->sequence = 0;
		if (byte == '\033') {
			/* Broken-then-legal: drop the trailing ESC we just emitted
			 * and prime a fresh sequence that consumes this ESC.
			 * Leave cterm->accumulator on ecma_seq and
			 * cterm->saved_accumulator untouched — if we were nested
			 * inside a string, the next sequence completes or breaks
			 * back to it. */
			if (cterm->print_buf[0]) {
				cterm->print_pos -= 2;
				*cterm->print_pos = 0;
			}
			cterm_uctputs(cterm, cterm->print_buf);
			cterm->print_buf[0] = 0;
			cterm->print_pos = cterm->print_buf;
			cterm->sequence = 1;
			cterm->seq_phase = SEQ_PHASE_C1;
			cterm->seq_key = SEQ_ESC;
			cterm->seq_param_len = 0;
			cterm->seq_params[0] = 0;
		}
		else {
			/* Genuine break — restore the saved accumulator (non-NULL
			 * when an ESC inside a DCS/OSC/… string kicked us here) or
			 * clear otherwise. */
			cterm->accumulator = cterm->saved_accumulator;
			cterm->saved_accumulator = NULL;
		}
		return true;
	}

	/* SEQ_COMPLETE */
	{
		const struct seq_dispatch *d = NULL;
		cterm_accumulator_fn pre_handler;
		if (cterm->dispatch_table != NULL && cterm->dispatch_table_len > 0) {
			d = bsearch(&cterm->seq_key, cterm->dispatch_table,
			    cterm->dispatch_table_len,
			    sizeof(cterm->dispatch_table[0]),
			    cterm_seq_dispatch_cmp);
		}
		cterm_build_param_strs(cterm);
		memset(cterm->seq_consumed, 0, sizeof(cterm->seq_consumed));
		cterm->seq_consumed_any = false;
		if (d != NULL && d->trailing > 0) {
			/* Set up trailing-byte accumulation; handler fires
			 * when cterm_accumulate_trailing counts down. */
			cterm->seq_trailing_handler = d->handler;
			cterm->seq_trailing_remain = d->trailing;
			cterm->seq_param_count = 0;
			cterm->seq_phase = SEQ_PHASE_IDLE;
			cterm->sequence = 0;
			cterm->lastch = 0;
			cterm->saved_accumulator = NULL;
			cterm->accumulator = cterm_accumulate_trailing;
			return true;
		}
		pre_handler = cterm->accumulator;
		if (d != NULL && d->handler != NULL)
			d->handler(cterm, speed);
		cterm->seq_phase = SEQ_PHASE_IDLE;
		cterm->sequence = 0;
		cterm->lastch = 0;
		if (cterm->accumulator == pre_handler) {
			/* Handler didn't install a new mode.  Pop back to the
			 * saved accumulator (string context we interrupted) or
			 * clear to idle. */
			cterm->accumulator = cterm->saved_accumulator;
			cterm->saved_accumulator = NULL;
		}
		/* else: handler installed a new accumulator (dcs_start, sos_start,
		 * font_load, music, …) — keep it. */
	}
	return true;
}

/*
 * Emit the minimal SGR sequence to transition from prev to next cell attributes.
 * Legacy SGR first, then extended colors if needed.
 */
static void
sgr_diff(struct cterminal *cterm, const struct vmem_cell *prev, const struct vmem_cell *next)
{
	static const int cga2ansi[] = {0,4,2,6,1,5,3,7};
	int need_reset = 0;
	uint8_t pa = prev->legacy_attr;
	uint8_t na = next->legacy_attr;

	/* Check if we need a reset — if any attribute was ON in prev but OFF in next */
	if ((pa & 0x08) && !(na & 0x08))	/* bold off */
		need_reset = 1;
	if ((pa & 0x80) && !(na & 0x80))	/* blink off */
		need_reset = 1;

	if (need_reset) {
		cterm_respond_printf(cterm, "\033[0");
		if (na & 0x08)
			cterm_respond_printf(cterm, ";1");
		if (na & 0x80)
			cterm_respond_printf(cterm, ";5");
		cterm_respond_printf(cterm, ";%d", 30 + cga2ansi[na & 0x07]);
		cterm_respond_printf(cterm, ";%d", 40 + cga2ansi[(na >> 4) & 0x07]);
		cterm_respond_printf(cterm, "m");
	}
	else {
		int params = 0;
		char sgrbuf[128];
		sgrbuf[0] = '\0';

		if ((na & 0x08) && !(pa & 0x08)) {
			strcat(sgrbuf, params++ ? ";1" : "1");
		}
		if ((na & 0x80) && !(pa & 0x80)) {
			strcat(sgrbuf, params++ ? ";5" : "5");
		}
		if ((na & 0x07) != (pa & 0x07)) {
			char tmp[8];
			snprintf(tmp, sizeof(tmp), "%s%d", params++ ? ";" : "", 30 + cga2ansi[na & 0x07]);
			strcat(sgrbuf, tmp);
		}
		if (((na >> 4) & 0x07) != ((pa >> 4) & 0x07)) {
			char tmp[8];
			snprintf(tmp, sizeof(tmp), "%s%d", params++ ? ";" : "", 40 + cga2ansi[(na >> 4) & 0x07]);
			strcat(sgrbuf, tmp);
		}
		if (params > 0)
			cterm_respond_printf(cterm, "\033[%sm", sgrbuf);
	}

	if (next->fg != prev->fg) {
		if (next->fg & 0x80000000)
			cterm_respond_printf(cterm, "\033[38;2;%d;%d;%dm",
			    (next->fg >> 16) & 0xff, (next->fg >> 8) & 0xff, next->fg & 0xff);
		else if (next->fg >= 16)
			cterm_respond_printf(cterm, "\033[38;5;%dm", next->fg & 0xffff);
	}

	if (next->bg != prev->bg) {
		if (next->bg & 0x80000000)
			cterm_respond_printf(cterm, "\033[48;2;%d;%d;%dm",
			    (next->bg >> 16) & 0xff, (next->bg >> 8) & 0xff, next->bg & 0xff);
		else if (next->bg >= 16)
			cterm_respond_printf(cterm, "\033[48;5;%dm", next->bg & 0xffff);
	}

	if (next->font != prev->font)
		cterm_respond_printf(cterm, "\033[0;%d D", next->font);
}

/*
 * STS — Transmit selected area content.
 * Framed as: SOS CTerm:STS:<N>: <content> ST
 *
 * The response content is inside an SOS frame, so it must never contain
 * SOS (ESC X) or ST (ESC \) — either would prematurely terminate the
 * frame.  OSC 8 hyperlinks use ESC : \ (escaped ST) instead of ESC \.
 * Cell characters that are C0 controls (including ESC) or DEL are
 * doorway-encoded (FETM=INSERT) or replaced with SPACE (FETM=EXCLUDE),
 * so they cannot combine with following cells to form ESC X or ESC \.
 */
void
cterm_transmit_selected(struct cterminal *cterm)
{
	int start_row, start_col, end_row, end_col;
	int cur_row, cur_col;
	struct text_info ti;
	struct vmem_cell *cells;
	int total_cells, i;
	int screen_width;

	/* Emit SOS with prefix */
	cterm_respond_printf(cterm, "\033XCTerm:STS:%d:", cterm->fetm);

	/* No SSA — empty response */
	if (cterm->ssa_row == 0)
		goto done;

	gettextinfo(&ti);
	screen_width = ti.screenwidth;
	start_row = cterm->ssa_row;
	start_col = cterm->ssa_col;

	/* Get cursor position in screen coords */
	{
		int cx, cy;
		CURR_XY(&cx, &cy);
		cterm_coord_conv_xy(cterm, CTERM_COORD_CURR, CTERM_COORD_SCREEN, &cx, &cy);
		cur_row = cy;
		cur_col = cx;
	}

	/* Determine end position */
	if (cterm->ttm) {
		/* TTM=ALL: use ESA, or default to last cell of viewport */
		if (cterm->esa_row != 0) {
			end_row = cterm->esa_row;
			end_col = cterm->esa_col;
		}
		else {
			/* Default ESA: last cell of screen (or scroll region if origin mode) */
			end_row = cterm->y + cterm->height - 1;
			end_col = cterm->x + cterm->width - 1;
		}
	}
	else {
		/* TTM=CURSOR: end is one before cursor position (cursor excluded) */
		end_row = cur_row;
		end_col = cur_col - 1;
		if (end_col < 1) {
			end_row--;
			end_col = screen_width;
		}
		/* If cursor is at or before SSA, nothing eligible */
		if (end_row < start_row || (end_row == start_row && end_col < start_col))
			goto done;
	}

	/* Validate range */
	if (end_row < start_row || (end_row == start_row && end_col < start_col))
		goto done;

	/* Calculate total cells and read them all via vmem_gettext */
	/* Convert to a linear cell range */
	int start_linear = (start_row - 1) * screen_width + (start_col - 1);
	int end_linear = (end_row - 1) * screen_width + (end_col - 1);
	total_cells = end_linear - start_linear + 1;

	if (total_cells <= 0)
		goto done;

	/* Read cells row by row */
	cells = calloc(total_cells, sizeof(struct vmem_cell));
	if (cells == NULL)
		goto done;

	/* Read cells — by row for efficiency */
	{
		int pos = 0;
		int r = start_row;
		int c = start_col;
		while (pos < total_cells) {
			int row_end = (r == end_row) ? end_col : screen_width;
			int count = row_end - c + 1;
			if (pos + count > total_cells)
				count = total_cells - pos;
			vmem_gettext(c, r, c + count - 1, r, &cells[pos]);
			pos += count;
			r++;
			c = 1;
		}
	}

	if (cterm->fetm) {
		/* FETM=EXCLUDE: text only, C0/DEL → SPACE */
		for (i = 0; i < total_cells; i++) {
			uint8_t ch = cells[i].ch;
			if (ch < 0x20 || ch == 0x7f)
				ch = ' ';
			cterm_respond(cterm, (char *)&ch, 1);
		}
	}
	else {
		/* FETM=INSERT: characters with SGR diffs, hyperlinks, doorway encoding */
		struct vmem_cell prev = {
			.legacy_attr = 7,
			.ch = ' ',
			.font = 0,
			.fg = 7,
			.bg = 0,
			.hyperlink_id = 0,
		};

		for (i = 0; i < total_cells; i++) {
			/* Emit SGR diff */
			sgr_diff(cterm, &prev, &cells[i]);

			/* Emit hyperlink change.
			 * OSC 8 normally ends with ST (ESC \), but ST would
			 * prematurely close the SOS frame.  Use ESC : \ instead
			 * — ESC : is an unassigned Fp private-use escape that
			 * any conformant parser silently drops.  The host parser
			 * recognizes ESC : \ as "escaped ST" within STS content
			 * and converts it back to ESC \ for replay. */
			if (cells[i].hyperlink_id != prev.hyperlink_id) {
				if (cells[i].hyperlink_id == 0) {
					cterm_respond_printf(cterm, "\033]8;;\033:\\");
				}
				else {
					char *url = ciolib_get_hyperlink_url(cells[i].hyperlink_id);
					char *params = ciolib_get_hyperlink_params(cells[i].hyperlink_id);
					cterm_respond_printf(cterm, "\033]8;%s;%s\033:\\",
					    params ? params : "", url ? url : "");
					free(url);
					free(params);
				}
			}

			uint8_t ch = cells[i].ch;
			if (ch < 0x20 || ch == 0x7f) {
				/* Doorway encode C0/DEL */
				char doorway[] = "\033[=255h\x00X\033[=255l";
				doorway[8] = ch;
				cterm_respond(cterm, doorway, sizeof(doorway) - 1);
			}
			else {
				cterm_respond(cterm, (char *)&ch, 1);
			}
			prev = cells[i];
		}
	}

	free(cells);

done:
	/* Always emit ST to close the SOS */
	cterm_respond(cterm, "\033\\", 2);
}

/* enum sequence_state lives in cterm_internal.h */

/* ------------------------------------------------------------------------
 * seq_feed() -- the incremental control-sequence parser
 *
 * Feeds one byte at a time and maintains cterm->seq_phase, seq_key, and
 * seq_param_int[] as the sequence builds.  Returns SEQ_COMPLETE when
 * the final byte lands (handler can be dispatched from seq_key),
 * SEQ_INCOMPLETE while more bytes are expected, or SEQ_BROKEN on
 * malformed input.
 *
 * Caller initialises state when the leading ESC is seen:
 *   cterm->seq_phase = SEQ_PHASE_C1;
 *   cterm->seq_key   = SEQ_ESC;
 *   cterm->seq_param_count = 0;
 *   cterm->seq_param_len = 0;
 *   cterm->seq_params[0] = 0;
 *   cterm->seq_overflow = false;
 * ------------------------------------------------------------------------ */

static enum sequence_state
seq_feed(struct cterminal *cterm, unsigned char ch)
{
	switch (cterm->seq_phase) {
		case SEQ_PHASE_IDLE:
			return SEQ_BROKEN;

		case SEQ_PHASE_C1:
			/* First byte after ESC */
			if (ch >= 0x20 && ch <= 0x2F) {
				/* Intermediate byte -- nF sequence (ESC I...I F) */
				cterm->seq_key &= ~((uint32_t)SEQ_INTERM_MASK << SEQ_INTERM_SHIFT);
				cterm->seq_key |= SEQ_INTERM(ch);
				return SEQ_INCOMPLETE;
			}
			if (ch >= 0x30 && ch <= 0x7E) {
				if (ch >= 0x40 && ch <= 0x5F) {
					/* Fe byte: C1 introducer */
					cterm->seq_key = SEQ_C1(ch);
					if (ch == '[') {
						/* CSI -- expect params + final */
						cterm->seq_phase = SEQ_PHASE_PARAMS;
						cterm->seq_param_len = 0;
						cterm->seq_param_count = 0;
						cterm->seq_cur_value = 0;
						cterm->seq_cur_has_digits = false;
						cterm->seq_cur_has_sub = false;
						cterm->seq_overflow = false;
						cterm->seq_params[0] = 0;
						return SEQ_INCOMPLETE;
					}
					/* Other C1 standalones (RI, NEL, DCS, OSC, PM, APC, ST, ...) */
					cterm->seq_phase = SEQ_PHASE_COMPLETE;
					return SEQ_COMPLETE;
				}
				/* Fp (0x30-0x3F) or Fs (0x60-0x7E) with optional intermediate. */
				cterm->seq_key |= SEQ_FINAL(ch);
				cterm->seq_phase = SEQ_PHASE_COMPLETE;
				return SEQ_COMPLETE;
			}
			return SEQ_BROKEN;

		case SEQ_PHASE_PARAMS:
			if (ch >= 0x30 && ch <= 0x3F) {
				if (cterm->seq_overflow)
					return SEQ_INCOMPLETE;
				if (cterm->seq_param_len >= (int)(sizeof(cterm->seq_params) - 1)) {
					cterm->seq_overflow = true;
					return SEQ_INCOMPLETE;
				}
				cterm->seq_params[cterm->seq_param_len++] = ch;
				cterm->seq_params[cterm->seq_param_len] = 0;

				if (ch >= 0x3C) {
					/* Private indicator -- only valid as first byte. */
					if (cterm->seq_param_len == 1)
						cterm->seq_key |= SEQ_PRIV(ch);
					else
						return SEQ_BROKEN;
				}
				else if (ch == ';') {
					if (cterm->seq_param_count >= MAX_SEQ_PARAMS) {
						cterm->seq_overflow = true;
						return SEQ_INCOMPLETE;
					}
					cterm->seq_param_int[cterm->seq_param_count] =
					    cterm->seq_cur_has_digits
					        ? cterm->seq_cur_value : UINT64_MAX;
					cterm->seq_param_count++;
					cterm->seq_cur_value = 0;
					cterm->seq_cur_has_digits = false;
					cterm->seq_cur_has_sub = false;
				}
				else if (ch == ':') {
					if (!cterm->seq_cur_has_sub) {
						if (cterm->seq_param_count < MAX_SEQ_PARAMS) {
							cterm->seq_param_int[cterm->seq_param_count] =
							    cterm->seq_cur_has_digits
							        ? cterm->seq_cur_value : UINT64_MAX;
						}
						cterm->seq_cur_has_sub = true;
					}
					cterm->seq_cur_value = 0;
					cterm->seq_cur_has_digits = false;
				}
				else {
					/* Digit 0x30-0x39 */
					cterm->seq_cur_value = cterm->seq_cur_value * 10 + (ch - '0');
					cterm->seq_cur_has_digits = true;
				}
				return SEQ_INCOMPLETE;
			}
			/* Non-param byte -- finalise the last parameter and
			 * transition to intermediate/final handling.  A parameter
			 * is only present if we saw a digit in the current slot
			 * (cur_has_digits) or we already closed at least one slot
			 * (count > 0 via a ';' or ':').  Don't be misled by
			 * seq_param_len: that includes the private indicator
			 * byte, which is NOT a parameter (sequences like CSI ? s
			 * must finish with seq_param_count == 0 so handlers can
			 * detect "no explicit parameters"). */
			if (!cterm->seq_overflow) {
				if (cterm->seq_param_count < MAX_SEQ_PARAMS
				    && (cterm->seq_cur_has_digits
				        || cterm->seq_param_count > 0)) {
					if (!cterm->seq_cur_has_sub) {
						cterm->seq_param_int[cterm->seq_param_count] =
						    cterm->seq_cur_has_digits
						        ? cterm->seq_cur_value : UINT64_MAX;
					}
					cterm->seq_param_count++;
				}
			}
			cterm->seq_phase = SEQ_PHASE_INTERM;
			/* FALLTHROUGH */

		case SEQ_PHASE_INTERM:
			if (ch >= 0x20 && ch <= 0x2F) {
				if (!cterm->seq_overflow) {
					cterm->seq_key &= ~((uint32_t)SEQ_INTERM_MASK << SEQ_INTERM_SHIFT);
					cterm->seq_key |= SEQ_INTERM(ch);
				}
				return SEQ_INCOMPLETE;
			}
			if (ch >= 0x40 && ch <= 0x7E) {
				if (cterm->seq_overflow)
					return SEQ_BROKEN;
				cterm->seq_key |= SEQ_FINAL(ch);
				cterm->seq_phase = SEQ_PHASE_COMPLETE;
				return SEQ_COMPLETE;
			}
			return SEQ_BROKEN;

		case SEQ_PHASE_COMPLETE:
			return SEQ_BROKEN;
	}
	return SEQ_BROKEN;
}

/* Populate cterm->seq_param_strs[] with pointers to each ';'-separated
 * parameter in cterm->seq_params, NUL-terminating them in place.  Must
 * be called once per completed sequence before any handler reads
 * seq_param_strs (currently only SGR extended-colour parsing needs it).
 * Any private indicator byte ('<', '=', '>', '?') at the start of
 * seq_params is skipped — param N corresponds to seq_param_int[N]. */
static void
cterm_build_param_strs(struct cterminal *cterm)
{
	int n = 0;
	char *p = cterm->seq_params;

	if (*p >= '<' && *p <= '?')
		p++;
	while (n < MAX_SEQ_PARAMS) {
		cterm->seq_param_strs[n++] = p;
		while (*p && *p != ';')
			p++;
		if (*p == 0)
			break;
		*p++ = 0;	/* split this ';' */
	}
	while (n < MAX_SEQ_PARAMS)
		cterm->seq_param_strs[n++] = "";
}

/* ------------------------------------------------------------------------
 * cterm_ecma48_apply_sgr — base of the SGR cascade.
 *
 * Handles the SGR parameter set defined by ECMA-48: reset (0),
 * intensity / attribute on/off (1, 2, 4, 5, 6, 7, 8, 22, 25, 27),
 * foreground (30-37, 39), background (40-47, 49).  SkyPix palette
 * remapping is applied inline via cterm_skypix_color so the same
 * path paints the correct index whether SkyPix is active or not.
 *
 * Extensions (38/48, 90-107) are handled upstream in the cascade
 * (cterm_cterm_apply_sgr) and never reach this function.  Unhandled
 * values are silently ignored — ECMA-48 §8.3.117 reserves unassigned
 * SGR selective-parameter values for future standardisation.
 * ------------------------------------------------------------------------ */
void
cterm_ecma48_apply_sgr(struct cterminal *cterm, int *pi)
{
	struct text_info ti;
	int flags;

	gettextinfo(&ti);
	flags = getvideoflags();
	switch (cterm->seq_param_int[*pi]) {
		case 0:
			cterm_set_negative(cterm, false);
			cterm->attr = ti.normattr;
			attr2palette(cterm->attr, &cterm->fg_color, &cterm->bg_color);
			FREE_AND_NULL(cterm->fg_tc_str);
			FREE_AND_NULL(cterm->bg_tc_str);
			break;
		case 1:
			if (!cterm->skypix)
				cterm->attr |= 8;
			if (!(flags & CIOLIB_VIDEO_NOBRIGHT)) {
				attr2palette(cterm->attr,
				    cterm->negative ? NULL : &cterm->fg_color,
				    cterm->negative ? &cterm->fg_color : NULL);
				FREE_AND_NULL(cterm->fg_tc_str);
			}
			break;
		case 2:
			if (!cterm->skypix)
				cterm->attr &= 247;
			if (!(flags & CIOLIB_VIDEO_NOBRIGHT)) {
				attr2palette(cterm->attr,
				    cterm->negative ? NULL : &cterm->fg_color,
				    cterm->negative ? &cterm->fg_color : NULL);
				FREE_AND_NULL(cterm->fg_tc_str);
			}
			break;
		case 4:	/* Underscore */
			break;
		case 5:
		case 6:
			if (!cterm->skypix)
				cterm->attr |= 128;
			if (flags & CIOLIB_VIDEO_BGBRIGHT) {
				attr2palette(cterm->attr,
				    cterm->negative ? &cterm->bg_color : NULL,
				    cterm->negative ? NULL : &cterm->bg_color);
				FREE_AND_NULL(cterm->bg_tc_str);
			}
			break;
		case 7:
			cterm_set_negative(cterm, true);
			break;
		case 8:
			{
				int j = cterm->attr & 112;
				cterm->attr &= 112;
				cterm->attr |= j >> 4;
			}
			cterm->fg_color = cterm->bg_color;
			FREE_AND_NULL(cterm->fg_tc_str);
			if (cterm->bg_tc_str != NULL)
				cterm->fg_tc_str = strdup(cterm->bg_tc_str);
			break;
		case 22:
			cterm->attr &= 0xf7;
			if (!(flags & CIOLIB_VIDEO_NOBRIGHT)) {
				attr2palette(cterm->attr,
				    cterm->negative ? NULL : &cterm->fg_color,
				    cterm->negative ? &cterm->fg_color : NULL);
				FREE_AND_NULL(cterm->fg_tc_str);
			}
			break;
		case 25:
			cterm->attr &= 0x7f;
			if (flags & CIOLIB_VIDEO_BGBRIGHT) {
				attr2palette(cterm->attr,
				    cterm->negative ? &cterm->bg_color : NULL,
				    cterm->negative ? NULL : &cterm->bg_color);
				FREE_AND_NULL(cterm->bg_tc_str);
			}
			break;
		case 27:
			cterm_set_negative(cterm, false);
			break;
		case 30:
			cterm_set_fgattr(cterm, cterm_skypix_color(cterm, BLACK));
			break;
		case 31:
			cterm_set_fgattr(cterm, cterm_skypix_color(cterm, RED));
			break;
		case 32:
			cterm_set_fgattr(cterm, cterm_skypix_color(cterm, GREEN));
			break;
		case 33:
			cterm_set_fgattr(cterm, cterm_skypix_color(cterm, BROWN));
			break;
		case 34:
			cterm_set_fgattr(cterm, cterm_skypix_color(cterm, BLUE));
			break;
		case 35:
			cterm_set_fgattr(cterm, cterm_skypix_color(cterm, MAGENTA));
			break;
		case 36:
			cterm_set_fgattr(cterm, cterm_skypix_color(cterm, CYAN));
			break;
		case 37:
		case 39:
			cterm_set_fgattr(cterm, cterm_skypix_color(cterm, LIGHTGRAY));
			break;
		case 40:
		case 49:
			cterm_set_bgattr(cterm, cterm_skypix_color(cterm, BLACK));
			break;
		case 41:
			cterm_set_bgattr(cterm, cterm_skypix_color(cterm, RED));
			break;
		case 42:
			cterm_set_bgattr(cterm, cterm_skypix_color(cterm, GREEN));
			break;
		case 43:
			cterm_set_bgattr(cterm, cterm_skypix_color(cterm, BROWN));
			break;
		case 44:
			cterm_set_bgattr(cterm, cterm_skypix_color(cterm, BLUE));
			break;
		case 45:
			cterm_set_bgattr(cterm, cterm_skypix_color(cterm, MAGENTA));
			break;
		case 46:
			cterm_set_bgattr(cterm, cterm_skypix_color(cterm, CYAN));
			break;
		case 47:
			cterm_set_bgattr(cterm, cterm_skypix_color(cterm, LIGHTGRAY));
			break;
	}
}

