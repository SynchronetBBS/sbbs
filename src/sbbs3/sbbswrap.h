/* sbbswrap.h */

/* Synchronet system-call wrappers */

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

#ifndef _SBBSWRAP_H
#define _SBBSWRAP_H

#include "gen_defs.h"	/* ulong */

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef WRAPPER_DLL
		#define DLLEXPORT	__declspec(dllexport)
	#else
		#define DLLEXPORT	__declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL __stdcall
	#else
		#define DLLCALL
	#endif
#else	/* !_WIN32 */
	#define DLLEXPORT
	#define DLLCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif


#ifdef _MSC_VER
	#include "msdirent.h"
#else
	#include <dirent.h>	/* POSIX directory functions */
#endif

/***************/
/* OS-specific */
/***************/

#ifdef __unix__

	#include <glob.h>		/* POSIX.2 directory pattern matching function */
	#define ALLFILES "*"	/* matches all files in a directory */

#else	/* glob-compatible findfirst/findnext wrapper */

	typedef struct 
	{
			size_t gl_pathc;			/* Count of paths matched so far */
			char **gl_pathv;			/* List of matched pathnames. */
			size_t gl_offs;				/* Slots to reserve in 'gl_pathv'. */
	} glob_t;

	/* Bits set in the FLAGS argument to `glob'.  */
	#define GLOB_ERR        (1 << 0)	/* Return on read errors.  */
	#define GLOB_MARK       (1 << 1)	/* Append a slash to each name.  */
	#define GLOB_NOSORT     (1 << 2)	/* Don't sort the names.  */
	#define GLOB_DOOFFS     (1 << 3)	/* Insert PGLOB->gl_offs NULLs.  */
	#define GLOB_NOCHECK    (1 << 4)	/* If nothing matches, return the pattern.  */
	#define GLOB_APPEND     (1 << 5)	/* Append to results of a previous call.  */
	#define GLOB_NOESCAPE   (1 << 6)	/* Backslashes don't quote metacharacters.  */
	#define GLOB_PERIOD     (1 << 7)	/* Leading `.' can be matched by metachars.  */
	#define GLOB_MAGCHAR    (1 << 8)	/* Set in gl_flags if any metachars seen.  */
	#define GLOB_ALTDIRFUNC (1 << 9)	/* Use gl_opendir et al functions.  */
	#define GLOB_BRACE      (1 << 10)	/* Expand "{a,b}" to "a" "b".  */
	#define GLOB_NOMAGIC    (1 << 11)	/* If no magic chars, return the pattern.  */
	#define GLOB_TILDE      (1 << 12)	/* Expand ~user and ~ to home directories. */
	#define GLOB_ONLYDIR    (1 << 13)	/* Match only directories.  */
	#define GLOB_TILDE_CHECK (1 << 14)	/* Like GLOB_TILDE but return an error
										  if the user name is not available.  */
	/* Error returns from `glob'.  */
	#define GLOB_NOSPACE    1       /* Ran out of memory.  */
	#define GLOB_ABORTED    2       /* Read error.  */
	#define GLOB_NOMATCH    3       /* No matches found.  */
	#define GLOB_NOSYS      4       /* Not implemented.  */

	DLLEXPORT int	DLLCALL	glob(const char *pattern, int flags, void* unused, glob_t*);
	DLLEXPORT void	DLLCALL globfree(glob_t*);

	#define ALLFILES "*.*"	/* matches all files in a directory */

#endif

#ifdef __unix__

	#include <pthread.h>	/* POSIX threads and mutexes */
	#include <semaphore.h>	/* POSIX semaphores */
	ulong _beginthread(void( *start_address )( void * )
		,unsigned stack_size, void *arglist);

#elif defined(_WIN32)	

	/* POIX semaphores */
	typedef HANDLE sem_t;
	#define sem_init(psem,ps,v)			ResetEvent(*(psem))
	#define sem_wait(psem)				WaitForSingleObject(*(psem),INFINITE)
	#define sem_post(psem)				SetEvent(*(psem))
	#define sem_destroy(psem)			CloseHandle(*(psem))
	int		sem_getvalue(sem_t*, int* val);

	/* POIX mutexes */
	typedef HANDLE pthread_mutex_t;
	#define PTHREAD_MUTEX_INITIALIZER	CreateMutex(NULL,FALSE,NULL)
	#define pthread_mutex_init(pmtx,v)	*(pmtx)=CreateMutex(NULL,FALSE,NULL)
	#define pthread_mutex_lock(pmtx)	WaitForSingleObject(*(pmtx),INFINITE)
	#define pthread_mutex_unlock(pmtx)	ReleaseMutex(*(pmtx))
	#define	pthread_mutex_destroy(pmtx)	CloseHandle(*(pmtx))

#else

	#warning "Need semaphore wrappers."

#endif


#if defined(_WIN32)

	#define mswait(x)			Sleep(x)
	#define sbbs_beep(freq,dur)	Beep(freq,dur)

#elif defined(__OS2__)

	#define mswait(x)			DosSleep(x)
	#define sbbs_beep(freq,dur)	DosBeep(freq,dur)

#elif defined(__unix__)

	#define mswait(x)			usleep(x*1000)
	#define _mkdir(dir)			mkdir(dir,0777)
	#define _rmdir(dir)			rmdir(dir)
	#define _fullpath(a,r,l)	realpath(r,a)
	#define tell(fd)			lseek(fd,0,SEEK_CUR)

	DLLEXPORT void	DLLCALL sbbs_beep(int freq, int dur);
	DLLEXPORT char* DLLCALL strrev(char* str);

#else	/* Unsupported OS */

	#warning "Unsupported Target: Need some macros of function prototypes here."

#endif

/*********************/
/* Compiler-specific */
/*********************/

/* Compiler Description */
#if defined(__BORLANDC__)

	#define COMPILER_DESC(str) sprintf(str,"BCC %X.%02X" \
		,__BORLANDC__>>8,__BORLANDC__&0xff);	

#elif defined(_MSC_VER)

	#define COMPILER_DESC(str) sprintf(str,"MSC %u", _MSC_VER);

/***
#elif defined(__GNUC__) && defined(__GLIBC__)

	#define COMPILER_DESC(str) sprintf(str,"GCC %u.%02u (GLIBC %u.%u)" \
		,__GNUC__,__GNUC_MINOR__,__GLIBC__,__GLIBC_MINOR__);
***/

#elif defined(__GNUC__)

	#define COMPILER_DESC(str) sprintf(str,"GCC %u.%02u" \
		,__GNUC__,__GNUC_MINOR__);

#else /* Unknown compiler */

	#define COMPILER_DESC(str) strcpy(str,"UNKNOWN COMPILER");

#endif

/**********/
/* Macros */
/**********/

/* POSIX readdir convenience macro */
#ifndef DIRENT
#define DIRENT struct dirent	
#endif

#if defined(__unix__)
#define BACKSLASH	'/'
#else /* MS-DOS based OS */
#define BACKSLASH	'\\'
#endif

/* Target Platform Description */
#if defined(_WIN32)
	#define PLATFORM_DESC	"Win32"
#elif defined(__OS2__)
	#define PLATFORM_DESC	"OS/2"
#elif defined(__MSDOS__)
	#define PLATFORM_DESC	"DOS"
#elif defined(__linux__)
	#define PLATFORM_DESC	"Linux"
#elif defined(__FreeBSD__)
	#define PLATFORM_DESC	"FreeBSD"
#elif defined(BSD)
	#define PLATFORM_DESC	"BSD"
#elif defined(__unix__)
	#define PLATFORM_DESC	"Unix"
#else
	#warning "Need to describe target platform"
	#define PLATFORM_DESC	"UNKNOWN"
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)

#define CHMOD(s,m)		_chmod(s,m)
#define PUTENV  		_putenv
#define GETCWD  		_getcwd
#define snprintf		_snprintf

#elif defined(__BORLANDC__)

#define CHMOD(s,m)		_chmod(s,1,m)
#define PUTENV  		putenv
#define GETCWD  		getcwd

#else	/* ??? */

#define CHMOD(s,m)		chmod(s,m)
#define PUTENV  		putenv
#define GETCWD  		getcwd

#endif

#if defined(__BORLANDC__)
	#define sbbs_random(x)		random(x)
#else 
	DLLEXPORT int	DLLCALL sbbs_random(int n);
#endif

#if __BORLANDC__ > 0x0410
	#define _chmod(p,f,a)		_rtl_chmod(p,f,a) 	/* _chmod obsolete in 4.x */
#endif

#if !defined(_MSC_VER) && !defined(__BORLANDC__)
	DLLEXPORT char* DLLCALL ultoa(ulong, char*, int radix);
#endif


/* General file system wrappers for all platforms and compilers */
DLLEXPORT long	DLLCALL fdate(char *filename);
DLLEXPORT BOOL	DLLCALL	isdir(char *filename);
DLLEXPORT int	DLLCALL getfattr(char* filename);
DLLEXPORT ulong	DLLCALL getfreediskspace(char* path);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
