/* Thread-related cross-platform development wrappers */

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

#ifndef _THREADWRAP_H
#define _THREADWRAP_H

#include "gen_defs.h"	/* HANDLE */
#include "wrapdll.h"	/* DLLEXPORT and */

#if !__STDC_NO_ATOMICS__
	#if defined __GNUC__ && (__GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 9)) && !defined(__llvm__) && !defined(__INTEL_COMPILER)
		#define __STDC_NO_ATOMICS__ 1
	#elif defined __BORLANDC__ || defined _MSC_VER
		#define __STDC_NO_ATOMICS__ 1
	#endif
#endif
#if !__STDC_NO_ATOMICS__
#include <stdbool.h>
#ifdef __cplusplus
#include <atomic>
#else
#include <stdatomic.h>
#endif
#endif

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
	typedef uintptr_t pthread_t;
	#define pthread_self()				GetCurrentThreadId()
	#define pthread_equal(t1,t2)		((t1)==(t2))

	/* POSIX mutexes */
	#ifdef PTHREAD_MUTEX_AS_WIN32_MUTEX	/* Much slower/heavier than critical sections */

		typedef HANDLE pthread_mutex_t;

	#else	/* Implemented as Win32 Critical Sections */

		typedef intptr_t pthread_mutex_t;

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

DLLEXPORT int pthread_mutex_init(pthread_mutex_t*, void* attr);
DLLEXPORT int pthread_mutex_lock(pthread_mutex_t*);
DLLEXPORT int pthread_mutex_trylock(pthread_mutex_t*);
DLLEXPORT int pthread_mutex_unlock(pthread_mutex_t*);
DLLEXPORT int pthread_mutex_destroy(pthread_mutex_t*);

#define SetThreadName(c)

// A structure in case we need to add an event or something...
typedef struct {
	long	state;
} pthread_once_t;

#define PTHREAD_ONCE_INIT	{0};
DLLEXPORT int pthread_once(pthread_once_t *oc, void (*init)(void));

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
#if !__STDC_NO_ATOMICS__
#ifdef __cplusplus
typedef std::atomic<int32_t> protected_int32_t;
typedef std::atomic<uint32_t> protected_uint32_t;
typedef std::atomic<int64_t> protected_int64_t;
typedef std::atomic<uint64_t> protected_uint64_t;
#define protected_int32_init(pval, val) std::atomic_store<int32_t>(pval, val)
#define protected_uint32_init(pval, val) std::atomic_store<uint32_t>(pval, val)
#define protected_int64_init(pval, val) std::atomic_store<int64_t>(pval, val)
#define protected_uint64_init(pval, val) std::atomic_store<uint64_t>(pval, val)

#define protected_int32_set(pval, val) std::atomic_store<int32_t>(pval, val)
#define protected_uint32_set(pval, val) std::atomic_store<uint32_t>(pval, val)
#define protected_int64_set(pval, val) std::atomic_store<int64_t>(pval, val)
#define protected_uint64_(pval, val) std::atomic_store<uint64_t>(pval, val)

#define protected_int32_adjust(pval, adj) std::atomic_fetch_add<int32_t>(pval, adj)
#define protected_uint32_adjust(pval, adj) std::atomic_fetch_add<uint32_t>(pval, adj)
#define protected_int64_adjust(pval, adj) std::atomic_fetch_add<int64_t>(pval, adj)
#define protected_uint64_adjust(pval, adj) std::atomic_fetch_add<uint64_t>(pval, adj)

#define protected_int32_adjust_fetch(pval, adj) (std::atomic_fetch_add<int32_t>(pval, adj) + adj)
#define protected_uint32_adjust_fetch(pval, adj) (std::atomic_fetch_add<uint32_t>(pval, adj) + adj)
#define protected_int64_adjust_fetch(pval, adj) (std::atomic_fetch_add<int64_t>(pval, adj) + adj)
#define protected_uint64_adjust_fetch(pval, adj) (std::atomic_fetch_add<uint64_t>(pval, adj) + adj)

#define protected_int32_value(val) std::atomic_load<int32_t>(&val)
#define protected_uint32_value(val) std::atomic_load<uint32_t>(&val)
#define protected_int64_value(val) std::atomic_load<int64_t>(&val)
#define protected_uint64_value(val) std::atomic_load<uint64_t>(&val)
#else
typedef _Atomic(int32_t) protected_int32_t;
typedef _Atomic(uint32_t) protected_uint32_t;
typedef _Atomic(int64_t) protected_int64_t;
typedef _Atomic(uint64_t) protected_uint64_t;

#define protected_int32_init(pval, val) atomic_init(pval, val)
#define protected_uint32_init(pval, val) atomic_init(pval, val)
#define protected_int64_init(pval, val) atomic_init(pval, val)
#define protected_uint64_init(pval, val) atomic_init(pval, val)

#define protected_int32_set(pval, val) atomic_init(pval, val)
#define protected_uint32_set(pval, val) atomic_init(pval, val)
#define protected_int64_set(pval, val) atomic_init(pval, val)
#define protected_uint64_set(pval, val) atomic_init(pval, val)

#define protected_int32_adjust(pval, adj) atomic_fetch_add(pval, adj)
#define protected_uint32_adjust(pval, adj) atomic_fetch_add(pval, adj)
#define protected_int64_adjust(pval, adj) atomic_fetch_add(pval, adj)
#define protected_uint64_adjust(pval, adj) atomic_fetch_add(pval, adj)

#define protected_int32_adjust_fetch(pval, adj) (atomic_fetch_add(pval, adj) + adj)
#define protected_uint32_adjust_fetch(pval, adj) (atomic_fetch_add(pval, adj) + adj)
#define protected_int64_adjust_fetch(pval, adj) (atomic_fetch_add(pval, adj) + adj)
#define protected_uint64_adjust_fetch(pval, adj) (atomic_fetch_add(pval, adj) + adj)

#define protected_int32_value(val) atomic_load(&val)
#define protected_uint32_value(val) atomic_load(&val)
#define protected_int64_value(val) atomic_load(&val)
#define protected_uint64_value(val) atomic_load(&val)
#endif

#define protected_int32_destroy(i)
#define protected_uint32_destroy(i)
#define protected_int64_destroy(i)
#define protected_uint64_destroy(i)
#else
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

#define protected_uint32_init(i, val)	protected_int32_init((protected_int32_t*)i, val)
#define protected_uint64_init(i, val)	protected_int64_init((protected_int64_t*)i, val)
/* Return 0 on success, non-zero on failure (see pthread_mutex_destroy): */
#define protected_int32_destroy(i)	pthread_mutex_destroy(&(i).mutex)
#define protected_uint32_destroy	protected_int32_destroy	
#define protected_int64_destroy		protected_int32_destroy	
#define protected_uint64_destroy	protected_int32_destroy	
#define protected_int32_value(i)		protected_int32_adjust(&(i),0)
#define protected_uint32_value(i)		protected_uint32_adjust(&(i),0)
#define protected_int64_value(i)		protected_int64_adjust(&(i),0)
#define protected_uint64_value(i)		protected_uint64_adjust(&(i),0)

#define protected_int32_adjust_fetch(a, b)	protected_int32_adjust(a, b)
#define protected_uint32_adjust_fetch(a, b)	protected_uint32_adjust(a, b)
#define protected_int64_adjust_fetch(a, b)	protected_int64_adjust(a, b)
#define protected_uint64_adjust_fetch(a, b)	protected_uint64_adjust(a, b)

/* Return 0 on success, non-zero on failure (see pthread_mutex_init): */
DLLEXPORT void protected_int32_init(protected_int32_t*,	int32_t value);
DLLEXPORT void protected_int64_init(protected_int64_t*,	int64_t value);

/* Return new value: */
DLLEXPORT int32_t protected_int32_adjust(protected_int32_t*, int32_t adjustment);
DLLEXPORT int32_t protected_int32_set(protected_int32_t*, int32_t val);
DLLEXPORT uint32_t protected_uint32_adjust(protected_uint32_t*, int32_t adjustment);
DLLEXPORT uint32_t protected_uint32_set(protected_uint32_t*, uint32_t val);
DLLEXPORT int64_t protected_int64_adjust(protected_int64_t*, int64_t adjustment);
DLLEXPORT int64_t protected_int64_set(protected_int64_t*, int64_t val);
DLLEXPORT uint64_t protected_uint64_adjust(protected_uint64_t*, int64_t adjustment);
DLLEXPORT uint64_t protected_uint64_set(protected_uint64_t*, uint64_t adjustment);

#endif

#if defined(__cplusplus)
}
#endif

#include "semwrap.h"

#endif	/* Don't add anything after this line */
