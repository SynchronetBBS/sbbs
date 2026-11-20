/* Private allocation-test owner. Caller and worker frames stay live until all
 * successfully created actors join. No initialized synchronization is copied. */
#ifndef DSSH_TEST_ALLOCATION_PAIR_FIXTURE_H
#define DSSH_TEST_ALLOCATION_PAIR_FIXTURE_H

#include "session_pair_fixture.h"
#include "../ssh-time.h"

/* Explicit private injection adapters. Test consumers are compiled without
 * DSSH_TESTING; library-only macro redirection must not cover fixture storage. */
int dssh_test_mtx_init(mtx_t *mutex, int type);
int dssh_test_cnd_init(cnd_t *condition);
int dssh_test_mtx_lock(mtx_t *mutex);
int dssh_test_mtx_unlock(mtx_t *mutex);
int dssh_test_cnd_wait(cnd_t *condition, mtx_t *mutex);
int dssh_test_cnd_timedwait(cnd_t *condition, mtx_t *mutex, const struct timespec *deadline);
int dssh_test_cnd_broadcast(cnd_t *condition);
int dssh_test_cnd_signal(cnd_t *condition);
int dssh_test_timespec_get(struct timespec *ts, int base);

struct allocation_gate {
	mtx_t mutex;
	cnd_t condition;
	bool mutex_ready;
	bool condition_ready;
	unsigned arrived;
	bool released;
	bool cancelled;
};

struct allocation_watch {
	mtx_t mutex;
	cnd_t condition;
	bool mutex_ready;
	bool condition_ready;
	bool cancelled;
	bool fired;
	struct timespec deadline;
	thrd_t thread;
	bool owned;
	unsigned created;
	unsigned joined;
};

struct allocation_pair {
	struct test_session_pair pair;
	struct allocation_gate gate;
	struct allocation_watch watch;
	dssh_channel channels[2];
	unsigned server_rounds;
	unsigned client_rounds;
	unsigned prompt_published;
	unsigned prompt_released;
	unsigned response_published;
	unsigned response_released;
	int (*operation)(struct allocation_pair *, dssh_session);
	bool registered;
	struct allocation_pair *previous;
};

static struct allocation_pair *allocation_pair_active;

static inline void
allocation_sync_check(int result, const char *operation)
{
	if (result != thrd_success) {
		/* Process-isolated fixture provenance, never production diagnostics.
		 * A failed valid-owner operation has no safe retry/cleanup path. */
		fprintf(stderr, "ALLOCATION_SYNC_FAILURE operation=%s result=%d\n", operation, result);
		abort();
	}
}

static inline void
allocation_pair_begin(struct allocation_pair *owner)
{
	if (allocation_pair_active == owner)
		abort();
	*owner = (struct allocation_pair){.registered = true,
	    .previous = allocation_pair_active};
	allocation_pair_active = owner;
	test_pair_begin(&owner->pair);
}

static inline int
allocation_pair_sync_init(struct allocation_pair *owner)
{
	if (dssh_test_mtx_init(&owner->gate.mutex, mtx_plain) != thrd_success)
		return DSSH_ERROR_INIT;
	owner->gate.mutex_ready = true;
	if (dssh_test_cnd_init(&owner->gate.condition) != thrd_success)
		return DSSH_ERROR_INIT;
	owner->gate.condition_ready = true;
	if (dssh_test_mtx_init(&owner->watch.mutex, mtx_plain) != thrd_success)
		return DSSH_ERROR_INIT;
	owner->watch.mutex_ready = true;
	if (dssh_test_cnd_init(&owner->watch.condition) != thrd_success)
		return DSSH_ERROR_INIT;
	owner->watch.condition_ready = true;
	return 0;
}

static inline void
allocation_gate_cancel(struct allocation_gate *gate)
{
	if (!gate->condition_ready)
		return;
	allocation_sync_check(dssh_test_mtx_lock(&gate->mutex), "gate.cancel_lock");
	gate->cancelled = true;
	allocation_sync_check(dssh_test_cnd_broadcast(&gate->condition), "gate.cancel_notify");
	allocation_sync_check(dssh_test_mtx_unlock(&gate->mutex), "gate.cancel_unlock");
}

static inline bool
allocation_gate_enter(struct allocation_gate *gate)
{
	allocation_sync_check(dssh_test_mtx_lock(&gate->mutex), "gate.enter_lock");
	if (gate->arrived >= 2)
		abort();
	gate->arrived++;
	allocation_sync_check(dssh_test_cnd_broadcast(&gate->condition), "gate.arrive_notify");
	while (!gate->released && !gate->cancelled)
		allocation_sync_check(dssh_test_cnd_wait(&gate->condition, &gate->mutex), "gate.enter_wait");
	bool proceed = !gate->cancelled;
	allocation_sync_check(dssh_test_mtx_unlock(&gate->mutex), "gate.enter_unlock");
	return proceed;
}

static inline void
allocation_gate_ready(struct allocation_gate *gate)
{
	allocation_sync_check(dssh_test_mtx_lock(&gate->mutex), "gate.ready_lock");
	while (gate->arrived != 2)
		allocation_sync_check(dssh_test_cnd_wait(&gate->condition, &gate->mutex), "gate.ready_wait");
	allocation_sync_check(dssh_test_mtx_unlock(&gate->mutex), "gate.ready_unlock");
}

static inline void
allocation_gate_release(struct allocation_gate *gate)
{
	allocation_sync_check(dssh_test_mtx_lock(&gate->mutex), "gate.release_lock");
	gate->released = true;
	allocation_sync_check(dssh_test_cnd_broadcast(&gate->condition), "gate.release_notify");
	allocation_sync_check(dssh_test_mtx_unlock(&gate->mutex), "gate.release_unlock");
}

static inline void
allocation_watch_cancel(struct allocation_watch *watch)
{
	if (!watch->condition_ready)
		return;
	allocation_sync_check(dssh_test_mtx_lock(&watch->mutex), "watch.cancel_lock");
	watch->cancelled = true;
	allocation_sync_check(dssh_test_cnd_signal(&watch->condition), "watch.cancel_notify");
	allocation_sync_check(dssh_test_mtx_unlock(&watch->mutex), "watch.cancel_unlock");
}

static int
allocation_watch_worker(void *opaque)
{
	struct allocation_pair *owner = opaque;
	struct allocation_watch *watch = &owner->watch;
	allocation_sync_check(dssh_test_mtx_lock(&watch->mutex), "watch.enter_lock");
	while (!watch->cancelled) {
		int ret = dssh_test_cnd_timedwait(&watch->condition, &watch->mutex, &watch->deadline);
		if (ret == thrd_timedout)
			break;
		allocation_sync_check(ret, "watch.enter_wait");
	}
	watch->fired = !watch->cancelled;
	allocation_sync_check(dssh_test_mtx_unlock(&watch->mutex), "watch.enter_unlock");
	/* Termination/callbacks and native I/O happen after releasing the only
	 * fixture mutex; session handles remain owned until this actor joins. */
	if (watch->fired)
		test_pair_cancel(&owner->pair);
	return 0;
}

static inline int
allocation_watch_start(struct allocation_pair *owner, intmax_t delay_ms)
{
	struct allocation_watch *watch = &owner->watch;
	if (watch->owned || !watch->condition_ready || delay_ms < 0)
		return DSSH_ERROR_INVALID;
	if (dssh_test_timespec_get(&watch->deadline, TIME_UTC) != TIME_UTC)
		return DSSH_ERROR_INIT;
	if (delay_ms > 0 && !dssh_timespec_add_ms(&watch->deadline, delay_ms))
		return DSSH_ERROR_INIT;
	watch->cancelled = false;
	watch->fired = false;
	if (thrd_create(&watch->thread, allocation_watch_worker, owner) != thrd_success)
		return DSSH_ERROR_INIT;
	watch->owned = true;
	/* At most one watchdog is created per fixture/run. */
	if (watch->created == UINT_MAX)
		abort();
	watch->created++;
	return 0;
}

static inline void
allocation_watch_join(struct allocation_watch *watch)
{
	if (!watch->owned)
		return;
	if (thrd_join(watch->thread, NULL) != thrd_success)
		abort();
	watch->owned = false;
	watch->joined++;
}

static int
allocation_pair_worker(dssh_session sess)
{
	struct allocation_pair *owner = allocation_pair_active;
	if (owner == NULL || !allocation_gate_enter(&owner->gate))
		return DSSH_ERROR_TERMINATED;
	return owner->operation(owner, sess);
}

static inline int
allocation_pair_run(struct allocation_pair *owner,
    int (*operation)(struct allocation_pair *, dssh_session), int point,
    bool provider_fault, bool watchdog)
{
	/* Only the controller modifies these fields, before native create and
	 * after all previous actors join. Workers share the actual owned I/O. */
	owner->operation = operation;
	owner->gate.arrived = 0;
	owner->gate.released = false;
	owner->gate.cancelled = false;
	owner->pair.operation = allocation_pair_worker;
	int ret = watchdog ? allocation_watch_start(owner, 3000) : 0;
	for (unsigned i = 0; ret == 0 && i < 2; i++)
		ret = test_pair_start(&owner->pair, i);
	if (ret < 0) {
		allocation_gate_cancel(&owner->gate);
		allocation_watch_cancel(&owner->watch);
		test_pair_cancel(&owner->pair);
		test_pair_join(&owner->pair);
		allocation_watch_join(&owner->watch);
		return ret;
	}
	allocation_gate_ready(&owner->gate);
	if (point >= 0) {
		if (provider_fault)
			dssh_test_ossl_fail_after(point);
		else
			dssh_test_alloc_fail_after(point);
	}
	allocation_gate_release(&owner->gate);
	test_pair_join(&owner->pair);
	allocation_watch_cancel(&owner->watch);
	allocation_watch_join(&owner->watch);
	if (owner->pair.client_result != 0)
		return owner->pair.client_result;
	return owner->pair.server_result;
}

static inline void
allocation_pair_destroy(struct allocation_pair *owner)
{
	if (!owner->registered)
		return;
	if (allocation_pair_active != owner)
		abort();
	allocation_gate_cancel(&owner->gate);
	allocation_watch_cancel(&owner->watch);
	test_pair_cancel(&owner->pair);
	allocation_watch_join(&owner->watch);
	test_pair_join(&owner->pair);
	for (unsigned i = 0; i < 2; i++) {
		dssh_channel channel = owner->channels[i];
		owner->channels[i] = NULL;
		if (channel != NULL && dssh_chan_close(channel, -1) != 0)
			abort();
	}
	if (owner->gate.condition_ready) {
		cnd_destroy(&owner->gate.condition);
		owner->gate.condition_ready = false;
	}
	if (owner->gate.mutex_ready) {
		mtx_destroy(&owner->gate.mutex);
		owner->gate.mutex_ready = false;
	}
	if (owner->watch.condition_ready) {
		cnd_destroy(&owner->watch.condition);
		owner->watch.condition_ready = false;
	}
	if (owner->watch.mutex_ready) {
		mtx_destroy(&owner->watch.mutex);
		owner->watch.mutex_ready = false;
	}
	test_pair_destroy(&owner->pair);
	allocation_pair_active = owner->previous;
	owner->previous = NULL;
	owner->registered = false;
}

static inline bool
allocation_pair_retired(const struct allocation_pair *owner)
{
	return !owner->registered && !owner->pair.registered && !owner->pair.io_ready
	    && owner->pair.client == NULL && owner->pair.server == NULL
	    && owner->channels[0] == NULL && owner->channels[1] == NULL
	    && !owner->pair.owned[0] && !owner->pair.owned[1]
	    && owner->pair.created == owner->pair.joined
	    && !owner->gate.mutex_ready && !owner->gate.condition_ready
	    && !owner->watch.mutex_ready && !owner->watch.condition_ready && !owner->watch.owned
	    && owner->watch.created == owner->watch.joined
	    && atomic_load(&owner->pair.io.c2s.rfd) == -1 && atomic_load(&owner->pair.io.c2s.wfd) == -1
	    && atomic_load(&owner->pair.io.s2c.rfd) == -1 && atomic_load(&owner->pair.io.s2c.wfd) == -1
	    && allocation_pair_active == NULL && test_pair_active == NULL;
}

#endif
