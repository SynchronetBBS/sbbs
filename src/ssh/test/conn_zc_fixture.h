/* Private single-producer ZC fixture; borrow storage until session teardown. */
#ifndef DSSH_TEST_CONN_ZC_FIXTURE_H
#define DSSH_TEST_CONN_ZC_FIXTURE_H

struct conn_zc_fixture {
	uint8_t data[4096];
	size_t used;
	size_t expected;
	bool blocking;
	atomic_bool active;
	atomic_bool entered;
	atomic_bool complete;
	atomic_bool release;
	atomic_bool failed;
	atomic_bool close_waiting;
	atomic_bool close_done;
	bool close_active;
	bool retired;
	thrd_t closer;
	dssh_channel closing_channel;
	dssh_channel reserved;
	int close_result;
	unsigned cleanup_cancels;
};

enum conn_zc_stage {
	CONN_ZC_OPENED,
	CONN_ZC_RECEIVED,
	CONN_ZC_BLOCKED,
	CONN_ZC_CLOSING,
	CONN_ZC_RESERVED
};

/* Controls are configured before any fixture reader starts, and retired after
 * all readers join. Checkpoints and retirement observation run on the owner. */
static bool (*conn_zc_stage_control)(enum conn_zc_stage);
static void (*conn_zc_retirement_observer)(const struct conn_zc_fixture *);
static int (*conn_zc_clock)(clockid_t, struct timespec *) = clock_gettime;
static int64_t (*conn_zc_read)(dssh_channel, int, uint8_t *, size_t) = dssh_chan_read;
static int64_t (*conn_zc_write)(dssh_channel, int, const uint8_t *, size_t) = dssh_chan_write;
static int (*conn_zc_sleep)(const struct timespec *, struct timespec *) = thrd_sleep;

static bool
conn_zc_checkpoint(enum conn_zc_stage stage)
{
	return conn_zc_stage_control == NULL || conn_zc_stage_control(stage);
}

static bool
conn_zc_prepare(struct conn_zc_fixture *fixture,
    size_t expected, bool blocking)
{
	if (expected > sizeof(fixture->data))
		return false;
	fixture->used = 0;
	fixture->expected = expected;
	fixture->blocking = blocking;
	fixture->close_active = false;
	fixture->retired = false;
	fixture->closing_channel = NULL;
	fixture->reserved = NULL;
	fixture->close_result = 0;
	fixture->cleanup_cancels = 0;
	atomic_init(&fixture->active, false);
	atomic_init(&fixture->entered, false);
	atomic_init(&fixture->complete, false);
	atomic_init(&fixture->release, false);
	atomic_init(&fixture->failed, false);
	atomic_init(&fixture->close_waiting, false);
	atomic_init(&fixture->close_done, false);
	return true;
}

static bool
conn_zc_init(struct conn_ctx *ctx, struct conn_zc_fixture *fixture,
    size_t expected, bool blocking)
{
	if (ctx->zc_fixture != NULL || !conn_zc_prepare(fixture, expected, blocking))
		return false;
	ctx->zc_fixture = fixture;
	return true;
}

static bool
conn_zc_time_valid(const struct timespec *start, const struct timespec *now,
    unsigned seconds)
{
	if ((seconds != 1 && seconds != 5)
	    || start->tv_nsec < 0 || start->tv_nsec >= 1000000000L
	    || now->tv_nsec < 0 || now->tv_nsec >= 1000000000L)
		return false;
	double elapsed = difftime(now->tv_sec, start->tv_sec);
	double limit = seconds == 1 ? 1.0 : 5.0;
	return elapsed >= 0 && elapsed <= limit
	    && !(elapsed == 0 && now->tv_nsec < start->tv_nsec)
	    && !(elapsed == limit && now->tv_nsec >= start->tv_nsec);
}

static bool
conn_zc_start_clock(struct timespec *start, unsigned seconds)
{
	if (seconds != 1 && seconds != 5)
		return false;
	return conn_zc_clock(CLOCK_MONOTONIC, start) == 0
	    && start->tv_nsec >= 0 && start->tv_nsec < 1000000000L;
}

static bool
conn_zc_tick(const struct timespec *start, unsigned seconds)
{
	struct timespec now;
	const struct timespec pause = {.tv_nsec = 1000000};
	if (conn_zc_clock(CLOCK_MONOTONIC, &now) != 0
	    || !conn_zc_time_valid(start, &now, seconds))
		return false;
	int ret = conn_zc_sleep(&pause, NULL);
	return ret == 0 || ret == -1;
}

static bool
conn_zc_wait_flag(struct conn_zc_fixture *fixture,
    const atomic_bool *flag, unsigned seconds)
{
	struct timespec start;
	if (!conn_zc_start_clock(&start, seconds))
		return false;
	for (;;) {
		if (atomic_load(&fixture->failed)
		    || (flag == &fixture->close_waiting && atomic_load(&fixture->close_done)))
			return false;
		if (atomic_load_explicit(flag, memory_order_acquire))
			return true;
		if (!conn_zc_tick(&start, seconds))
			return false;
	}
}

static uint32_t
conn_zc_receive(dssh_channel channel, int stream, const uint8_t *data,
    size_t length, void *opaque)
{
	(void)channel;
	struct conn_zc_fixture *fixture = opaque;
	if (length > UINT32_MAX) {
		atomic_store(&fixture->failed, true);
		return 0;
	}
	uint32_t consumed = (uint32_t)length;
	if (atomic_exchange(&fixture->active, true)) {
		atomic_store(&fixture->failed, true);
		return consumed;
	}
	atomic_store_explicit(&fixture->entered, true, memory_order_release);
	if (fixture->blocking && !conn_zc_wait_flag(fixture, &fixture->release, 5))
		atomic_store(&fixture->failed, true);
	if (!atomic_load(&fixture->failed) && length != 0) {
		/* Completion freezes the buffer. Later calls can record an error
		 * but must never write storage already published to the owner. */
		if (stream != 0 || data == NULL || atomic_load(&fixture->complete)
		    || fixture->expected > sizeof(fixture->data)
		    || fixture->used > fixture->expected
		    || length > fixture->expected - fixture->used)
			atomic_store(&fixture->failed, true);
		else {
			memcpy(fixture->data + fixture->used, data, length);
			fixture->used += length;
			if (fixture->used == fixture->expected)
				atomic_store_explicit(&fixture->complete, true, memory_order_release);
		}
	}
	atomic_store_explicit(&fixture->active, false, memory_order_release);
	return consumed;
}

static bool
conn_zc_io_count(int64_t result, size_t capacity, size_t *count)
{
	if (result < 0)
		return false;
	uintmax_t positive = (uintmax_t)result;
	if (positive > capacity)
		return false;
	size_t converted = (size_t)positive;
	*count = converted;
	return true;
}

static bool
conn_zc_write_all(dssh_channel channel, const uint8_t *data, size_t length,
    unsigned seconds)
{
	struct timespec start;
	if (!conn_zc_start_clock(&start, seconds))
		return false;
	size_t used = 0;
	while (used < length) {
		int64_t ret = conn_zc_write(channel, 0, data + used, length - used);
		if (ret == DSSH_ERROR_NOMORE) {
			if (!conn_zc_tick(&start, seconds))
				return false;
			continue;
		}
		size_t count;
		if (!conn_zc_io_count(ret, length - used, &count) || count == 0)
			return false;
		used += count;
	}
	return true;
}

static bool
conn_zc_read_exact(dssh_channel channel, uint8_t *data, size_t length,
    unsigned seconds)
{
	struct timespec start;
	if (!conn_zc_start_clock(&start, seconds))
		return false;
	size_t used = 0;
	while (used < length) {
		int64_t ret = conn_zc_read(channel, 0, data + used, length - used);
		if (ret == DSSH_ERROR_NOMORE) {
			if (!conn_zc_tick(&start, seconds))
				return false;
			continue;
		}
		size_t count;
		if (!conn_zc_io_count(ret, length - used, &count) || count == 0)
			return false;
		used += count;
	}
	return true;
}

static int
conn_zc_reserve(struct conn_zc_fixture *fixture, dssh_channel channel,
    uint8_t **buffer, size_t *capacity)
{
	if (fixture->reserved != NULL)
		abort();
	struct timespec start;
	if (!conn_zc_start_clock(&start, 5))
		return DSSH_ERROR_INVALID;
	int ret;
	for (;;) {
		ret = dssh_chan_zc_getbuf(channel, 0, buffer, capacity);
		if (ret != DSSH_ERROR_NOMORE)
			break;
		if (!conn_zc_tick(&start, 5))
			return DSSH_ERROR_TIMEOUT;
	}
	if (ret == 0) {
		fixture->reserved = channel;
		if (!conn_zc_checkpoint(CONN_ZC_RESERVED))
			return DSSH_ERROR_INVALID;
	}
	return ret;
}

static int
conn_zc_send(struct conn_zc_fixture *fixture, size_t length)
{
	dssh_channel channel = fixture->reserved;
	if (channel == NULL)
		abort();
	int ret = dssh_chan_zc_send(channel, length);
	fixture->reserved = NULL;
	return ret;
}

static int
conn_zc_cancel(struct conn_zc_fixture *fixture)
{
	dssh_channel channel = fixture->reserved;
	if (channel == NULL)
		abort();
	int ret = dssh_chan_zc_cancel(channel);
	if (ret != 0)
		abort();
	fixture->reserved = NULL;
	return ret;
}

static int
conn_zc_close_worker(void *opaque)
{
	struct conn_zc_fixture *fixture = opaque;
	dssh_channel channel = fixture->closing_channel;
	fixture->closing_channel = NULL;
	dssh_test_detach_observe(&fixture->close_waiting);
	fixture->close_result = dssh_chan_close(channel, -1);
	dssh_test_detach_observe(NULL);
	atomic_store_explicit(&fixture->close_done, true, memory_order_release);
	return 0;
}

static int
conn_zc_start_close(struct conn_ctx *ctx, dssh_channel *channel)
{
	struct conn_zc_fixture *fixture = ctx->zc_fixture;
	if (fixture == NULL || fixture->close_active || fixture->closing_channel != NULL)
		abort();
	fixture->closing_channel = *channel;
	int ret = channel_fixture_thread_create(&fixture->closer, conn_zc_close_worker, fixture);
	if (ret != thrd_success) {
		fixture->closing_channel = NULL;
		dssh_session_terminate(ctx->client);
		dssh_session_terminate(ctx->server);
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
		return -1;
	}
	fixture->close_active = true;
	*channel = NULL;
	return 0;
}

static void
conn_zc_release(struct conn_ctx *ctx)
{
	struct conn_zc_fixture *fixture = ctx->zc_fixture;
	if (fixture == NULL)
		return;
	atomic_store_explicit(&fixture->release, true, memory_order_release);
	if (fixture->reserved != NULL) {
		conn_zc_cancel(fixture);
		if (fixture->cleanup_cancels == UINT_MAX)
			abort();
		fixture->cleanup_cancels++;
	}
}

static void
conn_zc_join(struct conn_ctx *ctx)
{
	struct conn_zc_fixture *fixture = ctx->zc_fixture;
	if (fixture == NULL || !fixture->close_active)
		return;
	channel_fixture_owner = ctx;
	if (channel_fixture_thread_join(fixture->closer, NULL) != thrd_success)
		abort();
	fixture->close_active = false;
	channel_fixture_owner = NULL;
}

static void
conn_zc_retire(struct conn_ctx *ctx)
{
	struct conn_zc_fixture *fixture = ctx->zc_fixture;
	if (fixture == NULL)
		return;
	if (ctx->client != NULL || ctx->server != NULL || fixture->close_active
	    || fixture->reserved != NULL || atomic_load(&fixture->active))
		abort();
	ctx->zc_fixture = NULL;
	fixture->retired = true;
	if (conn_zc_retirement_observer != NULL)
		conn_zc_retirement_observer(fixture);
}
#endif
