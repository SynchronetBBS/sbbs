#ifndef DSSH_TEST_AUTH_STATE_WIRE_CASES_H
#define DSSH_TEST_AUTH_STATE_WIRE_CASES_H

/* Private fixture construction: publish the extent only after complete checked
 * serialization. Refused preflight leaves both storage and extent unchanged. */
static int
build_auth_state_request(const char *username, const char *method, uint8_t *packet,
    size_t capacity, size_t *length)
{
	if (username == NULL || method == NULL || packet == NULL || length == NULL)
		return DSSH_ERROR_INVALID;
	size_t user_length = strlen(username);
	size_t method_length = strlen(method);
#if SIZE_MAX > UINT32_MAX
	if (user_length > UINT32_MAX || method_length > UINT32_MAX)
		return DSSH_ERROR_TOOLONG;
#endif
	bool kbi = strcmp(method, "keyboard-interactive") == 0;
	bool password = strcmp(method, "password") == 0;
	size_t needed = 27; /* Type and three string prefixes, including service. */
	if (user_length > SIZE_MAX - needed)
		return DSSH_ERROR_TOOLONG;
	needed += user_length;
	if (method_length > SIZE_MAX - needed)
		return DSSH_ERROR_TOOLONG;
	needed += method_length;
	size_t suffix = kbi ? 8U : password ? 5U : 0U;
	if (suffix > SIZE_MAX - needed)
		return DSSH_ERROR_TOOLONG;
	needed += suffix;
	if (needed > capacity)
		return DSSH_ERROR_TOOLONG;
	uint32_t wire_user = (uint32_t)user_length;
	uint32_t wire_method = (uint32_t)method_length;
	size_t position = 0;
	packet[position++] = SSH_MSG_USERAUTH_REQUEST;
	int ret = dssh_serialize_uint32(wire_user, packet, capacity, &position);
	if (ret != 0)
		return ret;
	memcpy(&packet[position], username, user_length);
	position += user_length;
	ret = dssh_serialize_uint32(14, packet, capacity, &position);
	if (ret != 0)
		return ret;
	memcpy(&packet[position], "ssh-connection", 14);
	position += 14;
	ret = dssh_serialize_uint32(wire_method, packet, capacity, &position);
	if (ret != 0)
		return ret;
	memcpy(&packet[position], method, method_length);
	position += method_length;
	if (kbi) {
		ret = dssh_serialize_uint32(0, packet, capacity, &position);
		if (ret == 0)
			ret = dssh_serialize_uint32(0, packet, capacity, &position);
	}
	else if (password) {
		packet[position++] = 0;
		ret = dssh_serialize_uint32(0, packet, capacity, &position);
	}
	if (ret != 0)
		return ret;
	if (position != needed)
		return DSSH_ERROR_INVALID;
	*length = position;
	return 0;
}

static int
queue_auth_service(dssh_session sender, bool accept)
{
	if (sender == NULL)
		return DSSH_ERROR_INVALID;
	uint8_t packet[17] = {0, 0, 0, 0, 12, 's', 's', 'h', '-', 'u', 's', 'e', 'r', 'a', 'u', 't', 'h'};
	packet[0] = accept ? SSH_MSG_SERVICE_ACCEPT : SSH_MSG_SERVICE_REQUEST;
	return send_packet(sender, packet, sizeof(packet), NULL);
}

static int
queue_auth_state_request(dssh_session sender, const char *username, const char *method)
{
	if (sender == NULL)
		return DSSH_ERROR_INVALID;
	uint8_t packet[128];
	size_t length = 0;
	int ret = build_auth_state_request(username, method, packet, sizeof(packet), &length);
	if (ret != 0)
		return ret;
	return send_packet(sender, packet, length, NULL);
}

static const uint8_t auth_literal_prefix[24] = {50, 0, 0, 0, 1, 'u', 0, 0, 0, 14,
    's', 's', 'h', '-', 'c', 'o', 'n', 'n', 'e', 'c', 't', 'i', 'o', 'n'};
static const uint8_t auth_literal_none[8] = {0, 0, 0, 4, 'n', 'o', 'n', 'e'};
static const uint8_t auth_literal_password[17] = {0, 0, 0, 8, 'p', 'a', 's', 's', 'w', 'o', 'r', 'd'};
static const uint8_t auth_literal_kbi[32] = {0, 0, 0, 20, 'k', 'e', 'y', 'b', 'o', 'a', 'r', 'd', '-',
    'i', 'n', 't', 'e', 'r', 'a', 'c', 't', 'i', 'v', 'e'};
static const uint8_t auth_literal_unknown[7] = {0, 0, 0, 3, 'x', '-', 'm'};

static const struct {
	const char *method;
	const uint8_t *suffix;
	size_t length;
	size_t wire;
	uint8_t padding;
} auth_literal_cases[] = {
	{"none", auth_literal_none, sizeof(auth_literal_none), 48, 11},
	{"password", auth_literal_password, sizeof(auth_literal_password), 56, 10},
	{"keyboard-interactive", auth_literal_kbi, sizeof(auth_literal_kbi), 72, 11},
	{"x-m", auth_literal_unknown, sizeof(auth_literal_unknown), 40, 4},
};

static size_t
auth_literal_request(size_t which, uint8_t *packet, size_t capacity)
{
	if (which >= sizeof(auth_literal_cases) / sizeof(auth_literal_cases[0]) || packet == NULL
	    || capacity < sizeof(auth_literal_prefix)
	    || auth_literal_cases[which].length > capacity - sizeof(auth_literal_prefix))
		return 0;
	memcpy(packet, auth_literal_prefix, sizeof(auth_literal_prefix));
	memcpy(&packet[sizeof(auth_literal_prefix)], auth_literal_cases[which].suffix, auth_literal_cases[which].length);
	return sizeof(auth_literal_prefix) + auth_literal_cases[which].length;
}

static int
test_auth_state_builder(void)
{
	uint8_t guarded[130], original[130], expected[128];
	memset(original, 0xa5, sizeof(original));
	bool valid = true;
	unsigned capacities = 0, vectors = 0, refused = 0;
	for (size_t which = 0; which < sizeof(auth_literal_cases) / sizeof(auth_literal_cases[0]); which++) {
		size_t wanted = auth_literal_request(which, expected, sizeof(expected));
		if (wanted == 0) {
			valid = false;
			continue;
		}
		for (size_t capacity = 0; capacity < wanted; capacity++) {
			memcpy(guarded, original, sizeof(guarded));
			size_t length = 77;
			int ret = build_auth_state_request("u", auth_literal_cases[which].method, &guarded[1], capacity, &length);
			valid = ret == DSSH_ERROR_TOOLONG && length == 77
			    && memcmp(guarded, original, sizeof(guarded)) == 0 && valid;
			capacities++;
		}
		memcpy(guarded, original, sizeof(guarded));
		size_t length = 77;
		int ret = build_auth_state_request("u", auth_literal_cases[which].method, &guarded[1], wanted, &length);
		valid = ret == 0 && length == wanted && memcmp(&guarded[1], expected, wanted) == 0
		    && guarded[0] == 0xa5 && valid;
		for (size_t i = wanted + 1; i < sizeof(guarded); i++)
			valid = guarded[i] == 0xa5 && valid;
		vectors++;
	}
	char large[129];
	memset(large, 'a', sizeof(large) - 1);
	large[sizeof(large) - 1] = 0;
	for (unsigned which = 0; which < 6; which++) {
		memcpy(guarded, original, sizeof(guarded));
		size_t length = 77;
		const char *user = which == 0 ? NULL : which == 4 ? large : "u";
		const char *method = which == 1 ? NULL : which == 5 ? large : "none";
		uint8_t *output = which == 2 ? NULL : &guarded[1];
		size_t *extent = which == 3 ? NULL : &length;
		int ret = build_auth_state_request(user, method, output, 128, extent);
		valid = ret == (which < 4 ? DSSH_ERROR_INVALID : DSSH_ERROR_TOOLONG) && length == 77
		    && memcmp(guarded, original, sizeof(guarded)) == 0 && valid;
		refused++;
	}
	int printed = printf("auth_builder vectors=%u capacities=%u refused=%u valid=%d\n", vectors, capacities, refused, valid);
	ASSERT_TRUE(valid && vectors == 4 && capacities == 160 && refused == 6 && printed > 0);
	return TEST_PASS;
}

static bool
auth_queue_exchange(struct test_session_pair *pair, struct packet_wire_state *state, size_t which)
{
	if (which >= 6)
		return false;
	size_t role = which == 1 ? 1U : 0U;
	dssh_session sender = state->sessions[role];
	struct packet_wire_stream *stream = &state->streams[role];
	if (stream->used != stream->read)
		return false;
	stream->used = 0;
	stream->read = 0;
	uint8_t expected[128];
	size_t length;
	size_t total;
	uint8_t padding;
	if (which < 2) {
		static const uint8_t service[17] = {5, 0, 0, 0, 12, 's', 's', 'h', '-', 'u', 's', 'e', 'r', 'a', 'u', 't', 'h'};
		memcpy(expected, service, sizeof(service));
		expected[0] = which == 0 ? 5 : 6;
		length = sizeof(service);
		total = 32;
		padding = 10;
	}
	else {
		length = auth_literal_request(which - 2, expected, sizeof(expected));
		if (length == 0)
			return false;
		total = auth_literal_cases[which - 2].wire;
		padding = auth_literal_cases[which - 2].padding;
	}
	uint_fast32_t packets = atomic_load(&sender->trans.tx_since_rekey);
	uint_fast64_t bytes = atomic_load(&sender->trans.tx_bytes_since_rekey);
	if (packets == UINT_FAST32_MAX || total > UINT_FAST64_MAX - bytes || stream->writes == UINT_MAX)
		return false;
	unsigned writes = stream->writes;
	struct packet_wire_token token = {.sequence = sender->trans.tx_seq};
	int ret = which < 2 ? queue_auth_service(sender, which == 1)
	    : queue_auth_state_request(sender, "u", auth_literal_cases[which - 2].method);
	if (atomic_load(&sender->trans.tx_owner_active))
		abort();
	token.end = stream->used;
	return ret == 0 && state->valid && stream->writes == writes + 1
	    && transport_rekey_sequence(token.sequence, sender->trans.tx_seq, 1)
	    && atomic_load(&sender->trans.tx_since_rekey) == packets + 1
	    && atomic_load(&sender->trans.tx_bytes_since_rekey) == bytes + total
	    && token.end <= sizeof(stream->guarded) - 2
	    && packet_wire_valid(&stream->guarded[1], token.end, expected, length, total, padding)
	    && packet_wire_receive(pair, state, role, expected, length, &token, false);
}

static bool
auth_queue_refusals(struct test_session_pair *pair, struct packet_wire_state *state)
{
	char large[129];
	memset(large, 'a', sizeof(large) - 1);
	large[sizeof(large) - 1] = 0;
	dssh_session sender = pair->client;
	struct packet_wire_stream *stream = &state->streams[0];
	uint32_t sequence = sender->trans.tx_seq;
	uint_fast32_t packets = atomic_load(&sender->trans.tx_since_rekey);
	uint_fast64_t bytes = atomic_load(&sender->trans.tx_bytes_since_rekey);
	unsigned writes = stream->writes;
	unsigned reads = stream->reads;
	size_t used = stream->used;
	size_t read = stream->read;
	bool valid = true;
	for (unsigned which = 0; which < 6; which++) {
		int ret;
		if (which == 0)
			ret = queue_auth_service(NULL, false);
		else if (which == 1)
			ret = queue_auth_state_request(NULL, "u", "none");
		else
			ret = queue_auth_state_request(sender, which == 2 ? NULL : which == 4 ? large : "u",
			    which == 3 ? NULL : which == 5 ? large : "none");
		if (atomic_load(&sender->trans.tx_owner_active))
			abort();
		valid = ret == (which < 4 ? DSSH_ERROR_INVALID : DSSH_ERROR_TOOLONG) && state->valid
		    && sender->trans.tx_seq == sequence && atomic_load(&sender->trans.tx_since_rekey) == packets
		    && atomic_load(&sender->trans.tx_bytes_since_rekey) == bytes && stream->writes == writes
		    && stream->reads == reads && stream->used == used && stream->read == read
		    && !dssh_session_is_terminated(sender) && valid;
		if (state->refusals == UINT_MAX)
			return false;
		state->refusals++;
	}
	return valid;
}

static struct packet_wire_result
auth_queue_case(int allocation_failure)
{
	struct packet_wire_result result = {.fixture = DSSH_ERROR_INIT};
	struct test_session_pair pair;
	struct packet_wire_state state = {.valid = true};
	result.fixture = state_prepare(&pair, false);
	if (result.fixture == 0)
		result.fixture = dssh_transport_set_callbacks(packet_wire_tx, packet_wire_rx, packet_wire_line, NULL);
	if (result.fixture != 0)
		goto done;
	for (size_t role = 0; role < 2; role++)
		memset(state.streams[role].guarded, 0xa5, sizeof(state.streams[role].guarded));
	dssh_test_alloc_fail_after(allocation_failure);
	result.fixture = state_sessions(&pair, true);
	state.sessions[0] = pair.client;
	state.sessions[1] = pair.server;
	if (result.fixture == 0)
		result.fixture = dssh_session_set_cbdata(pair.client, &state, &state, &state, NULL);
	if (result.fixture == 0)
		result.fixture = dssh_session_set_cbdata(pair.server, &state, &state, &state, NULL);
	result.allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	if (result.fixture != 0)
		goto done;
	result.ran = true;
	result.valid = true;
	for (size_t which = 0; which < 6; which++)
		result.valid = auth_queue_exchange(&pair, &state, which) && result.valid;
	result.valid = auth_queue_refusals(&pair, &state) && result.valid;
done:
	state_finish(&pair);
	result.frames = state.frames;
	result.refusals = state.refusals;
	result.retired = state_retired(&pair) && state.valid;
	for (size_t role = 0; role < 2; role++) {
		struct packet_wire_stream *stream = &state.streams[role];
		unsigned writes = result.ran ? role == 0 ? 5U : 1U : 0U;
		if (stream->writes != writes || stream->reads != 2 * writes || stream->used != stream->read)
			result.retired = false;
	}
	state.sessions[0] = NULL;
	state.sessions[1] = NULL;
	result.valid = result.valid && result.retired && result.frames == 6 && result.refusals == 6;
	return result;
}

static int
test_auth_state_queue_wire(void)
{
	struct packet_wire_result result = auth_queue_case(-1);
	int printed = printf("auth_queue frames=%u refused=%u retired=%d valid=%d\n", result.frames, result.refusals, result.retired, result.valid);
	ASSERT_TRUE(result.fixture == 0 && result.ran && result.valid && printed > 0);
	return TEST_PASS;
}

static int
test_auth_state_queue_constructors(void)
{
	struct packet_wire_result baseline = auth_queue_case(INT_MAX);
	bool valid = baseline.fixture == 0 && baseline.ran && baseline.valid && baseline.allocations == 6;
	unsigned refused = 0;
	for (int i = 0; valid && i < baseline.allocations; i++) {
		struct packet_wire_result result = auth_queue_case(i);
		valid = result.fixture == DSSH_ERROR_ALLOC && !result.ran && result.retired && result.allocations == i + 1;
		if (valid)
			refused++;
	}
	struct packet_wire_result final = auth_queue_case(baseline.allocations);
	valid = final.fixture == 0 && final.ran && final.valid && final.allocations == baseline.allocations && valid;
	int printed = printf("auth_queue_constructors baseline=%d refused=%u final=%d valid=%d\n",
	    baseline.allocations, refused, final.allocations, valid);
	ASSERT_TRUE(valid && refused == 6 && printed > 0);
	return TEST_PASS;
}

#endif
