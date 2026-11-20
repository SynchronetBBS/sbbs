#ifndef DSSH_TEST_TX_SLOT_CASES_H
#define DSSH_TEST_TX_SLOT_CASES_H

static bool
tx_slot_receive_reply(dssh_session peer, uint8_t expected)
{
	uint8_t type;
	uint8_t *payload;
	size_t length;
	return recv_packet(peer, &type, &payload, &length) == 0
	    && type == expected && length == 1 && payload[0] == expected;
}

static bool
tx_slot_ordinary_reply(dssh_session sender, dssh_session peer)
{
	uint8_t reply = SSH_MSG_REQUEST_FAILURE;
	return send_to_slot(sender, &sender->trans.global_reply_slot, &reply, 1) == 0
	    && tx_slot_receive_reply(peer, reply);
}

static bool
tx_slot_foreign_owner(struct conn_ctx *ctx, bool credit)
{
	if (tx_acquire(ctx->client, SSH_MSG_IGNORE) != 0)
		return false;
	uint8_t reply = SSH_MSG_REQUEST_FAILURE;
	int ret = credit ? schedule_wa_slot(ctx->server)
	    : send_to_slot(ctx->server, &ctx->server->trans.global_reply_slot, &reply, 1);
	bool owned = tx_owner_is_current(ctx->client) && atomic_load(&ctx->client->trans.tx_owner_active);
	if (tx_owner_is_current(ctx->client))
		tx_unlock_with_slots(ctx->client);
	bool valid = ret == DSSH_ERROR_INVALID && owned
	    && !atomic_load(&ctx->client->trans.tx_owner_active)
	    && !dssh_session_is_terminated(ctx->server);
	if (printf("tx_slot_foreign credit=%d result=%d owner_retained=%d\n", credit, ret, owned) < 0)
		return false;
	return valid && tx_slot_ordinary_reply(ctx->client, ctx->server)
	    && tx_slot_ordinary_reply(ctx->server, ctx->client);
}

static bool
tx_slot_owned_wait(struct conn_ctx *ctx)
{
	dssh_session sess = ctx->client;
	struct dssh_tx_slot *slot = &sess->trans.global_reply_slot;
	if (tx_acquire(sess, SSH_MSG_IGNORE) != 0)
		return false;
	uint8_t first = SSH_MSG_REQUEST_FAILURE;
	int queued = send_to_slot(sess, slot, &first, 1);
	/* If the old path attempts to wait, model a native refusal and let it
	 * unwind without waiting for work only this caller can drain. */
	struct dssh_test_wait_probe probe = {.condition = &sess->trans.tx_slot_cnd,
	    .mutex = &sess->trans.tx_queue_mtx, .result = thrd_error};
	atomic_init(&probe.calls, 0);
	atomic_init(&probe.native_calls, 0);
	atomic_init(&probe.native_result, thrd_busy);
	atomic_init(&probe.returning, false);
	atomic_init(&probe.release, true);
	dssh_test_wait_probe(&probe);
	uint8_t second = SSH_MSG_REQUEST_SUCCESS;
	int ret = queued == 0 ? send_to_slot(sess, slot, &second, 1) : queued;
	dssh_test_wait_probe(NULL);
	unsigned waits = atomic_load(&probe.calls);
	bool valid = queued == 0 && ret == DSSH_ERROR_INVALID && waits == 0
	    && atomic_load(&slot->ready) && tx_owner_is_current(sess)
	    && !dssh_session_is_terminated(sess);
	tx_unlock_with_slots(sess);
	if (printf("tx_slot_owner_wait queued=%d result=%d waits=%u\n", queued, ret, waits) < 0)
		return false;
	return valid && !atomic_load(&slot->ready) && tx_slot_receive_reply(ctx->server, first);
}

static bool
tx_slot_capacity(struct conn_ctx *ctx)
{
	dssh_session sess = ctx->client;
	struct dssh_tx_slot *slot = &sess->trans.global_reply_slot;
	if (tx_acquire(sess, SSH_MSG_IGNORE) != 0)
		return false;
	/* Two bytes fit in the old allocation's padding area. The fixture
	 * checks admission and drops any unexpectedly accepted test packet. */
	uint8_t oversized[] = {SSH_MSG_REQUEST_FAILURE, 0};
	int ret = send_to_slot(sess, slot, oversized, sizeof(oversized));
	bool ready = atomic_load(&slot->ready);
	if (ready)
		atomic_store(&slot->ready, false);
	bool valid = ret == DSSH_ERROR_TOOLONG && !ready && !dssh_session_is_terminated(sess);
	uint8_t reply = SSH_MSG_REQUEST_FAILURE;
	if (valid)
		valid = send_to_slot(sess, slot, &reply, 1) == 0 && atomic_load(&slot->ready);
	tx_unlock_with_slots(sess);
	if (printf("tx_slot_capacity result=%d published=%d\n", ret, ready) < 0)
		return false;
	return valid && !atomic_load(&slot->ready) && tx_slot_receive_reply(ctx->server, reply);
}

static int
test_tx_slot_admission(void)
{
	bool passed = true;
	for (unsigned mode = 0; mode < 4; mode++) {
		struct conn_ctx ctx;
		if (conn_setup_mode(&ctx, false) != 0)
			return TEST_FAIL;
		bool valid = mode < 2 ? tx_slot_foreign_owner(&ctx, mode == 1)
		    : mode == 2 ? tx_slot_owned_wait(&ctx) : tx_slot_capacity(&ctx);
		conn_cleanup(&ctx);
		passed = valid && passed;
	}
	ASSERT_TRUE(passed);
	return TEST_PASS;
}


struct tx_slot_sender {
	dssh_session sess;
	uint8_t reply;
};

static int
tx_slot_send_worker(void *opaque)
{
	struct tx_slot_sender *sender = opaque;
	return send_to_slot(sender->sess, &sender->sess->trans.global_reply_slot, &sender->reply, 1);
}

static bool
tx_slot_resize_while_waiting(struct conn_ctx *ctx)
{
	dssh_session sess = ctx->client;
	struct dssh_tx_slot *slot = &sess->trans.global_reply_slot;
	uint8_t first = SSH_MSG_REQUEST_FAILURE;
	/* An empty slot still supports a direct send with no queue storage. */
	struct dssh_tx_slot unallocated = {0};
	if (send_to_slot(sess, &unallocated, &first, 1) != 0
	    || !tx_slot_receive_reply(ctx->server, first)
	    || tx_acquire(sess, SSH_MSG_IGNORE) != 0)
		return false;
	struct flow_fixture flow;
	flow_init(&flow, ctx, NULL, NULL);
	flow.tx.session = sess;
	flow.tx.held = true;
	bool valid = send_to_slot(sess, slot, &first, 1) == 0 && atomic_load(&slot->ready);
	struct tx_slot_sender sender = {.sess = sess, .reply = SSH_MSG_REQUEST_SUCCESS};
	atomic_bool *waited = dssh_test_thrd_watch_condition(&sess->trans.tx_slot_cnd);
	flow.watching = true;
	ASSERT_TRUE(valid);
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, tx_slot_send_worker, &sender), 0);
	ASSERT_TRUE(flow_checkpoint(FLOW_TX_STARTED));
	valid = conn_actor_waiting(waited, &sess->trans.tx_queue_mtx);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(flow_checkpoint(FLOW_TX_WAITING));
	if (valid) {
		/* The owner replaces storage while a real producer waits on the
		 * occupied slot. The new allocation also fits the active epoch. */
		size_t old_size = slot->buf_sz;
		valid = resize_tx_slot_live(sess, slot, 1, 128, 64) == 0
		    && slot->buf_sz > old_size && slot->payload_capacity == 1
		    && atomic_load(&slot->ready) && slot->payload_len == 1 && slot->buf[9] == first;
	}
	if (valid) {
		dssh_test_alloc_fail_after(0);
		int ret = resize_tx_slot_live(sess, slot, 1, 128, 64);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		valid = ret == 0 && allocations == 0;
	}
	if (!valid)
		session_set_terminate(sess);
	callback_release_tx(&flow.tx);
	flow_finish(&flow);
	return valid && flow.tx.result == 0 && flow.workers.results[0] == 0 && !atomic_load(&slot->ready)
	    && tx_slot_receive_reply(ctx->server, first)
	    && tx_slot_receive_reply(ctx->server, sender.reply);
}

static int
test_tx_slot_resize(void)
{
	struct conn_ctx ctx;
	if (conn_setup_mode(&ctx, false) != 0)
		return TEST_FAIL;
	bool valid = tx_slot_resize_while_waiting(&ctx);
	conn_cleanup(&ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct tx_slot_refusal_peer {
	dssh_session sess;
	bool global;
};

static int
tx_slot_refusal_send_requests(void *opaque)
{
	struct tx_slot_refusal_peer *peer = opaque;
	for (uint32_t number = 0; number < 2; number++) {
		uint8_t msg[32];
		size_t len;
		if (peer->global) {
			const uint8_t request[] = {SSH_MSG_GLOBAL_REQUEST, 0, 0, 0, 1,
			    number == 0 ? 'a' : 'b', 1};
			memcpy(msg, request, sizeof(request));
			len = sizeof(request);
		}
		else {
			len = build_channel_open(msg, sizeof(msg), "unknown", number, 0, 0);
			if (len == 0)
				return DSSH_ERROR_INIT;
		}
		int ret = send_packet(peer->sess, msg, len, NULL);
		if (ret != 0)
			return ret;
	}
	return 0;
}

static int
tx_slot_refusal_global(const uint8_t *name, size_t name_len, bool want_reply,
    const uint8_t *payload, size_t len, void *data)
{
	(void)payload;
	(void)len;
	(void)data;
	return want_reply && name_len == 1 && name != NULL && name[0] == 'a' ? 0 : DSSH_ERROR_REJECTED;
}

static bool
tx_slot_wait_transmitted(dssh_session sess, uint_fast32_t count)
{
	struct timespec started;
	if (clock_gettime(CLOCK_MONOTONIC, &started) != 0)
		return false;
	while (atomic_load(&sess->trans.tx_since_rekey) < count) {
		if (sess->terminate || !dssh_test_wait_tick(&started))
			return false;
	}
	return true;
}

enum tx_slot_refusal_mode {
	TX_SLOT_NO_REFUSAL,
	TX_SLOT_KEY_REFUSAL,
	TX_SLOT_SIZE_REFUSAL,
};

static dssh_log_action
tx_slot_refusal_log(const struct dssh_log_record *record, void *opaque)
{
	atomic_int *error = opaque;
	if (record->level == DSSH_LOG_ERROR
	    && (record->error_code == DSSH_ERROR_REKEY_NEEDED || record->error_code == DSSH_ERROR_TOOLONG))
		atomic_store(error, record->error_code);
	return DSSH_LOG_LOCAL_ONLY;
}

static int
tx_slot_refusal_run(struct conn_ctx *ctx, bool global, enum tx_slot_refusal_mode mode,
    atomic_int *error)
{
	dssh_session sess = ctx->server;
	ASSERT_EQ(dssh_session_set_global_request_cb(sess, tx_slot_refusal_global, NULL), 0);
	ASSERT_EQ(dssh_session_set_log_cb(sess, tx_slot_refusal_log, error), 0);
	ASSERT_TRUE(compression_fixture_selected(ctx->client, sess));
	ASSERT_EQ(dssh_session_start(sess), 0);
	struct flow_fixture flow;
	flow_init(&flow, ctx, NULL, NULL);
	ASSERT_EQ(tx_acquire(sess, SSH_MSG_IGNORE), 0);
	flow.tx.session = sess;
	flow.tx.held = true;
	struct dssh_tx_slot *slot = global ? &sess->trans.global_reply_slot : &sess->trans.open_fail_slot;
	uint_fast32_t count = atomic_load(&sess->trans.tx_since_rekey);
	uint32_t seq = sess->trans.tx_seq;
	struct tx_slot_refusal_peer peer = {.sess = ctx->client, .global = global};
	atomic_bool *waiting = dssh_test_thrd_watch_condition(&sess->trans.tx_slot_cnd);
	flow.watching = true;
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, tx_slot_refusal_send_requests, &peer), 0);
	ASSERT_TRUE(flow_checkpoint(FLOW_REFUSAL_STARTED));
	channel_worker_join(&flow.workers, 0);
	bool valid = flow.workers.results[0] == 0
	    && conn_actor_waiting(waiting, &sess->trans.tx_queue_mtx) && atomic_load(&slot->ready);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(flow_checkpoint(FLOW_REFUSAL_WAITING));
	if (valid && mode == TX_SLOT_KEY_REFUSAL)
		atomic_store(&sess->trans.tx_since_rekey, DSSH_REKEY_HARD_LIMIT);
	else if (valid && mode == TX_SLOT_SIZE_REFUSAL)
		sess->trans.comp_tx_payload_limit = 0;
	callback_release_tx(&flow.tx);
	flow_finish(&flow);

	if (mode == TX_SLOT_NO_REFUSAL) {
		ASSERT_TRUE(valid);
		ASSERT_EQ(flow.tx.result, 0);
		ASSERT_TRUE(count <= UINT_FAST32_MAX - 2 && tx_slot_wait_transmitted(sess, count + 2));
		if (global) {
			ASSERT_TRUE(tx_slot_receive_reply(ctx->client, SSH_MSG_REQUEST_SUCCESS));
			ASSERT_TRUE(tx_slot_receive_reply(ctx->client, SSH_MSG_REQUEST_FAILURE));
		}
		else {
			ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_OPEN_FAILURE, 17, 0));
			ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_OPEN_FAILURE, 17, 1));
		}
		ASSERT_FALSE(dssh_session_is_terminated(sess));
		return TEST_PASS;
	}

	bool stopped = valid && dssh_test_wait_bool(&sess->demux_running, false);
	if (!stopped) {
		session_set_terminate(sess);
		mock_io_close_c2s(&ctx->io);
	}
	dssh_session_stop(sess);
	bool preserved = atomic_load(&slot->ready);
	bool emitted = sess->trans.tx_seq != seq;
	int reported = atomic_load(error);
	ASSERT_TRUE(printf("tx_slot_refusal global=%d mode=%d waited=%d stopped=%d preserved=%d emitted=%d error=%d\n",
	    global, mode, valid, stopped, preserved, emitted, reported) >= 0);
	ASSERT_TRUE(stopped);
	ASSERT_TRUE(dssh_session_is_terminated(sess));
	ASSERT_TRUE(preserved);
	ASSERT_FALSE(emitted);
	ASSERT_EQ(reported, mode == TX_SLOT_KEY_REFUSAL ? DSSH_ERROR_REKEY_NEEDED : DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_tx_slot_refusal(void)
{
	const char *compression = getenv("DSSH_TEST_COMPRESSION");
	bool growth = compression != NULL && strcmp(compression, "test-growth") == 0;
	for (unsigned i = 0; i < 4; i++) {
		struct conn_ctx ctx;
		if (conn_setup_mode(&ctx, false) != 0)
			return TEST_FAIL;
		enum tx_slot_refusal_mode mode = i < 2 ? TX_SLOT_NO_REFUSAL
		    : growth ? TX_SLOT_SIZE_REFUSAL : TX_SLOT_KEY_REFUSAL;
		atomic_int error;
		atomic_init(&error, 0);
		int ret = tx_slot_refusal_run(&ctx, i % 2 == 0, mode, &error);
		conn_cleanup(&ctx);
		if (ret != TEST_PASS)
			return TEST_FAIL;
	}
	return TEST_PASS;
}

#endif
