/* str_list.h */

/* Functions to deal with NULL-terminated string lists */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#if defined(__cplusplus)
extern "C" {
#endif

#define STR_LIST_LAST_INDEX	(~0)

typedef char** str_list_t;

/* Returns an allocated and terminated string list */
str_list_t	strListInit(void);

/* Frees the strings in the list (and the list itself) */
void		strListFree(str_list_t*);

/* Frees the strings in the list */
void		strListFreeStrings(str_list_t);

/* Pass a pointer to a string list, the string to add (append) */
/* Returns the updated list or NULL on error */
char*		strListAppend(str_list_t*, const char* str, size_t index);

/* Append a string list onto another string list */
size_t		strListAppendList(str_list_t*, const str_list_t append_list);

/* Inserts a string into the list at a specific index */
char*		strListInsert(str_list_t*, const char* str, size_t index);

/* Insert a string list into another string list */
size_t		strListInsertList(str_list_t*, const str_list_t append_list, size_t index);

/* Remove a string at a specific index */
char*		strListRemove(str_list_t*, size_t index);

/* Remove and free a string at a specific index */
BOOL		strListDelete(str_list_t*, size_t index);

/* Replace a string at a specific index */
char*		strListReplace(const str_list_t, size_t index, const char* str);

/* Swap the strings at index1 and index2 */
BOOL		strListSwap(const str_list_t, size_t index1, size_t index2);

/* Convenience macros for pushing, popping strings (LIFO stack) */
#define		strListPush(list, str)	strListAppend(list, str, STR_LIST_LAST_INDEX)
#define		strListPop(list)		strListRemove(list, STR_LIST_LAST_INDEX)

/* Add to an exiting or new string list by splitting specified string (str) */
/* into multiple strings, separated by one of the delimit characters */
str_list_t	strListSplit(str_list_t*, char* str, const char* delimit);

/* Same as above, but copies str to temporary heap buffer first */
str_list_t	strListSplitCopy(str_list_t*, const char* str, const char* delimit);

/* Merge 2 string lists (no copying of string data) */
size_t		strListMerge(str_list_t*, str_list_t append_list);

/* Count the number of strings in the list and returns the count */
size_t		strListCount(const str_list_t);

/* Returns the index of the specified str (by ptr compare) or -1 if not found */
int			strListIndexOf(const str_list_t, const char* str);

/* Sort the strings in the string list */
void		strListSortAlpha(str_list_t);
void		strListSortAlphaReverse(str_list_t);

/* Case-sensitive sorting */
void		strListSortAlphaCase(str_list_t);
void		strListSortAlphaCaseReverse(str_list_t);

/* Create/Copy/Append/Free NULL-terminated string block */
/* (e.g. for environment variable blocks) */
char*		strListCreateBlock(str_list_t);
char*		strListCopyBlock(char* block);
char*		strListAppendBlock(char* block, str_list_t);
size_t		strListBlockLength(char* block);
void		strListFreeBlock(char*);

/************/
/* File I/O */
/************/

/* Read lines from file appending each line to string list */
/* Pass NULL list to have list allocated for you */
str_list_t	strListReadFile(FILE*, str_list_t*, size_t max_line_len);
size_t		strListInsertFile(FILE*, str_list_t*, size_t index, size_t max_line_len);

/* Write to file (fp) each string in the list, optionally separated by separator (e.g. "\n") */
size_t		strListWriteFile(FILE*, const str_list_t, const char* separator);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
