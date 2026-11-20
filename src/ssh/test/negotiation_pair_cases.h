#ifndef DSSH_TEST_NEGOTIATION_PAIR_CASES_H
#define DSSH_TEST_NEGOTIATION_PAIR_CASES_H

/* Replace one algorithm list in a complete retained offer. All other fields,
 * cookie and trailer remain byte-identical. Publish only a complete result. */
static int
negotiation_replace_offer(const uint8_t *original, size_t length, size_t field,
    const char *replacement, size_t capacity, uint8_t **output, size_t *output_length)
{
	if (original == NULL || replacement == NULL || output == NULL || output_length == NULL
	    || field >= 8 || *output != NULL || *output_length != 0)
		return DSSH_ERROR_INVALID;
	struct dssh_name_list before[10], after[10];
	bool follows, new_follows;
	int ret = parse_peer_kexinit(original, length, before, &follows);
	if (ret != 0)
		return ret;
	size_t replacement_length = strlen(replacement);
	if (replacement_length == 0 || replacement_length > DSSH_ALGO_NAME_MAX)
		return DSSH_ERROR_INVALID;
	size_t position = 17;
	for (size_t i = 0; i < field; i++) {
		if (!dssh_buffer_has(length, position, 4)
		    || before[i].len > length - position - 4)
			return DSSH_ERROR_PARSE;
		position += 4;
		position += before[i].len;
	}
	if (!dssh_buffer_has(length, position, 4) || before[field].len > length - position - 4)
		return DSSH_ERROR_PARSE;
	size_t tail = position + 4 + before[field].len;
	size_t needed = length - before[field].len;
	if (replacement_length > SIZE_MAX - needed)
		return DSSH_ERROR_TOOLONG;
	needed += replacement_length;
	if (needed > capacity)
		return DSSH_ERROR_TOOLONG;
	uint8_t *candidate = dssh_test_malloc(needed);
	if (candidate == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(candidate, original, position);
	uint32_t wire_length = (uint32_t)replacement_length; /* At most64. */
	size_t cursor = position;
	ret = dssh_serialize_uint32(wire_length, candidate, needed, &cursor);
	if (ret == 0) {
		memcpy(&candidate[cursor], replacement, replacement_length);
		cursor += replacement_length;
		memcpy(&candidate[cursor], &original[tail], length - tail);
		ret = parse_peer_kexinit(candidate, needed, after, &new_follows);
	}
	if (ret == 0 && (follows != new_follows || memcmp(candidate, original, 17) != 0
	    || memcmp(&candidate[needed - 5], &original[length - 5], 5) != 0))
		ret = DSSH_ERROR_INVALID;
	for (size_t i = 0; ret == 0 && i < 10; i++) {
		const uint8_t *expected = i == field ? (const uint8_t *)replacement : before[i].data;
		size_t expected_length = i == field ? replacement_length : before[i].len;
		if (after[i].len != expected_length || memcmp(after[i].data, expected, expected_length) != 0)
			ret = DSSH_ERROR_INVALID;
	}
	if (ret != 0) {
		dssh_test_free(candidate);
		return ret;
	}
	*output = candidate;
	*output_length = needed;
	return 0;
}

static bool
negotiation_failure_selections(dssh_session sess, bool compression)
{
	const char *kex = dssh_transport_get_kex_name(sess);
	const char *key = dssh_transport_get_hostkey_name(sess);
	if (compression) {
		if (kex == NULL || key == NULL || strcmp(kex, test_kex_algo_name()) != 0
		    || strcmp(key, test_key_algo_name()) != 0)
			return false;
	}
	else if (kex != NULL || key != NULL)
		return false;
	enum dssh_direction directions[] = {DSSH_CLIENT_TO_SERVER, DSSH_SERVER_TO_CLIENT};
	for (size_t i = 0; i < sizeof(directions) / sizeof(directions[0]); i++) {
		const char *enc = dssh_transport_get_enc_name(sess, directions[i]);
		const char *mac = dssh_transport_get_mac_name(sess, directions[i]);
		const char *comp = dssh_transport_get_comp_name(sess, directions[i]);
		if (enc == NULL || mac == NULL || strcmp(enc, "aes256-ctr") != 0
		    || strcmp(mac, test_using_hmac256() ? "hmac-sha2-256" : "hmac-sha2-512") != 0)
			return false;
		if (compression && directions[i] == DSSH_SERVER_TO_CLIENT) {
			if (comp != NULL)
				return false;
		}
		else if (comp == NULL || strcmp(comp, "none") != 0)
			return false;
	}
	return true;
}

static bool
negotiation_failure_wire(struct test_session_pair *pair)
{
	struct dssh_name_list baseline[10], received[10];
	bool follows;
	if (parse_peer_kexinit(pair->server->trans.peer_kexinit,
	    pair->server->trans.peer_kexinit_sz, baseline, &follows) != 0 || follows)
		return false;
	uint8_t type = 0, *payload = NULL;
	size_t length = 0;
	for (unsigned round = 0; round < 2; round++) {
		if (recv_packet_raw(pair->server, &type, &payload, &length) != 0 || payload == NULL
		    || type != SSH_MSG_KEXINIT || parse_peer_kexinit(payload, length, received, &follows) != 0 || follows)
			return false;
		for (size_t i = 0; i < 10; i++) {
			if (received[i].len != baseline[i].len
			    || memcmp(received[i].data, baseline[i].data, baseline[i].len) != 0)
				return false;
		}
	}
	static const uint8_t expected[] = {SSH_MSG_DISCONNECT, 0, 0, 0, 3, 0, 0, 0, 19,
	    'n', 'o', ' ', 'c', 'o', 'm', 'm', 'o', 'n', ' ', 'a', 'l', 'g', 'o', 'r', 'i', 't', 'h', 'm',
	    0, 0, 0, 0};
	return recv_packet_raw(pair->server, &type, &payload, &length) == 0 && payload != NULL
	    && type == SSH_MSG_DISCONNECT && length == sizeof(expected)
	    && memcmp(payload, expected, sizeof(expected)) == 0;
}

static int
negotiation_pair_prepare(struct test_session_pair *pair)
{
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	return transport_pair_prepare(pair, 4);
}

static struct transport_rekey_result
negotiation_pair_case(bool compression)
{
	struct transport_rekey_result result = {.ret = DSSH_ERROR_INIT};
	struct test_session_pair pair;
	uint8_t *candidate = NULL;
	size_t candidate_length = 0;
	result.ret = negotiation_pair_prepare(&pair);
	if (result.ret == 0)
		result.ret = test_pair_run(&pair, direct_kex_negotiate);
	if (result.ret != 0)
		goto done;
	if (pair.created != 2 || pair.joined != 2 || !transport_pair_versions(&pair)
	    || !transport_pair_algorithms(&pair)) {
		result.ret = DSSH_ERROR_INVALID;
		goto done;
	}
	/* First prove unchanged cached-offer negotiation still succeeds. */
	result.ret = kexinit(pair.client);
	if (result.ret != 0)
		goto done;
	if (!transport_pair_algorithms(&pair)) {
		result.ret = DSSH_ERROR_INVALID;
		goto done;
	}
	result.ready = true;
	result.ret = negotiation_replace_offer(pair.client->trans.peer_kexinit,
	    pair.client->trans.peer_kexinit_sz, compression ? 7U : 0U,
	    compression ? "bogus" : "unavailable-kex", pair.client->trans.packet_buf_sz,
	    &candidate, &candidate_length);
	if (result.ret != 0)
		goto done;
	uint8_t *old = pair.client->trans.peer_kexinit;
	pair.client->trans.peer_kexinit = candidate;
	pair.client->trans.peer_kexinit_sz = candidate_length;
	candidate = NULL;
	dssh_test_free(old);
	result.ret = kexinit(pair.client);
	result.valid = result.ret == DSSH_ERROR_INVALID && dssh_session_is_terminated(pair.client)
	    && negotiation_failure_selections(pair.client, compression) && negotiation_failure_wire(&pair);
done:
	dssh_test_free(candidate);
	transport_pair_finish(&pair);
	result.created = pair.created;
	result.joined = pair.joined;
	result.retired = transport_pair_retired(&pair);
	result.valid = result.valid && result.retired;
	return result;
}

static int
test_negotiate_no_common_kex(void)
{
	struct transport_rekey_result result = negotiation_pair_case(false);
	int printed = printf("negotiation_pair kind=kex ready=%d created=%u joined=%u result=%d retired=%d valid=%d\n",
	    result.ready, result.created, result.joined, result.ret, result.retired, result.valid);
	ASSERT_TRUE(result.valid && result.ret == DSSH_ERROR_INVALID && printed > 0);
	return TEST_PASS;
}

static int
test_negotiate_no_common_comp_s2c(void)
{
	struct transport_rekey_result result = negotiation_pair_case(true);
	int printed = printf("negotiation_pair kind=comp ready=%d created=%u joined=%u result=%d retired=%d valid=%d\n",
	    result.ready, result.created, result.joined, result.ret, result.retired, result.valid);
	ASSERT_TRUE(result.valid && result.ret == DSSH_ERROR_INVALID && printed > 0);
	return TEST_PASS;
}

static int
test_negotiation_offer_replace(void)
{
	static const uint8_t original[70] = {SSH_MSG_KEXINIT,
	    [20] = 1, [21] = 'a', [25] = 1, [26] = 'b', [30] = 1, [31] = 'c', [35] = 1, [36] = 'd',
	    [40] = 1, [41] = 'e', [45] = 1, [46] = 'f', [50] = 1, [51] = 'g', [55] = 1, [56] = 'h'};
	static const uint8_t expected[74] = {SSH_MSG_KEXINIT,
	    [20] = 1, [21] = 'a', [25] = 1, [26] = 'b', [30] = 1, [31] = 'c', [35] = 1, [36] = 'd',
	    [40] = 1, [41] = 'e', [45] = 1, [46] = 'f', [50] = 1, [51] = 'g',
	    [55] = 5, [56] = 'b', [57] = 'o', [58] = 'g', [59] = 'u', [60] = 's'};
	uint8_t *candidate = NULL;
	size_t length = 0;
	int ret = negotiation_replace_offer(original, sizeof(original), 7, "bogus", sizeof(expected), &candidate, &length);
	bool valid = ret == 0 && length == sizeof(expected) && candidate != NULL
	    && memcmp(candidate, expected, sizeof(expected)) == 0;
	dssh_test_free(candidate);
	candidate = NULL;
	length = 0;
	dssh_test_alloc_fail_after(0);
	ret = negotiation_replace_offer(original, sizeof(original), 7, "bogus", sizeof(expected), &candidate, &length);
	int calls = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	valid = valid && ret == DSSH_ERROR_ALLOC && calls == 1 && candidate == NULL && length == 0;
	dssh_test_free(candidate);
	candidate = NULL;
	length = 0;
	valid = valid && negotiation_replace_offer(original, sizeof(original), 7, "bogus", sizeof(expected) - 1, &candidate, &length) == DSSH_ERROR_TOOLONG
	    && negotiation_replace_offer(original, sizeof(original) - 1, 7, "bogus", sizeof(expected), &candidate, &length) == DSSH_ERROR_PARSE
	    && negotiation_replace_offer(original, sizeof(original), 8, "bogus", sizeof(expected), &candidate, &length) == DSSH_ERROR_INVALID
	    && negotiation_replace_offer(original, sizeof(original), 7, "", sizeof(expected), &candidate, &length) == DSSH_ERROR_INVALID
	    && negotiation_replace_offer(original, sizeof(original), 7, ",", sizeof(expected), &candidate, &length) == DSSH_ERROR_PARSE
	    && candidate == NULL && length == 0;
	dssh_test_free(candidate);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#ifdef DSSH_TRANSPORT_PAIR_CONTROLS
static int
negotiation_pair_kind(bool *compression)
{
	const char *kind = getenv("DSSH_NEGOTIATION_KIND");
	if (kind == NULL || (strcmp(kind, "kex") != 0 && strcmp(kind, "comp") != 0))
		return DSSH_ERROR_INVALID;
	*compression = strcmp(kind, "comp") == 0;
	return 0;
}

static int
test_transport_all_pair_ownership(void)
{
	if (getenv("DSSH_NEGOTIATION_KIND") == NULL)
		return test_transport_selected_pair_ownership();
	bool compression;
	if (negotiation_pair_kind(&compression) != 0)
		return TEST_FAIL;
	struct transport_rekey_result result = negotiation_pair_case(compression);
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool valid = result.retired;
	if (mode != NULL && strcmp(mode, "plain") == 0)
		valid = valid && result.ready && result.valid && result.ret == DSSH_ERROR_INVALID;
	else if (mode != NULL && strcmp(mode, "tx") == 0)
		valid = valid && !result.ready && result.ret < 0 && result.created == 2;
	else if (mode != NULL && (strcmp(mode, "create-0") == 0 || strcmp(mode, "create-1") == 0))
		valid = valid && !result.ready && result.ret == DSSH_ERROR_INIT;
	else
		valid = false;
	int printed = printf("fixture_owner=1 created=%u joined=%u valid=%d\n", result.created, result.joined, valid);
	ASSERT_TRUE(valid && printed > 0);
	return TEST_PASS;
}

static int
negotiation_pair_assertion_failure(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static int
transport_all_pair_assertion_control(unsigned phase)
{
	if (getenv("DSSH_NEGOTIATION_KIND") == NULL)
		return transport_selected_pair_assertion_control(phase);
	bool compression;
	if (phase > 2 || negotiation_pair_kind(&compression) != 0)
		return 2;
	(void)compression;
	struct test_session_pair pair;
	int ret = negotiation_pair_prepare(&pair);
	pair.operation = direct_kex_negotiate;
	for (unsigned i = 0; ret == 0 && i < phase; i++)
		ret = test_pair_start(&pair, i);
	if (ret != 0) {
		transport_pair_finish(&pair);
		return 2;
	}
	ret = negotiation_pair_assertion_failure();
	bool valid = ret == TEST_FAIL && transport_pair_retired(&pair);
	int printed = printf("fixture_assertion=1 phase=%u created=%u joined=%u valid=%d\n",
	    phase, pair.created, pair.joined, valid);
	return valid && printed > 0 ? 1 : 2;
}
#endif

#endif
