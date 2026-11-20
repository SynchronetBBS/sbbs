/*
 * C11 <threads.h> shim — POSIX pthread implementation.  See the
 * header for scope and rationale.
 */

#include <errno.h>
#include <sched.h>	/* sched_yield */
#include <stdlib.h>	/* malloc / free */

#include "threads.h"

/* ------------------------------------------------------------- mutexes */

int
dssh_thread_mtx_init(mtx_t *mtx, int type)
{
	if (type != mtx_plain)
		return thrd_error;
	int rc = pthread_mutex_init(mtx, NULL);
	if (rc == ENOMEM)
		return thrd_nomem;
	return rc == 0 ? thrd_success : thrd_error;
}

void
dssh_thread_mtx_destroy(mtx_t *mtx)
{
	if (pthread_mutex_destroy(mtx) != 0)
		abort();
}

int
dssh_thread_mtx_lock(mtx_t *mtx)
{
	return pthread_mutex_lock(mtx) == 0 ? thrd_success : thrd_error;
}

int
dssh_thread_mtx_trylock(mtx_t *mtx)
{
	int rc = pthread_mutex_trylock(mtx);
	if (rc == 0)
		return thrd_success;
	if (rc == EBUSY)
		return thrd_busy;
	return thrd_error;
}

int
dssh_thread_mtx_unlock(mtx_t *mtx)
{
	return pthread_mutex_unlock(mtx) == 0 ? thrd_success : thrd_error;
}

/* ----------------------------------------------------- condition vars */

int
dssh_thread_cnd_init(cnd_t *cnd)
{
	int rc = pthread_cond_init(cnd, NULL);
	if (rc == ENOMEM)
		return thrd_nomem;
	return rc == 0 ? thrd_success : thrd_error;
}

void
dssh_thread_cnd_destroy(cnd_t *cnd)
{
	if (pthread_cond_destroy(cnd) != 0)
		abort();
}

int
dssh_thread_cnd_signal(cnd_t *cnd)
{
	return pthread_cond_signal(cnd) == 0
	    ? thrd_success : thrd_error;
}

int
dssh_thread_cnd_broadcast(cnd_t *cnd)
{
	return pthread_cond_broadcast(cnd) == 0
	    ? thrd_success : thrd_error;
}

int
dssh_thread_cnd_wait(cnd_t *cnd, mtx_t *mtx)
{
	return pthread_cond_wait(cnd, mtx) == 0
	    ? thrd_success : thrd_error;
}

int
dssh_thread_cnd_timedwait(cnd_t *cnd, mtx_t *mtx, const struct timespec *ts)
{
	int rc = pthread_cond_timedwait(cnd, mtx, ts);
	if (rc == 0)
		return thrd_success;
	if (rc == ETIMEDOUT)
		return thrd_timedout;
	return thrd_error;
}

/* ------------------------------------------------------------ threads */

/* One context adapts the callback and retains its full int result until join.
 * No integer is encoded in a pointer, and a failed join retains the owner. */
struct dssh_thread_context {
	thrd_start_t func;
	void *arg;
	int result;
};

static void *
dssh_thrd_trampoline(void *arg)
{
	struct dssh_thread_context *ctx = arg;
	ctx->result = ctx->func(ctx->arg);
	return NULL;
}

int
dssh_thread_thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	struct dssh_thread_context *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		return thrd_nomem;
	ctx->func = func;
	ctx->arg = arg;
	pthread_t native;
	int rc = pthread_create(&native, NULL, dssh_thrd_trampoline, ctx);
	if (rc != 0) {
		free(ctx);
		return rc == ENOMEM ? thrd_nomem : thrd_error;
	}
	*thr = (thrd_t){ .native = native, .context = ctx };
	return thrd_success;
}

int
dssh_thread_thrd_join(thrd_t thr, int *res)
{
	if (pthread_join(thr.native, NULL) != 0)
		return thrd_error;
	int result = thr.context->result;
	free(thr.context);
	if (res != NULL)
		*res = result;
	return thrd_success;
}

int
dssh_thread_thrd_sleep(const struct timespec *duration, struct timespec *remaining)
{
	if (duration == NULL || duration->tv_sec < 0
	    || duration->tv_nsec < 0 || duration->tv_nsec >= 1000000000L)
		return -2;
	if (nanosleep(duration, remaining) == 0)
		return 0;
	return errno == EINTR ? -1 : -2;
}

void
dssh_thread_thrd_yield(void)
{
	/* A failed scheduling hint owns no resources; yielding is best effort. */
	if (sched_yield() != 0)
		return;
}
