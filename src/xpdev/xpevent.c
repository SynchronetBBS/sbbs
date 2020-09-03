/* xpevent.c */

/* *nix emulation of Win32 *Event API */

/* $Id: xpevent.c,v 1.17 2018/07/24 01:13:10 rswindell Exp $ */

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

#include <stdio.h>		/* NULL */
#include <stdlib.h>		/* malloc() */
#include "xpevent.h"
#include "genwrap.h"

xpevent_t
CreateEvent(void *sec, BOOL bManualReset, BOOL bInitialState, void *name)
{
	xpevent_t	event;

	event = (xpevent_t)malloc(sizeof(struct xpevent));
	if (event == NULL) {
		errno = ENOSPC;
		return(NULL);
	}
	memset(event,0,sizeof(struct xpevent));

	/*
	 * Initialize
	 */
	if (pthread_mutex_init(&event->lock, NULL) != 0) {
		free(event);
		errno = ENOSPC;
		return(NULL);
	}

	if (pthread_cond_init(&event->gtzero, NULL) != 0) {
		while(pthread_mutex_destroy(&event->lock)==EBUSY)
			SLEEP(1);
		free(event);
		errno = ENOSPC;
		return(NULL);
	}

	event->mreset=bManualReset;
	event->value=bInitialState;
	event->nwaiters = 0;
	event->magic=EVENT_MAGIC;

	return(event);
}

BOOL
SetEvent(xpevent_t event)
{
	if (event==NULL || (event->magic != EVENT_MAGIC)) {
		errno = EINVAL;
		return(FALSE);
	}

	pthread_mutex_lock(&event->lock);

	event->value=TRUE;
	if (event->nwaiters > 0) {
		/*
		 * We must use pthread_cond_broadcast() rather than
		 * pthread_cond_signal() in order to assure that the highest
		 * priority thread is run by the scheduler, since
		 * pthread_cond_signal() signals waiting threads in FIFO order.
		 */
		pthread_cond_broadcast(&event->gtzero);
	}

	pthread_mutex_unlock(&event->lock);

	return(TRUE);
}

BOOL
ResetEvent(xpevent_t event)
{
	if (event==NULL || (event->magic != EVENT_MAGIC)) {
		errno = EINVAL;
		return(FALSE);
	}

	pthread_mutex_lock(&event->lock);

	event->value=FALSE;

	pthread_mutex_unlock(&event->lock);

	return(TRUE);
}

BOOL
CloseEvent(xpevent_t event)
{
	if (event==NULL || (event->magic != EVENT_MAGIC)) {
		errno = EINVAL;
		return(FALSE);
	}

	/* Make sure there are no waiters. */
	pthread_mutex_lock(&event->lock);
	if (event->nwaiters > 0) {
		pthread_mutex_unlock(&event->lock);
		errno = EBUSY;
		return(FALSE);
	}

	pthread_mutex_unlock(&event->lock);

	while(pthread_mutex_destroy(&event->lock)==EBUSY)
		SLEEP(1);
	while(pthread_cond_destroy(&event->gtzero)==EBUSY)
		SLEEP(1);
	event->magic = 0;

	free(event);

	return(TRUE);
}

DWORD
WaitForEvent(xpevent_t event, DWORD ms)
{
	DWORD	retval=WAIT_FAILED;
	struct timespec abstime;
	struct timeval currtime;

	if (event==NULL || (event->magic != EVENT_MAGIC)) {
		errno = EINVAL;
		return(WAIT_FAILED);
	}

	if(ms && ms!=INFINITE) {
		gettimeofday(&currtime,NULL);
		abstime.tv_sec=currtime.tv_sec + ((currtime.tv_usec/1000 + ms)/1000);
		abstime.tv_nsec=(currtime.tv_usec*1000 + ms*1000000)%1000000000;
	}

	pthread_mutex_lock(&event->lock);

	if(event->value)
		retval=WAIT_OBJECT_0;

	while ((!(event->value)) || (event->verify!=NULL && !event->verify(event->cbdata))) {
		event->nwaiters++;
		switch(ms) {
			case 0:
				if(event->value)
					retval=WAIT_OBJECT_0;
				else
					retval=WAIT_TIMEOUT;
				event->nwaiters--;
				goto DONE;
				break;
			case INFINITE:
				retval=pthread_cond_wait(&event->gtzero, &event->lock);
				if(retval) {
					errno=retval;
					retval=WAIT_FAILED;
					event->nwaiters--;
					goto DONE;
				}
				break;
			default:
				retval=pthread_cond_timedwait(&event->gtzero, &event->lock, &abstime);
				if(retval)  {
					if(retval==ETIMEDOUT)
						retval=WAIT_TIMEOUT;
					else {
						errno=retval;
						retval=WAIT_FAILED;
					}
					event->nwaiters--;
					goto DONE;
				}
		}
		event->nwaiters--;
	}

  DONE:

	if(retval==WAIT_OBJECT_0) {
		if(!event->mreset)
			event->value=FALSE;
	}

	pthread_mutex_unlock(&event->lock);

	return retval;
}
