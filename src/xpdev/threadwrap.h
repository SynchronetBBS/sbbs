/* threadwrap.h */

/* Thread-related cross-platform development wrappers */

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

#ifndef _THREADWRAP_H
#define _THREADWRAP_H

#include "gen_defs.h"	/* HANDLE */

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

#ifdef __unix__

	#include <pthread.h>	/* POSIX threads and mutexes */
	#include <semaphore.h>	/* POSIX semaphores */
	ulong _beginthread(void( *start_address )( void * )
		,unsigned stack_size, void *arglist);

#elif defined(_WIN32)	

	/* POSIX semaphores */
	typedef HANDLE sem_t;
	#define sem_init(psem,ps,v)			ResetEvent(*(psem))
	#define sem_wait(psem)				WaitForSingleObject(*(psem),INFINITE)
	#define sem_post(psem)				SetEvent(*(psem))
	#define sem_destroy(psem)			CloseHandle(*(psem))
	DLLEXPORT int DLLCALL sem_getvalue(sem_t*, int* val);

	/* POSIX mutexes */
	typedef HANDLE pthread_mutex_t;
	#define PTHREAD_MUTEX_INITIALIZER	CreateMutex(NULL,FALSE,NULL)
	#define pthread_mutex_init(pmtx,v)	*(pmtx)=CreateMutex(NULL,FALSE,NULL)
	#define pthread_mutex_lock(pmtx)	WaitForSingleObject(*(pmtx),INFINITE)
	#define pthread_mutex_unlock(pmtx)	ReleaseMutex(*(pmtx))
	#define	pthread_mutex_destroy(pmtx)	CloseHandle(*(pmtx))

#else

	#warning "Need semaphore wrappers."

#endif

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
