/* Synchronet UTF-8 translation functions */

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

#ifndef UTF8_H_
#define UTF8_H_

#include <stdlib.h>
#include "gen_defs.h" // bool
#include "unicode_defs.h"

#define UTF8_MAX_LEN 6  // Longest possible UTF-8 sequence

#if defined(__cplusplus)
extern "C" {
#endif

// Decode a UTF-8 first byte, returns length of character sequence (1-4) or 0 on error
int utf8_decode_firstbyte(char ch);

// Returns true if the string is valid UTF-8
bool utf8_str_is_valid(const char*);

// Returns the fixed printed-width of the UTF-8 string
size_t utf8_str_total_width(const char*, size_t zerowidth);

// Return the count of chars within the specified width range in UTF-8 string (str)
size_t utf8_str_count_width(const char*, size_t min_width, size_t max_width, size_t zerowidth);

// Like strlcpy(), but doesn't leave a partial UTF-8 sequence at the end of dst
size_t utf8_strlcpy(char* dst, const char* src, size_t size);

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
int utf8_to_cp437_str(const char *src, char *dest, size_t maxlen, unsigned char minval, size_t *outlen);

// Convert a Latin1 char string (src) to UTF-8 string (dest) up to 'maxlen' bytes long (sans NUL-terminator)
// 'minval' can be used to limit the range of converted chars.  On return, *outlen is set to the number
// of bytes written to dest unless it is NULL
int latin1_to_utf8_str(const char* str, char* dest, size_t maxlen, unsigned char minval, size_t *outlen);
int utf8_to_latin1_str(const char *src, char *dest, size_t maxlen, unsigned char minval, size_t *outlen);

// Decode a UTF-8 sequence to a UNICODE code point
int utf8_getc(const char* str, size_t len, enum unicode_codepoint* codepoint);

// Encode a UNICODE code point into a UTF-8 sequence (str)
int utf8_putc(char* str, size_t len, enum unicode_codepoint codepoint);

#if defined(__cplusplus)
}
#endif

#endif // Don't add anything after this line
