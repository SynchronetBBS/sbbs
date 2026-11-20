/* Public C17 consumer: native application C11 threads remain independent of
 * the production library's private fallback facility. No shim header is used. */
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <time.h>
#include "deucessh.h"

static atomic_bool start;
static atomic_uint failures;
static dssh_session sessions[4][16];

static int
worker(void *opaque)
{
	unsigned *index = opaque;
	while (!atomic_load(&start))
		thrd_yield();
	for (unsigned i = 0; i < 16; i++) {
		dssh_session session = dssh_session_init(i % 2 == 0, 0);
		if (session == NULL) {
			atomic_fetch_add(&failures, 1);
			continue;
		}
		sessions[*index][i] = session;
		if (dssh_session_is_terminated(session)
		    || dssh_session_set_timeout(session, 100) != 0)
			atomic_fetch_add(&failures, 1);
	}
	return 0;
}

int
main(void)
{
	mtx_t mutex;
	int status = mtx_init(&mutex, mtx_plain);
	if (status != thrd_success) {
		if (fprintf(stderr, "application mutex initialization: returned=%d expected=%d\n",
		    status, thrd_success) < 0)
			return 2;
		return 1;
	}
	cnd_t condition;
	if (cnd_init(&condition) != thrd_success)
		abort();
	if (mtx_lock(&mutex) != thrd_success)
		abort();
	struct timespec deadline;
	bool valid = timespec_get(&deadline, TIME_UTC) == TIME_UTC;
	if (valid)
		valid = cnd_timedwait(&condition, &mutex, &deadline) == thrd_timedout;
	if (mtx_unlock(&mutex) != thrd_success)
		abort();
	if (cnd_signal(&condition) != thrd_success || cnd_broadcast(&condition) != thrd_success)
		valid = false;
	cnd_destroy(&condition);
	mtx_destroy(&mutex);
	if (!valid)
		return 1;

	thrd_t threads[4];
	unsigned indices[4];
	unsigned created = 0;
	for (; created < 4; created++) {
		indices[created] = created;
		if (thrd_create(&threads[created], worker, &indices[created]) != thrd_success)
			break;
	}
	atomic_store(&start, true);
	for (unsigned i = 0; i < created; i++) {
		int result;
		if (thrd_join(threads[i], &result) != thrd_success)
			abort();
		if (result != 0)
			valid = false;
	}
	unsigned completed = 0;
	for (unsigned i = 0; i < created; i++) {
		for (unsigned j = 0; j < 16; j++) {
			if (sessions[i][j] != NULL) {
				completed++;
				dssh_session_cleanup(sessions[i][j]);
			}
		}
	}
	valid = valid && created == 4 && completed == 64 && atomic_load(&failures) == 0;
	if (printf("application_threads workers=%u sessions=%u failures=%u native=%d valid=%d\n",
	    created, completed, atomic_load(&failures), thrd_success, valid ? 1 : 0) < 0
	    || fflush(stdout) != 0)
		return 2;
	return valid ? 0 : 1;
}
