/* Thread-related cross-platform development wrappers */

/* $Id: threadwrap.h,v 1.52 2019/07/24 04:15:24 rswindell Exp $ */
// vi: tabstop=4

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
	#define pthread_equal(t1,t2)		((t1)==(t2))

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

pthread_mutex_t pthread_mutex_initializer_np(BOOL recursive);

#if defined(_POSIX_THREADS)

#if defined (__FreeBSD__) || defined (__OpenBSD__)
 #include <pthread_np.h>
 #define	SetThreadName(c)	pthread_set_name_np(pthread_self(),c)
#elif defined(__GLIBC__)
 #include <features.h>
 #if (__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 12))
  #define	SetThreadName(c)	pthread_setname_np(pthread_self(),c)
 #else
  #define SetThreadName(c)
 #endif
#else
 #define SetThreadName(c)
#endif

#else

DLLEXPORT int DLLCALL pthread_mutex_init(pthread_mutex_t*, void* attr);
DLLEXPORT int DLLCALL pthread_mutex_lock(pthread_mutex_t*);
DLLEXPORT int DLLCALL pthread_mutex_trylock(pthread_mutex_t*);
DLLEXPORT int DLLCALL pthread_mutex_unlock(pthread_mutex_t*);
DLLEXPORT int DLLCALL pthread_mutex_destroy(pthread_mutex_t*);

#define SetThreadName(c)

// A structure in case we need to add an event or something...
typedef struct {
	long	state;
} pthread_once_t;

#define PTHREAD_ONCE_INIT	{0};
DLLEXPORT int DLLCALL pthread_once(pthread_once_t *oc, void (*init)(void));

#endif

#if !defined(PTHREAD_MUTEX_INITIALIZER_NP)
	#define PTHREAD_MUTEX_INITIALIZER_NP			pthread_mutex_initializer_np(/* recursive: */FALSE)
#endif
#if !defined(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
	#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP	pthread_mutex_initializer_np(/* recursive: */TRUE)
#endif

/************************************************************************/
/* Protected (thread-safe) Integers (e.g. atomic/interlocked variables) */
/************************************************************************/
/* Use of these types and functions is not as fast as your compiler or  */
/* platform-specific functions (e.g. InterlockedIncrement on Windows or */
/* atomic_add_int on FreeBSD) but they have the advantage of always		*/
/* working and being thread-safe on all platforms that support pthread	*/
/* mutexes.																*/
/************************************************************************/
typedef struct {
	int32_t				value;
	pthread_mutex_t		mutex;
} protected_int32_t;

typedef struct {
	uint32_t			value;
	pthread_mutex_t		mutex;
} protected_uint32_t;

typedef struct {
	int64_t				value;
	pthread_mutex_t		mutex;
} protected_int64_t;

typedef struct {
	uint64_t			value;
	pthread_mutex_t		mutex;
} protected_uint64_t;

/* Return 0 on success, non-zero on failure (see pthread_mutex_init): */
DLLEXPORT int DLLCALL protected_int32_init(protected_int32_t*,	int32_t value);
#define protected_uint32_init(i, val)	protected_int32_init((protected_int32_t*)i, val)
DLLEXPORT int DLLCALL protected_int64_init(protected_int64_t*,	int64_t value);
#define protected_uint64_init(i, val)	protected_int64_init((protected_int64_t*)i, val)

/* Return new value: */
DLLEXPORT int32_t DLLCALL protected_int32_adjust(protected_int32_t*, int32_t adjustment);
DLLEXPORT int32_t DLLCALL protected_int32_set(protected_int32_t*, int32_t val);
#define protected_int32_value(i)		protected_int32_adjust(&i,0)
DLLEXPORT uint32_t DLLCALL protected_uint32_adjust(protected_uint32_t*, int32_t adjustment);
DLLEXPORT uint32_t DLLCALL protected_uint32_set(protected_uint32_t*, uint32_t val);
#define protected_uint32_value(i)		protected_uint32_adjust(&i,0)
DLLEXPORT int64_t DLLCALL protected_int64_adjust(protected_int64_t*, int64_t adjustment);
DLLEXPORT int64_t DLLCALL protected_int64_set(protected_int64_t*, int64_t val);
#define protected_int64_value(i)		protected_int64_adjust(&i,0)
DLLEXPORT uint64_t DLLCALL protected_uint64_adjust(protected_uint64_t*, int64_t adjustment);
DLLEXPORT uint64_t DLLCALL protected_uint64_set(protected_uint64_t*, uint64_t adjustment);
#define protected_uint64_value(i)		protected_uint64_adjust(&i,0)

/* Return 0 on success, non-zero on failure (see pthread_mutex_destroy): */
#define protected_int32_destroy(i)	pthread_mutex_destroy(&i.mutex)
#define protected_uint32_destroy	protected_int32_destroy	
#define protected_int64_destroy		protected_int32_destroy	
#define protected_uint64_destroy	protected_int32_destroy	

#if defined(__cplusplus)
}
#endif

#include "semwrap.h"

#endif	/* Don't add anything after this line */
