/* *nix emulation of Win32 *Event API */

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

#if defined(__unix__)

#include <stdio.h>      /* NULL */
#include <stdlib.h>     /* malloc() */
#include "eventwrap.h"
#include "genwrap.h"
#include "threadwrap.h"

xpevent_t
CreateEvent(void *sec, BOOL bManualReset, BOOL bInitialState, const char *name)
{
	xpevent_t event;
	int       result;

	(void)sec;
	(void)name;

	event = (xpevent_t)malloc(sizeof(struct xpevent));
	if (event == NULL)
		return NULL;
	memset(event, 0, sizeof(struct xpevent));

	/*
	 * Initialize
	 */
	result = pthread_mutex_init(&event->lock, NULL);
	if (result != 0) {
		free(event);
		errno = result;
		return NULL;
	}

	result = pthread_cond_init(&event->gtzero, NULL);
	if (result != 0) {
		pthread_mutex_destroy(&event->lock);
		free(event);
		errno = result;
		return NULL;
	}

	event->mreset = bManualReset;
	event->value = bInitialState;
	event->nwaiters = 0;
	event->magic = EVENT_MAGIC;

	return event;
}

BOOL
SetEvent(xpevent_t event)
{
	int result;
	int unlock_result;

	if (event == NULL || (event->magic != EVENT_MAGIC)) {
		errno = EINVAL;
		return FALSE;
	}

	result = pthread_mutex_lock(&event->lock);
	if (result != 0) {
		errno = result;
		return FALSE;
	}

	event->value = TRUE;
	if (event->nwaiters > 0) {
		/*
		 * We must use pthread_cond_broadcast() rather than
		 * pthread_cond_signal() in order to assure that the highest
		 * priority thread is run by the scheduler, since
		 * pthread_cond_signal() signals waiting threads in FIFO order.
		 */
		result = pthread_cond_broadcast(&event->gtzero);
	}

	unlock_result = pthread_mutex_unlock(&event->lock);
	if (result != 0 || unlock_result != 0) {
		errno = result != 0 ? result : unlock_result;
		return FALSE;
	}

	return TRUE;
}

BOOL
ResetEvent(xpevent_t event)
{
	int result;

	if (event == NULL || (event->magic != EVENT_MAGIC)) {
		errno = EINVAL;
		return FALSE;
	}

	result = pthread_mutex_lock(&event->lock);
	if (result != 0) {
		errno = result;
		return FALSE;
	}

	event->value = FALSE;

	result = pthread_mutex_unlock(&event->lock);
	if (result != 0) {
		errno = result;
		return FALSE;
	}

	return TRUE;
}

BOOL
CloseEvent(xpevent_t event)
{
	int result;

	if (event == NULL || (event->magic != EVENT_MAGIC)) {
		errno = EINVAL;
		return FALSE;
	}

	/* Make sure there are no waiters. */
	result = pthread_mutex_lock(&event->lock);
	if (result != 0) {
		errno = result;
		return FALSE;
	}
	if (event->nwaiters > 0) {
		result = pthread_mutex_unlock(&event->lock);
		if (result != 0) {
			errno = result;
			return FALSE;
		}
		errno = EBUSY;
		return FALSE;
	}

	result = pthread_mutex_unlock(&event->lock);
	if (result != 0) {
		errno = result;
		return FALSE;
	}

	result = pthread_cond_destroy(&event->gtzero);
	if (result != 0) {
		errno = result;
		return FALSE;
	}
	result = pthread_mutex_destroy(&event->lock);
	if (result != 0) {
		event->magic = 0;
		errno = result;
		return FALSE;
	}
	event->magic = 0;

	free(event);

	return TRUE;
}

static int
get_abstime(DWORD ms, struct timespec *abstime)
{
	struct timeval currtime;
	long           nanoseconds;

	if (gettimeofday(&currtime, NULL) != 0)
		return -1;
	abstime->tv_sec = currtime.tv_sec + (time_t)(ms / 1000);
	nanoseconds = currtime.tv_usec * 1000 + (long)(ms % 1000) * 1000000;
	abstime->tv_sec += nanoseconds / 1000000000;
	abstime->tv_nsec = nanoseconds % 1000000000;
	return 0;
}

DWORD
WaitForEvent(xpevent_t event, DWORD ms)
{
	DWORD           retval = WAIT_FAILED;
	struct timespec abstime;
	int             result;

	if (event == NULL || (event->magic != EVENT_MAGIC)) {
		errno = EINVAL;
		return WAIT_FAILED;
	}

	if (ms && ms != INFINITE) {
		if (get_abstime(ms, &abstime) != 0)
			return WAIT_FAILED;
	}

	result = pthread_mutex_lock(&event->lock);
	if (result != 0) {
		errno = result;
		return WAIT_FAILED;
	}

	if (event->value)
		retval = WAIT_OBJECT_0;

	while ((!(event->value)) || (event->verify != NULL && !event->verify(event->cbdata))) {
		if (event->nwaiters == UINT32_MAX) {
			errno = EOVERFLOW;
			retval = WAIT_FAILED;
			goto DONE;
		}
		event->nwaiters++;
		switch (ms) {
			case 0:
				retval = WAIT_TIMEOUT;
				event->nwaiters--;
				goto DONE;
			case INFINITE:
				retval = pthread_cond_wait(&event->gtzero, &event->lock);
				if (retval) {
					errno = retval;
					retval = WAIT_FAILED;
					event->nwaiters--;
					goto DONE;
				}
				break;
			default:
				retval = pthread_cond_timedwait(&event->gtzero, &event->lock, &abstime);
				if (retval)  {
					if (retval == ETIMEDOUT)
						retval = WAIT_TIMEOUT;
					else {
						errno = retval;
						retval = WAIT_FAILED;
					}
					event->nwaiters--;
					goto DONE;
				}
		}
		event->nwaiters--;
	}

DONE:

	if (retval == WAIT_OBJECT_0) {
		if (!event->mreset)
			event->value = FALSE;
	}

	result = pthread_mutex_unlock(&event->lock);
	if (result != 0) {
		errno = result;
		retval = WAIT_FAILED;
	}

	return retval;
}

#endif /* __unix__ */
