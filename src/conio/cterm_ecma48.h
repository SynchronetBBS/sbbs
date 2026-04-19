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
 * cterm_ecma48.h — handler prototypes for the ECMA-48 control functions
 * implemented in cterm_ecma48.c.  Registered in cterm_ansi_dispatch[] in
 * cterm.c.  Handler signatures are uniform:
 *   void fn(struct cterminal *cterm, int *speed);
 * Parameters are read from cterm->seq_param_int[] / cterm->seq_params.
 */

#ifndef _CTERM_ECMA48_H_
#define _CTERM_ECMA48_H_

#include "cterm.h"

/* Cursor movement */
void cterm_handle_cuu(struct cterminal *cterm, int *speed);
void cterm_handle_cud(struct cterminal *cterm, int *speed);
void cterm_handle_cuf(struct cterminal *cterm, int *speed);
void cterm_handle_cub(struct cterminal *cterm, int *speed);
void cterm_handle_cnl(struct cterminal *cterm, int *speed);
void cterm_handle_cpl(struct cterminal *cterm, int *speed);
void cterm_handle_cha(struct cterminal *cterm, int *speed);
void cterm_handle_cup(struct cterminal *cterm, int *speed);
void cterm_handle_cht(struct cterminal *cterm, int *speed);
void cterm_handle_cvt(struct cterminal *cterm, int *speed);
void cterm_handle_cbt(struct cterminal *cterm, int *speed);
void cterm_handle_hpr(struct cterminal *cterm, int *speed);
void cterm_handle_vpa(struct cterminal *cterm, int *speed);
void cterm_handle_vpr(struct cterminal *cterm, int *speed);
void cterm_handle_hpb(struct cterminal *cterm, int *speed);
void cterm_handle_vpb(struct cterminal *cterm, int *speed);

/* Editing / erase */
void cterm_handle_ich(struct cterminal *cterm, int *speed);
void cterm_handle_ed(struct cterminal *cterm, int *speed);
void cterm_handle_el(struct cterminal *cterm, int *speed);
void cterm_handle_il(struct cterminal *cterm, int *speed);
void cterm_handle_dl(struct cterminal *cterm, int *speed);
void cterm_handle_dch(struct cterminal *cterm, int *speed);
void cterm_handle_ech(struct cterminal *cterm, int *speed);
void cterm_handle_rep(struct cterminal *cterm, int *speed);

/* Scrolling */
void cterm_handle_su(struct cterminal *cterm, int *speed);
void cterm_handle_sd(struct cterminal *cterm, int *speed);
void cterm_handle_sl(struct cterminal *cterm, int *speed);
void cterm_handle_sr(struct cterminal *cterm, int *speed);
void cterm_handle_bansi_music(struct cterminal *cterm, int *speed);

/* Reporting / modes / SGR */
void cterm_handle_da(struct cterminal *cterm, int *speed);
void cterm_handle_tbc(struct cterminal *cterm, int *speed);
void cterm_handle_sm(struct cterminal *cterm, int *speed);
void cterm_handle_rm(struct cterminal *cterm, int *speed);
void cterm_handle_dsr(struct cterminal *cterm, int *speed);
void cterm_handle_dsr_cterm(struct cterminal *cterm, int *speed);
void cterm_handle_decstbm(struct cterminal *cterm, int *speed);
void cterm_handle_decslrm(struct cterminal *cterm, int *speed);
void cterm_handle_scosc(struct cterminal *cterm, int *speed);
void cterm_handle_csi_s(struct cterminal *cterm, int *speed);
void cterm_handle_csi_t(struct cterminal *cterm, int *speed);
void cterm_handle_scorc(struct cterminal *cterm, int *speed);
void cterm_handle_sgr(struct cterminal *cterm, int *speed);

/* C1 Fe standalone */
void cterm_handle_nel(struct cterminal *cterm, int *speed);
void cterm_handle_ssa(struct cterminal *cterm, int *speed);
void cterm_handle_esa(struct cterminal *cterm, int *speed);
void cterm_handle_hts(struct cterminal *cterm, int *speed);
void cterm_handle_vts(struct cterminal *cterm, int *speed);
void cterm_handle_ri(struct cterminal *cterm, int *speed);
void cterm_handle_sts(struct cterminal *cterm, int *speed);

/* Command-string framing: the five introducers (DCS / SOS / OSC / PM /
 * APC) + the ST terminator.  The framing itself is ECMA-48; the string
 * CONTENTS are dispatched to per-standard finishers (cterm_dec_dcs_*
 * in cterm_dec.c, cterm_cterm_osc_finish in cterm_cterm.c, user-
 * supplied cterm->apc_handler for APC). */
void cterm_handle_dcs_start(struct cterminal *cterm, int *speed);
void cterm_handle_sos_start(struct cterminal *cterm, int *speed);
void cterm_handle_osc_start(struct cterminal *cterm, int *speed);
void cterm_handle_pm_start(struct cterminal *cterm, int *speed);
void cterm_handle_apc_start(struct cterminal *cterm, int *speed);
void cterm_handle_st(struct cterminal *cterm, int *speed);

/* ECMA-48 command-string accumulators invoked from cterm_pick_accumulator
 * in cterm.c.  SOS gets its own accumulator because it captures literally
 * everything up to the ST, while the other four (DCS / OSC / PM / APC)
 * share cterm_accumulate_command_string. */
bool cterm_accumulate_command_string(struct cterminal *cterm, unsigned char byte,
    int *speed);
bool cterm_accumulate_sos(struct cterminal *cterm, unsigned char byte,
    int *speed);

/* Base of the SGR cascade — handles the ECMA-48 parameters; called
 * from cterm_dec_apply_sgr (which in turn is called from
 * cterm_cterm_apply_sgr, which is the root per cterm_apply_sgr in
 * cterm.c). */
void cterm_ecma48_apply_sgr(struct cterminal *cterm, int *pi);

#endif /* _CTERM_ECMA48_H_ */
