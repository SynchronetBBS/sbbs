/* smbwrap.h */

/* Synchronet SMBLIB system-call wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifndef _SMBWRAP_H
#define _SMBWRAP_H

#include "smblib.h"	/* SMBEXPORT/SMBCALL */

/**********/
/* Macros */
/**********/

#if defined(_WIN32)

	#include <io.h>				/* _sopen */
	#include <fcntl.h>			/* O_BINARY */
	#include <windows.h>		/* OF_SHARE_ */

	#define sopen(f,o,s)		_sopen(f,o,s,S_IREAD|S_IWRITE)
	#define close(f)			_close(f)
							
	#ifndef SH_DENYNO
	#define SH_DENYNO			OF_SHARE_DENY_NONE
	#define SH_DENYWR			OF_SHARE_DENY_WRITE
	#define SH_DENYRW			OF_SHARE_EXCLUSIVE
	#endif
	#ifndef O_DENYNONE
	#define O_DENYNONE			SH_DENYNO
	#endif

#elif defined(__unix__)

	#include <fcntl.h>

	#define O_BINARY	0		/* all files in binary mode on Unix */
	#define O_DENYNONE  (1<<31)	/* req'd for Baja/nopen compatibility */

	#define SH_DENYNO	2          // sopen() will *not* block
	#define SH_DENYRW	F_WRLCK	   // blocks on read/write
	#define SH_DENYRD   F_RDLCK	   // blocks on read
	#define SH_DENYWR   F_WRLCK    // blocks on write (and read)

	#define stricmp(x,y)		strcasecmp(x,y)
	#define strnicmp(x,y,z)		strncasecmp(x,y,z)
	#define chsize(fd,size)		ftruncate(fd,size)

#endif

#ifndef BOOL
	#define BOOL	int
#endif
#ifndef TRUE
	#define TRUE	1
	#define FALSE	0
#endif

/**************/
/* Prototypes */
/**************/

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__BORLANDC__)
	SMBEXPORT int	SMBCALL	lock(int fd, long pos, int len);
	SMBEXPORT int	SMBCALL unlock(int fd, long pos, int len);
#endif

#if defined(__unix__)
	SMBEXPORT int	SMBCALL sopen(char *fn, int access, int share);
	SMBEXPORT long	SMBCALL filelength(int fd);
	SMBEXPORT char*	SMBCALL strupr(char* str);
	SMBEXPORT char*	SMBCALL strlwr(char* str);
#endif

SMBEXPORT BOOL	SMBCALL fexist(char *filespec);
SMBEXPORT long	SMBCALL flength(char *filename);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
