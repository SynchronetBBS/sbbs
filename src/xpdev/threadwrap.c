/* threadwrap.c */

/* Thread-related cross-platform development wrappers */

/* $Id: threadwrap.c,v 1.37 2019/02/18 04:04:11 rswindell Exp $ */

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

#if defined(__unix__)
	#include <unistd.h>	/* _POSIX_THREADS */
	#include <sys/param.h>	/* BSD */
#endif

#if defined(_WIN32) && !defined(_WIN32_WINNT)
	#define _WIN32_WINNT 0x0400	/* Needed for TryEnterCriticalSection */
#endif

#include "genwrap.h"	/* SLEEP() */
#include "threadwrap.h"	/* DLLCALL */

/****************************************************************************/
/* Wrapper for Win32 create/begin thread function							*/
/* Uses POSIX threads														*/
/****************************************************************************/
#if defined(__unix__)
#if defined(_POSIX_THREADS)
ulong DLLCALL _beginthread(void( *start_address )( void * )
		,unsigned stack_size, void *arglist)
{
	pthread_t	thread;
	pthread_attr_t attr;
	size_t		default_stack;

	(void)stack_size;

	pthread_attr_init(&attr);     /* initialize attribute structure */

	/* set thread attributes to PTHREAD_CREATE_DETACHED which will ensure
	   that thread resources are freed on exit() */
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* Default stack size in BSD is too small for JS stuff */
	/* Force to at least 256k */
#define XPDEV_MIN_THREAD_STACK_SIZE	(256*1024)
	if(stack_size==0 && pthread_attr_getstacksize(&attr, &default_stack)==0 
			&& default_stack < XPDEV_MIN_THREAD_STACK_SIZE)
		stack_size=XPDEV_MIN_THREAD_STACK_SIZE;

	if(stack_size!=0)
		pthread_attr_setstacksize(&attr, stack_size);

	if(pthread_create(&thread
#if defined(__BORLANDC__) /* a (hopefully temporary) work-around */
			,NULL
#else
			,&attr	/* default attributes */
#endif
			/* POSIX defines this arg as "void *(*start_address)" */
			,(void * (*)(void *)) start_address
			,arglist)==0) {
		pthread_attr_destroy(&attr);
		return((ulong) thread /* thread handle */);
	}

	pthread_attr_destroy(&attr);
	return(-1);	/* error */
}
#else

#error "Need _beginthread implementation for non-POSIX thread library."

#endif

#endif	/* __unix__ */

/****************************************************************************/
/* Wrappers for POSIX thread (pthread) mutexes								*/
/****************************************************************************/
pthread_mutex_t DLLCALL pthread_mutex_initializer_np(BOOL recursive)
{
	pthread_mutex_t	mutex;
#if defined(_POSIX_THREADS)
	pthread_mutexattr_t attr;
	pthread_mutexattr_init(&attr);
	if(recursive)
#if defined(__linux__) && defined(PTHREAD_MUTEX_RECURSIVE_NP) && !defined(__USE_UNIX98)
		pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE_NP);
#else
		pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE);
#endif
	pthread_mutex_init(&mutex, &attr);
#else	/* Assumes recursive (e.g. Windows) */
	(void)recursive;
	pthread_mutex_init(&mutex,NULL);
#endif
	return(mutex);
}

#if !defined(_POSIX_THREADS)

int DLLCALL pthread_once(pthread_once_t *oc, void (*init)(void))
{
	if (oc == NULL || init == NULL)
		return EINVAL;
	switch(InterlockedCompareExchange(&(oc->state), 1, 0)) {
		case 0:	// Never called
			init();
			InterlockedIncrement(&(oc->state));
			return 0;
		case 1:	// In init function
			/* We may not need to use InterlockedCompareExchange() here,
			 * but I hate marking things as volatile, and hate tight loops
			 * testing things that aren't marked volatile.
			 */
			while(InterlockedCompareExchange(&(oc->state), 1, 0) != 2)
				SLEEP(1);
			return 0;
		case 2:	// Done.
			return 0;
	}
	return EINVAL;
}

int DLLCALL pthread_mutex_init(pthread_mutex_t* mutex, void* attr)
{
	(void)attr;
#if defined(PTHREAD_MUTEX_AS_WIN32_MUTEX)
	return ((((*mutex)=CreateMutex(/* security */NULL, /* owned */FALSE, /* name */NULL))==NULL) ? -1 : 0);
#elif defined(_WIN32)	/* Win32 Critical Section */
	InitializeCriticalSection(mutex);
	return 0;	/* No error */
#elif defined(__OS2__)
	return DosCreateMutexSem(/* name */NULL, mutex, /* attr */0, /* owned */0);
#endif
}

int DLLCALL pthread_mutex_lock(pthread_mutex_t* mutex)
{
#if defined(PTHREAD_MUTEX_AS_WIN32_MUTEX)
	return (WaitForSingleObject(*mutex, INFINITE)==WAIT_OBJECT_0 ? 0 : EBUSY);
#elif defined(_WIN32)	/* Win32 Critical Section */
	EnterCriticalSection(mutex);
	return 0;	/* No error */
#elif defined(__OS2__)
	return DosRequestMutexSem(*mutex, -1 /* SEM_INDEFINITE_WAIT */);
#endif
}

int DLLCALL pthread_mutex_trylock(pthread_mutex_t* mutex)
{
#if defined(PTHREAD_MUTEX_AS_WIN32_MUTEX)
	return (WaitForSingleObject(*mutex, 0)==WAIT_OBJECT_0 ? 0 : EBUSY);
#elif defined(_WIN32)	/* Win32 Critical Section */
	/* TryEnterCriticalSection only available on NT4+ :-( */
	return (TryEnterCriticalSection(mutex) ? 0 : EBUSY);
#elif defined(__OS2__)
	return DosRequestMutexSem(*mutex, 0 /* SEM_IMMEDIATE_RETURN */);
#endif
}

int DLLCALL pthread_mutex_unlock(pthread_mutex_t* mutex)
{
#if defined(PTHREAD_MUTEX_AS_WIN32_MUTEX)
	return (ReleaseMutex(*mutex) ? 0 : GetLastError());
#elif defined(_WIN32)	/* Win32 Critical Section */
	LeaveCriticalSection(mutex);
	return 0;	/* No error */
#elif defined(__OS2__)
	return DosReleaseMutexSem(*mutex);
#endif
}

int DLLCALL pthread_mutex_destroy(pthread_mutex_t* mutex)
{
#if defined(PTHREAD_MUTEX_AS_WIN32_MUTEX)
	return (CloseHandle(*mutex) ? 0 : GetLastError());
#elif defined(_WIN32)	/* Win32 Critical Section */
	DeleteCriticalSection(mutex);
	return 0;	/* No error */
#elif defined(__OS2__)
	return DosCloseMutexSem(*mutex);
#endif
}

#endif	/* POSIX thread mutexes */

/************************************************************************/
/* Protected (thread-safe) Integers (e.g. atomic/interlocked variables) */
/************************************************************************/

int	DLLCALL protected_int32_init(protected_int32_t* prot, int32_t value)
{
	prot->value = value;
	return pthread_mutex_init(&prot->mutex,NULL);
}

int	DLLCALL protected_int64_init(protected_int64_t* prot, int64_t value)
{
	prot->value = value;
	return pthread_mutex_init(&prot->mutex,NULL);
}

int32_t DLLCALL protected_int32_adjust(protected_int32_t* i, int32_t adjustment)
{
	int32_t	newval;
	pthread_mutex_lock(&i->mutex);
	newval = i->value += adjustment;
	pthread_mutex_unlock(&i->mutex);
	return newval;
}

uint32_t DLLCALL protected_uint32_adjust(protected_uint32_t* i, int32_t adjustment)
{
	uint32_t newval;
	pthread_mutex_lock(&i->mutex);
	newval = i->value += adjustment;
	pthread_mutex_unlock(&i->mutex);
	return newval;
}

int64_t DLLCALL protected_int64_adjust(protected_int64_t* i, int64_t adjustment)
{
	int64_t	newval;
	pthread_mutex_lock(&i->mutex);
	newval = i->value += adjustment;
	pthread_mutex_unlock(&i->mutex);
	return newval;
}

uint64_t DLLCALL protected_uint64_adjust(protected_uint64_t* i, int64_t adjustment)
{
	uint64_t newval;
	pthread_mutex_lock(&i->mutex);
	newval = i->value += adjustment;
	pthread_mutex_unlock(&i->mutex);
	return newval;
}

int32_t DLLCALL protected_int32_set(protected_int32_t* i, int32_t val)
{
	int32_t	newval;
	pthread_mutex_lock(&i->mutex);
	newval = i->value = val;
	pthread_mutex_unlock(&i->mutex);
	return newval;
}

uint32_t DLLCALL protected_uint32_set(protected_uint32_t* i, uint32_t val)
{
	uint32_t newval;
	pthread_mutex_lock(&i->mutex);
	newval = i->value = val;
	pthread_mutex_unlock(&i->mutex);
	return newval;
}

int64_t DLLCALL protected_int64_set(protected_int64_t* i, int64_t val)
{
	int64_t	newval;
	pthread_mutex_lock(&i->mutex);
	newval = i->value = val;
	pthread_mutex_unlock(&i->mutex);
	return newval;
}

uint64_t DLLCALL protected_uint64_set(protected_uint64_t* i, uint64_t val)
{
	uint64_t newval;
	pthread_mutex_lock(&i->mutex);
	newval = i->value = val;
	pthread_mutex_unlock(&i->mutex);
	return newval;
}
