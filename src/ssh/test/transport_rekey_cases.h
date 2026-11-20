#ifndef DSSH_TEST_TRANSPORT_REKEY_CASES_H
#define DSSH_TEST_TRANSPORT_REKEY_CASES_H

static const uint8_t transport_rekey_marker[] = {SSH_MSG_SERVICE_REQUEST, 0, 0, 0, 12,
	's', 's', 'h', '-', 'u', 's', 'e', 'r', 'a', 'u', 't', 'h'};

static int
transport_rekey_operation(dssh_session sess)
{
	if (sess->trans.client) {
		int ret = rekey(sess);
		if (ret == 0)
			ret = send_packet(sess, transport_rekey_marker, sizeof(transport_rekey_marker), NULL);
		return ret;
	}
	uint8_t type = 0;
	uint8_t *payload = NULL;
	size_t length = 0;
	int ret = recv_packet(sess, &type, &payload, &length);
	if (ret != 0)
		return ret;
	if (type != SSH_MSG_SERVICE_REQUEST || length != sizeof(transport_rekey_marker)
	    || memcmp(payload, transport_rekey_marker, sizeof(transport_rekey_marker)) != 0)
		return DSSH_ERROR_PARSE;
	return 0;
}

static int
transport_rekey_open(struct test_session_pair *pair)
{
	int ret = transport_pair_prepare(pair, 4);
	if (ret == 0)
		ret = dssh_session_set_rekey_seconds(pair->client, 0);
	if (ret == 0)
		ret = dssh_session_set_rekey_seconds(pair->server, 0);
	if (ret == 0)
		ret = test_pair_run(pair, dssh_transport_handshake);
	if (ret != 0)
		return ret;
	if (pair->created != 2 || pair->joined != 2 || !transport_pair_versions(pair)
	    || !transport_pair_algorithms(pair) || dssh_test_rekeys_completed(pair->client) != 0
	    || dssh_test_rekeys_completed(pair->server) != 0)
		return DSSH_ERROR_INVALID;
	/* Drain initial extension traffic before taking exact sequence snapshots. */
	if (!transport_pair_exchange(pair->client, pair->server, false, SSH_MSG_SERVICE_REQUEST)
	    || !transport_pair_exchange(pair->server, pair->client, false, SSH_MSG_SERVICE_ACCEPT))
		return DSSH_ERROR_INVALID;
	return 0;
}

static bool
transport_rekey_sequence(uint32_t before, uint32_t after, uint32_t packets)
{
	/* Two uint32_t values sum to at most 2^33-2, which fits uint64_t.
	 * Compare values modulo2^32 without object-representation assumptions. */
	uint64_t expected = before;
	expected += packets;
	expected &= UINT32_MAX;
	return after == expected;
}

struct transport_rekey_result {
	int ret;
	unsigned created;
	unsigned joined;
	bool ready;
	bool valid;
	bool retired;
};

static struct transport_rekey_result
transport_rekey_exercise(int (*ready)(struct test_session_pair *))
{
	struct transport_rekey_result result = {.ret = DSSH_ERROR_INIT};
	struct test_session_pair pair;
	uint8_t *saved = NULL;
	result.ret = transport_rekey_open(&pair);
	if (result.ret != 0)
		goto done;
	size_t length = pair.client->trans.session_id_sz;
	if (length == 0 || pair.server->trans.session_id_sz != length
	    || pair.client->trans.session_id == NULL || pair.server->trans.session_id == NULL
	    || memcmp(pair.client->trans.session_id, pair.server->trans.session_id, length) != 0) {
		result.ret = DSSH_ERROR_INVALID;
		goto done;
	}
	saved = malloc(length);
	if (saved == NULL) {
		result.ret = DSSH_ERROR_ALLOC;
		goto done;
	}
	memcpy(saved, pair.client->trans.session_id, length);
	uint32_t before[] = {pair.client->trans.tx_seq, pair.client->trans.rx_seq,
	    pair.server->trans.tx_seq, pair.server->trans.rx_seq};
	static const uint8_t ignored[] = {SSH_MSG_IGNORE, 0, 0, 0, 0};
	for (unsigned i = 0; i < 5; i++) {
		result.ret = send_packet(pair.client, ignored, sizeof(ignored), NULL);
		if (result.ret != 0)
			goto done;
	}
	if (ready != NULL) {
		result.ret = ready(&pair);
		if (result.ret != 0)
			goto done;
	}
	result.ready = true;
	result.ret = test_pair_run(&pair, transport_rekey_operation);
	if (result.ret != 0)
		goto done;
	uint32_t steps = test_using_dhgex() ? UINT32_C(4) : UINT32_C(3);
	uint32_t outbound = (uint32_t)(steps + 6);
	result.valid = pair.created == 4 && pair.joined == 4
	    && pair.client_result == 0 && pair.server_result == 0
	    && dssh_test_rekeys_completed(pair.client) == 1 && dssh_test_rekeys_completed(pair.server) == 1
	    && transport_rekey_sequence(before[0], pair.client->trans.tx_seq, outbound)
	    && transport_rekey_sequence(before[1], pair.client->trans.rx_seq, steps)
	    && transport_rekey_sequence(before[2], pair.server->trans.tx_seq, steps)
	    && transport_rekey_sequence(before[3], pair.server->trans.rx_seq, outbound)
	    && pair.client->trans.tx_since_rekey == 1 && pair.client->trans.rx_since_rekey == 0
	    && pair.server->trans.tx_since_rekey == 0 && pair.server->trans.rx_since_rekey == 1;
	if (result.valid)
		result.valid = transport_pair_exchange(pair.client, pair.server, false, SSH_MSG_SERVICE_REQUEST)
		    && transport_pair_exchange(pair.server, pair.client, false, SSH_MSG_SERVICE_ACCEPT);
	uint32_t final_outbound = (uint32_t)(steps + 7);
	uint32_t final_inbound = (uint32_t)(steps + 1);
	result.valid = result.valid && transport_pair_algorithms(&pair)
	    && transport_rekey_sequence(before[0], pair.client->trans.tx_seq, final_outbound)
	    && transport_rekey_sequence(before[1], pair.client->trans.rx_seq, final_inbound)
	    && transport_rekey_sequence(before[2], pair.server->trans.tx_seq, final_inbound)
	    && transport_rekey_sequence(before[3], pair.server->trans.rx_seq, final_outbound)
	    && pair.client->trans.tx_since_rekey == 2 && pair.client->trans.rx_since_rekey == 1
	    && pair.server->trans.tx_since_rekey == 1 && pair.server->trans.rx_since_rekey == 2
	    && pair.client->trans.session_id_sz == length && pair.server->trans.session_id_sz == length
	    && pair.client->trans.session_id != NULL && pair.server->trans.session_id != NULL
	    && memcmp(pair.client->trans.session_id, saved, length) == 0
	    && memcmp(pair.server->trans.session_id, saved, length) == 0;
done:
	free(saved);
	transport_pair_finish(&pair);
	result.created = pair.created;
	result.joined = pair.joined;
	result.retired = transport_pair_retired(&pair);
	result.valid = result.valid && result.retired;
	return result;
}

static int
test_rekey_owned_roundtrip(void)
{
	struct transport_rekey_result result = transport_rekey_exercise(NULL);
	int printed = printf("rekey_owned ready=%d created=%u joined=%u result=%d retired=%d valid=%d\n",
	    result.ready, result.created, result.joined, result.ret, result.retired, result.valid);
	ASSERT_TRUE(result.valid && result.ret == 0 && printed > 0);
	return TEST_PASS;
}

static int
test_rekey_sequence_oracle(void)
{
	const struct {
		uint32_t before;
		uint32_t after;
		uint32_t packets;
		bool expected;
	} cases[] = {
		{0, 0, 0, true}, {4, 13, 9, true}, {4, 1, 9, false},
		{UINT32_MAX, 0, 1, true}, {UINT32_MAX, UINT32_MAX, 0, true},
		{UINT32_MAX - 1, 1, 3, true}, {0, UINT32_MAX, UINT32_MAX, true},
		{UINT32_MAX, UINT32_MAX - 1, UINT32_MAX, true},
		{UINT32_MAX, 1, 1, false}, {4, 14, 9, false},
	};
	bool valid = true;
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
		valid = transport_rekey_sequence(cases[i].before, cases[i].after, cases[i].packets)
		    == cases[i].expected && valid;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#ifdef DSSH_TRANSPORT_REKEY_CONTROLS
void dssh_test_peer_controls_begin(void);

static int
transport_rekey_ready(struct test_session_pair *pair)
{
	bool valid = pair->created == 2 && pair->joined == 2
	    && !pair->owned[0] && !pair->owned[1];
	if (!valid)
		return DSSH_ERROR_INVALID;
	int printed = printf("rekey_prelude=1 created=2 joined=2 valid=1\n");
	if (printed < 0 || fflush(stdout) != 0)
		return DSSH_ERROR_INIT;
	dssh_test_peer_controls_begin();
	return 0;
}

static int
test_transport_rekey_ownership(void)
{
	struct transport_rekey_result result = transport_rekey_exercise(transport_rekey_ready);
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool valid = result.ready && result.retired && result.created >= 2 && result.joined >= 2;
	if (mode != NULL && strcmp(mode, "plain") == 0)
		valid = valid && result.valid && result.ret == 0;
	else if (mode != NULL && strcmp(mode, "tx") == 0)
		valid = valid && result.ret < 0;
	else if (mode != NULL && (strcmp(mode, "create-0") == 0 || strcmp(mode, "create-1") == 0))
		valid = valid && result.ret == DSSH_ERROR_INIT;
	else
		valid = false;
	unsigned created = result.created >= 2 ? result.created - 2 : 0;
	unsigned joined = result.joined >= 2 ? result.joined - 2 : 0;
	int printed = printf("fixture_owner=1 created=%u joined=%u valid=%d\n", created, joined, valid);
	ASSERT_TRUE(valid && printed > 0);
	return TEST_PASS;
}

static int
transport_rekey_assertion_failure(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static int
transport_rekey_assertion_control(unsigned phase)
{
	if (phase > 2)
		return 2;
	struct test_session_pair pair;
	int ret = transport_rekey_open(&pair);
	if (ret == 0)
		ret = transport_rekey_ready(&pair);
	pair.operation = transport_rekey_operation;
	for (unsigned i = 0; ret == 0 && i < phase; i++)
		ret = test_pair_start(&pair, i);
	if (ret != 0) {
		transport_pair_finish(&pair);
		return 2;
	}
	ret = transport_rekey_assertion_failure();
	bool valid = ret == TEST_FAIL && transport_pair_retired(&pair)
	    && pair.created >= 2 && pair.joined >= 2;
	unsigned created = pair.created >= 2 ? pair.created - 2 : 0;
	unsigned joined = pair.joined >= 2 ? pair.joined - 2 : 0;
	int printed = printf("fixture_assertion=1 phase=%u created=%u joined=%u valid=%d\n",
	    phase, created, joined, valid);
	return valid && printed > 0 ? 1 : 2;
}
#endif

#endif
