#ifndef _XPSEM_H_
#define _XPSEM_H_

/*
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
#include <time.h>
#include "wrapdll.h"

/* Opaque type definition. */
struct xp_sem;
typedef struct xp_sem *xp_sem_t;

#define XP_SEM_FAILED	((xp_sem_t *)0)
#define XP_SEM_VALUE_MAX	INT_MAX

#if defined(__cplusplus)
extern "C" {
#endif
DLLEXPORT int	 xp_sem_init (xp_sem_t *, int, unsigned int);
DLLEXPORT int	 xp_sem_destroy (xp_sem_t *);
DLLEXPORT xp_sem_t	*xp_sem_open (const char *, int, ...);
DLLEXPORT int	 xp_sem_close (xp_sem_t *);
DLLEXPORT int	 xp_sem_unlink (const char *);
DLLEXPORT int	 xp_sem_wait (xp_sem_t *);
DLLEXPORT int	 xp_sem_trywait (xp_sem_t *);
DLLEXPORT int	 xp_sem_post (xp_sem_t *);
DLLEXPORT int  xp_sem_timedwait (xp_sem_t *sem, const struct timespec *abs_timeout);
#if defined(__cplusplus)
}
#endif

#endif /* _XPSEM_H_ */
