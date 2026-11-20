#ifndef DSSH_TEST_PACKET_WIRE_CASES_H
#define DSSH_TEST_PACKET_WIRE_CASES_H

struct packet_wire_stream {
	uint8_t guarded[258];
	size_t used;
	size_t read;
	unsigned writes;
	unsigned reads;
	size_t last_reads[2];
};

struct packet_wire_state {
	struct packet_wire_stream streams[2];
	dssh_session sessions[2];
	unsigned frames;
	unsigned refusals;
	bool valid;
};

static struct packet_wire_stream *
packet_wire_stream_for(struct packet_wire_state *state, dssh_session sess, bool transmit)
{
	if (state == NULL || sess == NULL)
		return NULL;
	for (size_t role = 0; role < 2; role++) {
		if (sess == state->sessions[role] && sess->trans.client == (role == 0))
			return &state->streams[transmit ? role : 1 - role];
	}
	state->valid = false;
	return NULL;
}

static int
packet_wire_tx(uint8_t *buf, size_t length, dssh_session sess, void *opaque)
{
	struct packet_wire_state *state = opaque;
	struct packet_wire_stream *stream = packet_wire_stream_for(state, sess, true);
	if (stream == NULL)
		return DSSH_ERROR_INVALID;
	size_t capacity = sizeof(stream->guarded) - 2;
	if (buf == NULL || stream->used > capacity || length > capacity - stream->used || stream->writes == UINT_MAX) {
		state->valid = false;
		return DSSH_ERROR_TOOLONG;
	}
	memcpy(&stream->guarded[1 + stream->used], buf, length);
	stream->used += length;
	stream->writes++;
	return 0;
}

static int
packet_wire_rx(uint8_t *buf, size_t length, dssh_session sess, void *opaque)
{
	struct packet_wire_state *state = opaque;
	struct packet_wire_stream *stream = packet_wire_stream_for(state, sess, false);
	if (stream == NULL)
		return DSSH_ERROR_INVALID;
	if (buf == NULL || stream->read > stream->used || length > stream->used - stream->read || stream->reads == UINT_MAX) {
		state->valid = false;
		return DSSH_ERROR_PARSE;
	}
	memcpy(buf, &stream->guarded[1 + stream->read], length);
	stream->read += length;
	stream->last_reads[stream->reads % 2] = length;
	stream->reads++;
	return 0;
}

static int
packet_wire_line(uint8_t *buf, size_t length, size_t *received, dssh_session sess, void *opaque)
{
	(void)buf;
	(void)length;
	(void)sess;
	struct packet_wire_state *state = opaque;
	if (state != NULL)
		state->valid = false;
	if (received != NULL)
		*received = 0;
	return DSSH_ERROR_INIT;
}

/* Inspect only a complete captured extent; the independent literal size/padding
 * answers come from the caller, rather than the packet engine's helpers. */
static bool
packet_wire_valid(const uint8_t *wire, size_t available, const uint8_t *payload,
    size_t length, size_t expected_total, uint8_t expected_padding)
{
	if (wire == NULL || payload == NULL || length == 0 || available != expected_total || available < 5)
		return false;
	uint32_t declared = 0;
	int64_t parsed = dssh_parse_uint32(wire, available, &declared);
	if (parsed != 4 || declared < 2)
		return false;
#if SIZE_MAX <= UINT32_MAX
	if (declared > SIZE_MAX - 4)
		return false;
#endif
	size_t packet = (size_t)declared;
	size_t total = 4 + packet;
	size_t padding = wire[4];
	if (total != available || total % 8 != 0 || padding < 4 || padding >= packet
	    || wire[4] != expected_padding || packet - 1 - padding != length || length > available - 5)
		return false;
	return memcmp(&wire[5], payload, length) == 0;
}

struct packet_wire_token {
	size_t start;
	size_t end;
	uint32_t sequence;
};

static bool
packet_wire_send(struct test_session_pair *pair, struct packet_wire_state *state, size_t role,
    const uint8_t *payload, size_t length, size_t total, uint8_t padding, struct packet_wire_token *token)
{
	if (role >= 2)
		return false;
	dssh_session sender = role == 0 ? pair->client : pair->server;
	struct packet_wire_stream *stream = &state->streams[role];
	if (stream->read == stream->used) {
		stream->read = 0;
		stream->used = 0;
	}
	uint_fast32_t packets = atomic_load(&sender->trans.tx_since_rekey);
	uint_fast64_t bytes = atomic_load(&sender->trans.tx_bytes_since_rekey);
	if (packets == UINT_FAST32_MAX || total > UINT_FAST64_MAX - bytes || stream->writes == UINT_MAX)
		return false;
	unsigned writes = stream->writes;
	token->start = stream->used;
	token->sequence = sender->trans.tx_seq;
	uint32_t reported = token->sequence == UINT32_MAX ? 0 : UINT32_MAX;
	int ret = send_packet(sender, payload, length, &reported);
	/* This fixture has no other TX owner. Returning with an uncertain engine
	 * reservation cannot safely proceed into another send or disposal. */
	if (atomic_load(&sender->trans.tx_owner_active))
		abort();
	token->end = stream->used;
	return ret == 0 && state->valid && reported == token->sequence
	    && transport_rekey_sequence(token->sequence, sender->trans.tx_seq, 1)
	    && atomic_load(&sender->trans.tx_since_rekey) == packets + 1
	    && atomic_load(&sender->trans.tx_bytes_since_rekey) == bytes + total
	    && stream->writes == writes + 1 && token->end >= token->start
	    && token->end <= sizeof(stream->guarded) - 2
	    && packet_wire_valid(&stream->guarded[1 + token->start], token->end - token->start,
	        payload, length, total, padding)
	    && stream->guarded[0] == 0xa5 && stream->guarded[sizeof(stream->guarded) - 1] == 0xa5
	    && !dssh_session_is_terminated(sender);
}

static bool
packet_wire_receive(struct test_session_pair *pair, struct packet_wire_state *state, size_t role,
    const uint8_t *expected, size_t length, const struct packet_wire_token *token, bool raw)
{
	if (role >= 2 || token->end < token->start || token->end - token->start <= 8)
		return false;
	dssh_session receiver = role == 0 ? pair->server : pair->client;
	struct packet_wire_stream *stream = &state->streams[role];
	uint32_t packets = receiver->trans.rx_since_rekey;
	uint64_t bytes = receiver->trans.rx_bytes_since_rekey;
	size_t total = token->end - token->start;
	if (stream->read != token->start || receiver->trans.rx_seq != token->sequence || packets == UINT32_MAX
	    || total > UINT64_MAX - bytes || stream->reads > UINT_MAX - 2 || state->frames == UINT_MAX)
		return false;
	unsigned reads = stream->reads;
	uint8_t type = 0;
	uint8_t *payload = NULL;
	size_t received = 0;
	int ret = raw ? recv_packet_raw(receiver, &type, &payload, &received)
	    : recv_packet(receiver, &type, &payload, &received);
	bool valid = ret == 0 && state->valid && type == expected[0] && payload == &receiver->trans.rx_packet[9]
	    && received == length && memcmp(payload, expected, length) == 0
	    && stream->read == token->end && stream->reads == reads + 2
	    && stream->last_reads[0] == 8 && stream->last_reads[1] == total - 8
	    && receiver->trans.last_rx_seq == token->sequence
	    && transport_rekey_sequence(token->sequence, receiver->trans.rx_seq, 1)
	    && receiver->trans.rx_since_rekey == packets + 1 && receiver->trans.rx_bytes_since_rekey == bytes + total
	    && !dssh_session_is_terminated(receiver);
	if (valid)
		state->frames++;
	return valid;
}

static bool
packet_wire_exchange(struct test_session_pair *pair, struct packet_wire_state *state, size_t role,
    const uint8_t *payload, size_t length, size_t total, uint8_t padding, bool raw)
{
	struct packet_wire_token token;
	return packet_wire_send(pair, state, role, payload, length, total, padding, &token)
	    && packet_wire_receive(pair, state, role, payload, length, &token, raw);
}

static bool
packet_wire_refusal(struct test_session_pair *pair, struct packet_wire_state *state, size_t role,
    uint8_t *payload, size_t length)
{
	dssh_session sender = role == 0 ? pair->client : pair->server;
	struct packet_wire_stream *stream = &state->streams[role];
	if (sender->trans.packet_buf_sz > SIZE_MAX - 4 || state->refusals == UINT_MAX)
		return false;
	size_t extent = 4 + sender->trans.packet_buf_sz;
	memset(sender->trans.tx_packet, 0xa5, extent);
	uint32_t before = sender->trans.tx_seq;
	uint_fast32_t packets = atomic_load(&sender->trans.tx_since_rekey);
	uint_fast64_t bytes = atomic_load(&sender->trans.tx_bytes_since_rekey);
	unsigned writes = stream->writes;
	size_t used = stream->used;
	uint32_t reported = UINT32_MAX;
	int ret = send_packet(sender, payload, length, &reported);
	if (atomic_load(&sender->trans.tx_owner_active))
		abort();
	bool valid = ret == DSSH_ERROR_TOOLONG && reported == UINT32_MAX && state->valid
	    && sender->trans.tx_seq == before && atomic_load(&sender->trans.tx_since_rekey) == packets
	    && atomic_load(&sender->trans.tx_bytes_since_rekey) == bytes && stream->writes == writes
	    && stream->used == used && !dssh_session_is_terminated(sender);
	for (size_t i = 0; i < extent; i++)
		valid = sender->trans.tx_packet[i] == 0xa5 && valid;
	if (valid)
		state->refusals++;
	return valid;
}

struct packet_wire_result {
	int fixture;
	int allocations;
	unsigned frames;
	unsigned refusals;
	bool ran;
	bool valid;
	bool retired;
};

static struct packet_wire_result
packet_wire_case(unsigned kind, int allocation_failure)
{
	struct packet_wire_result result = {.fixture = DSSH_ERROR_INVALID};
	if (kind >= 8)
		return result;
	struct test_session_pair pair;
	struct packet_wire_state state = {.valid = true};
	uint8_t *large = NULL;
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
	if (result.fixture == 0 && kind == 5) {
		if (pair.client->trans.packet_buf_sz != pair.server->trans.packet_buf_sz
		    || pair.client->trans.packet_buf_sz == SIZE_MAX)
			result.fixture = DSSH_ERROR_INVALID;
		else {
			size_t capacity = pair.client->trans.packet_buf_sz + 1;
			large = dssh_test_malloc(capacity);
			if (large == NULL)
				result.fixture = DSSH_ERROR_ALLOC;
			else {
				memset(large, 0x42, capacity);
				large[0] = SSH_MSG_SERVICE_REQUEST;
			}
		}
	}
	result.allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	if (result.fixture != 0)
		goto done;
	result.ran = true;
	result.valid = true;
	static const size_t totals[16] = {16, 16, 16, 16, 16, 16, 16, 24, 24, 24, 24, 24, 24, 24, 24, 32};
	static const uint8_t pads[16] = {10, 9, 8, 7, 6, 5, 4, 11, 10, 9, 8, 7, 6, 5, 4, 11};
	uint8_t payload[16];
	memset(payload, 0x5a, sizeof(payload));
	for (size_t role = 0; role < 2; role++) {
		payload[0] = role == 0 ? SSH_MSG_SERVICE_REQUEST : SSH_MSG_SERVICE_ACCEPT;
		if (kind == 0) {
			static const uint8_t ignored[] = {SSH_MSG_IGNORE, 0, 0, 0, 3, 'a', 'b', 'c'};
			result.valid = packet_wire_exchange(&pair, &state, role, ignored, sizeof(ignored), 24, 11, true)
			    && packet_wire_exchange(&pair, &state, role, payload, 3, 16, 8, false) && result.valid;
		}
		else if (kind == 1 || kind == 2) {
			size_t count = kind == 1 ? 16 : 8;
			for (size_t i = 0; i < count; i++)
				result.valid = packet_wire_exchange(&pair, &state, role, payload, i + 1, totals[i], pads[i], false) && result.valid;
		}
		else if (kind == 3) {
			for (unsigned phase = 0; phase < 2; phase++) {
				if (phase != 0) {
					dssh_session sender = state.sessions[role];
					dssh_session receiver = state.sessions[1 - role];
					sender->trans.tx_seq = UINT32_MAX - 1;
					receiver->trans.rx_seq = UINT32_MAX - 1;
				}
				for (unsigned i = 0; i < 3; i++)
					result.valid = packet_wire_exchange(&pair, &state, role, payload, 1, 16, 10, false) && result.valid;
			}
		}
		else if (kind == 4) {
			static const uint8_t first[] = {SSH_MSG_SERVICE_REQUEST, 1};
			static const uint8_t second[] = {SSH_MSG_SERVICE_ACCEPT, 2};
			struct packet_wire_token one, two;
			result.valid = packet_wire_send(&pair, &state, role, first, sizeof(first), 16, 9, &one)
			    && packet_wire_send(&pair, &state, role, second, sizeof(second), 16, 9, &two)
			    && packet_wire_receive(&pair, &state, role, first, sizeof(first), &one, false)
			    && packet_wire_receive(&pair, &state, role, second, sizeof(second), &two, false) && result.valid;
		}
		else if (kind == 5) {
			dssh_session sender = state.sessions[role];
			result.valid = packet_wire_exchange(&pair, &state, role, payload, 3, 16, 8, false) && result.valid;
			size_t capacity = tx_payload_capacity(sender);
			if (capacity >= sender->trans.packet_buf_sz)
				result.valid = false;
			else {
				size_t lengths[] = {capacity + 1, sender->trans.packet_buf_sz + 1};
				for (size_t i = 0; i < 2; i++)
					result.valid = packet_wire_refusal(&pair, &state, role, large, lengths[i])
					    && packet_wire_exchange(&pair, &state, role, payload, 3, 16, 8, false) && result.valid;
			}
		}
		else if (kind == 7)
			result.valid = packet_wire_exchange(&pair, &state, role, payload, 1, 16, 10, false) && result.valid;
	}
	if (kind == 6) {
		for (unsigned round = 0; round < 6; round++) {
			for (size_t role = 0; role < 2; role++) {
				payload[0] = role == 0 ? SSH_MSG_SERVICE_REQUEST : SSH_MSG_SERVICE_ACCEPT;
				result.valid = packet_wire_exchange(&pair, &state, role, payload, 2, 16, 9, false) && result.valid;
			}
		}
	}
	for (size_t role = 0; role < 2; role++) {
		struct packet_wire_stream *stream = &state.streams[role];
		result.valid = stream->read == stream->used && stream->guarded[0] == 0xa5
		    && stream->guarded[sizeof(stream->guarded) - 1] == 0xa5 && state.valid && result.valid;
	}
done:
	state_finish(&pair);
	dssh_test_free(large);
	state.sessions[0] = NULL;
	state.sessions[1] = NULL;
	result.frames = state.frames;
	result.refusals = state.refusals;
	result.retired = state_retired(&pair);
	result.valid = result.valid && result.retired;
	return result;
}

static bool
packet_wire_report(unsigned kind)
{
	static const unsigned frames[] = {4, 32, 16, 12, 4, 6, 12, 2};
	if (kind >= sizeof(frames) / sizeof(frames[0]))
		return false;
	struct packet_wire_result result = packet_wire_case(kind, -1);
	bool valid = result.fixture == 0 && result.ran && result.valid && result.frames == frames[kind]
	    && result.refusals == (kind == 5 ? 4U : 0U);
	int printed = printf("packet_wire kind=%u frames=%u refused=%u retired=%d valid=%d\n",
	    kind, result.frames, result.refusals, result.retired, valid);
	return valid && printed > 0;
}

static int
test_packet_roundtrip(void)
{
	ASSERT_TRUE(packet_wire_report(0));
	return TEST_PASS;
}

static int
test_packet_alignment(void)
{
	ASSERT_TRUE(packet_wire_report(1));
	return TEST_PASS;
}

static int
test_packet_min_padding(void)
{
	ASSERT_TRUE(packet_wire_report(2));
	return TEST_PASS;
}

static int
test_packet_seq_increment(void)
{
	ASSERT_TRUE(packet_wire_report(3));
	return TEST_PASS;
}

static int
test_packet_server_receives_client(void)
{
	ASSERT_TRUE(packet_wire_report(4));
	return TEST_PASS;
}

static int
test_packet_too_large(void)
{
	ASSERT_TRUE(packet_wire_report(5));
	return TEST_PASS;
}

static int
test_packet_bidirectional(void)
{
	ASSERT_TRUE(packet_wire_report(6));
	return TEST_PASS;
}

static int
test_packet_minimum_payload(void)
{
	ASSERT_TRUE(packet_wire_report(7));
	return TEST_PASS;
}

static int
test_packet_wire_oracle(void)
{
	static const uint8_t expected[17] = {0, 0, 0, 12, 4, SSH_MSG_SERVICE_REQUEST, 1, 2, 3, 4, 5, 6};
	static const uint8_t payload[] = {SSH_MSG_SERVICE_REQUEST, 1, 2, 3, 4, 5, 6};
	uint8_t wire[sizeof(expected)];
	memcpy(wire, expected, sizeof(wire));
	bool valid = packet_wire_valid(wire, 16, payload, sizeof(payload), 16, 4);
	unsigned checks = 1;
	for (size_t length = 0; length < 16; length++) {
		valid = !packet_wire_valid(wire, length, payload, sizeof(payload), 16, 4) && valid;
		checks++;
	}
	valid = !packet_wire_valid(wire, sizeof(wire), payload, sizeof(payload), 16, 4) && valid;
	checks++;
	static const uint8_t lengths[][4] = {{0, 0, 0, 1}, {255, 255, 255, 255}};
	for (size_t i = 0; i < 2; i++) {
		memcpy(wire, lengths[i], 4);
		valid = !packet_wire_valid(wire, 16, payload, sizeof(payload), 16, 4) && valid;
		checks++;
	}
	memcpy(wire, expected, sizeof(wire));
	static const uint8_t pads[] = {3, 11, 12};
	for (size_t i = 0; i < sizeof(pads) / sizeof(pads[0]); i++) {
		wire[4] = pads[i];
		valid = !packet_wire_valid(wire, 16, payload, sizeof(payload), 16, 4) && valid;
		checks++;
	}
	memcpy(wire, expected, sizeof(wire));
	wire[6] = 42;
	valid = !packet_wire_valid(wire, 16, payload, sizeof(payload), 16, 4)
	    && !packet_wire_valid(NULL, 16, payload, sizeof(payload), 16, 4)
	    && !packet_wire_valid(wire, 16, NULL, sizeof(payload), 16, 4) && valid;
	checks += 3;
	int printed = printf("packet_wire_oracle checks=%u valid=%d\n", checks, valid);
	ASSERT_TRUE(valid && checks == 26 && printed > 0);
	return TEST_PASS;
}

static int
test_packet_wire_constructor_refusals(void)
{
	static const unsigned kinds[] = {0, 5};
	bool valid = true;
	unsigned refused = 0, baselines = 0, finals = 0;
	for (size_t k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++) {
		struct packet_wire_result baseline = packet_wire_case(kinds[k], INT_MAX);
		valid = baseline.fixture == 0 && baseline.ran && baseline.valid
		    && baseline.allocations == (kinds[k] == 0 ? 6 : 7) && valid;
		baselines++;
		for (int i = 0; valid && i < baseline.allocations; i++) {
			struct packet_wire_result result = packet_wire_case(kinds[k], i);
			valid = result.fixture == DSSH_ERROR_ALLOC && !result.ran && result.retired && result.allocations == i + 1;
			if (valid)
				refused++;
		}
		struct packet_wire_result final = packet_wire_case(kinds[k], baseline.allocations);
		valid = final.fixture == 0 && final.ran && final.valid && final.allocations == baseline.allocations && valid;
		finals++;
	}
	int printed = printf("packet_wire_constructors baselines=%u refused=%u final=%u valid=%d\n", baselines, refused, finals, valid);
	ASSERT_TRUE(valid && baselines == 2 && refused == 13 && finals == 2 && printed > 0);
	return TEST_PASS;
}

#endif
