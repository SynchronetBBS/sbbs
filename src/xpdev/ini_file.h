/* ini_file.h */

/* Functions to parse ini files */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2003 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifndef _INI_FILE_H
#define _INI_FILE_H

#include "genwrap.h"

#define INI_MAX_VALUE_LEN	128		/* Maximum value length, includes '\0' */

typedef struct {
	ulong		bit;
	const char*	name;
} ini_bitdesc_t;

#if defined(__cplusplus)
extern "C" {
#endif

/* Read all section names and return as an allocated string list */
/* Optionally (if prefix!=NULL), returns a subset of section names */
char**		iniGetSectionList		(FILE* fp, const char* prefix);
/* Read all key names and return as an allocated string list */
char**		iniGetKeyList			(FILE* fp, const char* section);
/* Read all key and value pairs and return as a named string list */
named_string_t**
			iniGetNamedStringList	(FILE* fp, const char* section);

/* These functions read a single key of the specified type */
char*		iniGetString	(FILE* fp, const char* section, const char* key, 
							 const char* deflt, char* value);
char**		iniGetStringList(FILE* fp, const char* section, const char* key
							,const char* sep, const char* deflt);
long		iniGetInteger	(FILE* fp, const char* section, const char* key, 
							 long deflt);
ushort		iniGetShortInt	(FILE* fp, const char* section, const char* key, 
							 ushort deflt);
ulong		iniGetIpAddress(FILE* fp, const char* section, const char* key, 
							 ulong deflt);
double		iniGetFloat	(FILE* fp, const char* section, const char* key, 
							 double deflt);
BOOL		iniGetBool		(FILE* fp, const char* section, const char* key, 
							 BOOL deflt);
ulong		iniGetBitField	(FILE* fp, const char* section, const char* key, 
							 ini_bitdesc_t* bitdesc, ulong deflt);

/* Free string list returned from iniGet*List functions */
void*		iniFreeStringList(char** list);

/* Free named string list returned from iniGetNamedStringList */
void*		iniFreeNamedStringList(named_string_t** list);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
