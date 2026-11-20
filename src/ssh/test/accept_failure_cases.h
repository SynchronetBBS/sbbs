/* Native authenticated transport peer, with client connection demux deliberately
 * not started: that peer reads exact channel wire replies itself. */
struct accept_failure_fixture {
	struct conn_ctx ctx;
	struct dssh_chan_accept_cbs cbs;
	dssh_channel accepted;
	atomic_bool peer_done;
	atomic_bool server_done;
	atomic_bool first_returned;
	atomic_bool close_seen;
	atomic_bool acknowledge;
	atomic_bool success_received;
	unsigned kind;
	unsigned allocations;
	unsigned callbacks;
	unsigned target_hit;
	bool fired;
	bool peer_valid;
	bool server_valid;
	size_t slot_size;
	unsigned sends;
	bool retired;
	struct dssh_chan_params watched_params;
};

static bool
accept_failure_preopen(unsigned kind)
{
	return kind == 0 || kind == 1 || (kind >= 10 && kind <= 14);
}

static bool
accept_failure_loops(unsigned kind)
{
	return kind == 5 || kind == 6 || kind == 7 || kind == 8 || kind == 9;
}

static bool
accept_failure_reply(unsigned kind)
{
	return kind != 6 && kind != 15 && kind != 16;
}

static void
accept_failure_allocation(size_t count, size_t size, void *opaque)
{
	struct accept_failure_fixture *fixture = opaque;
	if (fixture->fired)
		return;
	bool target = fixture->kind == 0 ? count == 1 && size == sizeof(struct dssh_channel_s)
	    : fixture->kind == 1 ? count == 1 && size == 8 * sizeof(struct dssh_event_entry)
	    : fixture->kind == 2 ? count == 1 && size == 5
	    : fixture->kind == 13 || fixture->kind == 14 ? count == 1 && size == fixture->slot_size
	    : fixture->kind == 3 || fixture->kind == 4 || fixture->kind == 15 || fixture->kind == 16
	        ? count == 1 && size == 6579 : false;
	if (!target)
		return;
	fixture->allocations++;
	unsigned wanted = fixture->kind == 4 || fixture->kind == 14 || fixture->kind == 16 ? 2 : 1;
	if (fixture->allocations == wanted) {
		fixture->fired = true;
		fixture->target_hit++;
		dssh_test_alloc_fail_after(0);
	}
}

static _Thread_local struct accept_failure_fixture *accept_phase_fixture;

static int
accept_phase_fault(dssh_session sess)
{
	(void)sess;
	struct accept_failure_fixture *fixture = accept_phase_fixture;
	fixture->sends++;
	unsigned target = fixture->kind == 19 ? 1 : 2;
	if (fixture->sends != target)
		return 0;
	/* Keep the successful preceding response observable before the
	 * application termination callback closes both mock read ends. */
	if (fixture->kind == 20 && !wire_wait(sess, &fixture->success_received))
		return DSSH_ERROR_TERMINATED;
	fixture->fired = true;
	fixture->target_hit++;
	return DSSH_ERROR_TERMINATED;
}

static void
accept_failure_terminate(dssh_session sess, void *opaque)
{
	(void)sess;
	struct accept_failure_fixture *fixture = opaque;
	mock_io_close_c2s(&fixture->ctx.io);
	mock_io_close_s2c(&fixture->ctx.io);
}

static int
accept_failure_terminal(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)ch;
	(void)params;
	struct accept_failure_fixture *fixture = opaque;
	fixture->callbacks++;
	result->max_window = 6579;
	if (fixture->kind >= 18) {
		fixture->watched_params = *params;
		dssh_test_alloc_watch_free(params->command, 0);
		if (fixture->kind == 18)
			dssh_test_wake_condition(&ch->poll_cnd, 1, false, false);
		else {
			accept_phase_fixture = fixture;
			accept_phase_test_hook = accept_phase_fault;
		}
	}
	return accept_failure_loops(fixture->kind) && fixture->kind != 7
	    && fixture->callbacks == 1 ? DSSH_ERROR_REJECTED : 0;
}

static int
accept_failure_server(void *opaque)
{
	struct accept_failure_fixture *fixture = opaque;
	dssh_test_alloc_observe_allocations(accept_failure_allocation, fixture);
	if (fixture->kind >= 10 && fixture->kind <= 12) {
		unsigned index = fixture->kind - 10;
		int fail_index = (int)index;
		dssh_test_ossl_fail_after(fail_index);
	}
	fixture->accepted = dssh_chan_accept(fixture->ctx.server, &fixture->cbs, 3000);
	atomic_store(&fixture->first_returned, true);
	accept_phase_test_hook = NULL;
	if (fixture->kind >= 18) {
		fixture->retired = dssh_test_alloc_watched_free(NULL);
		if (fixture->kind == 18) {
			fixture->target_hit = dssh_test_wake_hits();
			fixture->fired = fixture->target_hit == 1;
		}
		/* Old-protocol controls retain safe cleanup of an orphaned local
		 * parameter bundle without masking the failed retirement assertion. */
		if (!fixture->retired)
			dssh_chan_params_free(&fixture->watched_params);
		dssh_test_alloc_reset();
		dssh_test_ossl_reset();
		fixture->server_valid = fixture->accepted == NULL && fixture->retired;
		atomic_store(&fixture->server_done, true);
		return 0;
	}
	if (fixture->kind >= 10 && fixture->kind <= 12) {
		fixture->fired = dssh_test_ossl_count() > 0;
		fixture->target_hit = fixture->fired ? 1 : 0;
	}
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	bool local = !accept_failure_loops(fixture->kind);
	fixture->server_valid = !local || fixture->accepted == NULL;
	if (local && fixture->accepted == NULL && !dssh_session_is_terminated(fixture->ctx.server))
		fixture->accepted = dssh_chan_accept(fixture->ctx.server, &fixture->cbs, 3000);
	fixture->server_valid = fixture->server_valid && fixture->accepted != NULL;
	atomic_store(&fixture->server_done, true);
	return 0;
}

static int
accept_failure_open(dssh_session sess, uint32_t id)
{
	uint8_t packet[32];
	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_OPEN;
	DSSH_PUT_U32(DSSH_STRLEN("session"), packet, &pos);
	memcpy(packet + pos, "session", DSSH_STRLEN("session"));
	pos += DSSH_STRLEN("session");
	DSSH_PUT_U32(id, packet, &pos);
	DSSH_PUT_U32(1024, packet, &pos);
	DSSH_PUT_U32(1024, packet, &pos);
	return send_packet(sess, packet, pos, NULL);
}

static int
accept_failure_request(dssh_session sess, uint32_t id, const char *type, bool reply)
{
	uint8_t packet[128];
	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(id, packet, &pos);
	size_t len = strlen(type);
	if (len > UINT32_MAX || len > sizeof(packet) - 21)
		return DSSH_ERROR_INVALID;
	uint32_t wire_len = (uint32_t)len;
	DSSH_PUT_U32(wire_len, packet, &pos);
	memcpy(packet + pos, type, len);
	pos += len;
	packet[pos++] = reply ? 1 : 0;
	if (strcmp(type, "shell") != 0) {
		DSSH_PUT_U32(DSSH_STRLEN("failure"), packet, &pos);
		memcpy(packet + pos, "failure", DSSH_STRLEN("failure"));
		pos += DSSH_STRLEN("failure");
	}
	return send_packet(sess, packet, pos, NULL);
}

static int
accept_failure_close(dssh_session sess, uint32_t id)
{
	uint8_t packet[5];
	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_CLOSE;
	DSSH_PUT_U32(id, packet, &pos);
	return send_packet(sess, packet, pos, NULL);
}

static bool
accept_failure_receive(dssh_session sess, uint8_t wanted, uint32_t id, uint32_t *server_id)
{
	uint8_t type = 0;
	uint8_t *packet = NULL;
	size_t len = 0;
	int ret = recv_packet(sess, &type, &packet, &len);
	if (ret != 0 || type != wanted || len < 5 || DSSH_GET_U32(packet + 1) != id)
		return false;
	if (wanted == SSH_MSG_CHANNEL_OPEN_CONFIRMATION) {
		if (len != 17 || DSSH_GET_U32(packet + 9) != 0 || DSSH_GET_U32(packet + 13) == 0)
			return false;
		*server_id = DSSH_GET_U32(packet + 5);
		return true;
	}
	if (wanted == SSH_MSG_CHANNEL_OPEN_FAILURE)
		return len == 17 && DSSH_GET_U32(packet + 5) == 4
		    && DSSH_GET_U32(packet + 9) == 0 && DSSH_GET_U32(packet + 13) == 0;
	if (wanted == SSH_MSG_CHANNEL_WINDOW_ADJUST)
		return len == 9 && DSSH_GET_U32(packet + 5) == 6579;
	return len == 5;
}

static int
accept_failure_peer(void *opaque)
{
	struct accept_failure_fixture *fixture = opaque;
	dssh_test_alloc_exclude_thread();
	dssh_test_ossl_exclude_thread();
	dssh_session sess = fixture->ctx.client;
	uint32_t server_id = 0;
	bool valid = accept_failure_open(sess, 42) == 0;
	if (valid && fixture->kind >= 18) {
		valid = accept_failure_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 42, &server_id)
		    && accept_failure_request(sess, server_id, "exec", true) == 0;
		if (valid && fixture->kind == 20) {
			valid = accept_failure_receive(sess, SSH_MSG_CHANNEL_SUCCESS, 42, &server_id);
			atomic_store(&fixture->success_received, valid);
		}
		if (valid) {
			uint8_t type;
			uint8_t *packet;
			size_t len;
			valid = recv_packet(sess, &type, &packet, &len) < 0;
		}
		fixture->peer_valid = valid;
		atomic_store(&fixture->peer_done, true);
		return 0;
	}
	if (valid && accept_failure_preopen(fixture->kind))
		valid = accept_failure_receive(sess, SSH_MSG_CHANNEL_OPEN_FAILURE, 42, &server_id);
	else if (valid) {
		valid = accept_failure_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 42, &server_id);
		if (valid && fixture->kind == 7)
			valid = accept_failure_close(sess, server_id) == 0;
		else if (valid && fixture->kind != 2) {
			const char *type = fixture->kind == 8 ? "shell"
			    : fixture->kind == 9 ? "subsystem" : "exec";
			valid = accept_failure_request(sess, server_id, type, accept_failure_reply(fixture->kind)) == 0;
			if (valid && accept_failure_reply(fixture->kind))
				valid = accept_failure_receive(sess, SSH_MSG_CHANNEL_FAILURE, 42, &server_id);
		}
		if (valid)
			valid = accept_failure_receive(sess, SSH_MSG_CHANNEL_CLOSE, 42, &server_id);
		if (valid) {
			atomic_store(&fixture->close_seen, true);
			while (!atomic_load(&fixture->acknowledge) && !dssh_session_is_terminated(sess))
				thrd_yield();
			if (fixture->kind != 7)
				valid = accept_failure_close(sess, server_id) == 0;
		}
	}
	if (valid) {
		valid = accept_failure_open(sess, 43) == 0
		    && accept_failure_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 43, &server_id)
		    && accept_failure_request(sess, server_id, "exec", true) == 0
		    && accept_failure_receive(sess, SSH_MSG_CHANNEL_SUCCESS, 43, &server_id)
		    && accept_failure_receive(sess, SSH_MSG_CHANNEL_WINDOW_ADJUST, 43, &server_id);
	}
	fixture->peer_valid = valid;
	atomic_store(&fixture->peer_done, true);
	return 0;
}

static void
accept_failure_release(void *data)
{
	struct accept_failure_fixture *fixture = data;
	atomic_store(&fixture->acknowledge, true);
}

static int
accept_failure_case(unsigned kind)
{
	struct accept_failure_fixture fixture = {.kind = kind};
	ASSERT_EQ(conn_setup_no_start(&fixture.ctx), 0);
	ASSERT_EQ(dssh_session_set_timeout(fixture.ctx.server, 3000), 0);
	if (kind >= 18)
		ASSERT_EQ(dssh_session_set_terminate_cb(fixture.ctx.server, accept_failure_terminate, &fixture), 0);
	ASSERT_EQ(dssh_session_start(fixture.ctx.server), 0);
	fixture.cbs = accept_cbs_all;
	fixture.cbs.exec = accept_failure_terminal;
	fixture.cbs.shell = accept_failure_terminal;
	fixture.cbs.subsystem = accept_failure_terminal;
	fixture.cbs.cbdata = &fixture;
	uint16_t mac = fixture.ctx.server->trans.mac_s2c_active->digest_size;
	fixture.slot_size = tx_slot_buf_size(9, tx_block_size(fixture.ctx.server), mac);
	atomic_init(&fixture.peer_done, false);
	atomic_init(&fixture.server_done, false);
	atomic_init(&fixture.first_returned, false);
	atomic_init(&fixture.close_seen, false);
	atomic_init(&fixture.acknowledge, false);
	atomic_init(&fixture.success_received, false);
	struct channel_workers workers;
	channel_workers_init(&workers, &fixture.ctx);
	workers.release = accept_failure_release;
	workers.resource = &fixture;
	ASSERT_EQ(channel_worker_start(&workers, 0, accept_failure_server, &fixture), 0);
	ASSERT_EQ(channel_worker_start(&workers, 1, accept_failure_peer, &fixture), 0);
	ASSERT_TRUE(wire_checkpoint(WIRE_ACCEPT_FAILURE, kind, WIRE_STARTED));
	bool valid = true;
	if (valid && kind < 18 && !accept_failure_preopen(kind)) {
		struct timespec begin;
		ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC, &begin), 0);
		while (!atomic_load(&fixture.close_seen) && !atomic_load(&fixture.peer_done)) {
			if (!dssh_test_wait_tick(&begin)) {
				valid = false;
				break;
			}
		}
		valid = valid && atomic_load(&fixture.close_seen) && !atomic_load(&fixture.first_returned);
		if (mtx_lock(&fixture.ctx.server->channel_mtx) != thrd_success)
			abort();
		if (kind != 7)
			valid = valid && fixture.ctx.server->channel_count == 1;
		if (mtx_unlock(&fixture.ctx.server->channel_mtx) != thrd_success)
			abort();
	}
	if (valid && kind < 18 && !accept_failure_preopen(kind)) {
		ASSERT_TRUE(wire_checkpoint(WIRE_ACCEPT_FAILURE, kind, WIRE_CLOSE_WAIT));
	}
	accept_failure_release(&fixture);
	if (valid)
		valid = dssh_test_wait_bool(&fixture.peer_done, true)
		    && dssh_test_wait_bool(&fixture.server_done, true);
	if (!valid) {
		dssh_session_terminate(fixture.ctx.client);
		dssh_session_terminate(fixture.ctx.server);
		mock_io_close_c2s(&fixture.ctx.io);
		mock_io_close_s2c(&fixture.ctx.io);
	}
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_EQ(workers.results[1], 0);
	valid = valid && fixture.peer_valid && fixture.server_valid
	    && dssh_session_is_terminated(fixture.ctx.client) == (kind >= 18)
	    && dssh_session_is_terminated(fixture.ctx.server) == (kind >= 18);
	if (!accept_failure_loops(kind))
		valid = valid && fixture.fired && fixture.target_hit == 1;
	if (fixture.accepted != NULL)
		ASSERT_EQ(dssh_chan_close(fixture.accepted, -1), 0);
	conn_cleanup(&fixture.ctx);
	if (!valid)
		fprintf(stderr, "accept failure kind=%u peer=%u server=%u fired=%u target=%u callbacks=%u\n",
		    kind, fixture.peer_valid ? 1U : 0U, fixture.server_valid ? 1U : 0U,
		    fixture.fired ? 1U : 0U, fixture.target_hit, fixture.callbacks);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#define ACCEPT_FAILURE_TEST(name, kind) \
static int name(void) \
{ \
	return accept_failure_case(kind); \
}
ACCEPT_FAILURE_TEST(test_accept_channel_storage, 0)
ACCEPT_FAILURE_TEST(test_accept_event_storage, 1)
ACCEPT_FAILURE_TEST(test_accept_param_storage, 2)
ACCEPT_FAILURE_TEST(test_accept_stream_first, 3)
ACCEPT_FAILURE_TEST(test_accept_stream_second, 4)
ACCEPT_FAILURE_TEST(test_accept_exec_reject, 5)
ACCEPT_FAILURE_TEST(test_accept_exec_no_reply, 6)
ACCEPT_FAILURE_TEST(test_accept_peer_close, 7)
ACCEPT_FAILURE_TEST(test_accept_shell_reject, 8)
ACCEPT_FAILURE_TEST(test_accept_subsystem_reject, 9)
ACCEPT_FAILURE_TEST(test_accept_buf_init, 10)
ACCEPT_FAILURE_TEST(test_accept_cb_init, 11)
ACCEPT_FAILURE_TEST(test_accept_cnd_init, 12)
ACCEPT_FAILURE_TEST(test_accept_slot_first, 13)
ACCEPT_FAILURE_TEST(test_accept_slot_second, 14)
ACCEPT_FAILURE_TEST(test_accept_stream_first_no_reply, 15)
ACCEPT_FAILURE_TEST(test_accept_stream_second_no_reply, 16)
ACCEPT_FAILURE_TEST(test_accept_notify_failure, 18)
ACCEPT_FAILURE_TEST(test_accept_success_send_failure, 19)
ACCEPT_FAILURE_TEST(test_accept_window_send_failure, 20)
#undef ACCEPT_FAILURE_TEST

static int
test_accept_close_deadline(void)
{
	struct event_fixture fixture = {0};
	ASSERT_EQ(conn_setup_no_start(&fixture.ctx), 0);
	ASSERT_EQ(dssh_session_set_timeout(fixture.ctx.server, 3000), 0);
	ASSERT_EQ(dssh_session_start(fixture.ctx.client), 0);
	ASSERT_EQ(dssh_session_start(fixture.ctx.server), 0);
	fixture.open = (struct open_exec_ctx){.client = fixture.ctx.client,
	    .server = fixture.ctx.server, .command = "close-deadline",
	    .cbs = &accept_cbs_all, .accept_timeout = 3000};
	ASSERT_EQ(open_exec_channel(&fixture.ctx, &fixture.open), 0);
	dssh_channel ch = fixture.open.server_ch;
	struct timespec first;
	if (timespec_get(&first, TIME_UTC) != TIME_UTC)
		abort();
	struct timespec second = first;
	if (!dssh_timespec_add_ms(&second, 4000))
		abort();
	ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
	ch->setup_mode = true;
	ch->setup_ready = true;
	ch->setup_msg_type = SSH_MSG_CHANNEL_WINDOW_ADJUST;
	ch->setup_payload_len = 9;
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	atomic_bool *waited = dssh_test_thrd_watch_condition(&ch->poll_cnd);
	/* Deadline creation and the first queued-message iteration see first;
	 * the next iteration sees a time after that same deadline. */
	dssh_test_clock_schedule(first, 2, second);
	int ret = wait_for_close(fixture.ctx.server, ch);
	dssh_test_clock_reset();
	bool entered_wait = atomic_load(waited);
	dssh_test_thrd_watch_condition(NULL);
	bool restored = setup_test_mode(ch, false);
	conn_cleanup(&fixture.ctx);
	ASSERT_EQ(ret, DSSH_ERROR_TIMEOUT);
	ASSERT_FALSE(entered_wait);
	ASSERT_TRUE(restored);
	return TEST_PASS;
}
