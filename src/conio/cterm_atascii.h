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
 * cterm_atascii.h — ATASCII control-byte bitmap and handler table.
 * Consumed by cterm_dispatch_byte (in cterm.c) when the terminal is
 * in CTERM_EMULATION_ATASCII mode.
 */

#ifndef _CTERM_ATASCII_H_
#define _CTERM_ATASCII_H_

#include <stdint.h>
#include "cterm.h"

extern const uint8_t cterm_atascii_ctrl_bitmap[32];
extern void (*const cterm_atascii_handlers[256])(struct cterminal *);

#endif /* _CTERM_ATASCII_H_ */
