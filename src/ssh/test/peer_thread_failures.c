/* GNU --wrap controls for the separate production-linked peer executables.
 * The first create is on main, before any worker exists. Main joins every
 * successful application create before cleanup can join library workers.
 * Child threads have their own false controller flag and use real operations.
 * No thread identity is retained after a native join. */
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "deucessh.h"

#ifdef DSSH_THREADS_SHIM
 #define REAL_CREATE __real_dssh_thread_thrd_create
 #define REAL_JOIN __real_dssh_thread_thrd_join
 #define WRAP_CREATE __wrap_dssh_thread_thrd_create
 #define WRAP_JOIN __wrap_dssh_thread_thrd_join
#else
 #define REAL_CREATE __real_thrd_create
 #define REAL_JOIN __real_thrd_join
 #define WRAP_CREATE __wrap_thrd_create
 #define WRAP_JOIN __wrap_thrd_join
#endif

int REAL_CREATE(thrd_t *, thrd_start_t, void *);
int REAL_JOIN(thrd_t, int *);
int __real_mock_tx_client(uint8_t *, size_t, dssh_session, void *);

static bool configured;
static _Thread_local bool controller;
static const char *mode;
static unsigned create_index;
static unsigned join_index;
static unsigned application_workers;
static atomic_bool tx_failed;

#ifdef DSSH_PEER_DEFER_CONTROL
static atomic_bool controls_active;

/* A rekey consumer first completes and joins an ordinary handshake. Enable
 * only after that prefix retires; subsequent counters describe the rekey. */
void
dssh_test_peer_controls_begin(void)
{
	if (!configured || !controller || create_index != 0 || join_index != 0
	    || application_workers != 0 || atomic_exchange(&controls_active, true))
		abort();
}
#endif

static void
configure(void)
{
	controller = true;
	mode = getenv("DSSH_PEER_FAILURE");
	if (mode == NULL)
		mode = "plain";
	configured = true;
}

static bool
selected(const char *prefix, unsigned index, const char *suffix)
{
	size_t length = strlen(prefix);
	return index < 4 && strncmp(mode, prefix, length) == 0
	    && mode[length] == '0' + index && strcmp(mode + length + 1, suffix) == 0;
}

int
WRAP_CREATE(thrd_t *thread, thrd_start_t start, void *arg)
{
	/* This publication precedes the first real native create. Every child
	 * therefore observes configured/mode after native thread startup. */
	if (!configured)
		configure();
#ifdef DSSH_PEER_DEFER_CONTROL
	if (!atomic_load(&controls_active))
		return REAL_CREATE(thread, start, arg);
#endif
	if (controller) {
		unsigned index = create_index;
		if (create_index != UINT_MAX)
			create_index++;
		if (selected("create-", index, "")) {
			fprintf(stderr, "PEER_CREATE_FAILURE index=%u\n", index);
			return thrd_error;
		}
		int ret = REAL_CREATE(thread, start, arg);
		if (ret == thrd_success && application_workers != UINT_MAX)
			application_workers++;
		return ret;
	}
	return REAL_CREATE(thread, start, arg);
}

int
WRAP_JOIN(thrd_t thread, int *result)
{
#ifdef DSSH_PEER_DEFER_CONTROL
	if (!atomic_load(&controls_active))
		return REAL_JOIN(thread, result);
#endif
	if (!controller || join_index >= application_workers)
		return REAL_JOIN(thread, result);
	unsigned index = join_index;
	if (join_index != UINT_MAX)
		join_index++;
	if (selected("join-", index, "-retained")) {
		fprintf(stderr, "PEER_JOIN_FAILURE index=%u retained=1\n", index);
		return thrd_error;
	}
	int ret = REAL_JOIN(thread, result);
	fprintf(stderr, "PEER_NATIVE_JOIN index=%u success=%u\n", index,
	    ret == thrd_success ? 1U : 0U);
	if (ret == thrd_success && selected("join-", index, "-released")) {
		fprintf(stderr, "PEER_JOIN_FAILURE index=%u retained=0\n", index);
		return thrd_error;
	}
	return ret;
}

int
__wrap_mock_tx_client(uint8_t *buf, size_t len, dssh_session sess, void *opaque)
{
#ifdef DSSH_PEER_DEFER_CONTROL
	if (!atomic_load(&controls_active))
		return __real_mock_tx_client(buf, len, sess, opaque);
#endif
	if (strcmp(mode, "tx") == 0 && !atomic_exchange(&tx_failed, true)) {
		fputs("PEER_TX_FAILURE\n", stderr);
		return -1;
	}
	return __real_mock_tx_client(buf, len, sess, opaque);
}
