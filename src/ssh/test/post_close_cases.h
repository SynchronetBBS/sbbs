/* Included after the native peer channel helpers. */

static dssh_channel
post_close_accept(struct conn_ctx *ctx, uint32_t remote)
{
	struct dssh_incoming_open incoming;
	if (peer_id_send_open(ctx->client, remote) != 0
	    || accept_incoming(ctx->server, &incoming, 3000) != 0
	    || incoming.peer_channel != remote)
		return NULL;
	dssh_channel ch = accept_channel_init(ctx->server, &incoming);
	if (ch == NULL || !peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 17, remote))
		return NULL;
	return ch;
}

static int
post_close_receive_close(struct conn_ctx *ctx, dssh_channel ch)
{
	uint8_t close[5];
	size_t pos = 0;
	close[pos++] = SSH_MSG_CHANNEL_CLOSE;
	DSSH_PUT_U32(ch->local_id, close, &pos);
	ASSERT_OK(send_packet(ctx->client, close, pos, NULL));
	ASSERT_TRUE(channel_id_wire_barrier(ctx->client));
	ASSERT_OK(setup_release(ctx->server, ch));
	return TEST_PASS;
}

static int
post_close_capacity(struct conn_ctx *ctx, dssh_channel ch)
{
	dssh_session sess = ctx->server;
	ASSERT_TRUE(compression_fixture_selected(ctx->client, sess));
	ASSERT_OK(tx_acquire(sess, SSH_MSG_IGNORE));
	size_t previous = sess->trans.comp_tx_payload_limit;
	sess->trans.comp_tx_payload_limit = 1;
	uint8_t expected[40];
	memset(expected, 0xa5, sizeof(expected));
	memcpy(&sess->trans.tx_packet[9], expected, sizeof(expected));
	tx_unlock_with_slots(sess);

	int window = dssh_chan_send_window_change(ch, 80, 24, 0, 0);
	int brk = dssh_chan_send_break(ch, 100);
	ASSERT_OK(tx_acquire(sess, SSH_MSG_IGNORE));
	sess->trans.comp_tx_payload_limit = previous;
	bool intact = memcmp(&sess->trans.tx_packet[9], expected, sizeof(expected)) == 0;
	tx_unlock_with_slots(sess);

	ASSERT_EQ(window, DSSH_ERROR_TOOLONG);
	ASSERT_EQ(brk, DSSH_ERROR_TOOLONG);
	ASSERT_TRUE(intact);
	return TEST_PASS;
}

static int
post_close_direct_run(struct conn_ctx *ctx)
{
	const uint32_t remote = UINT32_MAX;
	ASSERT_OK(dssh_session_start(ctx->server));
	dssh_channel old = post_close_accept(ctx, remote);
	ASSERT_NOT_NULL(old);
	ASSERT_OK(post_close_receive_close(ctx, old));
	ASSERT_OK(conn_close(ctx->server, old));
	ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_CLOSE, 5, remote));
	dssh_channel replacement = post_close_accept(ctx, remote);
	ASSERT_NOT_NULL(replacement);

	int signal = dssh_chan_send_signal(old, "TERM");
	int resize = dssh_chan_send_window_change(old, 80, 24, 0, 0);
	int brk = dssh_chan_send_break(old, 100);
	int credit = send_window_adjust(ctx->server, old, 1);
	int eof = dssh_chan_shutwr(old);
	int close = dssh_chan_close(old, 42);
	bool ordered = channel_id_wire_barrier(ctx->client);
	printf("post_close_direct signal=%d resize=%d break=%d credit=%d eof=%d close=%d ordered=%d\n",
	    signal, resize, brk, credit, eof, close, ordered);
	ASSERT_TRUE(ordered);
	ASSERT_EQ(signal, DSSH_ERROR_TERMINATED);
	ASSERT_EQ(resize, DSSH_ERROR_TERMINATED);
	ASSERT_EQ(brk, DSSH_ERROR_TERMINATED);
	ASSERT_EQ(credit, DSSH_ERROR_TERMINATED);
	ASSERT_OK(eof);
	ASSERT_OK(close);
	ASSERT_FALSE(dssh_session_is_terminated(ctx->server));
	const char *compression = getenv("DSSH_TEST_COMPRESSION");
	if (compression != NULL && strcmp(compression, "test-growth") == 0)
		ASSERT_OK(post_close_capacity(ctx, replacement));
	ASSERT_OK(dssh_chan_send_break(replacement, 100));
	ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_REQUEST, 19, remote));
	ASSERT_OK(post_close_receive_close(ctx, replacement));
	ASSERT_OK(dssh_chan_close(replacement, 42));
	uint8_t type;
	uint8_t *payload;
	size_t len;
	ASSERT_OK(recv_packet(ctx->client, &type, &payload, &len));
	ASSERT_EQ(type, SSH_MSG_CHANNEL_REQUEST);
	ASSERT_EQ_U(len, 25);
	ASSERT_EQ_U(DSSH_GET_U32(&payload[1]), remote);
	ASSERT_MEM_EQ(&payload[9], "exit-status", 11);
	ASSERT_EQ_U(DSSH_GET_U32(&payload[21]), 42);
	ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_EOF, 5, remote));
	ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_CLOSE, 5, remote));
	return TEST_PASS;
}

static int
test_post_close_direct(void)
{
	for (unsigned gather = 0; gather < 2; gather++) {
		struct conn_ctx ctx;
		window_test_gather = gather != 0;
		int setup = conn_setup_mode(&ctx, false);
		window_test_gather = false;
		ASSERT_OK(setup);
		int ret = post_close_direct_run(&ctx);
		conn_cleanup(&ctx);
		if (ret != TEST_PASS)
			return ret;
	}
	return TEST_PASS;
}

static void
post_close_build_failure(uint8_t packet[5], uint32_t remote)
{
	size_t pos = 0;
	packet[pos++] = SSH_MSG_CHANNEL_FAILURE;
	DSSH_PUT_U32(remote, packet, &pos);
}

static bool
post_close_enqueue_controls(dssh_channel ch, uint32_t bytes, bool defer)
{
	dssh_session sess = ch->sess;
	if (tx_acquire(sess, SSH_MSG_IGNORE) != 0)
		return false;
	/* Model a producer that captured channel work before a CLOSE race. */
	if (mtx_lock(&ch->buf_mtx) != thrd_success)
		abort();
	ch->window_adjust_pending = bytes;
	atomic_store(&ch->wa_slot.ready, true);
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	uint8_t failure[5];
	post_close_build_failure(failure, ch->remote_id);
	int window_result = schedule_wa_slot(sess);
	int failure_result = send_channel_reply(sess, ch, failure, sizeof(failure));
	bool ready = atomic_load(&ch->wa_slot.ready) && atomic_load(&ch->chan_reply_slot.ready);
	if (defer)
		atomic_store(&sess->trans.rekey_in_progress, true);
	tx_unlock_with_slots(sess);
	if (defer)
		atomic_store(&sess->trans.rekey_in_progress, false);
	return window_result == 0 && failure_result == 0 && ready;
}

static int
post_close_queued_run(struct conn_ctx *ctx)
{
	const uint32_t remote = UINT32_MAX;
	ASSERT_OK(dssh_session_start(ctx->server));
	dssh_channel old = post_close_accept(ctx, remote);
	ASSERT_NOT_NULL(old);
	ASSERT_OK(post_close_receive_close(ctx, old));

	ASSERT_TRUE(post_close_enqueue_controls(old, 1, true));
	ASSERT_OK(conn_close(ctx->server, old));
	ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_WINDOW_ADJUST, 9, remote));
	ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_FAILURE, 5, remote));
	ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_CLOSE, 5, remote));

	dssh_channel replacement = post_close_accept(ctx, remote);
	ASSERT_NOT_NULL(replacement);
	ASSERT_TRUE(post_close_enqueue_controls(old, 2, false));
	ASSERT_FALSE(atomic_load(&old->wa_slot.ready));
	ASSERT_FALSE(atomic_load(&old->chan_reply_slot.ready));
	if (mtx_lock(&old->buf_mtx) != thrd_success)
		abort();
	uint32_t local_window = old->local_window;
	uint32_t pending = old->window_adjust_pending;
	uint32_t inflight = old->window_adjust_inflight;
	if (mtx_unlock(&old->buf_mtx) != thrd_success)
		abort();
	ASSERT_EQ_U(local_window, 1);
	ASSERT_EQ_U(pending, 0);
	ASSERT_EQ_U(inflight, 0);

	uint8_t failure[5];
	post_close_build_failure(failure, remote);
	ASSERT_OK(send_channel_reply(ctx->server, old, failure, sizeof(failure)));
	ASSERT_TRUE(channel_id_wire_barrier(ctx->client));
	ASSERT_OK(dssh_chan_close(old, -1));
	ASSERT_OK(dssh_chan_send_break(replacement, 100));
	ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_REQUEST, 19, remote));
	return TEST_PASS;
}

static int
test_post_close_queued(void)
{
	for (unsigned gather = 0; gather < 2; gather++) {
		struct conn_ctx ctx;
		window_test_gather = gather != 0;
		int setup = conn_setup_mode(&ctx, false);
		window_test_gather = false;
		ASSERT_OK(setup);
		int ret = post_close_queued_run(&ctx);
		conn_cleanup(&ctx);
		if (ret != TEST_PASS)
			return ret;
	}
	return TEST_PASS;
}

static int
post_close_buffered_run(struct conn_ctx *ctx)
{
	struct open_exec_ctx open = {.client = ctx->client, .server = ctx->server,
	    .command = "buffered", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	ASSERT_OK(open_exec_channel(ctx, &open));
	const uint8_t expected[] = {11, 22, 33, 44};
	ASSERT_EQ(dssh_chan_write(open.client_ch, 0, expected, sizeof(expected)), sizeof(expected));
	ASSERT_OK(dssh_chan_close(open.client_ch, -1));
	bool closed = false;
	for (unsigned i = 0; i < 2; i++) {
		struct dssh_chan_event event;
		ASSERT_EQ(dssh_chan_poll(open.server_ch, DSSH_POLL_EVENT, 5000), DSSH_POLL_EVENT);
		ASSERT_OK(dssh_chan_read_event(open.server_ch, &event));
		closed = closed || event.type == DSSH_EVENT_CLOSE;
	}
	ASSERT_TRUE(closed);
	ASSERT_OK(conn_close(ctx->server, open.server_ch));
	uint8_t received[sizeof(expected)];
	ASSERT_EQ(dssh_chan_read(open.server_ch, 0, received, sizeof(received)), sizeof(received));
	ASSERT_MEM_EQ(received, expected, sizeof(expected));
	ASSERT_EQ(dssh_chan_read(open.server_ch, 0, received, sizeof(received)), 0);
	ASSERT_OK(dssh_chan_close(open.server_ch, 42));
	ASSERT_FALSE(dssh_session_is_terminated(ctx->server));
	return TEST_PASS;
}

static int
test_post_close_buffered(void)
{
	struct conn_ctx ctx;
	ASSERT_OK(conn_setup(&ctx));
	int ret = post_close_buffered_run(&ctx);
	conn_cleanup(&ctx);
	return ret;
}
