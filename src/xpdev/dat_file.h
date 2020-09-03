/* dat_file.h */

/* Functions to deal with comma (CSV) and tab-delimited files and lists */

/* $Id: dat_file.h,v 1.4 2018/07/24 01:13:09 rswindell Exp $ */

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

/* CSV (comma separated value) API */
char*		csvLineCreator(const str_list_t);
str_list_t	csvLineParser(const char* line);
#define		csvParseList(list,col)			dataParseList(list,col,csvLineParser)
#define		csvCreateList(rec,col)			dataCreateList(rec,col,csvLineCreator)
#define		csvReadFile(fp,col)				dataReadFile(fp,col,csvLineParser)
#define		csvWriteFile(fp,rec,sep,col)	dataWriteFile(fp,rec,col,sep,csvLineCreator)

/* Tab-delimited API */
char*		tabLineCreator(const str_list_t);
str_list_t	tabLineParser(const char* line);
#define		tabParseList(list,col)			dataParseList(list,col,tabLineParser)
#define		tabCreateList(rec,col)			dataCreateList(rec,col,tabLineCreator)
#define		tabReadFile(fp,col)				dataReadFile(fp,col,tabLineParser)
#define		tabWriteFile(fp,rec,sep,col)	dataWriteFile(fp,rec,col,sep,tabLineCreator)

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
