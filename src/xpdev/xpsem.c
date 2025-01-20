/*
 * $Id: xpsem.c,v 1.13 2012/01/26 01:44:02 deuce Exp $
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
#include "xpsem.h"
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include "gen_defs.h"
#include "genwrap.h"
#include "threadwrap.h"

int
xp_sem_init(xp_sem_t *sem, int pshared, unsigned int value)
{
	int retval;

	/*
	 * Range check the arguments.
	 */
	if (pshared != 0) {
		/*
		 * The user wants a semaphore that can be shared among
		 * processes, which this implementation can't do.  Sounds like a
		 * permissions problem to me (yeah right).
		 */
		errno = EPERM;
		retval = -1;
		goto RETURN;
	}

#if XP_SEM_VALUE_MAX < UINT_MAX
	if (value > XP_SEM_VALUE_MAX) {
		errno = EINVAL;
		retval = -1;
		goto RETURN;
	}
#endif

	*sem = (xp_sem_t)malloc(sizeof(struct xp_sem));
	if (*sem == NULL) {
		errno = ENOSPC;
		retval = -1;
		goto RETURN;
	}

	/*
	 * Initialize the semaphore.
	 */
	if (pthread_mutex_init(&(*sem)->lock, NULL) != 0) {
		free(*sem);
		errno = ENOSPC;
		retval = -1;
		goto RETURN;
	}

	if (pthread_cond_init(&(*sem)->gtzero, NULL) != 0) {
		while (pthread_mutex_destroy(&(*sem)->lock) == EBUSY)
			SLEEP(1);
		free(*sem);
		errno = ENOSPC;
		retval = -1;
		goto RETURN;
	}

	(*sem)->count = (uint32_t)value;
	(*sem)->nwaiters = 0;
	(*sem)->magic = XP_SEM_MAGIC;

	retval = 0;
RETURN:
	return retval;
}

int
xp_sem_destroy(xp_sem_t *sem)
{
	int retval;

	_SEM_CHECK_VALIDITY(sem);

	/* Make sure there are no waiters. */
	assert_pthread_mutex_lock(&(*sem)->lock);
	if ((*sem)->nwaiters > 0) {
		assert_pthread_mutex_unlock(&(*sem)->lock);
		errno = EBUSY;
		retval = -1;
		goto RETURN;
	}
	assert_pthread_mutex_unlock(&(*sem)->lock);

	while (pthread_mutex_destroy(&(*sem)->lock) == EBUSY)
		SLEEP(1);
	while (pthread_cond_destroy(&(*sem)->gtzero) == EBUSY)
		SLEEP(1);
	(*sem)->magic = 0;

	free(*sem);

	retval = 0;
RETURN:
	return retval;
}

xp_sem_t *
xp_sem_open(const char *name, int oflag, ...)
{
	errno = ENOSYS;
	return XP_SEM_FAILED;
}

int
xp_sem_close(xp_sem_t *sem)
{
	errno = ENOSYS;
	return -1;
}

int
xp_sem_unlink(const char *name)
{
	errno = ENOSYS;
	return -1;
}

int
xp_sem_wait(xp_sem_t *sem)
{
	int retval;

	_SEM_CHECK_VALIDITY(sem);

	assert_pthread_mutex_lock(&(*sem)->lock);

	while ((*sem)->count == 0) {
		(*sem)->nwaiters++;
		pthread_cond_wait(&(*sem)->gtzero, &(*sem)->lock);
		(*sem)->nwaiters--;
	}
	(*sem)->count--;

	assert_pthread_mutex_unlock(&(*sem)->lock);

	retval = 0;
RETURN:

	return retval;
}

int
xp_sem_trywait(xp_sem_t *sem)
{
	int retval;

	_SEM_CHECK_VALIDITY(sem);

	assert_pthread_mutex_lock(&(*sem)->lock);

	if ((*sem)->count > 0) {
		(*sem)->count--;
		retval = 0;
	} else {
		errno = EAGAIN;
		retval = -1;
	}

	assert_pthread_mutex_unlock(&(*sem)->lock);

RETURN:
	return retval;
}

int
xp_sem_post(xp_sem_t *sem)
{
	int retval;

	_SEM_CHECK_VALIDITY(sem);

	assert_pthread_mutex_lock(&(*sem)->lock);

	(*sem)->count++;
	if ((*sem)->nwaiters > 0) {
		/*
		 * We must use pthread_cond_broadcast() rather than
		 * pthread_cond_signal() in order to assure that the highest
		 * priority thread is run by the scheduler, since
		 * pthread_cond_signal() signals waiting threads in FIFO order.
		 */
		pthread_cond_broadcast(&(*sem)->gtzero);
	}

	assert_pthread_mutex_unlock(&(*sem)->lock);

	retval = 0;
RETURN:
	return retval;
}

int
xp_sem_getvalue(xp_sem_t *sem, int *sval)
{
	int retval;

	_SEM_CHECK_VALIDITY(sem);

	assert_pthread_mutex_lock(&(*sem)->lock);
	*sval = (int)(*sem)->count;
	assert_pthread_mutex_unlock(&(*sem)->lock);

	retval = 0;
RETURN:
	return retval;
}

int
xp_sem_setvalue(xp_sem_t *sem, int sval)
{
	int retval;

	_SEM_CHECK_VALIDITY(sem);

	assert_pthread_mutex_lock(&(*sem)->lock);
	(*sem)->count = (uint32_t)sval;
	if (((*sem)->nwaiters > 0) && sval) {
		/*
		 * We must use pthread_cond_broadcast() rather than
		 * pthread_cond_signal() in order to assure that the highest
		 * priority thread is run by the scheduler, since
		 * pthread_cond_signal() signals waiting threads in FIFO order.
		 */
		pthread_cond_broadcast(&(*sem)->gtzero);
	}
	assert_pthread_mutex_unlock(&(*sem)->lock);

	retval = 0;
RETURN:
	return retval;
}

int
xp_sem_timedwait(xp_sem_t *sem, const struct timespec *abs_timeout)
{
	int retval = 0;

	_SEM_CHECK_VALIDITY(sem);

	assert_pthread_mutex_lock(&(*sem)->lock);

	while ((*sem)->count == 0) {
		(*sem)->nwaiters++;
		retval = pthread_cond_timedwait(&(*sem)->gtzero, &(*sem)->lock, abs_timeout);
		(*sem)->nwaiters--;
		if (retval)  {
			errno = retval;
			retval = -1;
			break;
		}
	}
	if (retval == 0)
		(*sem)->count--;

	assert_pthread_mutex_unlock(&(*sem)->lock);

RETURN:

	return retval;
}
