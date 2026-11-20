#ifndef DSSH_TEST_TRANSPORT_CLOCK_CASES_H
#define DSSH_TEST_TRANSPORT_CLOCK_CASES_H

static bool
clock_constructor_case(bool client, struct timespec point, bool refuse, bool success)
{
	struct test_session_pair pair;
	int ret = state_prepare(&pair, false);
	bool valid = ret == 0;
	if (valid) {
		dssh_test_clock_set(point);
		dssh_test_clock_fail_after(refuse ? 0 : INT_MAX);
		dssh_session sess = dssh_session_init(client, 0);
		if (client)
			pair.client = sess;
		else
			pair.server = sess;
		int calls = dssh_test_clock_count();
		dssh_test_clock_reset();
		valid = calls == 1 && (success ? sess != NULL : sess == NULL);
		if (success && sess != NULL)
			valid = sess->trans.rekey_time == point.tv_sec && !dssh_session_is_terminated(sess) && valid;
	}
	state_finish(&pair);
	return state_retired(&pair) && valid;
}

static int
test_clock_constructors(void)
{
	/* Produce a negative value only for a standard type that represents it.
	 * Unsigned or unrecognized time_t types retain zero and omit that case. */
	time_t before = _Generic((time_t)0,
	    char: CHAR_MIN < 0 ? -1 : 0, signed char: -1, short: -1, int: -1,
	    long: -1, long long: -1, float: -1.0F, double: -1.0, long double: -1.0L,
	    default: 0);
	long double before_wide = (long double)before;
	bool negative_case = before_wide < 0;
	struct timespec points[] = {{.tv_sec = 0}, {.tv_sec = 100, .tv_nsec = 999999999L},
	    {.tv_sec = 100}, {.tv_sec = 100, .tv_nsec = -1},
	    {.tv_sec = 100, .tv_nsec = 1000000000L}, {.tv_sec = before}};
	bool valid = true;
	unsigned checked = 0;
	unsigned successes = 0;
	unsigned refusals = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (size_t i = 0; i < sizeof(points) / sizeof(points[0]); i++) {
			if (i == 5 && !negative_case)
				continue;
			bool success = i < 2 || i == 5;
			valid = clock_constructor_case(role == 0, points[i], i == 2, success) && valid;
			checked++;
			if (success)
				successes++;
			else
				refusals++;
		}
	}
	int printed = printf("clock_constructors negative_case=%d roles=2 cases=%u success=%u refused=%u valid=%d\n",
	    negative_case, checked, successes, refusals, valid);
	ASSERT_TRUE(valid && printed > 0);
	return TEST_PASS;
}

static int
clock_pair_prepare(struct test_session_pair *pair)
{
	dssh_test_clock_reset();
	int ret = transport_pair_prepare(pair, 4);
	if (ret == 0)
		ret = dssh_session_set_rekey_seconds(pair->client, 0);
	if (ret == 0)
		ret = dssh_session_set_rekey_seconds(pair->server, 0);
	return ret;
}

struct clock_pair_result {
	struct transport_rekey_result owner;
	unsigned derive_refusals;
	unsigned queries;
	unsigned terminal_clocks;
	unsigned messages;
};

static bool
clock_key_refusal(dssh_session sess, dssh_session peer)
{
	dssh_enc_ctx *enc_c2s = sess->trans.enc_c2s_ctx;
	dssh_enc_ctx *enc_s2c = sess->trans.enc_s2c_ctx;
	dssh_mac_ctx *mac_c2s = sess->trans.mac_c2s_ctx;
	dssh_mac_ctx *mac_s2c = sess->trans.mac_s2c_ctx;
	dssh_enc enc_c2s_active = sess->trans.enc_c2s_active;
	dssh_enc enc_s2c_active = sess->trans.enc_s2c_active;
	dssh_mac mac_c2s_active = sess->trans.mac_c2s_active;
	dssh_mac mac_s2c_active = sess->trans.mac_s2c_active;
	const uint8_t *session_id = sess->trans.session_id;
	size_t session_id_size = sess->trans.session_id_sz;
	time_t key_time = sess->trans.rekey_time;
	uint32_t tx_seq = sess->trans.tx_seq;
	uint32_t rx_seq = sess->trans.rx_seq;
	uint_fast32_t tx_packets = atomic_load(&sess->trans.tx_since_rekey);
	uint32_t rx_packets = sess->trans.rx_since_rekey;
	uint_fast64_t tx_bytes = atomic_load(&sess->trans.tx_bytes_since_rekey);
	uint64_t rx_bytes = sess->trans.rx_bytes_since_rekey;
	if (enc_c2s == NULL || enc_s2c == NULL || mac_c2s == NULL || mac_s2c == NULL
	    || enc_c2s_active == NULL || enc_s2c_active == NULL || mac_c2s_active == NULL || mac_s2c_active == NULL
	    || session_id == NULL || session_id_size == 0 || peer->trans.session_id == NULL
	    || session_id == peer->trans.session_id || session_id_size != peer->trans.session_id_sz
	    || memcmp(session_id, peer->trans.session_id, session_id_size) != 0)
		return false;
	static const uint8_t encoded[] = {0, 0, 0, 1, 1};
	dssh_test_clock_fail_after(0);
	int ret = derive_and_apply_keys(sess, encoded, sizeof(encoded), enc_c2s_active,
	    enc_s2c_active, mac_c2s_active, mac_s2c_active);
	int calls = dssh_test_clock_count();
	dssh_test_clock_reset();
	return ret == DSSH_ERROR_INIT && calls == 1 && !dssh_session_is_terminated(sess)
	    && sess->trans.enc_c2s_ctx == enc_c2s && sess->trans.enc_s2c_ctx == enc_s2c
	    && sess->trans.mac_c2s_ctx == mac_c2s && sess->trans.mac_s2c_ctx == mac_s2c
	    && sess->trans.enc_c2s_active == enc_c2s_active && sess->trans.enc_s2c_active == enc_s2c_active
	    && sess->trans.mac_c2s_active == mac_c2s_active && sess->trans.mac_s2c_active == mac_s2c_active
	    && atomic_load(&sess->trans.enc_c2s_selected) == enc_c2s_active
	    && atomic_load(&sess->trans.enc_s2c_selected) == enc_s2c_active
	    && atomic_load(&sess->trans.mac_c2s_selected) == mac_c2s_active
	    && atomic_load(&sess->trans.mac_s2c_selected) == mac_s2c_active
	    && sess->trans.rekey_time == key_time && sess->trans.tx_seq == tx_seq && sess->trans.rx_seq == rx_seq
	    && atomic_load(&sess->trans.tx_since_rekey) == tx_packets && sess->trans.rx_since_rekey == rx_packets
	    && atomic_load(&sess->trans.tx_bytes_since_rekey) == tx_bytes && sess->trans.rx_bytes_since_rekey == rx_bytes
	    && sess->trans.session_id == session_id && sess->trans.session_id_sz == session_id_size
	    && memcmp(session_id, peer->trans.session_id, session_id_size) == 0;
}

static struct clock_pair_result
clock_pair_case(void)
{
	struct clock_pair_result result = {.owner = {.ret = DSSH_ERROR_INIT}};
	struct test_session_pair pair;
	result.owner.ret = clock_pair_prepare(&pair);
	if (result.owner.ret == 0)
		result.owner.ret = test_pair_run(&pair, dssh_transport_handshake);
	if (result.owner.ret != 0)
		goto done;
	result.owner.ret = DSSH_ERROR_INVALID;
	if (pair.created != 2 || pair.joined != 2 || pair.client_result != 0 || pair.server_result != 0
	    || !transport_pair_versions(&pair) || !transport_pair_algorithms(&pair)
	    || dssh_test_rekeys_completed(pair.client) != 0 || dssh_test_rekeys_completed(pair.server) != 0
	    || !transport_pair_exchange(pair.client, pair.server, false, SSH_MSG_SERVICE_REQUEST)
	    || !transport_pair_exchange(pair.server, pair.client, false, SSH_MSG_SERVICE_ACCEPT))
		goto done;
	result.messages = 2;
	result.owner.ready = true;
	dssh_session sessions[] = {pair.client, pair.server};
	for (size_t role = 0; role < sizeof(sessions) / sizeof(sessions[0]); role++) {
		dssh_session sess = sessions[role];
		dssh_session peer = sessions[1 - role];
		if (!clock_key_refusal(sess, peer))
			goto done;
		result.derive_refusals++;
		time_t key_time = sess->trans.rekey_time;
		if (dssh_session_set_rekey_seconds(sess, 10) != 0)
			goto done;
		sess->trans.rekey_time = 100;
		static const struct {
			time_t now;
			bool expected;
		} boundaries[] = {{109, false}, {110, true}, {90, false}};
		for (size_t i = 0; i < sizeof(boundaries) / sizeof(boundaries[0]); i++) {
			struct timespec now = {.tv_sec = boundaries[i].now};
			dssh_test_clock_set(now);
			dssh_test_clock_fail_after(INT_MAX);
			bool actual = rekey_needed(sess);
			int calls = dssh_test_clock_count();
			dssh_test_clock_reset();
			if (actual != boundaries[i].expected || calls != 1 || dssh_session_is_terminated(sess))
				goto done;
			result.queries++;
		}
		sess->trans.rekey_time = key_time;
		if (dssh_session_set_rekey_seconds(sess, 0) != 0)
			goto done;
	}
	/* Both directions must still process exact authenticated traffic with the
	 * retained cipher/MAC contexts before either local session is terminated. */
	if (!transport_pair_exchange(pair.client, pair.server, false, SSH_MSG_SERVICE_REQUEST)
	    || !transport_pair_exchange(pair.server, pair.client, false, SSH_MSG_SERVICE_ACCEPT))
		goto done;
	result.messages += 2;
	for (size_t role = 0; role < sizeof(sessions) / sizeof(sessions[0]); role++) {
		dssh_session sess = sessions[role];
		dssh_test_clock_fail_after(0);
		bool disabled = rekey_needed(sess);
		if (disabled || dssh_test_clock_count() != 0 || dssh_session_is_terminated(sess))
			goto done;
		result.queries++;
		if (dssh_session_set_rekey_seconds(sess, 10) != 0)
			goto done;
		bool failed = rekey_needed(sess);
		int calls = dssh_test_clock_count();
		dssh_test_clock_reset();
		if (failed || calls != 1 || !dssh_session_is_terminated(sess))
			goto done;
		result.queries++;
		result.terminal_clocks++;
	}
	result.owner.valid = result.derive_refusals == 2 && result.queries == 10
	    && result.terminal_clocks == 2 && result.messages == 4 && transport_pair_algorithms(&pair);
	if (result.owner.valid)
		result.owner.ret = 0;
done:
	dssh_test_clock_reset();
	transport_pair_finish(&pair);
	result.owner.created = pair.created;
	result.owner.joined = pair.joined;
	result.owner.retired = transport_pair_retired(&pair);
	result.owner.valid = result.owner.valid && result.owner.retired;
	return result;
}

static int
test_clock_transport(void)
{
	struct clock_pair_result result = clock_pair_case();
	int printed = printf("clock_pair ready=%d created=%u joined=%u derive=%u queries=%u terminal=%u messages=%u retired=%d valid=%d\n",
	    result.owner.ready, result.owner.created, result.owner.joined, result.derive_refusals, result.queries,
	    result.terminal_clocks, result.messages, result.owner.retired, result.owner.valid);
	ASSERT_TRUE(result.owner.ret == 0 && result.owner.valid && printed > 0);
	return TEST_PASS;
}

#ifdef DSSH_TRANSPORT_PAIR_CONTROLS
static int
test_clock_or_all_pair_ownership(void)
{
	const char *selected = getenv("DSSH_CLOCK_PAIR_OWNER");
	if (selected == NULL)
		return test_transport_all_pair_ownership();
	if (strcmp(selected, "1") != 0)
		return TEST_FAIL;
	struct clock_pair_result result = clock_pair_case();
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool valid = result.owner.retired;
	if (mode != NULL && strcmp(mode, "plain") == 0)
		valid = valid && result.owner.ready && result.owner.valid && result.owner.ret == 0;
	else if (mode != NULL && strcmp(mode, "tx") == 0)
		valid = valid && !result.owner.ready && result.owner.ret < 0 && result.owner.created == 2;
	else if (mode != NULL && (strcmp(mode, "create-0") == 0 || strcmp(mode, "create-1") == 0))
		valid = valid && !result.owner.ready && result.owner.ret == DSSH_ERROR_INIT;
	else
		valid = false;
	int printed = printf("fixture_owner=1 created=%u joined=%u valid=%d\n", result.owner.created, result.owner.joined, valid);
	ASSERT_TRUE(valid && printed > 0);
	return TEST_PASS;
}

static int
clock_pair_assertion_failure(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static int
clock_or_all_pair_assertion_control(unsigned phase)
{
	const char *selected = getenv("DSSH_CLOCK_PAIR_OWNER");
	if (selected == NULL)
		return transport_all_pair_assertion_control(phase);
	if (phase > 2 || strcmp(selected, "1") != 0)
		return 2;
	struct test_session_pair pair;
	int ret = clock_pair_prepare(&pair);
	pair.operation = dssh_transport_handshake;
	for (unsigned i = 0; ret == 0 && i < phase; i++)
		ret = test_pair_start(&pair, i);
	if (ret != 0) {
		transport_pair_finish(&pair);
		return 2;
	}
	ret = clock_pair_assertion_failure();
	bool valid = ret == TEST_FAIL && transport_pair_retired(&pair);
	int printed = printf("fixture_assertion=1 phase=%u created=%u joined=%u valid=%d\n",
	    phase, pair.created, pair.joined, valid);
	return valid && printed > 0 ? 1 : 2;
}
#endif

#endif
