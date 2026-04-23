/*
 * C11 <threads.h> shim — POSIX pthread implementation.  See the
 * header for scope and rationale.
 */

#include <errno.h>
#include <stdint.h>	/* intptr_t */
#include <stdlib.h>	/* malloc / free */

#include "threads.h"

/* ------------------------------------------------------------- mutexes */

int
mtx_init(mtx_t *mtx, int type)
{
	/* DeuceSSH only uses mtx_plain; the other C11 attributes (timed,
	 * recursive) aren't exercised.  Accept any combination silently —
	 * pthread's default mutex already covers mtx_plain. */
	(void)type;
	return pthread_mutex_init(mtx, NULL) == 0
	    ? thrd_success : thrd_error;
}

void
mtx_destroy(mtx_t *mtx)
{
	pthread_mutex_destroy(mtx);
}

int
mtx_lock(mtx_t *mtx)
{
	return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

int
mtx_trylock(mtx_t *mtx)
{
	int rc = pthread_mutex_trylock(mtx);
	if (rc == 0)
		return thrd_success;
	if (rc == EBUSY)
		return thrd_busy;
	return thrd_error;
}

int
mtx_unlock(mtx_t *mtx)
{
	return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}

/* ----------------------------------------------------- condition vars */

int
cnd_init(cnd_t *cnd)
{
	return pthread_cond_init(cnd, NULL) == 0
	    ? thrd_success : thrd_error;
}

void
cnd_destroy(cnd_t *cnd)
{
	pthread_cond_destroy(cnd);
}

int
cnd_signal(cnd_t *cnd)
{
	return pthread_cond_signal(cnd) == 0
	    ? thrd_success : thrd_error;
}

int
cnd_broadcast(cnd_t *cnd)
{
	return pthread_cond_broadcast(cnd) == 0
	    ? thrd_success : thrd_error;
}

int
cnd_wait(cnd_t *cnd, mtx_t *mtx)
{
	return pthread_cond_wait(cnd, mtx) == 0
	    ? thrd_success : thrd_error;
}

int
cnd_timedwait(cnd_t *cnd, mtx_t *mtx, const struct timespec *ts)
{
	int rc = pthread_cond_timedwait(cnd, mtx, ts);
	if (rc == 0)
		return thrd_success;
	if (rc == ETIMEDOUT)
		return thrd_timedout;
	return thrd_error;
}

/* ------------------------------------------------------------ threads */

/*
 * C11 `thrd_start_t` is `int(*)(void *)`; pthread_create wants
 * `void *(*)(void *)`.  Funnel through a small context struct so we
 * can convert the return type at the edge.
 */
struct dssh_thrd_ctx {
	thrd_start_t func;
	void        *arg;
};

static void *
dssh_thrd_trampoline(void *arg)
{
	struct dssh_thrd_ctx *ctx = arg;
	int rc = ctx->func(ctx->arg);
	free(ctx);
	/* Stuff the int return into the void* return so thrd_join can
	 * recover it via pthread_join's retval.  intptr_t round-trip is
	 * lossless for any int that fits in a pointer. */
	return (void *)(intptr_t)rc;
}

int
thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	struct dssh_thrd_ctx *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		return thrd_nomem;
	ctx->func = func;
	ctx->arg  = arg;
	if (pthread_create(thr, NULL, dssh_thrd_trampoline, ctx) != 0) {
		free(ctx);
		return thrd_error;
	}
	return thrd_success;
}

int
thrd_join(thrd_t thr, int *res)
{
	void *retval;
	if (pthread_join(thr, &retval) != 0)
		return thrd_error;
	if (res != NULL)
		*res = (int)(intptr_t)retval;
	return thrd_success;
}
