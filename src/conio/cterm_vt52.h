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
 * cterm_vt52.h — VT52 / Atari ST VT52 single-byte dispatcher and the
 * per-emulation dispatch table.  The sequence-parsing path is shared
 * with ECMA-48 via seq_feed() / cterm_accumulate_ecma_seq (in cterm.c);
 * only the dispatcher and table differ.
 */

#ifndef _CTERM_VT52_H_
#define _CTERM_VT52_H_

#include <stddef.h>
#include "cterm.h"
#include "cterm_internal.h"	/* struct seq_dispatch */

void cterm_dispatch_vt52(struct cterminal *cterm, unsigned char byte,
    int *speed);

extern const struct seq_dispatch cterm_st_vt52_dispatch[];
extern const size_t cterm_st_vt52_dispatch_len;

#endif /* _CTERM_VT52_H_ */
