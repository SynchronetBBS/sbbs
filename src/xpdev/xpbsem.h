#ifndef _XPBSEM_H_
#define _XPBSEM_H_

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

#include <limits.h>

#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>

/* Opaque type definition. */
struct bsem;
typedef struct bsem *bsem_t;

#ifdef __solaris__
typedef unsigned int	u_int32_t;
#endif

#if defined(__cplusplus)
extern "C" {
#endif
int	 bsem_init (bsem_t *, unsigned int);
int	 bsem_destroy (bsem_t *);
int	 bsem_wait (bsem_t *);
int	 bsem_trywait (bsem_t *);
int	 bsem_post (bsem_t *);
int	 bsem_getvalue (bsem_t *, int *);
int	 bsem_reset (bsem_t *);
int  bsem_timedwait (bsem_t *bsem, const struct timespec *abs_timeout);
#if defined(__cplusplus)
}
#endif

/*
* $Id$
*/

struct bsem {
#define BSEM_MAGIC       ((u_int32_t) 0x09fa4013)
        u_int32_t       magic;
        pthread_mutex_t lock;
        pthread_cond_t  gtzero;
        u_int32_t       count;
        u_int32_t       nwaiters;
};

#define _BSEM_CHECK_VALIDITY(bsem)		\
	if (bsem==NULL || (*(bsem))->magic != BSEM_MAGIC) {	\
		errno = EINVAL;			\
		retval = -1;			\
		goto RETURN;			\
	}

#endif /* _XPBSEM_H_ */
