/* Authenticated wire peer owns client RX. Each queued response is caused by
 * an actual request; receive-window credit follows an actual application read. */
struct queue_retry_fixture {
	struct conn_ctx ctx;
	dssh_channel channel;
	atomic_bool accepted;
	atomic_bool peer_ready;
	atomic_bool owner_ready;
	atomic_bool flush_ready;
	atomic_bool peer_done;
	bool peer_valid;
	unsigned kind;
	atomic_bool second_entered;
	atomic_bool second_release;
	atomic_bool rekey_go;
	atomic_bool rekey_started;
	atomic_bool bad;
	atomic_uint data_calls;
	atomic_bool *pause_entered;
	atomic_bool cancelled;
	atomic_bool merge_expired;
	bool merge_valid;
	int rekey_result;
	atomic_bool data_done;
	atomic_bool marker_release;
	atomic_bool stream_entered;
	atomic_bool stream_release;
	dssh_chan_zc_cb stream_cb;
	void *stream_cbdata;
};

static bool
queue_retry_rekey_kind(unsigned kind)
{
	return kind == 10 || kind == 11;
}

/* Deliver through the real stream callback, then pause before the receive
 * handler resumes. The application consumes the actual published bytes while
 * the library callback is still active, with no physical mutex held. */
static uint32_t
queue_retry_stream(dssh_channel ch, int stream, const uint8_t *data, size_t length, void *opaque)
{
	struct queue_retry_fixture *f = opaque;
	uint32_t consumed = 0;
	if (f->stream_cb == NULL)
		atomic_store(&f->bad, true);
	else
		consumed = f->stream_cb(ch, stream, data, length, f->stream_cbdata);
	atomic_store(&f->stream_entered, true);
	if (!wire_wait(ch->sess, &f->stream_release))
		atomic_store(&f->bad, true);
	return consumed;
}

/* Instrumented DH exchanges can exceed the ordinary five-second observation
 * window. Keep the real rekey bounded without cancelling a valid exchange. */
static bool
queue_retry_wait_peer(struct queue_retry_fixture *f)
{
	unsigned attempts = queue_retry_rekey_kind(f->kind) ? 6U : 1U;
	for (unsigned i = 0; i < attempts; i++) {
		if (dssh_test_wait_bool(&f->peer_done, true))
			return true;
	}
	return false;
}

static bool
queue_retry_merge_kind(unsigned kind)
{
	return kind == 6 || kind == 9;
}

static uint32_t
queue_retry_zc(dssh_channel ch, int stream, const uint8_t *data, size_t length, void *opaque)
{
	(void)ch;
	struct queue_retry_fixture *f = opaque;
	unsigned call = atomic_fetch_add(&f->data_calls, 1);
	if (stream != 0 || call > 1 || !flow_bytes(data, length, call == 0 ? 24U : 8U, 0x6d)) {
		atomic_store(&f->bad, true);
		return 0;
	}
	if (call == 1) {
		atomic_store(&f->second_entered, true);
		if (!wire_wait(ch->sess, &f->second_release))
			atomic_store(&f->bad, true);
	}
	return length <= UINT32_MAX ? (uint32_t)length : UINT32_MAX;
}

static int
queue_retry_merge(void *opaque)
{
	struct queue_retry_fixture *f = opaque;
	bool valid = wire_wait(f->ctx.server, f->pause_entered);
	if (!valid && !atomic_load(&f->cancelled))
		atomic_store(&f->merge_expired, true);
	valid = valid && !atomic_load(&f->cancelled);
	atomic_store(&f->second_release, true);
	struct timespec start;
	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
		valid = false;
	while (valid && !atomic_load(&f->cancelled)) {
		if (mtx_lock(&f->channel->buf_mtx) != thrd_success)
			abort();
		bool ready = f->channel->local_window == 0 && f->channel->window_adjust_pending == 8
		    && f->channel->window_adjust_inflight == 24;
		if (mtx_unlock(&f->channel->buf_mtx) != thrd_success)
			abort();
		if (ready)
			break;
		valid = dssh_test_wait_tick(&start);
		if (!valid && !atomic_load(&f->cancelled))
			atomic_store(&f->merge_expired, true);
	}
	f->merge_valid = valid && !atomic_load(&f->cancelled);
	dssh_test_thrd_resume_mutex();
	return 0;
}

/* An ordered no-reply marker isolates the queued-drain cases from receive
 * callback publication. The separate stream-callback cases hold that boundary. */
static int
queue_retry_global(const uint8_t *name, size_t name_len, bool want_reply,
    const uint8_t *data, size_t data_len, void *opaque)
{
	(void)data;
	struct queue_retry_fixture *f = opaque;
	if (name_len == DSSH_STRLEN("queue-credit-ready")
	    && name != NULL && memcmp(name, "queue-credit-ready", name_len) == 0
	    && !want_reply && data_len == 0) {
		atomic_store(&f->data_done, true);
		/* Keep the receiver at a known boundary while the controller models
		 * a temporary packet-budget refusal. Resume after that budget is
		 * restored so ordinary auto-rekey does not consume the model state. */
		if (!wire_wait(f->ctx.server, &f->marker_release))
			atomic_store(&f->bad, true);
	}
	return DSSH_ERROR_REJECTED;
}

static int
queue_retry_terminal(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)ch;
	(void)params;
	struct queue_retry_fixture *f = opaque;
	result->max_window = 32;
	if (queue_retry_merge_kind(f->kind)) {
		result->zc_cb = queue_retry_zc;
		result->zc_cbdata = f;
	}
	return 0;
}

static int
queue_retry_accept(void *opaque)
{
	struct queue_retry_fixture *f = opaque;
	struct dssh_chan_accept_cbs cbs = {.exec = queue_retry_terminal, .cbdata = f};
	f->channel = dssh_chan_accept(f->ctx.server, &cbs, 3000);
	atomic_store(&f->accepted, true);
	return 0;
}

static bool
queue_retry_receive(dssh_session sess, uint8_t wanted, uint32_t recipient, uint32_t *remote)
{
	uint8_t type;
	uint8_t *packet;
	size_t length;
	int ret = recv_packet(sess, &type, &packet, &length);
	if (ret != 0 || type != wanted)
		return false;
	if (wanted == SSH_MSG_REQUEST_FAILURE)
		return length == 1;
	if (length < 5 || DSSH_GET_U32(packet + 1) != recipient)
		return false;
	if (wanted == SSH_MSG_CHANNEL_OPEN_CONFIRMATION) {
		if (length != 17 || DSSH_GET_U32(packet + 9) != 0)
			return false;
		*remote = DSSH_GET_U32(packet + 5);
		return true;
	}
	if (wanted == SSH_MSG_CHANNEL_OPEN_FAILURE)
		return length == 17 && DSSH_GET_U32(packet + 5) == 3
		    && DSSH_GET_U32(packet + 9) == 0 && DSSH_GET_U32(packet + 13) == 0;
	return length == 5;
}

static bool
queue_retry_initial_window(dssh_session sess)
{
	uint8_t type;
	uint8_t *packet;
	size_t length;
	return recv_packet(sess, &type, &packet, &length) == 0
	    && type == SSH_MSG_CHANNEL_WINDOW_ADJUST && length == 9
	    && DSSH_GET_U32(packet + 1) == 42 && DSSH_GET_U32(packet + 5) == 32;
}

static int
queue_retry_peer(void *opaque)
{
	struct queue_retry_fixture *f = opaque;
	dssh_session sess = f->ctx.client;
	uint32_t remote = 0;
	bool valid = accept_failure_open(sess, 42) == 0
	    && queue_retry_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 42, &remote)
	    && accept_failure_request(sess, remote, "exec", true) == 0
	    && queue_retry_receive(sess, SSH_MSG_CHANNEL_SUCCESS, 42, NULL)
	    && queue_retry_initial_window(sess);
	atomic_store(&f->peer_ready, true);
	valid = valid && wire_wait(sess, &f->owner_ready);
	uint8_t packet[64];
	size_t pos = 0;
	packet[pos++] = SSH_MSG_GLOBAL_REQUEST;
	DSSH_PUT_U32(DSSH_STRLEN("queue-test"), packet, &pos);
	memcpy(packet + pos, "queue-test", DSSH_STRLEN("queue-test"));
	pos += DSSH_STRLEN("queue-test");
	packet[pos++] = 1;
	if (valid && f->kind != 8 && f->kind != 9)
		valid = send_packet(sess, packet, pos, NULL) == 0;
	pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_OPEN;
	DSSH_PUT_U32(DSSH_STRLEN("unknown"), packet, &pos);
	memcpy(packet + pos, "unknown", DSSH_STRLEN("unknown"));
	pos += DSSH_STRLEN("unknown");
	DSSH_PUT_U32(43, packet, &pos);
	DSSH_PUT_U32(1024, packet, &pos);
	DSSH_PUT_U32(1024, packet, &pos);
	if (valid && f->kind != 8 && f->kind != 9)
		valid = send_packet(sess, packet, pos, NULL) == 0;
	const uint8_t request[] = "queue-request";
	if (valid)
		valid = request_frame_send(sess, remote, request, sizeof(request) - 1, 1, NULL, 0) == 0;
	pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_DATA;
	DSSH_PUT_U32(remote, packet, &pos);
	DSSH_PUT_U32(24, packet, &pos);
	memset(packet + pos, 0x6d, 24);
	pos += 24;
	if (valid)
		valid = send_packet(sess, packet, pos, NULL) == 0;
	if (valid && queue_retry_merge_kind(f->kind)) {
		pos = 0;
		packet[pos++] = SSH_MSG_CHANNEL_DATA;
		DSSH_PUT_U32(remote, packet, &pos);
		DSSH_PUT_U32(8, packet, &pos);
		memset(packet + pos, 0x6d, 8);
		pos += 8;
		valid = send_packet(sess, packet, pos, NULL) == 0;
	}
	if (valid && !queue_retry_merge_kind(f->kind) && f->kind != 12) {
		pos = 0;
		packet[pos++] = SSH_MSG_GLOBAL_REQUEST;
		DSSH_PUT_U32(DSSH_STRLEN("queue-credit-ready"), packet, &pos);
		memcpy(packet + pos, "queue-credit-ready", DSSH_STRLEN("queue-credit-ready"));
		pos += DSSH_STRLEN("queue-credit-ready");
		packet[pos++] = 0;
		valid = send_packet(sess, packet, pos, NULL) == 0;
	}
	if (valid && queue_retry_rekey_kind(f->kind)) {
		valid = wire_wait(sess, &f->rekey_go);
		atomic_store(&f->rekey_started, true);
		f->rekey_result = valid ? rekey(sess) : DSSH_ERROR_TIMEOUT;
		valid = valid && f->rekey_result == 0;
	}
	if (valid)
		valid = wire_wait(sess, &f->flush_ready);
	if (valid && f->kind != 8 && f->kind != 9)
		valid = queue_retry_receive(sess, SSH_MSG_REQUEST_FAILURE, 0, NULL)
		    && queue_retry_receive(sess, SSH_MSG_CHANNEL_OPEN_FAILURE, 43, NULL);
	if (valid) {
		uint8_t type;
		uint8_t *message;
		size_t length;
		valid = recv_packet(sess, &type, &message, &length) == 0
		    && type == SSH_MSG_CHANNEL_WINDOW_ADJUST && length == 9
		    && DSSH_GET_U32(message + 1) == 42 && DSSH_GET_U32(message + 5) == (queue_retry_merge_kind(f->kind) ? 32U : 24U);
	}
	if (valid)
		valid = queue_retry_receive(sess, SSH_MSG_CHANNEL_FAILURE, 42, NULL)
		    && queue_retry_receive(sess, SSH_MSG_CHANNEL_SUCCESS, 42, NULL);
	f->peer_valid = valid;
	atomic_store(&f->peer_done, true);
	if (!valid) {
		mock_io_close_c2s(&f->ctx.io);
		mock_io_close_s2c(&f->ctx.io);
	}
	return 0;
}

static unsigned
queue_retry_mask(struct queue_retry_fixture *f)
{
	return (atomic_load(&f->ctx.server->trans.global_reply_slot.ready) ? 1U : 0U)
	    | (atomic_load(&f->ctx.server->trans.open_fail_slot.ready) ? 2U : 0U)
	    | (atomic_load(&f->channel->wa_slot.ready) ? 4U : 0U)
	    | (atomic_load(&f->channel->chan_reply_slot.ready) ? 8U : 0U);
}

static void
queue_retry_release(void *data)
{
	struct queue_retry_fixture *fixture = data;
	atomic_store(&fixture->cancelled, true);
	atomic_store(&fixture->owner_ready, true);
	atomic_store(&fixture->flush_ready, true);
	atomic_store(&fixture->second_release, true);
	atomic_store(&fixture->rekey_go, true);
	atomic_store(&fixture->marker_release, true);
	atomic_store(&fixture->stream_release, true);
}

static int
queue_retry_case(unsigned kind, bool gather, unsigned probe)
{
	struct queue_retry_fixture f = {.kind = kind, .rekey_result = DSSH_ERROR_INIT};
	atomic_init(&f.accepted, false);
	atomic_init(&f.peer_ready, false);
	atomic_init(&f.owner_ready, false);
	atomic_init(&f.flush_ready, false);
	atomic_init(&f.peer_done, false);
	atomic_init(&f.data_done, false);
	atomic_init(&f.marker_release, false);
	atomic_init(&f.stream_entered, false);
	atomic_init(&f.stream_release, false);
	atomic_init(&f.second_entered, false);
	atomic_init(&f.second_release, false);
	atomic_init(&f.rekey_go, false);
	atomic_init(&f.rekey_started, false);
	atomic_init(&f.bad, false);
	atomic_init(&f.cancelled, false);
	atomic_init(&f.merge_expired, false);
	atomic_init(&f.data_calls, 0);
	window_test_gather = gather;
	int setup = conn_setup_no_start(&f.ctx);
	window_test_gather = false;
	ASSERT_EQ(setup, 0);
	ASSERT_TRUE(f.ctx.server->trans.comp_s2c_active != NULL
	    && strcmp(f.ctx.server->trans.comp_s2c_active->name, "none") != 0);
	ASSERT_EQ(dssh_session_set_timeout(f.ctx.server, 30000), 0);
	ASSERT_EQ(dssh_session_set_timeout(f.ctx.client, 30000), 0);
	ASSERT_EQ(dssh_session_set_global_request_cb(f.ctx.server, queue_retry_global, &f), 0);
	ASSERT_EQ(dssh_session_start(f.ctx.server), 0);
	struct flow_fixture flow;
	flow_init(&flow, &f.ctx, queue_retry_release, &f);
	flow.tx.session = f.ctx.server;
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, queue_retry_accept, &f), 0);
	ASSERT_EQ(channel_worker_start(&flow.workers, 1, queue_retry_peer, &f), 0);
	ASSERT_TRUE(flow_checkpoint(FLOW_QUEUE_STARTED));
	bool valid = dssh_test_wait_bool(&f.peer_ready, true)
	    && dssh_test_wait_bool(&f.accepted, true);
	if (!valid) {
		mock_io_close_c2s(&f.ctx.io);
		mock_io_close_s2c(&f.ctx.io);
	}
	channel_worker_join(&flow.workers, 0);
	ASSERT_EQ(flow.workers.results[0], 0);
	valid = valid && f.channel != NULL;
	ASSERT_TRUE(valid);
	flow.tx.held = tx_acquire(f.ctx.server, SSH_MSG_IGNORE) == 0;
	valid = flow.tx.held;
	ASSERT_TRUE(valid);
	if (valid && kind == 12) {
		if (mtx_lock(&f.channel->buf_mtx) != thrd_success)
			abort();
		f.stream_cb = f.channel->zc_cb;
		f.stream_cbdata = f.channel->zc_cbdata;
		f.channel->zc_cb = queue_retry_stream;
		f.channel->zc_cbdata = &f;
		if (mtx_unlock(&f.channel->buf_mtx) != thrd_success)
			abort();
	}
	ASSERT_TRUE(flow_checkpoint(FLOW_QUEUE_OWNED));
	atomic_store(&f.owner_ready, true);
	if (valid && kind != 8 && kind != 9)
		valid = dssh_test_wait_bool(&f.ctx.server->trans.global_reply_slot.ready, true)
		    && dssh_test_wait_bool(&f.ctx.server->trans.open_fail_slot.ready, true);
	if (valid)
		valid = dssh_test_wait_bool(&f.channel->chan_reply_slot.ready, true);
	uint8_t data[24];
	if (valid && !queue_retry_merge_kind(kind) && kind != 12)
		valid = dssh_test_wait_bool(&f.data_done, true);
	if (valid && kind == 12)
		valid = dssh_test_wait_bool(&f.stream_entered, true);
	if (valid && queue_retry_merge_kind(kind))
		valid = dssh_test_wait_bool(&f.channel->wa_slot.ready, true)
		    && dssh_test_wait_bool(&f.second_entered, true);
	else if (valid) {
		int polled = dssh_chan_poll(f.channel, DSSH_POLL_READ, 3000);
		valid = polled >= 0 && (polled & DSSH_POLL_READ) != 0
		    && dssh_chan_read(f.channel, 0, data, sizeof(data)) == 24
		    && flow_bytes(data, sizeof(data), 24, 0x6d);
	}
	atomic_store(&f.stream_release, true);
	if (valid)
		valid = queue_retry_mask(&f) == ((kind == 8 || kind == 9) ? 12U : 15U);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(flow_checkpoint(FLOW_QUEUE_READY));
	size_t saved_capacity = f.ctx.server->trans.comp_tx_payload_limit;
	uint_fast32_t saved_count = atomic_load(&f.ctx.server->trans.tx_since_rekey);
	unsigned observed = 0;
	bool merger_started = false;
	if (valid) {
		if (queue_retry_merge_kind(kind)) {
			f.pause_entered = dssh_test_thrd_pause_mutex_after(&f.channel->buf_mtx, 1);
			flow.paused = true;
			ASSERT_EQ(channel_worker_start(&flow.workers, 0, queue_retry_merge, &f), 0);
			merger_started = true;
			ASSERT_TRUE(flow_checkpoint(FLOW_QUEUE_MERGER));
		}
		if (kind == 1 || kind == 10)
			atomic_store(&f.ctx.server->trans.tx_since_rekey, DSSH_REKEY_HARD_LIMIT);
		else if (kind == 2 || kind == 11)
			f.ctx.server->trans.comp_tx_payload_limit = 0;
		else if (kind == 3 || kind == 6)
			atomic_store(&f.ctx.server->trans.tx_since_rekey, DSSH_REKEY_HARD_LIMIT - 2);
		else if (kind == 4 || kind == 8 || kind == 9)
			f.ctx.server->trans.comp_tx_payload_limit = 8;
		else if (kind == 5)
			atomic_store(&f.ctx.server->trans.rekey_in_progress, true);
		else if (kind == 7)
			atomic_store(&f.ctx.server->trans.tx_since_rekey, DSSH_REKEY_HARD_LIMIT - 3);
		if (queue_retry_rekey_kind(kind)) {
			atomic_store(&f.rekey_go, true);
			valid = dssh_test_wait_bool(&f.rekey_started, true);
			uint8_t *payload = &f.ctx.server->trans.tx_packet[9];
			payload[0] = SSH_MSG_IGNORE;
			memset(payload + 1, 0, 4);
			int result = tx_gather_with_packet(f.ctx.server, f.ctx.server->trans.tx_packet, 5);
			valid = result == (kind == 10 ? DSSH_ERROR_REKEY_NEEDED : DSSH_ERROR_TOOLONG) && valid;
		}
		else {
			callback_release_tx(&flow.tx);
		}
		if (merger_started) {
			channel_worker_join(&flow.workers, 0);
			valid = valid && flow.workers.results[0] == 0 && f.merge_valid;
		}
		if (queue_retry_merge_kind(kind)) {
			(void)dssh_test_thrd_pause_mutex_after(NULL, 0);
			flow.paused = false;
			valid = valid && merger_started;
		}
		observed = queue_retry_mask(&f);
		unsigned wanted = (kind == 0 || kind == 12) ? 0U : kind == 3 || kind == 6 || kind == 8 || kind == 9
		    ? 12U : kind == 4 ? 14U : kind == 7 ? 8U : 15U;
		valid = observed == wanted && !dssh_session_is_terminated(f.ctx.server) && valid;
		if (mtx_lock(&f.channel->buf_mtx) != thrd_success)
			abort();
		uint32_t local = queue_retry_merge_kind(kind) ? 0U : kind == 0 || kind == 7 || kind == 12 ? 32U : 8U;
		uint32_t pending = queue_retry_merge_kind(kind) ? 32U : kind == 0 || kind == 7 || kind == 12 ? 0U : 24U;
		valid = valid && f.channel->local_window == local
		    && f.channel->window_adjust_pending == pending && f.channel->window_adjust_inflight == 0;
		if (mtx_unlock(&f.channel->buf_mtx) != thrd_success)
			abort();
		if (valid && probe != 0) {
			int64_t result;
			uint8_t ignored[] = {SSH_MSG_IGNORE, 0, 0, 0, 0};
			if (probe == 1)
				result = send_packet(f.ctx.server, ignored, sizeof(ignored), NULL);
			else if (probe == 2)
				result = dssh_chan_write(f.channel, 0, ignored, 1);
			else if (probe == 3) {
				uint8_t reply = SSH_MSG_REQUEST_SUCCESS;
				result = send_to_slot(f.ctx.server, &f.ctx.server->trans.global_reply_slot, &reply, 1);
			}
			else
				result = rekey(f.ctx.server);
			valid = result == (kind == 1 ? DSSH_ERROR_REKEY_NEEDED : DSSH_ERROR_TOOLONG)
			    && queue_retry_mask(&f) == wanted && !dssh_session_is_terminated(f.ctx.server)
			    && !atomic_load(&f.ctx.server->trans.rekey_in_progress)
			    && atomic_load(&f.channel->remote_window) == 1024;
		}
		ASSERT_TRUE(valid);
		ASSERT_TRUE(flow_checkpoint(FLOW_QUEUE_REFUSED));
		if (!flow.tx.held)
			flow.tx.held = tx_acquire(f.ctx.server, SSH_MSG_IGNORE) == 0;
		if (flow.tx.held) {
			f.ctx.server->trans.comp_tx_payload_limit = saved_capacity;
			unsigned sent = kind == 3 || kind == 6 ? 2U : kind == 4 ? 1U : kind == 7 ? 3U : 0U;
			if (saved_count > UINT_FAST32_MAX - sent)
				abort();
			uint_fast32_t restored = saved_count + sent;
			atomic_store(&f.ctx.server->trans.tx_since_rekey, restored);
			atomic_store(&f.ctx.server->trans.rekey_in_progress, false);
		}
		valid = valid && flow.tx.held;
	}
	atomic_store(&f.marker_release, true);
	atomic_store(&f.flush_ready, true);
	ASSERT_TRUE(valid);
	callback_release_tx(&flow.tx);
	if (f.channel != NULL && !dssh_session_is_terminated(f.ctx.server)) {
		uint8_t done[5];
		size_t pos = 0;
		done[pos++] = SSH_MSG_CHANNEL_SUCCESS;
		DSSH_PUT_U32(42, done, &pos);
		int ret = send_packet(f.ctx.server, done, pos, NULL);
		valid = ret == 0 && valid;
	}
	ASSERT_TRUE(valid);
	bool finished = queue_retry_wait_peer(&f);
	if (!finished) {
		mock_io_close_c2s(&f.ctx.io);
		mock_io_close_s2c(&f.ctx.io);
	}
	flow_finish(&flow);
	ASSERT_EQ(flow.workers.results[1], 0);
	if (queue_retry_rekey_kind(kind))
		valid = valid && f.rekey_result == 0
		    && dssh_test_wait_bool(&f.ctx.server->trans.rekey_in_progress, false);
	valid = valid && finished && f.peer_valid && !atomic_load(&f.bad) && queue_retry_mask(&f) == 0
	    && !dssh_session_is_terminated(f.ctx.server) && !dssh_session_is_terminated(f.ctx.client);
	if (!valid)
		ASSERT_TRUE(fprintf(stderr, "queued controls kind=%u gather=%u probe=%u retained=%u peer=%u rekey=%d finished=%u\n",
		    kind, gather ? 1U : 0U, probe, observed, f.peer_valid ? 1U : 0U, f.rekey_result, finished ? 1U : 0U) >= 0);
	conn_cleanup(&f.ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int test_queue_retry_packet_ordinary(void)
{
	return queue_retry_case(0, false, 0);
}

static int test_queue_retry_packet_key_refusal(void)
{
	return queue_retry_case(1, false, 0);
}

static int test_queue_retry_packet_size_refusal(void)
{
	return queue_retry_case(2, false, 0);
}

static int test_queue_retry_packet_key_partial(void)
{
	return queue_retry_case(3, false, 0);
}

static int test_queue_retry_packet_size_partial(void)
{
	return queue_retry_case(4, false, 0);
}

static int test_queue_retry_packet_phase_pause(void)
{
	return queue_retry_case(5, false, 0);
}

static int test_queue_retry_packet_concurrent_key(void)
{
	return queue_retry_case(6, false, 0);
}

static int test_queue_retry_packet_channel_key_partial(void)
{
	return queue_retry_case(7, false, 0);
}

static int test_queue_retry_packet_window_size(void)
{
	return queue_retry_case(8, false, 0);
}

static int test_queue_retry_packet_concurrent_size(void)
{
	return queue_retry_case(9, false, 0);
}

static int test_queue_retry_packet_key_rekey(void)
{
	return queue_retry_case(10, false, 0);
}

static int test_queue_retry_packet_size_rekey(void)
{
	return queue_retry_case(11, false, 0);
}

static int test_queue_retry_packet_key_probe_send(void)
{
	return queue_retry_case(1, false, 1);
}

static int test_queue_retry_packet_key_probe_write(void)
{
	return queue_retry_case(1, false, 2);
}

static int test_queue_retry_packet_key_probe_slot(void)
{
	return queue_retry_case(1, false, 3);
}

static int test_queue_retry_packet_key_probe_rekey(void)
{
	return queue_retry_case(1, false, 4);
}

static int test_queue_retry_packet_size_probe_send(void)
{
	return queue_retry_case(2, false, 1);
}

static int test_queue_retry_packet_size_probe_write(void)
{
	return queue_retry_case(2, false, 2);
}

static int test_queue_retry_packet_size_probe_slot(void)
{
	return queue_retry_case(2, false, 3);
}

static int test_queue_retry_packet_size_probe_rekey(void)
{
	return queue_retry_case(2, false, 4);
}

static int test_queue_retry_gather_ordinary(void)
{
	return queue_retry_case(0, true, 0);
}

static int test_queue_retry_gather_key_refusal(void)
{
	return queue_retry_case(1, true, 0);
}

static int test_queue_retry_gather_size_refusal(void)
{
	return queue_retry_case(2, true, 0);
}

static int test_queue_retry_gather_key_partial(void)
{
	return queue_retry_case(3, true, 0);
}

static int test_queue_retry_gather_size_partial(void)
{
	return queue_retry_case(4, true, 0);
}

static int test_queue_retry_gather_phase_pause(void)
{
	return queue_retry_case(5, true, 0);
}

static int test_queue_retry_gather_concurrent_key(void)
{
	return queue_retry_case(6, true, 0);
}

static int test_queue_retry_gather_channel_key_partial(void)
{
	return queue_retry_case(7, true, 0);
}

static int test_queue_retry_gather_window_size(void)
{
	return queue_retry_case(8, true, 0);
}

static int test_queue_retry_gather_concurrent_size(void)
{
	return queue_retry_case(9, true, 0);
}

static int test_queue_retry_gather_key_rekey(void)
{
	return queue_retry_case(10, true, 0);
}

static int test_queue_retry_gather_size_rekey(void)
{
	return queue_retry_case(11, true, 0);
}

static int test_queue_retry_gather_key_probe_send(void)
{
	return queue_retry_case(1, true, 1);
}

static int test_queue_retry_gather_key_probe_write(void)
{
	return queue_retry_case(1, true, 2);
}

static int test_queue_retry_gather_key_probe_slot(void)
{
	return queue_retry_case(1, true, 3);
}

static int test_queue_retry_gather_key_probe_rekey(void)
{
	return queue_retry_case(1, true, 4);
}

static int test_queue_retry_gather_size_probe_send(void)
{
	return queue_retry_case(2, true, 1);
}

static int test_queue_retry_gather_size_probe_write(void)
{
	return queue_retry_case(2, true, 2);
}

static int test_queue_retry_gather_size_probe_slot(void)
{
	return queue_retry_case(2, true, 3);
}

static int test_queue_retry_gather_size_probe_rekey(void)
{
	return queue_retry_case(2, true, 4);
}

static int test_queue_retry_packet_stream_callback(void)
{
	return queue_retry_case(12, false, 0);
}

static int test_queue_retry_gather_stream_callback(void)
{
	return queue_retry_case(12, true, 0);
}
