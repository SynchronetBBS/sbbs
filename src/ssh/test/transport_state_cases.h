#ifndef DSSH_TEST_TRANSPORT_STATE_CASES_H
#define DSSH_TEST_TRANSPORT_STATE_CASES_H

#include <limits.h>

static atomic_bool state_io_called;

static int
state_io(uint8_t *buf, size_t length, dssh_session sess, void *data)
{
	(void)buf;
	(void)length;
	(void)sess;
	(void)data;
	atomic_store(&state_io_called, true);
	return DSSH_ERROR_INIT;
}

static int
state_other_io(uint8_t *buf, size_t length, dssh_session sess, void *data)
{
	(void)buf;
	(void)length;
	(void)sess;
	(void)data;
	atomic_store(&state_io_called, true);
	return DSSH_ERROR_INVALID;
}

static int
state_line(uint8_t *buf, size_t length, size_t *received, dssh_session sess, void *data)
{
	(void)buf;
	(void)length;
	(void)sess;
	(void)data;
	atomic_store(&state_io_called, true);
	if (received != NULL)
		*received = 0;
	return DSSH_ERROR_INIT;
}

static int
state_gather(const struct dssh_tx_iov *iov, size_t count, dssh_session sess, void *data)
{
	(void)iov;
	(void)count;
	(void)sess;
	(void)data;
	atomic_store(&state_io_called, true);
	return DSSH_ERROR_INIT;
}

static int
state_prepare(struct test_session_pair *pair, bool ciphers)
{
	test_pair_begin(pair);
	atomic_init(&state_io_called, false);
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	int ret = dssh_register_none_comp();
	if (ret == 0 && ciphers)
		ret = dssh_register_aes256_ctr();
	if (ret == 0 && ciphers)
		ret = dssh_register_aes128_cbc();
	if (ret == 0)
		ret = dssh_transport_set_callbacks(state_io, state_io, state_line, NULL);
	return ret;
}

static void
state_finish(struct test_session_pair *pair)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	transport_pair_finish(pair);
}

static bool
state_retired(struct test_session_pair *pair)
{
	return transport_pair_retired(pair) && !atomic_load(&state_io_called);
}

static int
state_prepare_algorithms(struct test_session_pair *pair)
{
	test_pair_begin(pair);
	atomic_init(&state_io_called, false);
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_test_reset_global_config();
	int ret = register_all_algorithms();
	if (ret == 0)
		ret = dssh_transport_set_callbacks(state_io, state_io, state_line, NULL);
	return ret;
}

static int
state_sessions(struct test_session_pair *pair, bool both)
{
	pair->client = dssh_session_init(true, 0);
	if (pair->client == NULL)
		return DSSH_ERROR_ALLOC;
	if (both) {
		pair->server = dssh_session_init(false, 0);
		if (pair->server == NULL)
			return DSSH_ERROR_ALLOC;
	}
	return 0;
}

static int
state_cipher_contexts(dssh_session sess, dssh_enc first, dssh_enc second)
{
	if (sess == NULL || first == NULL || second == NULL || first->init == NULL || second->init == NULL
	    || sess->trans.enc_c2s_ctx != NULL || sess->trans.enc_s2c_ctx != NULL)
		return DSSH_ERROR_INVALID;
	static const uint8_t key[32] = {0x42};
	static const uint8_t iv[16] = {0};
	sess->trans.enc_c2s_selected = first;
	sess->trans.enc_s2c_selected = second;
	int ret = first->init(key, iv, sess->trans.client, &sess->trans.enc_c2s_ctx);
	if (ret == 0 && sess->trans.enc_c2s_ctx == NULL)
		ret = DSSH_ERROR_INIT;
	if (ret == 0) {
		sess->trans.enc_c2s_active = first;
		ret = second->init(key, iv, !sess->trans.client, &sess->trans.enc_s2c_ctx);
	}
	if (ret == 0 && sess->trans.enc_s2c_ctx == NULL)
		ret = DSSH_ERROR_INIT;
	if (ret == 0)
		sess->trans.enc_s2c_active = second;
	return ret;
}

struct transport_state_result {
	int ret;
	int allocations;
	bool valid;
	bool retired;
};

static struct transport_state_result
state_block_case(int allocation_failure)
{
	struct transport_state_result result = {.ret = DSSH_ERROR_INIT};
	struct test_session_pair pair;
	result.ret = state_prepare(&pair, true);
	if (result.ret != 0)
		goto done;
	dssh_enc first = gconf.enc_head;
	if (first == NULL) {
		result.ret = DSSH_ERROR_INIT;
		goto done;
	}
	dssh_enc second = first->next;
	if (second == NULL || first->init == NULL || second->init == NULL
	    || strcmp(first->name, "aes256-ctr") != 0 || strcmp(second->name, "aes128-cbc") != 0) {
		result.ret = DSSH_ERROR_INIT;
		goto done;
	}
	dssh_test_alloc_fail_after(allocation_failure);
	result.ret = state_sessions(&pair, true);
	if (result.ret == 0 && (tx_block_size(pair.client) != 8 || rx_block_size(pair.client) != 8
	    || tx_block_size(pair.server) != 8 || rx_block_size(pair.server) != 8))
		result.ret = DSSH_ERROR_INVALID;
	if (result.ret == 0)
		result.ret = state_cipher_contexts(pair.client, first, second);
	if (result.ret == 0)
		result.ret = state_cipher_contexts(pair.server, first, second);
	result.allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	if (result.ret != 0)
		goto done;
	uint16_t saved_first = first->blocksize;
	uint16_t saved_second = second->blocksize;
	static const struct {
		uint16_t first;
		uint16_t second;
		size_t first_expected;
		size_t second_expected;
	} cases[] = {{0, 32, 8, 32}, {1, 16, 8, 16}, {4, 8, 8, 8}, {7, 1, 8, 8},
	    {8, 4, 8, 8}, {16, 7, 16, 8}, {32, 0, 32, 8}, {UINT16_MAX, 16, UINT16_MAX, 16}};
	result.valid = true;
	dssh_session sessions[] = {pair.client, pair.server};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		first->blocksize = cases[i].first;
		second->blocksize = cases[i].second;
		for (unsigned active = 0; active < 2; active++) {
			for (size_t role = 0; role < sizeof(sessions) / sizeof(sessions[0]); role++) {
				dssh_session sess = sessions[role];
				/* Real contexts remain with their producers. An active module
				 * takes precedence over the next selected candidate, as at rekey. */
				sess->trans.enc_c2s_active = active ? first : NULL;
				sess->trans.enc_s2c_active = active ? second : NULL;
				sess->trans.enc_c2s_selected = active ? second : first;
				sess->trans.enc_s2c_selected = active ? first : second;
				size_t tx = sess->trans.client ? cases[i].first_expected : cases[i].second_expected;
				size_t rx = sess->trans.client ? cases[i].second_expected : cases[i].first_expected;
				result.valid = tx_block_size(sess) == tx && rx_block_size(sess) == rx && result.valid;
			}
		}
	}
	first->blocksize = saved_first;
	second->blocksize = saved_second;
	for (size_t role = 0; role < sizeof(sessions) / sizeof(sessions[0]); role++) {
		sessions[role]->trans.enc_c2s_active = first;
		sessions[role]->trans.enc_s2c_active = second;
		sessions[role]->trans.enc_c2s_selected = first;
		sessions[role]->trans.enc_s2c_selected = second;
	}
done:
	state_finish(&pair);
	result.retired = state_retired(&pair);
	result.valid = result.valid && result.retired;
	return result;
}

static int
test_blocksize_clamp_direct(void)
{
	struct transport_state_result result = state_block_case(-1);
	int printed = printf("typed_blocks roles=2 paths=2 pairs=8 queries=64 baseline=4 retired=%d valid=%d\n",
	    result.retired, result.valid);
	ASSERT_TRUE(result.ret == 0 && result.valid && printed > 0);
	return TEST_PASS;
}

static int
test_state_context_refusals(void)
{
	struct transport_state_result baseline = state_block_case(INT_MAX);
	bool valid = baseline.ret == 0 && baseline.valid && baseline.allocations > 0 && baseline.allocations < INT_MAX;
	int refused = 0;
	int tolerated = 0;
	for (int i = 0; valid && i < baseline.allocations; i++) {
		struct transport_state_result result = state_block_case(i);
		valid = result.retired && result.allocations >= i + 1;
		if (result.ret < 0 && valid)
			refused++;
		else if (result.ret == 0 && result.valid && valid)
			tolerated++;
		else
			valid = false;
	}
	struct transport_state_result final = state_block_case(baseline.allocations);
	valid = valid && final.ret == 0 && final.valid && final.allocations == baseline.allocations;
	int printed = printf("typed_contexts allocations=%d refused=%d tolerated=%d final=%d valid=%d\n",
	    baseline.allocations, refused, tolerated, final.allocations, valid);
	ASSERT_TRUE(valid && printed > 0);
	return TEST_PASS;
}

static bool
state_late_case(bool gather)
{
	bool valid = true;
	for (unsigned refused = 0; refused < 2; refused++) {
		struct test_session_pair pair;
		int ret = state_prepare(&pair, false);
		if (ret == 0)
			ret = dssh_transport_set_tx_gather(state_gather);
		if (ret == 0) {
			if (refused != 0)
				dssh_test_alloc_fail_after(0);
			pair.client = dssh_session_init(true, 0);
			int allocations = dssh_test_alloc_count();
			dssh_test_alloc_reset();
			valid = valid && (refused ? pair.client == NULL && allocations == 1 : pair.client != NULL)
			    && atomic_load(&gconf.used);
			for (unsigned disposed = 0; disposed < 2; disposed++) {
				if (disposed) {
					dssh_session sess = pair.client;
					pair.client = NULL;
					dssh_session_cleanup(sess);
				}
				ret = gather ? dssh_transport_set_tx_gather(NULL)
				    : dssh_transport_set_callbacks(state_other_io, state_other_io, NULL, NULL);
				valid = valid && ret == DSSH_ERROR_TOOLATE && gconf.tx_gather == state_gather
				    && gconf.tx == state_io && gconf.rx == state_io && gconf.rx_line == state_line;
			}
		}
		else
			valid = false;
		state_finish(&pair);
		valid = state_retired(&pair) && valid;
	}
	return valid;
}

static int
test_set_callbacks_after_session(void)
{
	ASSERT_TRUE(state_late_case(false));
	return TEST_PASS;
}

static int
test_tx_gather_toolate(void)
{
	ASSERT_TRUE(state_late_case(true));
	return TEST_PASS;
}

static int
test_tx_gather_set(void)
{
	struct test_session_pair pair;
	int ret = state_prepare(&pair, false);
	bool valid = ret == 0;
	if (valid) {
		ret = dssh_transport_set_tx_gather(NULL);
		valid = ret == 0 && gconf.tx_gather == NULL;
		ret = dssh_transport_set_tx_gather(state_gather);
		valid = ret == 0 && gconf.tx_gather == state_gather && valid;
		ret = dssh_transport_set_tx_gather(NULL);
		valid = ret == 0 && gconf.tx_gather == NULL && valid;
	}
	state_finish(&pair);
	valid = state_retired(&pair) && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct state_global_probe {
	unsigned calls;
	bool valid;
};

static int
state_global_request(const uint8_t *name, size_t name_length, bool reply,
    const uint8_t *data, size_t length, void *opaque)
{
	struct state_global_probe *probe = opaque;
	if (probe == NULL)
		return DSSH_ERROR_INVALID;
	probe->valid = probe->calls == 0 && name != NULL && name_length == 5 && reply
	    && memcmp(name, "probe", 5) == 0 && data != NULL && length == 1 && data[0] == 7;
	probe->calls = 1;
	return 17;
}

static int
test_set_global_request_cb(void)
{
	struct test_session_pair pair;
	int ret = state_prepare(&pair, false);
	if (ret == 0)
		ret = state_sessions(&pair, false);
	struct state_global_probe probe = {0};
	bool valid = ret == 0;
	if (valid) {
		ret = dssh_session_set_global_request_cb(pair.client, state_global_request, &probe);
		valid = ret == 0 && pair.client->global_request_cb == state_global_request
		    && pair.client->global_request_cbdata == &probe;
		if (valid) {
			static const uint8_t name[] = "probe";
			static const uint8_t data[] = {7};
			ret = pair.client->global_request_cb(name, 5, true, data, sizeof(data), pair.client->global_request_cbdata);
			valid = ret == 17 && probe.valid && probe.calls == 1;
		}
		ret = dssh_session_set_global_request_cb(pair.client, NULL, NULL);
		valid = ret == 0 && pair.client->global_request_cb == NULL && pair.client->global_request_cbdata == NULL && valid;
		ret = dssh_session_set_global_request_cb(NULL, state_global_request, &probe);
		valid = ret == DSSH_ERROR_INVALID && valid;
		/* Leave live caller-owned data installed through session disposal. */
		ret = dssh_session_set_global_request_cb(pair.client, state_global_request, &probe);
		valid = ret == 0 && valid;
	}
	state_finish(&pair);
	valid = state_retired(&pair) && probe.calls == 1 && probe.valid && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_get_names_before_handshake(void)
{
	struct test_session_pair pair;
	int ret = state_prepare_algorithms(&pair);
	if (ret == 0)
		ret = state_sessions(&pair, true);
	bool valid = ret == 0 && gconf.kex_head != NULL && gconf.key_algo_head != NULL
	    && gconf.enc_head != NULL && gconf.mac_head != NULL && gconf.comp_head != NULL;
	dssh_session sessions[] = {pair.client, pair.server};
	enum dssh_direction directions[] = {DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT};
	for (size_t role = 0; valid && role < sizeof(sessions) / sizeof(sessions[0]); role++) {
		valid = dssh_transport_get_kex_name(sessions[role]) == NULL
		    && dssh_transport_get_hostkey_name(sessions[role]) == NULL;
		for (size_t i = 0; i < sizeof(directions) / sizeof(directions[0]); i++) {
			valid = dssh_transport_get_enc_name(sessions[role], directions[i]) == NULL
			    && dssh_transport_get_mac_name(sessions[role], directions[i]) == NULL
			    && dssh_transport_get_comp_name(sessions[role], directions[i]) == NULL && valid;
		}
	}
	state_finish(&pair);
	valid = state_retired(&pair) && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_aes256_ctr_null_ctx(void)
{
	struct test_session_pair pair;
	int ret = state_prepare(&pair, true);
	uint8_t data[16], original[16];
	memset(data, 0x5a, sizeof(data));
	memcpy(original, data, sizeof(original));
	bool valid = ret == 0 && gconf.enc_head != NULL && gconf.enc_head->encrypt != NULL
	    && gconf.enc_head->decrypt != NULL && strcmp(gconf.enc_head->name, "aes256-ctr") == 0;
	if (valid) {
		dssh_enc enc = gconf.enc_head;
		int encrypt = enc->encrypt(data, sizeof(data), NULL);
		int decrypt = enc->decrypt(data, sizeof(data), NULL);
		valid = encrypt == DSSH_ERROR_INIT && decrypt == DSSH_ERROR_INIT
		    && memcmp(data, original, sizeof(data)) == 0;
	}
	state_finish(&pair);
	valid = state_retired(&pair) && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#ifdef DSSH_CRYPTO_OPENSSL
static int
test_aes256_ctr_encrypt_update_failure(void)
{
	struct test_session_pair pair;
	int ret = state_prepare(&pair, true);
	if (ret == 0)
		ret = state_sessions(&pair, false);
	if (ret == 0)
		ret = state_cipher_contexts(pair.client, gconf.enc_head, gconf.enc_head->next);
	bool valid = ret == 0;
	if (valid) {
		uint8_t data[16] = {0};
		dssh_test_ossl_fail_after(0);
		ret = gconf.enc_head->encrypt(data, sizeof(data), pair.client->trans.enc_c2s_ctx);
		int calls = dssh_test_ossl_count();
		dssh_test_ossl_reset();
		valid = ret == DSSH_ERROR_INIT && calls == 1;
	}
	state_finish(&pair);
	valid = state_retired(&pair) && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
#endif

static struct transport_state_result
state_packet_case(bool client, size_t requested, size_t expected, int allocation_failure)
{
	struct transport_state_result result = {.ret = DSSH_ERROR_INIT};
	struct test_session_pair pair;
	result.ret = state_prepare(&pair, false);
	if (result.ret != 0)
		goto done;
	if (expected < 33280 || expected > SIZE_MAX - 4) {
		result.ret = DSSH_ERROR_INVALID;
		goto done;
	}
	dssh_test_alloc_fail_after(allocation_failure);
	dssh_session sess = dssh_session_init(client, requested);
	if (client)
		pair.client = sess;
	else
		pair.server = sess;
	result.allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	if (sess == NULL) {
		result.ret = DSSH_ERROR_ALLOC;
		goto done;
	}
	result.ret = 0;
	result.valid = sess->trans.packet_buf_sz == expected && sess->trans.client == client
	    && sess->trans.tx_packet != NULL && sess->trans.rx_packet != NULL
	    && sess->trans.tx_packet != sess->trans.rx_packet;
	if (result.valid) {
		/* Touch the four-byte prefix and both ends of the exact payload
		 * extent. ASan checks the actual allocation, not just its size field. */
		size_t end = 4 + expected - 1;
		static const size_t starts[] = {0, 3, 4};
		for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); i++) {
			sess->trans.tx_packet[starts[i]] = 0xb6;
			sess->trans.rx_packet[starts[i]] = 0x4d;
		}
		sess->trans.tx_packet[end] = 0xa7;
		sess->trans.rx_packet[end] = 0x3e;
		for (size_t i = 0; i < sizeof(starts) / sizeof(starts[0]); i++)
			result.valid = sess->trans.tx_packet[starts[i]] == 0xb6
			    && sess->trans.rx_packet[starts[i]] == 0x4d && result.valid;
		result.valid = sess->trans.tx_packet[end] == 0xa7 && sess->trans.rx_packet[end] == 0x3e && result.valid;
	}
done:
	state_finish(&pair);
	result.retired = state_retired(&pair);
	result.valid = result.valid && result.retired;
	return result;
}

static int
test_init_small_packet_size(void)
{
	static const struct {
		size_t request;
		size_t expected;
	} cases[] = {{0, 33280}, {1, 33280}, {100, 33280}, {33279, 33280},
	    {33280, 33280}, {33281, 33281}};
	bool valid = true;
	bool retired = true;
	int refused = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			struct transport_state_result result = state_packet_case(role == 0, cases[i].request, cases[i].expected, -1);
			valid = result.ret == 0 && result.valid && valid;
			retired = result.retired && retired;
		}
		struct transport_state_result baseline = state_packet_case(role == 0, 100, 33280, INT_MAX);
		valid = baseline.ret == 0 && baseline.valid && baseline.allocations == 3 && valid;
		retired = baseline.retired && retired;
		for (int i = 0; valid && i < baseline.allocations; i++) {
			struct transport_state_result result = state_packet_case(role == 0, 100, 33280, i);
			valid = result.ret == DSSH_ERROR_ALLOC && result.allocations == i + 1 && result.retired;
			retired = result.retired && retired;
			if (valid)
				refused++;
		}
		struct transport_state_result final = state_packet_case(role == 0, 100, 33280, baseline.allocations);
		valid = final.ret == 0 && final.valid && final.allocations == baseline.allocations && valid;
		retired = final.retired && retired;
	}
	int printed = printf("packet_small roles=2 cases=6 allocations=3 refused=%d retired=%d valid=%d\n", refused, retired, valid);
	ASSERT_TRUE(valid && refused == 6 && retired && printed > 0);
	return TEST_PASS;
}

static int
test_init_large_packet_size(void)
{
	/* All constants and additions are representable even with 16-bit size_t.
	 * The native representable cap reserves the packet's four-byte prefix. */
#if SIZE_MAX < UINT32_C(67108864)
	size_t maximum = SIZE_MAX - 4;
#else
	size_t maximum = UINT32_C(67108864);
#endif
	size_t requests[] = {maximum - 1, maximum, maximum + 1, SIZE_MAX};
	bool valid = true;
	bool retired = true;
	for (unsigned role = 0; role < 2; role++) {
		for (size_t i = 0; i < sizeof(requests) / sizeof(requests[0]); i++) {
			size_t expected = i == 0 ? maximum - 1 : maximum;
			struct transport_state_result result = state_packet_case(role == 0, requests[i], expected, -1);
			valid = result.ret == 0 && result.valid && valid;
			retired = result.retired && retired;
		}
	}
	int printed = printf("packet_large roles=2 cases=4 maximum=%zu retired=%d valid=%d\n", maximum, retired, valid);
	ASSERT_TRUE(valid && retired && printed > 0);
	return TEST_PASS;
}

enum state_policy_kind {
	STATE_POLICY_INITIAL,
	STATE_POLICY_TX_PACKETS,
	STATE_POLICY_RX_PACKETS,
	STATE_POLICY_BYTES,
	STATE_POLICY_TIME,
	STATE_POLICY_DISABLED,
	STATE_POLICY_BELOW,
	STATE_POLICY_DIRECTIONS,
	STATE_POLICY_SERVER,
	STATE_POLICY_ZERO,
	STATE_POLICY_COUNT
};

static bool
state_policy_case(enum state_policy_kind kind)
{
	static const struct {
		enum state_policy_kind kind;
		uint64_t tx_bytes;
		uint64_t rx_bytes;
		uint_fast32_t tx_packets;
		uint32_t rx_packets;
		uint32_t seconds;
		time_t start;
		time_t now;
		long nanos;
		bool expected[2];
		int clock_calls;
	} cases[] = {
		{.kind = STATE_POLICY_INITIAL},
		{.kind = STATE_POLICY_TX_PACKETS, .tx_packets = DSSH_REKEY_SOFT_LIMIT - 1},
		{.kind = STATE_POLICY_TX_PACKETS, .tx_packets = DSSH_REKEY_SOFT_LIMIT, .expected = {true, true}},
		{.kind = STATE_POLICY_TX_PACKETS, .tx_packets = DSSH_REKEY_HARD_LIMIT, .expected = {true, true}},
		{.kind = STATE_POLICY_TX_PACKETS, .tx_packets = UINT_FAST32_MAX, .expected = {true, true}},
		{.kind = STATE_POLICY_RX_PACKETS, .rx_packets = DSSH_REKEY_SOFT_LIMIT - 1},
		{.kind = STATE_POLICY_RX_PACKETS, .rx_packets = DSSH_REKEY_SOFT_LIMIT, .expected = {true, true}},
		{.kind = STATE_POLICY_RX_PACKETS, .rx_packets = DSSH_REKEY_HARD_LIMIT, .expected = {true, true}},
		{.kind = STATE_POLICY_RX_PACKETS, .rx_packets = UINT32_MAX, .expected = {true, true}},
		{.kind = STATE_POLICY_BYTES, .tx_bytes = 1023},
		{.kind = STATE_POLICY_BYTES, .tx_bytes = 1024, .expected = {true, false}},
		{.kind = STATE_POLICY_BYTES, .tx_bytes = 8191, .expected = {true, false}},
		{.kind = STATE_POLICY_BYTES, .tx_bytes = 8192, .expected = {true, true}},
		{.kind = STATE_POLICY_BYTES, .rx_bytes = 1023},
		{.kind = STATE_POLICY_BYTES, .rx_bytes = 1024, .expected = {false, true}},
		{.kind = STATE_POLICY_BYTES, .rx_bytes = 8191, .expected = {false, true}},
		{.kind = STATE_POLICY_BYTES, .rx_bytes = 8192, .expected = {true, true}},
		{.kind = STATE_POLICY_TIME, .seconds = 10, .start = 100, .now = 109, .clock_calls = 1},
		{.kind = STATE_POLICY_TIME, .seconds = 10, .start = 100, .now = 110, .expected = {true, true}, .clock_calls = 1},
		{.kind = STATE_POLICY_TIME, .seconds = 10, .start = 100, .now = 111, .expected = {true, true}, .clock_calls = 1},
		{.kind = STATE_POLICY_TIME, .seconds = 10, .start = 100, .now = 90, .clock_calls = 1},
		{.kind = STATE_POLICY_TIME, .seconds = 10, .start = 100, .now = 109, .nanos = 999999999L, .clock_calls = 1},
		{.kind = STATE_POLICY_TIME, .seconds = UINT32_MAX, .start = 100, .now = 111, .clock_calls = 1},
		{.kind = STATE_POLICY_DISABLED, .start = 0, .now = 2000},
		{.kind = STATE_POLICY_DISABLED, .seconds = 60, .now = 2000, .expected = {true, true}, .clock_calls = 1},
		{.kind = STATE_POLICY_DISABLED, .seconds = 3600, .start = 100, .now = 110, .clock_calls = 1},
		{.kind = STATE_POLICY_DISABLED, .tx_bytes = 1024, .expected = {true, false}},
		{.kind = STATE_POLICY_DISABLED, .rx_bytes = 1024, .expected = {false, true}},
		{.kind = STATE_POLICY_BELOW, .tx_bytes = 1023, .rx_bytes = 1023,
		    .tx_packets = DSSH_REKEY_SOFT_LIMIT - 1, .rx_packets = DSSH_REKEY_SOFT_LIMIT - 1},
		{.kind = STATE_POLICY_BELOW, .tx_bytes = 1023, .rx_bytes = 1023,
		    .tx_packets = DSSH_REKEY_SOFT_LIMIT - 1, .rx_packets = DSSH_REKEY_SOFT_LIMIT - 1,
		    .seconds = 10, .start = 100, .now = 109, .clock_calls = 1},
		/* Both counters stay below their individual limits, even though
		 * their sum exceeds the smaller directional limit. */
		{.kind = STATE_POLICY_DIRECTIONS, .tx_bytes = 513, .rx_bytes = 513},
		{.kind = STATE_POLICY_DIRECTIONS, .tx_bytes = 4097, .rx_bytes = 513, .expected = {true, false}},
		{.kind = STATE_POLICY_DIRECTIONS, .tx_bytes = 513, .rx_bytes = 4097, .expected = {false, true}},
		{.kind = STATE_POLICY_DIRECTIONS, .tx_bytes = 8192, .expected = {true, true}},
		{.kind = STATE_POLICY_DIRECTIONS, .rx_bytes = 8192, .expected = {true, true}},
		{.kind = STATE_POLICY_SERVER, .tx_bytes = 1024, .expected = {true, false}},
		{.kind = STATE_POLICY_SERVER, .rx_bytes = 1024, .expected = {false, true}},
		{.kind = STATE_POLICY_SERVER, .rx_bytes = 8192, .expected = {true, true}},
		{.kind = STATE_POLICY_ZERO, .seconds = 1, .clock_calls = 1},
		{.kind = STATE_POLICY_ZERO, .seconds = 1, .now = 1, .expected = {true, true}, .clock_calls = 1},
		{.kind = STATE_POLICY_ZERO, .seconds = 1, .now = 2, .expected = {true, true}, .clock_calls = 1},
	};
	if (kind < STATE_POLICY_INITIAL || kind >= STATE_POLICY_COUNT)
		return false;
	struct test_session_pair pair;
	dssh_enc first = NULL;
	dssh_enc second = NULL;
	uint64_t saved_first = 0;
	uint64_t saved_second = 0;
	bool changed = false;
	bool valid = false;
	unsigned checked = 0;
	int ret = state_prepare(&pair, true);
	if (ret != 0)
		goto done;
	struct timespec initial = {.tv_sec = kind == STATE_POLICY_ZERO ? 0 : 100};
	dssh_test_clock_set(initial);
	ret = state_sessions(&pair, true);
	first = gconf.enc_head;
	second = first == NULL ? NULL : first->next;
	if (ret == 0)
		ret = state_cipher_contexts(pair.client, first, second);
	if (ret == 0)
		ret = state_cipher_contexts(pair.server, first, second);
	if (ret != 0)
		goto done;
	saved_first = first->bytes_per_key;
	saved_second = second->bytes_per_key;
	first->bytes_per_key = 1024;
	second->bytes_per_key = 8192;
	changed = true;
	valid = strcmp(first->name, "aes256-ctr") == 0 && strcmp(second->name, "aes128-cbc") == 0;
	dssh_session sessions[] = {pair.client, pair.server};
	for (size_t role = 0; role < sizeof(sessions) / sizeof(sessions[0]); role++) {
		dssh_session sess = sessions[role];
		valid = sess->trans.rekey_time == initial.tv_sec && atomic_load(&sess->trans.rekey_seconds) == 0
		    && atomic_load(&sess->trans.tx_since_rekey) == 0 && sess->trans.rx_since_rekey == 0
		    && atomic_load(&sess->trans.tx_bytes_since_rekey) == 0 && sess->trans.rx_bytes_since_rekey == 0 && valid;
		for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
			if (cases[i].kind != kind)
				continue;
			atomic_store(&sess->trans.tx_since_rekey, cases[i].tx_packets);
			sess->trans.rx_since_rekey = cases[i].rx_packets;
			atomic_store(&sess->trans.tx_bytes_since_rekey, cases[i].tx_bytes);
			sess->trans.rx_bytes_since_rekey = cases[i].rx_bytes;
			sess->trans.rekey_time = cases[i].start;
			ret = dssh_session_set_rekey_seconds(sess, cases[i].seconds);
			valid = ret == 0 && atomic_load(&sess->trans.rekey_seconds) == cases[i].seconds && valid;
			struct timespec now = {.tv_sec = cases[i].now, .tv_nsec = cases[i].nanos};
			dssh_test_clock_reset();
			dssh_test_clock_set(now);
			dssh_test_clock_fail_after(INT_MAX);
			bool actual = rekey_needed(sess);
			int clock_calls = dssh_test_clock_count();
			dssh_test_clock_reset();
			valid = actual == cases[i].expected[role] && clock_calls == cases[i].clock_calls
			    && !dssh_session_is_terminated(sess) && valid;
			checked++;
		}
	}
	valid = checked > 0 && valid;
done:
	if (changed) {
		first->bytes_per_key = saved_first;
		second->bytes_per_key = saved_second;
	}
	dssh_test_clock_reset();
	state_finish(&pair);
	bool retired = state_retired(&pair);
	int kind_number = (int)kind;
	int printed = printf("rekey_policy kind=%d roles=2 checked=%u retired=%d valid=%d\n", kind_number, checked, retired, valid);
	return valid && retired && printed > 0;
}

static int
test_rekey_needed_false_initially(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_INITIAL));
	return TEST_PASS;
}

static int
test_rekey_needed_tx_packets(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_TX_PACKETS));
	return TEST_PASS;
}

static int
test_rekey_needed_rx_packets(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_RX_PACKETS));
	return TEST_PASS;
}

static int
test_rekey_needed_bytes(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_BYTES));
	return TEST_PASS;
}

static int
test_rekey_needed_time(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_TIME));
	return TEST_PASS;
}

static int
test_rekey_seconds_disabled(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_DISABLED));
	return TEST_PASS;
}

static int
test_rekey_needed_below_threshold(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_BELOW));
	return TEST_PASS;
}

static int
test_rekey_bytes_per_direction(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_DIRECTIONS));
	return TEST_PASS;
}

static int
test_rekey_server_direction_limits(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_SERVER));
	return TEST_PASS;
}

static int
test_rekey_time_zero(void)
{
	ASSERT_TRUE(state_policy_case(STATE_POLICY_ZERO));
	return TEST_PASS;
}

#endif
