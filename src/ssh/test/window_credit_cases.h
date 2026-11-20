#include "ssh-window.h"

/* Real authenticated peers. Credit is returned before the sender's I/O
 * callback completes, with a wire-ordered marker as the native barrier. */
struct window_credit_fixture {
	struct conn_ctx ctx;
	dssh_channel client;
	dssh_channel server;
	atomic_bool data_seen;
	atomic_bool credit_sent;
	atomic_bool marker_seen;
	atomic_bool bad;
	bool zc;
	bool reverse;
	bool gather;
	bool large;
	int stream;
	unsigned mode;
	unsigned failures;
	int marker_result;
};
static _Thread_local struct window_credit_fixture *window_credit_current;

static int
window_credit_return(dssh_session sess)
{
	(void)sess;
	struct window_credit_fixture *f = window_credit_current;
	atomic_store(&f->credit_sent, true);
	return 0;
}

static uint32_t
window_credit_receive(dssh_channel ch, int stream, const uint8_t *bytes, size_t length, void *opaque)
{
	(void)ch;
	struct window_credit_fixture *f = opaque;
	bool duplicate = atomic_exchange(&f->data_seen, true);
	if (stream != f->stream || duplicate || !flow_bytes(bytes, length, 10, 0x6d))
		atomic_store(&f->bad, true);
	window_credit_current = f;
	window_tx_return_test_hook = window_credit_return;
	return length <= UINT32_MAX ? (uint32_t)length : UINT32_MAX;
}

static uint32_t
window_credit_unused(dssh_channel ch, int stream, const uint8_t *bytes, size_t length, void *opaque)
{
	(void)ch;
	(void)stream;
	(void)bytes;
	(void)opaque;
	return length <= UINT32_MAX ? (uint32_t)length : UINT32_MAX;
}

static int
window_credit_marker(const uint8_t *name, size_t name_len, bool reply, const uint8_t *data,
    size_t data_len, void *opaque)
{
	(void)data;
	struct window_credit_fixture *f = opaque;
	if (reply || data_len != 0 || name_len != DSSH_STRLEN("window-marker")
	    || name == NULL || memcmp(name, "window-marker", name_len) != 0)
		atomic_store(&f->bad, true);
	atomic_store(&f->marker_seen, true);
	return DSSH_ERROR_REJECTED;
}

static int
window_credit_after_send(dssh_session sess)
{
	(void)sess;
	struct window_credit_fixture *f = window_credit_current;
	return wire_wait(sess, &f->marker_seen) ? 0 : DSSH_ERROR_TERMINATED;
}

static int
window_credit_fail_send(dssh_session sess)
{
	(void)sess;
	window_credit_current->failures++;
	return DSSH_ERROR_INIT;
}

static int
window_credit_terminal(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)ch;
	(void)params;
	struct window_credit_fixture *f = opaque;
	result->max_window = f->reverse ? 128 : f->large ? UINT32_MAX - 5 : 1024;
	result->zc_cb = !f->reverse ? window_credit_receive : f->zc ? window_credit_unused : NULL;
	result->zc_cbdata = f;
	return 0;
}

static int
window_credit_accept(void *opaque)
{
	struct window_credit_fixture *f = opaque;
	struct dssh_chan_accept_cbs cbs = {.exec = window_credit_terminal, .cbdata = f};
	f->server = dssh_chan_accept(f->ctx.server, &cbs, 3000);
	return 0;
}

static int
window_credit_marker_thread(void *opaque)
{
	struct window_credit_fixture *f = opaque;
	bool ready = wire_wait(f->reverse ? f->ctx.client : f->ctx.server, &f->credit_sent);
	uint8_t packet[32];
	size_t pos = 0;
	packet[pos++] = SSH_MSG_GLOBAL_REQUEST;
	DSSH_PUT_U32(DSSH_STRLEN("window-marker"), packet, &pos);
	memcpy(packet + pos, "window-marker", DSSH_STRLEN("window-marker"));
	pos += DSSH_STRLEN("window-marker");
	packet[pos++] = 0;
	f->marker_result = ready ? send_packet(f->reverse ? f->ctx.client : f->ctx.server, packet, pos, NULL) : DSSH_ERROR_TIMEOUT;
	if (f->marker_result != 0) {
		mock_io_close_c2s(&f->ctx.io);
		mock_io_close_s2c(&f->ctx.io);
	}
	return f->marker_result;
}

static bool
window_credit_ready(dssh_channel ch, uint32_t expected)
{
	struct timespec start;
	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
		return false;
	while (atomic_load(&ch->remote_window) != expected) {
		if (!dssh_test_wait_tick(&start))
			return false;
	}
	return true;
}

static int
window_credit_case(bool zc, int stream, bool gather, bool large, unsigned mode, bool reverse)
{
	struct window_credit_fixture f = {.zc = zc, .stream = stream, .gather = gather,
	    .large = large, .mode = mode, .reverse = reverse, .marker_result = DSSH_ERROR_INIT};
	atomic_init(&f.data_seen, false);
	atomic_init(&f.credit_sent, false);
	atomic_init(&f.marker_seen, false);
	atomic_init(&f.bad, false);
	window_test_gather = gather;
	int setup = conn_setup_no_start(&f.ctx);
	window_test_gather = false;
	ASSERT_EQ(setup, 0);
	const char *provider = getenv("DSSH_TEST_COMPRESSION");
	if (provider != NULL && strcmp(provider, "none") != 0) {
		ASSERT_NOT_NULL(f.ctx.client->trans.comp_c2s_active);
		ASSERT_NOT_NULL(f.ctx.server->trans.comp_s2c_active);
		ASSERT_EQ(strcmp(f.ctx.client->trans.comp_c2s_active->name, provider), 0);
		ASSERT_EQ(strcmp(f.ctx.server->trans.comp_s2c_active->name, provider), 0);
	}
	ASSERT_EQ(dssh_session_set_timeout(f.ctx.client, 3000), 0);
	ASSERT_EQ(dssh_session_set_timeout(f.ctx.server, 3000), 0);
	ASSERT_EQ(dssh_session_set_global_request_cb(reverse ? f.ctx.server : f.ctx.client, window_credit_marker, &f), 0);
	ASSERT_EQ(dssh_session_start(f.ctx.client), 0);
	ASSERT_EQ(dssh_session_start(f.ctx.server), 0);
	struct flow_fixture flow;
	flow_init(&flow, &f.ctx, NULL, NULL);
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, window_credit_accept, &f), 0);
	ASSERT_TRUE(flow_checkpoint(FLOW_WINDOW_STARTED));
	struct dssh_chan_params params = {0};
	int ret = dssh_chan_params_init(&params, DSSH_CHAN_EXEC);
	if (ret == 0)
		ret = dssh_chan_params_set_command(&params, "window-credit");
	if (ret == 0)
		ret = dssh_chan_params_set_max_window(&params, reverse ? large ? UINT32_MAX - 5 : 1024 : 128);
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&params, false);
	if (ret == 0)
		f.client = reverse ? dssh_chan_zc_open(f.ctx.client, &params, window_credit_receive, &f)
		    : zc ? dssh_chan_zc_open(f.ctx.client, &params, window_credit_unused, NULL)
		    : dssh_chan_open(f.ctx.client, &params);
	dssh_chan_params_free(&params);
	if (f.client == NULL) {
		mock_io_close_c2s(&f.ctx.io);
		mock_io_close_s2c(&f.ctx.io);
	}
	channel_worker_join(&flow.workers, 0);
	ASSERT_EQ(flow.workers.results[0], 0);
	dssh_channel actor = reverse ? f.server : f.client;
	dssh_session sending_session = reverse ? f.ctx.server : f.ctx.client;
	uint32_t expected = large ? UINT32_MAX - 5 : 1024;
	bool valid = ret == 0 && f.client != NULL && f.server != NULL
	    && window_credit_ready(actor, expected);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(flow_checkpoint(FLOW_WINDOW_ACCEPTED));
	if (valid && mode >= 1 && mode <= 3) {
		uint8_t *bytes = NULL;
		size_t capacity = 0;
		int first = dssh_chan_zc_getbuf(actor, stream, &bytes, &capacity);
		ASSERT_EQ(first, 0);
		flow.reserved = actor;
		ASSERT_TRUE(bytes != NULL && capacity >= 10);
		ASSERT_TRUE(flow_checkpoint(FLOW_WINDOW_RESERVED));
		if (mode == 1) {
			ASSERT_EQ(dssh_chan_zc_cancel(actor), 0);
			flow.reserved = NULL;
		}
		else if (mode == 2) {
			ASSERT_TRUE(capacity < SIZE_MAX);
			int refused = dssh_chan_zc_send(actor, capacity + 1);
			flow.reserved = NULL;
			valid = refused == DSSH_ERROR_TOOLONG;
		}
		else {
			memset(bytes, 0x6d, 10);
			uint_fast32_t saved = atomic_load(&sending_session->trans.tx_since_rekey);
			atomic_store(&sending_session->trans.tx_since_rekey, DSSH_REKEY_HARD_LIMIT);
			int refusal = dssh_chan_zc_send(actor, 10);
			flow.reserved = NULL;
			atomic_store(&sending_session->trans.tx_since_rekey, saved);
			valid = refusal == DSSH_ERROR_REKEY_NEEDED;
		}
		valid = valid && atomic_load(&actor->remote_window) == expected
		    && !dssh_session_is_terminated(sending_session) && !atomic_load(&f.data_seen);
	}
	ASSERT_TRUE(valid);
	if (mode != 4) {
		ASSERT_EQ(channel_worker_start(&flow.workers, 1, window_credit_marker_thread, &f), 0);
		ASSERT_TRUE(flow_checkpoint(FLOW_WINDOW_MARKER));
	}
	int64_t result = DSSH_ERROR_INIT;
	if (valid) {
		if (zc) {
			uint8_t *bytes = NULL;
			size_t capacity = 0;
			ASSERT_EQ(dssh_chan_zc_getbuf(actor, stream, &bytes, &capacity), 0);
			flow.reserved = actor;
			ASSERT_TRUE(bytes != NULL && capacity >= 10);
			ASSERT_TRUE(flow_checkpoint(FLOW_WINDOW_RESERVED));
			memset(bytes, 0x6d, 10);
			window_credit_current = &f;
			if (mode == 4)
				setup_tx_test_hook = window_credit_fail_send;
			else
				window_tx_return_test_hook = window_credit_after_send;
			result = dssh_chan_zc_send(actor, 10);
			flow.reserved = NULL;
		}
		else {
			uint8_t bytes[10];
			memset(bytes, 0x6d, sizeof(bytes));
			window_credit_current = &f;
			if (mode == 4)
				setup_tx_test_hook = window_credit_fail_send;
			else
				window_tx_return_test_hook = window_credit_after_send;
			result = dssh_chan_write(actor, stream, bytes, sizeof(bytes));
		}
		window_tx_return_test_hook = NULL;
		setup_tx_test_hook = NULL;
		window_credit_current = NULL;
	}
	if (!valid || mode == 4) {
		mock_io_close_c2s(&f.ctx.io);
		mock_io_close_s2c(&f.ctx.io);
	}
	flow_finish(&flow);
	if (mode != 4)
		ASSERT_EQ(flow.workers.results[1], 0);
	valid = valid && !atomic_load(&f.bad) && atomic_load(&actor->remote_window) == expected;
	if (mode == 4)
		valid = valid && result == DSSH_ERROR_INIT && f.failures == 1
		    && dssh_session_is_terminated(sending_session) && !atomic_load(&f.data_seen);
	else
		valid = valid && result == (zc ? 0 : 10) && f.marker_result == 0
		    && atomic_load(&f.data_seen) && atomic_load(&f.credit_sent)
		    && atomic_load(&f.marker_seen) && !dssh_session_is_terminated(f.ctx.client)
		    && !dssh_session_is_terminated(f.ctx.server);
	if (!valid) {
		intmax_t printed_result = result;
		uintmax_t printed_window = actor != NULL ? atomic_load(&actor->remote_window) : 0;
		ASSERT_TRUE(fprintf(stderr, "window credit reverse=%u zc=%u stream=%d gather=%u large=%u mode=%u result=%jd window=%ju\n",
		    reverse ? 1U : 0U, zc ? 1U : 0U, stream, gather ? 1U : 0U, large ? 1U : 0U, mode,
		    printed_result, printed_window) >= 0);
	}
	conn_cleanup(&f.ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_window_credit_arithmetic(void)
{
	const uint32_t values[] = {0, 1, 2, 17, UINT32_C(0x7fffffff), UINT32_C(0x80000000),
	    UINT32_MAX - 17, UINT32_MAX - 1, UINT32_MAX};
	for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
		for (size_t j = 0; j < sizeof(values) / sizeof(values[0]); j++) {
			uint64_t sum = (uint64_t)values[i] + values[j];
			uint32_t expected = sum > UINT32_MAX ? UINT32_MAX : (uint32_t)sum;
			ASSERT_EQ_U(window_add(values[i], values[j]), expected);
			atomic_uint_least32_t value;
			atomic_init(&value, values[i]);
			window_atomic_add(&value, values[j]);
			ASSERT_EQ_U(atomic_load(&value), expected);
			atomic_store(&value, values[i]);
			window_atomic_sub(&value, values[j]);
			uint32_t difference = values[j] <= values[i] ? values[i] - values[j] : 0;
			ASSERT_EQ_U(atomic_load(&value), difference);
		}
	}
	return TEST_PASS;
}

struct window_counter_worker {
	atomic_uint_least32_t *value;
	bool add;
};

static int
window_counter_run(void *opaque)
{
	struct window_counter_worker *worker = opaque;
	for (unsigned i = 0; i < 20000; i++) {
		if (worker->add)
			window_atomic_add(worker->value, 1);
		else
			window_atomic_sub(worker->value, 1);
	}
	return 0;
}

static int
test_window_credit_atomic(void)
{
	atomic_uint_least32_t value;
	atomic_init(&value, 100000);
	struct window_counter_worker add = {.value = &value, .add = true};
	struct window_counter_worker sub = {.value = &value, .add = false};
	thrd_t first, second;
	ASSERT_EQ(thrd_create(&first, window_counter_run, &add), thrd_success);
	bool started = thrd_create(&second, window_counter_run, &sub) == thrd_success;
	int first_result, second_result = -1;
	if (thrd_join(first, &first_result) != thrd_success)
		abort();
	if (started && thrd_join(second, &second_result) != thrd_success)
		abort();
	ASSERT_TRUE(started);
	ASSERT_EQ(first_result, 0);
	ASSERT_EQ(second_result, 0);
	ASSERT_EQ_U(atomic_load(&value), 100000);
	return TEST_PASS;
}

static int test_window_credit_packet_0_stream_small(void)
{
	return window_credit_case(false, 0, false, false, 0, false);
}

static int test_window_credit_packet_0_stream_full(void)
{
	return window_credit_case(false, 0, false, true, 0, false);
}

static int test_window_credit_packet_0_zc_small(void)
{
	return window_credit_case(true, 0, false, false, 0, false);
}

static int test_window_credit_packet_0_zc_full(void)
{
	return window_credit_case(true, 0, false, true, 0, false);
}

static int test_window_credit_packet_0_cancel(void)
{
	return window_credit_case(true, 0, false, false, 1, false);
}

static int test_window_credit_packet_0_oversize(void)
{
	return window_credit_case(true, 0, false, false, 2, false);
}

static int test_window_credit_packet_0_key_limit(void)
{
	return window_credit_case(true, 0, false, false, 3, false);
}

static int test_window_credit_packet_0_io_failure(void)
{
	return window_credit_case(true, 0, false, false, 4, false);
}

static int test_window_credit_packet_1_stream_small(void)
{
	return window_credit_case(false, 1, false, false, 0, false);
}

static int test_window_credit_packet_1_stream_full(void)
{
	return window_credit_case(false, 1, false, true, 0, false);
}

static int test_window_credit_packet_1_zc_small(void)
{
	return window_credit_case(true, 1, false, false, 0, false);
}

static int test_window_credit_packet_1_zc_full(void)
{
	return window_credit_case(true, 1, false, true, 0, false);
}

static int test_window_credit_packet_1_cancel(void)
{
	return window_credit_case(true, 1, false, false, 1, false);
}

static int test_window_credit_packet_1_oversize(void)
{
	return window_credit_case(true, 1, false, false, 2, false);
}

static int test_window_credit_packet_1_key_limit(void)
{
	return window_credit_case(true, 1, false, false, 3, false);
}

static int test_window_credit_packet_1_io_failure(void)
{
	return window_credit_case(true, 1, false, false, 4, false);
}

static int test_window_credit_gather_0_stream_small(void)
{
	return window_credit_case(false, 0, true, false, 0, false);
}

static int test_window_credit_gather_0_stream_full(void)
{
	return window_credit_case(false, 0, true, true, 0, false);
}

static int test_window_credit_gather_0_zc_small(void)
{
	return window_credit_case(true, 0, true, false, 0, false);
}

static int test_window_credit_gather_0_zc_full(void)
{
	return window_credit_case(true, 0, true, true, 0, false);
}

static int test_window_credit_gather_0_cancel(void)
{
	return window_credit_case(true, 0, true, false, 1, false);
}

static int test_window_credit_gather_0_oversize(void)
{
	return window_credit_case(true, 0, true, false, 2, false);
}

static int test_window_credit_gather_0_key_limit(void)
{
	return window_credit_case(true, 0, true, false, 3, false);
}

static int test_window_credit_gather_0_io_failure(void)
{
	return window_credit_case(true, 0, true, false, 4, false);
}

static int test_window_credit_gather_1_stream_small(void)
{
	return window_credit_case(false, 1, true, false, 0, false);
}

static int test_window_credit_gather_1_stream_full(void)
{
	return window_credit_case(false, 1, true, true, 0, false);
}

static int test_window_credit_gather_1_zc_small(void)
{
	return window_credit_case(true, 1, true, false, 0, false);
}

static int test_window_credit_gather_1_zc_full(void)
{
	return window_credit_case(true, 1, true, true, 0, false);
}

static int test_window_credit_gather_1_cancel(void)
{
	return window_credit_case(true, 1, true, false, 1, false);
}

static int test_window_credit_gather_1_oversize(void)
{
	return window_credit_case(true, 1, true, false, 2, false);
}

static int test_window_credit_gather_1_key_limit(void)
{
	return window_credit_case(true, 1, true, false, 3, false);
}

static int test_window_credit_gather_1_io_failure(void)
{
	return window_credit_case(true, 1, true, false, 4, false);
}

static int test_window_credit_reverse_packet_0_stream_small(void)
{
	return window_credit_case(false, 0, false, false, 0, true);
}

static int test_window_credit_reverse_packet_0_stream_full(void)
{
	return window_credit_case(false, 0, false, true, 0, true);
}

static int test_window_credit_reverse_packet_0_zc_small(void)
{
	return window_credit_case(true, 0, false, false, 0, true);
}

static int test_window_credit_reverse_packet_0_zc_full(void)
{
	return window_credit_case(true, 0, false, true, 0, true);
}

static int test_window_credit_reverse_packet_0_cancel(void)
{
	return window_credit_case(true, 0, false, false, 1, true);
}

static int test_window_credit_reverse_packet_0_oversize(void)
{
	return window_credit_case(true, 0, false, false, 2, true);
}

static int test_window_credit_reverse_packet_0_key_limit(void)
{
	return window_credit_case(true, 0, false, false, 3, true);
}

static int test_window_credit_reverse_packet_0_io_failure(void)
{
	return window_credit_case(true, 0, false, false, 4, true);
}

static int test_window_credit_reverse_packet_1_stream_small(void)
{
	return window_credit_case(false, 1, false, false, 0, true);
}

static int test_window_credit_reverse_packet_1_stream_full(void)
{
	return window_credit_case(false, 1, false, true, 0, true);
}

static int test_window_credit_reverse_packet_1_zc_small(void)
{
	return window_credit_case(true, 1, false, false, 0, true);
}

static int test_window_credit_reverse_packet_1_zc_full(void)
{
	return window_credit_case(true, 1, false, true, 0, true);
}

static int test_window_credit_reverse_packet_1_cancel(void)
{
	return window_credit_case(true, 1, false, false, 1, true);
}

static int test_window_credit_reverse_packet_1_oversize(void)
{
	return window_credit_case(true, 1, false, false, 2, true);
}

static int test_window_credit_reverse_packet_1_key_limit(void)
{
	return window_credit_case(true, 1, false, false, 3, true);
}

static int test_window_credit_reverse_packet_1_io_failure(void)
{
	return window_credit_case(true, 1, false, false, 4, true);
}

static int test_window_credit_reverse_gather_0_stream_small(void)
{
	return window_credit_case(false, 0, true, false, 0, true);
}

static int test_window_credit_reverse_gather_0_stream_full(void)
{
	return window_credit_case(false, 0, true, true, 0, true);
}

static int test_window_credit_reverse_gather_0_zc_small(void)
{
	return window_credit_case(true, 0, true, false, 0, true);
}

static int test_window_credit_reverse_gather_0_zc_full(void)
{
	return window_credit_case(true, 0, true, true, 0, true);
}

static int test_window_credit_reverse_gather_0_cancel(void)
{
	return window_credit_case(true, 0, true, false, 1, true);
}

static int test_window_credit_reverse_gather_0_oversize(void)
{
	return window_credit_case(true, 0, true, false, 2, true);
}

static int test_window_credit_reverse_gather_0_key_limit(void)
{
	return window_credit_case(true, 0, true, false, 3, true);
}

static int test_window_credit_reverse_gather_0_io_failure(void)
{
	return window_credit_case(true, 0, true, false, 4, true);
}

static int test_window_credit_reverse_gather_1_stream_small(void)
{
	return window_credit_case(false, 1, true, false, 0, true);
}

static int test_window_credit_reverse_gather_1_stream_full(void)
{
	return window_credit_case(false, 1, true, true, 0, true);
}

static int test_window_credit_reverse_gather_1_zc_small(void)
{
	return window_credit_case(true, 1, true, false, 0, true);
}

static int test_window_credit_reverse_gather_1_zc_full(void)
{
	return window_credit_case(true, 1, true, true, 0, true);
}

static int test_window_credit_reverse_gather_1_cancel(void)
{
	return window_credit_case(true, 1, true, false, 1, true);
}

static int test_window_credit_reverse_gather_1_oversize(void)
{
	return window_credit_case(true, 1, true, false, 2, true);
}

static int test_window_credit_reverse_gather_1_key_limit(void)
{
	return window_credit_case(true, 1, true, false, 3, true);
}

static int test_window_credit_reverse_gather_1_io_failure(void)
{
	return window_credit_case(true, 1, true, false, 4, true);
}
