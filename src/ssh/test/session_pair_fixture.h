/* Private ownership for paired-session tests. No initialized resource or native
 * worker is copied. Assertion cleanup runs before caller stack frames expire. */
#ifndef DSSH_TEST_SESSION_PAIR_FIXTURE_H
#define DSSH_TEST_SESSION_PAIR_FIXTURE_H

#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>
#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "mock_io.h"

struct test_session_pair;
struct test_pair_worker {
	struct test_session_pair *pair;
	bool client;
};

struct test_session_pair {
	struct mock_io_state io;
	dssh_session client;
	dssh_session server;
	int client_result;
	int server_result;
	bool io_ready;
	bool registered;
	struct test_session_pair *previous;
	int (*operation)(dssh_session);
	struct test_pair_worker worker[2];
	thrd_t threads[2];
	bool owned[2];
	unsigned created;
	unsigned joined;
};

static struct test_session_pair *test_pair_active;

static inline void
test_pair_begin(struct test_session_pair *pair)
{
	if (test_pair_active == pair)
		abort();
	*pair = (struct test_session_pair){.registered = true,
	    .previous = test_pair_active, .client_result = DSSH_ERROR_INIT,
	    .server_result = DSSH_ERROR_INIT};
	struct mock_io_pipe *pipes[] = { &pair->io.c2s, &pair->io.s2c };
	for (size_t i = 0; i < sizeof(pipes) / sizeof(pipes[0]); i++) {
		atomic_init(&pipes[i]->rfd, -1);
		atomic_init(&pipes[i]->wfd, -1);
		atomic_init(&pipes[i]->read_closed, true);
		atomic_init(&pipes[i]->write_closed, true);
	}
	test_pair_active = pair;
}

static inline void
test_pair_cancel(struct test_session_pair *pair)
{
	if (pair->io_ready) {
		mock_io_close_c2s(&pair->io);
		mock_io_close_s2c(&pair->io);
	}
	if (pair->client != NULL)
		dssh_session_terminate(pair->client);
	if (pair->server != NULL)
		dssh_session_terminate(pair->server);
}

static inline void
test_pair_join(struct test_session_pair *pair)
{
	for (unsigned i = 0; i < 2; i++) {
		if (!pair->owned[i])
			continue;
		/* This is the actual joinable handle owned by this fixture. A failed
		 * native join leaves ownership unknown; no retry or destruction is safe. */
		if (thrd_join(pair->threads[i], NULL) != thrd_success)
			abort();
		pair->owned[i] = false;
		pair->joined++;
	}
}

static inline void
test_pair_destroy(struct test_session_pair *pair)
{
	if (!pair->registered)
		return;
	if (test_pair_active != pair)
		abort();
	test_pair_cancel(pair);
	test_pair_join(pair);
	dssh_session server = pair->server;
	dssh_session client = pair->client;
	pair->server = NULL;
	pair->client = NULL;
	dssh_session_cleanup(server);
	dssh_session_cleanup(client);
	if (pair->io_ready) {
		pair->io_ready = false;
		mock_io_free(&pair->io);
	}
	test_pair_active = pair->previous;
	pair->previous = NULL;
	pair->registered = false;
}

static inline void
test_pair_destroy_active(void)
{
	while (test_pair_active != NULL)
		test_pair_destroy(test_pair_active);
}

static inline int
test_pair_open(struct test_session_pair *pair)
{
	if (mock_io_init(&pair->io, 0) < 0)
		return DSSH_ERROR_INIT;
	pair->io_ready = true;
	pair->client = dssh_session_init(true, 0);
	if (pair->client == NULL)
		return DSSH_ERROR_ALLOC;
	int ret = dssh_session_set_cbdata(pair->client, &pair->io, &pair->io, &pair->io, &pair->io);
	if (ret == 0)
		ret = dssh_session_set_hostkey_verify_cb(pair->client, dssh_test_accept_hostkey, NULL);
	if (ret < 0)
		return ret;
	pair->server = dssh_session_init(false, 0);
	if (pair->server == NULL)
		return DSSH_ERROR_ALLOC;
	return dssh_session_set_cbdata(pair->server, &pair->io, &pair->io, &pair->io, &pair->io);
}

static int
test_pair_worker_run(void *opaque)
{
	struct test_pair_worker *worker = opaque;
	struct test_session_pair *pair = worker->pair;
	dssh_session sess = worker->client ? pair->client : pair->server;
	int ret = pair->operation(sess);
	if (worker->client)
		pair->client_result = ret;
	else
		pair->server_result = ret;
	if (ret != 0) {
		mock_io_close_c2s(&pair->io);
		mock_io_close_s2c(&pair->io);
	}
	return 0;
}

static inline int
test_pair_start(struct test_session_pair *pair, unsigned index)
{
	if (index >= 2 || pair->owned[index] || pair->operation == NULL)
		return DSSH_ERROR_INVALID;
	pair->worker[index] = (struct test_pair_worker){.pair = pair, .client = index == 0};
	if (thrd_create(&pair->threads[index], test_pair_worker_run, &pair->worker[index]) != thrd_success)
		return DSSH_ERROR_INIT;
	pair->owned[index] = true;
	pair->created++;
	return 0;
}

static inline int
test_pair_run(struct test_session_pair *pair, int (*operation)(dssh_session))
{
	pair->operation = operation;
	for (unsigned i = 0; i < 2; i++) {
		int ret = test_pair_start(pair, i);
		if (ret < 0) {
			test_pair_cancel(pair);
			test_pair_join(pair);
			return ret;
		}
	}
	test_pair_join(pair);
	if (pair->client_result != 0)
		return pair->client_result;
	return pair->server_result;
}

#endif
