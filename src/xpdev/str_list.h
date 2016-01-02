/* str_list.h */

/* Functions to deal with NULL-terminated string lists */

/* $Id$ */

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

#ifndef _STR_LIST_H
#define _STR_LIST_H

#include <stdio.h>			/* FILE */
#include <stddef.h>         /* size_t */
#include "gen_defs.h"
#include "wrapdll.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define STR_LIST_LAST_INDEX	(~0)

typedef char** str_list_t;

/* Returns an allocated and terminated string list */
DLLEXPORT str_list_t DLLCALL	strListInit(void);

/* Frees the strings in the list (and the list itself) */
DLLEXPORT void DLLCALL		strListFree(str_list_t*);

/* Frees the strings in the list */
DLLEXPORT void DLLCALL		strListFreeStrings(str_list_t);

/* Pass a pointer to a string list, the string to add (append) */
/* Returns the updated list or NULL on error */
DLLEXPORT char* DLLCALL		strListAppend(str_list_t*, const char* str, size_t index);

/* Append a string list onto another string list */
DLLEXPORT size_t DLLCALL	strListAppendList(str_list_t*, const str_list_t append_list);

/* Inserts a string into the list at a specific index */
DLLEXPORT char* DLLCALL		strListInsert(str_list_t*, const char* str, size_t index);

/* Insert a string list into another string list */
DLLEXPORT size_t DLLCALL	strListInsertList(str_list_t*, const str_list_t append_list, size_t index);

/* Remove a string at a specific index */
DLLEXPORT char* DLLCALL		strListRemove(str_list_t*, size_t index);

/* Remove and free a string at a specific index */
DLLEXPORT BOOL DLLCALL		strListDelete(str_list_t*, size_t index);

/* Replace a string at a specific index */
DLLEXPORT char* DLLCALL		strListReplace(const str_list_t, size_t index, const char* str);

/* Swap the strings at index1 and index2 */
DLLEXPORT BOOL DLLCALL		strListSwap(const str_list_t, size_t index1, size_t index2);

/* Convenience macros for pushing, popping strings (LIFO stack) */
#define		strListPush(list, str)	strListAppend(list, str, STR_LIST_LAST_INDEX)
#define		strListPop(list)		strListRemove(list, STR_LIST_LAST_INDEX)

/* Add to an exiting or new string list by splitting specified string (str) */
/* into multiple strings, separated by one of the delimit characters */
DLLEXPORT str_list_t DLLCALL	strListSplit(str_list_t*, char* str, const char* delimit);

/* Same as above, but copies str to temporary heap buffer first */
DLLEXPORT str_list_t DLLCALL	strListSplitCopy(str_list_t*, const char* str, const char* delimit);

/* Merge 2 string lists (no copying of string data) */
DLLEXPORT size_t DLLCALL		strListMerge(str_list_t*, str_list_t append_list);

/* Create a single delimited string from the specified list */
/* If buf is NULL, the buf is malloc'd and should be freed using strListFreeBlock() */
/* Note: maxlen includes '\0' terminator */
DLLEXPORT char* DLLCALL		strListCombine(str_list_t, char* buf, size_t maxlen, const char* delimit);

/* Count the number of strings in the list and returns the count */
DLLEXPORT size_t DLLCALL	strListCount(const str_list_t);

/* Returns the index of the specified str (by ptr compare) or -1 if not found */
DLLEXPORT int DLLCALL		strListIndexOf(const str_list_t, const char* str);
/* Returns the index of the specified str (by string compare) or -1 if not found */
DLLEXPORT int DLLCALL		strListFind(const str_list_t, const char* str, BOOL case_sensitive);

/* Sort the strings in the string list */
DLLEXPORT void DLLCALL		strListSortAlpha(str_list_t);
DLLEXPORT void DLLCALL		strListSortAlphaReverse(str_list_t);

/* Case-sensitive sorting */
DLLEXPORT void DLLCALL		strListSortAlphaCase(str_list_t);
DLLEXPORT void DLLCALL		strListSortAlphaCaseReverse(str_list_t);

/* Create/Copy/Append/Free NULL-terminated string block */
/* (e.g. for environment variable blocks) */
DLLEXPORT char* DLLCALL		strListCreateBlock(str_list_t);
DLLEXPORT char* DLLCALL		strListCopyBlock(char* block);
DLLEXPORT char* DLLCALL		strListAppendBlock(char* block, str_list_t);
DLLEXPORT size_t DLLCALL	strListBlockLength(char* block);
DLLEXPORT void DLLCALL		strListFreeBlock(char*);

/* Duplicates a list */
DLLEXPORT str_list_t DLLCALL	strListDup(str_list_t list);

/* Compares two lists */
DLLEXPORT int DLLCALL			strListCmp(str_list_t list1, str_list_t list2);

/************/
/* File I/O */
/************/

/* Read lines from file appending each line to string list */
/* Pass NULL list to have list allocated for you */
DLLEXPORT str_list_t DLLCALL	strListReadFile(FILE*, str_list_t*, size_t max_line_len);
DLLEXPORT size_t DLLCALL		strListInsertFile(FILE*, str_list_t*, size_t index, size_t max_line_len);

/* Write to file (fp) each string in the list, optionally separated by separator (e.g. "\n") */
DLLEXPORT size_t DLLCALL		strListWriteFile(FILE*, const str_list_t, const char* separator);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
