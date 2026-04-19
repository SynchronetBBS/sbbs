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
 * cterm_prestel.h — Prestel / Viewdata / BBC Micro (BEEB) serial-attribute
 * helpers, accumulators, and single-byte dispatchers.
 *
 * The serial-attribute helpers are also called from cterm_reset,
 * cterm_gotoxy, and cterm_adjust_currpos in cterm.c to re-apply Prestel
 * line state after motion or reset.
 */

#ifndef _CTERM_PRESTEL_H_
#define _CTERM_PRESTEL_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "cterm.h"

/* cterm_set_fgattr / cterm_set_bgattr are general fg/bg attribute
 * setters defined in cterm.c.  Prestel calls them from the serial-
 * attribute state machine; SGR colour handlers (ecma48, cterm) use
 * them too — see cterm_internal.h for their declarations. */

/* Serial-attribute helpers */
void cterm_prestel_new_line(struct cterminal *cterm);
void cterm_prestel_apply_ctrl_before(struct cterminal *cterm, uint8_t ch);
void cterm_prestel_apply_ctrl_after(struct cterminal *cterm, uint8_t ch);
void cterm_prestel_apply_ctrl(struct cterminal *cterm, uint8_t ch);
void cterm_prestel_colour(struct cterminal *cterm, bool alpha, int colour);
void cterm_prestel_get_state(struct cterminal *cterm);
void cterm_prestel_move(struct cterminal *cterm, int xadj, int yadj);
void cterm_prestel_send_memory(struct cterminal *cterm, uint8_t mem,
    char *retbuf, size_t retsize);
void cterm_prestel_handle_escaped(struct cterminal *cterm, uint8_t ctrl);
void cterm_prestel_fix_line(struct cterminal *cterm, int x, int y,
    bool restore, bool force);

/* Accumulator + dispatchers plugged into the cterm_write byte loop.
 * BEEB has no sequence accumulator — VDU 23 / VDU 28 install the generic
 * cterm_accumulate_trailing via cterm->seq_trailing_handler. */
bool cterm_accumulate_prestel_seq(struct cterminal *cterm, unsigned char byte,
    int *speed);
void cterm_dispatch_prestel(struct cterminal *cterm, unsigned char byte,
    int *speed);
void cterm_dispatch_beeb(struct cterminal *cterm, unsigned char byte,
    int *speed);

#endif /* _CTERM_PRESTEL_H_ */
