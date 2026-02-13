/* Synchronet string utility functions */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _STR_UTIL_H_
#define _STR_UTIL_H_

#include "gen_defs.h"
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT char*     backslashcolon(char *str);
DLLEXPORT ulong     ahtoul(const char *str);    /* Converts ASCII hex to ulong */
DLLEXPORT uint32_t  ahtou32(const char* str);   /* Converts ASCII hex to uint32_t */
DLLEXPORT int       pstrcmp(const char **str1, const char **str2);  /* Compares pointers to pointers */
DLLEXPORT int       strsame(const char *str1, const char *str2);    /* Compares number of same chars */
DLLEXPORT char *    remove_ctrl_a(const char* instr, char* outstr);
DLLEXPORT void      remove_end_substr(char* str, const char* substr);
DLLEXPORT char      ctrl_a_to_ascii_char(char code);
DLLEXPORT char *    truncstr(char* str, const char* set);
DLLEXPORT char *    truncated_str(char* str, const char* set);
DLLEXPORT char *    ascii_str(uchar* str);
DLLEXPORT char *    replace_named_values(const char* src, char* buf, size_t buflen
                                         , const char* escape_seq
                                         , named_string_t** strlist_list
                                         , named_string_t* string_list
                                         , named_long_t* int_list
                                         , bool case_sensitive);
DLLEXPORT char *    replace_chars(char *str, char c1, char c2);
DLLEXPORT char *    condense_whitespace(char* str);
DLLEXPORT char      exascii_to_ascii_char(uchar ch);
DLLEXPORT char *    convert_ansi(const char* src, char* dest, size_t, int width, bool ice_color);
DLLEXPORT char *    strip_ansi(char* str);
DLLEXPORT char *    strip_exascii(const char *str, char* dest);
DLLEXPORT char *    strip_cp437_graphics(const char *str, char* dest);
DLLEXPORT char *    strip_space(const char *str, char* dest);
DLLEXPORT char *    strip_ctrl(const char *str, char* dest);
DLLEXPORT char *    strip_char(const char* str, char* dest, char);
DLLEXPORT bool      valid_ctrl_a_attr(char a);
DLLEXPORT bool      valid_ctrl_a_code(char a);
DLLEXPORT size_t    strip_invalid_attr(char *str);
DLLEXPORT bool      contains_ctrl_a_attr(const char*);
DLLEXPORT char * u32toac(uint32_t, char*, char sep);
DLLEXPORT char * u64toac(uint64_t, char*, char sep);
DLLEXPORT char *    rot13(char* str);
DLLEXPORT uint32_t  str_to_bits(uint32_t currval, const char *str);
DLLEXPORT bool      str_has_ctrl(const char*);
DLLEXPORT bool      str_is_ascii(const char*);
DLLEXPORT char *    utf8_to_cp437_inplace(char* str);
DLLEXPORT char *    separate_thousands(const char* src, char *dest, size_t maxlen, char sep);
DLLEXPORT char *    make_newsgroup_name(char* str);
DLLEXPORT size_t	widest_line(const char* str);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
