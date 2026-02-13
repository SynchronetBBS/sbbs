/* File system-call wrappers */

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

#ifndef _FILEWRAP_H
#define _FILEWRAP_H

#include "wrapdll.h"	/* DLLEXPORT */
#include "gen_defs.h"	/* int32_t, int64_t */

#include <sys/stat.h>	/* S_IREAD and S_IWRITE (for use with sopen) */
#include <stdio.h>

#if defined(__unix__)
	#include <unistd.h>	/* read, write, close, ftruncate, lseek, etc. */
#endif

#if defined(_WIN32) || defined(__BORLANDC__)
	#include <io.h>
#endif

#include <fcntl.h>		/* O_RDONLY, O_CREAT, etc. */

/**********/
/* Macros */
/**********/

#if defined(_WIN32)

	
	#include <windows.h>		/* OF_SHARE_ */
	#include <share.h>			/* SH_DENY */

	#ifndef SH_DENYNO
	#define SH_DENYNO			OF_SHARE_DENY_NONE
	#define SH_DENYWR			OF_SHARE_DENY_WRITE
	#define SH_DENYRW			OF_SHARE_EXCLUSIVE
	#endif

	#ifndef SH_COMPAT
	#define SH_COMPAT			0
	#endif

	#if defined(_MSC_VER)
		#define chsize(fd,size)		_chsize_s(fd,size)
	#endif

	#if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS==64)
		#define	lseek			_lseeki64
		#define	tell			_telli64
		#define filelength		_filelengthi64
		#define	stat			_stati64
		#define	fstat			_fstati64
		#define fseeko			_fseeki64
		#define ftello			_ftelli64
	#else
		#define fseeko			fseek
		#define ftello			ftell
	#endif

#ifndef DISABLE_MKSTEMP_DEFINE
	#define mkstemp(t)	_open(_mktemp(t), O_RDWR | O_CREAT | O_EXCL)
#endif
#if !defined(__BORLANDC__)
	typedef short unsigned int mode_t;
#endif

#elif defined(__unix__)

	#ifdef __solaris__
		#define LOCK_NB	1
		#define LOCK_SH 2
		#define LOCK_EX 4
	#endif

	#ifdef __QNX__
		#include <share.h>
		#define L_SET	SEEK_SET
	#else
		#ifndef O_TEXT
		#define O_TEXT		0		/* all files in binary mode on Unix */
		#define O_BINARY	0		/* all files in binary mode on Unix */
		#endif
		#undef	O_DENYNONE
		#define O_DENYNONE  (1U<<31)	/* req'd for Baja/nopen compatibility */

		#define SH_DENYNO	2          /* no locks */
		#define SH_DENYRW	F_WRLCK	   /* exclusive lock */
		#define SH_DENYWR   F_RDLCK    /* shareable lock */

		#ifndef SH_COMPAT
			#define SH_COMPAT SH_DENYNO
		#endif
	#endif

	#ifndef DEFFILEMODE
	#define DELFILEMODE			(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
	#endif

	#define chsize(fd,size)		ftruncate(fd,size)
	#define tell(fd)			lseek(fd,0,SEEK_CUR)
	#ifndef __cplusplus	// Conflict with FreeBSD /usr/include/c++/v1/iterator
		#define eof(fd)			(tell(fd)==filelength(fd))
	#endif

#elif defined(__OS2__)

	#include <share.h>			/* SH_DENY */

#endif

#ifndef DEFFILEMODE
#define DEFFILEMODE			(S_IREAD|S_IWRITE)
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

#define CLOSE_OPEN_FILE(x)	do { if((x) >= 0)    { close(x);  (x) = -1;   } } while(0)
#define FCLOSE_OPEN_FILE(x)	do { if((x) != NULL) { fclose(x); (x) = NULL; } } while(0)

// These errno values should trigger sopen() and lock() retries, within limits
#define FILE_RETRY_ERRNO(x) ((x)==EACCES || (x)==EAGAIN || (x)==EDEADLOCK || (x)==EBUSY || (x)==EIO)
#define FILE_RETRY_DELAY(x, y) SLEEP((((x) / 10) * ((y) / 2)) + xp_random((y) + 1))

/**************/
/* Prototypes */
/**************/

#if defined(__cplusplus)
extern "C" {
#endif

DLLEXPORT int	xp_lockfile(int fd, off_t pos, off_t len, bool block);
#if !defined(__BORLANDC__) && !defined(__WATCOMC__)
	DLLEXPORT int	lock(int fd, off_t pos, off_t len);
	DLLEXPORT int	unlock(int fd, off_t pos, off_t len);
#endif

#if defined(_WIN32 )
	DLLEXPORT ssize_t	getdelim(char **linep, size_t *linecapp, int delimiter, FILE *stream);
#endif

#if !defined(__BORLANDC__) && defined(__unix__)
	DLLEXPORT int		sopen(const char* fn, int sh_access, int share, ...);
	DLLEXPORT off_t		filelength(int fd);
#endif

#if defined(__unix__)
	DLLEXPORT FILE * _fsopen(const char *pszFilename, const char *pszMode, int shmode);
#endif

#if defined(_MSC_VER) && (_MSC_VER < 1300)	/* missing prototypes */
	DLLEXPORT int		_fseeki64(FILE*, int64_t, int origin);
	DLLEXPORT int64_t	_ftelli64(FILE*);
#endif

DLLEXPORT time_t	filetime(int fd);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
