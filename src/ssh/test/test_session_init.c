/* Fresh-process, public-header consumer of the production library. No test
 * global reset is called: the first concurrent constructors are exercised. */
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include "deucessh.h"

static atomic_bool start;
static atomic_uint failures;
static atomic_uint sessions;
static dssh_session held[8][32];

static int
worker(void *opaque)
{
	unsigned *index = opaque;
	while (!atomic_load(&start))
		thrd_yield();
	for (unsigned round = 0; round < 32; round++) {
		dssh_session sess = dssh_session_init(round % 2 == 0, 0);
		if (sess == NULL) {
			atomic_fetch_add(&failures, 1);
			continue;
		}
		atomic_fetch_add(&sessions, 1);
		held[*index][round] = sess;
		if (dssh_session_is_terminated(sess)
		    || dssh_session_set_timeout(sess, 100) != 0
		    || dssh_session_set_rekey_seconds(sess, 0) != 0)
			atomic_fetch_add(&failures, 1);
	}
	return 0;
}

int
main(int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "configured") != 0
		    || dssh_transport_set_version("InitFixture", "parallel sessions") != 0)
			return 1;
	}
	else if (argc != 1)
		return 1;
	thrd_t threads[8];
	unsigned indices[8];
	unsigned created = 0;
	for (; created < 8; created++) {
		indices[created] = created;
		if (thrd_create(&threads[created], worker, &indices[created]) != thrd_success)
			break;
	}
	atomic_store(&start, true);
	for (unsigned i = 0; i < created; i++) {
		if (thrd_join(threads[i], NULL) != thrd_success)
			abort();
	}
	/* Retain every construction until the workers have joined. This tests
	 * concurrent first use without also recycling the host thread runtime's
	 * private mutex allocations between constructors on different threads. */
	for (unsigned i = 0; i < created; i++) {
		for (unsigned round = 0; round < 32; round++) {
			if (held[i][round] != NULL)
				dssh_session_cleanup(held[i][round]);
		}
	}
	bool frozen = dssh_transport_set_version("Later", NULL) == DSSH_ERROR_TOOLATE;
	printf("workers=%u sessions=%u failures=%u frozen=%d\n", created,
	    atomic_load(&sessions), atomic_load(&failures), frozen);
	return created == 8 && atomic_load(&sessions) == 256 && atomic_load(&failures) == 0 && frozen ? 0 : 1;
}
