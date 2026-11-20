#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "dssh_test.h"
#include "dssh_test_alloc.h"
#include "deucessh.h"
#include "kex/workspace.h"

void
dssh_test_failure_cleanup(void)
{
	dssh_test_alloc_reset();
}

static bool
bytes_are(const uint8_t *data, size_t size, uint8_t value)
{
	for (size_t i = 0; i < size; i++) {
		if (data[i] != value)
			return false;
	}
	return true;
}

static int
test_cycle(void)
{
	struct dssh_kex_workspace *workspace = NULL;
	ASSERT_OK(dssh_kex_workspace_create(2, &workspace));
	uint8_t *first = NULL, *second = NULL, *secret = NULL, *hash = NULL;
	ASSERT_OK(dssh_kex_workspace_get(workspace, 0, 64, &first));
	memset(first, 0x41, 64);
	ASSERT_OK(dssh_kex_workspace_drop(workspace, 0));
	ASSERT_TRUE(bytes_are(first, 64, 0));
	uint8_t *smaller = NULL;
	ASSERT_OK(dssh_kex_workspace_get(workspace, 0, 8, &smaller));
	ASSERT_TRUE(smaller == first && bytes_are(smaller, 64, 0));
	memset(smaller, 0x42, 8);
	ASSERT_OK(dssh_kex_workspace_get(workspace, 1, 32, &second));
	memset(second, 0x43, 32);
	struct dssh_result_lease lease = {0};
	ASSERT_OK(dssh_kex_workspace_result(workspace, 64, 32, &lease, &secret, &hash));
	ASSERT_TRUE(secret != hash && secret != first && hash != second);
	memset(secret, 0x51, 64);
	memset(hash, 0x52, 32);
	ASSERT_EQ(dssh_kex_workspace_reset(workspace), DSSH_ERROR_INVALID);
	struct dssh_result_lease rejected = {0};
	uint8_t *unused = NULL, *other = NULL;
	ASSERT_EQ(dssh_kex_workspace_result(workspace, 1, 1, &rejected, &unused, &other), DSSH_ERROR_INVALID);
	ASSERT_NULL(unused);
	ASSERT_NULL(other);
	ASSERT_NULL(rejected.release);
	/* Scratch cleanup cannot invalidate a result that is still being produced. */
	ASSERT_OK(dssh_kex_workspace_drop(workspace, 0));
	ASSERT_TRUE(bytes_are(secret, 64, 0x51) && bytes_are(hash, 32, 0x52));
	dssh_result_release(&lease);
	dssh_result_release(&lease);
	ASSERT_TRUE(bytes_are(secret, 64, 0) && bytes_are(hash, 32, 0));
	ASSERT_TRUE(bytes_are(second, 32, 0x43));
	ASSERT_OK(dssh_kex_workspace_reset(workspace));
	ASSERT_TRUE(bytes_are(first, 64, 0) && bytes_are(second, 32, 0));
	uint8_t *reused_secret = NULL, *reused_hash = NULL;
	dssh_test_alloc_fail_after(0);
	int ret = dssh_kex_workspace_result(workspace, 8, 8, &lease, &reused_secret, &reused_hash);
	int scratch = dssh_kex_workspace_get(workspace, 0, 8, &smaller);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_OK(ret);
	ASSERT_OK(scratch);
	ASSERT_EQ(allocations, 0);
	ASSERT_TRUE(reused_secret == secret && reused_hash == hash && smaller == first);
	memset(reused_secret, 0x61, 8);
	memset(reused_hash, 0x62, 8);
	/* The consumer's lengths are not supplied to storage cleanup. */
	struct dssh_bytes reported_secret = {reused_secret, SIZE_MAX};
	struct dssh_bytes reported_hash = {reused_hash, 0};
	dssh_result_release(&lease);
	ASSERT_TRUE(bytes_are(reported_secret.data, 64, 0) && bytes_are(reported_hash.data, 32, 0));
	dssh_kex_workspace_destroy(workspace);
	return TEST_PASS;
}

struct watched_storage {
	void *pointer;
	size_t size;
	unsigned calls;
	bool cleared;
};

struct free_probe {
	struct watched_storage items[4];
	unsigned unexpected;
};

static void
observe_free(const void *storage, void *data)
{
	struct free_probe *probe = data;
	for (size_t i = 0; i < sizeof(probe->items) / sizeof(probe->items[0]); i++) {
		struct watched_storage *item = &probe->items[i];
		if (item->pointer != NULL && storage == item->pointer) {
			item->calls++;
			item->cleared = item->size == 0 || bytes_are(storage, item->size, 0);
			item->pointer = NULL;
			return;
		}
	}
	probe->unexpected++;
}

struct release_worker {
	struct dssh_result_lease *lease;
	struct free_probe *probe;
};

static int
release_on_worker(void *data)
{
	struct release_worker *worker = data;
	dssh_test_alloc_observe_frees(observe_free, worker->probe);
	dssh_result_release(worker->lease);
	dssh_result_release(worker->lease);
	dssh_test_alloc_observe_frees(NULL, NULL);
	return 0;
}

static int
test_retirement(void)
{
	struct dssh_kex_workspace *workspace = NULL;
	ASSERT_OK(dssh_kex_workspace_create(2, &workspace));
	uint8_t *scratch = NULL, *secret = NULL, *hash = NULL;
	ASSERT_OK(dssh_kex_workspace_get(workspace, 0, 96, &scratch));
	struct dssh_result_lease lease = {0};
	ASSERT_OK(dssh_kex_workspace_result(workspace, 64, 32, &lease, &secret, &hash));
	memset(scratch, 0x31, 96);
	memset(secret, 0x32, 64);
	memset(hash, 0x33, 32);
	struct free_probe probe = {.items = {
		{scratch, 96, 0, false}, {secret, 64, 0, false},
		{hash, 32, 0, false}, {workspace, 0, 0, false},
	}};
	dssh_test_alloc_observe_frees(observe_free, &probe);
	dssh_kex_workspace_destroy(workspace);
	dssh_test_alloc_observe_frees(NULL, NULL);
	bool valid = probe.items[0].calls == 1 && probe.items[0].cleared
	    && probe.items[1].calls == 0 && probe.items[2].calls == 0 && probe.items[3].calls == 0
	    && bytes_are(secret, 64, 0x32) && bytes_are(hash, 32, 0x33);
	valid = valid && dssh_kex_workspace_reset(workspace) == DSSH_ERROR_INVALID
	    && dssh_kex_workspace_drop(workspace, 0) == DSSH_ERROR_INVALID;
	uint8_t *unused = NULL, *other = NULL;
	struct dssh_result_lease rejected = {0};
	valid = valid && dssh_kex_workspace_get(workspace, 0, 1, &unused) == DSSH_ERROR_INVALID
	    && dssh_kex_workspace_result(workspace, 1, 1, &rejected, &unused, &other) == DSSH_ERROR_INVALID;
	/* Retirement is serialized before this thread starts; a live lease keeps
	 * the result owner alive until the worker finishes its final release. */
	struct release_worker worker = {&lease, &probe};
	thrd_t thread;
	if (thrd_create(&thread, release_on_worker, &worker) == thrd_success) {
		if (thrd_join(thread, NULL) != thrd_success)
			abort();
	}
	else {
		valid = false;
		dssh_test_alloc_observe_frees(observe_free, &probe);
		dssh_result_release(&lease);
		dssh_test_alloc_observe_frees(NULL, NULL);
	}
	for (size_t i = 0; i < sizeof(probe.items) / sizeof(probe.items[0]); i++)
		valid = valid && probe.items[i].calls == 1 && probe.items[i].cleared;
	valid = valid && probe.unexpected == 0 && lease.release == NULL && lease.owner == NULL;
	dssh_test_alloc_reset();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_failures(void)
{
	struct dssh_kex_workspace *workspace = NULL;
	dssh_test_alloc_fail_after(0);
	int ret = dssh_kex_workspace_create(1, &workspace);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_NULL(workspace);
	ASSERT_OK(dssh_kex_workspace_create(1, &workspace));
	uint8_t marker = 0x7b;
	uint8_t *secret = &marker, *hash = &marker;
	struct dssh_result_lease lease = {0};
	dssh_test_alloc_fail_after(0);
	ret = dssh_kex_workspace_result(workspace, 16, 32, &lease, &secret, &hash);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_TRUE(secret == &marker && hash == &marker && marker == 0x7b);
	dssh_test_alloc_fail_after(1);
	ret = dssh_kex_workspace_result(workspace, 16, 32, &lease, &secret, &hash);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_EQ(allocations, 2);
	ASSERT_NULL(lease.release);
	ASSERT_NULL(lease.owner);
	ASSERT_TRUE(secret == &marker && hash == &marker);
	/* The successful first growth remains cached when the second fails. */
	dssh_test_alloc_fail_after(0);
	ret = dssh_kex_workspace_result(workspace, 16, 32, &lease, &secret, &hash);
	allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_EQ(allocations, 1);
	ASSERT_OK(dssh_kex_workspace_result(workspace, 16, 32, &lease, &secret, &hash));
	memset(secret, 0x41, 16);
	memset(hash, 0x42, 32);
	dssh_result_release(&lease);
	uint8_t *larger_secret = &marker, *larger_hash = &marker;
	dssh_test_alloc_fail_after(0);
	ret = dssh_kex_workspace_result(workspace, 128, 256, &lease, &larger_secret, &larger_hash);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_TRUE(larger_secret == &marker && larger_hash == &marker);
	uint8_t *reused_secret = NULL, *reused_hash = NULL;
	dssh_test_alloc_fail_after(0);
	ret = dssh_kex_workspace_result(workspace, 16, 32, &lease, &reused_secret, &reused_hash);
	allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_OK(ret);
	ASSERT_EQ(allocations, 0);
	ASSERT_TRUE(reused_secret == secret && reused_hash == hash);
	dssh_result_release(&lease);
	struct free_probe probe = {.items = {{secret, 16, 0, false}}};
	dssh_test_alloc_fail_after(1);
	dssh_test_alloc_observe_frees(observe_free, &probe);
	ret = dssh_kex_workspace_result(workspace, 128, 256, &lease, &larger_secret, &larger_hash);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_TRUE(probe.items[0].calls == 1 && probe.items[0].cleared && probe.unexpected == 0);
	ASSERT_TRUE(larger_secret == &marker && larger_hash == &marker);
	dssh_test_alloc_fail_after(0);
	ret = dssh_kex_workspace_result(workspace, 128, 32, &lease, &larger_secret, &larger_hash);
	allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_OK(ret);
	ASSERT_EQ(allocations, 0);
	ASSERT_TRUE(larger_hash == hash && bytes_are(larger_secret, 128, 0));
	memset(larger_secret, 0x51, 128);
	memset(larger_hash, 0x52, 32);
	dssh_result_release(&lease);
	ASSERT_TRUE(bytes_are(larger_secret, 128, 0) && bytes_are(larger_hash, 32, 0));
	uint8_t *scratch = &marker;
	dssh_test_alloc_fail_after(0);
	ret = dssh_kex_workspace_get(workspace, 0, 64, &scratch);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_TRUE(scratch == &marker && marker == 0x7b);
	ASSERT_OK(dssh_kex_workspace_get(workspace, 0, 64, &scratch));
	memset(scratch, 0x61, 64);
	uint8_t *saved = scratch;
	dssh_test_alloc_fail_after(0);
	ret = dssh_kex_workspace_get(workspace, 0, 128, &scratch);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_TRUE(scratch == saved && bytes_are(scratch, 64, 0x61));
	ASSERT_OK(dssh_kex_workspace_drop(workspace, 0));
	ASSERT_OK(dssh_kex_workspace_reset(workspace));
	dssh_kex_workspace_destroy(workspace);
	return TEST_PASS;
}

static void
no_release(void *owner)
{
	(void)owner;
}

static int
test_invalid(void)
{
	struct dssh_kex_workspace *workspace = NULL;
	ASSERT_EQ(dssh_kex_workspace_create(0, NULL), DSSH_ERROR_INVALID);
	dssh_test_alloc_fail_after(0);
	int ret = dssh_kex_workspace_create(SIZE_MAX, &workspace);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_INVALID);
	ASSERT_EQ(allocations, 0);
	ASSERT_NULL(workspace);
	ASSERT_OK(dssh_kex_workspace_create(0, &workspace));
	ASSERT_EQ(dssh_kex_workspace_create(0, &workspace), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_reset(NULL), DSSH_ERROR_INVALID);
	ASSERT_OK(dssh_kex_workspace_reset(workspace));
	uint8_t *secret = NULL, *hash = NULL;
	struct dssh_result_lease lease = {0};
	ASSERT_EQ(dssh_kex_workspace_get(workspace, 0, 1, &secret), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_drop(workspace, 0), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_drop(NULL, 0), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_get(NULL, 0, 1, &secret), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_result(NULL, 1, 1, &lease, &secret, &hash), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_result(workspace, 0, 1, &lease, &secret, &hash), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_result(workspace, 1, 0, &lease, &secret, &hash), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_result(workspace, 1, 1, NULL, &secret, &hash), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_result(workspace, 1, 1, &lease, NULL, &hash), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_result(workspace, 1, 1, &lease, &secret, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_result(workspace, 1, 1, &lease, &secret, &secret), DSSH_ERROR_INVALID);
	lease.owner = workspace;
	ASSERT_EQ(dssh_kex_workspace_result(workspace, 1, 1, &lease, &secret, &hash), DSSH_ERROR_INVALID);
	lease.owner = NULL;
	lease.release = no_release;
	ASSERT_EQ(dssh_kex_workspace_result(workspace, 1, 1, &lease, &secret, &hash), DSSH_ERROR_INVALID);
	lease.release = NULL;
	ASSERT_NULL(secret);
	ASSERT_NULL(hash);
	dssh_kex_workspace_destroy(workspace);
	workspace = NULL;
	ASSERT_OK(dssh_kex_workspace_create(1, &workspace));
	ASSERT_OK(dssh_kex_workspace_drop(workspace, 0));
	ASSERT_EQ(dssh_kex_workspace_get(workspace, 0, 0, &secret), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_get(workspace, 0, 1, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_kex_workspace_get(workspace, SIZE_MAX, 1, &secret), DSSH_ERROR_INVALID);
	dssh_kex_workspace_destroy(workspace);
	dssh_kex_workspace_destroy(NULL);
	return TEST_PASS;
}

#define WORKERS 8
struct session_worker {
	struct dssh_kex_workspace *workspace;
	struct dssh_result_lease lease;
	uint8_t *secret;
	uint8_t *hash;
	unsigned index;
	bool valid;
};

static int
session_round(void *data)
{
	struct session_worker *worker = data;
	uint8_t *scratch = NULL;
	worker->valid = dssh_kex_workspace_reset(worker->workspace) == 0
	    && dssh_kex_workspace_get(worker->workspace, 0, 32, &scratch) == 0
	    && dssh_kex_workspace_result(worker->workspace, 64, 32, &worker->lease,
	        &worker->secret, &worker->hash) == 0;
	if (worker->valid) {
		uint8_t pattern = (uint8_t)(worker->index + 1); /* index is below WORKERS. */
		worker->valid = bytes_are(worker->secret, 64, 0) && bytes_are(worker->hash, 32, 0)
		    && bytes_are(scratch, 32, 0);
		memset(worker->secret, pattern, 64);
		memset(worker->hash, pattern, 32);
		memset(scratch, pattern, 32);
	}
	if (dssh_kex_workspace_drop(worker->workspace, 0) != 0)
		worker->valid = false;
	/* The parent consumes/releases the result after this producer exits. */
	return 0;
}

static int
test_independent_sessions(void)
{
	struct session_worker workers[WORKERS] = {0};
	unsigned count = 0;
	bool valid = true;
	for (unsigned i = 0; i < WORKERS; i++) {
		if (dssh_kex_workspace_create(1, &workers[i].workspace) != 0) {
			valid = false;
			break;
		}
		workers[i].index = i;
		count++;
	}
	for (unsigned round = 0; valid && round < 3; round++) {
		if (round != 0)
			dssh_test_alloc_fail_after(0);
		thrd_t threads[WORKERS];
		unsigned started = 0;
		for (unsigned i = 0; i < count; i++) {
			if (thrd_create(&threads[i], session_round, &workers[i]) != thrd_success) {
				valid = false;
				break;
			}
			started++;
		}
		for (unsigned i = 0; i < started; i++) {
			if (thrd_join(threads[i], NULL) != thrd_success)
				abort();
		}
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		if (round != 0)
			valid = valid && allocations == 0;
		for (unsigned i = 0; i < started; i++) {
			struct session_worker *worker = &workers[i];
			uint8_t pattern = (uint8_t)(i + 1);
			valid = valid && worker->valid && bytes_are(worker->secret, 64, pattern)
			    && bytes_are(worker->hash, 32, pattern);
			for (unsigned j = 0; j < i; j++)
				valid = valid && worker->secret != workers[j].secret && worker->hash != workers[j].hash;
		}
		for (unsigned i = 0; i < count; i++)
			dssh_result_release(&workers[i].lease);
	}
	for (unsigned i = 0; i < count; i++) {
		dssh_result_release(&workers[i].lease);
		dssh_kex_workspace_destroy(workers[i].workspace);
	}
	ASSERT_TRUE(valid && count == WORKERS);
	return TEST_PASS;
}

static int
test_state_adapter(void)
{
	void *state = NULL;
	ASSERT_EQ(dssh_kex_workspace_init_slots(2, NULL), DSSH_ERROR_INVALID);
	dssh_test_alloc_fail_after(0);
	int ret = dssh_kex_workspace_init_slots(2, &state);
	dssh_test_alloc_reset();
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_NULL(state);
	ASSERT_OK(dssh_kex_workspace_init_slots(2, &state));
	void *original = state;
	uint8_t *scratch = NULL, *secret = NULL, *hash = NULL;
	ASSERT_OK(dssh_kex_workspace_get(state, 1, 48, &scratch));
	memset(scratch, 0x21, 48);
	ASSERT_EQ(dssh_kex_workspace_init_slots(1, &state), DSSH_ERROR_INVALID);
	ASSERT_TRUE(state == original && bytes_are(scratch, 48, 0x21));
	struct dssh_result_lease lease = {0};
	ASSERT_OK(dssh_kex_workspace_result(state, 32, 32, &lease, &secret, &hash));
	memset(secret, 0x31, 32);
	memset(hash, 0x32, 32);
	ASSERT_EQ(dssh_kex_workspace_init_slots(2, &state), DSSH_ERROR_INVALID);
	ASSERT_TRUE(state == original && bytes_are(secret, 32, 0x31));
	dssh_result_release(&lease);
	dssh_test_alloc_fail_after(0);
	ret = dssh_kex_workspace_init_slots(2, &state);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_OK(ret);
	ASSERT_EQ(allocations, 0);
	ASSERT_TRUE(state == original && bytes_are(scratch, 48, 0));
	dssh_kex_workspace_cleanup(state);
	dssh_kex_workspace_cleanup(NULL);
	return TEST_PASS;
}

static struct dssh_test_entry tests[] = {
	{"workspace/cycle", test_cycle},
	{"workspace/retirement", test_retirement},
	{"workspace/failures", test_failures},
	{"workspace/invalid", test_invalid},
	{"workspace/independent_sessions", test_independent_sessions},
	{"workspace/state_adapter", test_state_adapter},
};

DSSH_TEST_MAIN(tests)
