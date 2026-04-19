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
 * cterm_cterm.h — handler prototypes for SyncTERM / XTerm / CTerm private
 * extensions implemented in cterm_cterm.c (ANSI music, font loading,
 * SyncTERM SGR extensions, extended state queries, …).  Registered in
 * cterm_ansi_dispatch[] in cterm.c.
 */

#ifndef _CTERM_CTERM_H_
#define _CTERM_CTERM_H_

#include "cterm.h"

void cterm_handle_syncterm_music(struct cterminal *cterm, int *speed);
void cterm_handle_fnt(struct cterminal *cterm, int *speed);
void cterm_handle_tsr(struct cterminal *cterm, int *speed);
void cterm_handle_decscs(struct cterminal *cterm, int *speed);
void cterm_handle_ansi_music_setup(struct cterminal *cterm, int *speed);
void cterm_handle_ansi_music(struct cterminal *cterm, int *speed);
void cterm_handle_syncterm_sm_ext(struct cterminal *cterm, int *speed);
void cterm_handle_syncterm_rm_ext(struct cterminal *cterm, int *speed);
void cterm_handle_xterm_gfx_query(struct cterminal *cterm, int *speed);
void cterm_handle_ext_state_query(struct cterminal *cterm, int *speed);
void cterm_handle_macro_query(struct cterminal *cterm, int *speed);
void cterm_handle_font_load(struct cterminal *cterm, int *speed);
void cterm_handle_decrqm_syncterm(struct cterminal *cterm, int *speed);
void cterm_handle_syncterm_ext_da(struct cterminal *cterm, int *speed);

/* String-type dispatchers called from the generic ST handler in cterm.c
 * (for OSC) or from cterm_dec_dcs_finish (for the CTerm:Font DCS
 * extension, which is SyncTERM-private but travels inside DEC DCS
 * framing). */
void cterm_cterm_osc_finish(struct cterminal *cterm);
void cterm_cterm_handle_font_dcs(struct cterminal *cterm);

/* ANSI music playback thread — spawned from cterm_start() in cterm.c. */
void cterm_playnote_thread(void *args);

/* CTerm / SyncTERM accumulators called from cterm_pick_accumulator
 * in cterm.c.  doorway passes one raw byte after a NUL, music
 * captures a string until the SO terminator, font collects a fixed
 * number of bytes into fontbuf. */
bool cterm_accumulate_doorway(struct cterminal *cterm, unsigned char byte,
    int *speed);
bool cterm_accumulate_music(struct cterminal *cterm, unsigned char byte,
    int *speed);
bool cterm_accumulate_font(struct cterminal *cterm, unsigned char byte,
    int *speed);

/* Extended-colour SGR helpers (truecolor / 256-colour — SGR 38/48).
 * Called from cterm_cterm_apply_sgr for the 38/48 sub-parameter
 * parsing and from the DECRQSS save-back path. */
void cterm_save_extended_colour_seq(struct cterminal *cterm, int fg,
    int seqoff, int count);
void cterm_parse_extended_colour(struct cterminal *cterm, int *i, bool fg);

/* Top of the SGR cascade.  Owns the extensions ECMA-48 doesn't define
 * (38/48 extended colour, 90-107 aixterm bright).  Tail-calls
 * cterm_dec_apply_sgr for anything else. */
void cterm_cterm_apply_sgr(struct cterminal *cterm, int *pi);

#endif /* _CTERM_CTERM_H_ */
