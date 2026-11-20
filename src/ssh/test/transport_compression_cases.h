#ifndef DSSH_TRANSPORT_COMPRESSION_CASES_H
#define DSSH_TRANSPORT_COMPRESSION_CASES_H

struct compression_sender {
	struct handshake_ctx *pair;
	dssh_session sess;
	const uint8_t *payload;
	size_t length;
	int result;
	bool do_rekey;
	bool exclude_alloc;
	atomic_bool *completed;
	mtx_t *watched_mutex;
};

static int
compression_sender_thread(void *data)
{
	struct compression_sender *sender = data;
	dssh_test_mtx_watch_ownership(sender->watched_mutex);
	compression_callback_session = sender->sess;
	if (sender->exclude_alloc)
		dssh_test_alloc_exclude_thread();
	sender->result = sender->do_rekey ? rekey(sender->sess) :
	    send_packet(sender->sess, sender->payload, sender->length, NULL);
	compression_callback_session = NULL;
	dssh_test_mtx_watch_ownership(NULL);
	if (sender->completed != NULL)
		atomic_store(sender->completed, true);
	if (sender->result < 0) {
		mock_io_close_c2s(&sender->pair->io);
		mock_io_close_s2c(&sender->pair->io);
	}
	return 0;
}

static bool
compression_bytes_zero(const uint8_t *buf, size_t length)
{
	if (buf == NULL)
		return false;
	for (size_t i = 0; i < length; i++) {
		if (buf[i] != 0)
			return false;
	}
	return true;
}

static bool
compression_exchange(struct handshake_ctx *pair, bool from_client, const uint8_t *payload, size_t length)
{
	dssh_session tx = from_client ? pair->client : pair->server;
	dssh_session rx = from_client ? pair->server : pair->client;
	struct compression_sender sender = { .pair = pair, .sess = tx, .payload = payload,
	    .length = length, .result = -999 };
	thrd_t thread;
	if (thrd_create(&thread, compression_sender_thread, &sender) != thrd_success)
		return false;
	uint8_t type;
	uint8_t *received;
	size_t received_len;
	compression_callback_session = rx;
	int ret = recv_packet(rx, &type, &received, &received_len);
	compression_callback_session = NULL;
	if (ret < 0) {
		mock_io_close_c2s(&pair->io);
		mock_io_close_s2c(&pair->io);
	}
	int joined = thrd_join(thread, NULL);
	if (joined != thrd_success)
		abort();
	return ret == 0 && sender.result == 0
	    && type == payload[0] && received_len == length && memcmp(received, payload, length) == 0
	    && compression_bytes_zero(tx->trans.comp_tx_packet, 4 + tx->trans.packet_buf_sz)
	    && compression_bytes_zero(rx->trans.comp_rx_input, rx->trans.packet_buf_sz);
}

static int
compression_handshake_with(struct handshake_ctx *ctx, int (*registration)(void))
{
	extra_algorithm_registration = registration;
	int ret = handshake_setup(ctx);
	extra_algorithm_registration = NULL;
	return ret;
}

static int
compression_handshake(struct handshake_ctx *ctx)
{
	return compression_handshake_with(ctx, dssh_test_register_growth_comp);
}

static size_t
compression_capacity(dssh_session sess)
{
	size_t length = 0;
	int error;
	uint8_t *buf = send_begin(sess, SSH_MSG_CHANNEL_DATA, &length, &error);
	if (buf == NULL)
		return 0;
	send_cancel(sess);
	return length;
}

static int
test_compression_expansion(void)
{
	ASSERT_OK(dssh_test_comp_reset());
	struct handshake_ctx ctx;
	if (compression_handshake(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	size_t capacity = compression_capacity(ctx.client);
	size_t server_capacity = compression_capacity(ctx.server);
	bool valid = capacity > 0 && capacity == server_capacity
	    && capacity == ctx.client->trans.comp_tx_payload_limit;
	uint8_t *payload = capacity > 0 ? malloc(capacity) : NULL;
	if (payload == NULL) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	for (size_t i = 0; i < capacity; i++) {
		uint8_t value = (uint8_t)(i % 251);
		payload[i] = value;
	}
	payload[0] = SSH_MSG_CHANNEL_DATA;
	/* Ordinary packet I/O must reuse the initialized library/module buffers.
	 * Initial EXT_INFO adds one transform in each direction after four data packets. */
	dssh_test_alloc_fail_after(0);
	for (unsigned round = 0; round < 4 && valid; round++)
		valid = compression_exchange(&ctx, round % 2 == 0, payload, capacity);
	int allocation_calls = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	struct dssh_test_comp_stats stats;
	dssh_test_comp_stats(&stats);
	valid = valid && allocation_calls == 0 && stats.allocations == 4 && stats.live == 4
	    && stats.compressions == 5 && stats.decompressions == 5 && stats.owner_errors == 0;
	free(payload);
	handshake_cleanup(&ctx);
	dssh_test_comp_stats(&stats);
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(stats.live, 0);
	ASSERT_EQ_U(stats.cleanups, 4);
	return TEST_PASS;
}

static bool
compression_rekey_results(struct handshake_ctx *pair, int *client_result, int *server_result,
    dssh_session allocation_target)
{
	struct compression_sender client = { .pair = pair, .sess = pair->client,
	    .result = -999, .do_rekey = true,
	    .exclude_alloc = allocation_target != NULL && allocation_target != pair->client };
	struct compression_sender server = { .pair = pair, .sess = pair->server,
	    .result = -999, .do_rekey = true,
	    .exclude_alloc = allocation_target != NULL && allocation_target != pair->server };
	thrd_t ct, st;
	if (thrd_create(&ct, compression_sender_thread, &client) != thrd_success)
		return false;
	if (thrd_create(&st, compression_sender_thread, &server) != thrd_success) {
		mock_io_close_c2s(&pair->io);
		mock_io_close_s2c(&pair->io);
		if (thrd_join(ct, NULL) != thrd_success)
			abort();
		return false;
	}
	int cj = thrd_join(ct, NULL);
	int sj = thrd_join(st, NULL);
	if (cj != thrd_success || sj != thrd_success)
		abort();
	*client_result = client.result;
	*server_result = server.result;
	return client.result == 0 && server.result == 0;
}

static bool
compression_rekey_pair(struct handshake_ctx *pair)
{
	int client, server;
	return compression_rekey_results(pair, &client, &server, NULL);
}

static int
test_compression_rekey_reuse(void)
{
	ASSERT_OK(dssh_test_comp_reset());
	struct handshake_ctx ctx;
	if (compression_handshake(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	dssh_comp_ctx *client_tx = ctx.client->trans.comp_c2s_ctx;
	dssh_comp_ctx *client_rx = ctx.client->trans.comp_s2c_ctx;
	dssh_comp_ctx *server_rx = ctx.server->trans.comp_c2s_ctx;
	dssh_comp_ctx *server_tx = ctx.server->trans.comp_s2c_ctx;
	uint8_t *tx_buffer = ctx.client->trans.comp_tx_packet;
	uint8_t *rx_buffer = ctx.client->trans.comp_rx_input;
	uint8_t payload[] = { SSH_MSG_CHANNEL_DATA, 1, 2, 3, 4 };
	bool valid = compression_exchange(&ctx, true, payload, sizeof(payload))
	    && compression_exchange(&ctx, false, payload, sizeof(payload))
	    && compression_rekey_pair(&ctx);
	valid = valid && client_tx == ctx.client->trans.comp_c2s_ctx
	    && client_rx == ctx.client->trans.comp_s2c_ctx && server_rx == ctx.server->trans.comp_c2s_ctx
	    && server_tx == ctx.server->trans.comp_s2c_ctx && tx_buffer == ctx.client->trans.comp_tx_packet
	    && rx_buffer == ctx.client->trans.comp_rx_input;
	if (valid)
		valid = compression_exchange(&ctx, true, payload, sizeof(payload))
		    && compression_exchange(&ctx, false, payload, sizeof(payload));
	struct dssh_test_comp_stats stats;
	dssh_test_comp_stats(&stats);
	valid = valid && stats.allocations == 4 && stats.resets == 4 && stats.cleanups == 0
	    && stats.live == 4 && stats.owner_errors == 0;
	handshake_cleanup(&ctx);
	dssh_test_comp_stats(&stats);
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(stats.live, 0);
	ASSERT_EQ_U(stats.cleanups, 4);
	return TEST_PASS;
}

static int
test_compression_transform_failures(void)
{
	const int outcomes[] = { -77, DSSH_ERROR_TOOLONG, DSSH_ERROR_REKEY_NEEDED, 3, 0 };
	for (unsigned direction = 0; direction < 2; direction++) {
		for (size_t i = 0; i < sizeof(outcomes) / sizeof(outcomes[0]); i++) {
			ASSERT_OK(dssh_test_comp_reset());
			struct handshake_ctx ctx;
			if (compression_handshake(&ctx) < 0) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			enum dssh_test_comp_stage stage = direction == 0 ? DSSH_TEST_COMP_COMPRESS : DSSH_TEST_COMP_UNCOMPRESS;
			dssh_test_comp_fail(stage, outcomes[i]);
			uint8_t packet[] = { SSH_MSG_CHANNEL_DATA, 1, 2, 3 };
			int ret = send_packet(ctx.client, packet, sizeof(packet), NULL);
			if (direction != 0 && ret == 0) {
				uint8_t type;
				uint8_t *received;
				size_t length;
				ret = recv_packet(ctx.server, &type, &received, &length);
			}
			dssh_session failed = direction == 0 ? ctx.client : ctx.server;
			int expected = outcomes[i] < 0 ? outcomes[i] : DSSH_ERROR_INVALID;
			bool valid = ret == expected && dssh_session_is_terminated(failed)
			    && atomic_load(&failed->terminate_notified)
			    && !tx_owner_is_current(ctx.client)
			    && compression_bytes_zero(ctx.client->trans.comp_tx_packet, 4 + ctx.client->trans.packet_buf_sz)
			    && (direction == 0 || compression_bytes_zero(ctx.server->trans.comp_rx_input,
			        ctx.server->trans.packet_buf_sz));
			handshake_cleanup(&ctx);
			struct dssh_test_comp_stats stats;
			dssh_test_comp_stats(&stats);
			ASSERT_TRUE(valid);
			ASSERT_EQ_U(stats.live, 0);
			ASSERT_EQ_U(stats.owner_errors, 0);
		}
	}
	return TEST_PASS;
}

static int
test_compression_zlib(void)
{
	if (!dssh_test_zlib_available())
		return TEST_SKIP;
	struct handshake_ctx ctx;
	if (compression_handshake_with(&ctx, dssh_test_register_zlib_comp) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	size_t capacity = compression_capacity(ctx.client);
	bool valid = capacity >= 32768 && capacity == compression_capacity(ctx.server);
	uint8_t *payload = capacity > 0 ? malloc(capacity) : NULL;
	if (payload == NULL) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint32_t seed = 1;
	for (unsigned pattern = 0; pattern < 4 && valid; pattern++) {
		for (size_t i = 0; i < capacity; i++) {
			seed = seed * UINT32_C(1664525) + UINT32_C(1013904223);
			uint8_t value = (uint8_t)(seed >> 24);
			payload[i] = pattern % 2 == 0 ? 0 : value;
		}
		payload[0] = SSH_MSG_CHANNEL_DATA;
		valid = compression_exchange(&ctx, pattern % 2 == 0, payload, capacity);
	}
	unsigned before, live_count, cleanup_errors;
	dssh_test_zlib_stats(&before, &live_count, &cleanup_errors);
	dssh_comp_ctx *client_tx = ctx.client->trans.comp_c2s_ctx;
	dssh_comp_ctx *client_rx = ctx.client->trans.comp_s2c_ctx;
	if (valid)
		valid = compression_rekey_pair(&ctx);
	valid = valid && client_tx == ctx.client->trans.comp_c2s_ctx
	    && client_rx == ctx.client->trans.comp_s2c_ctx;
	if (valid)
		valid = compression_exchange(&ctx, true, payload, capacity)
		    && compression_exchange(&ctx, false, payload, capacity);
	unsigned after;
	dssh_test_zlib_stats(&after, &live_count, &cleanup_errors);
	valid = valid && before == after && live_count > 0 && cleanup_errors == 0;
	free(payload);
	handshake_cleanup(&ctx);
	dssh_test_zlib_stats(&after, &live_count, &cleanup_errors);
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(live_count, 0);
	ASSERT_EQ_U(cleanup_errors, 0);
	return TEST_PASS;
}

static int
compression_register_both(void)
{
	int ret = dssh_test_register_growth_comp();
	if (ret < 0)
		return ret;
	return dssh_test_register_growth_alt();
}

static int
test_compression_provider_changes(void)
{
	ASSERT_OK(dssh_test_comp_reset());
	struct handshake_ctx ctx;
	if (compression_handshake_with(&ctx, compression_register_both) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint8_t *tx_buffer = ctx.client->trans.comp_tx_packet;
	uint8_t *rx_buffer = ctx.client->trans.comp_rx_input;
	const char *methods[] = { "test-growth-alt", "none", "test-growth" };
	uint8_t payload[] = { SSH_MSG_CHANNEL_DATA, 31, 32, 33 };
	bool valid = true;
	for (size_t i = 0; i < sizeof(methods) / sizeof(methods[0]) && valid; i++) {
		valid = dssh_session_set_comp_filter(ctx.client, &methods[i], 1) == 0
		    && dssh_session_set_comp_filter(ctx.server, &methods[i], 1) == 0
		    && compression_rekey_pair(&ctx);
		if (valid)
			valid = strcmp(ctx.client->trans.comp_c2s_active->name, methods[i]) == 0
			    && strcmp(ctx.server->trans.comp_s2c_active->name, methods[i]) == 0
			    && compression_exchange(&ctx, true, payload, sizeof(payload))
			    && compression_exchange(&ctx, false, payload, sizeof(payload))
			    && tx_buffer == ctx.client->trans.comp_tx_packet
			    && rx_buffer == ctx.client->trans.comp_rx_input;
	}
	struct dssh_test_comp_stats stats;
	dssh_test_comp_stats(&stats);
	valid = valid && stats.allocations == 12 && stats.cleanups == 8
	    && stats.live == 4 && stats.owner_errors == 0;
	handshake_cleanup(&ctx);
	dssh_test_comp_stats(&stats);
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(stats.live, 0);
	ASSERT_EQ_U(stats.cleanups, 12);
	ASSERT_EQ_U(stats.owner_errors, 0);
	return TEST_PASS;
}

static int
test_compression_setup_failures(void)
{
	const enum dssh_test_comp_stage stages[] = { DSSH_TEST_COMP_INIT, DSSH_TEST_COMP_BOUND };
	const int results[] = { -77, 3 };
	for (size_t stage = 0; stage < sizeof(stages) / sizeof(stages[0]); stage++) {
		for (size_t result = 0; result < sizeof(results) / sizeof(results[0]); result++) {
			for (unsigned reset = 0; reset < 2; reset++) {
				ASSERT_OK(dssh_test_comp_reset());
				struct handshake_ctx ctx;
				if (reset == 0)
					dssh_test_comp_fail(stages[stage], results[result]);
				int ret = compression_handshake(&ctx);
				bool failed = ret < 0;
				if (reset != 0 && ret == 0) {
					dssh_test_comp_fail(stages[stage], results[result]);
					failed = !compression_rekey_pair(&ctx);
				}
				bool terminated = ctx.client != NULL && ctx.server != NULL
				    && (dssh_session_is_terminated(ctx.client) || dssh_session_is_terminated(ctx.server));
				handshake_cleanup(&ctx);
				struct dssh_test_comp_stats stats;
				dssh_test_comp_stats(&stats);
				ASSERT_TRUE(failed && terminated);
				ASSERT_EQ_U(stats.live, 0);
				ASSERT_EQ_U(stats.allocations, stats.cleanups);
				ASSERT_EQ_U(stats.owner_errors, 0);
			}
		}
	}
	return TEST_PASS;
}

static int
test_compression_registration(void)
{
	for (unsigned missing = 0; missing < 5; missing++) {
		dssh_test_reset_global_config();
		ASSERT_OK(dssh_register_none_comp());
		dssh_comp comp = malloc(sizeof(*comp) + sizeof("test-incomplete"));
		ASSERT_NOT_NULL(comp);
		*comp = *gconf.comp_head;
		comp->next = NULL;
		memcpy(comp->name, "test-incomplete", sizeof("test-incomplete"));
		switch (missing) {
			case 0:
				comp->init = NULL;
				break;
			case 1:
				comp->bound = NULL;
				break;
			case 2:
				comp->compress = NULL;
				break;
			case 3:
				comp->uncompress = NULL;
				break;
			default:
				comp->cleanup = NULL;
				break;
		}
		int ret = dssh_transport_register_comp(comp);
		bool valid = ret == DSSH_ERROR_INVALID && gconf.comp_head->next == NULL;
		/* Rejected registration must leave ownership with this caller. */
		free(comp);
		dssh_test_reset_global_config();
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_compression_zlib_allocations(void)
{
	if (!dssh_test_zlib_available())
		return TEST_SKIP;
	dssh_test_reset_global_config();
	ASSERT_OK(dssh_test_register_zlib_comp());
	dssh_comp module = gconf.comp_head;
	bool valid = true;
	for (unsigned direction = 0; direction < 2 && valid; direction++) {
		bool succeeded = false;
		for (int limit = 0; limit < 16 && valid; limit++) {
			dssh_comp_ctx *ctx = NULL;
			dssh_test_zlib_fail_after(limit);
			int ret = module->init(direction == 0, &ctx);
			dssh_test_zlib_fail_after(-1);
			if (ret == 0)
				succeeded = true;
			else if (ret != DSSH_ERROR_ALLOC)
				valid = false;
			module->cleanup(ctx);
			unsigned allocated, remaining, errors;
			dssh_test_zlib_stats(&allocated, &remaining, &errors);
			valid = valid && remaining == 0 && errors == 0;
			if (succeeded)
				break;
		}
		valid = valid && succeeded;
	}
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
compression_register_gather(void)
{
	int ret = dssh_transport_set_tx_gather(test_gather_cb);
	if (ret < 0)
		return ret;
	return dssh_test_register_growth_comp();
}

static int
test_compression_gather_slots(void)
{
	ASSERT_OK(dssh_test_comp_reset());
	struct handshake_ctx ctx;
	if (compression_handshake_with(&ctx, compression_register_gather) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint8_t packet[] = { SSH_MSG_CHANNEL_DATA, 21, 22, 23, 24 };
	size_t capacity;
	int error;
	uint8_t *buf = send_begin(ctx.client, packet[0], &capacity, &error);
	if (buf == NULL) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint32_t first_seq = ctx.client->trans.tx_seq;
	bool valid = capacity >= sizeof(packet);
	uint8_t success = SSH_MSG_REQUEST_SUCCESS;
	uint8_t failure = SSH_MSG_REQUEST_FAILURE;
	int queued1 = send_to_slot(ctx.client, &ctx.client->trans.global_reply_slot, &success, 1);
	int queued2 = send_to_slot(ctx.client, &ctx.client->trans.open_fail_slot, &failure, 1);
	valid = valid && queued1 == 0 && queued2 == 0
	    && atomic_load(&ctx.client->trans.global_reply_slot.ready)
	    && atomic_load(&ctx.client->trans.open_fail_slot.ready);
	uint32_t actual_seq = 0;
	if (valid) {
		memcpy(buf, packet, sizeof(packet));
		valid = send_commit(ctx.client, sizeof(packet), &actual_seq) == 0
		    && actual_seq == first_seq + 2;
	}
	else
		send_cancel(ctx.client);
	for (unsigned i = 0; i < 3 && valid; i++) {
		uint8_t type;
		uint8_t *received;
		size_t length;
		int ret = recv_packet(ctx.server, &type, &received, &length);
		if (i < 2)
			valid = ret == 0 && length == 1 && type == (i == 0 ? success : failure);
		else
			valid = ret == 0 && length == sizeof(packet) && memcmp(received, packet, sizeof(packet)) == 0;
		valid = valid && ctx.server->trans.last_rx_seq == first_seq + i;
	}
	struct dssh_test_comp_stats stats;
	dssh_test_comp_stats(&stats);
	/* Initial server EXT_INFO is compressed but remains unread on this
	 * client-to-server-only fixture; the three gathered packets decode here. */
	valid = valid && stats.compressions == 4 && stats.decompressions == 3
	    && stats.owner_errors == 0
	    && compression_bytes_zero(ctx.client->trans.comp_tx_packet, 4 + ctx.client->trans.packet_buf_sz);
	handshake_cleanup(&ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_compression_mac_before_decode(void)
{
	ASSERT_OK(dssh_test_comp_reset());
	struct handshake_ctx ctx;
	if (compression_handshake(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint8_t packet[] = { SSH_MSG_CHANNEL_DATA, 1, 2, 3 };
	int ret = send_packet(ctx.client, packet, sizeof(packet), NULL);
	uint8_t wire[256];
	size_t length = mock_io_drain(&ctx.io.c2s, wire, sizeof(wire));
	bool valid = ret == 0 && length > 0 && length < sizeof(wire);
	if (valid) {
		wire[length - 1] ^= 1;
		valid = mock_io_inject(&ctx.io.c2s, wire, length) == length;
	}
	if (valid) {
		uint8_t type;
		uint8_t *received;
		size_t received_len;
		ret = recv_packet(ctx.server, &type, &received, &received_len);
		valid = ret < 0 && dssh_session_is_terminated(ctx.server);
	}
	struct dssh_test_comp_stats stats;
	dssh_test_comp_stats(&stats);
	valid = valid && stats.decompressions == 0;
	handshake_cleanup(&ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_compression_zlib_bounds(void)
{
	if (!dssh_test_zlib_available())
		return TEST_SKIP;
	dssh_test_reset_global_config();
	ASSERT_OK(dssh_test_register_zlib_comp());
	dssh_comp module = gconf.comp_head;
	dssh_comp_ctx *tx = NULL;
	dssh_comp_ctx *rx = NULL;
	int tx_init = module->init(true, &tx);
	int rx_init = module->init(false, &rx);
	bool valid = tx_init == 0 && rx_init == 0;
	uint8_t *input = malloc(65536);
	uint8_t *output = malloc(66000);
	uint8_t *decoded = malloc(65537);
	if (input == NULL || output == NULL || decoded == NULL)
		valid = false;
	const size_t lengths[] = { 0, 1, 2, 3, 7, 16, 127, 255, 1024, 4095, 4096,
	    8191, 16383, 16384, 32767, 32768, 65535, 65536 };
	uint32_t random = UINT32_C(0x1234abcd);
	for (unsigned pattern = 0; pattern < 6 && valid; pattern++) {
		for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]) && valid; i++) {
			size_t length = lengths[i];
			for (size_t j = 0; j < length; j++) {
				random = random * UINT32_C(1664525) + UINT32_C(1013904223);
				uint8_t byte = (uint8_t)(random >> 24);
				input[j] = pattern % 3 == 0 ? 0 : pattern % 3 == 1 ? byte : 255;
			}
			size_t bound = 0;
			int ret = module->bound(length, &bound, tx);
			valid = ret == 0 && bound >= length && bound < 66000;
			if (!valid)
				break;
			output[bound] = 0xA5;
			size_t compressed = SIZE_MAX;
			ret = module->compress(input, length, output, bound, &compressed, tx);
			valid = ret == 0 && compressed <= bound && output[bound] == 0xA5;
			if (!valid)
				break;
			decoded[length] = 0x5A;
			size_t decompressed = SIZE_MAX;
			ret = module->uncompress(output, compressed, decoded, length, &decompressed, rx);
			valid = ret == 0 && decompressed == length && decoded[length] == 0x5A
			    && memcmp(input, decoded, length) == 0;
		}
		if (pattern == 2 && valid)
			valid = module->init(true, &tx) == 0 && module->init(false, &rx) == 0;
	}
	size_t unused;
	if (valid)
		valid = module->bound(SIZE_MAX, &unused, tx) == DSSH_ERROR_TOOLONG
		    && module->bound(SIZE_MAX - 32, &unused, tx) == DSSH_ERROR_TOOLONG;
	/* Decode with deliberately insufficient space must fail within its span. */
	if (valid) {
		size_t compressed, decompressed;
		valid = module->init(true, &tx) == 0 && module->init(false, &rx) == 0;
		memset(input, 'a', 4096);
		if (valid)
			valid = module->compress(input, 4096, output, 66000, &compressed, tx) == 0;
		if (valid) {
			decoded[4095] = 0xA5;
			valid = module->uncompress(output, compressed, decoded, 4095, &decompressed, rx)
			    == DSSH_ERROR_TOOLONG && decoded[4095] == 0xA5;
		}
	}
	module->cleanup(tx);
	module->cleanup(rx);
	free(input);
	free(output);
	free(decoded);
	unsigned allocated, remaining, errors;
	dssh_test_zlib_stats(&allocated, &remaining, &errors);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(remaining, 0);
	ASSERT_EQ_U(errors, 0);
	return TEST_PASS;
}

static int
test_compression_newkeys_framing(void)
{
	ASSERT_OK(dssh_test_comp_reset());
	struct handshake_ctx ctx;
	if (compression_handshake(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	uint8_t malformed[] = { SSH_MSG_NEWKEYS, 0 };
	bool valid = send_packet(ctx.client, malformed, sizeof(malformed), NULL) == 0;
	size_t capacity;
	int error;
	uint8_t *buf = send_begin(ctx.server, SSH_MSG_CHANNEL_DATA, &capacity, &error);
	if (buf == NULL)
		valid = false;
	else {
		uint8_t reply = SSH_MSG_REQUEST_SUCCESS;
		valid = valid && send_to_slot(ctx.server, &ctx.server->trans.global_reply_slot, &reply, 1) == 0;
		atomic_store(&ctx.server->trans.rekey_in_progress, true);
		send_cancel(ctx.server);
	}
	uint32_t before = ctx.server->trans.tx_seq;
	if (valid) {
		int ret = newkeys(ctx.server);
		valid = ret == DSSH_ERROR_PARSE && dssh_session_is_terminated(ctx.server)
		    && atomic_load(&ctx.server->terminate_notified)
		    && ctx.server->trans.tx_seq == before + 1
		    && atomic_load(&ctx.server->trans.global_reply_slot.ready)
		    && !tx_owner_is_current(ctx.server);
	}
	handshake_cleanup(&ctx);
	struct dssh_test_comp_stats stats;
	dssh_test_comp_stats(&stats);
	ASSERT_TRUE(valid);
	ASSERT_EQ_U(stats.live, 0);
	ASSERT_EQ_U(stats.owner_errors, 0);
	return TEST_PASS;
}

static atomic_uint compression_observed_stages;
static atomic_bool compression_callback_failed;

static void
compression_observe_callback(enum dssh_test_comp_stage stage)
{
	dssh_session sess = compression_callback_session;
	atomic_fetch_or(&compression_observed_stages, 1U << stage);
	if (sess == NULL) {
		atomic_store(&compression_callback_failed, true);
		return;
	}
	bool valid = dssh_io_callback_active(sess);
	mtx_t *mutexes[] = { &sess->trans.tx_mtx, &sess->trans.tx_queue_mtx };
	for (size_t i = 0; i < sizeof(mutexes) / sizeof(mutexes[0]); i++) {
		int locked = mtx_trylock(mutexes[i]);
		if (locked == thrd_success) {
			if (mtx_unlock(mutexes[i]) != thrd_success)
				valid = false;
		}
		else
			valid = false;
	}
	uint8_t packet = SSH_MSG_REQUEST_SUCCESS;
	valid = valid && send_packet(sess, &packet, 1, NULL) == DSSH_ERROR_INVALID;
	if (!valid)
		atomic_store(&compression_callback_failed, true);
}

static int
test_compression_callbacks(void)
{
	ASSERT_OK(dssh_test_comp_reset());
	atomic_store(&compression_observed_stages, 0);
	atomic_store(&compression_callback_failed, false);
	dssh_test_comp_observe(compression_observe_callback);
	struct handshake_ctx ctx;
	bool valid = compression_handshake(&ctx) == 0;
	uint8_t packet[] = { SSH_MSG_CHANNEL_DATA, 1, 2, 3 };
	if (valid)
		valid = compression_exchange(&ctx, true, packet, sizeof(packet))
		    && compression_exchange(&ctx, false, packet, sizeof(packet))
		    && compression_rekey_pair(&ctx);
	handshake_cleanup(&ctx);
	dssh_test_comp_observe(NULL);
	unsigned stages = (1U << DSSH_TEST_COMP_INIT) | (1U << DSSH_TEST_COMP_BOUND)
	    | (1U << DSSH_TEST_COMP_COMPRESS) | (1U << DSSH_TEST_COMP_UNCOMPRESS)
	    | (1U << DSSH_TEST_COMP_CLEANUP);
	ASSERT_TRUE(valid);
	ASSERT_FALSE(atomic_load(&compression_callback_failed));
	ASSERT_EQ_U(atomic_load(&compression_observed_stages), stages);
	return TEST_PASS;
}

static int
test_compression_scratch_failures(void)
{
	for (unsigned endpoint = 0; endpoint < 2; endpoint++) {
		for (unsigned receive = 0; receive < 2; receive++) {
			ASSERT_OK(dssh_test_comp_reset());
			struct handshake_ctx ctx;
			extra_algorithm_registration = dssh_test_register_growth_comp;
			int ret = handshake_setup_options(&ctx, false, "none");
			extra_algorithm_registration = NULL;
			if (ret < 0) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			dssh_session target = endpoint == 0 ? ctx.client : ctx.server;
			const char *method = "test-growth";
			bool valid = target->trans.comp_tx_packet == NULL && target->trans.comp_rx_input == NULL
			    && dssh_session_set_comp_filter(ctx.client, &method, 1) == 0
			    && dssh_session_set_comp_filter(ctx.server, &method, 1) == 0;
			size_t size = target->trans.packet_buf_sz;
			if (receive == 0)
				size += 4;
			dssh_test_malloc_fail_size(size);
			int client_ret = -999, server_ret = -999;
			if (valid)
				valid = !compression_rekey_results(&ctx, &client_ret, &server_ret, target);
			int target_ret = endpoint == 0 ? client_ret : server_ret;
			valid = valid && dssh_test_malloc_size_failed() && target_ret == DSSH_ERROR_ALLOC
			    && dssh_session_is_terminated(target) && atomic_load(&target->terminate_notified)
			    && (receive == 0 ? target->trans.comp_tx_packet == NULL : target->trans.comp_tx_packet != NULL)
			    && target->trans.comp_rx_input == NULL
			    && strcmp(target->trans.comp_c2s_active->name, "none") == 0
			    && strcmp(target->trans.comp_s2c_active->name, "none") == 0;
			dssh_test_alloc_reset();
			handshake_cleanup(&ctx);
			struct dssh_test_comp_stats stats;
			dssh_test_comp_stats(&stats);
			ASSERT_TRUE(valid);
			ASSERT_EQ_U(stats.live, 0);
			ASSERT_EQ_U(stats.allocations, stats.cleanups);
			ASSERT_EQ_U(stats.owner_errors, 0);
		}
	}
	/* Existing scratch must survive rekey without another matching malloc. */
	for (unsigned receive = 0; receive < 2; receive++) {
		ASSERT_OK(dssh_test_comp_reset());
		struct handshake_ctx ctx;
		if (compression_handshake(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		size_t size = ctx.client->trans.packet_buf_sz;
		if (receive == 0)
			size += 4;
		dssh_test_malloc_fail_size(size);
		bool valid = compression_rekey_pair(&ctx) && !dssh_test_malloc_size_failed();
		dssh_test_alloc_reset();
		handshake_cleanup(&ctx);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_compression_replay_cleanup(void)
{
	static const uint8_t packet[] = { SSH_MSG_CHANNEL_DATA, 0, 0, 0, 7, 0, 0, 0, 16,
	    'p', 'r', 'i', 'v', 'a', 't', 'e', '-', 'p', 'a', 'y', 'l', 'o', 'a', 'd', '!' };
	for (unsigned discard = 0; discard < 2; discard++) {
		ASSERT_OK(dssh_test_comp_reset());
		struct handshake_ctx ctx;
		if (compression_handshake(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		uint32_t sequences[3];
		bool valid = true;
		for (unsigned i = 0; i < 3 && valid; i++)
			valid = send_packet(ctx.client, packet, sizeof(packet), &sequences[i]) == 0;
		if (valid)
			valid = compression_rekey_pair(&ctx) && ctx.server->trans.rekey_queue_count == 3;
		struct dssh_test_comp_stats before, after;
		dssh_test_comp_stats(&before);
		bool cleared = false, observed = false;
		if (discard == 0) {
			for (unsigned i = 0; i < 3 && valid; i++) {
				struct dssh_rekey_msg *node = ctx.server->trans.rekey_queue_head;
				valid = node != NULL && node->len == sizeof(packet)
				    && memcmp(node->data, packet, sizeof(packet)) == 0;
				if (!valid)
					break;
				dssh_test_alloc_watch_free(node, sizeof(*node) + node->len);
				uint8_t type;
				uint8_t *received;
				size_t length;
				int ret = recv_packet(ctx.server, &type, &received, &length);
				observed = dssh_test_alloc_watched_free(&cleared);
				valid = ret == 0 && type == packet[0] && length == sizeof(packet)
				    && memcmp(received, packet, sizeof(packet)) == 0
				    && ctx.server->trans.last_rx_seq == sequences[i] && observed && cleared;
				dssh_test_alloc_reset();
			}
			dssh_test_comp_stats(&after);
			valid = valid && before.decompressions == after.decompressions
			    && ctx.server->trans.rekey_queue_count == 0 && ctx.server->trans.rekey_queue_bytes == 0;
		}
		else if (valid) {
			struct dssh_rekey_msg *node = ctx.server->trans.rekey_queue_head;
			dssh_test_alloc_watch_free(node, sizeof(*node) + node->len);
		}
		handshake_cleanup(&ctx);
		if (discard != 0) {
			/* Global-config reset does not clear the allocator's observation. */
			observed = dssh_test_alloc_watched_free(&cleared);
			valid = valid && observed && cleared;
		}
		dssh_test_alloc_reset();
		dssh_test_comp_stats(&after);
		ASSERT_TRUE(valid);
		ASSERT_EQ_U(after.live, 0);
		ASSERT_EQ_U(after.owner_errors, 0);
	}
	return TEST_PASS;
}

struct compression_queue_gate {
	dssh_session sess;
	atomic_bool entered;
	atomic_bool release;
	bool valid;
	unsigned calls;
};
static struct compression_queue_gate *compression_active_queue_gate;

/* Hold an actual KEX transmit callback while the controller queues reserved
 * control slots. Callback timing for inbound globals is tested separately. */
static void
compression_queue_gate_tx(dssh_session sess)
{
	struct compression_queue_gate *gate = compression_active_queue_gate;
	if (gate == NULL || sess != gate->sess || atomic_load(&gate->entered))
		return;
	gate->calls++;
	gate->valid = atomic_load(&sess->trans.rekey_in_progress)
	    && tx_owner_is_current(sess) && !dssh_test_mtx_ownership();
	atomic_store(&gate->entered, true);
	if (!dssh_test_wait_bool(&gate->release, true))
		gate->valid = false;
}

static int
compression_register_both_gather(void)
{
	int ret = dssh_transport_set_tx_gather(test_gather_cb);
	if (ret < 0)
		return ret;
	return compression_register_both();
}

static int
test_compression_queued_rekey(void)
{
	for (unsigned mode = 0; mode < 4; mode++) {
		ASSERT_OK(dssh_test_comp_reset());
		struct handshake_ctx ctx;
		int (*registration)(void) = mode < 2 ? compression_register_both : compression_register_both_gather;
		if (compression_handshake_with(&ctx, registration) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		const char *method = mode % 2 == 0 ? "test-growth" : "test-growth-alt";
		bool valid = dssh_session_set_comp_filter(ctx.client, &method, 1) == 0
		    && dssh_session_set_comp_filter(ctx.server, &method, 1) == 0;
		struct compression_queue_gate gate = { .sess = ctx.server };
		atomic_init(&gate.entered, false);
		atomic_init(&gate.release, false);
		void (*previous_observer)(dssh_session) = compression_tx_observer;
		compression_active_queue_gate = &gate;
		compression_tx_observer = compression_queue_gate_tx;
		struct compression_sender client = { .pair = &ctx, .sess = ctx.client, .do_rekey = true, .result = -999 };
		struct compression_sender server = { .pair = &ctx, .sess = ctx.server, .do_rekey = true, .result = -999,
		    .watched_mutex = mode % 2 == 0 ? &ctx.server->trans.tx_mtx : &ctx.server->trans.tx_queue_mtx };
		atomic_bool app_completed;
		atomic_init(&app_completed, false);
		uint8_t application[] = { SSH_MSG_CHANNEL_DATA, 11, 22, 33, 44 };
		struct compression_sender app = { .pair = &ctx, .sess = ctx.server,
		    .payload = application, .length = sizeof(application), .result = -999,
		    .completed = &app_completed };
		atomic_bool *waited = dssh_test_thrd_watch_condition(&ctx.server->trans.rekey_cnd);
		thrd_t ct, st, at;
		bool cc = false, sc = false, ac = false;
		if (valid)
			cc = thrd_create(&ct, compression_sender_thread, &client) == thrd_success;
		if (cc)
			sc = thrd_create(&st, compression_sender_thread, &server) == thrd_success;
		if (sc)
			valid = dssh_test_wait_bool(&gate.entered, true);
		else
			valid = false;
		if (valid)
			ac = thrd_create(&at, compression_sender_thread, &app) == thrd_success;
		valid = valid && ac;
		uint8_t channel_reply[17] = { SSH_MSG_CHANNEL_OPEN_FAILURE };
		channel_reply[4] = 7;
		channel_reply[8] = 3; /* RFC 4254: unknown channel type. */
		uint8_t global_reply = SSH_MSG_REQUEST_SUCCESS;
		if (valid)
			valid = dssh_test_wait_bool(waited, true) && !atomic_load(&app_completed)
			    && send_to_slot(ctx.server, &ctx.server->trans.global_reply_slot, &global_reply, 1) == 0
			    && atomic_load(&ctx.server->trans.global_reply_slot.ready)
			    && send_to_slot(ctx.server, &ctx.server->trans.open_fail_slot,
			        channel_reply, sizeof(channel_reply)) == 0
			    && atomic_load(&ctx.server->trans.open_fail_slot.ready);
		atomic_store(&gate.release, true);
		if (!valid) {
			mock_io_close_c2s(&ctx.io);
			mock_io_close_s2c(&ctx.io);
		}
		if (cc && thrd_join(ct, NULL) != thrd_success)
			abort();
		if (sc && thrd_join(st, NULL) != thrd_success)
			abort();
		if (ac && thrd_join(at, NULL) != thrd_success)
			abort();
		compression_tx_observer = previous_observer;
		compression_active_queue_gate = NULL;
		dssh_test_thrd_watch_condition(NULL);
		valid = valid && client.result == 0 && server.result == 0 && app.result == 0
		    && gate.valid && gate.calls == 1 && atomic_load(&app_completed)
		    && !atomic_load(&ctx.server->trans.global_reply_slot.ready)
		    && !atomic_load(&ctx.server->trans.open_fail_slot.ready);
		for (unsigned i = 0; i < 3 && valid; i++) {
			uint8_t type;
			uint8_t *received;
			size_t length;
			int ret = recv_packet(ctx.client, &type, &received, &length);
			if (i == 0)
				valid = ret == 0 && type == SSH_MSG_REQUEST_SUCCESS && length == 1;
			else if (i == 1)
				valid = ret == 0 && length == sizeof(channel_reply) && memcmp(received, channel_reply, length) == 0;
			else
				valid = ret == 0 && length == sizeof(application) && memcmp(received, application, length) == 0;
		}
		struct dssh_test_comp_stats stats;
		dssh_test_comp_stats(&stats);
		valid = valid && stats.owner_errors == 0 && stats.live == 4;
		handshake_cleanup(&ctx);
		dssh_test_comp_stats(&stats);
		ASSERT_TRUE(valid);
		ASSERT_EQ_U(stats.live, 0);
		ASSERT_EQ_U(stats.allocations, stats.cleanups);
		ASSERT_EQ_U(stats.owner_errors, 0);
	}
	return TEST_PASS;
}

static const char compression_auth_secret[] = "credential-buffer-value";

struct compression_auth_probe {
	dssh_session sess;
	const uint8_t *secret;
	size_t secret_len;
	unsigned calls;
	unsigned observed_writes;
	bool armed;
	bool clean;
	bool inputs_valid;
	bool accept;
	int result;
};
static struct compression_auth_probe *compression_auth_active;

static void
compression_auth_tx_watch(dssh_session sess)
{
	struct compression_auth_probe *probe = compression_auth_active;
	if (probe == NULL || probe->sess != sess || !probe->armed)
		return;
	probe->observed_writes++;
	probe->clean = probe->clean && compression_bytes_zero(probe->secret, probe->secret_len)
	    && compression_bytes_zero(sess->trans.comp_rx_input, sess->trans.packet_buf_sz);
}

static int
compression_auth_password_cb(const uint8_t *username, size_t username_len,
    const uint8_t *password, size_t password_len, struct dssh_bytes_result *output_result, void *opaque)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **prompt = &fixture_output;
	size_t *prompt_len = &fixture_length;
	(void)prompt;
	(void)prompt_len;

	(void)prompt;
	(void)prompt_len;
	struct compression_auth_probe *probe = opaque;
	probe->calls++;
	probe->inputs_valid = username_len == 5 && memcmp(username, "audit", 5) == 0
	    && password_len == sizeof(compression_auth_secret) - 1
	    && memcmp(password, compression_auth_secret, password_len) == 0;
	probe->secret = password;
	probe->secret_len = password_len;
	probe->armed = true;
	return test_auth_output_finish(output_result, fixture_output, fixture_length, probe->accept ? DSSH_AUTH_SUCCESS : DSSH_AUTH_FAILURE);
}

static int
compression_auth_server_thread(void *opaque)
{
	struct compression_auth_probe *probe = opaque;
	struct dssh_auth_server_cbs callbacks = { .methods_str = "password",
	    .password_cb = compression_auth_password_cb, .cbdata = probe };
	compression_callback_session = probe->sess;
	probe->result = dssh_auth_server(probe->sess, &callbacks, NULL, NULL);
	compression_callback_session = NULL;
	return 0;
}

static int
test_compression_auth_cleanup(void)
{
	for (unsigned provider = 0; provider < 2; provider++) {
		if (provider != 0 && !dssh_test_zlib_available())
			continue;
		for (unsigned accept = 0; accept < 2; accept++) {
			ASSERT_OK(dssh_test_comp_reset());
			struct handshake_ctx ctx;
			int (*registration)(void) = provider == 0 ? dssh_test_register_growth_comp : dssh_test_register_zlib_comp;
			if (compression_handshake_with(&ctx, registration) < 0) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			struct compression_auth_probe probe = { .sess = ctx.server, .clean = true, .accept = accept != 0 };
			compression_auth_active = &probe;
			compression_tx_observer = compression_auth_tx_watch;
			thrd_t server;
			int created = thrd_create(&server, compression_auth_server_thread, &probe);
			int result = -999;
			if (created == thrd_success) {
				result = dssh_auth_password(ctx.client, "audit", compression_auth_secret, NULL, NULL);
				/* Rejection keeps the server auth loop alive; EOF ends this test. */
				if (accept == 0 || result < 0)
					mock_io_close_c2s_write(&ctx.io);
				if (thrd_join(server, NULL) != thrd_success)
					abort();
			}
			compression_tx_observer = NULL;
			compression_auth_active = NULL;
			int expected = accept != 0 ? 0 : DSSH_AUTH_CONTINUE;
			/* RFC 4252 password request prefix for the exact username/method. */
			size_t request_len = 45 + sizeof(compression_auth_secret) - 1;
			bool valid = created == thrd_success && result == expected && probe.calls == 1
			    && probe.observed_writes > 0 && probe.clean && probe.inputs_valid
			    && (accept == 0 ? probe.result == DSSH_ERROR_TERMINATED : probe.result == 0)
			    && compression_bytes_zero(&ctx.client->trans.tx_packet[9], request_len)
			    && compression_bytes_zero(ctx.server->trans.rx_packet, 4 + ctx.server->trans.packet_buf_sz)
			    && compression_bytes_zero(ctx.client->trans.comp_tx_packet, 4 + ctx.client->trans.packet_buf_sz)
			    && compression_bytes_zero(ctx.server->trans.comp_rx_input, ctx.server->trans.packet_buf_sz);
			if (!valid)
				fprintf(stderr, "compressed auth check: provider=%u accept=%u client=%d server=%d calls=%u writes=%u clean=%d inputs=%d tx=%d rx=%d\n",
				    provider, accept, result, probe.result, probe.calls, probe.observed_writes,
				    probe.clean, probe.inputs_valid,
				    compression_bytes_zero(&ctx.client->trans.tx_packet[9], request_len),
				    compression_bytes_zero(ctx.server->trans.rx_packet, 4 + ctx.server->trans.packet_buf_sz));
			handshake_cleanup(&ctx);
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

struct compression_sensitive_probe {
	size_t length;
	unsigned calls;
	bool clean;
};

static void
compression_sensitive_terminated(dssh_session sess, void *opaque)
{
	struct compression_sensitive_probe *probe = opaque;
	probe->calls++;
	probe->clean = compression_bytes_zero(&sess->trans.tx_packet[9], probe->length)
	    && compression_bytes_zero(sess->trans.comp_tx_packet, 4 + sess->trans.packet_buf_sz);
}

static int
test_compression_sensitive_failure(void)
{
	const int errors[] = { DSSH_ERROR_INIT, DSSH_ERROR_TOOLONG, DSSH_ERROR_REKEY_NEEDED, 3, 0 };
	for (unsigned mode = 0; mode < 6; mode++) {
		ASSERT_OK(dssh_test_comp_reset());
		struct handshake_ctx ctx;
		if (compression_handshake(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		struct compression_sensitive_probe probe = { .length = sizeof(compression_auth_secret), .clean = false };
		bool valid = dssh_session_set_terminate_cb(ctx.client, compression_sensitive_terminated, &probe) == 0;
		struct control_tx_failure failure = { .sess = ctx.client, .fail_at = 0 };
		if (mode < 5)
			dssh_test_comp_fail(DSSH_TEST_COMP_COMPRESS, errors[mode]);
		else
			control_tx_failure = &failure;
		size_t capacity;
		int error;
		uint8_t *buf = send_begin(ctx.client, SSH_MSG_USERAUTH_REQUEST, &capacity, &error);
		int ret = -999;
		if (buf != NULL) {
			if (capacity >= probe.length) {
				memcpy(buf, compression_auth_secret, probe.length);
				buf[0] = SSH_MSG_USERAUTH_REQUEST;
				ret = send_commit_sensitive(ctx.client, probe.length, NULL);
			}
			else
				send_cancel(ctx.client);
		}
		control_tx_failure = NULL;
		int expected = mode < 5 ? (errors[mode] <= 0 ? errors[mode] : DSSH_ERROR_INVALID) : DSSH_ERROR_INIT;
		if (mode == 4)
			expected = DSSH_ERROR_INVALID;
		valid = valid && ret == expected && probe.calls == 1 && probe.clean
		    && dssh_session_is_terminated(ctx.client) && !tx_owner_is_current(ctx.client);
		handshake_cleanup(&ctx);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_compression_teardown_cleanup(void)
{
	for (unsigned target = 0; target < 4; target++) {
		ASSERT_OK(dssh_test_comp_reset());
		struct handshake_ctx ctx;
		if (compression_handshake(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		uint8_t *buffers[] = { ctx.client->trans.comp_tx_packet, ctx.client->trans.comp_rx_input,
		    ctx.client->trans.tx_packet, ctx.client->trans.rx_packet };
		size_t size = ctx.client->trans.packet_buf_sz;
		if (target != 1)
			size += 4;
		/* No operation holds a borrowed packet pointer here. Observe the whole
		 * owned allocation immediately before the real allocator releases it. */
		memset(buffers[target], 0xA5, size);
		dssh_test_alloc_watch_free(buffers[target], size);
		handshake_cleanup(&ctx);
		bool cleared = false;
		bool observed = dssh_test_alloc_watched_free(&cleared);
		dssh_test_alloc_reset();
		ASSERT_TRUE(observed && cleared);
	}
	return TEST_PASS;
}

static int
test_compression_context_replacement(void)
{
	for (unsigned fail = 0; fail < 2; fail++) {
		ASSERT_OK(dssh_test_comp_reset());
		struct handshake_ctx ctx;
		if (compression_handshake(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		uint8_t *tx_scratch = ctx.client->trans.comp_tx_packet;
		uint8_t *rx_scratch = ctx.client->trans.comp_rx_input;
		dssh_test_comp_replace_on_reset(true);
		if (fail != 0)
			dssh_test_comp_fail(DSSH_TEST_COMP_INIT, DSSH_ERROR_INIT);
		bool success = compression_rekey_pair(&ctx);
		bool valid = success == (fail == 0);
		struct dssh_test_comp_stats stats;
		dssh_test_comp_stats(&stats);
		if (fail == 0) {
			/* Counts and subsequent packet I/O verify replacement ownership;
			 * never inspect pointer values after their object lifetime ended. */
			valid = valid && tx_scratch == ctx.client->trans.comp_tx_packet && rx_scratch == ctx.client->trans.comp_rx_input
			    && stats.allocations == 8 && stats.cleanups == 4 && stats.live == 4 && stats.resets == 4;
			uint8_t payload[] = { SSH_MSG_CHANNEL_DATA, 1, 2, 3 };
			if (valid)
				valid = compression_exchange(&ctx, true, payload, sizeof(payload))
				    && compression_exchange(&ctx, false, payload, sizeof(payload));
		}
		else
			valid = valid && stats.allocations >= 5 && stats.resets >= 1;
		handshake_cleanup(&ctx);
		dssh_test_comp_replace_on_reset(false);
		dssh_test_comp_stats(&stats);
		ASSERT_TRUE(valid);
		ASSERT_EQ_U(stats.live, 0);
		ASSERT_EQ_U(stats.allocations, stats.cleanups);
		ASSERT_EQ_U(stats.owner_errors, 0);
	}
	return TEST_PASS;
}

#endif
