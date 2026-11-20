/* Included by test_conn.c after the channel wire fixture helpers. */

static int
peer_id_dispatch_open(dssh_session receiver, const char *type, uint32_t number)
{
	uint8_t message[32];
	size_t len = build_channel_open(message, sizeof(message), type, number, 123, 456);
	if (len == 0)
		return DSSH_ERROR_INIT;
	return dssh_test_demux_dispatch(receiver, SSH_MSG_CHANNEL_OPEN, message, len);
}

static bool
peer_id_receive_control(dssh_session peer, uint8_t expected, size_t length, uint32_t number)
{
	uint8_t type;
	uint8_t *payload;
	size_t len;
	return recv_packet(peer, &type, &payload, &len) == 0 && type == expected
	    && len == length && len >= 5 && DSSH_GET_U32(&payload[1]) == number;
}

enum peer_id_open_stage {
	PEER_ID_QUEUED,
	PEER_ID_POPPED,
	PEER_ID_CONFIRMED,
};

static int
peer_id_open_stage_run(struct conn_ctx *ctx, enum peer_id_open_stage stage)
{
	static const uint32_t numbers[] = {0, UINT32_MAX};
	ASSERT_EQ(dssh_session_start(ctx->server), 0);
	if (stage == PEER_ID_QUEUED) {
		dssh_test_alloc_fail_after(0);
		int ret = peer_id_dispatch_open(ctx->server, "session", numbers[0]);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, 0);
		ASSERT_EQ(allocations, 1);
		ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_OPEN_FAILURE,
		    17, numbers[0]));
	}
	for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++)
		ASSERT_EQ(peer_id_dispatch_open(ctx->server, "session", numbers[i]), 0);

	if (stage != PEER_ID_QUEUED) {
		/* Keep both pops alive through the ownership check. Each represents
		 * a different acceptor after leaving the FIFO. */
		struct dssh_incoming_open incoming[2];
		for (size_t i = 0; i < sizeof(incoming) / sizeof(incoming[0]); i++) {
			ASSERT_EQ(accept_incoming(ctx->server, &incoming[i], 0), 0);
			ASSERT_EQ_U(incoming[i].peer_channel, numbers[i]);
		}
		if (stage == PEER_ID_CONFIRMED) {
			for (size_t i = 0; i < sizeof(incoming) / sizeof(incoming[0]); i++) {
				ASSERT_NOT_NULL(accept_channel_init(ctx->server, &incoming[i]));
				ASSERT_TRUE(peer_id_receive_control(ctx->client,
				    SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 17, numbers[i]));
			}
		}
	}

	ASSERT_TRUE(lifecycle_checkpoint(LC_PEER_PHASE));
	int zero = peer_id_dispatch_open(ctx->server, "session", numbers[0]);
	int maximum = peer_id_dispatch_open(ctx->server, "session", numbers[1]);
	int unsupported = peer_id_dispatch_open(ctx->server, "unknown", numbers[1]);
	int distinct = peer_id_dispatch_open(ctx->server, "session", 1);
	unsigned stage_number = (unsigned)stage;
	ASSERT_TRUE(printf("peer_id_open stage=%u zero=%d maximum=%d unsupported=%d distinct=%d\n",
	    stage_number, zero, maximum, unsupported, distinct) >= 0);
	ASSERT_EQ(zero, DSSH_ERROR_PARSE);
	ASSERT_EQ(maximum, DSSH_ERROR_PARSE);
	ASSERT_EQ(unsupported, DSSH_ERROR_PARSE);
	ASSERT_EQ(distinct, 0);

	struct dssh_incoming_open next;
	if (stage == PEER_ID_QUEUED) {
		for (size_t i = 0; i < sizeof(numbers) / sizeof(numbers[0]); i++) {
			ASSERT_EQ(accept_incoming(ctx->server, &next, 0), 0);
			ASSERT_EQ_U(next.peer_channel, numbers[i]);
		}
	}
	ASSERT_EQ(accept_incoming(ctx->server, &next, 0), 0);
	ASSERT_EQ_U(next.peer_channel, 1);
	ASSERT_EQ(accept_incoming(ctx->server, &next, 0), DSSH_ERROR_NOMORE);
	return TEST_PASS;
}

static int
peer_id_open_stage_test(enum peer_id_open_stage stage)
{
	struct conn_ctx ctx;
	if (conn_setup_mode(&ctx, false) != 0)
		return TEST_FAIL;
	int ret = peer_id_open_stage_run(&ctx, stage);
	conn_cleanup(&ctx);
	return ret;
}

static int
test_peer_id_queued(void)
{
	return peer_id_open_stage_test(PEER_ID_QUEUED);
}

static int
test_peer_id_popped(void)
{
	return peer_id_open_stage_test(PEER_ID_POPPED);
}

static int
test_peer_id_confirmed(void)
{
	return peer_id_open_stage_test(PEER_ID_CONFIRMED);
}

static int
peer_id_dispatch_confirmation(dssh_session receiver, uint32_t local, uint32_t remote)
{
	uint8_t message[17];
	size_t len = channel_id_build_reply(message, local, true, remote);
	return dssh_test_demux_dispatch(receiver, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, message, len);
}

static int
peer_id_late_confirmation_run(struct conn_ctx *ctx)
{
	ASSERT_EQ(dssh_session_set_timeout(ctx->client, 10), 0);
	ASSERT_EQ(dssh_session_start(ctx->client), 0);
	uint32_t first;
	uint32_t second;
	ASSERT_EQ(channel_id_open_without_reply(ctx), 0);
	ASSERT_TRUE(channel_id_receive_open(ctx->server, &first));
	ASSERT_EQ(channel_id_open_without_reply(ctx), 0);
	ASSERT_TRUE(channel_id_receive_open(ctx->server, &second));
	ASSERT_EQ_U(channel_id_pending_count(ctx->client), 2);

	ASSERT_EQ(peer_id_dispatch_confirmation(ctx->client, first, UINT32_MAX), 0);
	ASSERT_TRUE(peer_id_receive_control(ctx->server, SSH_MSG_CHANNEL_CLOSE, 5, UINT32_MAX));
	ASSERT_TRUE(lifecycle_checkpoint(LC_PEER_LATE));
	int duplicate = peer_id_dispatch_confirmation(ctx->client, second, UINT32_MAX);
	int distinct = peer_id_dispatch_confirmation(ctx->client, second, 0);
	ASSERT_TRUE(printf("peer_id_late_confirmation duplicate=%d distinct=%d\n", duplicate, distinct) >= 0);
	ASSERT_EQ(duplicate, DSSH_ERROR_PARSE);
	ASSERT_EQ(distinct, 0);
	ASSERT_TRUE(peer_id_receive_control(ctx->server, SSH_MSG_CHANNEL_CLOSE, 5, 0));
	return TEST_PASS;
}

static int
test_peer_id_late_confirmation(void)
{
	struct conn_ctx ctx;
	if (conn_setup_mode(&ctx, false) != 0)
		return TEST_FAIL;
	int ret = peer_id_late_confirmation_run(&ctx);
	conn_cleanup(&ctx);
	return ret;
}

static int
peer_id_send_open(dssh_session peer, uint32_t number)
{
	uint8_t message[24];
	size_t len = build_channel_open(message, sizeof(message), "session", number, 123, 456);
	if (len != sizeof(message))
		return DSSH_ERROR_INIT;
	return send_packet(peer, message, len, NULL);
}

struct peer_id_accept_job {
	dssh_session sess;
	atomic_bool paused;
	atomic_bool resume;
	dssh_channel channel;
	uint32_t remote;
};

static void
peer_id_pause_allocation(size_t count, size_t size, void *opaque)
{
	struct peer_id_accept_job *job = opaque;
	if (count != 1 || size != sizeof(struct dssh_channel_s))
		return;
	atomic_store(&job->paused, true);
	while (!atomic_load(&job->resume))
		thrd_yield();
}

static int
peer_id_accept_worker(void *opaque)
{
	struct peer_id_accept_job *job = opaque;
	struct dssh_incoming_open incoming;
	int ret = accept_incoming(job->sess, &incoming, 3000);
	if (ret != 0)
		return ret;
	job->remote = incoming.peer_channel;
	dssh_test_alloc_observe_allocations(peer_id_pause_allocation, job);
	job->channel = accept_channel_init(job->sess, &incoming);
	dssh_test_alloc_observe_allocations(NULL, NULL);
	return job->channel == NULL ? DSSH_ERROR_INIT : 0;
}

static void
peer_id_release_accept(void *data)
{
	struct peer_id_accept_job *jobs = data;
	for (size_t i = 0; i < 2; i++)
		atomic_store(&jobs[i].resume, true);
}

static int
peer_id_concurrent_accept_run(struct conn_ctx *ctx)
{
	ASSERT_EQ(dssh_session_start(ctx->server), 0);
	ASSERT_EQ(peer_id_send_open(ctx->client, 0), 0);
	ASSERT_EQ(peer_id_send_open(ctx->client, UINT32_MAX), 0);
	struct peer_id_accept_job jobs[2] = {{.sess = ctx->server}, {.sess = ctx->server}};
	for (size_t i = 0; i < 2; i++) {
		atomic_init(&jobs[i].paused, false);
		atomic_init(&jobs[i].resume, false);
	}
	struct flow_fixture flow;
	flow_init(&flow, ctx, peer_id_release_accept, jobs);
	for (unsigned i = 0; i < 2; i++)
		ASSERT_EQ(channel_worker_start(&flow.workers, i, peer_id_accept_worker, &jobs[i]), 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_PEER_ACCEPT_STARTED));
	bool valid = dssh_test_wait_bool(&jobs[0].paused, true)
	    && dssh_test_wait_bool(&jobs[1].paused, true) && jobs[0].remote != jobs[1].remote;
	ASSERT_TRUE(valid);
	ASSERT_TRUE(lifecycle_checkpoint(LC_PEER_ACCEPT_PAUSED));
	if (valid) {
		struct dssh_incoming_open next;
		valid = peer_id_send_open(ctx->client, 0) == 0
		    && peer_id_send_open(ctx->client, UINT32_MAX) == 0
		    && peer_id_send_open(ctx->client, 1) == 0
		    && accept_incoming(ctx->server, &next, 3000) == 0
		    && next.peer_channel == 1;
	}
	peer_id_release_accept(jobs);
	flow_finish(&flow);
	valid = valid && flow.workers.results[0] == 0 && flow.workers.results[1] == 0;
	ASSERT_TRUE(valid);
	uint32_t received[2];
	for (size_t i = 0; i < 2; i++) {
		uint8_t type;
		uint8_t *payload;
		size_t len;
		ASSERT_EQ(recv_packet(ctx->client, &type, &payload, &len), 0);
		ASSERT_EQ(type, SSH_MSG_CHANNEL_OPEN_CONFIRMATION);
		ASSERT_EQ_U(len, 17);
		received[i] = DSSH_GET_U32(&payload[1]);
		ASSERT_TRUE(received[i] == 0 || received[i] == UINT32_MAX);
	}
	ASSERT_NE(received[0], received[1]);
	return TEST_PASS;
}

static int
test_peer_id_concurrent_accept(void)
{
	struct conn_ctx ctx;
	if (conn_setup_mode(&ctx, false) != 0)
		return TEST_FAIL;
	int ret = peer_id_concurrent_accept_run(&ctx);
	conn_cleanup(&ctx);
	return ret;
}

struct peer_id_terminal_job {
	dssh_session sess;
	dssh_channel closing;
	atomic_bool *acquiring;
	atomic_bool armed;
	atomic_bool returning;
	atomic_bool resume;
};

static _Thread_local struct peer_id_terminal_job *peer_id_terminal;

static int
peer_id_pause_terminal_tx(dssh_session sess)
{
	struct peer_id_terminal_job *job = peer_id_terminal;
	if (job->sess != sess)
		return DSSH_ERROR_INIT;
	atomic_store(&job->returning, true);
	while (!atomic_load(&job->resume))
		thrd_yield();
	return 0;
}

static int
peer_id_terminal_worker(void *opaque)
{
	struct peer_id_terminal_job *job = opaque;
	peer_id_terminal = job;
	window_tx_return_test_hook = peer_id_pause_terminal_tx;
	int ret;
	if (job->closing != NULL) {
		job->acquiring = dssh_test_thrd_pause_mutex_after(&job->sess->trans.tx_mtx, 0);
		atomic_store(&job->armed, true);
		if (atomic_load(&job->resume))
			dssh_test_thrd_resume_mutex();
		ret = conn_close(job->sess, job->closing);
	}
	else {
		dssh_test_alloc_fail_after(0);
		dssh_channel accepted = dssh_chan_accept(job->sess, &accept_cbs_all, 3000);
		dssh_test_alloc_reset();
		ret = accepted == NULL ? 0 : DSSH_ERROR_INIT;
	}
	window_tx_return_test_hook = NULL;
	peer_id_terminal = NULL;
	(void)dssh_test_thrd_pause_mutex_after(NULL, 0);
	return ret;
}

static void
peer_id_release_terminal(void *data)
{
	struct peer_id_terminal_job *job = data;
	atomic_store(&job->resume, true);
	dssh_test_thrd_resume_mutex();
}

static int
peer_id_ordered_terminal_run(struct conn_ctx *ctx, bool close)
{
	const uint32_t remote = UINT32_MAX;
	ASSERT_EQ(dssh_session_start(ctx->server), 0);
	ASSERT_EQ(peer_id_send_open(ctx->client, remote), 0);
	/* Finish admission before arming the process-wide allocation failure.
	 * The failure must belong to the acceptor's channel construction. */
	ASSERT_TRUE(channel_id_wire_barrier(ctx->client));
	struct peer_id_terminal_job job = {.sess = ctx->server};
	if (close) {
		struct dssh_incoming_open incoming;
		ASSERT_EQ(accept_incoming(ctx->server, &incoming, 3000), 0);
		job.closing = accept_channel_init(ctx->server, &incoming);
		ASSERT_NOT_NULL(job.closing);
		ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_OPEN_CONFIRMATION,
		    17, remote));
		uint8_t message[5];
		size_t pos = 0;
		message[pos++] = SSH_MSG_CHANNEL_CLOSE;
		DSSH_PUT_U32(job.closing->local_id, message, &pos);
		ASSERT_EQ(send_packet(ctx->client, message, pos, NULL), 0);
		ASSERT_TRUE(channel_id_wire_barrier(ctx->client));
	}
	atomic_init(&job.armed, false);
	atomic_init(&job.returning, false);
	atomic_init(&job.resume, false);
	struct flow_fixture flow;
	flow_init(&flow, ctx, peer_id_release_terminal, &job);
	flow.paused = true;
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, peer_id_terminal_worker, &job), 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_PEER_TERMINAL_STARTED));
	bool valid = true;
	bool acquiring = true;
	if (close) {
		/* A close sender has claimed the operation but has not acquired the
		 * packet engine. Its peer number must remain unavailable. */
		struct dssh_incoming_open next;
		valid = dssh_test_wait_bool(&job.armed, true)
		    && dssh_test_wait_bool(job.acquiring, true)
		    && peer_id_send_open(ctx->client, remote) == 0
		    && peer_id_send_open(ctx->client, 0) == 0
		    && accept_incoming(ctx->server, &next, 3000) == 0
		    && next.peer_channel == 0;
		ASSERT_TRUE(valid);
		ASSERT_TRUE(lifecycle_checkpoint(LC_PEER_ACQUIRING));
		acquiring = valid;
		dssh_test_thrd_resume_mutex();
	}
	bool returning = valid && dssh_test_wait_bool(&job.returning, true);
	ASSERT_TRUE(returning);
	ASSERT_TRUE(lifecycle_checkpoint(LC_PEER_RETURNING));
	bool received = returning && peer_id_receive_control(ctx->client, close ? SSH_MSG_CHANNEL_CLOSE
	        : SSH_MSG_CHANNEL_OPEN_FAILURE, close ? 5 : 17, remote);
	valid = received;
	struct dssh_incoming_open replacement;
	if (valid) {
		valid = peer_id_send_open(ctx->client, remote) == 0
		    && accept_incoming(ctx->server, &replacement, 3000) == 0
		    && replacement.peer_channel == remote;
	}
	peer_id_release_terminal(&job);
	flow_finish(&flow);
	int ret = flow.workers.results[0];
	ASSERT_TRUE(printf("peer_id_terminal close=%d acquiring=%d returning=%d received=%d replacement=%d worker=%d\n",
	    close, acquiring, returning, received, valid, ret) >= 0);
	ASSERT_TRUE(valid);
	ASSERT_EQ(ret, 0);
	ASSERT_FALSE(dssh_session_is_terminated(ctx->server));
	ASSERT_NOT_NULL(accept_channel_init(ctx->server, &replacement));
	ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 17, remote));
	return TEST_PASS;
}

struct peer_id_required_notice {
	bool called;
	bool owned;
};

static void
peer_id_required_terminated(dssh_session sess, void *opaque)
{
	struct peer_id_required_notice *notice = opaque;
	notice->called = true;
	notice->owned = tx_owner_is_current(sess);
}

static int
peer_id_required_refusal(struct conn_ctx *ctx, struct peer_id_required_notice *notice)
{
	dssh_session sess = ctx->server;
	ASSERT_EQ(dssh_session_set_terminate_cb(sess, peer_id_required_terminated, notice), 0);
	size_t capacity;
	int ret = DSSH_ERROR_INIT;
	uint8_t *packet = send_begin(sess, SSH_MSG_CHANNEL_CLOSE, &capacity, &ret);
	if (packet == NULL)
		return TEST_FAIL;
	if (capacity < 5) {
		send_cancel(sess);
		return TEST_FAIL;
	}
	packet[0] = SSH_MSG_CHANNEL_CLOSE;
	size_t pos = 1;
	DSSH_PUT_U32(UINT32_MAX, packet, &pos);
	uint32_t seq = sess->trans.tx_seq;
	atomic_store(&sess->trans.tx_since_rekey, UINT_FAST32_MAX);
	ret = send_commit_required(sess, pos);
	ASSERT_EQ(ret, DSSH_ERROR_REKEY_NEEDED);
	ASSERT_TRUE(notice->called && notice->owned);
	ASSERT_TRUE(dssh_session_is_terminated(sess));
	ASSERT_FALSE(atomic_load(&sess->trans.tx_owner_active));
	ASSERT_EQ_U(sess->trans.tx_seq, seq);
	ASSERT_TRUE(lifecycle_checkpoint(LC_PEER_REQUIRED));
	return TEST_PASS;
}

static int
test_peer_id_ordered_terminal(void)
{
	for (unsigned mode = 0; mode < 4; mode++) {
		struct conn_ctx ctx;
		window_test_gather = mode >= 2;
		int setup = conn_setup_mode(&ctx, false);
		window_test_gather = false;
		ASSERT_EQ(setup, 0);
		int ret = peer_id_ordered_terminal_run(&ctx, mode % 2 != 0);
		conn_cleanup(&ctx);
		if (ret != TEST_PASS)
			return ret;
	}
	struct conn_ctx ctx;
	struct peer_id_required_notice notice = {0};
	ASSERT_EQ(conn_setup_mode(&ctx, false), 0);
	int ret = peer_id_required_refusal(&ctx, &notice);
	conn_cleanup(&ctx);
	return ret;
}
