#ifndef _SEMAPHORE_H_
#define _SEMAPHORE_H_

/*
 * $Id$
 *
 * semaphore.h: POSIX 1003.1b semaphores
*/

/*-
 * Copyright (c) 1996, 1997
 *	HD Associates, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by HD Associates, Inc
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL HD ASSOCIATES OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/posix4/semaphore.h,v 1.6 2000/01/20 07:55:42 jasone Exp $
 */

#include <machine/limits.h>

#include <sys/types.h>
#include <fcntl.h>

/* Opaque type definition. */
struct sem;
typedef struct sem *sem_t;

#define SEM_FAILED	((sem_t *)0)
#define SEM_VALUE_MAX	UINT_MAX

#ifndef KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int	 sem_init __P((sem_t *, int, unsigned int));
int	 sem_destroy __P((sem_t *));
sem_t	*sem_open __P((const char *, int, ...));
int	 sem_close __P((sem_t *));
int	 sem_unlink __P((const char *));
int	 sem_wait __P((sem_t *));
int	 sem_trywait __P((sem_t *));
int	 sem_post __P((sem_t *));
int	 sem_getvalue __P((sem_t *, int *));
__END_DECLS
#endif /* KERNEL */

/*
* $Id$
*/

/* Begin thread_private.h kluge */
/*
 * These come out of (or should go into) thread_private.h - rather than have 
 * to copy (or symlink) the files from the source tree these definitions are 
 * inlined here.  Obviously these go away when this module is part of libc.
*/

struct sem {
#define SEM_MAGIC       ((u_int32_t) 0x09fa4012)
        u_int32_t       magic;
        pthread_mutex_t lock;
        pthread_cond_t  gtzero;
        u_int32_t       count;
        u_int32_t       nwaiters;
};

extern pthread_once_t _thread_init_once;
extern int _threads_initialized;
extern void  _thread_init __P((void));
#define THREAD_INIT() \
	(void) pthread_once(&_thread_init_once, _thread_init)
#define THREAD_SAFE() \
	(_threads_initialized != 0)

#define _SEM_CHECK_VALIDITY(sem)		\
	if ((*(sem))->magic != SEM_MAGIC) {	\
		errno = EINVAL;			\
		retval = -1;			\
		goto RETURN;			\
	}

struct pthread_rwlockattr {
        int             pshared;
	};

struct pthread_rwlock {
        pthread_mutex_t lock;   /* monitor lock */
	int             state;  /* 0 = idle  >0 = # of readers  -1 = writer */
	pthread_cond_t  read_signal;
	pthread_cond_t  write_signal;
	int             blocked_writers;
	};
/* End thread_private.h kluge */

#endif /* _SEMAPHORE_H_ */
