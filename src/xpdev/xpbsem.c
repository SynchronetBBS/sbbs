/*
 * $Id$
 *
 * Copyright (C) 2000 Jason Evans <jasone@freebsd.org>.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libc_r/uthread/uthread_sem.c,v 1.3.2.1 2000/07/18 02:05:57 jasone Exp $
 */

#include <errno.h>
#include "xpbsem.h"
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include "gen_defs.h"

int
bsem_init(bsem_t *bsem, unsigned int value)
{
	int	retval;

	/*
	 * Range check the arguments.
	 */
	if (value)
		value=TRUE;

	*bsem = (bsem_t)malloc(sizeof(struct bsem));
	if (*bsem == NULL) {
		errno = ENOSPC;
		retval = -1;
		goto RETURN;
	}

	/*
	 * Initialize the semaphore.
	 */
	if (pthread_mutex_init(&(*bsem)->lock, NULL) != 0) {
		free(*bsem);
		errno = ENOSPC;
		retval = -1;
		goto RETURN;
	}

	if (pthread_cond_init(&(*bsem)->gtzero, NULL) != 0) {
		pthread_mutex_destroy(&(*bsem)->lock);
		free(*bsem);
		errno = ENOSPC;
		retval = -1;
		goto RETURN;
	}

	(*bsem)->count = (u_int32_t)value;
	(*bsem)->nwaiters = 0;
	(*bsem)->magic = BSEM_MAGIC;

	retval = 0;
  RETURN:
	return retval;
}

int
bsem_destroy(bsem_t *bsem)
{
	int	retval;
	
	_BSEM_CHECK_VALIDITY(bsem);

	/* Make sure there are no waiters. */
	pthread_mutex_lock(&(*bsem)->lock);
	if ((*bsem)->nwaiters > 0) {
		pthread_mutex_unlock(&(*bsem)->lock);
		errno = EBUSY;
		retval = -1;
		goto RETURN;
	}
	pthread_mutex_unlock(&(*bsem)->lock);
	
	pthread_mutex_destroy(&(*bsem)->lock);
	pthread_cond_destroy(&(*bsem)->gtzero);
	(*bsem)->magic = 0;

	free(*bsem);

	retval = 0;
  RETURN:
	return retval;
}

int
bsem_wait(bsem_t *bsem)
{
	int	retval;

	pthread_testcancel();
	
	_BSEM_CHECK_VALIDITY(bsem);

	pthread_mutex_lock(&(*bsem)->lock);

	while (!((*bsem)->count)) {
		(*bsem)->nwaiters++;
		pthread_cond_wait(&(*bsem)->gtzero, &(*bsem)->lock);
		(*bsem)->nwaiters--;
	}

	pthread_mutex_unlock(&(*bsem)->lock);

	retval = 0;
  RETURN:

	pthread_testcancel();
	return retval;
}

int
bsem_trywait(bsem_t *bsem)
{
	int	retval;

	_BSEM_CHECK_VALIDITY(bsem);

	pthread_mutex_lock(&(*bsem)->lock);

	if ((*bsem)->count)
		retval = 0;
	else {
		errno = EAGAIN;
		retval = -1;
	}
	
	pthread_mutex_unlock(&(*bsem)->lock);

  RETURN:
	return retval;
}

int
bsem_post(bsem_t *bsem)
{
	int	retval;

	_BSEM_CHECK_VALIDITY(bsem);

	pthread_mutex_lock(&(*bsem)->lock);

	(*bsem)->count=TRUE;
	if ((*bsem)->nwaiters > 0) {
		/*
		 * We must use pthread_cond_broadcast() rather than
		 * to wake up ALL waiting threads
		 */
		pthread_cond_broadcast(&(*bsem)->gtzero);
	}

	pthread_mutex_unlock(&(*bsem)->lock);

	retval = 0;
  RETURN:
	return retval;
}

int
bsem_getvalue(bsem_t *bsem, int *sval)
{
	int	retval;

	_BSEM_CHECK_VALIDITY(bsem);

	pthread_mutex_lock(&(*bsem)->lock);
	*sval = (int)(*bsem)->count;
	pthread_mutex_unlock(&(*bsem)->lock);

	retval = 0;
  RETURN:
	return retval;
}

int
bsem_timedwait(bsem_t *bsem, const struct timespec *abs_timeout)
{
	int	retval=0;

	pthread_testcancel();
	
	_BSEM_CHECK_VALIDITY(bsem);

	pthread_mutex_lock(&(*bsem)->lock);

	while (!((*bsem)->count)) {
		(*bsem)->nwaiters++;
		retval=pthread_cond_timedwait(&(*bsem)->gtzero, &(*bsem)->lock, abs_timeout);
		(*bsem)->nwaiters--;
		if(retval)  {
			errno=retval;
			retval=-1;
			break;
		}
	}

	pthread_mutex_unlock(&(*bsem)->lock);

  RETURN:

	pthread_testcancel();
	return retval;
}

int
bsem_reset(bsem_t *bsem)
{
	int	retval;

	_BSEM_CHECK_VALIDITY(bsem);

	pthread_mutex_lock(&(*bsem)->lock);
	(*bsem)->count=FALSE;
	pthread_mutex_unlock(&(*bsem)->lock);

	retval = 0;
  RETURN:
	return retval;
}
