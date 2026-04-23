/*
 * C11 <threads.h> shim — MinGW-w64 implementation.  See the header
 * for scope and rationale.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <errno.h>
#include <process.h>	/* _beginthreadex */
#include <stdlib.h>	/* malloc / free */

#include "threads.h"

/* ------------------------------------------------------------- mutexes */

int
mtx_init(mtx_t *mtx, int type)
{
	/* DeuceSSH only uses mtx_plain; the other C11 attributes (timed,
	 * recursive) aren't exercised.  Accept any combination silently —
	 * CRITICAL_SECTION is already recursive on the same thread, and
	 * timed-lock support would need SRWLock or a condition variable
	 * retrofit that we don't need today. */
	(void)type;
	InitializeCriticalSection(mtx);
	return thrd_success;
}

void
mtx_destroy(mtx_t *mtx)
{
	DeleteCriticalSection(mtx);
}

int
mtx_lock(mtx_t *mtx)
{
	EnterCriticalSection(mtx);
	return thrd_success;
}

int
mtx_trylock(mtx_t *mtx)
{
	return TryEnterCriticalSection(mtx) ? thrd_success : thrd_busy;
}

int
mtx_unlock(mtx_t *mtx)
{
	LeaveCriticalSection(mtx);
	return thrd_success;
}

/* ----------------------------------------------------- condition vars */

int
cnd_init(cnd_t *cnd)
{
	InitializeConditionVariable(cnd);
	return thrd_success;
}

void
cnd_destroy(cnd_t *cnd)
{
	/* CONDITION_VARIABLE has no destructor on Windows. */
	(void)cnd;
}

int
cnd_signal(cnd_t *cnd)
{
	WakeConditionVariable(cnd);
	return thrd_success;
}

int
cnd_broadcast(cnd_t *cnd)
{
	WakeAllConditionVariable(cnd);
	return thrd_success;
}

int
cnd_wait(cnd_t *cnd, mtx_t *mtx)
{
	return SleepConditionVariableCS(cnd, mtx, INFINITE)
	    ? thrd_success : thrd_error;
}

int
cnd_timedwait(cnd_t *cnd, mtx_t *mtx, const struct timespec *ts)
{
	/* C11 timedwait takes an *absolute* deadline; SleepConditionVariable
	 * takes a relative timeout in milliseconds.  Convert. */
	struct timespec now;
	timespec_get(&now, TIME_UTC);
	long long sec_diff  = (long long)ts->tv_sec  - (long long)now.tv_sec;
	long long nsec_diff = (long long)ts->tv_nsec - (long long)now.tv_nsec;
	long long ms        = sec_diff * 1000LL + nsec_diff / 1000000LL;
	if (ms < 0)
		ms = 0;
	if (ms > 0xFFFFFFFELL)		/* leave INFINITE (0xFFFFFFFF) for cnd_wait */
		ms = 0xFFFFFFFELL;
	if (SleepConditionVariableCS(cnd, mtx, (DWORD)ms))
		return thrd_success;
	return (GetLastError() == ERROR_TIMEOUT) ? thrd_timedout : thrd_error;
}

/* ------------------------------------------------------------ threads */

/*
 * C11 `thrd_start_t` is `int(*)(void *)`; Windows `_beginthreadex`
 * wants `unsigned __stdcall (*)(void *)`.  Funnel through a small
 * context struct so we can convert the calling convention + return
 * type at the edge.
 */
struct dssh_thrd_ctx {
	thrd_start_t func;
	void        *arg;
};

static unsigned __stdcall
dssh_thrd_trampoline(void *arg)
{
	struct dssh_thrd_ctx *ctx = arg;
	int rc = ctx->func(ctx->arg);
	free(ctx);
	return (unsigned)rc;
}

int
thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
{
	struct dssh_thrd_ctx *ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		return thrd_nomem;
	ctx->func = func;
	ctx->arg  = arg;
	/* _beginthreadex rather than CreateThread so the MSVCRT per-thread
	 * state is initialised (errno, locale, etc.). */
	uintptr_t h = _beginthreadex(NULL, 0, dssh_thrd_trampoline, ctx, 0, NULL);
	if (h == 0) {
		free(ctx);
		return thrd_error;
	}
	*thr = (HANDLE)h;
	return thrd_success;
}

int
thrd_join(thrd_t thr, int *res)
{
	if (WaitForSingleObject(thr, INFINITE) != WAIT_OBJECT_0) {
		CloseHandle(thr);
		return thrd_error;
	}
	if (res != NULL) {
		DWORD code = 0;
		GetExitCodeThread(thr, &code);
		*res = (int)code;
	}
	CloseHandle(thr);
	return thrd_success;
}
