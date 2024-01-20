/* Functions to deal with comma (CSV) and tab-delimited files and lists */

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

#ifndef _DAT_FILE_H
#define _DAT_FILE_H

#include "str_list.h"

#if defined(__cplusplus)
extern "C" {
#endif

/***************/
/* Generic API */
/***************/

typedef str_list_t	(*dataLineParser_t)(const char*);
typedef char*		(*dataLineCreator_t)(const str_list_t);

/* columns arguments are optional (may be NULL) */
str_list_t*	dataParseList(const str_list_t records, str_list_t* columns, dataLineParser_t);
str_list_t*	dataReadFile(FILE* fp, str_list_t* columns, dataLineParser_t);

str_list_t	dataCreateList(const str_list_t records[], const str_list_t columns, dataLineCreator_t);
BOOL		dataWriteFile(FILE* fp, const str_list_t records[], const str_list_t columns
						  ,const char* separator, dataLineCreator_t);
FILE*		dataOpenFile(const char* path, const char* mode);
int			dataCloseFile(FILE*);
BOOL		dataListFree(str_list_t*);

/* CSV (comma separated value) API */
char*		csvLineCreator(const str_list_t);
str_list_t	csvLineParser(const char* line);
#define		csvParseList(list,col)			dataParseList(list,col,csvLineParser)
#define		csvCreateList(rec,col)			dataCreateList(rec,col,csvLineCreator)
#define		csvReadFile(fp,col)				dataReadFile(fp,col,csvLineParser)
#define		csvWriteFile(fp,rec,sep,col)	dataWriteFile(fp,rec,col,sep,csvLineCreator)
#define		cvsOpenFile(path, mode)			dataOpenFile(path, mode)
#define		csvCloseFile(fp)				dataCloseFile(fp)
#define		cvsListFree(list)				dataListFree(list)

/* Tab-delimited API */
char*		tabLineCreator(const str_list_t);
str_list_t	tabLineParser(const char* line);
#define		tabParseList(list,col)			dataParseList(list,col,tabLineParser)
#define		tabCreateList(rec,col)			dataCreateList(rec,col,tabLineCreator)
#define		tabReadFile(fp,col)				dataReadFile(fp,col,tabLineParser)
#define		tabWriteFile(fp,rec,sep,col)	dataWriteFile(fp,rec,col,sep,tabLineCreator)
#define		tabOpenFile(path, mode)			dataOpenFile(path, mode)
#define		tabCloseFile(fp)				dataCloseFile(fp)
#define		tabListFree(list)				dataListFree(list)

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
