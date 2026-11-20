/* Native authenticated peer owns client transport RX; only server demux runs. */
struct request_frame_fixture {
	struct conn_ctx ctx;
	struct dssh_chan_accept_cbs cbs;
	dssh_channel accepted[2];
	atomic_bool peer_done;
	atomic_bool server_done;
	atomic_uint events;
	atomic_uint callbacks;
	bool setup;
	unsigned kind;
	bool peer_valid;
};

static int
request_frame_terminal(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)ch;
	(void)params;
	struct request_frame_fixture *fixture = opaque;
	atomic_fetch_add(&fixture->callbacks, 1);
	result->max_window = 4096;
	return 0;
}

static void
request_frame_event(dssh_channel ch, const struct dssh_chan_event *event, void *opaque)
{
	(void)ch;
	(void)event;
	struct request_frame_fixture *fixture = opaque;
	atomic_fetch_add(&fixture->events, 1);
}

static int
request_frame_server(void *opaque)
{
	struct request_frame_fixture *fixture = opaque;
	fixture->accepted[0] = dssh_chan_accept(fixture->ctx.server, &fixture->cbs, 3000);
	if (fixture->kind == 11 && !fixture->setup && fixture->accepted[0] != NULL)
		fixture->accepted[1] = dssh_chan_accept(fixture->ctx.server, &fixture->cbs, 3000);
	atomic_store(&fixture->server_done, true);
	return 0;
}

static bool
request_frame_receive(dssh_session sess, uint8_t wanted, uint32_t id, uint32_t *remote)
{
	uint8_t type = 0;
	uint8_t *packet = NULL;
	size_t length = 0;
	int ret = recv_packet(sess, &type, &packet, &length);
	if (wanted == SSH_MSG_DISCONNECT)
		return ret == DSSH_ERROR_TERMINATED && type == wanted && length >= 5
		    && DSSH_GET_U32(packet + 1) == SSH_DISCONNECT_PROTOCOL_ERROR;
	if (ret != 0 || type != wanted || length < 5 || DSSH_GET_U32(packet + 1) != id)
		return false;
	if (wanted == SSH_MSG_CHANNEL_OPEN_CONFIRMATION) {
		if (length != 17 || DSSH_GET_U32(packet + 9) != 0 || DSSH_GET_U32(packet + 13) == 0)
			return false;
		*remote = DSSH_GET_U32(packet + 5);
		return true;
	}
	if (wanted == SSH_MSG_CHANNEL_WINDOW_ADJUST)
		return length == 9 && DSSH_GET_U32(packet + 5) == 4096;
	return length == 5;
}

static int
request_frame_send(dssh_session sess, uint32_t remote, const uint8_t *name,
    size_t name_len, uint8_t reply, const uint8_t *body, size_t body_len)
{
	uint8_t packet[160];
	if (name_len > 80 || body_len > 32)
		return DSSH_ERROR_INVALID;
	uint32_t wire_name = (uint32_t)name_len;
	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(remote, packet, &pos);
	DSSH_PUT_U32(wire_name, packet, &pos);
	if (name_len != 0)
		memcpy(packet + pos, name, name_len);
	pos += name_len;
	packet[pos++] = reply;
	if (body_len != 0)
		memcpy(packet + pos, body, body_len);
	pos += body_len;
	return send_packet(sess, packet, pos, NULL);
}

static bool
request_frame_exec(dssh_session sess, uint32_t remote, uint32_t id)
{
	const uint8_t name[] = "exec";
	uint8_t body[8];
	size_t pos = 0;
	DSSH_PUT_U32(4, body, &pos);
	memcpy(body + pos, "test", 4);
	return request_frame_send(sess, remote, name, sizeof(name) - 1, 1, body, sizeof(body)) == 0
	    && request_frame_receive(sess, SSH_MSG_CHANNEL_SUCCESS, id, NULL)
	    && request_frame_receive(sess, SSH_MSG_CHANNEL_WINDOW_ADJUST, id, NULL);
}

static int
request_frame_malformed(dssh_session sess, uint32_t remote, unsigned kind)
{
	uint8_t packet[128];
	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(remote, packet, &pos);
	if (kind == 0)
		return send_packet(sess, packet, pos, NULL);
	if (kind == 1) {
		DSSH_PUT_U32(8, packet, &pos);
		packet[pos++] = 'x';
		packet[pos++] = 'y';
		return send_packet(sess, packet, pos, NULL);
	}
	if (kind == 2) {
		DSSH_PUT_U32(5, packet, &pos);
		memcpy(packet + pos, "shell", 5);
		pos += 5;
		return send_packet(sess, packet, pos, NULL);
	}
	uint8_t name[65];
	memset(name, 'x', sizeof(name));
	size_t length = kind == 3 ? 0 : kind == 7 ? 65 : 1;
	if (kind == 4)
		name[0] = 0x80;
	if (kind == 5)
		name[0] = 0;
	if (kind == 6)
		name[0] = ',';
	/* Empty name deliberately has a false flag; others request a reply. */
	return request_frame_send(sess, remote, name, length, kind == 3 ? 0 : 1, NULL, 0);
}

static int
request_frame_peer(void *opaque)
{
	struct request_frame_fixture *fixture = opaque;
	dssh_session sess = fixture->ctx.client;
	uint32_t remote = 0;
	bool valid = accept_failure_open(sess, 42) == 0
	    && request_frame_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 42, &remote);
	if (valid && !fixture->setup && fixture->kind != 10)
		valid = request_frame_exec(sess, remote, 42);
	/* A normal-phase test starts after the first accept caller has finished
	 * its grant/send release, not merely when the peer sees those bytes. */
	if (valid && !fixture->setup && fixture->kind < 10)
		valid = dssh_test_wait_bool(&fixture->server_done, true);
	if (valid && fixture->kind < 8)
		valid = request_frame_malformed(sess, remote, fixture->kind) == 0
		    && request_frame_receive(sess, SSH_MSG_DISCONNECT, 0, NULL);
	else if (valid && fixture->kind < 10) {
		const uint8_t name[] = "break";
		const uint8_t body[] = {0, 0, 0, 1, 0};
		valid = request_frame_send(sess, remote, name, sizeof(name) - 1, 1,
		    body, fixture->kind == 8 ? 3 : sizeof(body)) == 0
		    && request_frame_receive(sess, SSH_MSG_CHANNEL_FAILURE, 42, NULL);
		const uint8_t unknown[] = "unknown";
		if (valid)
			valid = request_frame_send(sess, remote, unknown, sizeof(unknown) - 1, 1, NULL, 0) == 0
			    && request_frame_receive(sess, SSH_MSG_CHANNEL_FAILURE, 42, NULL);
	}
	else if (valid && fixture->kind == 10) {
		const uint8_t name[] = "shell";
		const uint8_t body[] = {0};
		uint8_t reply = fixture->setup ? 1 : 0;
		valid = request_frame_send(sess, remote, name, sizeof(name) - 1, reply, body, sizeof(body)) == 0;
		if (valid && reply != 0)
			valid = request_frame_receive(sess, SSH_MSG_CHANNEL_FAILURE, 42, NULL);
		if (valid)
			valid = request_frame_receive(sess, SSH_MSG_CHANNEL_CLOSE, 42, NULL)
			    && accept_failure_close(sess, remote) == 0
			    && accept_failure_open(sess, 43) == 0
			    && request_frame_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 43, &remote)
			    && request_frame_exec(sess, remote, 43);
	}
	else if (valid) {
		uint8_t name[64];
		memset(name, 'x', sizeof(name));
		const uint8_t local[] = "unknown@example.com";
		const uint8_t opaque_data[] = {0, 0x80, 0xff};
		valid = request_frame_send(sess, remote, local, sizeof(local) - 1, 0,
		    opaque_data, sizeof(opaque_data)) == 0
		    && request_frame_send(sess, remote, name, sizeof(name), 0xff,
		        opaque_data, sizeof(opaque_data)) == 0
		    && request_frame_receive(sess, SSH_MSG_CHANNEL_FAILURE, 42, NULL);
		if (valid && fixture->setup)
			valid = request_frame_exec(sess, remote, 42);
		else if (valid)
			valid = accept_failure_open(sess, 43) == 0
			    && request_frame_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 43, &remote)
			    && request_frame_exec(sess, remote, 43);
	}
	fixture->peer_valid = valid;
	atomic_store(&fixture->peer_done, true);
	return 0;
}

static int
request_frame_case(bool setup, unsigned kind)
{
	struct request_frame_fixture fixture = {.setup = setup, .kind = kind};
	atomic_init(&fixture.peer_done, false);
	atomic_init(&fixture.server_done, false);
	atomic_init(&fixture.events, 0);
	atomic_init(&fixture.callbacks, 0);
	ASSERT_EQ(conn_setup_no_start(&fixture.ctx), 0);
	ASSERT_EQ(dssh_session_set_timeout(fixture.ctx.server, 3000), 0);
	ASSERT_EQ(dssh_session_set_event_cb(fixture.ctx.server, request_frame_event, &fixture), 0);
	ASSERT_EQ(dssh_session_start(fixture.ctx.server), 0);
	fixture.cbs = accept_cbs_all;
	fixture.cbs.shell = request_frame_terminal;
	fixture.cbs.exec = request_frame_terminal;
	fixture.cbs.cbdata = &fixture;
	struct channel_workers workers;
	channel_workers_init(&workers, &fixture.ctx);
	ASSERT_EQ(channel_worker_start(&workers, 0, request_frame_server, &fixture), 0);
	ASSERT_EQ(channel_worker_start(&workers, 1, request_frame_peer, &fixture), 0);
	ASSERT_TRUE(wire_checkpoint(WIRE_REQUEST_FRAME, kind * 2 + (setup ? 1U : 0U), WIRE_STARTED));
	bool valid = dssh_test_wait_bool(&fixture.peer_done, true)
	    && dssh_test_wait_bool(&fixture.server_done, true);
	if (!valid || !fixture.peer_valid) {
		mock_io_close_c2s(&fixture.ctx.io);
		mock_io_close_s2c(&fixture.ctx.io);
	}
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_EQ(workers.results[1], 0);
	valid = valid && fixture.peer_valid && atomic_load(&fixture.events) == 0;
	if (kind < 8)
		valid = valid && dssh_test_wait_bool(&fixture.ctx.client->terminate, true)
		    && dssh_test_wait_bool(&fixture.ctx.server->terminate, true);
	else
		valid = valid && !dssh_session_is_terminated(fixture.ctx.client)
		    && !dssh_session_is_terminated(fixture.ctx.server) && fixture.accepted[0] != NULL;
	unsigned callbacks = kind == 11 && !setup ? 2U : kind < 8 && setup ? 0U : 1U;
	valid = valid && atomic_load(&fixture.callbacks) == callbacks;
	if (kind == 11 && !setup)
		valid = valid && fixture.accepted[1] != NULL && fixture.accepted[0] != fixture.accepted[1];
	conn_cleanup(&fixture.ctx);
	if (!valid)
		fprintf(stderr, "request framing setup=%u kind=%u peer=%u events=%u callbacks=%u\n",
		    setup ? 1U : 0U, kind, fixture.peer_valid ? 1U : 0U,
		    atomic_load(&fixture.events), atomic_load(&fixture.callbacks));
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#define REQUEST_FRAME_TEST(name, setup, kind) \
static int name(void) \
{ \
	return request_frame_case(setup, kind); \
}
REQUEST_FRAME_TEST(test_request_setup_short_length, true, 0)
REQUEST_FRAME_TEST(test_request_setup_short_name, true, 1)
REQUEST_FRAME_TEST(test_request_setup_short_reply, true, 2)
REQUEST_FRAME_TEST(test_request_setup_empty, true, 3)
REQUEST_FRAME_TEST(test_request_setup_nonascii, true, 4)
REQUEST_FRAME_TEST(test_request_setup_zero, true, 5)
REQUEST_FRAME_TEST(test_request_setup_comma, true, 6)
REQUEST_FRAME_TEST(test_request_setup_long, true, 7)
REQUEST_FRAME_TEST(test_request_normal_short_length, false, 0)
REQUEST_FRAME_TEST(test_request_normal_short_name, false, 1)
REQUEST_FRAME_TEST(test_request_normal_short_reply, false, 2)
REQUEST_FRAME_TEST(test_request_normal_empty, false, 3)
REQUEST_FRAME_TEST(test_request_normal_nonascii, false, 4)
REQUEST_FRAME_TEST(test_request_normal_zero, false, 5)
REQUEST_FRAME_TEST(test_request_normal_comma, false, 6)
REQUEST_FRAME_TEST(test_request_normal_long, false, 7)
REQUEST_FRAME_TEST(test_request_break_short, false, 8)
REQUEST_FRAME_TEST(test_request_break_trailer, false, 9)
/* For kind 10 the boolean selects reply versus no reply, both during setup. */
REQUEST_FRAME_TEST(test_request_shell_reply, true, 10)
REQUEST_FRAME_TEST(test_request_shell_no_reply, false, 10)
REQUEST_FRAME_TEST(test_request_setup_unknown, true, 11)
REQUEST_FRAME_TEST(test_request_normal_unknown, false, 11)
#undef REQUEST_FRAME_TEST

static int
request_event_length_case(bool zc)
{
	struct event_capture capture = {0};
	event_capture_init(&capture, false);
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, &capture, zc));
	ASSERT_EQ(dssh_chan_set_event_cb(fixture.open.client_ch, capture_event, &capture), 0);
	int types[] = {DSSH_EVENT_SIGNAL, DSSH_EVENT_WINDOW_CHANGE, TEST_CHANNEL_BREAK_REQUEST,
	    DSSH_EVENT_EXIT_STATUS, DSSH_EVENT_EXIT_SIGNAL};
	bool valid = true;
	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		dssh_channel target = types[i] == DSSH_EVENT_EXIT_STATUS || types[i] == DSSH_EVENT_EXIT_SIGNAL
		    ? fixture.open.client_ch : fixture.open.server_ch;
		uint8_t packet[128];
		size_t length = 0;
		ASSERT_EQ(event_packet(target, types[i], packet, sizeof(packet), &length), 0);
		ASSERT_TRUE(length > 0 && length < sizeof(packet));
		packet[length] = 0;
		int ret = dssh_test_demux_dispatch(target->sess, packet[0], packet, length + 1);
		if (mtx_lock(&fixture.open.client_ch->buf_mtx) != thrd_success)
			abort();
		bool exit_received = fixture.open.client_ch->exit_code_received;
		if (mtx_unlock(&fixture.open.client_ch->buf_mtx) != thrd_success)
			abort();
		valid = ret == DSSH_ERROR_PARSE && atomic_load(&capture.count) == 0 && !exit_received && valid;
	}
	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		dssh_channel target = types[i] == DSSH_EVENT_EXIT_STATUS || types[i] == DSSH_EVENT_EXIT_SIGNAL
		    ? fixture.open.client_ch : fixture.open.server_ch;
		uint8_t packet[128];
		size_t length = 0;
		ASSERT_EQ(event_packet(target, types[i], packet, sizeof(packet), &length), 0);
		ASSERT_TRUE(length > 0 && length <= sizeof(packet));
		int ret = dssh_test_demux_dispatch(target->sess, packet[0], packet, length);
		valid = ret == 0 && valid;
	}
	valid = valid && atomic_load(&capture.count) == 4 && !atomic_load(&capture.bad)
	    && !dssh_session_is_terminated(fixture.ctx.client) && !dssh_session_is_terminated(fixture.ctx.server);
	conn_cleanup(&fixture.ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int test_request_event_lengths_stream(void)
{
	return request_event_length_case(false);
}

static int test_request_event_lengths_zc(void)
{
	return request_event_length_case(true);
}

static int
test_request_name_byte_and_boolean(void)
{
	uint8_t packet[80];
	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_REQUEST;
	DSSH_PUT_U32(7, packet, &pos);
	DSSH_PUT_U32(1, packet, &pos);
	packet[pos++] = 'x';
	packet[pos++] = 0;
	for (unsigned value = 0; value <= 255; value++) {
		uint8_t byte = (uint8_t)value;
		packet[9] = byte;
		const uint8_t *name;
		uint32_t length;
		bool reply;
		const uint8_t *body;
		size_t body_len;
		int ret = parse_channel_request(packet, pos, &name, &length, &reply, &body, &body_len);
		bool permitted = value > 32 && value < 127 && value != ',';
		ASSERT_EQ(ret, permitted ? 0 : DSSH_ERROR_PARSE);
		if (permitted) {
			ASSERT_EQ_U(length, 1);
			ASSERT_EQ(name[0], byte);
			ASSERT_FALSE(reply);
			ASSERT_EQ_U(body_len, 0);
		}
	}
	packet[9] = 'x';
	for (unsigned value = 0; value <= 255; value++) {
		uint8_t byte = (uint8_t)value;
		packet[10] = byte;
		const uint8_t *name;
		uint32_t length;
		bool reply;
		const uint8_t *body;
		size_t body_len;
		ASSERT_EQ(parse_channel_request(packet, pos, &name, &length, &reply, &body, &body_len), 0);
		ASSERT_EQ(reply, value != 0);
	}
	return TEST_PASS;
}
