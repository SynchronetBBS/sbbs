/* threadwrap.h */

/* Thread-related cross-platform development wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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
	#include <unistd.h>	/* _POSIX_THREADS definition on FreeBSD (at least) */

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
	#ifdef PTHREAD_MUTEX_AS_WIN32_MUTEX	/* Much slower/heavier than critical sections */

		typedef HANDLE pthread_mutex_t;

	#else	/* Implemented as Win32 Critical Sections */

		typedef CRITICAL_SECTION pthread_mutex_t;

	#endif

#elif defined(__OS2__)

	/* POSIX mutexes */
	typedef TID pthread_t;
	typedef HEV pthread_mutex_t;

#else

	#error "Need thread wrappers."

#endif

/****************************************************************************/
/* Wrappers for POSIX thread (pthread) mutexes								*/
/****************************************************************************/

pthread_mutex_t pthread_mutex_initializer(void);

#if defined(_POSIX_THREADS)

#ifdef _DEBUG
#define	SetThreadName(c)	pthread_set_name_np(pthread_self(),c)
#else
#define SetThreadName(c)
#endif

#else

int pthread_mutex_init(pthread_mutex_t*, void* attr);
int pthread_mutex_lock(pthread_mutex_t*);
int pthread_mutex_trylock(pthread_mutex_t*);
int pthread_mutex_unlock(pthread_mutex_t*);
int pthread_mutex_destroy(pthread_mutex_t*);

#define PTHREAD_MUTEX_INITIALIZER	pthread_mutex_initializer()
#define SetThreadName(c)

#endif

#if defined(__cplusplus)
}
#endif

#include "semwrap.h"

#endif	/* Don't add anything after this line */
