/*
 * dssh_test_ossl.h -- OpenSSL + C11 threads failure injection.
 *
 * Under DSSH_TESTING, library calls to OpenSSL and C11 thread
 * functions are redirected via macros in ssh-internal.h.  This
 * header declares the wrapper functions and control API.
 *
 * The wrappers use a shared atomic countdown: each failable call
 * decrements the counter, and when it reaches zero, returns the
 * appropriate failure value (NULL, 0, thrd_error).  Non-failable
 * calls (free, destroy, cleanse) are not wrapped.
 */

#ifndef DSSH_TEST_OSSL_H
#define DSSH_TEST_OSSL_H

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <threads.h>
#include <time.h>

/*
 * Control API -- call from test code (NOT redirected).
 */
void dssh_test_ossl_reset(void);
void dssh_test_ossl_fail_after(int n);
void dssh_test_ossl_exclude_thread(void);
int  dssh_test_ossl_count(void);

/* Thread-local reported-metadata overrides for helper boundary tests.
 * Real computation still runs; reset disables the overrides. */
void dssh_test_ossl_md_size(int value);
void dssh_test_ossl_digest_length(unsigned int value);
void dssh_test_ossl_encoded_length(int value);
void dssh_test_ossl_encrypt_length(int value);
bool dssh_test_ossl_encrypt_observed(int *actual_length);
void dssh_test_ossl_mac_length(size_t value);
bool dssh_test_ossl_mac_observed(size_t *actual_length);
/* Observe the actual private RNG request. Post-failure runs native generation
 * first and reports failure afterward; it does not promise output rollback. */
void dssh_test_ossl_random_post_failure(bool enabled);
bool dssh_test_ossl_random_observed(size_t *length, unsigned int *strength, bool *native_completed);

/* Observe only the named private BN call on this thread. Mode 0 forwards,
 * 1 refuses before native work, 2 reports failure after native work. End(true)
 * checks numeric clearing before native-owner cleanup. End(false) only returns
 * recorded observations and drops the borrow; it never inspects the result. */
struct dssh_test_bn_random_stats {
	unsigned calls;
	unsigned int strength;
	int range_bits;
	bool context_present;
	bool constant_time;
	bool native_called;
	bool native_completed;
	bool in_range;
	bool cleared;
};
void dssh_test_ossl_bn_random_begin(unsigned mode);
struct dssh_test_bn_random_stats dssh_test_ossl_bn_random_end(bool inspect_result);

/* Current-thread observations of actual sntrup clears, before owner expiry.
 * No owner address is retained. Counts saturate; end disables observation. */
struct dssh_test_sntrup_clear_stats {
	size_t calls;
	size_t matches;
	bool zero;
};
void dssh_test_sntrup_clear_begin(size_t matching_size);
struct dssh_test_sntrup_clear_stats dssh_test_sntrup_clear_end(void);
void dssh_test_sntrup_cleared(const void *buffer, size_t length);

/*
 * Separate countdown for lock/condvar wrappers and the legacy join hook.
 * Independent of the OpenSSL + init countdown.
 * dssh_test_ossl_reset() resets both countdowns.
 */
void dssh_test_thrd_fail_after(int n);
void dssh_test_thrd_reset(void);
int  dssh_test_thrd_count(void);
/* Target join independently of earlier termination/wake operations. */
void dssh_test_join_fail_next(void);
int dssh_test_join_count(void);

/* Reject one acquisition of this mutex on the current thread. Record and
 * suppress any subsequent unlock without a successful reacquisition: the
 * fixture must never invoke a native unlock on an unowned mutex. */
void dssh_test_mtx_reject_once(mtx_t *mutex);
unsigned dssh_test_mtx_rejected_unlocks(void);
/* Current-thread, named-mutex trylock control. Default calls remain native and
 * do not consume the general lock countdown. Failure is before acquisition;
 * a native observation can establish actual contention on a valid mutex. */
struct dssh_test_trylock_stats {
	unsigned attempts;
	unsigned native_calls;
	int last_result;
	bool injected;
};
void dssh_test_trylock_begin(mtx_t *mutex, bool fail_once);
struct dssh_test_trylock_stats dssh_test_trylock_end(void);
/* Fatal-path control, independent of the recoverable operation countdown.
 * skip is the number of successful matching releases before reporting failure.
 * release_real distinguishes retained ownership from error after real release. */
void dssh_test_unlock_fail_after(mtx_t *mutex, unsigned skip, bool release_real);

/* Target termination wake operations on the controlling thread. Report the
 * final selected failure for fresh-process provenance. complete_real chooses
 * failure before or after performing the native operation. */
void dssh_test_wake_lock(mtx_t *mutex, bool complete_real);
void dssh_test_wake_condition(cnd_t *condition, unsigned failures,
    bool complete_real, bool report);
unsigned dssh_test_wake_hits(void);

/* Observe entry into an actual wait wrapper for this condition. No callback
 * is invoked under the mutex. Configure/disable while no waiter can race a
 * reconfiguration; the returned flag has process lifetime. */
atomic_bool *dssh_test_thrd_watch_condition(cnd_t *condition);

/* Pause a matching native acquisition before calling mtx_lock. Configure and
 * disable only while no worker can race reconfiguration; resume before join. */
atomic_bool *dssh_test_thrd_pause_mutex(mtx_t *mutex);
/* Current-thread-only pause after skip matching acquisitions. */
atomic_bool *dssh_test_thrd_pause_mutex_after(mtx_t *mutex, unsigned skip);
void dssh_test_thrd_resume_mutex(void);

/* Target one valid condition wait on this thread. The first matching call
 * reports result, optionally after a real native wait/reacquisition. A test
 * controller acknowledges returning only after observing the owned mutex.
 * Later matching calls run normally and count repeated predicate waits.
 * Only the waiter writes non-atomic release statistics; read them after join.
 * Initialize all fields and keep the record live until that waiter joins. */
struct dssh_test_wait_probe {
	cnd_t *condition;
	mtx_t *mutex;
	atomic_uint calls;
	atomic_uint native_calls;
	atomic_int native_result;
	atomic_bool returning;
	atomic_bool release;
	unsigned releases;
	int unlock_result;
	int result;
	bool complete_real;
	bool until_timeout;
	bool timed;
};
void dssh_test_wait_probe(struct dssh_test_wait_probe *probe);

/* Passive current-thread observation of actual timed native waits. NULL target
 * accepts any condition except exclude. Both reset functions end/clear this scope.
 * No callbacks or native operations run
 * while recording. End drops both condition borrows before owner retirement. */
struct dssh_test_wait_observation {
	bool native;
	bool timed_out;
	bool failed;
};
void dssh_test_wait_observe_begin(const cnd_t *target, const cnd_t *exclude);
struct dssh_test_wait_observation dssh_test_wait_observe_end(void);

/* First result at the selected actual connection wait site on this thread.
 * No callback, pointer borrow, allocation, status override or native operation.
 * End and both reset functions disable/clear the observation. */
enum dssh_test_channel_stage {
	DSSH_TEST_CHANNEL_OPEN_WAIT,
	DSSH_TEST_CHANNEL_ACCEPT_WAIT
};
struct dssh_test_channel_observation {
	bool seen;
	int result;
};
void dssh_test_channel_observe_begin(enum dssh_test_channel_stage stage);
int dssh_test_channel_observe_result(enum dssh_test_channel_stage stage, int result);
struct dssh_test_channel_observation dssh_test_channel_observe_end(void);
/* Expiry must belong to the selected operation, not a later cleanup wait. */
bool dssh_test_channel_expired(struct dssh_test_wait_observation native,
    struct dssh_test_channel_observation channel);

/* Observe wrapped library ownership of one mutex on this thread. Configure
 * with no ownership of that mutex; native test/controller calls are not counted.
 * No callback or native mutex operation occurs when querying the flag. */
void dssh_test_mtx_watch_ownership(mtx_t *mutex);
bool dssh_test_mtx_ownership(void);

/* Independent per-thread controls for the library UTC clock. */
void dssh_test_clock_reset(void);
void dssh_test_clock_fail_after(int n);
void dssh_test_clock_set(struct timespec value);
/* Return first for the first count UTC reads, then second, on this thread. */
void dssh_test_clock_schedule(struct timespec first, unsigned count, struct timespec second);
int dssh_test_clock_count(void);

#include "dssh_test_key_file.h"

#endif /* DSSH_TEST_OSSL_H */
