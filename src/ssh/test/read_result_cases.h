/* Public stream-read completion on actual paired encrypted channels. */
#ifndef DSSH_TEST_READ_RESULT_CASES_H
#define DSSH_TEST_READ_RESULT_CASES_H

enum read_result_kind {
	RR_NORMAL, RR_TX, RR_LOCK_BEFORE, RR_LOCK_AFTER, RR_QUEUE_LOCK,
	RR_RANDOM, RR_KEY, RR_SIZE, RR_COMPRESS, RR_EOF, RR_TERMINATED
};

struct read_result_fixture {
	struct conn_ctx ctx;
	dssh_channel client, server;
	unsigned tx_failures;
	bool zero_copy;
};
static _Thread_local struct read_result_fixture *read_result_current;

static uint32_t
read_result_discard(dssh_channel channel, int stream, const uint8_t *data,
    size_t length, void *opaque)
{
	(void)channel;
	(void)stream;
	(void)data;
	(void)opaque;
	if (length > UINT32_MAX)
		return 0;
	uint32_t consumed = (uint32_t)length;
	return consumed;
}

static int
read_result_client(void *opaque)
{
	struct read_result_fixture *fixture = opaque;
	struct dssh_chan_params params;
	int ret = dssh_chan_params_init(&params, DSSH_CHAN_EXEC);
	if (ret != 0)
		return ret;
	ret = dssh_chan_params_set_command(&params, "read-result");
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&params, false);
	if (ret == 0)
		ret = dssh_chan_params_set_max_window(&params, 32);
	if (ret == 0) {
		fixture->client = fixture->zero_copy
		    ? dssh_chan_zc_open(fixture->ctx.client, &params, read_result_discard, NULL)
		    : dssh_chan_open(fixture->ctx.client, &params);
		ret = wait_stream_writable(fixture->client);
	}
	dssh_chan_params_free(&params);
	return ret;
}

static int
read_result_terminal(dssh_channel channel, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *data)
{
	(void)channel;
	(void)params;
	struct read_result_fixture *fixture = data;
	result->max_window = 32;
	if (fixture->zero_copy)
		result->zc_cb = read_result_discard;
	return 0;
}

static int
read_result_server(void *opaque)
{
	struct read_result_fixture *fixture = opaque;
	struct dssh_chan_accept_cbs callbacks = {.exec = read_result_terminal, .cbdata = fixture};
	fixture->server = dssh_chan_accept(fixture->ctx.server, &callbacks, 3000);
	return wait_stream_writable(fixture->server);
}

static bool
read_result_setup(struct read_result_fixture *fixture, bool gather)
{
	window_test_gather = gather;
	int ret = conn_setup(&fixture->ctx);
	window_test_gather = false;
	if (ret != 0)
		return false;
	return open_channel_pair(&fixture->ctx, read_result_client, fixture, &fixture->client,
	    read_result_server, fixture, &fixture->server) == 0;
}

static int
read_result_fail_tx(dssh_session session)
{
	(void)session;
	read_result_current->tx_failures++;
	return DSSH_ERROR_INIT;
}

/* A real second ZC channel shares the selected session's packet engine.
 * The stream channel itself correctly rejects the public ZC getbuf API. */
static dssh_channel
read_result_reservation_channel(struct read_result_fixture *fixture, dssh_session session)
{
	dssh_channel client = fixture->client;
	dssh_channel server = fixture->server;
	fixture->zero_copy = true;
	bool valid = open_channel_pair(&fixture->ctx, read_result_client, fixture, &fixture->client,
	    read_result_server, fixture, &fixture->server) == 0;
	dssh_channel result = valid ? session == fixture->ctx.client ? fixture->client : fixture->server : NULL;
	if (valid)
		valid = window_credit_ready(fixture->client, 32) && window_credit_ready(fixture->server, 32);
	fixture->client = client;
	fixture->server = server;
	fixture->zero_copy = false;
	return valid ? result : NULL;
}

static bool
read_result_fill(dssh_channel writer, dssh_channel reader, int stream)
{
	uint8_t input[24];
	for (size_t i = 0; i < sizeof(input); i++)
		input[i] = (uint8_t)(i + 1);
	if (dssh_chan_write(writer, stream, input, sizeof(input)) != 24)
		return false;
	struct timespec start;
	if (!conn_zc_start_clock(&start, 5))
		return false;
	while (dssh_chan_read(reader, stream, NULL, 0) != 24) {
		if (!conn_zc_tick(&start, 5))
			return false;
	}
	return atomic_load(&writer->remote_window) == 8;
}

static bool
read_result_bytes(const uint8_t *bytes, size_t length, size_t offset)
{
	if (offset > 24 || length > 24 - offset)
		return false;
	for (size_t i = 0; i < length; i++) {
		if (bytes[i] != i + offset + 1)
			return false;
	}
	return true;
}

static bool
read_result_case(enum read_result_kind kind, bool reverse, int stream,
    bool gather, size_t amount)
{
	size_t requested = amount;
	struct read_result_fixture fixture = {0};
	if (!read_result_setup(&fixture, gather)) {
		conn_cleanup(&fixture.ctx);
		return false;
	}
	dssh_channel writer = reverse ? fixture.server : fixture.client;
	dssh_channel reader = reverse ? fixture.client : fixture.server;
	dssh_session session = reader->sess;
	bool valid = read_result_fill(writer, reader, stream);
	uint8_t output[26];
	memset(output, 0xa5, sizeof(output));
	uint_fast32_t saved_packets = atomic_load(&session->trans.tx_since_rekey);
	size_t saved_capacity = session->trans.comp_tx_payload_limit;
	read_result_current = &fixture;
	if (kind == RR_EOF) {
		valid = valid && dssh_chan_shutwr(writer) == 0;
		struct timespec start;
		valid = valid && conn_zc_start_clock(&start, 5);
		bool eof = false;
		while (valid && !eof) {
			if (mtx_lock(&reader->buf_mtx) != thrd_success)
				abort();
			eof = reader->eof_received;
			if (mtx_unlock(&reader->buf_mtx) != thrd_success)
				abort();
			if (!eof)
				valid = conn_zc_tick(&start, 5);
		}
	}
	if (kind == RR_TERMINATED)
		dssh_session_terminate(session);
	if (valid) {
		if (kind == RR_TX)
			setup_tx_test_hook = read_result_fail_tx;
		else if (kind == RR_LOCK_BEFORE)
			dssh_test_mtx_reject_once(&reader->buf_mtx);
		else if (kind == RR_LOCK_AFTER)
			dssh_test_thrd_fail_after(1);
		else if (kind == RR_QUEUE_LOCK)
			dssh_test_mtx_reject_once(&session->trans.tx_queue_mtx);
		else if (kind == RR_RANDOM)
			dssh_test_ossl_fail_after(0);
		else if (kind == RR_KEY)
			atomic_store(&session->trans.tx_since_rekey, DSSH_REKEY_HARD_LIMIT);
		else if (kind == RR_SIZE)
			session->trans.comp_tx_payload_limit = 1;
		else if (kind == RR_COMPRESS)
			dssh_test_comp_fail(DSSH_TEST_COMP_COMPRESS, DSSH_ERROR_TOOLONG);
	}
	int64_t delivered = valid ? dssh_chan_read(reader, stream, output + 1, amount) : DSSH_ERROR_INIT;
	unsigned unowned = dssh_test_mtx_rejected_unlocks();
	int thread_calls = dssh_test_thrd_count();
	size_t random_length = 0;
	unsigned int random_strength = 0;
	bool random_completed = false;
#ifdef DSSH_CRYPTO_OPENSSL
	bool random_seen = dssh_test_ossl_random_observed(&random_length, &random_strength, &random_completed);
#else
	bool random_seen = false;
#endif
	setup_tx_test_hook = NULL;
	dssh_test_ossl_reset();
	dssh_test_comp_fail(DSSH_TEST_COMP_NONE, 0);
	bool refused = kind == RR_KEY || kind == RR_SIZE;
	bool failed = kind == RR_TX || kind == RR_LOCK_AFTER || kind == RR_QUEUE_LOCK
	    || kind == RR_RANDOM || kind == RR_COMPRESS || refused;
	int expected_error = kind == RR_KEY ? DSSH_ERROR_REKEY_NEEDED
	    : kind == RR_SIZE || kind == RR_COMPRESS ? DSSH_ERROR_TOOLONG
	    : kind == RR_LOCK_AFTER || kind == RR_QUEUE_LOCK ? DSSH_ERROR_TERMINATED : DSSH_ERROR_INIT;
	int64_t expected_delivered = kind == RR_LOCK_BEFORE ? DSSH_ERROR_TERMINATED : (int64_t)amount;
	valid = valid && delivered == expected_delivered && output[0] == 0xa5
	    && output[amount + 1] == 0xa5 && unowned == 0;
	if (kind == RR_LOCK_BEFORE) {
		for (size_t i = 0; i < sizeof(output); i++)
			valid = valid && output[i] == 0xa5;
		amount = 0;
	}
	else
		valid = valid && read_result_bytes(output + 1, amount, 0);
	valid = valid && dssh_chan_read(reader, stream, NULL, 0) == 24 - (int64_t)amount;
	if (failed) {
		int events = dssh_chan_poll(reader, DSSH_POLL_READ | DSSH_POLL_READEXT, 0);
		uint8_t sentinel = 0xa5;
		int other = stream == 0 ? 1 : 0;
		int64_t error = dssh_chan_read(reader, other, &sentinel, 1);
		valid = valid && events == (DSSH_POLL_READ | DSSH_POLL_READEXT)
		    && error == expected_error && sentinel == 0xa5
		    && dssh_chan_read(reader, stream, NULL, 0) == 24 - (int64_t)amount;
	}
	if (kind == RR_TX)
		valid = valid && fixture.tx_failures == 1;
	if (kind == RR_LOCK_AFTER)
		valid = valid && thread_calls > 1;
	if (kind == RR_RANDOM)
		valid = valid && random_seen && random_length > 0 && random_strength == 256 && !random_completed;
	if (refused) {
		/* The peer is quiescent; these are bounded pre-wire state models. */
		atomic_store(&session->trans.tx_since_rekey, saved_packets);
		session->trans.comp_tx_payload_limit = saved_capacity;
	}
	uint8_t remainder[24];
	memset(remainder, 0xa5, sizeof(remainder));
	int64_t remaining = dssh_chan_read(reader, stream, remainder, sizeof(remainder));
	if (amount == 24) {
		int64_t expected = failed && !refused ? 0
		    : kind == RR_EOF || kind == RR_TERMINATED ? 0 : DSSH_ERROR_NOMORE;
		valid = valid && remaining == expected;
	}
	else
		valid = valid && remaining == 24 - (int64_t)amount
		    && read_result_bytes(remainder, 24 - amount, amount);
	if (refused)
		valid = valid && window_credit_ready(writer, amount == 24 ? 32 : 16)
		    && !dssh_session_is_terminated(session) && !atomic_load(&reader->wa_slot.ready);
	if (kind == RR_EOF || kind == RR_TERMINATED || (failed && !refused)
	    || kind == RR_LOCK_BEFORE) {
		uint8_t sentinel = 0xa5;
		valid = valid && dssh_chan_read(reader, stream, &sentinel, 1) == 0 && sentinel == 0xa5;
	}
	if (printf("read_result kind=%u reverse=%u stream=%d gather=%u delivered=%lld amount=%zu requested=%zu valid=%u\n",
	    (unsigned)kind, reverse ? 1U : 0U, stream, gather ? 1U : 0U,
	    (long long)delivered, amount, requested, valid ? 1U : 0U) < 0)
		valid = false;
	read_result_current = NULL;
	conn_cleanup(&fixture.ctx);
	return valid && fixture.ctx.client == NULL && fixture.ctx.server == NULL;
}

static int
read_result_group(unsigned group)
{
	unsigned cases = 0;
	const char *compression = getenv("DSSH_TEST_COMPRESSION");
	bool growth = compression != NULL && strcmp(compression, "test-growth") == 0;
	for (unsigned kind = 0; kind <= RR_TERMINATED; kind++) {
		bool selected = group == 0 ? kind == RR_TX
		    : group == 1 ? kind <= RR_QUEUE_LOCK && kind != RR_TX
		    : group == 2 ? kind == RR_KEY || (growth && kind == RR_SIZE)
		    : group == 3 ? kind == RR_EOF || kind == RR_TERMINATED
		    : group == 4 ? kind == RR_RANDOM || (growth && kind == RR_COMPRESS) : false;
#ifndef DSSH_CRYPTO_OPENSSL
		if (kind == RR_RANDOM)
			selected = false;
#endif
		if (!selected)
			continue;
		for (unsigned reverse = 0; reverse < 2; reverse++) {
			for (int stream = 0; stream < 2; stream++) {
				for (unsigned gather = 0; gather < 2; gather++) {
					for (unsigned partial = 0; partial < 2; partial++) {
						enum read_result_kind kind_value = (enum read_result_kind)kind;
						if (!read_result_case(kind_value, reverse != 0, stream, gather != 0, partial != 0 ? 8 : 24))
							return TEST_FAIL;
						cases++;
					}
				}
			}
		}
	}
	if (printf("read_result_group=%u cases=%u valid=1\n", group, cases) < 0 || fflush(stdout) != 0)
		return TEST_FAIL;
	return cases != 0 || (group == 4 && !growth) ? TEST_PASS : TEST_FAIL;
}

static int
test_read_result_deferred(void)
{
	return read_result_group(0);
}

static int
test_read_result_failures(void)
{
	return read_result_group(1);
}

static int
test_read_result_retry(void)
{
	return read_result_group(2);
}

static int
test_read_result_terminal(void)
{
	return read_result_group(3);
}

static int
test_read_result_provider(void)
{
	return read_result_group(4);
}

struct read_result_job {
	dssh_channel channel;
	atomic_bool go;
	bool poll;
	int stream;
	int64_t result;
	uint8_t sentinel;
};

static int
read_result_worker(void *opaque)
{
	struct read_result_job *job = opaque;
	if (job->poll)
		job->result = dssh_chan_poll(job->channel, DSSH_POLL_READEXT, -1);
	else {
		if (!wire_wait(job->channel->sess, &job->go))
			return DSSH_ERROR_TERMINATED;
		job->result = dssh_chan_read(job->channel, job->stream, &job->sentinel, 1);
	}
	return 0;
}

static bool
read_result_reservation_case(bool other, bool reverse, int stream, bool gather)
{
	struct read_result_fixture fixture = {0};
	if (!read_result_setup(&fixture, gather)) {
		conn_cleanup(&fixture.ctx);
		return false;
	}
	dssh_channel writer = reverse ? fixture.server : fixture.client;
	dssh_channel reader = reverse ? fixture.client : fixture.server;
	dssh_channel reserved = read_result_reservation_channel(&fixture, other ? writer->sess : reader->sess);
	bool valid = read_result_fill(writer, reader, stream);
	uint8_t *buffer = NULL;
	size_t capacity = 0;
	bool acquired = valid && reserved != NULL && dssh_chan_zc_getbuf(reserved, 0, &buffer, &capacity) == 0;
	valid = valid && acquired && buffer != NULL && capacity > 0;
	if (valid)
		buffer[0] = 0x7a;
	uint8_t output[24];
	memset(output, 0xa5, sizeof(output));
	int64_t ret = valid ? dssh_chan_read(reader, stream, output, sizeof(output)) : DSSH_ERROR_INIT;
	bool owned_after_read = reserved != NULL && tx_owner_is_current(reserved->sess);
	int stamp_after_read = buffer != NULL ? buffer[0] : -1;
	valid = valid && ret == (other ? DSSH_ERROR_INVALID : 24)
	    && owned_after_read && buffer[0] == 0x7a
	    && dssh_chan_read(reader, stream, NULL, 0) == (other ? 24 : 0);
	if (other) {
		for (size_t i = 0; i < sizeof(output); i++)
			valid = valid && output[i] == 0xa5;
	}
	else
		valid = valid && read_result_bytes(output, sizeof(output), 0)
		    && atomic_load(&reader->wa_slot.ready);
	int cancel_result = acquired ? dssh_chan_zc_cancel(reserved) : DSSH_ERROR_INVALID;
	if (acquired)
		valid = cancel_result == 0 && valid;
	if (other)
		valid = valid && dssh_chan_read(reader, stream, output, sizeof(output)) == 24
		    && read_result_bytes(output, sizeof(output), 0);
	valid = valid && window_credit_ready(writer, 32)
	    && !dssh_session_is_terminated(reader->sess) && !dssh_session_is_terminated(writer->sess);
	if (!valid && fprintf(stderr, "read reservation other=%u reverse=%u stream=%d gather=%u acquired=%u capacity=%zu read=%lld owner=%u stamp=%d cancel=%d window=%ju terminated=%u\n",
	    other ? 1U : 0U, reverse ? 1U : 0U, stream, gather ? 1U : 0U,
	    acquired ? 1U : 0U, capacity, (long long)ret, owned_after_read ? 1U : 0U,
	    stamp_after_read, cancel_result, (uintmax_t)atomic_load(&writer->remote_window),
	    dssh_session_is_terminated(reader->sess) ? 1U : 0U) < 0)
		valid = false;
	conn_cleanup(&fixture.ctx);
	return valid;
}

static int
test_read_result_reservations(void)
{
	unsigned cases = 0;
	for (unsigned other = 0; other < 2; other++) {
		for (unsigned reverse = 0; reverse < 2; reverse++) {
			for (int stream = 0; stream < 2; stream++) {
				for (unsigned gather = 0; gather < 2; gather++) {
					if (!read_result_reservation_case(other != 0, reverse != 0, stream, gather != 0))
						return TEST_FAIL;
					cases++;
				}
			}
		}
	}
	return printf("read_result_reservations cases=%u valid=1\n", cases) >= 0 ? TEST_PASS : TEST_FAIL;
}

static bool
read_result_poll_case(bool overwrite, bool reverse, bool gather)
{
	struct read_result_fixture fixture = {0};
	if (!read_result_setup(&fixture, gather)) {
		conn_cleanup(&fixture.ctx);
		return false;
	}
	dssh_channel writer = reverse ? fixture.server : fixture.client;
	dssh_channel reader = reverse ? fixture.client : fixture.server;
	dssh_session session = reader->sess;
	bool valid = read_result_fill(writer, reader, 0);
	unsigned phase = 0;
	dssh_channel reservation = read_result_reservation_channel(&fixture, session);
	uint8_t *buffer = NULL;
	size_t capacity = 0;
	bool reserved = valid && reservation != NULL && dssh_chan_zc_getbuf(reservation, 0, &buffer, &capacity) == 0;
	valid = valid && reserved;
	uint8_t output[24];
	valid = valid && dssh_chan_read(reader, 0, output, sizeof(output)) == 24
	    && read_result_bytes(output, sizeof(output), 0) && atomic_load(&reader->wa_slot.ready);
	if (valid)
		phase = 1;
	struct flow_fixture flow;
	flow_init(&flow, &fixture.ctx, NULL, NULL);
	flow.reserved = reserved ? reservation : NULL;
	struct read_result_job job = {.channel = reader, .poll = true, .sentinel = 0xa5};
	atomic_init(&job.go, false);
	atomic_bool *entered = dssh_test_thrd_watch_condition(&reader->poll_cnd);
	flow.watching = true;
	if (valid)
		valid = channel_worker_start(&flow.workers, 0, read_result_worker, &job) == 0
		    && conn_actor_waiting(entered, &reader->buf_mtx);
	if (valid)
		phase = 2;
	uint_fast32_t saved = atomic_load(&session->trans.tx_since_rekey);
	if (valid) {
		atomic_store(&session->trans.tx_since_rekey, DSSH_REKEY_HARD_LIMIT);
		valid = dssh_chan_zc_cancel(reservation) == 0;
		flow.reserved = NULL;
		if (valid)
			phase = 3;
	}
	if (!valid) {
		flow_release(&flow);
		channel_workers_cancel(&flow.workers);
	}
	flow_finish(&flow);
	valid = valid && flow.workers.results[0] == 0 && job.result == DSSH_POLL_READEXT;
	if (valid)
		phase = 4;
	atomic_store(&session->trans.tx_since_rekey, saved);
	if (valid && overwrite) {
		read_result_current = &fixture;
		setup_tx_test_hook = read_result_fail_tx;
		int ret = schedule_wa_slot(session);
		setup_tx_test_hook = NULL;
		read_result_current = NULL;
		valid = ret == DSSH_ERROR_TERMINATED && fixture.tx_failures == 1;
	}
	uint8_t sentinel = 0xa5;
	valid = valid && dssh_chan_read(reader, 0, NULL, 0) == 0
	    && dssh_chan_read(reader, 1, NULL, 0) == 0
	    && dssh_chan_poll(reader, DSSH_POLL_EVENT, 0) >= 0;
	/* A zero-capacity non-peek must not acknowledge the retained error. */
	if (valid && !overwrite)
		valid = dssh_chan_read(reader, 0, &sentinel, 0) == DSSH_ERROR_NOMORE;
	valid = valid && dssh_chan_read(reader, 1, &sentinel, 1) == DSSH_ERROR_REKEY_NEEDED
	    && sentinel == 0xa5;
	int64_t following = dssh_chan_read(reader, 0, &sentinel, 1);
	valid = valid && following == (overwrite ? 0 : DSSH_ERROR_NOMORE) && sentinel == 0xa5;
	if (!overwrite)
		valid = valid && window_credit_ready(writer, 32)
		    && dssh_chan_poll(reader, DSSH_POLL_READ | DSSH_POLL_READEXT, 0) == 0;
	if (!valid && fprintf(stderr, "read poll overwrite=%u reverse=%u gather=%u phase=%u worker=%d result=%lld following=%lld window=%ju terminated=%u\n",
	    overwrite ? 1U : 0U, reverse ? 1U : 0U, gather ? 1U : 0U,
	    phase, flow.workers.results[0], (long long)job.result, (long long)following,
	    (uintmax_t)atomic_load(&writer->remote_window), dssh_session_is_terminated(session) ? 1U : 0U) < 0)
		valid = false;
	conn_cleanup(&fixture.ctx);
	return valid;
}

static int
test_read_result_poll(void)
{
	unsigned cases = 0;
	for (unsigned overwrite = 0; overwrite < 2; overwrite++) {
		for (unsigned reverse = 0; reverse < 2; reverse++) {
			for (unsigned gather = 0; gather < 2; gather++) {
				if (!read_result_poll_case(overwrite != 0, reverse != 0, gather != 0))
					return TEST_FAIL;
				cases++;
			}
		}
	}
	return printf("read_result_poll cases=%u valid=1\n", cases) >= 0 ? TEST_PASS : TEST_FAIL;
}

static bool
read_result_concurrent_case(bool reverse, int stream, bool gather)
{
	struct read_result_fixture fixture = {0};
	if (!read_result_setup(&fixture, gather)) {
		conn_cleanup(&fixture.ctx);
		return false;
	}
	dssh_channel writer = reverse ? fixture.server : fixture.client;
	dssh_channel reader = reverse ? fixture.client : fixture.server;
	dssh_session session = reader->sess;
	bool valid = read_result_fill(writer, reader, stream);
	uint_fast32_t saved = atomic_load(&session->trans.tx_since_rekey);
	atomic_store(&session->trans.tx_since_rekey, DSSH_REKEY_HARD_LIMIT);
	uint8_t output[24];
	valid = valid && dssh_chan_read(reader, stream, output, sizeof(output)) == 24
	    && read_result_bytes(output, sizeof(output), 0);
	atomic_store(&session->trans.tx_since_rekey, saved);
	struct read_result_job jobs[2] = {{.channel = reader, .stream = 0, .sentinel = 0xa5},
	    {.channel = reader, .stream = 1, .sentinel = 0xa5}};
	atomic_init(&jobs[0].go, false);
	atomic_init(&jobs[1].go, false);
	struct channel_workers workers;
	channel_workers_init(&workers, &fixture.ctx);
	if (valid)
		valid = channel_worker_start(&workers, 0, read_result_worker, &jobs[0]) == 0
		    && channel_worker_start(&workers, 1, read_result_worker, &jobs[1]) == 0;
	atomic_store(&jobs[0].go, true);
	atomic_store(&jobs[1].go, true);
	if (!valid)
		channel_workers_cancel(&workers);
	channel_workers_finish(&workers);
	valid = valid && workers.results[0] == 0 && workers.results[1] == 0
	    && ((jobs[0].result == DSSH_ERROR_REKEY_NEEDED && jobs[1].result == DSSH_ERROR_NOMORE)
	        || (jobs[1].result == DSSH_ERROR_REKEY_NEEDED && jobs[0].result == DSSH_ERROR_NOMORE))
	    && jobs[0].sentinel == 0xa5 && jobs[1].sentinel == 0xa5 && window_credit_ready(writer, 32);
	conn_cleanup(&fixture.ctx);
	return valid;
}

struct read_result_handoff {
	struct read_result_fixture *fixture;
	dssh_channel reader;
	atomic_bool installed;
	atomic_bool *paused;
	int stream;
	int64_t result;
	uint8_t output[24];
};
static _Thread_local struct read_result_handoff *read_result_handoff_current;

static int
read_result_handoff_tx(dssh_session session)
{
	struct read_result_handoff *job = read_result_handoff_current;
	job->paused = dssh_test_thrd_pause_mutex_after(&session->trans.tx_mtx, 0);
	atomic_store(&job->installed, true);
	return read_result_fail_tx(session);
}

static int
read_result_handoff_worker(void *opaque)
{
	struct read_result_handoff *job = opaque;
	read_result_current = job->fixture;
	read_result_handoff_current = job;
	setup_tx_test_hook = read_result_handoff_tx;
	job->result = dssh_chan_read(job->reader, job->stream, job->output, sizeof(job->output));
	setup_tx_test_hook = NULL;
	read_result_handoff_current = NULL;
	read_result_current = NULL;
	return 0;
}

/* A second reader acknowledges the actual TX error while the original
 * reader is still completing termination notification and ownership release. */
static bool
read_result_handoff_case(bool reverse, int stream, bool gather)
{
	struct read_result_fixture fixture = {0};
	if (!read_result_setup(&fixture, gather)) {
		conn_cleanup(&fixture.ctx);
		return false;
	}
	dssh_channel writer = reverse ? fixture.server : fixture.client;
	dssh_channel reader = reverse ? fixture.client : fixture.server;
	bool valid = read_result_fill(writer, reader, stream);
	struct read_result_handoff job = {.fixture = &fixture, .reader = reader, .stream = stream};
	atomic_init(&job.installed, false);
	struct channel_workers workers;
	channel_workers_init(&workers, &fixture.ctx);
	if (valid)
		valid = channel_worker_start(&workers, 0, read_result_handoff_worker, &job) == 0
		    && dssh_test_wait_bool(&job.installed, true)
		    && dssh_test_wait_bool(job.paused, true);
	uint8_t sentinel = 0xa5;
	int64_t first = valid ? dssh_chan_read(reader, 1 - stream, &sentinel, 1) : DSSH_ERROR_INVALID;
	valid = valid && first == DSSH_ERROR_INIT && sentinel == 0xa5;
	dssh_test_thrd_resume_mutex();
	if (!valid)
		channel_workers_cancel(&workers);
	channel_workers_finish(&workers);
	int64_t next = dssh_chan_read(reader, stream, &sentinel, 1);
	valid = valid && workers.results[0] == 0 && job.result == 24
	    && read_result_bytes(job.output, sizeof(job.output), 0)
	    && fixture.tx_failures == 1 && next == 0 && sentinel == 0xa5;
	if (printf("read_result_handoff reverse=%u stream=%d gather=%u first=%lld next=%lld delivered=%lld valid=%u\n",
	    reverse ? 1U : 0U, stream, gather ? 1U : 0U,
	    (long long)first, (long long)next, (long long)job.result, valid ? 1U : 0U) < 0)
		valid = false;
	conn_cleanup(&fixture.ctx);
	return valid;
}

static int
test_read_result_concurrent(void)
{
	unsigned cases = 0;
	for (unsigned reverse = 0; reverse < 2; reverse++) {
		for (int stream = 0; stream < 2; stream++) {
			for (unsigned gather = 0; gather < 2; gather++) {
				if (!read_result_concurrent_case(reverse != 0, stream, gather != 0))
					return TEST_FAIL;
				cases++;
				if (!read_result_handoff_case(reverse != 0, stream, gather != 0))
					return TEST_FAIL;
				cases++;
			}
		}
	}
	/* Refuse each new reader prefix after the two actual open workers. */
	for (unsigned prefix = 2; prefix < 4; prefix++) {
		struct channel_worker_probe probe = {.fail_at = prefix, .cancelled_before_join = true};
		channel_worker_probe_begin(&probe);
		bool host = read_result_concurrent_case(false, 0, false);
		channel_worker_probe_end();
		if (host || !probe.creation_failed || probe.created != prefix || probe.joined != prefix
		    || !probe.cancelled_before_join || g_active_ctx != NULL || channel_fixture_owner != NULL)
			return TEST_FAIL;
		cases++;
	}
	struct channel_worker_probe probe = {.fail_at = 2, .cancelled_before_join = true};
	channel_worker_probe_begin(&probe);
	bool host = read_result_handoff_case(false, 0, false);
	channel_worker_probe_end();
	if (host || !probe.creation_failed || probe.created != 2 || probe.joined != 2
	    || !probe.cancelled_before_join || g_active_ctx != NULL || channel_fixture_owner != NULL)
		return TEST_FAIL;
	cases++;
	return printf("read_result_concurrent cases=%u valid=1\n", cases) >= 0 ? TEST_PASS : TEST_FAIL;
}

#endif
