/* filewrap.h */

/* File system-call wrappers */

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

#ifndef _FILEWRAP_H
#define _FILEWRAP_H

#include "wrapdll.h"	/* DLLEXPORT and DLLCALL */

#include <sys/stat.h>	/* S_IREAD and S_IWRITE (for use with sopen) */

#if defined(__unix__)
	#include <unistd.h>	/* read, write, close, ftruncate, lseek, etc. */
#endif

#if defined(_WIN32) || defined(__BORLANDC__)
	#include <io.h>
#endif

/**********/
/* Macros */
/**********/

#if defined(_WIN32)

	#include <fcntl.h>			/* O_BINARY */
	#include <windows.h>		/* OF_SHARE_ */
	#include <share.h>			/* SH_DENY */

	#ifndef SH_DENYNO
	#define SH_DENYNO			OF_SHARE_DENY_NONE
	#define SH_DENYWR			OF_SHARE_DENY_WRITE
	#define SH_DENYRW			OF_SHARE_EXCLUSIVE
	#endif

#elif defined(__unix__)

	#include <fcntl.h>

	#ifdef __QNX__
		#include <share.h>
		#define L_SET	SEEK_SET
	#else
		#define O_TEXT		0		/* all files in binary mode on Unix */
		#define O_BINARY	0		/* all files in binary mode on Unix */
		#undef	O_DENYNONE
		#define O_DENYNONE  (1<<31)	/* req'd for Baja/nopen compatibility */

		#define SH_DENYNO	2          // no locks
		#ifdef F_SANEWRLCKNO
			#define SH_DENYRW	F_SANEWRLCKNO	   // exclusive lock
		#else
			#define SH_DENYRW	F_WRLCK	   // exclusive lock
		#endif
	
		#ifdef F_SANERDLCKNO
			#define SH_DENYWR   F_SANERDLCKNO    // shareable lock
		#else
			#define SH_DENYWR   F_RDLCK    // shareable lock
		#endif
	#endif
	#define chsize(fd,size)		ftruncate(fd,size)
	#define tell(fd)			lseek(fd,0,SEEK_CUR)

#elif defined(__OS2__)

	#include <share.h>			/* SH_DENY */

#endif

/* Standard file descriptors.  */
#ifndef STDIN_FILENO
#define STDIN_FILENO    0       /* Standard input */
#define STDOUT_FILENO   1       /* Standard output */
#define STDERR_FILENO   2       /* Standard error output */
#endif

#ifndef O_DENYNONE
#define O_DENYNONE		SH_DENYNO
#endif

/**************/
/* Prototypes */
/**************/

#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(__BORLANDC__)
	DLLEXPORT int	DLLCALL	lock(int fd, long pos, long len);
	DLLEXPORT int	DLLCALL unlock(int fd, long pos, long len);
#endif

#if !defined(__BORLANDC__) && defined(__unix__)
	DLLEXPORT int	DLLCALL sopen(const char* fn, int access, int share, ...);
	DLLEXPORT long	DLLCALL filelength(int fd);
#endif

DLLEXPORT time_t	DLLCALL filetime(int fd);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
