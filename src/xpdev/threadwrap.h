/* threadwrap.h */

/* Thread-related cross-platform development wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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

	#include <pthread.h>	/* POSIX threads and mutexes */
	#include <semaphore.h>	/* POSIX semaphores */
	ulong _beginthread(void( *start_address )( void * )
		,unsigned stack_size, void *arglist);

	#if defined(__OpenBSD__)	/* thread-safe version of realpath for OpenBSD */
		char* realpath_r(const char *pathname, char *resolvedname);
	#endif

#elif defined(_WIN32)	

	#include <process.h>	/* _beginthread */
	#include <limits.h>		/* INT_MAX */

	/* POSIX semaphores */
	typedef HANDLE sem_t;
	#define sem_init(psem,ps,v)			*(psem)=CreateSemaphore(NULL,v,INT_MAX,NULL)
	#define sem_wait(psem)				WaitForSingleObject(*(psem),INFINITE)
	#define sem_post(psem)				ReleaseSemaphore(*(psem),1,NULL)
	#define sem_destroy(psem)			CloseHandle(*(psem))
	/* No Win32 implementation for sem_getvalue() */

	/* POSIX mutexes */
#if 1	/* Implemented as Win32 Critical Sections */
	typedef CRITICAL_SECTION pthread_mutex_t;
	#define pthread_mutex_init(pmtx,v)	InitializeCriticalSection(pmtx)
	#define pthread_mutex_lock(pmtx)	EnterCriticalSection(pmtx)
	#define pthread_mutex_unlock(pmtx)	LeaveCriticalSection(pmtx)
	#define	pthread_mutex_destroy(pmtx)	DeleteCriticalSection(pmtx)
#else	/* Implemented as Win32 Mutexes (much slower) */
	typedef HANDLE pthread_mutex_t;
	#define PTHREAD_MUTEX_INITIALIZER	CreateMutex(NULL,FALSE,NULL)
	#define pthread_mutex_init(pmtx,v)	*(pmtx)=CreateMutex(NULL,FALSE,NULL)
	#define pthread_mutex_lock(pmtx)	WaitForSingleObject(*(pmtx),INFINITE)
	#define pthread_mutex_unlock(pmtx)	ReleaseMutex(*(pmtx))
	#define	pthread_mutex_destroy(pmtx)	CloseHandle(*(pmtx))
#endif

#elif defined(__OS2__)	/* These have *not* been tested! */

	/* POSIX semaphores */
	typedef HEV sem_t;
	#define	sem_init(psem,ps,v)			DosCreateEventSem(NULL,psem,0,0);
	#define sem_wait(psem)				DosWaitEventSem(*(psem),-1)
	#define sem_post(psem)				DosPostEventSem(*(psem))
	#define sem_destroy(psem)			DosCloseEventSem(*(psem))

	/* POSIX mutexes */
	typedef HEV pthread_mutex_t;
	#define pthread_mutex_init(pmtx,v)	DosCreateMutexSem(NULL,pmtx,0,0)
	#define pthread_mutex_lock(pmtx)	DosRequestMutexSem(*(psem),-1)
	#define pthread_mutex_unlock(pmtx)	DosReleaseMutexSem(*(psem))
	#define	pthread_mutex_destroy(pmtx)	DosCloseMutexSem(*(psem))

#else

	#error "Need semaphore wrappers."

#endif

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
