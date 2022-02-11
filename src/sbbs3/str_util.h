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

#include "scfgdefs.h"	// scfg_t
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT char*		backslashcolon(char *str);
DLLEXPORT ulong		ahtoul(const char *str);	/* Converts ASCII hex to ulong */
DLLEXPORT char *	hexplus(uint num, char *str); 	/* Hex plus for 3 digits up to 9000 */
DLLEXPORT uint		hptoi(const char *str);
DLLEXPORT int		pstrcmp(const char **str1, const char **str2);  /* Compares pointers to pointers */
DLLEXPORT int		strsame(const char *str1, const char *str2);	/* Compares number of same chars */
DLLEXPORT char *	remove_ctrl_a(const char* instr, char* outstr);
DLLEXPORT char 		ctrl_a_to_ascii_char(char code);
DLLEXPORT char *	truncstr(char* str, const char* set);
DLLEXPORT char *	truncated_str(char* str, const char* set);
DLLEXPORT char *	ascii_str(uchar* str);
DLLEXPORT char *    replace_named_values(const char* src ,char* buf, size_t buflen,	 
                       const char* escape_seq, named_string_t* string_list,	 
                       named_int_t* int_list, BOOL case_sensitive);
DLLEXPORT char *	condense_whitespace(char* str);
DLLEXPORT char		exascii_to_ascii_char(uchar ch);
DLLEXPORT BOOL		findstr(const char *insearch, const char *fname);
DLLEXPORT BOOL		findstr_in_string(const char* insearchof, const char* pattern);
DLLEXPORT BOOL		findstr_in_list(const char* insearchof, str_list_t list);
DLLEXPORT str_list_t findstr_list(const char* fname);
DLLEXPORT BOOL		trashcan(scfg_t* cfg, const char *insearch, const char *name);
DLLEXPORT char *	trashcan_fname(scfg_t* cfg, const char *name, char* fname, size_t);
DLLEXPORT str_list_t trashcan_list(scfg_t* cfg, const char* name);
DLLEXPORT char *	convert_ansi(const char* src, char* dest, size_t, int width, BOOL ice_color);
DLLEXPORT char *	strip_ansi(char* str);
DLLEXPORT char *	strip_exascii(const char *str, char* dest);
DLLEXPORT char *	strip_space(const char *str, char* dest);
DLLEXPORT char *	strip_ctrl(const char *str, char* dest);
DLLEXPORT char *	strip_char(const char* str, char* dest, char);
DLLEXPORT char *	net_addr(net_t* net);
DLLEXPORT BOOL		valid_ctrl_a_attr(char a);
DLLEXPORT BOOL		valid_ctrl_a_code(char a);
DLLEXPORT size_t	strip_invalid_attr(char *str);
DLLEXPORT char *	ultoac(ulong l,char *str);
DLLEXPORT char *	rot13(char* str);
DLLEXPORT uint32_t	str_to_bits(uint32_t currval, const char *str);
DLLEXPORT BOOL		str_has_ctrl(const char*);
DLLEXPORT BOOL		str_is_ascii(const char*);
DLLEXPORT char *	utf8_to_cp437_inplace(char* str);
DLLEXPORT char *	sub_newsgroup_name(scfg_t*, sub_t*, char*, size_t);
DLLEXPORT char *	sub_area_tag(scfg_t*, sub_t*, char*, size_t);
DLLEXPORT char *	dir_area_tag(scfg_t*, dir_t*, char*, size_t);
DLLEXPORT char * 	get_ctrl_dir(BOOL warn);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
