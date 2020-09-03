/* Synchronet UTF-8 translation functions */

/* $Id: utf8.h,v 1.6 2019/08/03 08:05:09 rswindell Exp $ */

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
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef UTF8_H_
#define UTF8_H_

#include <stdbool.h>
#include <stdlib.h>
#include "unicode_defs.h"

#define UTF8_MAX_LEN 6	// Longest possible UTF-8 sequence

#if defined(__cplusplus)
extern "C" {
#endif

// Returns true if the string is valid UTF-8
bool utf8_str_is_valid(const char*);

// Returns the fixed printed-width of the UTF-8 string
size_t utf8_str_total_width(const char*);

// Return the count of chars within the specified width range in UTF-8 string (str)
size_t utf8_str_count_width(const char*, size_t min_width, size_t max_width);

// Normalizes (to ASCII) chars in UTF-8 string 'str', in-place, resulting in string <= original in length
char* utf8_normalize_str(char* str);

// Replace or strip UTF-8 sequences in str (in-place)
// 'lookup' is a Unicode codepoint look-up function (optional)
// 'unsupported_ch' is the character used to replace unsupported Unicode codepoints (optional)
// 'unsupported_zwch' is the character used to replace unsupported zero-width Unicode codepoints (optional)
// 'error_ch' is the character used to replace invalid UTF-8 sequence bytes (optional)
char* utf8_replace_chars(char* str, char (*lookup)(enum unicode_codepoint), char unsupported_ch, char unsupported_zwch, char error_ch);

// Convert a CP437 char string (src) to UTF-8 string (dest) up to 'maxlen' chars long (sans NUL-terminator)
// 'minval' can be used to limit the range of converted chars
int cp437_to_utf8_str(const char* src, char* dest, size_t maxlen, unsigned char minval);

// Decode a UTF-8 sequence to a UNICODE code point
int utf8_getc(const char* str, size_t len, enum unicode_codepoint* codepoint);

// Encode a UNICODE code point into a UTF-8 sequence (str)
int utf8_putc(char* str, size_t len, enum unicode_codepoint codepoint);

#if defined(__cplusplus)
}
#endif

#endif // Don't add anything after this line
