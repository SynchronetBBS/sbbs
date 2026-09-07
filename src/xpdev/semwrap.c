/* Semaphore-related cross-platform development wrappers */

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

#include <errno.h>
#include "semwrap.h"

#if defined(__unix__)

#include <sys/time.h>   /* timespec */
#include <stdlib.h> /* NULL */

int
xp_sem_trywait_block(sem_t *sem, unsigned long timeout)
{
	int             retval;
	long            nanoseconds;
	struct timespec abstime;
	struct timeval  currtime;

	if (gettimeofday(&currtime, NULL) != 0)
		return -1;
	abstime.tv_sec = currtime.tv_sec + (time_t)(timeout / 1000);
	nanoseconds = currtime.tv_usec * 1000 + (long)(timeout % 1000) * 1000000;
	abstime.tv_sec += nanoseconds / 1000000000;
	abstime.tv_nsec = nanoseconds % 1000000000;

	retval = sem_timedwait(sem, &abstime);
	if (retval && errno == ETIMEDOUT)
		errno = EAGAIN;
	return retval;
}

#elif defined(_WIN32)

#include <limits.h>     /* INT_MAX */

#if defined(__BORLANDC__)
	#pragma argsused
#endif
int sem_init(sem_t* psem, int pshared, unsigned int value)
{
	if (psem == NULL || value > INT_MAX) {
		errno = EINVAL;
		return -1;
	}
	if (pshared != 0) {
		errno = EINVAL;
		return -1;
	}
	if ((*(psem) = CreateSemaphore(NULL, value, INT_MAX, NULL)) == NULL)
		return -1;

	return 0;
}

int xp_sem_trywait_block(sem_t* psem, unsigned long timeout)
{
	if (psem == NULL || *psem == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (WaitForSingleObject(*psem, timeout) != WAIT_OBJECT_0) {
		errno = EAGAIN;
		return -1;
	}
	return 0;
}

int sem_post(sem_t* psem)
{
	if (psem == NULL || *psem == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (ReleaseSemaphore(*psem, 1, NULL) == TRUE)
		return 0;

	return -1;
}

int sem_getvalue(sem_t* psem, int* vp)
{
	int retval = 0;

	if (psem == NULL || *psem == NULL || vp == NULL) {
		errno = EINVAL;
		return -1;
	}
	/* Note, this should REALLY be in a critical section... */
	if (WaitForSingleObject(*(psem), 0) != WAIT_OBJECT_0)
		*vp = 0;
	else {
		if (ReleaseSemaphore(*(psem), 1, (LPLONG)vp))
			(*vp)++;
		else
			retval = -1;
	}
	return retval;
}

int sem_destroy(sem_t* psem)
{
	if (psem == NULL || *psem == NULL) {
		errno = EINVAL;
		return -1;
	}
	if (CloseHandle(*psem) == TRUE) {
		*psem = NULL;
		return 0;
	}
	return -1;
}

#endif /* _WIN32 */
