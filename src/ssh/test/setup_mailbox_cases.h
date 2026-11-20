/* Actual native transport peers plus controlled direct dispatch, for setup
 * storage ownership, allocation locking, rollback and stream publication. */
struct setup_allocation_observer {
	size_t wanted;
	unsigned allocations;
	unsigned frees;
	unsigned matching;
	unsigned fail_matching;
	bool held;
};

static void
setup_observe_allocation(size_t count, size_t size, void *opaque)
{
	struct setup_allocation_observer *observer = opaque;
	if (observer->wanted != 0 && (count != 1 || size != observer->wanted))
		return;
	observer->allocations++;
	if (dssh_test_mtx_ownership())
		observer->held = true;
	if (observer->wanted != 0) {
		observer->matching++;
		if (observer->matching == observer->fail_matching)
			dssh_test_malloc_fail_size(size);
	}
}

static void
setup_observe_free(const void *ptr, void *opaque)
{
	struct setup_allocation_observer *observer = opaque;
	if (ptr != NULL) {
		observer->frees++;
		if (dssh_test_mtx_ownership())
			observer->held = true;
	}
}

static bool
setup_test_mode(dssh_channel ch, bool enabled)
{
	if (mtx_lock(&ch->buf_mtx) != thrd_success)
		return false;
	ch->setup_mode = enabled;
	ch->setup_ready = false;
	ch->setup_error = false;
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	return true;
}

static int
setup_test_packet(dssh_channel ch, uint8_t *packet, size_t capacity, size_t len)
{
	if (ch == NULL || packet == NULL || len < 9)
		return DSSH_ERROR_INVALID;
#if SIZE_MAX > UINT32_MAX
	if (len - 9 > UINT32_MAX)
		return DSSH_ERROR_INVALID;
#endif
	if (len > capacity)
		return DSSH_ERROR_TOOLONG;
	uint32_t data_len = (uint32_t)(len - 9);
	memset(packet, 0x85, len);
	packet[0] = SSH_MSG_CHANNEL_DATA;
	size_t pos = 1;
	DSSH_PUT_U32(ch->local_id, packet, &pos);
	DSSH_PUT_U32(data_len, packet, &pos);
	return 0;
}

/* setup_recv borrows storage from the occupied mailbox until release. */
static bool
setup_test_consume(struct event_fixture *fixture, const uint8_t *packet, size_t length,
    bool *retained)
{
	dssh_channel ch = fixture->open.server_ch;
	uint8_t type = 0;
	uint8_t *payload = NULL;
	size_t len = 0;
	int ret = setup_recv(fixture->ctx.server, ch, &type, &payload, &len);
	if (ret != 0)
		return false;
	bool valid = packet != NULL && payload != NULL && length != 0
	    && type == packet[0] && len == length
	    && memcmp(payload, packet, length) == 0;
	if (mtx_lock(&ch->buf_mtx) != thrd_success)
		abort();
	bool owned = ch->setup_payload == payload;
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	*retained = *retained && owned;
	return setup_release(fixture->ctx.server, ch) == 0 && valid;
}

static int
setup_mailbox_reuse_case(bool grow)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	dssh_channel ch = fixture.open.server_ch;
	ASSERT_TRUE(setup_test_mode(ch, true));
	struct setup_allocation_observer observer = {0};
	dssh_test_mtx_watch_ownership(&ch->buf_mtx);
	dssh_test_alloc_observe_allocations(setup_observe_allocation, &observer);
	dssh_test_alloc_observe_frees(setup_observe_free, &observer);
	uint8_t packet[1024];
	bool valid = true;
	bool retained = true;
	for (unsigned i = 0; i < 40 && valid; i++) {
		size_t len = grow && i >= 1 ? sizeof(packet) : 64;
		valid = setup_test_packet(ch, packet, sizeof(packet), len) == 0;
		valid = valid && dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, len) == 0
		    && setup_test_consume(&fixture, packet, len, &retained);
	}
	dssh_test_alloc_observe_allocations(NULL, NULL);
	dssh_test_alloc_observe_frees(NULL, NULL);
	dssh_test_mtx_watch_ownership(NULL);
	bool reset = setup_test_mode(ch, false);
	ASSERT_TRUE(valid && retained && reset);
	ASSERT_FALSE(observer.held);
	ASSERT_EQ_U(observer.allocations, grow ? 1 : 0);
	ASSERT_EQ_U(observer.frees, grow ? 1 : 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_DONE));
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

static int
test_setup_mailbox_reuse(void)
{
	return setup_mailbox_reuse_case(false);
}

static int
test_setup_mailbox_growth(void)
{
	return setup_mailbox_reuse_case(true);
}

static int
test_setup_mailbox_rollback(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	dssh_channel ch = fixture.open.server_ch;
	ASSERT_TRUE(setup_test_mode(ch, true));
	uint8_t packet[1024];
	bool retained = true;
	ASSERT_EQ(setup_test_packet(ch, packet, sizeof(packet), 64), 0);
	ASSERT_EQ(dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, 64), 0);
	ASSERT_TRUE(setup_test_consume(&fixture, packet, 64, &retained));
	ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
	uint8_t *old = ch->setup_payload;
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	ASSERT_EQ(setup_test_packet(ch, packet, sizeof(packet), sizeof(packet)), 0);
	dssh_test_alloc_fail_after(0);
	int ret = dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, sizeof(packet));
	int attempts = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
	bool intact = old != NULL && ch->setup_payload == old && ch->setup_error && !ch->setup_ready;
	ch->setup_error = false;
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	bool valid = ret == 0 && attempts == 1
	    && atomic_load(&ch->dispatch_state) == DSSH_CHANNEL_DISPATCH_IDLE
	    && dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, sizeof(packet)) == 0
	    && setup_test_consume(&fixture, packet, sizeof(packet), &retained);
	bool reset = setup_test_mode(ch, false);
	ASSERT_TRUE(valid && retained && intact && reset);
	ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_DONE));
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

static int
test_setup_mailbox_bound(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	dssh_channel ch = fixture.open.server_ch;
	ASSERT_TRUE(setup_test_mode(ch, true));
	size_t limit = fixture.ctx.server->trans.packet_buf_sz;
	ASSERT_TRUE(limit < SIZE_MAX);
	uint8_t *packet = malloc(limit + 1);
	ASSERT_NOT_NULL(packet);
	int ret = setup_test_packet(ch, packet, limit + 1, limit + 1);
	if (ret == 0)
		ret = dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, limit + 1);
	bool valid = ret == DSSH_ERROR_PARSE
	    && atomic_load(&ch->dispatch_state) == DSSH_CHANNEL_DISPATCH_IDLE;
	bool reset = setup_test_mode(ch, false);
	free(packet);
	ASSERT_TRUE(valid && reset);
	ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_DONE));
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

static int
setup_accept_observed(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)params;
	struct setup_allocation_observer *observer = opaque;
	result->max_window = 6579;
	dssh_test_mtx_watch_ownership(&ch->buf_mtx);
	dssh_test_alloc_observe_allocations(setup_observe_allocation, observer);
	dssh_test_alloc_observe_frees(setup_observe_free, observer);
	return 0;
}

static int
setup_terminal_case(unsigned fail)
{
	struct conn_ctx ctx = {0};
	ASSERT_EQ(conn_setup_no_start(&ctx), 0);
	ASSERT_EQ(dssh_session_set_timeout(ctx.client, 1000), 0);
	ASSERT_EQ(dssh_session_set_timeout(ctx.server, 1000), 0);
	ASSERT_EQ(dssh_session_start(ctx.client), 0);
	ASSERT_EQ(dssh_session_start(ctx.server), 0);
	struct setup_allocation_observer observer = {.wanted = 6579, .fail_matching = fail};
	struct dssh_chan_accept_cbs cbs = accept_cbs_all;
	cbs.exec = setup_accept_observed;
	cbs.cbdata = &observer;
	struct open_exec_ctx open = {.client = ctx.client, .server = ctx.server,
	    .command = "setup-storage", .cbs = &cbs, .accept_timeout = 30000};
	int ret = open_exec_channel(&ctx, &open);
	bool failed = dssh_test_malloc_size_failed();
	dssh_test_alloc_reset();
	bool valid = fail == 0 ? ret == 0 && observer.matching == 2
	    : ret < 0 && open.server_ch == NULL && failed && observer.matching == fail;
	ASSERT_TRUE(valid);
	ASSERT_FALSE(observer.held);
	ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_DONE));
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_setup_terminal_storage(void)
{
	return setup_terminal_case(0);
}

static int
test_setup_terminal_first_failure(void)
{
	return setup_terminal_case(1);
}

static int
test_setup_terminal_second_failure(void)
{
	return setup_terminal_case(2);
}

static int
test_setup_mailbox_no_allocation(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	dssh_channel ch = fixture.open.server_ch;
	ASSERT_TRUE(setup_test_mode(ch, true));
	uint8_t packet[64];
	ASSERT_EQ(setup_test_packet(ch, packet, sizeof(packet), sizeof(packet)), 0);
	bool valid = true;
	bool retained = true;
	dssh_test_alloc_fail_after(0);
	for (unsigned i = 0; i < 160 && valid; i++) {
		valid = dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, sizeof(packet)) == 0
		    && setup_test_consume(&fixture, packet, sizeof(packet), &retained);
	}
	int attempts = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	bool reset = setup_test_mode(ch, false);
	ASSERT_TRUE(valid && retained && reset);
	ASSERT_EQ(attempts, 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_DONE));
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

struct setup_gap_job {
	struct event_fixture *fixture;
	atomic_bool entered;
	atomic_bool release;
	atomic_bool done;
	bool held;
	unsigned kind;
	unsigned bad_unlocks;
	int ret;
};

static void
setup_gap_allocation(size_t count, size_t size, void *opaque)
{
	struct setup_gap_job *job = opaque;
	if (count != 1 || size != 1024)
		return;
	job->held = dssh_test_mtx_ownership();
	if (job->kind == 3)
		dssh_test_mtx_reject_once(&job->fixture->open.server_ch->buf_mtx);
	atomic_store(&job->entered, true);
	while (!atomic_load(&job->release)) {
	}
}

static int
setup_gap_worker(void *opaque)
{
	struct setup_gap_job *job = opaque;
	dssh_channel ch = job->fixture->open.server_ch;
	uint8_t packet[1024];
	int built = setup_test_packet(ch, packet, sizeof(packet), sizeof(packet));
	if (built != 0) {
		job->ret = built;
		atomic_store(&job->done, true);
		return built;
	}
	dssh_test_mtx_watch_ownership(&ch->buf_mtx);
	dssh_test_alloc_observe_allocations(setup_gap_allocation, job);
	job->ret = dssh_test_demux_dispatch(job->fixture->ctx.server, packet[0], packet, sizeof(packet));
	job->bad_unlocks = dssh_test_mtx_rejected_unlocks();
	dssh_test_alloc_observe_allocations(NULL, NULL);
	dssh_test_ossl_reset();
	atomic_store(&job->done, true);
	return 0;
}

struct setup_detach_job {
	dssh_session sess;
	dssh_channel ch;
	atomic_bool done;
	atomic_bool waiting;
	int ret;
};

static int
setup_detach_worker(void *opaque)
{
	struct setup_detach_job *job = opaque;
	dssh_test_detach_observe(&job->waiting);
	job->ret = detach_channel(job->sess, job->ch);
	dssh_test_detach_observe(NULL);
	atomic_store(&job->done, true);
	return 0;
}

struct setup_gap_lifetime {
	struct flow_fixture flow;
	struct setup_gap_job *writer;
	struct setup_detach_job *detacher;
	bool detach_started;
	bool closed;
};

static void
setup_gap_release(void *data)
{
	struct setup_gap_lifetime *life = data;
	atomic_store(&life->writer->release, true);
}

static void
setup_gap_retire(void *data)
{
	struct flow_fixture *flow = data;
	struct setup_gap_lifetime *life = flow->data;
	flow_retire(flow);
	if (life->detach_started && life->detacher->ret == 0) {
		dssh_channel *channel = &life->writer->fixture->open.server_ch;
		if (dssh_chan_close(*channel, -1) != 0)
			abort();
		*channel = NULL;
		life->closed = true;
	}
}

static int
setup_gap_case(unsigned kind)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	dssh_channel ch = fixture.open.server_ch;
	ASSERT_TRUE(setup_test_mode(ch, true));
	struct setup_gap_job job = {.fixture = &fixture, .kind = kind};
	atomic_init(&job.entered, false);
	atomic_init(&job.release, false);
	atomic_init(&job.done, false);
	struct setup_detach_job detach = {.sess = fixture.ctx.server, .ch = ch,
	    .ret = DSSH_ERROR_INIT};
	atomic_init(&detach.done, false);
	atomic_init(&detach.waiting, false);
	struct setup_gap_lifetime life = {.writer = &job, .detacher = &detach};
	flow_init(&life.flow, &fixture.ctx, setup_gap_release, &life);
	life.flow.workers.retire = setup_gap_retire;
	ASSERT_EQ(channel_worker_start(&life.flow.workers, 0, setup_gap_worker, &job), 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_STARTED));
	bool valid = dssh_test_wait_bool(&job.entered, true);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_PAUSED));
	if (valid) {
		int ret = mtx_trylock(&ch->buf_mtx);
		valid = ret == thrd_success && !job.held;
		if (ret == thrd_success) {
			if (kind == 0)
				ch->setup_mode = false;
			if (mtx_unlock(&ch->buf_mtx) != thrd_success)
				abort();
		}
	}
	ASSERT_TRUE(valid);
	if (kind == 1) {
		ASSERT_EQ(channel_worker_start(&life.flow.workers, 1, setup_detach_worker, &detach), 0);
		life.detach_started = true;
		ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_DETACH_STARTED));
		valid = wire_wait(fixture.ctx.server, &detach.waiting)
		    && atomic_load(&ch->closing) && !atomic_load(&detach.done);
		ASSERT_TRUE(valid);
		ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_DETACH_WAITING));
	}
	if (valid && kind == 2)
		dssh_session_terminate(fixture.ctx.server);
	setup_gap_release(&life);
	flow_finish(&life.flow);
	ASSERT_EQ(life.flow.workers.results[0], 0);
	if (life.detach_started)
		ASSERT_EQ(life.flow.workers.results[1], 0);
	valid = valid && atomic_load(&job.done) && job.bad_unlocks == 0;
	if (kind == 0 && valid) {
		uint8_t received[1015];
		valid = job.ret == 0 && dssh_chan_read(ch, 0, received, sizeof(received)) == 1015;
		for (size_t i = 0; i < sizeof(received); i++)
			valid = valid && received[i] == 0x85;
	}
	else if (kind == 1)
		valid = valid && job.ret == 0 && atomic_load(&detach.done) && detach.ret == 0;
	else
		valid = valid && job.ret == DSSH_ERROR_TERMINATED;
	if (kind == 1)
		valid = valid && life.closed && fixture.open.server_ch == NULL;
	else if (!atomic_load(&fixture.ctx.server->terminate))
		valid = setup_test_mode(ch, false) && valid;
	conn_cleanup(&fixture.ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_setup_gap_disable(void)
{
	return setup_gap_case(0);
}

static int
test_setup_gap_detach(void)
{
	return setup_gap_case(1);
}

static int
test_setup_gap_terminate(void)
{
	return setup_gap_case(2);
}

static int
test_setup_gap_reacquire(void)
{
	return setup_gap_case(3);
}

static _Thread_local uint8_t *setup_confirmation_storage;
static _Thread_local bool setup_confirmation_published;

static int
setup_confirmation_fail(dssh_session sess)
{
	if (mtx_lock(&sess->channel_mtx) != thrd_success)
		abort();
	dssh_channel ch = sess->channel_count == 1 ? sess->channels[0] : NULL;
	if (mtx_unlock(&sess->channel_mtx) != thrd_success)
		abort();
	if (ch == NULL)
		return DSSH_ERROR_TERMINATED;
	uint8_t packet[64];
	if (setup_test_packet(ch, packet, sizeof(packet), sizeof(packet)) != 0)
		return DSSH_ERROR_TERMINATED;
	int ret = dssh_test_demux_dispatch(sess, packet[0], packet, sizeof(packet));
	if (mtx_lock(&ch->buf_mtx) != thrd_success)
		abort();
	setup_confirmation_storage = ch->setup_payload;
	setup_confirmation_published = ret == 0 && ch->setup_ready
	    && setup_confirmation_storage != NULL;
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	dssh_test_alloc_watch_free(setup_confirmation_storage, 0);
	return DSSH_ERROR_TERMINATED;
}

static int
test_setup_confirmation_cleanup(void)
{
	struct conn_ctx ctx = {0};
	ASSERT_EQ(conn_setup(&ctx), 0);
	ASSERT_EQ(peer_id_dispatch_open(ctx.server, "session", 7), 0);
	struct dssh_incoming_open incoming;
	ASSERT_EQ(accept_incoming(ctx.server, &incoming, 0), 0);
	setup_confirmation_storage = NULL;
	setup_confirmation_published = false;
	setup_tx_test_hook = setup_confirmation_fail;
	dssh_channel ch = accept_channel_init(ctx.server, &incoming);
	setup_tx_test_hook = NULL;
	bool seen = dssh_test_alloc_watched_free(NULL);
	bool valid = ch == NULL && setup_confirmation_published && seen;
	setup_confirmation_storage = NULL;
	dssh_test_alloc_reset();
	ASSERT_TRUE(valid);
	ASSERT_TRUE(lifecycle_checkpoint(LC_SETUP_DONE));
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_setup_packet_bounds(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	dssh_channel ch = fixture.open.server_ch;
	const size_t lengths[] = {9, 10, 64};
	for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
		for (size_t capacity = 0; capacity <= 64; capacity++) {
			uint8_t guarded[66];
			memset(guarded, 0xa5, sizeof(guarded));
			size_t length = lengths[i];
			int ret = setup_test_packet(ch, guarded + 1, capacity, length);
			ASSERT_EQ(ret, capacity < length ? DSSH_ERROR_TOOLONG : 0);
			ASSERT_EQ(guarded[0], 0xa5);
			if (ret == 0) {
				ASSERT_EQ(guarded[1], SSH_MSG_CHANNEL_DATA);
				ASSERT_EQ_U(DSSH_GET_U32(guarded + 2), ch->local_id);
				ASSERT_EQ_U(DSSH_GET_U32(guarded + 6), length - 9);
				for (size_t j = 10; j <= length; j++)
					ASSERT_EQ(guarded[j], 0x85);
			}
			for (size_t j = ret == 0 ? length + 1 : 1; j < sizeof(guarded); j++)
				ASSERT_EQ(guarded[j], 0xa5);
		}
	}
	uint8_t packet[64];
	for (size_t length = 0; length < 9; length++) {
		memset(packet, 0xa5, sizeof(packet));
		ASSERT_EQ(setup_test_packet(ch, packet, sizeof(packet), length), DSSH_ERROR_INVALID);
		for (size_t j = 0; j < sizeof(packet); j++)
			ASSERT_EQ(packet[j], 0xa5);
	}
	ASSERT_EQ(setup_test_packet(NULL, packet, sizeof(packet), 9), DSSH_ERROR_INVALID);
	ASSERT_EQ(setup_test_packet(ch, NULL, 64, 9), DSSH_ERROR_INVALID);
#if SIZE_MAX > UINT32_MAX
	ASSERT_EQ(setup_test_packet(ch, packet, SIZE_MAX, SIZE_MAX), DSSH_ERROR_INVALID);
#endif
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}
