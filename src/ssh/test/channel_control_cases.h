/* RFC 4254 sections 6.7/6.9 and RFC 4335 section 3: actual client handle,
 * with the synchronous native server peer owning its transport reader. */
static dssh_channel
channel_control_open(struct conn_ctx *ctx)
{
	dssh_session swap = ctx->client;
	ctx->client = ctx->server;
	ctx->server = swap;
	if (dssh_session_start(ctx->server) != 0)
		return NULL;
	return half_close_accept(ctx, UINT32_MAX);
}

static int
channel_control_call(dssh_channel ch, unsigned operation)
{
	if (operation == 0)
		return dssh_chan_send_signal(ch, "TERM");
	if (operation == 1)
		return dssh_chan_send_window_change(ch, 0, UINT32_MAX, 17, 23);
	return dssh_chan_send_break(ch, UINT32_MAX);
}

static bool
channel_control_receive(dssh_session peer, unsigned operation, const char *signal_name,
    const uint32_t *values)
{
	uint8_t type;
	uint8_t *packet;
	size_t length;
	if (recv_packet(peer, &type, &packet, &length) != 0 || type != SSH_MSG_CHANNEL_REQUEST
	    || length < 10 || DSSH_GET_U32(packet + 1) != UINT32_MAX)
		return false;
	const char *name = operation == 0 ? "signal" : operation == 1 ? "window-change" : "break";
	size_t name_len = strlen(name);
	if (DSSH_GET_U32(packet + 5) != name_len || length < 10 + name_len
	    || memcmp(packet + 9, name, name_len) != 0 || packet[9 + name_len] != 0)
		return false;
	size_t offset = 10 + name_len;
	if (operation == 0) {
		size_t signal_len = strlen(signal_name);
		return length == offset + 4 + signal_len
		    && DSSH_GET_U32(packet + offset) == signal_len
		    && memcmp(packet + offset + 4, signal_name, signal_len) == 0;
	}
	size_t count = operation == 1 ? 4 : 1;
	if (length != offset + count * 4)
		return false;
	for (size_t i = 0; i < count; i++) {
		if (DSSH_GET_U32(packet + offset + i * 4) != values[i])
			return false;
	}
	return true;
}

static int
channel_control_wire_run(struct conn_ctx *ctx, bool after_eof)
{
	dssh_channel ch = channel_control_open(ctx);
	ASSERT_NOT_NULL(ch);
	ASSERT_EQ(dssh_chan_send_signal(ch, NULL), DSSH_ERROR_INVALID);
	uint_least32_t window = atomic_load(&ch->remote_window);
	if (after_eof) {
		ASSERT_OK(dssh_chan_shutwr(ch));
		ASSERT_TRUE(peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_EOF, 5, UINT32_MAX));
	}
	static const char *const signals[] = {
		"ABRT", "ALRM", "FPE", "HUP", "ILL", "INT", "KILL", "PIPE",
		"QUIT", "SEGV", "TERM", "USR1", "USR2"
	};
	for (size_t i = 0; i < sizeof(signals) / sizeof(signals[0]); i++) {
		ASSERT_OK(dssh_chan_send_signal(ch, signals[i]));
		ASSERT_TRUE(channel_control_receive(ctx->client, 0, signals[i], NULL));
	}
	static const uint32_t dimensions[][4] = {
		{0, 0, 0, 0}, {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX}, {80, 24, 640, 480}
	};
	for (size_t i = 0; i < sizeof(dimensions) / sizeof(dimensions[0]); i++) {
		const uint32_t *v = dimensions[i];
		ASSERT_OK(dssh_chan_send_window_change(ch, v[0], v[1], v[2], v[3]));
		ASSERT_TRUE(channel_control_receive(ctx->client, 1, NULL, v));
	}
	static const uint32_t durations[] = {0, 1, 499, 500, 3000, 3001, UINT32_MAX};
	for (size_t i = 0; i < sizeof(durations) / sizeof(durations[0]); i++) {
		ASSERT_OK(dssh_chan_send_break(ch, durations[i]));
		ASSERT_TRUE(channel_control_receive(ctx->client, 2, NULL, &durations[i]));
	}
	ASSERT_TRUE(channel_id_wire_barrier(ctx->client));
	ASSERT_EQ_U(atomic_load(&ch->remote_window), window);
	ASSERT_FALSE(dssh_session_is_terminated(ctx->server));
	ASSERT_EQ(half_close_sent(ch), after_eof);
	printf("channel_control_wire after_eof=%u requests=23 window_unchanged=1\n", after_eof ? 1U : 0U);
	return TEST_PASS;
}

static int
channel_control_wire_profiles(bool after_eof)
{
	ASSERT_EQ(dssh_chan_send_signal(NULL, "TERM"), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_chan_send_window_change(NULL, 0, 0, 0, 0), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_chan_send_break(NULL, 0), DSSH_ERROR_INVALID);
	for (unsigned gather = 0; gather < 2; gather++) {
		struct conn_ctx ctx;
		window_test_gather = gather != 0;
		int setup = conn_setup_mode(&ctx, false);
		window_test_gather = false;
		ASSERT_OK(setup);
		ASSERT_TRUE(half_close_algorithms(&ctx));
		int ret = channel_control_wire_run(&ctx, after_eof);
		conn_cleanup(&ctx);
		if (ret != TEST_PASS)
			return ret;
	}
	return TEST_PASS;
}

static _Thread_local unsigned channel_control_tx_calls;

static int
channel_control_after_tx(dssh_session session)
{
	(void)session;
	channel_control_tx_calls++;
	window_tx_return_test_hook = channel_control_after_tx;
	return 0;
}

static int
channel_control_refusal_run(struct conn_ctx *ctx)
{
	dssh_channel ch = channel_control_open(ctx);
	ASSERT_NOT_NULL(ch);
	const uint32_t values[] = {0, UINT32_MAX, 17, 23};
	const uint32_t duration = UINT32_MAX;
	const size_t sizes[] = {24, 39, 19};
	uint_least32_t window = atomic_load(&ch->remote_window);
	for (unsigned operation = 0; operation < 3; operation++) {
		ASSERT_OK(tx_acquire(ctx->server, SSH_MSG_IGNORE));
		size_t previous = ctx->server->trans.comp_tx_payload_limit;
		ctx->server->trans.comp_tx_payload_limit = sizes[operation] - 1;
		bool bounded = tx_payload_capacity(ctx->server) == sizes[operation] - 1;
		uint8_t sentinel[48];
		memset(sentinel, 0xa5, sizeof(sentinel));
		memcpy(ctx->server->trans.tx_packet + 9, sentinel, sizeof(sentinel));
		tx_unlock_with_slots(ctx->server);
		ASSERT_TRUE(bounded);
		channel_control_tx_calls = 0;
		window_tx_return_test_hook = channel_control_after_tx;
		int result = channel_control_call(ch, operation);
		window_tx_return_test_hook = NULL;
		unsigned calls = channel_control_tx_calls;
		ASSERT_OK(tx_acquire(ctx->server, SSH_MSG_IGNORE));
		ctx->server->trans.comp_tx_payload_limit = previous;
		bool intact = memcmp(ctx->server->trans.tx_packet + 9, sentinel, sizeof(sentinel)) == 0;
		tx_unlock_with_slots(ctx->server);
		ASSERT_EQ(result, DSSH_ERROR_TOOLONG);
		ASSERT_EQ_U(calls, 0);
		ASSERT_TRUE(intact);
		ASSERT_FALSE(dssh_session_is_terminated(ctx->server));
		ASSERT_OK(channel_control_call(ch, operation));
		ASSERT_TRUE(channel_control_receive(ctx->client, operation, "TERM",
		    operation == 1 ? values : &duration));
	}
	ASSERT_TRUE(channel_id_wire_barrier(ctx->client));
	ASSERT_EQ_U(atomic_load(&ch->remote_window), window);
	printf("channel_control_refusal operations=3 scratch_unchanged=1 no_callback=1 retry=1\n");
	return TEST_PASS;
}

static int
test_channel_control_refusal(void)
{
	const char *compression = getenv("DSSH_TEST_COMPRESSION");
	ASSERT_TRUE(compression != NULL && strcmp(compression, "test-growth") == 0);
	for (unsigned gather = 0; gather < 2; gather++) {
		struct conn_ctx ctx;
		window_test_gather = gather != 0;
		int setup = conn_setup_mode(&ctx, false);
		window_test_gather = false;
		ASSERT_OK(setup);
		ASSERT_TRUE(half_close_algorithms(&ctx));
		int ret = channel_control_refusal_run(&ctx);
		window_tx_return_test_hook = NULL;
		conn_cleanup(&ctx);
		if (ret != TEST_PASS)
			return ret;
	}
	return TEST_PASS;
}

static int
channel_control_failure_run(struct conn_ctx *ctx, unsigned operation, bool acquisition)
{
	dssh_channel ch = channel_control_open(ctx);
	ASSERT_NOT_NULL(ch);
	if (acquisition)
		dssh_test_mtx_reject_once(&ch->buf_mtx);
	else
		setup_tx_test_hook = half_close_fail_tx;
	int result = channel_control_call(ch, operation);
	setup_tx_test_hook = NULL;
	unsigned rejected_unlocks = dssh_test_mtx_rejected_unlocks();
	dssh_test_thrd_reset();
	ASSERT_EQ(result, acquisition ? DSSH_ERROR_TERMINATED : DSSH_ERROR_INIT);
	ASSERT_EQ_U(rejected_unlocks, 0);
	ASSERT_TRUE(dssh_session_is_terminated(ctx->server));
	ASSERT_FALSE(atomic_load(&ctx->server->trans.tx_owner_active));
	ASSERT_EQ(channel_control_call(ch, operation), DSSH_ERROR_TERMINATED);
	mock_io_close_c2s_write(&ctx->io);
	ASSERT_EQ(conn_native_eof(&ctx->io.c2s), 0);
	printf("channel_control_failure operation=%u acquisition=%u result=%d no_packet=1 no_unowned_unlock=1\n",
	    operation, acquisition ? 1U : 0U, result);
	return TEST_PASS;
}

static int
test_channel_control_failure(void)
{
	for (unsigned gather = 0; gather < 2; gather++) {
		for (unsigned operation = 0; operation < 3; operation++) {
			for (unsigned acquisition = 0; acquisition < 2; acquisition++) {
				struct conn_ctx ctx;
				window_test_gather = gather != 0;
				int setup = conn_setup_mode(&ctx, false);
				window_test_gather = false;
				ASSERT_OK(setup);
				ASSERT_TRUE(half_close_algorithms(&ctx));
				int ret = channel_control_failure_run(&ctx, operation, acquisition != 0);
				setup_tx_test_hook = NULL;
				conn_cleanup(&ctx);
				if (ret != TEST_PASS)
					return ret;
			}
		}
	}
	return TEST_PASS;
}

static int
test_channel_control_wire(void)
{
	return channel_control_wire_profiles(false);
}

static int
test_channel_control_after_eof(void)
{
	return channel_control_wire_profiles(true);
}
