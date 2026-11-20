#ifndef DSSH_PACKET_BOUNDS_CASES_H
#define DSSH_PACKET_BOUNDS_CASES_H

static bool
packet_metadata_case(bool client, uint16_t block, size_t length)
{
	dssh_test_reset_global_config();
	struct module_fixture fixture = {0};
	active = &fixture;
	bool valid = dssh_transport_set_callbacks(memory_tx, memory_rx, memory_line, NULL) == 0;
	if (length == 0 || length > 256 || sizeof(struct dssh_enc_s) > SIZE_MAX - sizeof("bounds")) {
		active = NULL;
		dssh_test_reset_global_config();
		return false;
	}
	dssh_enc enc = calloc(1, sizeof(*enc) + sizeof("bounds"));
	size_t packet_size = UINT16_MAX;
	if (packet_size <= SIZE_MAX / 2)
		packet_size *= 2;
	else
		packet_size -= 4;
	dssh_session sender = dssh_session_init(client, packet_size);
	dssh_session receiver = dssh_session_init(!client, packet_size);
	valid = valid && enc != NULL && sender != NULL && receiver != NULL;
	int ret = DSSH_ERROR_INIT;
	int expected = DSSH_ERROR_INIT;
	if (valid) {
		memcpy(enc->name, "bounds", sizeof("bounds"));
		enc->blocksize = block;
		enc->bytes_per_key = UINT64_MAX;
		enc->encrypt = stateless_encrypt;
		enc->decrypt = stateless_decrypt;
		sender->trans.enc_c2s_active = enc;
		sender->trans.enc_s2c_active = enc;
		receiver->trans.enc_c2s_active = enc;
		receiver->trans.enc_s2c_active = enc;
		uint8_t payload[256];
		memset(payload, 0x33, length);
		payload[0] = SSH_MSG_CHANNEL_DATA;
		size_t alignment = block < 8 ? 8 : block;
		/* Test arithmetic uses uint64_t independently of the packet engine. */
		uint64_t overhead_payload = 5 + length;
		uint64_t pad = alignment - overhead_payload % alignment;
		if (pad < 4)
			pad += alignment;
		uint64_t total = overhead_payload + pad;
		if (length > tx_payload_capacity(sender))
			expected = DSSH_ERROR_TOOLONG;
		else
			expected = pad > UINT8_MAX ? DSSH_ERROR_INVALID : 0;
		ret = send_packet(sender, payload, length, NULL);
		valid = ret == expected;
		if (expected != 0)
			valid = valid && fixture.encrypts == 0 && fixture.writes == 0 && sender->trans.tx_seq == 0;
		else {
			valid = valid && fixture.encrypts == 1 && fixture.writes == 1 && fixture.used == total;
			if (valid) {
				uint8_t type;
				uint8_t *received = NULL;
				size_t received_length = 0;
				ret = recv_packet(receiver, &type, &received, &received_length);
				valid = ret == 0 && type == payload[0] && received_length == length && received != NULL
				    && memcmp(received, payload, length) == 0;
			}
		}
	}
	dssh_session_cleanup(receiver);
	dssh_session_cleanup(sender);
	free(enc);
	active = NULL;
	dssh_test_reset_global_config();
	unsigned block_print = block;
	if (!valid && fprintf(stderr, "packet bounds: client=%d block=%u length=%zu result=%d expected=%d encrypts=%u writes=%u\n",
		    client, block_print, length, ret, expected, fixture.encrypts, fixture.writes) < 0)
		return false;
	return valid;
}

static int
test_packet_metadata_bounds(void)
{
	const uint16_t blocks[] = { 1, 8, 16, 252, 253, 254, 255, 256, 257, 512, UINT16_MAX };
	const size_t lengths[] = { 1, 31, 245, 246, 247, 248, 256 };
	bool valid = true;
	for (unsigned role = 0; role < 2; role++) {
		for (size_t b = 0; b < sizeof(blocks) / sizeof(blocks[0]); b++) {
			for (size_t n = 0; n < sizeof(lengths) / sizeof(lengths[0]); n++) {
				bool matched = packet_metadata_case(role != 0, blocks[b], lengths[n]);
				valid = valid && matched;
			}
		}
	}
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_receive_block_bounds(void)
{
	const uint16_t blocks[] = { 16, 33280, 33281, UINT16_MAX };
	for (unsigned role = 0; role < 2; role++) {
		for (size_t i = 0; i < sizeof(blocks) / sizeof(blocks[0]); i++) {
			dssh_test_reset_global_config();
			struct module_fixture fixture = {0};
			active = &fixture;
			ASSERT_EQ(dssh_transport_set_callbacks(memory_tx, memory_rx, memory_line, NULL), 0);
			dssh_session sess = dssh_session_init(role != 0, 0);
			ASSERT_NOT_NULL(sess);
			struct dssh_enc_s enc = { .blocksize = blocks[i], .decrypt = stateless_decrypt };
			sess->trans.enc_c2s_active = &enc;
			sess->trans.enc_s2c_active = &enc;
			bool fits = blocks[i] <= sess->trans.packet_buf_sz;
			uint8_t type;
			uint8_t *payload;
			size_t length;
			int ret = recv_packet(sess, &type, &payload, &length);
			bool valid = ret == (fits ? DSSH_ERROR_INIT : DSSH_ERROR_INVALID)
			    && fixture.rx_calls == (fits ? 1U : 0U) && fixture.decrypts == 0;
			dssh_session_cleanup(sess);
			active = NULL;
			dssh_test_reset_global_config();
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static int
test_slot_size_bounds(void)
{
	ASSERT_EQ_U(tx_slot_buf_size(1, 16, 16), 36);
	ASSERT_EQ_U(tx_slot_buf_size(9, 16, 64), 100);
	const struct {
		size_t payload;
		size_t block;
		uint16_t mac;
	} invalid[] = {
		{ SIZE_MAX, 16, 16 }, { SIZE_MAX - 4, 16, 0 },
		{ SIZE_MAX - 5, 16, 0 }, { 1, SIZE_MAX, 0 },
		{ SIZE_MAX - 8, SIZE_MAX, UINT16_MAX }, { 245, 253, 16 },
		{ 1, 512, 0 },
#if SIZE_MAX > UINT32_MAX
		{ UINT32_MAX, 16, 0 },
#endif
#if SIZE_MAX == UINT16_MAX
		{ 1, 16, UINT16_MAX },
#endif
	};
	for (unsigned reuse = 0; reuse < 2; reuse++) {
		for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
			struct dssh_tx_slot slot = {0};
			atomic_init(&slot.ready, false);
			if (reuse != 0) {
				int initialized = alloc_tx_slot(&slot, 1, 16, 16);
				if (initialized != 0 || slot.buf == NULL || slot.buf_sz != 36) {
					free_tx_slot(&slot);
					dssh_test_failure_cleanup();
					return TEST_FAIL;
				}
				memset(slot.buf, 0xa5, slot.buf_sz);
				slot.payload_len = 1;
				atomic_store(&slot.ready, true);
			}
			uint8_t *old = slot.buf;
			size_t capacity = slot.buf_sz;
			dssh_test_alloc_watch_free(old, capacity);
			dssh_test_alloc_fail_after(0);
			size_t size = tx_slot_buf_size(invalid[i].payload, invalid[i].block, invalid[i].mac);
			int ret = alloc_tx_slot(&slot, invalid[i].payload, invalid[i].block, invalid[i].mac);
			int allocations = dssh_test_alloc_count();
			bool released = dssh_test_alloc_watched_free(NULL);
			dssh_test_alloc_reset();
			bool valid = !released && slot.buf_sz == capacity;
			if (valid)
				valid = slot.buf == old;
			if (reuse != 0)
				valid = valid && atomic_load(&slot.ready) && slot.payload_len == 1
				    && mac_bytes_equal(slot.buf, capacity, 0xa5);
			free_tx_slot(&slot);
			ASSERT_EQ_U(size, 0);
			ASSERT_EQ(ret, DSSH_ERROR_INVALID);
			ASSERT_EQ(allocations, 0);
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

struct packet_input_fixture {
	uint8_t *packet;
	size_t packet_total;
	size_t read;
	size_t tag_read;
	size_t mac_input;
	uint16_t tag_size;
	uint32_t sequence;
	unsigned rx_calls;
	unsigned mac_calls;
};

/* These isolated raw receive cases have one caller and no library worker. */
static struct packet_input_fixture *packet_input;

static int
packet_input_tx(uint8_t *buf, size_t length, dssh_session sess, void *data)
{
	(void)buf;
	(void)length;
	(void)sess;
	(void)data;
	return DSSH_ERROR_INVALID;
}

static int
packet_input_rx(uint8_t *buf, size_t length, dssh_session sess, void *data)
{
	(void)sess;
	struct packet_input_fixture *input = data;
	if (input == NULL || input->read > input->packet_total || input->rx_calls == UINT_MAX
	    || (buf == NULL && length != 0) || (input->packet == NULL && input->packet_total != 0))
		return DSSH_ERROR_INVALID;
	input->rx_calls++;
	if (input->read < input->packet_total) {
		if (length > input->packet_total - input->read)
			return DSSH_ERROR_INIT;
		if (length != 0)
			memcpy(buf, &input->packet[input->read], length);
		input->read += length;
		return 0;
	}
	if (input->tag_read > input->tag_size || length > input->tag_size - input->tag_read)
		return DSSH_ERROR_INIT;
	if (length != 0)
		memset(buf, 0, length);
	input->tag_read += length;
	return 0;
}

static int
packet_input_mac(const uint8_t *buf, size_t length, uint8_t *out, dssh_mac_ctx *ctx)
{
	if (packet_input == NULL || ctx != NULL || buf == NULL || out == NULL
	    || packet_input->packet_total > SIZE_MAX - 4 || packet_input->mac_calls == UINT_MAX
	    || length != packet_input->packet_total + 4
	    || DSSH_GET_U32(buf) != packet_input->sequence
	    || memcmp(&buf[4], packet_input->packet, packet_input->packet_total) != 0)
		return DSSH_ERROR_INVALID;
	packet_input->mac_input = length;
	packet_input->mac_calls++;
	memset(out, 0, packet_input->tag_size);
	return 0;
}

static int
test_packet_receive_extents(void)
{
	const size_t total = UINT16_MAX - 7U;
	uint8_t *packet = malloc(total);
	if (packet == NULL)
		return TEST_FAIL;
	memset(packet, 0x53, total);
	size_t pos = 0;
	DSSH_PUT_U32(UINT32_C(65524), packet, &pos);
	packet[4] = 4;
	packet[5] = SSH_MSG_CHANNEL_DATA;
	const uint16_t tags[] = {128, UINT16_MAX};
	bool valid = true;
	unsigned packets = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (size_t t = 0; t < sizeof(tags) / sizeof(tags[0]); t++) {
			dssh_test_reset_global_config();
			struct packet_input_fixture input = {.packet = packet, .packet_total = total, .tag_size = tags[t]};
			packet_input = &input;
			int ret = dssh_transport_set_callbacks(packet_input_tx, packet_input_rx, NULL, NULL);
			dssh_session sess = ret == 0 ? dssh_session_init(role != 0, UINT16_MAX - 4U) : NULL;
			if (sess == NULL) {
				valid = false;
				packet_input = NULL;
				continue;
			}
			struct dssh_mac_s mac = {.generate = packet_input_mac, .digest_size = tags[t]};
			sess->trans.mac_c2s_active = &mac;
			sess->trans.mac_s2c_active = &mac;
			ret = dssh_session_set_cbdata(sess, NULL, &input, NULL, NULL);
			if (ret == 0)
				ret = reserve_mac_buffers(sess, tags[t]);
			uint8_t *received_storage = sess->trans.rx_mac_buf;
			uint8_t *computed_storage = sess->trans.rx_mac_computed;
			valid = valid && ret == 0;
			for (unsigned near_limit = 0; near_limit < 2 && ret == 0; near_limit++) {
				input.read = 0;
				input.tag_read = 0;
				input.rx_calls = 0;
				input.mac_calls = 0;
				input.mac_input = 0;
				input.sequence = sess->trans.rx_seq;
				sess->trans.rx_bytes_since_rekey = near_limit != 0 ? UINT64_MAX - 1 : 0;
				uint8_t type = 0;
				uint8_t *payload = NULL;
				size_t length = 0;
				dssh_test_alloc_fail_after(0);
				ret = recv_packet_raw(sess, &type, &payload, &length);
				int allocations = dssh_test_alloc_count();
				dssh_test_alloc_reset();
				uint64_t expected_bytes = UINT64_C(65528) + tags[t];
				if (near_limit != 0)
					expected_bytes = UINT64_MAX;
				valid = valid && ret == 0 && allocations == 0 && type == SSH_MSG_CHANNEL_DATA
				    && length == total - 9 && payload != NULL && memcmp(payload, &packet[5], length) == 0
				    && input.read == total && input.tag_read == tags[t] && input.rx_calls == 3
				    && input.mac_calls == 1 && input.mac_input == total + 4
				    && sess->trans.rx_bytes_since_rekey == expected_bytes
				    && sess->trans.rx_mac_buf == received_storage && sess->trans.rx_mac_computed == computed_storage;
				packets++;
			}
			dssh_session_cleanup(sess);
			packet_input = NULL;
		}
	}
	packet_input = NULL;
	free(packet);
	dssh_test_reset_global_config();
	if (printf("packet_receive_extents packets=%u payload=65519 mac_input=65532 tags=128/65535 valid=%u\n",
	    packets, valid ? 1U : 0U) < 0 || fflush(stdout) != 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid && packets == 8);
	return TEST_PASS;
}

static int
test_packet_receive_length_refusal(void)
{
	const uint32_t lengths[] = {33277, UINT16_MAX, UINT32_C(65536), UINT32_C(65548), UINT32_MAX};
	bool valid = true;
	unsigned refusals = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (size_t i = 0; i < sizeof(lengths) / sizeof(lengths[0]); i++) {
			dssh_test_reset_global_config();
			uint8_t packet[8] = {0};
			size_t pos = 0;
			DSSH_PUT_U32(lengths[i], packet, &pos);
			packet[4] = 4;
			struct packet_input_fixture input = {.packet = packet, .packet_total = sizeof(packet)};
			int ret = dssh_transport_set_callbacks(packet_input_tx, packet_input_rx, NULL, NULL);
			dssh_session sess = ret == 0 ? dssh_session_init(role != 0, 0) : NULL;
			if (sess == NULL) {
				valid = false;
				continue;
			}
			ret = dssh_session_set_cbdata(sess, NULL, &input, NULL, NULL);
			uint8_t type = 0x5a;
			uint8_t *payload = NULL;
			size_t length = 17;
			if (ret == 0)
				ret = recv_packet_raw(sess, &type, &payload, &length);
			valid = valid && ret == DSSH_ERROR_TOOLONG && input.rx_calls == 1 && input.read == 8
			    && input.tag_read == 0 && type == 0x5a && payload == NULL && length == 17
			    && dssh_session_is_terminated(sess);
			refusals++;
			dssh_session_cleanup(sess);
		}
	}
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid && refusals == 10);
	return TEST_PASS;
}

#endif
