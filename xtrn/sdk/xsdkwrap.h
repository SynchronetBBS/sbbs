/* xsdkwrap.h */

/* Synchronet XSDK system-call wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
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
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout xtrn	*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _XSDKWRAP_H
#define _XSDKWRAP_H

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
#elif defined(__OpenBSD__)
	#define PLATFORM_DESC	"OpenBSD"
#elif defined(BSD)
	#define PLATFORM_DESC	"BSD"
#elif defined(__unix__)
	#define PLATFORM_DESC	"Unix"
#else
	#warning "Need to describe target platform"
	#define PLATFORM_DESC	"UNKNOWN"
#endif

#if defined(__unix__)

	int kbhit(void);
	int getch(void);
	#define ungetch(x)	/* !need a wrapper for this */

#else	/* DOS-Based */

	#include <conio.h>

#endif

#if defined(_WIN32)

	#include <io.h>				/* _sopen */
	#include <sys/stat.h>		/* S_IREAD */
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

	#define SH_DENYNO	2          // no locks
	#define SH_DENYRW	F_WRLCK	   // exclusive lock
	#define SH_DENYWR   F_RDLCK    // shareable lock

	#define stricmp(x,y)		strcasecmp(x,y)
	#define strnicmp(x,y,z)		strncasecmp(x,y,z)
	#define chsize(fd,size)		ftruncate(fd,size)

#endif

#ifdef __unix__

	#include <pthread.h>	/* POSIX threads and mutexes */
	#include <semaphore.h>	/* POSIX semaphores */
	unsigned long _beginthread(void( *start_address )( void * )
		,unsigned stack_size, void *arglist);

#elif defined(_WIN32)	

	#include <process.h>	/* _beginthread() */

	/* POSIX semaphores */
	typedef HANDLE sem_t;
	#define sem_init(psem,ps,v)			ResetEvent(*(psem))
	#define sem_wait(psem)				WaitForSingleObject(*(psem),INFINITE)
	#define sem_post(psem)				SetEvent(*(psem))
	#define sem_destroy(psem)			CloseHandle(*(psem))

	/* POIX mutexes */
	typedef HANDLE pthread_mutex_t;
	#define pthread_mutex_init(pmtx,v)	*(pmtx)=CreateMutex(NULL,FALSE,NULL)
	#define pthread_mutex_lock(pmtx)	WaitForSingleObject(*(pmtx),INFINITE)
	#define pthread_mutex_unlock(pmtx)	ReleaseMutex(*(pmtx))
	#define	pthread_mutex_destroy(pmtx)	CloseHandle(*(pmtx))


#elif defined(__MSDOS__)

	/* No semaphores */

#else

	#warning "Need semaphore wrappers."

#endif


#if defined(_WIN32)

	#define mswait(x)			Sleep(x)

#elif defined(__OS2__)

	#define mswait(x)			DosSleep(x)

#elif defined(__unix__)

	#define mswait(x)			usleep(x*1000)
	#define _mkdir(dir)			mkdir(dir,0777)
	#define _rmdir(dir)			rmdir(dir)
	#define tell(fd)			lseek(fd,0,SEEK_CUR)

	int		sopen(char *fn, int access, int share);
	long	filelength(int fd);
	char*	strupr(char* str);
	char*	strlwr(char* str);

	char*	strrev(char* str);
	char*	_fullpath(char* absPath, const char* relPath
								,size_t maxLength);

#elif defined(__MSDOS__)

	void mswait(int ms);	/* Wait a specific number of milliseconds */

#else	/* Unsupported OS */

	#warning "Unsupported Target: Need some macros of function prototypes here."

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
	int	lock(int fd, long pos, int len);
	int	unlock(int fd, long pos, int len);
#endif

#if !defined(_MSC_VER) && !defined(__BORLANDC__)
	char*	ultoa(unsigned long val, char* str, int radix);
#endif

BOOL	fexist(char *filespec);
long	flength(char *filename);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
