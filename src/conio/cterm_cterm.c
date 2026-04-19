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
 * cterm_cterm.c — SyncTERM / CTerm / XTerm-private control-sequence handlers.
 *
 * Holds handlers for sequences specific to CTerm (the SyncTERM BBS terminal
 * emulation) plus a few XTerm device-attribute queries SyncTERM reuses:
 *   - CSI `=` music toggle (syncterm_music) and CSI `=` s SM/RM variants
 *   - BananANSI music (CSI N, repurposed ECMA-48 EF)
 *   - SCO cursor save/restore (SCOSC, SCORC) and the CSI s
 *     dispatcher that picks SCOSC vs DECSLRM based on DECLRMM
 *   - ANSI music (CSI M) dispatcher that picks music vs DL
 *   - Font load / select:   font_load, fnt/tsr, decscs (SCS designation)
 *   - ANSI music setup
 *   - Query sequences:      xterm_gfx_query, ext_state_query,
 *                           macro_query, decrqm_syncterm
 *   - Extended device-attr: syncterm_ext_da
 *   - CSI t (RGB palette set for FG/BG)
 *
 * Every handler uses the cterm_handle_* signature and is registered in
 * cterm_ansi_dispatch[] via keys from cterm_internal.h.
 */

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <xpprintf.h>	/* asprintf() on Win32 */

#include <genwrap.h>
#include <link_list.h>
#include <xpbeep.h>

#include "ciolib.h"
#include "cterm.h"
#include "cterm_internal.h"
#include "cterm_cterm.h"
#include "cterm_ecma48.h"	/* cterm_handle_sm/rm chained from SyncTERM ext */
#include "cterm_dec.h"	/* cterm_handle_decsm/decrm chained from ext */
#include "vidmodes.h"
#include "base64.h"
#include <crc16.h>

void
cterm_handle_syncterm_music(struct cterminal *cterm, int *speed)	/* CSI | */
{
	(void)speed;
	cterm->music = 1;
	cterm->accumulator = cterm_accumulate_music;
}

void
cterm_handle_fnt(struct cterminal *cterm, int *speed)	/* CSI SP D — Font Select */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 0);
	cterm_seq_default(cterm, 1, 0);
	switch (cterm->seq_param_int[0]) {
		case 0:
		case 1:
		case 2:
		case 3:
			if (setfont((int)cterm->seq_param_int[1], FALSE,
			    (int)cterm->seq_param_int[0] + 1) == 0)
				cterm->setfont_result = 1;
			else
				cterm->setfont_result = 0;
			if (cterm->setfont_result == 0)
				cterm->altfont[cterm->seq_param_int[0]] =
				    (int)cterm->seq_param_int[1];
			break;
	}
}

void
cterm_handle_tsr(struct cterminal *cterm, int *speed)	/* CSI SP d — delete tab stop */
{
	(void)speed;
	if (cterm->seq_param_count > 0)
		cterm_delete_tabstop(cterm, (int)cterm->seq_param_int[0]);
}

void
cterm_handle_decscs(struct cterminal *cterm, int *speed)	/* CSI Ps1;Ps2 * r — Set Comm Speed */
{
	int newspeed = -1;
	cterm_seq_default(cterm, 0, 0);
	cterm_seq_default(cterm, 1, 0);
	if (cterm->seq_param_int[0] < 2) {
		switch (cterm->seq_param_int[1]) {
			case 0:  newspeed = 0; break;
			case 1:  newspeed = 300; break;
			case 2:  newspeed = 600; break;
			case 3:  newspeed = 1200; break;
			case 4:  newspeed = 2400; break;
			case 5:  newspeed = 4800; break;
			case 6:  newspeed = 9600; break;
			case 7:  newspeed = 19200; break;
			case 8:  newspeed = 38400; break;
			case 9:  newspeed = 57600; break;
			case 10: newspeed = 76800; break;
			case 11: newspeed = 115200; break;
		}
	}
	if (newspeed >= 0 && speed != NULL) {
		*speed = newspeed;
		cterm->decscs_speed = newspeed;
	}
}

void
cterm_handle_ansi_music_setup(struct cterminal *cterm, int *speed)	/* CSI = M */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 0);
	switch (cterm->seq_param_int[0]) {
		case 0:
			cterm->music_enable = CTERM_MUSIC_SYNCTERM;
			break;
		case 1:
			cterm->music_enable = CTERM_MUSIC_BANSI;
			break;
		case 2:
			cterm->music_enable = CTERM_MUSIC_ENABLED;
			break;
	}
}

void
cterm_handle_syncterm_sm_ext(struct cterminal *cterm, int *speed)	/* CSI = h */
{
	int i;
	(void)speed;
	for (i = 0; i < cterm->seq_param_count; i++) {
		cterm_seq_default(cterm, i, 0);
		switch (cterm->seq_param_int[i]) {
			case 4:
				cterm->last_column_flag |= CTERM_LCF_ENABLED;
				cterm->last_column_flag &= ~CTERM_LCF_SET;
				break;
			case 5:
				cterm->last_column_flag |=
				    CTERM_LCF_FORCED | CTERM_LCF_ENABLED;
				break;
			case 255:
				cterm->doorway_mode = 1;
				break;
		}
	}
}

void
cterm_handle_syncterm_rm_ext(struct cterminal *cterm, int *speed)	/* CSI = l */
{
	int i;
	(void)speed;
	for (i = 0; i < cterm->seq_param_count; i++) {
		cterm_seq_default(cterm, i, 0);
		switch (cterm->seq_param_int[i]) {
			case 4:
				if ((cterm->last_column_flag & CTERM_LCF_FORCED) == 0)
					cterm->last_column_flag = 0;
				break;
			case 255:
				cterm->doorway_mode = 0;
				break;
		}
	}
}

void
cterm_handle_xterm_gfx_query(struct cterminal *cterm, int *speed)	/* CSI ? S */
{
	struct text_info ti;
	int vmode;
	(void)speed;
	cterm_seq_default(cterm, 0, 0);
	cterm_seq_default(cterm, 1, 0);
	if (cterm->seq_param_int[0] != 2 || cterm->seq_param_int[1] != 1)
		return;
	gettextinfo(&ti);
	vmode = find_vmode(ti.currmode);
	if (vmode != -1)
		cterm_respond_printf(cterm, "\x1b[?2;0;%u;%uS",
		    vparams[vmode].charwidth * cterm->width,
		    vparams[vmode].charheight * cterm->height);
}

void
cterm_handle_ext_state_query(struct cterminal *cterm, int *speed)	/* CSI = n */
{
	char tmp[3072];
	int vidflags;
	struct text_info ti;
	int vmode;
	(void)speed;
	tmp[0] = 0;
	if (cterm->seq_param_count > 1)
		return;
	cterm_seq_default(cterm, 0, 1);
	switch (cterm->seq_param_int[0]) {
		case 1:
			sprintf(tmp, "\x1b[=1;%u;%u;%u;%u;%u;%un",
			    CONIO_FIRST_FREE_FONT,
			    (uint8_t)cterm->setfont_result,
			    (uint8_t)cterm->altfont[0],
			    (uint8_t)cterm->altfont[1],
			    (uint8_t)cterm->altfont[2],
			    (uint8_t)cterm->altfont[3]);
			break;
		case 2:
			vidflags = getvideoflags();
			strcpy(tmp, "\x1b[=2");
			if (cterm->extattr & CTERM_EXTATTR_ORIGINMODE)
				strcat(tmp, ";6");
			if (cterm->extattr & CTERM_EXTATTR_AUTOWRAP)
				strcat(tmp, ";7");
			if (cterm->mouse_state_query(9, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";9");
			if (cterm->cursor == _NORMALCURSOR)
				strcat(tmp, ";25");
			if (vidflags & CIOLIB_VIDEO_ALTCHARS)
				strcat(tmp, ";31");
			if (vidflags & CIOLIB_VIDEO_NOBRIGHT)
				strcat(tmp, ";32");
			if (vidflags & CIOLIB_VIDEO_BGBRIGHT)
				strcat(tmp, ";33");
			if (vidflags & CIOLIB_VIDEO_BLINKALTCHARS)
				strcat(tmp, ";34");
			if (vidflags & CIOLIB_VIDEO_NOBLINK)
				strcat(tmp, ";35");
			if (cterm->extattr & CTERM_EXTATTR_DECLRMM)
				strcat(tmp, ";69");
			if (cterm->extattr & CTERM_EXTATTR_SXSCROLL)
				strcat(tmp, ";80");
			if (cterm->mouse_state_query(1000, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";1000");
			if (cterm->mouse_state_query(1001, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";1001");
			if (cterm->mouse_state_query(1002, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";1002");
			if (cterm->mouse_state_query(1003, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";1003");
			if (cterm->mouse_state_query(1004, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";1004");
			if (cterm->mouse_state_query(1005, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";1005");
			if (cterm->mouse_state_query(1006, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";1006");
			if (cterm->mouse_state_query(1007, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";1007");
			if (cterm->mouse_state_query(1015, cterm->mouse_state_query_cbdata))
				strcat(tmp, ";1015");
			if (strlen(tmp) == 4)
				strcat(tmp, ";");
			strcat(tmp, "n");
			break;
		case 3:	/* Query font char dimensions */
			gettextinfo(&ti);
			vmode = find_vmode(ti.currmode);
			if (vmode != -1)
				sprintf(tmp, "\x1b[=3;%u;%un",
				    vparams[vmode].charheight,
				    vparams[vmode].charwidth);
			break;
		case 4:
			sprintf(tmp, "\x1b[=4;%dn",
			    (cterm->last_column_flag & CTERM_LCF_ENABLED) ? 1 : 0);
			break;
		case 5:
			sprintf(tmp, "\x1b[=5;%dn",
			    (cterm->last_column_flag & CTERM_LCF_FORCED) ? 1 : 0);
			break;
		case 6:
			sprintf(tmp, "\x1b[=6;%dn", cio_api.vmem_gettext ? 1 : 0);
			break;
	}
	if (*tmp)
		cterm_respond(cterm, tmp, strlen(tmp));
}

void
cterm_handle_macro_query(struct cterminal *cterm, int *speed)	/* CSI ? n */
{
	char tmp[3072];
	size_t i, k;
	(void)speed;
	cterm_seq_default(cterm, 0, 1);
	tmp[0] = 0;
	switch (cterm->seq_param_int[0]) {
		case 62:	/* Query macro space available */
			if (cterm->seq_param_count > 1)
				return;
			strcpy(tmp, "\x1b[32767*{");
			break;
		case 63:	/* Query macro space "checksum" */
		{
			uint16_t crc = 0;
			if (cterm->seq_param_count > 2)
				return;
			cterm_seq_default(cterm, 1, 1);
			for (k = 0; k < (sizeof(cterm->macros) / sizeof(cterm->macros[0])); k++) {
				if (cterm->macros[k]) {
					for (i = 0; i <= cterm->macro_lens[k]; i++)
						crc = ucrc16(cterm->macros[k][i], crc);
				}
				else {
					crc = ucrc16(0, crc);
				}
			}
			snprintf(tmp, sizeof(tmp), "\x1bP%u!~%04X\x1b\\",
			    (unsigned)cterm->seq_param_int[1], crc);
			break;
		}
	}
	if (*tmp)
		cterm_respond(cterm, tmp, strlen(tmp));
}

void
cterm_handle_font_load(struct cterminal *cterm, int *speed)	/* CSI = { */
{
	(void)speed;
	cterm_seq_default(cterm, 0, 255);
	cterm_seq_default(cterm, 1, 0);
	if (cterm->seq_param_int[0] > 255)
		return;
	cterm->font_read = 0;
	cterm->font_slot = (int)cterm->seq_param_int[0];
	switch (cterm->seq_param_int[1]) {
		case 0:  cterm->font_size = 4096; break;
		case 1:  cterm->font_size = 3584; break;
		case 2:  cterm->font_size = 2048; break;
		default: cterm->font_size = 0;   break;
	}
	if (cterm->font_size > 0)
		cterm->accumulator = cterm_accumulate_font;
}

void
cterm_handle_decrqm_syncterm(struct cterminal *cterm, int *speed)	/* CSI = $ p */
{
	int pm = 0;
	(void)speed;
	if (cterm->seq_param_count != 1)
		return;
	cterm_seq_default(cterm, 0, 0);
	switch (cterm->seq_param_int[0]) {
		case 4:
			pm = (cterm->last_column_flag & CTERM_LCF_ENABLED) ? 1 : 2;
			break;
		case 5:
			pm = (cterm->last_column_flag & CTERM_LCF_FORCED) ? 3 : 2;
			break;
		case 255:
			pm = cterm->doorway_mode ? 1 : 2;
			break;
	}
	cterm_respond_printf(cterm, "\x1b[=%u;%d$y",
	    (unsigned)cterm->seq_param_int[0], pm);
}

void
cterm_handle_syncterm_ext_da(struct cterminal *cterm, int *speed)	/* CSI < c */
{
	char tmp[128];
	(void)speed;
	cterm_seq_default(cterm, 0, 0);
	tmp[0] = 0;
	if (cterm->seq_param_int[0] != 0)
		return;
	strcpy(tmp, "\x1b[<0");
	if (cio_api.options & CONIO_OPT_LOADABLE_FONTS)
		strcat(tmp, ";1");
	if (cio_api.options & CONIO_OPT_BRIGHT_BACKGROUND)
		strcat(tmp, ";2");
	if (cio_api.options & CONIO_OPT_PALETTE_SETTING)
		strcat(tmp, ";3");
	if (cio_api.options & CONIO_OPT_SET_PIXEL)
		strcat(tmp, ";4");
	if (cio_api.options & CONIO_OPT_FONT_SELECT)
		strcat(tmp, ";5");
	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
		strcat(tmp, ";6");
	if (cio_api.mouse)
		strcat(tmp, ";7");
	strcat(tmp, "c");
	cterm_respond(cterm, tmp, strlen(tmp));
}

void
cterm_handle_bansi_music(struct cterminal *cterm, int *speed)	/* CSI N */
{
	(void)speed;
	/* BananANSI style... does NOT start with MF or MB.
	 * This still conflicts with ANSI erase field. */
	if (cterm->music_enable >= CTERM_MUSIC_BANSI) {
		cterm->music = 2;
		cterm->accumulator = cterm_accumulate_music;
	}
}

void
cterm_handle_ansi_music(struct cterminal *cterm, int *speed)	/* CSI M */
{
	/* CSI M with no parameters enters "ANSI" music when music is
	 * enabled; otherwise it is the standard ECMA-48 DL. */
	if (cterm->music_enable == CTERM_MUSIC_ENABLED
	    && cterm->seq_param_count == 0) {
		cterm->music = 1;
		cterm->accumulator = cterm_accumulate_music;
		return;
	}
	cterm_handle_dl(cterm, speed);
}

void
cterm_handle_scosc(struct cterminal *cterm, int *speed)		/* CSI s (SCO) */
{
	(void)speed;
	CURR_XY(&cterm->save_xpos, &cterm->save_ypos);
}

void
cterm_handle_scorc(struct cterminal *cterm, int *speed)		/* CSI u (SCO) */
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
cterm_handle_csi_s(struct cterminal *cterm, int *speed)		/* CSI s */
{
	/* CSI s is DECSLRM when DECLRMM (left/right-margin mode) is set,
	 * otherwise SCOSC (save cursor position). */
	if (cterm->extattr & CTERM_EXTATTR_DECLRMM)
		cterm_handle_decslrm(cterm, speed);
	else
		cterm_handle_scosc(cterm, speed);
}

void
cterm_handle_csi_t(struct cterminal *cterm, int *speed)	/* CSI Pc ; Pr ; Pg ; Pb t */
{
	/* CTerm private: set FG or BG RGB palette entry. Pc=0 sets BG,
	 * Pc=1 sets FG; the remaining three parameters are 8-bit R/G/B. */
	uint32_t *c = NULL;
	uint32_t nc;
	(void)speed;
	if (cterm->seq_param_count < 4)
		return;
	if ((cterm->seq_param_int[0] == 0) ^ cterm->negative)
		c = &cterm->bg_color;
	else if ((cterm->seq_param_int[0] == 1) ^ cterm->negative)
		c = &cterm->fg_color;
	if (c == NULL)
		return;
	nc = map_rgb((uint16_t)(cterm->seq_param_int[1] << 8),
	    (uint16_t)(cterm->seq_param_int[2] << 8),
	    (uint16_t)(cterm->seq_param_int[3] << 8));
	if (nc != UINT32_MAX)
		*c = nc;
	setcolour(cterm->fg_color, cterm->bg_color);
}

/* ---- DECSET / DECRESET cascade (CSI ? h / CSI ? l) ---------------------
 * SyncTERM-private DEC private modes layer.  Processes CTerm-specific
 * modes (CIOLIB video flags 31-35, sixel-scroll 80) first, marks those
 * parameters consumed, then forwards the remaining parameters to the
 * DEC / XTerm handler in cterm_dec.c.  A final palette refresh is done
 * after the chain completes because CIOLIB_VIDEO_* flags change how the
 * current attribute maps to fg/bg colour. */

static void
cterm_decset_reset_common(struct cterminal *cterm, bool set)
{
	uint32_t oldfg, oldbg;
	bool updfg, updbg;
	int i, flags;

	attr2palette(cterm->attr, &oldfg, &oldbg);
	updfg = (oldfg == cterm->fg_color);
	updbg = (oldbg == cterm->bg_color);

	for (i = 0; i < cterm->seq_param_count; i++) {
		if (cterm_seq_param_consumed(cterm, i))
			continue;
		cterm_seq_default(cterm, i, 0);
		flags = getvideoflags();
		switch (cterm->seq_param_int[i]) {
			case 31:
				if (set)
					flags |= CIOLIB_VIDEO_ALTCHARS;
				else
					flags &= ~CIOLIB_VIDEO_ALTCHARS;
				setvideoflags(flags);
				break;
			case 32:
				if (set)
					flags |= CIOLIB_VIDEO_NOBRIGHT;
				else
					flags &= ~CIOLIB_VIDEO_NOBRIGHT;
				setvideoflags(flags);
				break;
			case 33:
				if (set)
					flags |= CIOLIB_VIDEO_BGBRIGHT;
				else
					flags &= ~CIOLIB_VIDEO_BGBRIGHT;
				setvideoflags(flags);
				break;
			case 34:
				if (set)
					flags |= CIOLIB_VIDEO_BLINKALTCHARS;
				else
					flags &= ~CIOLIB_VIDEO_BLINKALTCHARS;
				setvideoflags(flags);
				break;
			case 35:
				if (set)
					flags |= CIOLIB_VIDEO_NOBLINK;
				else
					flags &= ~CIOLIB_VIDEO_NOBLINK;
				setvideoflags(flags);
				break;
			case 80:
				if (set)
					cterm->extattr |= CTERM_EXTATTR_SXSCROLL;
				else
					cterm->extattr &= ~CTERM_EXTATTR_SXSCROLL;
				break;
			default:
				continue;
		}
		cterm_seq_consume_param(cterm, i);
	}

	/* Chain to the DEC / XTerm handler for any unconsumed params. */
	if (set)
		cterm_handle_decsm(cterm, NULL);
	else
		cterm_handle_decrm(cterm, NULL);

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
cterm_handle_decsm_cterm(struct cterminal *cterm, int *speed)	/* CSI ? h */
{
	(void)speed;
	cterm_decset_reset_common(cterm, true);
}

void
cterm_handle_decrm_cterm(struct cterminal *cterm, int *speed)	/* CSI ? l */
{
	(void)speed;
	cterm_decset_reset_common(cterm, false);
}

/* ---- DSR cascade (CSI n) ------------------------------------------------
 * CTerm adds param 255 ("report screen bottom-right as CPR") on top of
 * the ECMA-48 DSR.  Handle that here, mark it consumed, then let the
 * ECMA-48 handler take care of 5 / 6. */
void
cterm_handle_dsr_cterm(struct cterminal *cterm, int *speed)	/* CSI n */
{
	if (cterm->seq_param_count > 0
	    && !cterm_seq_param_consumed(cterm, 0)
	    && cterm->seq_param_int[0] == 255) {
		cterm_respond_printf(cterm, "\x1b[%d;%dR", CURR_MAXY, CURR_MAXX);
		cterm_seq_consume_param(cterm, 0);
	}
	cterm_handle_dsr(cterm, speed);
}

/* ========================================================================
 * String-type dispatchers invoked from the generic cterm_handle_st()
 * shell in cterm.c.
 * ======================================================================== */

/* ------------------------------------------------------------------------
 * cterm_cterm_handle_font_dcs — CTerm:Font:<slot>:<base64-payload>
 *
 * SyncTERM-private DCS extension that loads a user-defined font into a
 * slot.  Called from cterm_dec_dcs_finish() when the DCS strbuf starts
 * with "CTerm:Font:".  The prefix + colon have already been matched;
 * this helper parses the slot number and decodes the base64 body.
 * ------------------------------------------------------------------------ */
void
cterm_cterm_handle_font_dcs(struct cterminal *cterm)
{
	char *p, *p2;
	size_t i;

	cterm->font_slot = strtoul(cterm->strbuf + 11, &p, 10);
	if (cterm->font_slot < CONIO_FIRST_FREE_FONT)
		return;
	if (cterm->font_slot > 255)
		return;
	if (p && *p == ':') {
		p++;
		i = b64_decode(cterm->fontbuf, sizeof(cterm->fontbuf), p, 0);
		p2 = malloc(i);
		if (p2) {
			memcpy(p2, cterm->fontbuf, i);
			replace_font(cterm->font_slot,
			    strdup("Remote Defined Font"), p2, i);
		}
	}
}

/* ------------------------------------------------------------------------
 * cterm_cterm_osc_finish — OSC-content dispatcher
 *
 * ECMA-48 defines the OSC framing but leaves the contents up to the
 * host registration.  The sequences we handle here are XTerm's
 * "Change Color(s)" family (OSC 4 — palette set/query, OSC 104 —
 * palette reset, OSC 10/11 — default fg/bg query) and the XTerm/
 * SyncTERM hyperlink extension (OSC 8).
 * ------------------------------------------------------------------------ */
void
cterm_cterm_osc_finish(struct cterminal *cterm)
{
	char *p, *p2;
	size_t i;
	int palette_offset = 0;

	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
		palette_offset = 16;

	/* OSC 4 — Change colour palette */
	if (cterm->strbuf[0] == '4' && cterm->strbuf[1] == ';') {
		unsigned long index = ULONG_MAX;
		char *seqlast;
		p2 = &cterm->strbuf[2];
		while ((p = strtok_r(p2, ";", &seqlast)) != NULL) {
			p2 = NULL;
			if (index == ULONG_MAX) {
				index = strtoull(p, NULL, 10);
				if (index == ULONG_MAX || index > 13200)
					break;
			}
			else {
				if (p[0] == '?' && p[1] == 0) {
					uint8_t qr, qg, qb;
					if (getpalette(index + palette_offset, &qr, &qg, &qb))
						cterm_respond_printf(cterm,
						    "\033]4;%lu;rgb:%02x/%02x/%02x\033\\",
						    index, qr, qg, qb);
				}
				else if (strncmp(p, "rgb:", 4) == 0) {
					char *p3, *p4, *collast;
					uint16_t rgb[3];
					int ccount = 0;
					bool broken = false;
					p4 = &p[4];
					while (ccount < 3
					    && (p3 = strtok_r(p4, "/", &collast)) != NULL) {
						p4 = NULL;
						unsigned long v;
						v = strtoul(p3, NULL, 16);
						if (v > UINT16_MAX)
							break;
						switch (strlen(p3)) {
							case 1:
								rgb[ccount] = v | (v << 4) | (v << 8) | (v << 12);
								break;
							case 2:
								rgb[ccount] = v | (v << 8);
								break;
							case 3:
								rgb[ccount] = (v & 0x0f) | (v << 4);
								break;
							case 4:
								rgb[ccount] = v;
								break;
							default:
								broken = true;
								break;
						}
						ccount++;
					}
					if (ccount == 3 && !broken)
						cterm_setpalette(cterm, index + palette_offset,
						    rgb[0], rgb[1], rgb[2]);
				}
				else
					break;
				index = ULONG_MAX;
			}
		}
	}
	/* OSC 104 — reset colour palette */
	else if (strncmp("104", cterm->strbuf, 3) == 0) {
		if (strlen(cterm->strbuf) == 3) {
			for (i = 0; i < sizeof(dac_default) / sizeof(struct dac_colors); i++)
				cterm_setpalette(cterm, i + palette_offset,
				    dac_default[i].red << 8 | dac_default[i].red,
				    dac_default[i].green << 8 | dac_default[i].green,
				    dac_default[i].blue << 8 | dac_default[i].blue);
		}
		else if (cterm->strbuf[3] == ';') {
			char *seqlast;
			unsigned long pi;
			p2 = &cterm->strbuf[4];
			while ((p = strtok_r(p2, ";", &seqlast)) != NULL) {
				p2 = NULL;
				pi = strtoull(p, NULL, 10);
				if (pi < sizeof(dac_default) / sizeof(struct dac_colors))
					cterm_setpalette(cterm, pi + palette_offset,
					    dac_default[pi].red << 8 | dac_default[pi].red,
					    dac_default[pi].green << 8 | dac_default[pi].green,
					    dac_default[pi].blue << 8 | dac_default[pi].blue);
			}
		}
	}
	/* OSC 10 / 11 — default fg / bg colour query */
	else if (cterm->strbuf[0] == '1'
	    && (cterm->strbuf[1] == '0' || cterm->strbuf[1] == '1')
	    && cterm->strbuf[2] == ';'
	    && cterm->strbuf[3] == '?'
	    && cterm->strbuf[4] == 0) {
		uint32_t colour = cterm->strbuf[1] == '0'
		    ? cterm->default_fg_palette : cterm->default_bg_palette;
		cterm_respond_printf(cterm, "\x1b]1%c;rgb:%02x/%02x/%02x\x1b\\",
		    cterm->strbuf[1],
		    colour >> 16 & 0xff, colour >> 8 & 0xff, colour & 0xff);
	}
	/* OSC 8 — hyperlinks (XTerm extension) */
	else if (cterm->strbuf[0] == '8' && cterm->strbuf[1] == ';') {
		char *params = &cterm->strbuf[2];
		char *uri = strchr(params, ';');
		if (uri) {
			*uri++ = '\0';
			if (*uri) {
				char *id_param = NULL;
				char *ptmp = params;
				char *tok;
				char *savelast;
				while ((tok = strtok_r(ptmp, ":", &savelast)) != NULL) {
					ptmp = NULL;
					if (strnicmp(tok, "id=", 3) == 0)
						id_param = tok + 3;
				}
				ciolib_set_current_hyperlink(
				    ciolib_add_hyperlink(uri, id_param));
			}
			else {
				ciolib_set_current_hyperlink(0);
			}
		}
	}
}

/* ========================================================================
 * SyncTERM / CTerm / XTerm extensions — ANSI music, extended colour,
 * and the accumulators for doorway mode, music, and font loading.
 * None of these are ECMA-48; they all live here because they're the
 * SyncTERM / CTerm extensions.
 * ======================================================================== */

/* ---- ANSI music data tables ---- */
static const char *octave="C#D#EF#G#A#B";

/* Characters allowed in music strings... if one that is NOT in here is
 * found, CTerm leaves music capture mode and tosses the buffer away */
static const char *musicchars="aAbBcCdDeEfFgGlLmMnNoOpPsStT0123456789.-+#<> ";
static const uint note_frequency[]={	/* Hz*1000 */
/* Octave 0 (Note 1) */
	 65406
	,69296
	,73416
	,77782
	,82407
	,87307
	,92499
	,97999
	,103820
	,110000
	,116540
	,123470
/* Octave 1 */
	,130810
	,138590
	,146830
	,155560
	,164810
	,174610
	,185000
	,196000
	,207650
	,220000
	,233080
	,246940
/* Octave 2 */
	,261620
	,277180
	,293660
	,311130
	,329630
	,349230
	,369990
	,392000
	,415300
	,440000
	,466160
	,493880
/* Octave 3 */
	,523250
	,554370
	,587330
	,622250
	,659260
	,698460
	,739990
	,783990
	,830610
	,880000
	,932330
	,987770
/* Octave 4 */
	,1046500
	,1108700
	,1174700
	,1244500
	,1318500
	,1396900
	,1480000
	,1568000
	,1661200
	,1760000
	,1864600
	,1975500
/* Octave 5 */
	,2093000
	,2217500
	,2349300
	,2489000
	,2637000
	,2793800
	,2959900
	,3136000
	,3322400
	,3520000
	,3729300
	,3951100
/* Octave 6 */
	,4186000
	,4435000
	,4698600
	,4978000
	,5274000
	,5587000
	,5920000
	,6272000
	,6644000
	,7040000
	,7458600
	,7902200
};

struct note_params {
	int notenum;
	int	notelen;
	int	dotted;
	int	tempo;
	int	noteshape;
	int	foreground;
};

/* ---- ANSI music playback thread + driver ---- */
static void tone_or_beep(double freq, int duration, int device_open)
{
	if (device_open)
		xptone(freq, duration, WAVE_SHAPE_SINE_SAW_HARM);
	else
		xpbeep(freq, duration);
}

void cterm_playnote_thread(void *args)
{
	/* Tempo is quarter notes per minute */
	int duration;
	int pauselen;
	struct note_params *note;
	int	device_open=FALSE;
	struct cterminal *cterm=(struct cterminal *)args;

	SetThreadName("PlayNote");
	while(1) {
		if(device_open) {
			if(!listSemTryWaitBlock(&cterm->notes,5000)) {
				xptone_close();
				listSemWait(&cterm->notes);
			}
		}
		else
			listSemWait(&cterm->notes);
		note=listShiftNode(&cterm->notes);
		if(note==NULL)
			break;
		device_open=xptone_open();
		if(note->dotted)
			duration=360000/note->tempo;
		else
			duration=240000/note->tempo;
		duration/=note->notelen;
		switch(note->noteshape) {
			case CTERM_MUSIC_STACATTO:
				pauselen=duration/4;
				break;
			case CTERM_MUSIC_LEGATO:
				pauselen=0;
				break;
			case CTERM_MUSIC_NORMAL:
			default:
				pauselen=duration/8;
				break;
		}
		duration-=pauselen;
		if(note->notenum < 72 && note->notenum >= 0)
			tone_or_beep(((double)note_frequency[note->notenum])/1000,duration,device_open);
		else
			tone_or_beep(0,duration,device_open);
		if(pauselen)
			tone_or_beep(0,pauselen,device_open);
		if(note->foreground)
			sem_post(&cterm->note_completed_sem);
		free(note);
	}
	if (device_open)
		xptone_close();
	cterm->playnote_thread_running=FALSE;
	sem_post(&cterm->playnote_thread_terminated);
}

static void play_music(struct cterminal *cterm)
{
	int		i;
	char	*p;
	char	*out;
	int		offset;
	char	note;
	int		notelen;
	char	numbuf[10];
	int		dotted;
	int		notenum;
	struct	note_params *np;
	int		fore_count;

	if(cterm->quiet) {
		cterm->music=0;
		cterm->accumulator = NULL;
		cterm->musicbuf[0]=0;
		return;
	}
	p=cterm->musicbuf;
	fore_count=0;
	if(cterm->music==1) {
		switch(toupper(*p)) {
			case 'F':
				cterm->musicfore=TRUE;
				p++;
				break;
			case 'B':
				cterm->musicfore=FALSE;
				p++;
				break;
			case 'N':
				if(!isdigit(*(p+1))) {
					cterm->noteshape=CTERM_MUSIC_NORMAL;
					p++;
				}
				break;
			case 'L':
				cterm->noteshape=CTERM_MUSIC_LEGATO;
				p++;
				break;
			case 'S':
				cterm->noteshape=CTERM_MUSIC_STACATTO;
				p++;
				break;
		}
	}
	for(;*p;p++) {
		notenum=0;
		offset=0;
		switch(toupper(*p)) {
			case 'M':
				p++;
				switch(toupper(*p)) {
					case 'F':
						cterm->musicfore=TRUE;
						break;
					case 'B':
						cterm->musicfore=FALSE;
						break;
					case 'N':
						cterm->noteshape=CTERM_MUSIC_NORMAL;
						break;
					case 'L':
						cterm->noteshape=CTERM_MUSIC_LEGATO;
						break;
					case 'S':
						cterm->noteshape=CTERM_MUSIC_STACATTO;
						break;
					default:
						p--;
				}
				break;
			case 'T':						/* Tempo */
				out=numbuf;
				while(isdigit(*(p+1)))
					*(out++)=*(++p);
				*out=0;
				cterm->tempo=strtoul(numbuf,NULL,10);
				if(cterm->tempo>255)
					cterm->tempo=255;
				if(cterm->tempo<32)
					cterm->tempo=32;
				break;
			case 'O':						/* Octave */
				out=numbuf;
				while(isdigit(*(p+1)))
					*(out++)=*(++p);
				*out=0;
				cterm->octave=strtoul(numbuf,NULL,10);
				if(cterm->octave>6)
					cterm->octave=6;
				break;
			case 'L':
				out=numbuf;
				while(isdigit(*(p+1)))
					*(out++)=*(++p);
				*out=0;
				cterm->notelen=strtoul(numbuf,NULL,10);
				if(cterm->notelen<1)
					cterm->notelen=1;
				if(cterm->notelen>64)
					cterm->notelen=64;
				break;
			case 'N':						/* Note by number */
				if(isdigit(*(p+1))) {
					out=numbuf;
					while(isdigit(*(p+1))) {
						*(out++)=*(p+1);
						p++;
					}
					*out=0;
					notenum=strtoul(numbuf,NULL,10);
				}
				if(notenum==0) {
					notenum=-1;
					offset=1;
				}
				/* Fall-through */
			case 'A':						/* Notes in current octave by letter */
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
			case 'G':
			case 'P':
				note=toupper(*p);
				notelen=cterm->notelen;
				dotted=0;
				i=1;
				while(i) {
					i=0;
					if(*(p+1)=='+' || *(p+1)=='#') {	/* SHARP */
						offset+=1;
						p++;
						i=1;
					}
					if(*(p+1)=='-') {					/* FLAT */
						offset-=1;
						p++;
						i=1;
					}
					if(*(p+1)=='.') {					/* Dotted note (1.5*notelen) */
						dotted=1;
						p++;
						i=1;
					}
					if(isdigit(*(p+1))) {
						out=numbuf;
						while(isdigit(*(p+1))) {
							*(out++)=*(p+1);
							p++;
						}
						*out=0;
						notelen=strtoul(numbuf,NULL,10);
						i=1;
					}
				}
				if(note=='P') {
					notenum=-1;
					offset=0;
				}
				if(notenum==0) {
					out=(char*)strchr(octave,note);
					if(out==NULL) {
						notenum=-1;
						offset=1;
					}
					else {
						notenum=cterm->octave*12+1;
						notenum+=(out-octave);
					}
				}
				notenum+=offset;
				np=(struct note_params *)malloc(sizeof(struct note_params));
				if(np!=NULL) {
					np->notenum=notenum;
					np->notelen=notelen;
					if(np->notelen<1)
						np->notelen=1;
					if(np->notelen>64)
						np->notelen=64;
					np->dotted=dotted;
					np->tempo=cterm->tempo;
					np->noteshape=cterm->noteshape;
					np->foreground=cterm->musicfore;
					listPushNode(&cterm->notes, np);
					if(cterm->musicfore)
						fore_count++;
				}
				break;
			case '<':							/* Down one octave */
				cterm->octave--;
				if(cterm->octave<0)
					cterm->octave=0;
				break;
			case '>':							/* Up one octave */
				cterm->octave++;
				if(cterm->octave>6)
					cterm->octave=6;
				break;
		}
	}
	cterm->music=0;
	cterm->accumulator = NULL;
	cterm->musicbuf[0]=0;
	if (fore_count) {
		while(fore_count) {
			sem_wait(&cterm->note_completed_sem);
			fore_count--;
		}
		xptone_complete();
	}
}

/* ---- Extended-colour SGR parameter helpers (truecolor / 256-colour) ---- */
void cterm_save_extended_colour_seq(struct cterminal *cterm, int fg, int seqoff, int count)
{
	char **str = fg ? &cterm->fg_tc_str : &cterm->bg_tc_str;
	const char **p = cterm->seq_param_strs;

	if (*str)
		FREE_AND_NULL(*str);

	switch (count) {
		case 1:
			if (asprintf(str, "%s", p[seqoff]) < 0)
				*str = NULL;
			break;
		case 2:
			if (asprintf(str, "%s;%s", p[seqoff], p[seqoff+1]) < 0)
				*str = NULL;
			break;
		case 3:
			if (asprintf(str, "%s;%s;%s", p[seqoff], p[seqoff+1], p[seqoff+2]) < 0)
				*str = NULL;
			break;
		case 5:
			if (asprintf(str, "%s;%s;%s;%s;%s", p[seqoff], p[seqoff+1], p[seqoff+2], p[seqoff+3], p[seqoff+4]) < 0)
				*str = NULL;
			break;
	}
}

void cterm_parse_extended_colour(struct cterminal *cterm, int *i, bool fg)
{
	struct sub_params sub = {0};
	uint32_t nc;
	uint32_t *co;
	int palette_offset = 0;

	if (cio_api.options & CONIO_OPT_EXTENDED_PALETTE)
		palette_offset = 16;

	if (cterm == NULL || i == NULL)
		return;
	if (fg)
		FREE_AND_NULL(cterm->fg_tc_str);
	else
		FREE_AND_NULL(cterm->bg_tc_str);
	co = fg ? (&cterm->fg_color) : (&cterm->bg_color);
	if (*i >= cterm->seq_param_count)
		return;

	if (cterm->seq_param_strs[*i][2] == ':') {
		/* CSI 38 : 5 : X m   (indexed)
		 * CSI 38 : 2 : Z? : R : G : B m   (RGB with optional colour-space id) */
		if (cterm_parse_sub_parameters(&sub, cterm, *i)) {
			if (sub.param_count == 3 && sub.param_int[1] == 5) {
				*co = sub.param_int[2];
				cterm_save_extended_colour_seq(cterm, fg, *i, 1);
			}
			else if (sub.param_count >= 5 && sub.param_int[1] == 2) {
				if (sub.param_count == 5) {
					nc = map_rgb(sub.param_int[2]<<8, sub.param_int[3]<<8, sub.param_int[4]<<8);
					if (nc != UINT32_MAX)
						*co = nc;
					cterm_save_extended_colour_seq(cterm, fg, *i, 1);
				}
				else if (sub.param_count > 5) {
					nc = map_rgb(sub.param_int[3]<<8, sub.param_int[4]<<8, sub.param_int[5]<<8);
					if (nc != UINT32_MAX)
						*co = nc;
					cterm_save_extended_colour_seq(cterm, fg, *i, 1);
				}
			}
		}
	}
	else if ((*i)+1 < cterm->seq_param_count
	    && cterm->seq_param_int[(*i)+1] == 5
	    && cterm->seq_param_strs[(*i)+1][1] == ':') {
		/* CSI 38 ; 5 : X m */
		if (cterm_parse_sub_parameters(&sub, cterm, (*i)+1)) {
			if (sub.param_count == 2)
				*co = sub.param_int[1];
			cterm_save_extended_colour_seq(cterm, fg, *i, 2);
			(*i)++;
		}
	}
	else if ((*i)+2 < cterm->seq_param_count
	    && cterm->seq_param_int[(*i)+1] == 5) {
		/* CSI 38 ; 5 ; X m */
		*co = cterm->seq_param_int[(*i)+2] + palette_offset;
		cterm_save_extended_colour_seq(cterm, fg, *i, 3);
		*i += 2;
	}
	else if ((*i)+1 < cterm->seq_param_count
	    && cterm->seq_param_int[(*i)+1] == 2
	    && cterm->seq_param_strs[(*i)+1][1] == ':') {
		/* CSI 38 ; 2 : Z? : R : G : B m */
		if (cterm_parse_sub_parameters(&sub, cterm, (*i)+1)) {
			nc = UINT32_MAX;
			if (sub.param_count > 4)
				nc = map_rgb(sub.param_int[2]<<8, sub.param_int[3]<<8, sub.param_int[4]<<8);
			else if (sub.param_count == 4)
				nc = map_rgb(sub.param_int[1]<<8, sub.param_int[2]<<8, sub.param_int[3]<<8);
			if (nc != UINT32_MAX)
				*co = nc;
			cterm_save_extended_colour_seq(cterm, fg, *i, 2);
			*i += 1;
		}
	}
	else if ((*i)+4 < cterm->seq_param_count
	    && cterm->seq_param_int[(*i)+1] == 2) {
		/* CSI 38 ; 2 ; R ; G ; B m */
		nc = map_rgb(cterm->seq_param_int[(*i)+2]<<8,
		    cterm->seq_param_int[(*i)+3]<<8,
		    cterm->seq_param_int[(*i)+4]<<8);
		if (nc != UINT32_MAX)
			*co = nc;
		cterm_save_extended_colour_seq(cterm, fg, *i, 5);
		*i += 4;
	}
	cterm_free_sub_parameters(&sub);
}

/* ---- Accumulators for multi-byte modes (doorway / music / font) ---- */

/* ------------------------------------------------------------------------
 * cterm_accumulate_doorway — doorway-mode one-byte passthrough
 *
 * When doorway mode is active and the previous byte was NUL, the next
 * byte is written to the screen as a literal cell regardless of its
 * control-character value.  Clears the one-shot flag on exit.  Flushes
 * the caller's print-batch buffer before writing the cell.
 * ------------------------------------------------------------------------ */
bool
cterm_accumulate_doorway(struct cterminal *cterm, unsigned char byte, int *speed)
{
	struct vmem_cell tmpvc[1] = {0};
	int sx, sy, x, y;
	(void)speed;

	cterm_uctputs(cterm, cterm->print_buf);
	cterm->print_buf[0] = 0;
	cterm->print_pos = cterm->print_buf;
	tmpvc[0].ch = byte;
	tmpvc[0].legacy_attr = cterm->attr;
	tmpvc[0].fg = cterm->fg_color;
	tmpvc[0].bg = cterm->bg_color;
	tmpvc[0].font = ciolib_attrfont(cterm->attr);
	tmpvc[0].hyperlink_id = ciolib_get_current_hyperlink();
	SCR_XY(&sx, &sy);
	vmem_puttext(sx, sy, sx, sy, tmpvc);
	CURR_XY(&x, &y);
	cterm_advance_char(cterm, &x, &y, 1);
	cterm->doorway_char = 0;
	cterm->accumulator = NULL;
	return true;
}

/* ------------------------------------------------------------------------
 * cterm_accumulate_music — ANSI / BANSI music string accumulator
 *
 * Collects bytes into musicbuf until the terminator 0x0E (SO) fires the
 * music playback.  Any byte outside the allowed musicchars set aborts
 * the accumulation silently.  Touches hold_update / puttext_can_move so
 * rendering stays still across play_music().
 * ------------------------------------------------------------------------ */
bool
cterm_accumulate_music(struct cterminal *cterm, unsigned char byte, int *speed)
{
	int x, y;
	unsigned char ch[2] = {byte, 0};
	(void)speed;

	if (byte == 14) {
		hold_update = 0;
		puttext_can_move = 0;
		CURR_XY(&x, &y);
		gotoxy(x, y);
		ciolib_setcursortype(cterm->cursor);
		hold_update = 1;
		puttext_can_move = 1;
		play_music(cterm);
	}
	else if (strchr(musicchars, byte) != NULL) {
		if (strlen((char *)cterm->musicbuf) < sizeof(cterm->musicbuf) - 2)
			ustrcat(cterm->musicbuf, ch);
	}
	else {
		cterm->music = 0;
		cterm->accumulator = NULL;
		cterm->musicbuf[0] = 0;
	}
	return true;
}

/* ------------------------------------------------------------------------
 * cterm_accumulate_font — remote font download accumulator
 *
 * Collects exactly font_size raw bytes into fontbuf, then hands the
 * completed image to replace_font().  No sequence parsing — just bulk
 * byte capture.  Returns true while accumulation is active.
 * ------------------------------------------------------------------------ */
bool
cterm_accumulate_font(struct cterminal *cterm, unsigned char byte, int *speed)
{
	(void)speed;
	cterm->fontbuf[cterm->font_read++] = byte;
	if (cterm->font_read == cterm->font_size) {
		char *buf2 = malloc(cterm->font_size);
		if (buf2 != NULL) {
			memcpy(buf2, cterm->fontbuf, cterm->font_size);
			if (cterm->font_slot >= CONIO_FIRST_FREE_FONT
			    && cterm->font_slot < 256)
				replace_font(cterm->font_slot,
				    strdup("Remote Defined Font"),
				    buf2, cterm->font_size);
			else
				FREE_AND_NULL(buf2);
		}
		cterm->font_size = 0;
		cterm->accumulator = NULL;
	}
	return true;
}

/* ========================================================================
 * SGR cascade — SyncTERM / XTerm / aixterm additions
 *
 * Catches the SGR parameters ECMA-48 doesn't define:
 *   38, 48       — extended (256-colour / truecolor) fg / bg; delegate
 *                  to cterm_parse_extended_colour which advances *pi
 *                  through the sub-parameters.
 *   90-97        — aixterm "bright" foreground colours.
 *   100-107      — aixterm "bright" background colours.
 * Anything else is passed down the chain to cterm_dec_apply_sgr.
 * ======================================================================== */

static void
set_bright_fg(struct cterminal *cterm, unsigned char colour, int flags)
{
	if (!(flags & CIOLIB_VIDEO_NOBRIGHT)) {
		cterm_set_fgattr(cterm, colour);
		if (!cterm->skypix)
			cterm->attr |= 8;
		attr2palette(cterm->attr,
		    cterm->negative ? NULL : &cterm->fg_color,
		    cterm->negative ? &cterm->fg_color : NULL);
		FREE_AND_NULL(cterm->fg_tc_str);
	}
}

static void
set_bright_bg(struct cterminal *cterm, unsigned char colour, int flags)
{
	if (flags & CIOLIB_VIDEO_BGBRIGHT) {
		cterm_set_bgattr(cterm, colour);
		if (!cterm->skypix)
			cterm->attr |= 128;
		attr2palette(cterm->attr,
		    cterm->negative ? &cterm->bg_color : NULL,
		    cterm->negative ? NULL : &cterm->bg_color);
		FREE_AND_NULL(cterm->bg_tc_str);
	}
}

void
cterm_cterm_apply_sgr(struct cterminal *cterm, int *pi)
{
	int flags = getvideoflags();

	switch (cterm->seq_param_int[*pi]) {
		case 38:
			cterm_parse_extended_colour(cterm, pi, true ^ cterm->negative);
			return;
		case 48:
			cterm_parse_extended_colour(cterm, pi, false ^ cterm->negative);
			return;
		case 90:  set_bright_fg(cterm, BLACK,     flags); return;
		case 91:  set_bright_fg(cterm, RED,       flags); return;
		case 92:  set_bright_fg(cterm, GREEN,     flags); return;
		case 93:  set_bright_fg(cterm, BROWN,     flags); return;
		case 94:  set_bright_fg(cterm, BLUE,      flags); return;
		case 95:  set_bright_fg(cterm, MAGENTA,   flags); return;
		case 96:  set_bright_fg(cterm, CYAN,      flags); return;
		case 97:  set_bright_fg(cterm, LIGHTGRAY, flags); return;
		case 100: set_bright_bg(cterm, BLACK,     flags); return;
		case 101: set_bright_bg(cterm, RED,       flags); return;
		case 102: set_bright_bg(cterm, GREEN,     flags); return;
		case 103: set_bright_bg(cterm, BROWN,     flags); return;
		case 104: set_bright_bg(cterm, BLUE,      flags); return;
		case 105: set_bright_bg(cterm, MAGENTA,   flags); return;
		case 106: set_bright_bg(cterm, CYAN,      flags); return;
		case 107: set_bright_bg(cterm, LIGHTGRAY, flags); return;
	}
	cterm_dec_apply_sgr(cterm, pi);
}

