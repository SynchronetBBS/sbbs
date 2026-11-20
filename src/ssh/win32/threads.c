/*
 * C11 <threads.h> shim — MinGW-w64 implementation.  See the header
 * for scope and rationale.
 */

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <process.h>	/* _beginthreadex */
#include <stdlib.h>	/* malloc / free */

#include "threads.h"
#include "../ssh-time.h"

/* ------------------------------------------------------------- mutexes */

int
dssh_thread_mtx_init(mtx_t *mtx, int type)
{
	if (type != mtx_plain)
		return thrd_error;
	InitializeCriticalSection(mtx);
	return thrd_success;
}

void
dssh_thread_mtx_destroy(mtx_t *mtx)
{
	DeleteCriticalSection(mtx);
}

int
dssh_thread_mtx_lock(mtx_t *mtx)
{
	EnterCriticalSection(mtx);
	return thrd_success;
}

int
dssh_thread_mtx_trylock(mtx_t *mtx)
{
	return TryEnterCriticalSection(mtx) ? thrd_success : thrd_busy;
}

int
dssh_thread_mtx_unlock(mtx_t *mtx)
{
	LeaveCriticalSection(mtx);
	return thrd_success;
}

/* ----------------------------------------------------- condition vars */

int
dssh_thread_cnd_init(cnd_t *cnd)
{
	InitializeConditionVariable(cnd);
	return thrd_success;
}

void
dssh_thread_cnd_destroy(cnd_t *cnd)
{
	/* CONDITION_VARIABLE has no destructor on Windows. */
	(void)cnd;
}

int
dssh_thread_cnd_signal(cnd_t *cnd)
{
	WakeConditionVariable(cnd);
	return thrd_success;
}

int
dssh_thread_cnd_broadcast(cnd_t *cnd)
{
	WakeAllConditionVariable(cnd);
	return thrd_success;
}

int
dssh_thread_cnd_wait(cnd_t *cnd, mtx_t *mtx)
{
	return SleepConditionVariableCS(cnd, mtx, INFINITE)
	    ? thrd_success : thrd_error;
}

int
dssh_thread_cnd_timedwait(cnd_t *cnd, mtx_t *mtx, const struct timespec *ts)
{
	if (ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000L)
		return thrd_error;
	bool waited = false;

	for (;;) {
		/* Windows waits use a bounded relative millisecond interval. Read
		 * the absolute clock again after expiration, including when the
		 * deadline needs multiple intervals or the clock moved backwards. */
		struct timespec now;
		if (timespec_get(&now, TIME_UTC) != TIME_UTC)
			return thrd_error;
		if (now.tv_nsec < 0 || now.tv_nsec >= 1000000000L)
			return thrd_error;
		bool expired = ts->tv_sec < now.tv_sec
		    || (ts->tv_sec == now.tv_sec && ts->tv_nsec <= now.tv_nsec);
		if (expired && waited)
			return thrd_timedout;
		DWORD timeout = 0;
		if (!expired) {
			/* Clamp before subtraction or multiplication. Below this
			 * threshold, widening the signed Win32 time_t values to
			 * int64_t makes their difference provably representable. */
			if (dssh_time_interval_reached(ts->tv_sec, now.tv_sec, UINT32_C(4294969)))
				timeout = UINT32_C(0xfffffffe);
			else {
				int64_t due = ts->tv_sec;
				int64_t current = now.tv_sec;
				int64_t difference = due - current;
				if (difference < 0)
					return thrd_error;
				uint64_t seconds = (uint64_t)difference;
				long nanos = ts->tv_nsec - now.tv_nsec;
				uint32_t millis;
				if (!dssh_relative_wait_ms(seconds, nanos, UINT32_C(0xfffffffe), &millis))
					return thrd_error;
				timeout = millis;
			}
		}
		if (SleepConditionVariableCS(cnd, mtx, timeout))
			return thrd_success;
		if (GetLastError() != ERROR_TIMEOUT)
			return thrd_error;
		if (expired)
			return thrd_timedout;
		waited = true;
	}
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
	unsigned result = (unsigned)rc;
	return result;
}

int
dssh_thread_thrd_create(thrd_t *thr, thrd_start_t func, void *arg)
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
	HANDLE handle = (HANDLE)h;
	*thr = handle;
	return thrd_success;
}

int
dssh_thread_thrd_join(thrd_t thr, int *res)
{
	/* Consume the handle only after every operation needed by this join
	 * succeeds.  A failure must allow the owner to retry joining. */
	if (WaitForSingleObject(thr, INFINITE) != WAIT_OBJECT_0)
		return thrd_error;
	int result = 0;

	if (res != NULL) {
		DWORD code;
		if (!GetExitCodeThread(thr, &code))
			return thrd_error;
		/* The trampoline converts int to the unsigned Windows exit code.
		 * Restore its signed value without an out-of-range signed cast. */
		int64_t signed_code = code;
		if (signed_code > INT32_MAX)
			signed_code -= INT64_C(4294967296);
		if (signed_code < INT_MIN || signed_code > INT_MAX)
			return thrd_error;
		int checked_result = (int)signed_code;
		result = checked_result;
	}
	if (!CloseHandle(thr))
		return thrd_error;
	if (res != NULL)
		*res = result;
	return thrd_success;
}

/* Sleep alone may wake before its requested interval. Recheck a monotonic
 * lower bound, allowing one counter tick for sampling quantization. Ceiling
 * ticks/ms underestimates elapsed time without multiplying a large counter. */
static int
dssh_sleep_millis(DWORD millis)
{
	if (millis == 0) {
		Sleep(0);
		return 0;
	}
	LARGE_INTEGER frequency, start, now;
	if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0
	    || !QueryPerformanceCounter(&start) || start.QuadPart < 0)
		return -2;
	uint64_t rate = (uint64_t)frequency.QuadPart;
	uint64_t ticks_per_ms = rate / 1000 + (rate % 1000 != 0 ? 1U : 0U);
	DWORD remaining = millis;
	for (;;) {
		Sleep(remaining);
		if (!QueryPerformanceCounter(&now) || now.QuadPart < start.QuadPart)
			return -2;
		int64_t difference = now.QuadPart - start.QuadPart;
		uint64_t ticks = (uint64_t)difference;
		if (ticks > 0)
			ticks--;
		uint64_t seconds = ticks / rate;
		if (seconds > millis / 1000)
			return 0;
		uint64_t elapsed = seconds * 1000 + (ticks % rate) / ticks_per_ms;
		if (elapsed >= millis)
			return 0;
		uint64_t left = millis - elapsed;
		/* left is positive and at most the original finite DWORD interval. */
		DWORD interval = (DWORD)left;
		remaining = interval;
	}
}

int
dssh_thread_thrd_sleep(const struct timespec *duration, struct timespec *remaining)
{
	(void)remaining; /* Sleep is not interrupted by signals or APC callbacks. */
	if (duration == NULL || duration->tv_sec < 0
	    || duration->tv_nsec < 0 || duration->tv_nsec >= 1000000000L)
		return -2;
	uint64_t seconds = (uint64_t)duration->tv_sec;
	/* Leave space for rounding the final fractional second, and never pass
	 * the INFINITE sentinel to Sleep. Each subtraction is guarded. */
	while (seconds > UINT32_C(4294966)) {
		if (dssh_sleep_millis(UINT32_C(4294966000)) != 0)
			return -2;
		seconds -= UINT32_C(4294966);
	}
	uint32_t last_seconds = (uint32_t)seconds;
	long rounded_nanos = (duration->tv_nsec + 999999L) / 1000000L;
	uint32_t fraction = (uint32_t)rounded_nanos;
	DWORD millis = last_seconds * UINT32_C(1000) + fraction;
	return dssh_sleep_millis(millis);
}

void
dssh_thread_thrd_yield(void)
{
	SwitchToThread();
}
