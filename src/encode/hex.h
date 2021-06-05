/* Hex encode/decode (e.g. URL encode/decode) functions */

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

#ifndef hex_h_
#define hex_h_

#include <stdlib.h>	 // size_t

#ifdef __cplusplus
extern "C" {
#endif

char* hex_encode(char esc, const char* src, char* chars, char* dest, size_t size);
char* hex_decode(char esc, char* str);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */