/* Synchronet Unicode encode/decode/translate functions */

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
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef UNICODE_H_
#define UNICODE_H_

#include <stdlib.h>
#include "gen_defs.h"
#include "unicode_defs.h"

#if defined(__cplusplus)
extern "C" {
#endif

extern enum unicode_codepoint cp437_unicode_tbl[];
bool unicode_is_zerowidth(enum unicode_codepoint);
size_t unicode_width(enum unicode_codepoint, size_t zerowidth);
char unicode_to_cp437(enum unicode_codepoint);
char unicode_to_latin1(enum unicode_codepoint);

#if defined(__cplusplus)
}
#endif

#endif // Don't add anything after this line
