#ifndef DSSH_TEST_DIRECT_KEX_CASES_H
#define DSSH_TEST_DIRECT_KEX_CASES_H

static int
direct_kex_kind(bool *gex)
{
	const char *kind = getenv("DSSH_DIRECT_KEX_KIND");
	if (kind == NULL || (strcmp(kind, "gex") != 0 && strcmp(kind, "curve") != 0))
		return DSSH_ERROR_INVALID;
	*gex = strcmp(kind, "gex") == 0;
	return 0;
}

static struct transport_rekey_result
direct_kex_case(bool gex)
{
	struct transport_rekey_result result = {.ret = DSSH_ERROR_INIT};
	struct test_session_pair pair;
	result.ret = direct_kex_setup(&pair, gex);
	if (result.ret != 0)
		goto done;
	static const uint8_t request[] = {34, 0, 0, 8, 0, 0, 0, 8, 0, 0, 0, 8, 0};
	static const uint8_t init[] = {32, 0, 0, 0, 1, 2};
	uint8_t curve[37] = {30, 0, 0, 0, 32, 9};
	uint8_t wire[96];
	size_t length = 0;
	if (gex) {
		result.ret = build_plaintext_packet_t(request, sizeof(request), wire, sizeof(wire), &length);
		if (result.ret == 0)
			result.ret = build_plaintext_packet_t(init, sizeof(init), wire, sizeof(wire), &length);
	}
	else
		result.ret = build_plaintext_packet_t(curve, sizeof(curve), wire, sizeof(wire), &length);
	for (unsigned round = 0; result.ret == 0 && round < 2; round++) {
		int operation = DSSH_ERROR_INIT;
		result.ret = direct_kex_run(&pair, wire, length, -1, &operation);
		if (result.ret != 0)
			break;
		struct dssh_kex_result *out = &pair.server->trans.kex_result;
		if (operation != 0 || out->shared_secret.data == NULL || out->shared_secret.len == 0
		    || out->shared_secret.len > (gex ? 256U : 32U)
		    || out->exchange_hash.data == NULL || out->exchange_hash.len != 32 || out->lease.release == NULL) {
			result.ret = DSSH_ERROR_INVALID;
			break;
		}
		result.valid = round == 1;
	}
done:
	direct_kex_dispose(&pair);
	result.created = pair.created;
	result.joined = pair.joined;
	result.retired = transport_pair_retired(&pair);
	result.valid = result.valid && result.retired;
	return result;
}

static int
test_direct_kex_fixture(void)
{
	bool gex = false;
	if (getenv("DSSH_DIRECT_KEX_KIND") != NULL && direct_kex_kind(&gex) != 0)
		return TEST_FAIL;
	struct transport_rekey_result result = direct_kex_case(gex);
	int printed = printf("direct_kex kind=%s created=%u joined=%u result=%d retired=%d valid=%d\n",
	    gex ? "gex" : "curve", result.created, result.joined, result.ret, result.retired, result.valid);
	ASSERT_TRUE(result.ret == 0 && result.valid && printed > 0);
	return TEST_PASS;
}

static int
test_direct_packet_builder(void)
{
	const struct {
		size_t payload;
		size_t total;
		uint8_t packet;
		uint8_t padding;
	} cases[] = {{0, 16, 12, 11}, {1, 16, 12, 10}, {7, 16, 12, 4},
	    {8, 24, 20, 11}, {9, 24, 20, 10}};
	uint8_t payload[9];
	memset(payload, 0x42, sizeof(payload));
	bool valid = true;
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		uint8_t wire[64], expected[64];
		memset(wire, 0x5a, sizeof(wire));
		memset(expected, 0x5a, sizeof(expected));
		expected[3] = 0;
		expected[4] = 0;
		expected[5] = 0;
		expected[6] = cases[i].packet;
		expected[7] = cases[i].padding;
		memcpy(&expected[8], payload, cases[i].payload);
		memset(&expected[8 + cases[i].payload], 0, cases[i].padding);
		size_t position = 3;
		int ret = build_plaintext_packet_t(payload, cases[i].payload, wire, sizeof(wire), &position);
		valid = valid && ret == 0 && position == 3 + cases[i].total
		    && memcmp(wire, expected, sizeof(wire)) == 0;
	}
	uint8_t wire[64], saved[64];
	memset(wire, 0x5a, sizeof(wire));
	memcpy(saved, wire, sizeof(saved));
	size_t position = 3;
	valid = valid && build_plaintext_packet_t(payload, 9, wire, 26, &position) == DSSH_ERROR_TOOLONG
	    && position == 3 && memcmp(wire, saved, sizeof(wire)) == 0
	    && build_plaintext_packet_t(payload, SIZE_MAX, wire, sizeof(wire), &position) == DSSH_ERROR_TOOLONG
	    && position == 3 && memcmp(wire, saved, sizeof(wire)) == 0
	    && build_plaintext_packet_t(NULL, 1, wire, sizeof(wire), &position) == DSSH_ERROR_INVALID
	    && build_plaintext_packet_t(payload, 1, NULL, sizeof(wire), &position) == DSSH_ERROR_INVALID
	    && build_plaintext_packet_t(payload, 1, wire, sizeof(wire), NULL) == DSSH_ERROR_INVALID;
	position = 65;
	valid = valid && build_plaintext_packet_t(payload, 1, wire, sizeof(wire), &position) == DSSH_ERROR_INVALID
	    && position == 65 && memcmp(wire, saved, sizeof(wire)) == 0;
	position = 0;
	valid = valid && build_plaintext_packet_t(NULL, 0, wire, 16, &position) == 0 && position == 16
	    && build_plaintext_packet_t(payload, 1, wire, 32, &position) == 0 && position == 32
	    && wire[3] == 12 && wire[4] == 11 && wire[19] == 12 && wire[20] == 10 && wire[21] == 0x42;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#ifdef DSSH_TRANSPORT_PAIR_CONTROLS
static int
test_transport_selected_pair_ownership(void)
{
	if (getenv("DSSH_DIRECT_KEX_KIND") == NULL)
		return test_transport_pair_ownership();
	bool gex;
	if (direct_kex_kind(&gex) != 0)
		return TEST_FAIL;
	struct transport_rekey_result result = direct_kex_case(gex);
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool valid = result.retired;
	if (mode != NULL && strcmp(mode, "plain") == 0)
		valid = valid && result.ret == 0 && result.valid;
	else if (mode != NULL && strcmp(mode, "tx") == 0)
		valid = valid && result.ret < 0 && result.created == 2;
	else if (mode != NULL && (strcmp(mode, "create-0") == 0 || strcmp(mode, "create-1") == 0))
		valid = valid && result.ret == DSSH_ERROR_INIT;
	else
		valid = false;
	int printed = printf("fixture_owner=1 created=%u joined=%u valid=%d\n", result.created, result.joined, valid);
	ASSERT_TRUE(valid && printed > 0);
	return TEST_PASS;
}

static int
direct_kex_assertion_failure(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static int
transport_selected_pair_assertion_control(unsigned phase)
{
	if (getenv("DSSH_DIRECT_KEX_KIND") == NULL)
		return transport_pair_assertion_control(phase);
	bool gex;
	if (phase > 2 || direct_kex_kind(&gex) != 0)
		return 2;
	struct test_session_pair pair;
	int ret = direct_kex_prepare(&pair, gex);
	pair.operation = direct_kex_negotiate;
	for (unsigned i = 0; ret == 0 && i < phase; i++)
		ret = test_pair_start(&pair, i);
	if (ret != 0) {
		direct_kex_dispose(&pair);
		return 2;
	}
	ret = direct_kex_assertion_failure();
	bool valid = ret == TEST_FAIL && transport_pair_retired(&pair);
	int printed = printf("fixture_assertion=1 phase=%u created=%u joined=%u valid=%d\n",
	    phase, pair.created, pair.joined, valid);
	return valid && printed > 0 ? 1 : 2;
}
#endif

#endif
