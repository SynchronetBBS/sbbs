/*
 * Minimal C11 <threads.h> shim for POSIX systems whose libc doesn't
 * expose the C11 threads API.
 *
 * macOS (Apple's libc), older NetBSD, and older glibc ship pthread
 * but not <threads.h>.  This file provides the subset of the C11
 * threads API that DeuceSSH actually uses — mtx_*, cnd_*, thrd_* —
 * as thin wrappers over pthread primitives.  Platform-gated in
 * DeuceSSH's CMakeLists: only added to the include path when the
 * libc <threads.h> probe fails and no libstdthreads is available.
 *
 * Deliberately minimal: no tss_* (thread-specific storage), no
 * call_once, no mtx_timed / mtx_recursive support.  Add as needed.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DSSH_POSIX_THREADS_H
#define DSSH_POSIX_THREADS_H

#include <pthread.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef pthread_mutex_t  mtx_t;
typedef pthread_cond_t   cnd_t;
typedef pthread_t        thrd_t;
typedef int            (*thrd_start_t)(void *);

enum {
	mtx_plain      = 0,
	mtx_timed      = 1,
	mtx_recursive  = 2,
};

enum {
	thrd_success   = 0,
	thrd_error     = 1,
	thrd_nomem     = 2,
	thrd_busy      = 3,
	thrd_timedout  = 4,
};

int  mtx_init(mtx_t *mtx, int type);
void mtx_destroy(mtx_t *mtx);
int  mtx_lock(mtx_t *mtx);
int  mtx_trylock(mtx_t *mtx);
int  mtx_unlock(mtx_t *mtx);

int  cnd_init(cnd_t *cnd);
void cnd_destroy(cnd_t *cnd);
int  cnd_signal(cnd_t *cnd);
int  cnd_broadcast(cnd_t *cnd);
int  cnd_wait(cnd_t *cnd, mtx_t *mtx);
int  cnd_timedwait(cnd_t *cnd, mtx_t *mtx, const struct timespec *ts);

int  thrd_create(thrd_t *thr, thrd_start_t func, void *arg);
int  thrd_join(thrd_t thr, int *res);

#ifdef __cplusplus
}
#endif

#endif
