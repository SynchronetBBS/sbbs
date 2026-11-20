/* An authenticated raw peer observes each offer before generating a reply. */
enum transmit_rekey_case {
	TRANSMIT_REKEY_PACKETS,
	TRANSMIT_REKEY_BYTES,
	TRANSMIT_REKEY_INFLIGHT,
	TRANSMIT_REKEY_SIMULTANEOUS,
	TRANSMIT_REKEY_ALLOC_FAILURE,
	TRANSMIT_REKEY_IO_FAILURE,
	TRANSMIT_REKEY_PEER_FAILURE,
};

struct transmit_rekey_fixture {
	struct conn_ctx ctx;
	struct conn_rx_gate gate;
	struct flow_fixture flow;
	dssh_session peer;
	enum transmit_rekey_case mode;
	atomic_bool peer_done;
	atomic_bool peer_offer;
	atomic_uint replayed;
	atomic_int failure;
	unsigned writes;
};

static _Thread_local struct transmit_rekey_fixture *transmit_rekey_hook;
static const uint8_t transmit_rekey_ignore[] = {SSH_MSG_IGNORE, 0, 0, 0, 0};

static void
transmit_rekey_debug(bool display, const uint8_t *message, size_t len, void *opaque)
{
	struct transmit_rekey_fixture *fixture = opaque;
	unsigned index = atomic_fetch_add(&fixture->replayed, 1);
	if (index != 0 || !display || len != 1 || message == NULL || message[0] != 'd'
	    || atomic_load(&fixture->gate.sess->trans.rekey_in_progress))
		atomic_store(&fixture->failure, DSSH_ERROR_INVALID);
}

static void
transmit_rekey_unimplemented(uint32_t rejected, void *opaque)
{
	struct transmit_rekey_fixture *fixture = opaque;
	unsigned index = atomic_fetch_add(&fixture->replayed, 1);
	if (index != 2 || rejected != 123
	    || atomic_load(&fixture->gate.sess->trans.rekey_in_progress))
		atomic_store(&fixture->failure, DSSH_ERROR_INVALID);
}

static int
transmit_rekey_global(const uint8_t *name, size_t name_len, bool want_reply,
    const uint8_t *payload, size_t payload_len, void *opaque)
{
	(void)payload;
	(void)payload_len;
	struct transmit_rekey_fixture *fixture = opaque;
	if (name_len == 2 && name != NULL && memcmp(name, "id", 2) == 0)
		return DSSH_ERROR_REJECTED;
	unsigned index = atomic_fetch_add(&fixture->replayed, 1);
	if (!want_reply || name_len != 1 || name == NULL || (index != 1 && index != 3)
	    || name[0] != (index == 1 ? 'a' : 'b')
	    || atomic_load(&fixture->gate.sess->trans.rekey_in_progress))
		atomic_store(&fixture->failure, DSSH_ERROR_INVALID);
	return index == 1 ? 0 : DSSH_ERROR_REJECTED;
}

static dssh_log_action
transmit_rekey_log(const struct dssh_log_record *record, void *opaque)
{
	struct transmit_rekey_fixture *fixture = opaque;
	if (record->level == DSSH_LOG_ERROR && record->error_code < 0
	    && record->error_code != DSSH_ERROR_TERMINATED)
		atomic_store(&fixture->failure, record->error_code);
	return DSSH_LOG_LOCAL_ONLY;
}

static int
transmit_rekey_peer_offer(dssh_session sess)
{
	struct transmit_rekey_fixture *fixture = transmit_rekey_hook;
	if (sess != fixture->peer)
		return DSSH_ERROR_INVALID;
	atomic_store(&fixture->peer_offer, true);
	return 0;
}

static int
transmit_rekey_refuse_offer(dssh_session sess)
{
	struct transmit_rekey_fixture *fixture = transmit_rekey_hook;
	if (sess != fixture->gate.sess)
		return DSSH_ERROR_INVALID;
	return ++fixture->writes == 2 ? DSSH_ERROR_TOOLONG : 0;
}

static int
transmit_rekey_receive_offer(dssh_session peer)
{
	uint8_t type;
	uint8_t *payload;
	size_t len;
	int ret = recv_packet_raw(peer, &type, &payload, &len);
	if (ret != 0)
		return ret;
	if (type != SSH_MSG_KEXINIT || len < 17 || peer->trans.peer_kexinit != NULL)
		return DSSH_ERROR_PARSE;
	uint8_t *copy = malloc(len);
	if (copy == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(copy, payload, len);
	peer->trans.peer_kexinit = copy;
	peer->trans.peer_kexinit_sz = len;
	return 0;
}

static int
transmit_rekey_peer_worker(void *opaque)
{
	struct transmit_rekey_fixture *fixture = opaque;
	dssh_session peer = fixture->peer;
	int ret = 0;
	if (fixture->mode == TRANSMIT_REKEY_SIMULTANEOUS) {
		transmit_rekey_hook = fixture;
		window_tx_return_test_hook = transmit_rekey_peer_offer;
	}
	else {
		ret = transmit_rekey_receive_offer(peer);
	}
	if (ret == 0)
		ret = rekey(peer);
	window_tx_return_test_hook = NULL;
	transmit_rekey_hook = NULL;
	if (ret == 0 && fixture->mode == TRANSMIT_REKEY_INFLIGHT) {
		if (!tx_slot_receive_reply(peer, SSH_MSG_REQUEST_SUCCESS)
		    || !tx_slot_receive_reply(peer, SSH_MSG_REQUEST_FAILURE)
		    || !peer_id_receive_control(peer, SSH_MSG_CHANNEL_OPEN_FAILURE, 17, 42))
			ret = DSSH_ERROR_PARSE;
	}
	if (ret == 0 && !channel_id_wire_barrier(peer))
		ret = DSSH_ERROR_PARSE;
	atomic_store(&fixture->peer_done, true);
	return ret;
}

static bool
transmit_rekey_pipeline(dssh_session peer)
{
	const uint8_t debug[] = {SSH_MSG_DEBUG, 1, 0, 0, 0, 1, 'd', 0, 0, 0, 0};
	const uint8_t unimplemented[] = {SSH_MSG_UNIMPLEMENTED, 0, 0, 0, 123};
	const uint8_t first[] = {SSH_MSG_GLOBAL_REQUEST, 0, 0, 0, 1, 'a', 1};
	const uint8_t second[] = {SSH_MSG_GLOBAL_REQUEST, 0, 0, 0, 1, 'b', 1};
	uint8_t open[32];
	size_t len = build_channel_open(open, sizeof(open), "unknown", 42, 0, 0);
	return len != 0 && send_packet(peer, debug, sizeof(debug), NULL) == 0
	    && send_packet(peer, first, sizeof(first), NULL) == 0
	    && send_packet(peer, unimplemented, sizeof(unimplemented), NULL) == 0
	    && send_packet(peer, second, sizeof(second), NULL) == 0
	    && send_packet(peer, open, len, NULL) == 0;
}

static int
transmit_rekey_prepare(struct transmit_rekey_fixture *fixture)
{
	dssh_session sender = fixture->gate.sess;
	ASSERT_TRUE(compression_fixture_selected(fixture->ctx.client, fixture->ctx.server));
	dssh_kex method = atomic_load(&sender->trans.kex_selected);
	ASSERT_TRUE(method != NULL && strcmp(method->name, test_kex_algo_name()) == 0);
	ASSERT_EQ(dssh_session_set_log_cb(sender, transmit_rekey_log, fixture), 0);
	ASSERT_EQ(dssh_session_set_debug_cb(sender, transmit_rekey_debug, fixture), 0);
	ASSERT_EQ(dssh_session_set_unimplemented_cb(sender, transmit_rekey_unimplemented, fixture), 0);
	ASSERT_EQ(dssh_session_set_global_request_cb(sender, transmit_rekey_global, fixture), 0);
	ASSERT_EQ(dssh_session_start(sender), 0);
	ASSERT_TRUE(dssh_test_wait_bool(&fixture->gate.empty, true));
	ASSERT_EQ(tx_acquire(sender, SSH_MSG_IGNORE), 0);
	if (fixture->mode == TRANSMIT_REKEY_BYTES) {
		dssh_enc cipher = sender->trans.client ? sender->trans.enc_c2s_active : sender->trans.enc_s2c_active;
		if (cipher == NULL || cipher->bytes_per_key <= 1) {
			tx_unlock_with_slots(sender);
			return TEST_FAIL;
		}
		atomic_store(&sender->trans.tx_bytes_since_rekey, cipher->bytes_per_key - 1);
	}
	else {
		atomic_store(&sender->trans.tx_since_rekey, DSSH_REKEY_SOFT_LIMIT - 1);
	}
	tx_unlock_with_slots(sender);
	ASSERT_TRUE(flow_checkpoint(FLOW_REKEY_PREPARED));
	return TEST_PASS;
}

/* Instrumented finite-field exchanges can take longer than the ordinary
 * observation window. Allow the same bound as the queued-control cases. */
static bool
transmit_rekey_wait_peer(struct transmit_rekey_fixture *fixture)
{
	for (unsigned attempt = 0; attempt < 6; attempt++) {
		if (dssh_test_wait_bool(&fixture->peer_done, true))
			return true;
	}
	return false;
}

static int
transmit_rekey_success(struct transmit_rekey_fixture *fixture)
{
	dssh_session sender = fixture->gate.sess;
	dssh_session peer = fixture->peer;
	if (fixture->mode == TRANSMIT_REKEY_INFLIGHT)
		ASSERT_TRUE(transmit_rekey_pipeline(peer));
	ASSERT_EQ(send_packet(sender, transmit_rekey_ignore, sizeof(transmit_rekey_ignore), NULL), 0);
	ASSERT_TRUE(atomic_load(&sender->trans.rekey_in_progress));
	ASSERT_EQ(atomic_load(&sender->trans.rekey_read), DSSH_REKEY_READ_CLAIMED);
	uint8_t type;
	uint8_t *payload;
	size_t len;
	ASSERT_EQ(recv_packet_raw(peer, &type, &payload, &len), 0);
	ASSERT_EQ(type, SSH_MSG_IGNORE);
	ASSERT_EQ_U(len, sizeof(transmit_rekey_ignore));

	ASSERT_EQ(channel_worker_start(&fixture->flow.workers, 0, transmit_rekey_peer_worker, fixture), 0);
	ASSERT_TRUE(flow_checkpoint(FLOW_REKEY_STARTED));
	bool valid = fixture->mode != TRANSMIT_REKEY_SIMULTANEOUS
	    || dssh_test_wait_bool(&fixture->peer_offer, true);
	atomic_store(&fixture->gate.resume, true);
	valid = valid && transmit_rekey_wait_peer(fixture);
	if (!valid) {
		session_set_terminate(sender);
		session_set_terminate(peer);
		mock_io_close_c2s(&fixture->ctx.io);
		mock_io_close_s2c(&fixture->ctx.io);
	}
	channel_worker_join(&fixture->flow.workers, 0);
	ASSERT_TRUE(valid);
	ASSERT_EQ(fixture->flow.workers.results[0], 0);
	ASSERT_FALSE(atomic_load(&sender->trans.rekey_in_progress));
	ASSERT_TRUE(atomic_load(&sender->trans.tx_since_rekey) < DSSH_REKEY_SOFT_LIMIT);
	ASSERT_EQ_U(atomic_load(&fixture->replayed), fixture->mode == TRANSMIT_REKEY_INFLIGHT ? 4 : 0);
	ASSERT_EQ(atomic_load(&fixture->failure), 0);
	ASSERT_FALSE(dssh_session_is_terminated(sender));
	return TEST_PASS;
}

static int
transmit_rekey_failure_run(struct transmit_rekey_fixture *fixture)
{
	dssh_session sender = fixture->gate.sess;
	bool allocation = fixture->mode == TRANSMIT_REKEY_ALLOC_FAILURE;
	if (allocation)
		dssh_test_alloc_fail_after(0);
	else {
		transmit_rekey_hook = fixture;
		accept_phase_test_hook = transmit_rekey_refuse_offer;
	}
	int ret = send_packet(sender, transmit_rekey_ignore, sizeof(transmit_rekey_ignore), NULL);
	dssh_test_alloc_reset();
	accept_phase_test_hook = NULL;
	transmit_rekey_hook = NULL;
	atomic_store(&fixture->gate.resume, true);
	ASSERT_EQ(ret, DSSH_ERROR_TERMINATED);
	ASSERT_TRUE(dssh_session_is_terminated(sender));
	ASSERT_FALSE(atomic_load(&sender->trans.tx_owner_active));
	ASSERT_EQ(atomic_load(&fixture->failure), allocation ? DSSH_ERROR_ALLOC : DSSH_ERROR_TOOLONG);
	ASSERT_TRUE(allocation || fixture->writes == 2);
	ASSERT_EQ_U(atomic_load(&sender->trans.tx_since_rekey), DSSH_REKEY_SOFT_LIMIT);
	uint8_t type;
	uint8_t *payload;
	size_t len;
	ASSERT_EQ(recv_packet_raw(fixture->peer, &type, &payload, &len), 0);
	ASSERT_EQ(type, SSH_MSG_IGNORE);
	return TEST_PASS;
}

struct transmit_rekey_waiter {
	dssh_session sess;
	atomic_bool done;
};

static int
transmit_rekey_waiter_send(void *opaque)
{
	struct transmit_rekey_waiter *waiter = opaque;
	const uint8_t request[] = {SSH_MSG_GLOBAL_REQUEST, 0, 0, 0, 2, 'i', 'd', 1};
	int ret = send_packet(waiter->sess, request, sizeof(request), NULL);
	atomic_store(&waiter->done, true);
	return ret;
}

static int
transmit_rekey_exchange_failure(struct transmit_rekey_fixture *fixture)
{
	dssh_session sender = fixture->gate.sess;
	ASSERT_EQ(send_packet(sender, transmit_rekey_ignore, sizeof(transmit_rekey_ignore), NULL), 0);
	ASSERT_TRUE(atomic_load(&sender->trans.rekey_in_progress));
	uint_fast32_t sent = atomic_load(&sender->trans.tx_since_rekey);
	struct transmit_rekey_waiter waiter = {.sess = sender};
	atomic_init(&waiter.done, false);
	atomic_bool *waiting = dssh_test_thrd_watch_condition(&sender->trans.rekey_cnd);
	fixture->flow.watching = true;
	ASSERT_EQ(channel_worker_start(&fixture->flow.workers, 0, transmit_rekey_waiter_send, &waiter), 0);
	ASSERT_TRUE(flow_checkpoint(FLOW_REKEY_STARTED));
	ASSERT_TRUE(conn_actor_waiting(waiting, &sender->trans.tx_mtx));
	ASSERT_TRUE(flow_checkpoint(FLOW_REKEY_WAITING));
	const uint8_t malformed = SSH_MSG_KEXINIT;
	bool valid = send_packet(fixture->peer, &malformed, 1, NULL) == 0;
	atomic_store(&fixture->gate.resume, true);
	valid = valid && dssh_test_wait_bool(&waiter.done, true)
	    && dssh_test_wait_bool(&sender->demux_running, false);
	if (!valid) {
		session_set_terminate(sender);
		mock_io_close_c2s(&fixture->ctx.io);
		mock_io_close_s2c(&fixture->ctx.io);
	}
	channel_worker_join(&fixture->flow.workers, 0);
	dssh_test_thrd_watch_condition(NULL);
	fixture->flow.watching = false;
	ASSERT_TRUE(valid);
	ASSERT_EQ(fixture->flow.workers.results[0], DSSH_ERROR_TERMINATED);
	ASSERT_EQ(atomic_load(&fixture->failure), DSSH_ERROR_PARSE);
	ASSERT_EQ_U(atomic_load(&sender->trans.tx_since_rekey), sent);
	ASSERT_FALSE(atomic_load(&sender->trans.tx_owner_active));
	return TEST_PASS;
}

static void
transmit_rekey_release(void *data)
{
	struct transmit_rekey_fixture *fixture = data;
	atomic_store(&fixture->gate.resume, true);
}

static int
transmit_rekey_run(enum transmit_rekey_case mode)
{
	struct transmit_rekey_fixture fixture = {.mode = mode};
	bool reverse = mode == TRANSMIT_REKEY_BYTES;
	window_test_gather = reverse;
	int setup = conn_setup_mode(&fixture.ctx, false);
	window_test_gather = false;
	ASSERT_EQ(setup, 0);
	fixture.gate.sess = reverse ? fixture.ctx.server : fixture.ctx.client;
	fixture.peer = reverse ? fixture.ctx.client : fixture.ctx.server;
	atomic_init(&fixture.gate.empty, false);
	atomic_init(&fixture.gate.resume, false);
	atomic_init(&fixture.peer_done, false);
	atomic_init(&fixture.peer_offer, false);
	atomic_init(&fixture.replayed, 0);
	atomic_init(&fixture.failure, 0);
	conn_rx_gate = &fixture.gate;
	flow_init(&fixture.flow, &fixture.ctx, transmit_rekey_release, &fixture);
	int ret = transmit_rekey_prepare(&fixture);
	if (ret == TEST_PASS) {
		if (mode == TRANSMIT_REKEY_PEER_FAILURE)
			ret = transmit_rekey_exchange_failure(&fixture);
		else if (mode >= TRANSMIT_REKEY_ALLOC_FAILURE)
			ret = transmit_rekey_failure_run(&fixture);
		else
			ret = transmit_rekey_success(&fixture);
	}
	atomic_store(&fixture.gate.resume, true);
	flow_finish(&fixture.flow);
	conn_cleanup(&fixture.ctx);
	conn_rx_gate = NULL;
	return ret;
}

static int
test_transmit_rekey_threshold(void)
{
	ASSERT_EQ(transmit_rekey_run(TRANSMIT_REKEY_PACKETS), TEST_PASS);
	return transmit_rekey_run(TRANSMIT_REKEY_BYTES);
}

static int
test_transmit_rekey_inflight(void)
{
	return transmit_rekey_run(TRANSMIT_REKEY_INFLIGHT);
}

static int
test_transmit_rekey_simultaneous(void)
{
	return transmit_rekey_run(TRANSMIT_REKEY_SIMULTANEOUS);
}

static int
test_transmit_rekey_failure(void)
{
	ASSERT_EQ(transmit_rekey_run(TRANSMIT_REKEY_ALLOC_FAILURE), TEST_PASS);
	ASSERT_EQ(transmit_rekey_run(TRANSMIT_REKEY_IO_FAILURE), TEST_PASS);
	return transmit_rekey_run(TRANSMIT_REKEY_PEER_FAILURE);
}
