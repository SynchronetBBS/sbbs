/* str_list.h */

/* Functions to deal with NULL-terminated string lists */

/* $Id: str_list.h,v 1.32 2020/04/24 07:02:17 rswindell Exp $ */

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
DLLEXPORT str_list_t	strListInit(void);

/* Frees the strings in the list (and the list itself) */
DLLEXPORT void			strListFree(str_list_t*);

/* Frees the strings in the list */
DLLEXPORT void			strListFreeStrings(str_list_t);

/* Adds a string to the end of a string list (see strListPush) */
/* Pass a pointer to a string list, the string to add (append) */
/* The string to add is duplicated (using strdup) and the duplicate is added to the list */
/* If you already know the index of the last string, pass it, otherwise pass STR_LIST_LAST_INDEX */
/* Returns the updated list or NULL on error */
DLLEXPORT char*			strListAppend(str_list_t*, const char* str, size_t index);

/* Append a string list onto another string list */
DLLEXPORT size_t		strListAppendList(str_list_t*, const str_list_t append_list);

/* Append a malloc'd formatted string to the end of the list */
DLLEXPORT char*			strListAppendFormat(str_list_t* list, const char* format, ...);

/* Inserts a string into the list at a specific index */
/* Pass a pointer to a string list, the string to add (insert) */
/* The string to add is duplicated (using strdup) and the duplicate is added to the list */
DLLEXPORT char*			strListInsert(str_list_t*, const char* str, size_t index);

/* Insert a string list into another string list */
DLLEXPORT size_t		strListInsertList(str_list_t*, const str_list_t append_list, size_t index);

/* Insert a malloc'd formatted string into the list */
DLLEXPORT char*			strListInsertFormat(str_list_t* list, size_t index, const char* format, ...);

/* Remove a string at a specific index */
DLLEXPORT char*			strListRemove(str_list_t*, size_t index);

/* Remove and free a string at a specific index */
DLLEXPORT BOOL			strListDelete(str_list_t*, size_t index);

/* Replace a string at a specific index */
DLLEXPORT char*			strListReplace(const str_list_t, size_t index, const char* str);

/* Return a single-string representation of the entire string list, joined with the specified separator */
DLLEXPORT char*			strListJoin(const str_list_t, char* buf, size_t buflen, const char* separator);

/* Call a modification callback function for each string in a list */
/* and replace each string with the result of the modification callback. */
/* If the modification callback function returns NULL, the string is not modified. */
/* If the modification callback function returns the same string item pointer it was passed, the string is not realloc'd. */
/* If the modification callback function needs to expand the string item (make it bigger), it must return a new valid pointer */
/* (possibly, the cbdata, a global array or a static automatic variable). Since the new pointer is not free'd here, it should */
/* not be dynamically allocated by the callback function. */
/* Returns the number of modified strings (normally, the list count unless there was a failure) */
DLLEXPORT size_t		strListModifyEach(const str_list_t list, char*(modify(size_t index, char* str, void*)), void* cbdata);

/* Swap the strings at index1 and index2 */
DLLEXPORT BOOL			strListSwap(const str_list_t, size_t index1, size_t index2);

/* Convenience macros for pushing, popping strings (LIFO stack) */
#define		strListPush(list, str)	strListAppend(list, str, STR_LIST_LAST_INDEX)
#define		strListPop(list)		strListRemove(list, STR_LIST_LAST_INDEX)

/* Add to an exiting or new string list by splitting specified string (str) */
/* into multiple strings, separated by one of the delimit characters */
DLLEXPORT str_list_t	strListSplit(str_list_t*, char* str, const char* delimit);

/* Same as above, but copies str to temporary heap buffer first */
DLLEXPORT str_list_t	strListSplitCopy(str_list_t*, const char* str, const char* delimit);

/* Merge 2 string lists (no copying of string data) */
DLLEXPORT size_t		strListMerge(str_list_t*, str_list_t append_list);

/* Create a single delimited string from the specified list */
/* If buf is NULL, the buf is malloc'd and should be freed using strListFreeBlock() */
/* Note: maxlen includes '\0' terminator */
DLLEXPORT char*			strListCombine(str_list_t, char* buf, size_t maxlen, const char* delimit);

/* Count the number of strings in the list and returns the count */
DLLEXPORT size_t		strListCount(const str_list_t);
DLLEXPORT BOOL			strListIsEmpty(const str_list_t);

/* Returns the index of the specified str (by ptr compare) or -1 if not found */
DLLEXPORT int			strListIndexOf(const str_list_t, const char* str);
/* Returns the index of the specified str (by string compare) or -1 if not found */
DLLEXPORT int			strListFind(const str_list_t, const char* str, BOOL case_sensitive);

/* Sort the strings in the string list */
DLLEXPORT void			strListSortAlpha(str_list_t);
DLLEXPORT void			strListSortAlphaReverse(str_list_t);

/* Case-sensitive sorting */
DLLEXPORT void			strListSortAlphaCase(str_list_t);
DLLEXPORT void			strListSortAlphaCaseReverse(str_list_t);

/* Create/Copy/Append/Free NULL-terminated string block */
/* (e.g. for environment variable blocks) */
DLLEXPORT char*			strListCreateBlock(str_list_t);
DLLEXPORT char*			strListCopyBlock(char* block);
DLLEXPORT char*			strListAppendBlock(char* block, str_list_t);
DLLEXPORT size_t		strListBlockLength(char* block);
DLLEXPORT void			strListFreeBlock(char*);

/* Duplicates a list */
DLLEXPORT str_list_t	strListDup(str_list_t list);

/* Compares two lists */
DLLEXPORT int			strListCmp(str_list_t list1, str_list_t list2);

/* Modifies strings in list (returns count of items in list) */
DLLEXPORT int			strListTruncateTrailingWhitespaces(str_list_t);
DLLEXPORT int			strListTruncateTrailingLineEndings(str_list_t);
/* Truncate strings in list at first occurrence of any char in 'set' */
DLLEXPORT int			strListTruncateStrings(str_list_t, const char* set);
/* Remove all occurrences of chars in set from string in list */
DLLEXPORT int			strListStripStrings(str_list_t, const char* set);
/* Remove duplicate strings from list, return the new list length */
DLLEXPORT int			strListDedupe(str_list_t*, BOOL case_sensitive);

/************/
/* File I/O */
/************/

/* Read lines from file appending each line (with '\n' char) to string list */
/* Pass NULL list to have list allocated for you */
DLLEXPORT str_list_t	strListReadFile(FILE*, str_list_t*, size_t max_line_len);
DLLEXPORT size_t		strListInsertFile(FILE*, str_list_t*, size_t index, size_t max_line_len);

/* Write to file (fp) each string in the list, optionally separated by separator (e.g. "\n") */
DLLEXPORT size_t		strListWriteFile(FILE*, const str_list_t, const char* separator);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
