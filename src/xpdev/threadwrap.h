/* threadwrap.h */

/* Thread-related cross-platform development wrappers */

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

#ifndef _THREADWRAP_H
#define _THREADWRAP_H

#include "gen_defs.h"	/* HANDLE */
#include "wrapdll.h"	/* DLLEXPORT and DLLCALL */

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__unix__)

	#include <sys/param.h>
	#include <pthread.h>	/* POSIX threads and mutexes */

	/* Win32 thread API wrappers */
	ulong _beginthread(void( *start_address )( void * )
			,unsigned stack_size, void *arglist);

	#define GetCurrentThreadId()		pthread_self()

#elif defined(_WIN32)	

	#include <process.h>	/* _beginthread */
	#include <limits.h>		/* INT_MAX */
	#include <errno.h>		/* EAGAIN and EBUSY */

	/* POSIX threads */
	typedef DWORD pthread_t;
	#define pthread_self()				GetCurrentThreadId()

	/* POSIX mutexes */
#if 1	/* Implemented as Win32 Critical Sections */
	typedef CRITICAL_SECTION pthread_mutex_t;
	#define pthread_mutex_init(pmtx,v)	InitializeCriticalSection(pmtx),0
	#define pthread_mutex_lock(pmtx)	EnterCriticalSection(pmtx),0
	#define pthread_mutex_unlock(pmtx)	LeaveCriticalSection(pmtx),0
	#define	pthread_mutex_destroy(pmtx)	DeleteCriticalSection(pmtx),0
	/* TryEnterCriticalSection only available on NT4+ :-( */
	#define pthread_mutex_trylock(pmtx) (TryEnterCriticalSection(pmtx)?0:EBUSY)

#else	/* Implemented as Win32 Mutexes (much slower) */
	typedef HANDLE pthread_mutex_t;
	#define PTHREAD_MUTEX_INITIALIZER	CreateMutex(NULL,FALSE,NULL)
	#define pthread_mutex_init(pmtx,v)	(*(pmtx)=CreateMutex(NULL,FALSE,NULL))==NULL?-1:0)
	#define pthread_mutex_lock(pmtx)	(WaitForSingleObject(*(pmtx),INFINITE)==WAIT_OBJECT_0?0:EBUSY)
	#define pthread_mutex_unlock(pmtx)	(ReleaseMutex(*(pmtx))?0:GetLastError())
	#define	pthread_mutex_destroy(pmtx)	(CloseHandle(*(pmtx))?0:GetLastError())
	#define pthread_mutex_trylock(pmtx) (WaitForSingleObject(*(pmtx),0)==WAIT_OBJECT_0?0:EBUSY)
#endif

#elif defined(__OS2__)	/* These have *not* been tested! */

	/* POSIX mutexes */
	typedef HEV pthread_mutex_t;
	#define pthread_mutex_init(pmtx,v)	DosCreateMutexSem(NULL,pmtx,0,0)
	#define pthread_mutex_lock(pmtx)	DosRequestMutexSem(*(psem),-1)
	#define pthread_mutex_unlock(pmtx)	DosReleaseMutexSem(*(psem))
	#define	pthread_mutex_destroy(pmtx)	DosCloseMutexSem(*(psem))

#else

	#error "Need thread wrappers."

#endif

#if defined(__cplusplus)
}
#endif

#include "semwrap.h"

#endif	/* Don't add anything after this line */
