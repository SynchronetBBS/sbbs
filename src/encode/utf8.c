/* Synchronet UTF-8 encode/decode/translate functions */

/* $Id: utf8.c,v 1.9 2019/08/03 08:05:09 rswindell Exp $ */

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

#include "utf8.h"
#include "unicode.h"
#include <stdbool.h>
#include <string.h>

char* utf8_normalize_str(char* str)
{
	char* dest = str;

	for(char* src = str; *src != 0; src++) {
		if(*src == '\xC2' && *(src + 1) == '\xA0') { // NO-BREAK SPACE
			src++;
			*dest++ = ' ';
			continue;
		}
		if(*src == '\xE2') {
			if(*(src + 1) == '\x80') {
				switch(*(src + 2)) {
					case '\x82': // EN SPACE
					case '\x83': // EM SPACE
						src += 2;
						*dest++ = ' ';
						continue;
					case '\x8B': // ZERO WIDTH SPACE
					case '\x8C': // ZERO WIDTH NON-JOINER
					case '\x8D': // ZERO WIDTH JOINER
						src += 2;
						continue;
					case '\x90': // HYPHEN
					case '\x91': // NON-BREAKING HYPHEN
					case '\x92': // FIGURE DASH
					case '\x93': // EN DASH
						src += 2;
						*dest++ = '-';
						continue;
					case '\x98': // LEFT SINGLE QUOTATION MARK
						src += 2;
						*dest++ = '`';
						continue;
					case '\x99': // RIGHT SINGLE QUOTATION MARK
					case '\xB2': // PRIME
						src += 2;
						*dest++ = '\'';
						continue;
					case '\x9C': // LEFT DOUBLE QUOTATION MARK
					case '\x9D': // RIGHT DOUBLE QUOTATION MARK
						src += 2;
						*dest++ = '"';
						continue;
					case '\xA6': // HORIZONTAL ELLIPSIS -> ASCII periods (3)
						src += 2;
						for(int i = 0; i < 3; i++)
							*dest++ =  '.';
						continue;
				}
			}
			else if(*(src + 1) == '\x81') {
				switch(*(src + 2)) {
					case '\x83': // HYPEN BULLET
						src += 2;
						*dest++ = '-';
						continue;
					case '\x84': // FRACTION SLASH
						src += 2;
						*dest++ = '/';
						continue;
				}
			}
			else if(*(src + 1) == '\x88') {
				switch(*(src + 2)) {
					case '\x92': // MINUS SIGN
						src += 2;
						*dest++ = '-';
						continue;
				}
			}
		}
		else if(*src == '\xEF') {
			if(*(src + 1) == '\xBB' && *(src + 2) == '\xBF') {
				// Zero Width No-Break Space (BOM, ZWNBSP)
				src += 2;
				continue;
			}
			if(*(src + 1) == '\xBC') {
				if(*(src + 2) >= '\x81' && *(src + 2) <= '\xBF') { // FULLWIDTH EXCLAMATION MARK through FULLWIDTH LOW LINE
					src += 2;
					*src -= '\x81';
					*dest++ = '!' + *src;
					continue;
				}
			}
			else if(*(src + 1) == '\xBD') {
				if(*(src + 2) >= '\x80' && *(src + 2) <= '\x9E') { // FULLWIDTH GRAVE ACCENT through FULLWIDTH TILDE
					src += 2;
					*src -= '\x80';
					*dest++ = '`' + *src;
					continue;
				}
			}
		}
		*dest++ = *src;
	}
	*dest = 0;
	return str;
}

/* Replace all multi-byte UTF-8 sequences with 'ch' or 'zwch' (when non-zero) */
/* When ch and zwch are 0, effectively strips all UTF-8 chars from str */
char* utf8_replace_chars(char* str, char (*lookup)(enum unicode_codepoint), char unsupported_ch, char unsupported_zwch, char error_ch)
{
	char* end = str + strlen(str);
	char* dest = str;

	int len ;
	for(char* src= str; src < end; src += len) {
		if(!(*src & 0x80)) {
			*dest++ = *src;
			len = 1;
			continue;
		}
		enum unicode_codepoint codepoint = 0;
		len = utf8_getc(src, end - src, &codepoint);
		if(len < 2) {
			if(error_ch)
				*dest++ = error_ch;
			len = 1;
			continue;
		}
		if(lookup != NULL) {
			char ch = lookup(codepoint);
			if(ch) {
				*dest++ = ch;
				continue;
			}
		}
		if(unicode_width(codepoint) == 0) {
			if(unsupported_zwch)
				*dest++ = unsupported_zwch;
		} 
		else if(unsupported_ch)
			*dest++ = unsupported_ch;
	}
	*dest = 0;
	return str;
}

bool utf8_str_is_valid(const char* str)
{
	const char* end = str + strlen(str);
	while (str < end) {
		int len = utf8_getc(str, end - str, NULL);
		if (len < 1)
			return false;
		str += len;
	}
	return true;
}

// Return the total printed-width of UTF-8 string (str) accounting for zero/half/full-width codepoints
size_t utf8_str_total_width(const char* str)
{
	size_t count = 0;
	const char* end = str + strlen(str);
	while (str < end) {
		enum unicode_codepoint codepoint = 0;
		int len = utf8_getc(str, end - str, &codepoint);
		if (len < 1)
			break;
		count += unicode_width(codepoint);
		str += len;
	}
	return count;
}

// Return the count of chars within the specified width range in UTF-8 string (str)
size_t utf8_str_count_width(const char* str, size_t min_width, size_t max_width)
{
	size_t count = 0;
	const char* end = str + strlen(str);
	while (str < end) {
		enum unicode_codepoint codepoint = 0;
		int len = utf8_getc(str, end - str, &codepoint);
		if (len < 1)
			break;
		size_t width = unicode_width(codepoint);
		if(width >= min_width && width <= max_width)
			count++;
		str += len;
	}
	return count;
}

int cp437_to_utf8_str(const char* str, char* dest, size_t maxlen, unsigned char minval)
{
	int retval = 0;
	size_t outlen = 0;
	for(const unsigned char* p = (const unsigned char*)str; *p != 0; p++) {
		if(outlen >= maxlen) {
			retval = -1;
			break;
		}
		enum unicode_codepoint codepoint = 0;
		if(*p >= minval)
			codepoint = cp437_unicode_tbl[*p];
		if(codepoint) {
			retval = utf8_putc(dest + outlen, maxlen - outlen, codepoint);
			if(retval < 1)
				break;
			outlen += retval;
		} else {
			*(dest + outlen) = *p;
			outlen++;
		}
	}
	*(dest + outlen) = 0;
	return retval;
}


// From openssl/crypto/asn1/a_utf8.c:
/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

/* UTF8 utilities */

/*-
 * This parses a UTF8 string one character at a time. It is passed a pointer
 * to the string and the length of the string. It sets 'value' to the value of
 * the current character. It returns the number of characters read or a
 * negative error code:
 * -1 = string too short
 * -2 = illegal character
 * -3 = subsequent characters not of the form 10xxxxxx
 * -4 = character encoded incorrectly (not minimal length).
 */

int utf8_getc(const char *str, size_t len, enum unicode_codepoint* val)
{
    const unsigned char *p;
    unsigned long value;
    int ret;
    if (len <= 0)
        return 0;
    p = (const unsigned char*)str;

    /* Check syntax and work out the encoded value (if correct) */
    if ((*p & 0x80) == 0) {
        value = *p++ & 0x7f;
        ret = 1;
    } else if ((*p & 0xe0) == 0xc0) {
        if (len < 2)
            return -1;
        if ((p[1] & 0xc0) != 0x80)
            return -3;
        value = (*p++ & 0x1f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x80)
            return -4;
        ret = 2;
    } else if ((*p & 0xf0) == 0xe0) {
        if (len < 3)
            return -1;
        if (((p[1] & 0xc0) != 0x80)
            || ((p[2] & 0xc0) != 0x80))
            return -3;
        value = (*p++ & 0xf) << 12;
        value |= (*p++ & 0x3f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x800)
            return -4;
        ret = 3;
    } else if ((*p & 0xf8) == 0xf0) {
        if (len < 4)
            return -1;
        if (((p[1] & 0xc0) != 0x80)
            || ((p[2] & 0xc0) != 0x80)
            || ((p[3] & 0xc0) != 0x80))
            return -3;
        value = ((unsigned long)(*p++ & 0x7)) << 18;
        value |= (*p++ & 0x3f) << 12;
        value |= (*p++ & 0x3f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x10000)
            return -4;
        ret = 4;
    } else if ((*p & 0xfc) == 0xf8) {
        if (len < 5)
            return -1;
        if (((p[1] & 0xc0) != 0x80)
            || ((p[2] & 0xc0) != 0x80)
            || ((p[3] & 0xc0) != 0x80)
            || ((p[4] & 0xc0) != 0x80))
            return -3;
        value = ((unsigned long)(*p++ & 0x3)) << 24;
        value |= ((unsigned long)(*p++ & 0x3f)) << 18;
        value |= ((unsigned long)(*p++ & 0x3f)) << 12;
        value |= (*p++ & 0x3f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x200000)
            return -4;
        ret = 5;
    } else if ((*p & 0xfe) == 0xfc) {
        if (len < 6)
            return -1;
        if (((p[1] & 0xc0) != 0x80)
            || ((p[2] & 0xc0) != 0x80)
            || ((p[3] & 0xc0) != 0x80)
            || ((p[4] & 0xc0) != 0x80)
            || ((p[5] & 0xc0) != 0x80))
            return -3;
        value = ((unsigned long)(*p++ & 0x1)) << 30;
        value |= ((unsigned long)(*p++ & 0x3f)) << 24;
        value |= ((unsigned long)(*p++ & 0x3f)) << 18;
        value |= ((unsigned long)(*p++ & 0x3f)) << 12;
        value |= (*p++ & 0x3f) << 6;
        value |= *p++ & 0x3f;
        if (value < 0x4000000)
            return -4;
        ret = 6;
	} else
        return -2;
	if(val != NULL)
		*val = value;
    return ret;
}

/*
 * This takes a character 'value' and writes the UTF8 encoded value in 'str'
 * where 'str' is a buffer containing 'len' characters. Returns the number of
 * characters written or -1 if 'len' is too small. 'str' can be set to NULL
 * in which case it just returns the number of characters. It will need at
 * most 6 characters.
 */

int utf8_putc(char *str, size_t len, enum unicode_codepoint value)
{
    if (!str)
        len = 6;                /* Maximum we will need */
    else if (len <= 0)
        return -1;
    if (value < 0x80) {
        if (str)
            *str = (unsigned char)value;
        return 1;
    }
    if (value < 0x800) {
        if (len < 2)
            return -1;
        if (str) {
            *str++ = (unsigned char)(((value >> 6) & 0x1f) | 0xc0);
            *str = (unsigned char)((value & 0x3f) | 0x80);
        }
        return 2;
    }
    if (value < 0x10000) {
        if (len < 3)
            return -1;
        if (str) {
            *str++ = (unsigned char)(((value >> 12) & 0xf) | 0xe0);
            *str++ = (unsigned char)(((value >> 6) & 0x3f) | 0x80);
            *str = (unsigned char)((value & 0x3f) | 0x80);
        }
        return 3;
    }
    if (value < 0x200000) {
        if (len < 4)
            return -1;
        if (str) {
            *str++ = (unsigned char)(((value >> 18) & 0x7) | 0xf0);
            *str++ = (unsigned char)(((value >> 12) & 0x3f) | 0x80);
            *str++ = (unsigned char)(((value >> 6) & 0x3f) | 0x80);
            *str = (unsigned char)((value & 0x3f) | 0x80);
        }
        return 4;
    }
    if (value < 0x4000000) {
        if (len < 5)
            return -1;
        if (str) {
            *str++ = (unsigned char)(((value >> 24) & 0x3) | 0xf8);
            *str++ = (unsigned char)(((value >> 18) & 0x3f) | 0x80);
            *str++ = (unsigned char)(((value >> 12) & 0x3f) | 0x80);
            *str++ = (unsigned char)(((value >> 6) & 0x3f) | 0x80);
            *str = (unsigned char)((value & 0x3f) | 0x80);
        }
        return 5;
    }
    if (len < 6)
        return -1;
    if (str) {
        *str++ = (unsigned char)(((value >> 30) & 0x1) | 0xfc);
        *str++ = (unsigned char)(((value >> 24) & 0x3f) | 0x80);
        *str++ = (unsigned char)(((value >> 18) & 0x3f) | 0x80);
        *str++ = (unsigned char)(((value >> 12) & 0x3f) | 0x80);
        *str++ = (unsigned char)(((value >> 6) & 0x3f) | 0x80);
        *str = (unsigned char)((value & 0x3f) | 0x80);
    }
    return 6;
}
