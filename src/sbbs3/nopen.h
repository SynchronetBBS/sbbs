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
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _NOPEN_H
#define _NOPEN_H

#include <stdio.h>			/* FILE */
#include <fcntl.h>			/* O_RDONLY */
#include "gen_defs.h"		/* bool */

#define FNOPEN_BUF_SIZE		(2*1024)
#define LOOP_NOPEN	  100	/* Retries before file access denied			*/

#ifdef __cplusplus
extern "C" {
#endif

int		nopen(const char* str, uint access);
FILE *	fnopen(int* file, const char* str, uint access);
bool	ftouch(const char* fname);
bool	fmutex(const char* fname, const char* text, long max_age, time_t*);
int		fmutex_open(const char* fname, const char* text, long max_age, time_t*, bool atomic_remove);
bool	fmutex_close(const char* fname, int file);
bool	fcompare(const char* fn1, const char* fn2);
bool	backup(const char* org, int backup_level, bool ren);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
