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
 * cterm_dec.h — handler prototypes for DEC private extensions implemented
 * in cterm_dec.c (DECSM, DECRM, DECRQM, DECCARA, DECFRA, rectangular-area
 * ops, DECSC/DECRC, …).  Registered in cterm_ansi_dispatch[] in cterm.c.
 */

#ifndef _CTERM_DEC_H_
#define _CTERM_DEC_H_

#include "cterm.h"

void cterm_handle_decscusr(struct cterminal *cterm, int *speed);
void cterm_handle_decrqm_public(struct cterminal *cterm, int *speed);
void cterm_handle_dectabsr(struct cterminal *cterm, int *speed);
void cterm_handle_decera(struct cterminal *cterm, int *speed);
void cterm_handle_decfra(struct cterminal *cterm, int *speed);
void cterm_handle_deccra(struct cterminal *cterm, int *speed);
void cterm_handle_decic(struct cterminal *cterm, int *speed);
void cterm_handle_decdc(struct cterminal *cterm, int *speed);
void cterm_handle_decsace(struct cterminal *cterm, int *speed);
void cterm_handle_deccara(struct cterminal *cterm, int *speed);
void cterm_handle_decrara(struct cterminal *cterm, int *speed);
void cterm_handle_decmacro(struct cterminal *cterm, int *speed);
void cterm_handle_deccksr(struct cterminal *cterm, int *speed);
void cterm_handle_decrqm_dec(struct cterminal *cterm, int *speed);
void cterm_handle_save_dec_modes(struct cterminal *cterm, int *speed);
void cterm_handle_restore_dec_modes(struct cterminal *cterm, int *speed);
void cterm_handle_decsc(struct cterminal *cterm, int *speed);
void cterm_handle_decrc(struct cterminal *cterm, int *speed);
void cterm_handle_ris(struct cterminal *cterm, int *speed);
void cterm_handle_decsm(struct cterminal *cterm, int *speed);
void cterm_handle_decrm(struct cterminal *cterm, int *speed);
void cterm_handle_decsm_cterm(struct cterminal *cterm, int *speed);
void cterm_handle_decrm_cterm(struct cterminal *cterm, int *speed);

/* DCS hooks invoked from the generic string-framing machinery in
 * cterm.c.  cterm_dec_dcs_begin() arms the sixel / DECDMAC detectors
 * at ESC P; cterm_dec_dcs_byte() drives the incremental parsers as
 * bytes arrive; cterm_dec_dcs_finish() runs at ST to finalise sixel /
 * macro output or handle DECRQSS / CTerm:Font content. */
void cterm_dec_dcs_begin(struct cterminal *cterm);
void cterm_dec_dcs_byte(struct cterminal *cterm);
void cterm_dec_dcs_finish(struct cterminal *cterm);

/* Middle of the SGR cascade.  No DEC-owned parameters yet — this
 * simply tail-calls cterm_ecma48_apply_sgr — but the slot exists so
 * future DEC-specific SGR values have a home. */
void cterm_dec_apply_sgr(struct cterminal *cterm, int *pi);

#endif /* _CTERM_DEC_H_ */
