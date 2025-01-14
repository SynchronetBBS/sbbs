/* POSIX directory operations using Microsoft _findfirst/next functions. */

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

#ifndef __MSDIRENT_H
#define __MSDIRENT_H

#include <io.h>         /* Microsoft _findfirst stuff */
#include <windows.h>    /* BOOL definition */

#ifdef __cplusplus
extern "C" {
#endif


/* dirent structure returned by readdir().
 */
struct dirent
{
	char d_name[260];           /* filename */
};

/* DIR type returned by opendir().  The members of this structure
 * must not be accessed by application programs.
 */
typedef struct
{
	char filespec[260];
	struct dirent dirent;
	long handle;
	struct _finddata_t finddata;
	BOOL end;                       /* End of directory flag */
} DIR;


/* Prototypes.
 */
DIR            *    opendir  (const char *__dirname);
struct dirent  *    readdir  (DIR *__dir);
int                 closedir (DIR *__dir);
void                rewinddir(DIR *__dir);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
