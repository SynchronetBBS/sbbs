/* Synchronet find string functions */

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

#ifndef FINDSTR_H_
#define FINDSTR_H_

#include "str_list.h"
#include "dllexport.h"

#define FINDSTR_MAX_LINE_LEN 1000

#ifdef __cplusplus
extern "C" {
#endif

DLLEXPORT bool      findstr(const char *insearch, const char *fname);
DLLEXPORT bool      find2strs(const char *str1, const char* str2, const char *fname, char* metadata);
DLLEXPORT bool      findstr_in_string(const char* insearchof, const char* pattern);
DLLEXPORT bool      findstr_in_list(const char* insearchof, str_list_t list, char* metadata);
DLLEXPORT bool      find2strs_in_list(const char* str1, const char* str2, str_list_t list, char* metadata);
DLLEXPORT str_list_t findstr_list(const char* fname);
/* Free a list returned by findstr_list(), in the same module/heap that		*/
/* allocated it (findstr_list is often dllimport'd, so callers must not free	*/
/* its result with their own locally-linked strListFree - see GitLab #1099)	*/
DLLEXPORT void      findstr_list_free(str_list_t* list);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
