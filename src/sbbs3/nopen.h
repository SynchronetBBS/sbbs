/* $Id: nopen.h,v 1.4 2017/11/06 06:28:56 rswindell Exp $ */

/* "Network-file Open" (nopen) and friends */

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

#ifndef _NOPEN_H
#define _NOPEN_H

#include <stdio.h>			/* FILE */
#include "gen_defs.h"		/* BOOL (switch to stdbool when we stop using BCB6) */

#ifdef __cplusplus
extern "C" {
#endif

int		nopen(const char* str, int access);
FILE *	fnopen(int* file, const char* str, int access);
BOOL	ftouch(const char* fname);
BOOL	fmutex(const char* fname, const char* text, long max_age);
BOOL	fcopy(const char* src, const char* dest);
BOOL	fcompare(const char* fn1, const char* fn2);
BOOL	backup(const char* org, int backup_level, BOOL ren);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
