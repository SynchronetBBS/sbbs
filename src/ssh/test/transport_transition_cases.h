#ifndef DSSH_TEST_TRANSPORT_TRANSITION_CASES_H
#define DSSH_TEST_TRANSPORT_TRANSITION_CASES_H

static bool
transport_transition_algorithms(struct test_session_pair *pair, const char *kex,
    const char *cipher, const char *mac)
{
	dssh_session sessions[] = {pair->client, pair->server};
	enum dssh_direction directions[] = {DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT};
	for (size_t i = 0; i < sizeof(sessions) / sizeof(sessions[0]); i++) {
		const char *selected = dssh_transport_get_kex_name(sessions[i]);
		const char *key = dssh_transport_get_hostkey_name(sessions[i]);
		if (selected == NULL || key == NULL || strcmp(selected, kex) != 0
		    || strcmp(key, test_key_algo_name()) != 0)
			return false;
		for (size_t j = 0; j < sizeof(directions) / sizeof(directions[0]); j++) {
			const char *enc = dssh_transport_get_enc_name(sessions[i], directions[j]);
			const char *auth = dssh_transport_get_mac_name(sessions[i], directions[j]);
			const char *comp = dssh_transport_get_comp_name(sessions[i], directions[j]);
			if (enc == NULL || auth == NULL || comp == NULL || strcmp(enc, cipher) != 0
			    || strcmp(auth, mac) != 0 || strcmp(comp, "none") != 0)
				return false;
		}
	}
	return true;
}

static int
transport_transition_open(struct test_session_pair *pair, bool fixed)
{
	test_pair_begin(pair);
	dssh_test_reset_global_config();
	int ret = register_all_algorithms();
	if (ret == 0)
		ret = fixed ? register_fixed_rekey_methods() : dssh_register_aes128_cbc();
	if (ret == 0)
		ret = test_generate_host_key();
	if (ret == 0)
		ret = dssh_transport_set_callbacks(mock_tx_dispatch, mock_rx_dispatch,
		    mock_rxline_dispatch, mock_extra_line_cb);
	if (ret == 0)
		ret = test_pair_open(pair);
	const char *initial = "diffie-hellman-group16-sha512";
	dssh_session sessions[] = {pair->client, pair->server};
	for (size_t i = 0; ret == 0 && i < sizeof(sessions) / sizeof(sessions[0]); i++) {
		ret = dssh_session_set_rekey_seconds(sessions[i], 0);
		if (ret == 0 && fixed)
			ret = dssh_session_set_kex_filter(sessions[i], &initial, 1);
	}
	if (ret == 0)
		ret = test_pair_run(pair, dssh_transport_handshake);
	if (ret != 0)
		return ret;
	if (pair->created != 2 || pair->joined != 2 || !transport_pair_versions(pair)
	    || !transport_transition_algorithms(pair, fixed ? initial : test_kex_algo_name(),
	        "aes256-ctr", test_using_hmac256() ? "hmac-sha2-256" : "hmac-sha2-512")
	    || dssh_test_rekeys_completed(pair->client) != 0 || dssh_test_rekeys_completed(pair->server) != 0)
		return DSSH_ERROR_INVALID;
	if (!transport_pair_exchange(pair->client, pair->server, false, SSH_MSG_SERVICE_REQUEST)
	    || !transport_pair_exchange(pair->server, pair->client, false, SSH_MSG_SERVICE_ACCEPT))
		return DSSH_ERROR_INVALID;
	return 0;
}

struct transport_transition_result {
	struct transport_rekey_result owner;
	unsigned completed;
	unsigned prefix;
};

static int
transport_transition_assertion_failure(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static struct transport_transition_result
transport_transition_exercise(bool fixed, unsigned selected_round, unsigned assertion_phase,
    int (*ready)(struct test_session_pair *, bool, unsigned))
{
	struct transport_transition_result result = {.owner.ret = DSSH_ERROR_INIT};
	struct test_session_pair pair;
	uint8_t *saved = NULL;
	result.owner.ret = transport_transition_open(&pair, fixed);
	if (result.owner.ret != 0)
		goto done;
	size_t length = pair.client->trans.session_id_sz;
	if (length == 0 || (fixed && length != 64) || pair.server->trans.session_id_sz != length
	    || pair.client->trans.session_id == NULL || pair.server->trans.session_id == NULL
	    || memcmp(pair.client->trans.session_id, pair.server->trans.session_id, length) != 0) {
		result.owner.ret = DSSH_ERROR_INVALID;
		goto done;
	}
	saved = malloc(length);
	if (saved == NULL) {
		result.owner.ret = DSSH_ERROR_ALLOC;
		goto done;
	}
	memcpy(saved, pair.client->trans.session_id, length);
	const char *methods[] = {"diffie-hellman-group14-sha256", "curve25519-sha256", "diffie-hellman-group16-sha512"};
	unsigned rounds = fixed ? 3U : 2U;
	const char *initial_mac = test_using_hmac256() ? "hmac-sha2-256" : "hmac-sha2-512";
	const char *other_mac = test_using_hmac256() ? "hmac-sha2-512" : "hmac-sha2-256";
	for (unsigned round = 0; round < rounds; round++) {
		const char *kex = fixed ? methods[round] : test_kex_algo_name();
		const char *cipher = !fixed && round == 0 ? "aes128-cbc" : "aes256-ctr";
		const char *mac = !fixed && round == 0 ? other_mac : initial_mac;
		dssh_session sessions[] = {pair.client, pair.server};
		for (size_t i = 0; i < sizeof(sessions) / sizeof(sessions[0]); i++) {
			result.owner.ret = dssh_session_set_kex_filter(sessions[i], &kex, 1);
			if (result.owner.ret == 0)
				result.owner.ret = dssh_session_set_enc_filter(sessions[i], &cipher, 1);
			if (result.owner.ret == 0)
				result.owner.ret = dssh_session_set_mac_filter(sessions[i], &mac, 1);
			if (result.owner.ret != 0)
				goto done;
		}
		uint32_t before[] = {pair.client->trans.tx_seq, pair.client->trans.rx_seq,
		    pair.server->trans.tx_seq, pair.server->trans.rx_seq};
		if (ready != NULL && round == selected_round) {
			result.prefix = pair.created;
			result.owner.ret = ready(&pair, fixed, round);
			if (result.owner.ret != 0)
				goto done;
			result.owner.ready = true;
			if (assertion_phase < 3) {
				/* The assertion hook owns the pair, not this saved byte array. */
				free(saved);
				saved = NULL;
				pair.operation = transport_rekey_operation;
				for (unsigned i = 0; result.owner.ret == 0 && i < assertion_phase; i++)
					result.owner.ret = test_pair_start(&pair, i);
				if (result.owner.ret != 0)
					goto done;
				result.owner.ret = transport_transition_assertion_failure();
				result.owner.valid = result.owner.ret == TEST_FAIL && transport_pair_retired(&pair);
				goto done;
			}
		}
		result.owner.ret = test_pair_run(&pair, transport_rekey_operation);
		if (result.owner.ret != 0)
			goto done;
		uint32_t steps = strcmp(kex, "diffie-hellman-group-exchange-sha256") == 0
		    ? UINT32_C(4) : UINT32_C(3);
		uint32_t marker_delta = (uint32_t)(steps + 1);
		bool valid = pair.client_result == 0 && pair.server_result == 0
		    && transport_rekey_sequence(before[0], pair.client->trans.tx_seq, marker_delta)
		    && transport_rekey_sequence(before[1], pair.client->trans.rx_seq, steps)
		    && transport_rekey_sequence(before[2], pair.server->trans.tx_seq, steps)
		    && transport_rekey_sequence(before[3], pair.server->trans.rx_seq, marker_delta)
		    && pair.client->trans.tx_since_rekey == 1 && pair.client->trans.rx_since_rekey == 0
		    && pair.server->trans.tx_since_rekey == 0 && pair.server->trans.rx_since_rekey == 1;
		if (valid)
			valid = transport_pair_exchange(pair.client, pair.server, false, SSH_MSG_SERVICE_REQUEST)
			    && transport_pair_exchange(pair.server, pair.client, false, SSH_MSG_SERVICE_ACCEPT);
		uint32_t outbound = (uint32_t)(steps + 2);
		valid = valid && transport_transition_algorithms(&pair, kex, cipher, mac)
		    && pair.created == 4 + 2 * round && pair.joined == pair.created
		    && dssh_test_rekeys_completed(pair.client) == round + 1
		    && dssh_test_rekeys_completed(pair.server) == round + 1
		    && transport_rekey_sequence(before[0], pair.client->trans.tx_seq, outbound)
		    && transport_rekey_sequence(before[1], pair.client->trans.rx_seq, marker_delta)
		    && transport_rekey_sequence(before[2], pair.server->trans.tx_seq, marker_delta)
		    && transport_rekey_sequence(before[3], pair.server->trans.rx_seq, outbound)
		    && pair.client->trans.tx_since_rekey == 2 && pair.client->trans.rx_since_rekey == 1
		    && pair.server->trans.tx_since_rekey == 1 && pair.server->trans.rx_since_rekey == 2
		    && pair.client->trans.session_id_sz == length && pair.server->trans.session_id_sz == length
		    && pair.client->trans.session_id != NULL && pair.server->trans.session_id != NULL
		    && memcmp(pair.client->trans.session_id, saved, length) == 0
		    && memcmp(pair.server->trans.session_id, saved, length) == 0;
		int printed = printf("transition_round fixed=%d round=%u kex=%s cipher=%s mac=%s valid=%d\n",
		    fixed, round, kex, cipher, mac, valid);
		if (!valid || printed < 0) {
			result.owner.ret = DSSH_ERROR_INVALID;
			goto done;
		}
		result.completed++;
		if (ready != NULL && round == selected_round)
			break;
	}
	result.owner.valid = result.completed == (ready == NULL ? rounds : selected_round + 1);
done:
	free(saved);
	transport_pair_finish(&pair);
	result.owner.created = pair.created;
	result.owner.joined = pair.joined;
	result.owner.retired = transport_pair_retired(&pair);
	result.owner.valid = result.owner.valid && result.owner.retired;
	return result;
}

static int
test_fixed_rekey_methods(void)
{
	struct transport_transition_result result = transport_transition_exercise(true, 0, 3, NULL);
	ASSERT_TRUE(result.owner.ret == 0 && result.owner.valid && result.completed == 3);
	return TEST_PASS;
}

static int
test_rekey_changes_algorithms(void)
{
	struct transport_transition_result result = transport_transition_exercise(false, 0, 3, NULL);
	ASSERT_TRUE(result.owner.ret == 0 && result.owner.valid && result.completed == 2);
	return TEST_PASS;
}

#ifdef DSSH_TRANSPORT_REKEY_CONTROLS
static int
transport_transition_selection(bool *fixed, unsigned *round)
{
	const char *kind = getenv("DSSH_TRANSITION_KIND");
	const char *text = getenv("DSSH_TRANSITION_ROUND");
	if (kind == NULL || text == NULL || strlen(text) != 1 || text[0] < '0' || text[0] > '2')
		return DSSH_ERROR_INVALID;
	if (strcmp(kind, "fixed") != 0 && strcmp(kind, "cipher") != 0)
		return DSSH_ERROR_INVALID;
	*fixed = strcmp(kind, "fixed") == 0;
	unsigned selected = (unsigned)(text[0] - '0');
	if (!*fixed && selected > 1)
		return DSSH_ERROR_INVALID;
	*round = selected;
	return 0;
}

static int
transport_transition_ready(struct test_session_pair *pair, bool fixed, unsigned round)
{
	if (round > 2 || pair->created != 2 + 2 * round || pair->joined != pair->created
	    || pair->owned[0] || pair->owned[1])
		return DSSH_ERROR_INVALID;
	int printed = printf("transition_prelude fixed=%d round=%u created=%u joined=%u valid=1\n",
	    fixed, round, pair->created, pair->joined);
	if (printed < 0 || fflush(stdout) != 0)
		return DSSH_ERROR_INIT;
	dssh_test_peer_controls_begin();
	return 0;
}

static int
test_transport_rekey_selected_ownership(void)
{
	if (getenv("DSSH_TRANSITION_KIND") == NULL)
		return test_transport_rekey_ownership();
	bool fixed;
	unsigned round;
	if (transport_transition_selection(&fixed, &round) != 0)
		return TEST_FAIL;
	struct transport_transition_result result = transport_transition_exercise(fixed, round, 3, transport_transition_ready);
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool valid = result.owner.ready && result.owner.retired
	    && result.owner.created >= result.prefix && result.owner.joined >= result.prefix;
	if (mode != NULL && strcmp(mode, "plain") == 0)
		valid = valid && result.owner.valid && result.owner.ret == 0;
	else if (mode != NULL && strcmp(mode, "tx") == 0)
		valid = valid && result.owner.ret < 0;
	else if (mode != NULL && (strcmp(mode, "create-0") == 0 || strcmp(mode, "create-1") == 0))
		valid = valid && result.owner.ret == DSSH_ERROR_INIT;
	else
		valid = false;
	unsigned created = result.owner.created >= result.prefix ? result.owner.created - result.prefix : 0;
	unsigned joined = result.owner.joined >= result.prefix ? result.owner.joined - result.prefix : 0;
	int printed = printf("fixture_owner=1 created=%u joined=%u valid=%d\n", created, joined, valid);
	ASSERT_TRUE(valid && printed > 0);
	return TEST_PASS;
}

static int
transport_rekey_selected_assertion_control(unsigned phase)
{
	if (getenv("DSSH_TRANSITION_KIND") == NULL)
		return transport_rekey_assertion_control(phase);
	bool fixed;
	unsigned round;
	if (phase > 2 || transport_transition_selection(&fixed, &round) != 0)
		return 2;
	struct transport_transition_result result = transport_transition_exercise(fixed, round, phase, transport_transition_ready);
	bool valid = result.owner.ready && result.owner.valid && result.owner.ret == TEST_FAIL
	    && result.owner.created >= result.prefix && result.owner.joined >= result.prefix;
	unsigned created = result.owner.created >= result.prefix ? result.owner.created - result.prefix : 0;
	unsigned joined = result.owner.joined >= result.prefix ? result.owner.joined - result.prefix : 0;
	int printed = printf("fixture_assertion=1 phase=%u created=%u joined=%u valid=%d\n",
	    phase, created, joined, valid);
	return valid && printed > 0 ? 1 : 2;
}
#endif

#endif
