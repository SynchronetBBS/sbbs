/* ini_file.h */

/* Functions to parse ini files */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "wrapdll.h"	/* DLLEXPORT and DLLCALL */

typedef struct {
	ulong		bit;
	const char*	name;
} ini_bitdesc_t;

#if defined(__cplusplus)
extern "C" {
#endif

//DLLEXPORT 
char*		DLLCALL iniReadString	(FILE* fp, const char* section, const char* key, 
											 const char* deflt);
//DLLEXPORT 
long		DLLCALL	iniReadInteger	(FILE* fp, const char* section, const char* key, 
											 long deflt);

//DLLEXPORT 
ushort		DLLCALL	iniReadShortInt	(FILE* fp, const char* section, const char* key, 
											 ushort deflt);

//DLLEXPORT 
ulong		DLLCALL	iniReadIpAddress	(FILE* fp, const char* section, const char* key, 
											 ulong deflt);

//DLLEXPORT 
double	DLLCALL	iniReadFloat	(FILE* fp, const char* section, const char* key, 
											 double deflt);
//DLLEXPORT 
BOOL		DLLCALL iniReadBool		(FILE* fp, const char* section, const char* key, 
											 BOOL deflt);
//DLLEXPORT 
ulong		DLLCALL	iniReadBitField	(FILE* fp, const char* section, const char* key, 
											 ini_bitdesc_t* bitdesc, ulong deflt);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
