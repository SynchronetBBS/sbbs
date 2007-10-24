/* threadwrap.c */

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

#if defined(__unix__)
	#include <unistd.h>	/* _POSIX_THREADS */
	#include <sys/param.h>	/* BSD */
#endif

#if defined(_WIN32) && !defined(_WIN32_WINNT)
	#define _WIN32_WINNT 0x0400	/* Needed for TryEnterCriticalSection */
#endif

#include "threadwrap.h"	/* DLLCALL */

/****************************************************************************/
/* Wrapper for Win32 create/begin thread function							*/
/* Uses POSIX threads														*/
/****************************************************************************/
#if defined(__unix__)
#if defined(_POSIX_THREADS)
#if defined(__BORLANDC__)
        #pragma argsused
#endif
ulong _beginthread(void( *start_address )( void * )
		,unsigned stack_size, void *arglist)
{
	pthread_t	thread;
	pthread_attr_t attr;
	size_t		default_stack;

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
	else
		pthread_attr_setstacksize(&attr, default_stack);

	if(pthread_create(&thread
#if defined(__BORLANDC__) /* a (hopefully temporary) work-around */
		,NULL
#else
		,&attr	/* default attributes */
#endif
		/* POSIX defines this arg as "void *(*start_address)" */
		,(void * (*)(void *)) start_address
		,arglist)==0)
		pthread_attr_destroy(&attr);
		return((int) thread /* thread handle */);

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
pthread_mutex_t pthread_mutex_initializer(void)
{
	pthread_mutex_t	mutex;

	pthread_mutex_init(&mutex,NULL);

	return(mutex);
}

#if !defined(_POSIX_THREADS)

#if defined(__BORLANDC__)
        #pragma argsused	/* attr arg not used */
#endif
int pthread_mutex_init(pthread_mutex_t* mutex, void* attr)
{
#if defined(PTHREAD_MUTEX_AS_WIN32_MUTEX)
	return ((((*mutex)=CreateMutex(/* security */NULL, /* owned */FALSE, /* name */NULL))==NULL) ? -1 : 0);
#elif defined(_WIN32)	/* Win32 Critical Section */
	InitializeCriticalSection(mutex);
	return 0;	/* No error */
#elif defined(__OS2__)
	return DosCreateMutexSem(/* name */NULL, mutex, /* attr */0, /* owned */0);
#endif
}

int pthread_mutex_lock(pthread_mutex_t* mutex)
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

int pthread_mutex_trylock(pthread_mutex_t* mutex)
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

int pthread_mutex_unlock(pthread_mutex_t* mutex)
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

int pthread_mutex_destroy(pthread_mutex_t* mutex)
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
