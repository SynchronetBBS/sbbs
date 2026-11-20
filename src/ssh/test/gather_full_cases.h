/* Real authenticated channel credit drives retained batch growth and splitting.
 * All assertions follow the original connection/worker owners' retirement. */
#ifndef DSSH_GATHER_FULL_CASES_H
#define DSSH_GATHER_FULL_CASES_H

#define GATHER_FULL_CHANNELS 17
struct gather_full_fixture {
	struct conn_ctx ctx;
	struct open_exec_ctx pairs[GATHER_FULL_CHANNELS];
	atomic_bool published, begun, data_seen, done, bad;
	uint64_t rx_bytes_before, rx_bytes_after;
	uint32_t rx_packets_before, rx_packets_after;
	bool credits_valid;
	size_t vectors[4], vector_calls, wire_bytes;
	size_t allocation_calls, allocation_size;
};
static _Thread_local struct gather_full_fixture *gather_full_current;

static int
gather_full_mac(void)
{
	int ret = dssh_register_hmac_sha2_512();
	if (ret == 0)
		ret = dssh_register_hmac_sha2_256();
	return ret;
}

static bool
gather_full_name(const uint8_t *name, size_t length, const char *text)
{
	size_t expected = strlen(text);
	return name != NULL && length == expected && memcmp(name, text, length) == 0;
}

static int
gather_full_server_marker(const uint8_t *name, size_t length, bool reply,
    const uint8_t *data, size_t data_length, void *opaque)
{
	(void)data;
	struct gather_full_fixture *f = opaque;
	if (reply || data_length != 0 || !atomic_load(&f->published)) {
		atomic_store(&f->bad, true);
		return DSSH_ERROR_REJECTED;
	}
	if (gather_full_name(name, length, "gather-begin")) {
		f->rx_bytes_before = f->ctx.server->trans.rx_bytes_since_rekey;
		f->rx_packets_before = f->ctx.server->trans.rx_since_rekey;
		atomic_store(&f->begun, true);
	}
	else if (gather_full_name(name, length, "gather-complete")) {
		f->credits_valid = true;
		for (size_t i = 0; i < GATHER_FULL_CHANNELS; i++) {
			if (f->pairs[i].server_ch == NULL
			    || atomic_load(&f->pairs[i].server_ch->remote_window) != 16)
				f->credits_valid = false;
		}
		f->rx_bytes_after = f->ctx.server->trans.rx_bytes_since_rekey;
		f->rx_packets_after = f->ctx.server->trans.rx_since_rekey;
		atomic_store(&f->done, true);
	}
	else
		atomic_store(&f->bad, true);
	return DSSH_ERROR_REJECTED;
}

static int
gather_full_client_marker(const uint8_t *name, size_t length, bool reply,
    const uint8_t *data, size_t data_length, void *opaque)
{
	(void)data;
	struct gather_full_fixture *f = opaque;
	if (reply || data_length != 0 || !gather_full_name(name, length, "gather-data"))
		atomic_store(&f->bad, true);
	atomic_store(&f->data_seen, true);
	return DSSH_ERROR_REJECTED;
}

static size_t
gather_full_marker(uint8_t buffer[32], const char *name)
{
	size_t length = strlen(name);
	/* All three private literal names fit this fixed complete packet. */
	if (length > 26)
		return 0;
	uint32_t wire_length = (uint32_t)length;
	size_t pos = 0;
	buffer[pos++] = SSH_MSG_GLOBAL_REQUEST;
	DSSH_PUT_U32(wire_length, buffer, &pos);
	memcpy(&buffer[pos], name, length);
	pos += length;
	buffer[pos++] = 0;
	return pos;
}

static int
gather_full_terminal(dssh_channel channel, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *data)
{
	(void)channel;
	(void)params;
	(void)data;
	result->max_window = 16;
	return 0;
}

static int
gather_full_open(void *opaque)
{
	struct open_exec_ctx *pair = opaque;
	struct dssh_chan_params params;
	int ret = dssh_chan_params_init(&params, DSSH_CHAN_EXEC);
	if (ret != 0)
		return ret;
	ret = dssh_chan_params_set_command(&params, "gather-batch");
	if (ret == 0)
		ret = dssh_chan_params_set_max_window(&params, 16);
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&params, false);
	if (ret == 0)
		pair->client_ch = dssh_chan_open(pair->client, &params);
	dssh_chan_params_free(&params);
	return ret == 0 ? wait_stream_writable(pair->client_ch) : ret;
}

static int
gather_full_vector(const struct dssh_tx_iov *iov, size_t count, dssh_session session)
{
	struct gather_full_fixture *f = gather_full_current;
	if (session != f->ctx.client || count == 0 || count > 17 || f->vector_calls >= 4)
		return DSSH_ERROR_INVALID;
	f->vectors[f->vector_calls++] = count;
	for (size_t i = 0; i < count; i++) {
		if (iov[i].base == NULL || iov[i].len == 0 || iov[i].len > SIZE_MAX - f->wire_bytes)
			return DSSH_ERROR_INVALID;
		f->wire_bytes += iov[i].len;
	}
	return 0;
}

static void
gather_full_allocation(size_t count, size_t size, void *opaque)
{
	struct gather_full_fixture *f = opaque;
	if (f->allocation_calls == SIZE_MAX)
		abort();
	f->allocation_calls++;
	if (count != 1)
		atomic_store(&f->bad, true);
	f->allocation_size = size;
}

/* Warm one actual grant, then gather17 grants. Both the decoded marker and
 * the native counters establish that each staged packet was transmitted once. */
static bool
gather_full_exchange(struct gather_full_fixture *f, unsigned channels,
    int point, bool growth_failure, int *allocation_count)
{
	if (channels == 0 || channels > GATHER_FULL_CHANNELS)
		return false;
	atomic_store(&f->begun, false);
	atomic_store(&f->data_seen, false);
	atomic_store(&f->done, false);
	uint8_t marker[32];
	size_t length = gather_full_marker(marker, "gather-begin");
	if (length == 0 || send_packet(f->ctx.client, marker, length, NULL) != 0
	    || !dssh_test_wait_bool(&f->begun, true))
		return false;
	length = gather_full_marker(marker, "gather-complete");
	if (length == 0)
		return false;
	static const uint8_t data[] = {21,22,23,24,25,26,27,28,29};
	bool valid = true;
	/* Complete opposite-direction sends before reserving this thread's client
	 * engine. Engine ownership excludes nested acquisition on any session. */
	for (unsigned i = 0; i < channels && valid; i++)
		valid = dssh_chan_write(f->pairs[i].server_ch, 0, data, sizeof(data)) == sizeof(data);
	uint8_t barrier[32];
	size_t barrier_length = gather_full_marker(barrier, "gather-data");
	if (valid)
		valid = barrier_length != 0 && send_packet(f->ctx.server, barrier, barrier_length, NULL) == 0
		    && dssh_test_wait_bool(&f->data_seen, true);
	if (!valid)
		return false;
	size_t capacity = 0;
	int error = DSSH_ERROR_INIT;
	uint8_t *payload = send_begin(f->ctx.client, marker[0], &capacity, &error);
	if (payload == NULL)
		return false;
	valid = capacity >= length;
	uint_fast32_t count_before = atomic_load(&f->ctx.client->trans.tx_since_rekey);
	uint_fast64_t bytes_before = atomic_load(&f->ctx.client->trans.tx_bytes_since_rekey);
	uint32_t first = f->ctx.client->trans.tx_seq;
	valid = valid && first <= UINT32_MAX - GATHER_FULL_CHANNELS;
	if (valid)
		memcpy(payload, marker, length);
	for (unsigned i = 0; i < channels && valid; i++) {
		uint8_t received[sizeof(data)];
		valid = dssh_chan_read(f->pairs[i].client_ch, 0, received, sizeof(received)) == sizeof(received)
		    && memcmp(received, data, sizeof(data)) == 0
		    && atomic_load(&f->pairs[i].client_ch->wa_slot.ready);
	}
	if (!valid) {
		send_cancel(f->ctx.client);
		return false;
	}
	f->vector_calls = f->wire_bytes = f->allocation_calls = f->allocation_size = 0;
	gather_full_current = f;
	gather_vector_test_hook = gather_full_vector;
	dssh_test_alloc_observe_allocations(gather_full_allocation, f);
	dssh_test_alloc_fail_after(point);
	uint32_t last = 0;
	int result = send_commit(f->ctx.client, length, &last);
	*allocation_count = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	gather_vector_test_hook = NULL;
	gather_full_current = NULL;
	if (growth_failure) {
		return result == DSSH_ERROR_TERMINATED && f->ctx.client->terminate
		    && *allocation_count == 1 && f->allocation_calls == 1 && f->allocation_size == 2048
		    && f->vector_calls == 0 && f->wire_bytes == 0
		    && f->ctx.client->trans.tx_gather_capacity == 1024 && !atomic_load(&f->bad);
	}
	valid = result == 0 && !f->ctx.client->terminate && dssh_test_wait_bool(&f->done, true)
	    && f->credits_valid && !atomic_load(&f->bad)
	    && f->rx_packets_after >= f->rx_packets_before
	    && f->rx_packets_after - f->rx_packets_before == channels + 1
	    && f->rx_bytes_after >= f->rx_bytes_before
	    && f->rx_bytes_after - f->rx_bytes_before == f->wire_bytes
	    && atomic_load(&f->ctx.client->trans.tx_since_rekey) >= count_before
	    && atomic_load(&f->ctx.client->trans.tx_since_rekey) - count_before == channels + 1
	    && atomic_load(&f->ctx.client->trans.tx_bytes_since_rekey) >= bytes_before
	    && atomic_load(&f->ctx.client->trans.tx_bytes_since_rekey) - bytes_before == f->wire_bytes
	    && last == first + channels;
	if (channels == 1)
		valid = valid && f->vector_calls == 1 && f->vectors[0] == 2;
	else
		valid = valid && f->vector_calls == 2 && f->vectors[0] == 16 && f->vectors[1] == 2;
	return valid;
}

static bool
gather_full_case(int point)
{
	struct gather_full_fixture f = {0};
	atomic_init(&f.published, false);
	atomic_init(&f.begun, false);
	atomic_init(&f.data_seen, false);
	atomic_init(&f.done, false);
	atomic_init(&f.bad, false);
	dssh_test_alloc_reset();
	window_test_gather = true;
	conn_mac_registration = gather_full_mac;
	int setup = conn_setup_no_start(&f.ctx);
	window_test_gather = false;
	conn_mac_registration = NULL;
	bool valid = setup == 0;
	if (valid)
		valid = f.ctx.client->trans.mac_c2s_selected->digest_size == 64
		    && dssh_session_set_global_request_cb(f.ctx.client, gather_full_client_marker, &f) == 0
		    && dssh_session_set_global_request_cb(f.ctx.server, gather_full_server_marker, &f) == 0;
	dssh_test_alloc_exclude_new_threads();
	if (valid)
		valid = dssh_session_start(f.ctx.client) == 0 && dssh_session_start(f.ctx.server) == 0;
	struct dssh_chan_accept_cbs callbacks = {.exec = gather_full_terminal};
	unsigned pairs = 0;
	for (size_t i = 0; i < GATHER_FULL_CHANNELS && valid; i++) {
		struct open_exec_ctx *pair = &f.pairs[i];
		pair->client = f.ctx.client;
		pair->server = f.ctx.server;
		pair->cbs = &callbacks;
		pair->accept_timeout = 5000;
		valid = open_channel_pair(&f.ctx, gather_full_open, pair, &pair->client_ch,
		    server_accept_exec_thread, pair, &pair->server_ch) == 0;
		if (valid)
			pairs++;
	}
	atomic_store(&f.published, true);
	int allocations = -1;
	if (valid)
		valid = gather_full_exchange(&f, 1, INT_MAX, false, &allocations)
		    && f.ctx.client->trans.tx_gather_capacity == 1024;
	if (valid)
		valid = gather_full_exchange(&f, GATHER_FULL_CHANNELS,
		    point < 0 ? INT_MAX : point, point == 0, &allocations);
	int growth_calls = allocations;
	bool reused = false;
	if (valid && point != 0) {
		valid = growth_calls == 1 && f.allocation_calls == 1 && f.allocation_size == 2048
		    && f.ctx.client->trans.tx_gather_capacity == 2048;
		uint8_t *storage = f.ctx.client->trans.tx_gather_buf;
		if (valid)
			valid = gather_full_exchange(&f, GATHER_FULL_CHANNELS, 0, false, &allocations)
			    && allocations == 0 && f.allocation_calls == 0
			    && f.ctx.client->trans.tx_gather_buf == storage;
		reused = valid;
	}
	bool watched = f.ctx.client != NULL && f.ctx.client->trans.tx_gather_buf != NULL;
	if (watched)
		dssh_test_alloc_watch_free(f.ctx.client->trans.tx_gather_buf, f.ctx.client->trans.tx_gather_capacity);
	/* Both helper workers joined before every pair-opening return. All marker
	 * callback storage remains live until connection cleanup joins the demuxes. */
	conn_cleanup(&f.ctx);
	bool cleared = false;
	bool freed = dssh_test_alloc_watched_free(&cleared);
	dssh_test_alloc_reset();
	valid = valid && pairs == GATHER_FULL_CHANNELS && watched && freed && cleared;
	if (printf("gather_full point=%d setup=%d pairs=%u growth_calls=%d reused=%u freed=%u cleared=%u valid=%u\n",
	    point, setup, pairs, growth_calls, reused ? 1U : 0U, freed ? 1U : 0U,
	    cleared ? 1U : 0U, valid ? 1U : 0U) < 0)
		valid = false;
	return valid;
}

static int
test_gather_full_batch(void)
{
	for (int point = -1; point < 2; point++) {
		bool valid = gather_full_case(point);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

#endif
