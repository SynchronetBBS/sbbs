/* threadwrap.c */

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

#if defined(__unix__)
	#include <unistd.h>	/* _POSIX_THREADS */
	#include <stdlib.h>	/* realpath() */
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

	pthread_attr_init(&attr);     /* initialize attribute structure */

	/* set thread attributes to PTHREAD_CREATE_DETACHED which will ensure
	   that thread resources are freed on exit() */
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

#ifdef BSD
	/* Default stack size in BSD is too small for JS stuff */
	if(stack_size==0)
		stack_size=1<<17;
#endif
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
		,arglist)==0)
		return((int) thread /* thread handle */);

	return(-1);	/* error */
}
#else

#error "Need _beginthread implementation for non-POSIX thread library."

#endif


/****************************************************************************/
/* Thread-safe (reentrant) version of realpath() - required for OpenBSD		*/
/* And for NON-DEBUG FreeBSD builds (on 4.7 anyways)						*/
/****************************************************************************/
#if defined(BSD)
char* realpath_r(const char *pathname, char *resolvedname)  
{
    static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
    char *p;

    pthread_mutex_lock(&mutex);
    p=realpath(pathname,resolvedname);
    pthread_mutex_unlock(&mutex);
    return p;
}
#endif

#endif	/* __unix__ */
