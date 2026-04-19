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
 * cterm_petscii.h — PETSCII control-byte bitmap, handler table, and
 * the C64 reverse-video attribute helpers shared with the byte-dispatch
 * print path in cterm.c.
 */

#ifndef _CTERM_PETSCII_H_
#define _CTERM_PETSCII_H_

#include <stdbool.h>
#include <stdint.h>
#include "cterm.h"

extern const uint8_t cterm_petscii_ctrl_bitmap[32];
extern void (*const cterm_petscii_handlers[256])(struct cterminal *);

void cterm_c64_set_reverse(struct cterminal *cterm, bool on);
uint8_t cterm_c64_get_attr(struct cterminal *cterm);

#endif /* _CTERM_PETSCII_H_ */
