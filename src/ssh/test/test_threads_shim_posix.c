/* Actual private POSIX implementation with typed native failure controls. */
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../posix/threads.h"

enum operation {
	OP_NONE, OP_MUTEX_INIT, OP_MUTEX_DESTROY, OP_LOCK, OP_TRYLOCK, OP_UNLOCK,
	OP_COND_INIT, OP_COND_DESTROY, OP_SIGNAL, OP_BROADCAST, OP_WAIT,
	OP_TIMEDWAIT, OP_CREATE, OP_JOIN, OP_SLEEP, OP_YIELD
};
static enum operation failure;
static int native_error;
static bool allocation_failure;
static bool report_destroy_failure;
static unsigned allocations, releases;

#define NATIVE(name, parameters, arguments, operation) \
	static int native_##name parameters \
	{ \
		if (failure == operation) { \
			failure = OP_NONE; \
			if (report_destroy_failure) { \
				if (fputs("DSSH_TEST_SHIM_DESTROY_FAILURE\n", stderr) < 0 || fflush(stderr) != 0) \
					_Exit(9); \
			} \
			return native_error; \
		} \
		return name arguments; \
	}
NATIVE(pthread_mutex_init, (pthread_mutex_t *m, const pthread_mutexattr_t *a), (m, a), OP_MUTEX_INIT)
NATIVE(pthread_mutex_destroy, (pthread_mutex_t *m), (m), OP_MUTEX_DESTROY)
NATIVE(pthread_mutex_lock, (pthread_mutex_t *m), (m), OP_LOCK)
NATIVE(pthread_mutex_trylock, (pthread_mutex_t *m), (m), OP_TRYLOCK)
NATIVE(pthread_mutex_unlock, (pthread_mutex_t *m), (m), OP_UNLOCK)
NATIVE(pthread_cond_init, (pthread_cond_t *c, const pthread_condattr_t *a), (c, a), OP_COND_INIT)
NATIVE(pthread_cond_destroy, (pthread_cond_t *c), (c), OP_COND_DESTROY)
NATIVE(pthread_cond_signal, (pthread_cond_t *c), (c), OP_SIGNAL)
NATIVE(pthread_cond_broadcast, (pthread_cond_t *c), (c), OP_BROADCAST)
NATIVE(pthread_cond_wait, (pthread_cond_t *c, pthread_mutex_t *m), (c, m), OP_WAIT)
NATIVE(pthread_cond_timedwait, (pthread_cond_t *c, pthread_mutex_t *m, const struct timespec *t), (c, m, t), OP_TIMEDWAIT)
NATIVE(pthread_create, (pthread_t *t, const pthread_attr_t *a, void *(*f)(void *), void *v), (t, a, f, v), OP_CREATE)
NATIVE(pthread_join, (pthread_t t, void **r), (t, r), OP_JOIN)
#undef NATIVE

static int
native_nanosleep(const struct timespec *duration, struct timespec *remaining)
{
	if (failure == OP_SLEEP) {
		failure = OP_NONE;
		if (native_error == EINTR && remaining != NULL)
			*remaining = (struct timespec){ .tv_nsec = 7 };
		errno = native_error;
		return -1;
	}
	return nanosleep(duration, remaining);
}

static int
native_sched_yield(void)
{
	if (failure == OP_YIELD) {
		failure = OP_NONE;
		errno = native_error;
		return -1;
	}
	return sched_yield();
}

static void *
native_malloc(size_t size)
{
	if (allocation_failure) {
		allocation_failure = false;
		return NULL;
	}
	void *result = malloc(size);
	if (result != NULL)
		allocations++;
	return result;
}

static void
native_free(void *pointer)
{
	if (pointer != NULL)
		releases++;
	free(pointer);
}

#define pthread_mutex_init native_pthread_mutex_init
#define pthread_mutex_destroy native_pthread_mutex_destroy
#define pthread_mutex_lock native_pthread_mutex_lock
#define pthread_mutex_trylock native_pthread_mutex_trylock
#define pthread_mutex_unlock native_pthread_mutex_unlock
#define pthread_cond_init native_pthread_cond_init
#define pthread_cond_destroy native_pthread_cond_destroy
#define pthread_cond_signal native_pthread_cond_signal
#define pthread_cond_broadcast native_pthread_cond_broadcast
#define pthread_cond_wait native_pthread_cond_wait
#define pthread_cond_timedwait native_pthread_cond_timedwait
#define pthread_create native_pthread_create
#define pthread_join native_pthread_join
#define nanosleep native_nanosleep
#define sched_yield native_sched_yield
#define malloc native_malloc
#define free native_free
#include "../posix/threads.c"
#undef free
#undef malloc
#undef sched_yield
#undef nanosleep
#undef pthread_join
#undef pthread_create
#undef pthread_cond_timedwait
#undef pthread_cond_wait
#undef pthread_cond_broadcast
#undef pthread_cond_signal
#undef pthread_cond_destroy
#undef pthread_cond_init
#undef pthread_mutex_unlock
#undef pthread_mutex_trylock
#undef pthread_mutex_lock
#undef pthread_mutex_destroy
#undef pthread_mutex_init

static unsigned checks;
#define CHECK(expression) do { \
	checks++; \
	if (!(expression)) { \
		if (fprintf(stderr, "shim check failed at %d: %s\n", __LINE__, #expression) < 0) \
			return 2; \
		return 1; \
	} \
} while (0)

static int
worker(void *argument)
{
	return *(const int *)argument;
}

int
main(int argc, char **argv)
{
	mtx_t mutex;
	cnd_t condition;
	if (argc == 3 && strcmp(argv[1], "--join-fatal") == 0) {
		if (mtx_init(&mutex, mtx_plain) != thrd_success || cnd_init(&condition) != thrd_success)
			return 2;
		native_error = EBUSY;
		report_destroy_failure = true;
		if (strcmp(argv[2], "mutex-destroy") == 0) {
			failure = OP_MUTEX_DESTROY;
			mtx_destroy(&mutex);
		}
		else if (strcmp(argv[2], "condition-destroy") == 0) {
			failure = OP_COND_DESTROY;
			cnd_destroy(&condition);
		}
		return 3;
	}
	CHECK(argc == 1);
	CHECK(mtx_init(&mutex, mtx_recursive) == thrd_error);
	CHECK(mtx_init(&mutex, mtx_timed) == thrd_error);
	CHECK(mtx_init(&mutex, -1) == thrd_error);
	for (int nomem = 0; nomem < 2; nomem++) {
		native_error = nomem ? ENOMEM : EAGAIN;
		failure = OP_MUTEX_INIT;
		CHECK(mtx_init(&mutex, mtx_plain) == (nomem ? thrd_nomem : thrd_error));
		failure = OP_COND_INIT;
		CHECK(cnd_init(&condition) == (nomem ? thrd_nomem : thrd_error));
	}
	CHECK(mtx_init(&mutex, mtx_plain) == thrd_success);
	CHECK(cnd_init(&condition) == thrd_success);
	native_error = EINVAL;
	failure = OP_LOCK;
	CHECK(mtx_lock(&mutex) == thrd_error);
	failure = OP_TRYLOCK;
	CHECK(mtx_trylock(&mutex) == thrd_error);
	CHECK(mtx_trylock(&mutex) == thrd_success);
	CHECK(mtx_trylock(&mutex) == thrd_busy);
	failure = OP_UNLOCK;
	CHECK(mtx_unlock(&mutex) == thrd_error);
	CHECK(mtx_unlock(&mutex) == thrd_success);
	failure = OP_SIGNAL;
	CHECK(cnd_signal(&condition) == thrd_error);
	failure = OP_BROADCAST;
	CHECK(cnd_broadcast(&condition) == thrd_error);
	CHECK(cnd_signal(&condition) == thrd_success);
	CHECK(cnd_broadcast(&condition) == thrd_success);
	CHECK(mtx_lock(&mutex) == thrd_success);
	failure = OP_WAIT;
	CHECK(cnd_wait(&condition, &mutex) == thrd_error);
	struct timespec deadline;
	CHECK(timespec_get(&deadline, TIME_UTC) == TIME_UTC);
	failure = OP_TIMEDWAIT;
	CHECK(cnd_timedwait(&condition, &mutex, &deadline) == thrd_error);
	CHECK(cnd_timedwait(&condition, &mutex, &deadline) == thrd_timedout);
	CHECK(mtx_unlock(&mutex) == thrd_success);
	cnd_destroy(&condition);
	mtx_destroy(&mutex);

	thrd_t thread;
	unsigned char saved[sizeof(thread)];
	memset(&thread, 0x5a, sizeof(thread));
	memcpy(saved, &thread, sizeof(thread));
	int result = 7;
	allocation_failure = true;
	CHECK(thrd_create(&thread, worker, &result) == thrd_nomem);
	CHECK(memcmp(saved, &thread, sizeof(thread)) == 0 && allocations == releases);
	for (int nomem = 0; nomem < 2; nomem++) {
		native_error = nomem ? ENOMEM : EAGAIN;
		failure = OP_CREATE;
		CHECK(thrd_create(&thread, worker, &result) == (nomem ? thrd_nomem : thrd_error));
		CHECK(memcmp(saved, &thread, sizeof(thread)) == 0 && allocations == releases);
	}
	int values[] = {0, 1, -1, INT_MIN, INT_MAX, 12345, -12345};
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		for (unsigned mode = 0; mode < 3; mode++) {
			if (thrd_create(&thread, worker, &values[i]) != thrd_success)
				abort();
			result = 42;
			bool valid = allocations == releases + 1;
			if (mode == 1) {
				failure = OP_JOIN;
				native_error = EINVAL;
				int ret = thrd_join(thread, &result);
				valid = valid && ret == thrd_error && result == 42 && allocations == releases + 1;
			}
			if (thrd_join(thread, mode == 2 ? NULL : &result) != thrd_success)
				abort();
			CHECK(valid && (mode == 2 || result == values[i]) && allocations == releases);
		}
	}
	const struct timespec pause = { .tv_nsec = 1 };
	CHECK(thrd_sleep(&pause, NULL) == 0);
	CHECK(thrd_sleep(NULL, NULL) == -2);
	struct timespec invalid = { .tv_nsec = 1000000000L };
	CHECK(thrd_sleep(&invalid, NULL) == -2);
	invalid.tv_sec = -1;
	invalid.tv_nsec = 0;
	CHECK(thrd_sleep(&invalid, NULL) == -2);
	failure = OP_SLEEP;
	native_error = EINTR;
	CHECK(thrd_sleep(&pause, NULL) == -1);
	failure = OP_SLEEP;
	native_error = EINVAL;
	CHECK(thrd_sleep(&pause, NULL) == -2);
	struct timespec remaining = { .tv_sec = 1 };
	failure = OP_SLEEP;
	native_error = EINTR;
	CHECK(thrd_sleep(&remaining, &remaining) == -1);
	CHECK(remaining.tv_sec == 0 && remaining.tv_nsec == 7);
	remaining.tv_sec = 1;
	const struct timespec interrupted = { .tv_sec = 1 };
	failure = OP_SLEEP;
	CHECK(thrd_sleep(&interrupted, &remaining) == -1);
	CHECK(remaining.tv_sec == 0 && remaining.tv_nsec == 7 && interrupted.tv_sec == 1);
	failure = OP_YIELD;
	thrd_yield();
	CHECK(failure == OP_NONE);
	thrd_yield();
	if (printf("posix_thread_shim checks=%u results=21 allocations=%u releases=%u valid=1\n",
	    checks, allocations, releases) < 0 || fflush(stdout) != 0)
		return 2;
	return 0;
}
