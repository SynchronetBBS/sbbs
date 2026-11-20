/* Fixed control layouts: real native channels, deterministic dispatch checks
 * plus authenticated wire peers. All native synchronization is initialized. */
static uint8_t
control_frame_type(unsigned kind)
{
	static const uint8_t types[] = {SSH_MSG_CHANNEL_WINDOW_ADJUST, SSH_MSG_CHANNEL_EOF,
	    SSH_MSG_CHANNEL_CLOSE, SSH_MSG_CHANNEL_SUCCESS, SSH_MSG_CHANNEL_FAILURE};
	return types[kind];
}

static void
control_frame_packet(uint8_t *packet, uint8_t type, uint32_t recipient)
{
	memset(packet, 0xa5, 32);
	packet[0] = type;
	size_t pos = 1;
	DSSH_PUT_U32(recipient, packet, &pos);
	DSSH_PUT_U32(17, packet, &pos);
}

static bool
control_frame_window_ready(dssh_channel ch)
{
	struct timespec start;
	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
		return false;
	while (atomic_load(&ch->remote_window) == 0) {
		if (!dssh_test_wait_tick(&start))
			return false;
	}
	return true;
}

static void
control_frame_allocate(size_t count, size_t size, void *opaque)
{
	(void)count;
	(void)size;
	unsigned *calls = opaque;
	if (*calls < UINT_MAX)
		(*calls)++;
}

static int
control_frame_direct(unsigned kind, bool setup, bool zc)
{
	struct event_fixture fixture = {0};
	struct event_capture capture = {0};
	event_capture_init(&capture, false);
	ASSERT_TRUE(event_setup(&fixture, &capture, zc));
	dssh_channel ch = fixture.open.server_ch;
	ASSERT_TRUE(control_frame_window_ready(ch));
	ASSERT_TRUE(setup_test_mode(ch, setup));
	uint8_t type = control_frame_type(kind);
	size_t expected = kind == 0 ? 9 : 5;
	uint8_t packet[32];
	control_frame_packet(packet, type, ch->local_id);
	ASSERT_EQ(mtx_lock(&ch->buf_mtx), thrd_success);
	ch->request_pending = true;
	ch->request_responded = false;
	ch->request_success = false;
	uint32_t window = atomic_load(&ch->remote_window);
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	bool valid = true;
	unsigned rejected = 0;
	unsigned allocations = 0;
	dssh_test_alloc_observe_allocations(control_frame_allocate, &allocations);
	for (size_t length = 0; length <= expected + 16; length++) {
		if (length == expected)
			continue;
		unsigned before_allocations = allocations;
		int ret = dssh_test_demux_dispatch(ch->sess, type, packet, length);
		bool unchanged = before_allocations == allocations;
		if (mtx_lock(&ch->buf_mtx) != thrd_success)
			abort();
		bool ready = ch->setup_ready;
		unchanged = unchanged && !ready && !ch->eof_received && !ch->close_received
		    && ch->request_pending && !ch->request_responded && !ch->request_success
		    && atomic_load(&ch->remote_window) == window;
		if (mtx_unlock(&ch->buf_mtx) != thrd_success)
			abort();
		valid = ret == DSSH_ERROR_PARSE && unchanged && atomic_load(&capture.count) == 0 && valid;
		if (ret == DSSH_ERROR_PARSE)
			rejected++;
		/* Before-source controls must never leave an occupied mailbox for
		 * the next direct dispatch. This is test cleanup, not accepted input. */
		if (ready && setup_release(ch->sess, ch) != 0)
			valid = false;
	}
	dssh_test_alloc_observe_allocations(NULL, NULL);
	valid = valid && rejected == expected + 16;
	/* Retire the controlled request state before ordinary session cleanup. */
	if (mtx_lock(&ch->buf_mtx) != thrd_success)
		abort();
	ch->request_pending = false;
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	bool restored = setup_test_mode(ch, false);
	valid = valid && restored && !dssh_session_is_terminated(ch->sess)
	    && !atomic_load(&capture.bad);
	conn_cleanup(&fixture.ctx);
	if (!valid)
		fprintf(stderr, "control framing kind=%u setup=%u zc=%u rejected=%u\n",
		    kind, setup ? 1U : 0U, zc ? 1U : 0U, rejected);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
control_frame_absent(bool stale)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	uint32_t recipient = stale ? fixture.open.server_ch->local_id : UINT32_MAX;
	if (stale) {
		ASSERT_EQ(dssh_chan_close(fixture.open.server_ch, -1), 0);
		fixture.open.server_ch = NULL;
		ASSERT_EQ(dssh_chan_close(fixture.open.client_ch, -1), 0);
		fixture.open.client_ch = NULL;
	}
	bool valid = true;
	for (unsigned kind = 0; kind < 5; kind++) {
		uint8_t type = control_frame_type(kind);
		uint8_t packet[32];
		control_frame_packet(packet, type, recipient);
		size_t expected = kind == 0 ? 9 : 5;
		for (size_t length = 0; length <= expected + 16; length++) {
			int ret = dssh_test_demux_dispatch(fixture.ctx.server, type, packet, length);
			valid = ret == (length == expected ? 0 : DSSH_ERROR_PARSE) && valid;
		}
	}
	valid = valid && !dssh_session_is_terminated(fixture.ctx.server);
	conn_cleanup(&fixture.ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
control_frame_valid(bool setup, bool zc)
{
	struct event_fixture fixture = {0};
	struct event_capture capture = {0};
	event_capture_init(&capture, false);
	ASSERT_TRUE(event_setup(&fixture, &capture, zc));
	dssh_channel ch = fixture.open.server_ch;
	ASSERT_TRUE(control_frame_window_ready(ch));
	ASSERT_TRUE(setup_test_mode(ch, setup));
	bool valid = true;
	uint32_t window = atomic_load(&ch->remote_window);
	ASSERT_TRUE(window <= UINT32_MAX - 17);
	for (unsigned kind = 0; kind < 5; kind++) {
		uint8_t packet[32];
		uint8_t type = control_frame_type(kind);
		control_frame_packet(packet, type, ch->local_id);
		size_t length = kind == 0 ? 9 : 5;
		if (mtx_lock(&ch->buf_mtx) != thrd_success)
			abort();
		ch->request_pending = kind >= 3;
		ch->request_responded = false;
		if (mtx_unlock(&ch->buf_mtx) != thrd_success)
			abort();
		int ret = dssh_test_demux_dispatch(ch->sess, type, packet, length);
		valid = ret == 0 && valid;
		if (setup) {
			uint8_t got_type;
			uint8_t *got_packet;
			size_t got_length;
			/* The valid entry is occupied and can be consumed exactly once. */
			ret = setup_recv(ch->sess, ch, &got_type, &got_packet, &got_length);
			valid = ret == 0 && got_type == type && got_length == length
			    && memcmp(got_packet, packet, length) == 0 && valid;
			if (setup_release(ch->sess, ch) != 0)
				valid = false;
		}
		if (mtx_lock(&ch->buf_mtx) != thrd_success)
			abort();
		if (!setup && kind == 0)
			valid = valid && atomic_load(&ch->remote_window) == window + 17;
		if (kind == 1)
			valid = valid && ch->eof_received;
		if (kind == 2)
			valid = valid && ch->close_received;
		if (!setup && kind >= 3)
			valid = valid && ch->request_responded && ch->request_success == (kind == 3);
		if (mtx_unlock(&ch->buf_mtx) != thrd_success)
			abort();
	}
	valid = valid && atomic_load(&capture.count) == (setup ? 0 : 2)
	    && !atomic_load(&capture.bad);
	bool restored = setup_test_mode(ch, false);
	valid = valid && restored;
	conn_cleanup(&fixture.ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int test_control_frame_stream_normal_window(void)
{
	return control_frame_direct(0, false, false);
}

static int test_control_frame_stream_normal_eof(void)
{
	return control_frame_direct(1, false, false);
}

static int test_control_frame_stream_normal_close(void)
{
	return control_frame_direct(2, false, false);
}

static int test_control_frame_stream_normal_success(void)
{
	return control_frame_direct(3, false, false);
}

static int test_control_frame_stream_normal_failure(void)
{
	return control_frame_direct(4, false, false);
}

static int test_control_frame_stream_normal_valid(void)
{
	return control_frame_valid(false, false);
}

static int test_control_frame_stream_setup_window(void)
{
	return control_frame_direct(0, true, false);
}

static int test_control_frame_stream_setup_eof(void)
{
	return control_frame_direct(1, true, false);
}

static int test_control_frame_stream_setup_close(void)
{
	return control_frame_direct(2, true, false);
}

static int test_control_frame_stream_setup_success(void)
{
	return control_frame_direct(3, true, false);
}

static int test_control_frame_stream_setup_failure(void)
{
	return control_frame_direct(4, true, false);
}

static int test_control_frame_stream_setup_valid(void)
{
	return control_frame_valid(true, false);
}

static int test_control_frame_zc_normal_window(void)
{
	return control_frame_direct(0, false, true);
}

static int test_control_frame_zc_normal_eof(void)
{
	return control_frame_direct(1, false, true);
}

static int test_control_frame_zc_normal_close(void)
{
	return control_frame_direct(2, false, true);
}

static int test_control_frame_zc_normal_success(void)
{
	return control_frame_direct(3, false, true);
}

static int test_control_frame_zc_normal_failure(void)
{
	return control_frame_direct(4, false, true);
}

static int test_control_frame_zc_normal_valid(void)
{
	return control_frame_valid(false, true);
}

static int test_control_frame_zc_setup_window(void)
{
	return control_frame_direct(0, true, true);
}

static int test_control_frame_zc_setup_eof(void)
{
	return control_frame_direct(1, true, true);
}

static int test_control_frame_zc_setup_close(void)
{
	return control_frame_direct(2, true, true);
}

static int test_control_frame_zc_setup_success(void)
{
	return control_frame_direct(3, true, true);
}

static int test_control_frame_zc_setup_failure(void)
{
	return control_frame_direct(4, true, true);
}

static int test_control_frame_zc_setup_valid(void)
{
	return control_frame_valid(true, true);
}

static int test_control_frame_unknown(void)
{
	return control_frame_absent(false);
}

static int test_control_frame_stale(void)
{
	return control_frame_absent(true);
}


struct control_wire_fixture {
	struct conn_ctx ctx;
	struct dssh_chan_accept_cbs cbs;
	dssh_channel accepted;
	atomic_bool accepted_ready;
	atomic_bool server_done;
	atomic_bool peer_done;
	atomic_uint events;
	bool setup;
	bool reply_case;
	unsigned kind;
	bool peer_valid;
	int request_result;
};

static void
control_wire_event(dssh_channel ch, const struct dssh_chan_event *event, void *opaque)
{
	(void)ch;
	(void)event;
	struct control_wire_fixture *f = opaque;
	atomic_fetch_add(&f->events, 1);
}

static int
control_wire_terminal(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)ch;
	(void)params;
	(void)opaque;
	result->max_window = 4096;
	return 0;
}

static int
control_wire_server(void *opaque)
{
	struct control_wire_fixture *f = opaque;
	f->accepted = dssh_chan_accept(f->ctx.server, &f->cbs, 3000);
	atomic_store(&f->accepted_ready, true);
	if (f->accepted != NULL && f->reply_case)
		f->request_result = send_channel_request_wait(f->ctx.server, f->accepted,
		    "audit-control", NULL, 0);
	atomic_store(&f->server_done, true);
	return 0;
}

static bool
control_wire_sentinel(dssh_session sess, uint32_t remote)
{
	const uint8_t name[] = "unknown";
	return request_frame_send(sess, remote, name, sizeof(name) - 1, 1, NULL, 0) == 0
	    && request_frame_receive(sess, SSH_MSG_CHANNEL_FAILURE, 42, NULL);
}

static bool
control_wire_bad(dssh_session sess, uint32_t remote, unsigned kind)
{
	uint8_t type = control_frame_type(kind);
	uint8_t packet[32];
	control_frame_packet(packet, type, remote);
	size_t expected = kind == 0 ? 9 : 5;
	for (size_t length = 1; length <= expected + 16; length++) {
		if (length != expected && send_packet(sess, packet, length, NULL) != 0)
			return false;
	}
	return true;
}

static bool
control_wire_pending_request(dssh_session sess)
{
	uint8_t type;
	uint8_t *packet;
	size_t length;
	int ret = recv_packet(sess, &type, &packet, &length);
	return ret == 0 && type == SSH_MSG_CHANNEL_REQUEST && length == 23
	    && DSSH_GET_U32(packet + 1) == 42 && DSSH_GET_U32(packet + 5) == 13
	    && memcmp(packet + 9, "audit-control", 13) == 0 && packet[22] == 1;
}

static int
control_wire_peer(void *opaque)
{
	struct control_wire_fixture *f = opaque;
	dssh_session sess = f->ctx.client;
	uint32_t remote = 0;
	bool valid = accept_failure_open(sess, 42) == 0
	    && request_frame_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 42, &remote);
	if (valid && !f->setup)
		valid = request_frame_exec(sess, remote, 42)
		    && dssh_test_wait_bool(&f->accepted_ready, true);
	if (valid && f->reply_case)
		valid = control_wire_pending_request(sess);
	if (valid)
		valid = control_wire_bad(sess, remote, f->kind) && control_wire_sentinel(sess, remote);
	if (valid && f->reply_case) {
		dssh_channel ch = f->accepted;
		if (mtx_lock(&ch->buf_mtx) != thrd_success)
			abort();
		valid = ch->request_pending && !ch->request_responded && !ch->request_success
		    && !atomic_load(&f->server_done);
		if (mtx_unlock(&ch->buf_mtx) != thrd_success)
			abort();
		uint8_t packet[32];
		control_frame_packet(packet, control_frame_type(f->kind), remote);
		if (valid)
			valid = send_packet(sess, packet, 5, NULL) == 0;
	}
	else if (valid) {
		/* Valid window credit and a following request complete in wire order. */
		uint8_t packet[32];
		control_frame_packet(packet, SSH_MSG_CHANNEL_WINDOW_ADJUST, remote);
		valid = send_packet(sess, packet, 9, NULL) == 0 && control_wire_sentinel(sess, remote);
		if (valid && f->setup)
			valid = request_frame_exec(sess, remote, 42);
	}
	f->peer_valid = valid;
	atomic_store(&f->peer_done, true);
	return 0;
}

static int
control_frame_wire(unsigned kind, bool setup, bool reply_case)
{
	struct control_wire_fixture f = {.kind = kind, .setup = setup, .reply_case = reply_case,
	    .request_result = DSSH_ERROR_INIT};
	atomic_init(&f.accepted_ready, false);
	atomic_init(&f.server_done, false);
	atomic_init(&f.peer_done, false);
	atomic_init(&f.events, 0);
	ASSERT_EQ(conn_setup_no_start(&f.ctx), 0);
	ASSERT_EQ(dssh_session_set_timeout(f.ctx.server, 3000), 0);
	ASSERT_EQ(dssh_session_set_event_cb(f.ctx.server, control_wire_event, &f), 0);
	ASSERT_EQ(dssh_session_start(f.ctx.server), 0);
	f.cbs = accept_cbs_all;
	f.cbs.exec = control_wire_terminal;
	f.cbs.cbdata = &f;
	struct channel_workers workers;
	channel_workers_init(&workers, &f.ctx);
	ASSERT_EQ(channel_worker_start(&workers, 0, control_wire_server, &f), 0);
	ASSERT_EQ(channel_worker_start(&workers, 1, control_wire_peer, &f), 0);
	unsigned variant = kind * 4 + (setup ? 2U : 0U) + (reply_case ? 1U : 0U);
	ASSERT_TRUE(wire_checkpoint(WIRE_CONTROL_FRAME, variant, WIRE_STARTED));
	bool valid = dssh_test_wait_bool(&f.peer_done, true)
	    && dssh_test_wait_bool(&f.server_done, true);
	if (!valid || !f.peer_valid) {
		mock_io_close_c2s(&f.ctx.io);
		mock_io_close_s2c(&f.ctx.io);
	}
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_EQ(workers.results[1], 0);
	valid = valid && f.peer_valid && f.accepted != NULL && atomic_load(&f.events) == 0
	    && !dssh_session_is_terminated(f.ctx.server) && !dssh_session_is_terminated(f.ctx.client);
	if (f.accepted != NULL) {
		if (mtx_lock(&f.accepted->buf_mtx) != thrd_success)
			abort();
		valid = valid && !f.accepted->eof_received && !f.accepted->close_received
		    && !f.accepted->setup_ready && !f.accepted->request_pending
		    && atomic_load(&f.accepted->remote_window) == (reply_case ? 1024 : 1041);
		if (mtx_unlock(&f.accepted->buf_mtx) != thrd_success)
			abort();
	}
	if (reply_case)
		valid = valid && f.request_result == (kind == 3 ? 0 : DSSH_ERROR_REJECTED);
	if (!valid)
		fprintf(stderr, "control wire kind=%u setup=%u reply=%u peer=%u events=%u result=%d\n",
		    kind, setup ? 1U : 0U, reply_case ? 1U : 0U, f.peer_valid ? 1U : 0U,
		    atomic_load(&f.events), f.request_result);
	conn_cleanup(&f.ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int test_control_frame_wire_normal_window(void)
{
	return control_frame_wire(0, false, false);
}

static int test_control_frame_wire_normal_eof(void)
{
	return control_frame_wire(1, false, false);
}

static int test_control_frame_wire_normal_close(void)
{
	return control_frame_wire(2, false, false);
}

static int test_control_frame_wire_normal_success(void)
{
	return control_frame_wire(3, false, false);
}

static int test_control_frame_wire_normal_failure(void)
{
	return control_frame_wire(4, false, false);
}

static int test_control_frame_wire_setup_window(void)
{
	return control_frame_wire(0, true, false);
}

static int test_control_frame_wire_setup_eof(void)
{
	return control_frame_wire(1, true, false);
}

static int test_control_frame_wire_setup_close(void)
{
	return control_frame_wire(2, true, false);
}

static int test_control_frame_wire_setup_success(void)
{
	return control_frame_wire(3, true, false);
}

static int test_control_frame_wire_setup_failure(void)
{
	return control_frame_wire(4, true, false);
}

static int test_control_frame_wire_pending_success(void)
{
	return control_frame_wire(3, false, true);
}

static int test_control_frame_wire_pending_failure(void)
{
	return control_frame_wire(4, false, true);
}
