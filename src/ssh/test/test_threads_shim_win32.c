/* Standalone MinGW test: compile this file directly, then run on Windows. */
#include <windows.h>
#include <limits.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>

static int fail_operation;

static DWORD WINAPI
test_wait(HANDLE handle, DWORD timeout)
{
	if (fail_operation == 1) {
		fail_operation = 0;
		return WAIT_FAILED;
	}
	return WaitForSingleObject(handle, timeout);
}

static BOOL WINAPI
test_exit_code(HANDLE handle, LPDWORD code)
{
	if (fail_operation == 2) {
		fail_operation = 0;
		return FALSE;
	}
	return GetExitCodeThread(handle, code);
}

static BOOL WINAPI
test_close(HANDLE handle)
{
	if (fail_operation == 3) {
		fail_operation = 0;
		return FALSE;
	}
	return CloseHandle(handle);
}

static bool scripted_clock;
static struct timespec clock_script[4];
static size_t clock_steps;
static size_t clock_calls;
static size_t clock_failure;
static DWORD waits[4];
static size_t wait_calls;
static DWORD wait_error;
static bool wait_success;

static int
test_timespec_get(struct timespec *ts, int base)
{
	if (!scripted_clock)
		return timespec_get(ts, base);
	size_t step = clock_calls++;
	if (step == clock_failure)
		return 0;
	if (step >= clock_steps)
		step = clock_steps - 1;
	*ts = clock_script[step];
	return TIME_UTC;
}

static BOOL WINAPI
test_cond_wait(PCONDITION_VARIABLE cnd, PCRITICAL_SECTION mtx, DWORD timeout)
{
	if (!scripted_clock)
		return SleepConditionVariableCS(cnd, mtx, timeout);
	if (wait_calls < sizeof(waits) / sizeof(waits[0]))
		waits[wait_calls] = timeout;
	wait_calls++;
	LeaveCriticalSection(mtx);
	EnterCriticalSection(mtx);
	SetLastError(wait_error);
	return wait_success;
}

static bool scripted_sleep;
static DWORD sleep_requests[2];
static size_t sleep_calls;
static bool sleep_early;
static int sleep_failure;
static unsigned counter_calls;
static LONGLONG sleep_ticks;

static void WINAPI
native_sleep(DWORD millis)
{
	if (!scripted_sleep) {
		Sleep(millis);
		return;
	}
	if (sleep_calls >= sizeof(sleep_requests) / sizeof(sleep_requests[0]))
		abort();
	sleep_requests[sleep_calls] = millis;
	sleep_calls++;
	if (sleep_early) {
		sleep_early = false;
		return;
	}
	sleep_ticks += millis;
	sleep_ticks++;
}

static BOOL WINAPI
native_frequency(LARGE_INTEGER *frequency)
{
	if (!scripted_sleep)
		return QueryPerformanceFrequency(frequency);
	if (sleep_failure == 1)
		return FALSE;
	frequency->QuadPart = sleep_failure == 2 ? 0 : 1000;
	return TRUE;
}

static BOOL WINAPI
native_counter(LARGE_INTEGER *counter)
{
	if (!scripted_sleep)
		return QueryPerformanceCounter(counter);
	counter_calls++;
	if (sleep_failure == 3 || (sleep_failure == 4 && counter_calls == 2))
		return FALSE;
	counter->QuadPart = sleep_ticks;
	if (sleep_failure == 5 && counter_calls == 2)
		counter->QuadPart = -1;
	return TRUE;
}

#define QueryPerformanceFrequency native_frequency
#define QueryPerformanceCounter native_counter
#define Sleep native_sleep
#define timespec_get test_timespec_get
#define SleepConditionVariableCS test_cond_wait
#define WaitForSingleObject test_wait
#define GetExitCodeThread test_exit_code
#define CloseHandle test_close
#include "../win32/threads.c"
#undef QueryPerformanceCounter
#undef QueryPerformanceFrequency
#undef Sleep
#undef WaitForSingleObject
#undef GetExitCodeThread
#undef CloseHandle
#undef timespec_get
#undef SleepConditionVariableCS

#define CHECK(expr) do { \
	if (!(expr)) { \
		if (fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #expr) < 0) \
			return 2; \
		return 1; \
	} \
} while (0)

static void
reset_wait_script(void)
{
	scripted_clock = true;
	clock_steps = 2;
	clock_calls = 0;
	clock_failure = SIZE_MAX;
	wait_calls = 0;
	wait_error = ERROR_TIMEOUT;
	wait_success = false;
	clock_script[0].tv_sec = 100;
	clock_script[0].tv_nsec = 999500000L;
	clock_script[1].tv_sec = 101;
	clock_script[1].tv_nsec = 0;
}

static int
test_deadlines(void)
{
	mtx_t mtx;
	cnd_t cnd;
	CHECK(mtx_init(&mtx, mtx_plain) == thrd_success);
	CHECK(cnd_init(&cnd) == thrd_success);
	CHECK(mtx_lock(&mtx) == thrd_success);
	struct timespec ts = { .tv_sec = 101, .tv_nsec = 0 };

	reset_wait_script();
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_timedout);
	CHECK(wait_calls == 1 && waits[0] == 1);
	CHECK(clock_calls == 2);

	reset_wait_script();
	wait_success = true;
	clock_script[0].tv_sec = 100;
	clock_script[0].tv_nsec = 0;
	ts.tv_sec = 105;
	ts.tv_nsec = 1;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_success);
	CHECK(wait_calls == 1 && waits[0] == 5001);

	reset_wait_script();
	clock_script[0].tv_sec = 0;
	clock_script[0].tv_nsec = 0;
	clock_script[1].tv_sec = INT64_MAX;
	ts.tv_sec = INT64_MAX;
	ts.tv_nsec = 0;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_timedout);
	CHECK(wait_calls == 1 && waits[0] == UINT32_C(0xfffffffe));
	CHECK(clock_calls == 2);

	reset_wait_script();
	clock_script[0].tv_sec = INT64_MIN;
	clock_script[0].tv_nsec = 0;
	clock_script[1].tv_sec = INT64_MAX;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_timedout);
	CHECK(wait_calls == 1 && waits[0] == UINT32_C(0xfffffffe));

	reset_wait_script();
	clock_script[0].tv_sec = INT64_MAX - 1;
	clock_script[0].tv_nsec = 0;
	clock_script[1].tv_sec = INT64_MAX;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_timedout);
	CHECK(wait_calls == 1 && waits[0] == 1000);

	reset_wait_script();
	clock_script[0].tv_sec = 0;
	clock_script[0].tv_nsec = 999999999L;
	ts.tv_sec = 4294968;
	ts.tv_nsec = 0;
	clock_script[1] = ts;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_timedout);
	CHECK(wait_calls == 1 && waits[0] == UINT32_C(4294967001));

	/* A relative wait may expire while the absolute clock moves back. */
	reset_wait_script();
	ts.tv_sec = 101;
	clock_script[1].tv_sec = 99;
	clock_script[2] = ts;
	clock_steps = 3;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_timedout);
	CHECK(wait_calls == 2 && waits[0] == 1 && waits[1] == 2000);

	reset_wait_script();
	clock_failure = 0;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_error);
	CHECK(wait_calls == 0);

	reset_wait_script();
	clock_failure = 1;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_error);
	CHECK(wait_calls == 1);

	reset_wait_script();
	wait_error = ERROR_INVALID_PARAMETER;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_error);
	CHECK(wait_calls == 1 && clock_calls == 1);

	reset_wait_script();
	ts.tv_nsec = -1;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_error);
	CHECK(wait_calls == 0 && clock_calls == 0);
	ts.tv_nsec = 1000000000L;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_error);
	CHECK(wait_calls == 0 && clock_calls == 0);

	reset_wait_script();
	ts.tv_nsec = 0;
	clock_script[0].tv_nsec = -1;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_error);
	CHECK(wait_calls == 0);

	reset_wait_script();
	ts.tv_sec = 99;
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_timedout);
	CHECK(wait_calls == 1 && waits[0] == 0);

	/* Exercise the real Windows release/wait/reacquire path as well. */
	scripted_clock = false;
	CHECK(timespec_get(&ts, TIME_UTC) == TIME_UTC);
	CHECK(cnd_timedwait(&cnd, &mtx, &ts) == thrd_timedout);
	CHECK(mtx_unlock(&mtx) == thrd_success);
	cnd_destroy(&cnd);
	mtx_destroy(&mtx);
	return 0;
}

static int
test_sleep_requests(void)
{
	struct timespec duration = { .tv_nsec = 1 };
	CHECK(thrd_sleep(&duration, NULL) == 0); /* Actual native Windows wait. */
	scripted_sleep = true;
	sleep_calls = 0;
	CHECK(thrd_sleep(&duration, NULL) == 0);
	CHECK(sleep_calls == 1 && sleep_requests[0] == 1);
	sleep_calls = 0;
	duration.tv_sec = 4294967;
	duration.tv_nsec = 999999999L;
	CHECK(thrd_sleep(&duration, NULL) == 0);
	CHECK(sleep_calls == 2 && sleep_requests[0] == UINT32_C(4294966000)
	    && sleep_requests[1] == 2000);
	sleep_calls = 0;
	CHECK(thrd_sleep(NULL, NULL) == -2);
	duration.tv_sec = -1;
	CHECK(thrd_sleep(&duration, NULL) == -2);
	duration.tv_sec = 0;
	duration.tv_nsec = -1;
	CHECK(thrd_sleep(&duration, NULL) == -2);
	duration.tv_nsec = 1000000000L;
	CHECK(thrd_sleep(&duration, NULL) == -2);
	CHECK(sleep_calls == 0);
	duration.tv_nsec = 0;
	CHECK(thrd_sleep(&duration, NULL) == 0);
	CHECK(sleep_calls == 1 && sleep_requests[0] == 0);
	sleep_calls = 0;
	duration.tv_nsec = 1;
	sleep_early = true;
	CHECK(thrd_sleep(&duration, NULL) == 0);
	CHECK(sleep_calls == 2 && sleep_requests[0] == 1 && sleep_requests[1] == 1);
	for (int fail = 1; fail <= 5; fail++) {
		sleep_calls = 0;
		counter_calls = 0;
		sleep_failure = fail;
		CHECK(thrd_sleep(&duration, NULL) == -2);
		CHECK(sleep_calls == (fail >= 4 ? 1 : 0));
	}
	sleep_failure = 0;
	scripted_sleep = false;
	return 0;
}

static int
worker(void *arg)
{
	return *(const int *)arg;
}

int
main(void)
{
	int results[] = { 0, 1, -1, INT_MIN, INT_MAX };
	mtx_t unsupported;
	CHECK(mtx_init(&unsupported, mtx_recursive) == thrd_error);
	CHECK(mtx_init(&unsupported, mtx_timed) == thrd_error);

	for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
		for (int failure = 0; failure < 4; failure++) {
			thrd_t thread;
			CHECK(thrd_create(&thread, worker, &results[i]) == thrd_success);
			int result = 42;

			fail_operation = failure;
			if (failure != 0) {
				CHECK(thrd_join(thread, &result) == thrd_error);
				CHECK(result == 42);
				CHECK(fail_operation == 0);
			}
			CHECK(thrd_join(thread, &result) == thrd_success);
			CHECK(result == results[i]);
		}
	}
	int result = -1;
	thrd_t thread;
	CHECK(thrd_create(&thread, worker, &result) == thrd_success);
	CHECK(thrd_join(thread, NULL) == thrd_success);
	CHECK(test_deadlines() == 0);
	CHECK(test_sleep_requests() == 0);
	if (puts("Win32 thread join, deadline and sleep checks passed") == EOF || fflush(stdout) != 0)
		return 2;
	return 0;
}
