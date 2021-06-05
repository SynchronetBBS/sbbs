/* Hexadecimal encode/decode (e.g. URL encode/decode) functions */

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
 
#include "hex.h"
#include "genwrap.h"
#include "gen_defs.h"

char* hex_encode(char esc, const char* src, char* chars, char* dest, size_t size)
{
	char* result = dest;
	char* end = dest + (size - 1);

	while(*src != '\0' && dest < end) {
		if((*src == esc || strchr(chars, *src) != NULL) && dest + 3 < end)
			dest += sprintf(dest, "%c%2X", esc, *src);
		else
			*(dest++) = *src;
		src++;
	}
	*end = '\0';
	return result;
}

char* hex_decode(char esc, char* str)
{
	char* src = str;
	char* dest = str;
	while(*src != '\0') {
		if(*src == esc && IS_HEXDIGIT(*(src + 1)) && IS_HEXDIGIT(*(src + 2))) {
			src++;
			*dest = HEX_CHAR_TO_INT(*src) << 4;
			src++;
			*dest |= HEX_CHAR_TO_INT(*src);
		} else
			*dest = *src;
		src++;
		dest++;
	}
	*dest = '\0';
	return str;
}
