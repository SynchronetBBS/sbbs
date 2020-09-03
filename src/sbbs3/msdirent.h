/* msdirent.h */

/* POSIX directory operations using Microsoft _findfirst/next functions. */

/* $Id: msdirent.h,v 1.4 2018/07/24 01:11:07 rswindell Exp $ */

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

#ifndef __MSDIRENT_H
#define __MSDIRENT_H

#include <io.h>			/* Microsoft _findfirst stuff */
#include <windows.h>	/* BOOL definition */

#ifdef __cplusplus
extern "C" {
#endif


/* dirent structure returned by readdir().
 */
struct dirent
{	
    char        d_name[260];	/* filename */
};

/* DIR type returned by opendir().  The members of this structure
 * must not be accessed by application programs.
 */
typedef struct
{
	char				filespec[260];
	struct dirent		dirent;
    long				handle;
	struct _finddata_t	finddata;
	BOOL				end;		/* End of directory flag */
} DIR;


/* Prototypes.
 */
DIR            *	opendir  (const char *__dirname);
struct dirent  *	readdir  (DIR *__dir);
int                 closedir (DIR *__dir);
void                rewinddir(DIR *__dir);

#ifdef __cplusplus
}
#endif

#endif /* Don't add anything after this line */
