/* semwrap.c */

/* Semaphore-related cross-platform development wrappers */

/* $Id: semwrap.c,v 1.15 2018/07/24 01:13:09 rswindell Exp $ */

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

#include <errno.h>
#include "semwrap.h"

#if defined(__unix__)

#include <sys/time.h>	/* timespec */
#include <stdlib.h>	/* NULL */

int DLLCALL
sem_trywait_block(sem_t *sem, unsigned long timeout)
{
	int	retval;
	struct timespec abstime;
	struct timeval currtime;
	
	gettimeofday(&currtime,NULL);
	abstime.tv_sec=currtime.tv_sec + (currtime.tv_usec/1000 + timeout)/1000;
	abstime.tv_nsec=(currtime.tv_usec*1000 + timeout*1000000)%1000000000;

	retval=sem_timedwait(sem, &abstime);
	if(retval && errno==ETIMEDOUT)
		errno=EAGAIN;
	return retval;
}

#elif defined(_WIN32)

#include <limits.h>		/* INT_MAX */

#if defined(__BORLANDC__)
	#pragma argsused
#endif
int DLLCALL sem_init(sem_t* psem, int pshared, unsigned int value)
{

	if((*(psem)=CreateSemaphore(NULL,value,INT_MAX,NULL))==NULL)
		return -1;
		
	return 0;
}

int DLLCALL sem_trywait_block(sem_t* psem, unsigned long timeout)
{
	if(WaitForSingleObject(*(psem),timeout)!=WAIT_OBJECT_0) {
		errno=EAGAIN;
		return -1;
	}

	return 0;
}

int DLLCALL sem_post(sem_t* psem)
{
	if(ReleaseSemaphore(*(psem),1,NULL)==TRUE)
		return 0;

	return -1;
}

int DLLCALL sem_getvalue(sem_t* psem, int* vp)
{
#if 0		/* This only works on 9x *sniff* */
	ReleaseSemaphore(*(psem),0,(LPLONG)vp);
	return 0;
#else
	/* Note, this should REALLY be in a critical section... */
	int	retval=0;

	if(WaitForSingleObject(*(psem),0)!=WAIT_OBJECT_0)
		*vp=0;
	else {
		if(ReleaseSemaphore(*(psem),1,(LPLONG)vp))
			(*vp)++;
		else
			retval=-1;
	}
	return(retval);
#endif
}

int DLLCALL sem_destroy(sem_t* psem)
{
	if(CloseHandle(*(psem))==TRUE)
		return 0;
	return -1;
}

#endif /* _WIN32 */
