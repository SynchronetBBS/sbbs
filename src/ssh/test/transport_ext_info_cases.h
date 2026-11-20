/* RFC 8308 wire framing, original-exchange positions and real encrypted peers.
 * Parser-only models are separate from handshake/rekey/production-peer proof. */
static size_t
ext_info_packet(uint8_t *buf, size_t capacity, const char *name, const uint8_t *value, size_t length)
{
	size_t name_length = strlen(name);
	if (capacity < 5 || name_length > UINT32_MAX || length > UINT32_MAX)
		return 0;
	uint32_t name_wire = (uint32_t)name_length;
	uint32_t value_wire = (uint32_t)length;
	size_t pos = 0;
	buf[pos++] = SSH_MSG_EXT_INFO;
	if (dssh_serialize_uint32(1, buf, capacity, &pos) < 0
	    || dssh_serialize_uint32(name_wire, buf, capacity, &pos) < 0
	    || !dssh_buffer_has(capacity, pos, name_length))
		return 0;
	memcpy(&buf[pos], name, name_length);
	pos += name_length;
	if (dssh_serialize_uint32(value_wire, buf, capacity, &pos) < 0
	    || !dssh_buffer_has(capacity, pos, length))
		return 0;
	if (length != 0)
		memcpy(&buf[pos], value, length);
	return pos + length;
}

static int
test_ext_info_codec(void)
{
	const uint8_t empty[] = { 7, 0, 0, 0, 0 };
	ASSERT_OK(parse_ext_info(empty, sizeof(empty), true));
	ASSERT_OK(parse_ext_info(empty, sizeof(empty), false));
	const uint8_t binary[] = { 0, 255, 0, 128, 0 };
	uint8_t packet[512];
	size_t length = ext_info_packet(packet, sizeof(packet), "future", binary, sizeof(binary));
	ASSERT_TRUE(length > 0);
	ASSERT_OK(parse_ext_info(packet, length, true));
	ASSERT_OK(parse_ext_info(packet, length, false));
	size_t empty_length = ext_info_packet(packet, sizeof(packet), "future", NULL, 0);
	ASSERT_TRUE(empty_length > 0);
	ASSERT_OK(parse_ext_info(packet, empty_length, true));
	length = ext_info_packet(packet, sizeof(packet), "future", binary, sizeof(binary));
	for (size_t i = 0; i < length; i++)
		ASSERT_EQ(parse_ext_info(packet, i, true), DSSH_ERROR_PARSE);
	packet[length] = 0;
	ASSERT_EQ(parse_ext_info(packet, length + 1, true), DSSH_ERROR_PARSE);
	packet[1] = 255;
	ASSERT_EQ(parse_ext_info(packet, length, true), DSSH_ERROR_PARSE);
	packet[1] = 0;
	packet[0] = 8;
	ASSERT_EQ(parse_ext_info(packet, length, true), DSSH_ERROR_PARSE);
	const char *bad_names[] = { "", ",", "bad,name", "bad name", "bad\177" };
	for (size_t i = 0; i < sizeof(bad_names) / sizeof(bad_names[0]); i++) {
		length = ext_info_packet(packet, sizeof(packet), bad_names[i], binary, sizeof(binary));
		ASSERT_TRUE(length > 0);
		ASSERT_EQ(parse_ext_info(packet, length, true), DSSH_ERROR_PARSE);
	}
	length = ext_info_packet(packet, sizeof(packet), "future", binary, sizeof(binary));
	packet[10] = 0;
	ASSERT_EQ(parse_ext_info(packet, length, true), DSSH_ERROR_PARSE);
	const char *lists[] = { "", "rsa-sha2-256,ssh-ed25519", "rsa-sha2-256,rsa-sha2-256",
	    ",a", "a,", "a,,b", "a b", "a\177" };
	for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++) {
		const uint8_t *value = (const uint8_t *)lists[i];
		length = ext_info_packet(packet, sizeof(packet), "server-sig-algs", value, strlen(lists[i]));
		ASSERT_TRUE(length > 0);
		ASSERT_EQ(parse_ext_info(packet, length, true), i < 3 ? 0 : DSSH_ERROR_PARSE);
		/* A server is allowed to ignore this client-sent extension. */
		ASSERT_OK(parse_ext_info(packet, length, false));
	}
	char long_name[66];
	memset(long_name, 'a', sizeof(long_name) - 1);
	long_name[sizeof(long_name) - 1] = 0;
	length = ext_info_packet(packet, sizeof(packet), long_name, binary, sizeof(binary));
	ASSERT_EQ(parse_ext_info(packet, length, true), DSSH_ERROR_PARSE);
	const uint8_t *long_value = (const uint8_t *)long_name;
	length = ext_info_packet(packet, sizeof(packet), "server-sig-algs", long_value, strlen(long_name));
	ASSERT_EQ(parse_ext_info(packet, length, true), DSSH_ERROR_PARSE);
	long_name[64] = 0;
	length = ext_info_packet(packet, sizeof(packet), long_name, binary, sizeof(binary));
	ASSERT_OK(parse_ext_info(packet, length, true));
	length = ext_info_packet(packet, sizeof(packet), "server-sig-algs", long_value, strlen(long_name));
	ASSERT_OK(parse_ext_info(packet, length, true));
	packet[24] = 255;
	ASSERT_EQ(parse_ext_info(packet, length, true), DSSH_ERROR_PARSE);
	packet[5] = 255;
	ASSERT_EQ(parse_ext_info(packet, length, true), DSSH_ERROR_PARSE);
	/* Unknown binary values may surround a recognized value in either order. */
	uint8_t known[128], unknown[128];
	const uint8_t algorithm[] = "rsa-sha2-256";
	size_t known_length = ext_info_packet(known, sizeof(known), "server-sig-algs", algorithm, sizeof(algorithm) - 1);
	size_t unknown_length = ext_info_packet(unknown, sizeof(unknown), "future", binary, sizeof(binary));
	ASSERT_TRUE(known_length > 5 && unknown_length > 5);
	for (unsigned order = 0; order < 2; order++) {
		const uint8_t *first = order == 0 ? known : unknown;
		const uint8_t *second = order == 0 ? unknown : known;
		size_t first_length = order == 0 ? known_length : unknown_length;
		size_t second_length = order == 0 ? unknown_length : known_length;
		memcpy(packet, first, first_length);
		packet[4] = 2;
		memcpy(&packet[first_length], &second[5], second_length - 5);
		ASSERT_OK(parse_ext_info(packet, first_length + second_length - 5, true));
	}
	return TEST_PASS;
}

static int
test_ext_info_packet_bound(void)
{
	size_t capacity = SSH_BPP_PACKET_SIZE_MIN - 9;
	uint8_t *packet = malloc(capacity);
	ASSERT_NOT_NULL(packet);
	size_t pos = 0;
	packet[pos++] = SSH_MSG_EXT_INFO;
	DSSH_PUT_U32(1, packet, &pos);
	DSSH_PUT_U32(1, packet, &pos);
	packet[pos++] = 'x';
	/* This concrete packet bound is below UINT32_MAX on every supported width. */
	uint32_t value_length = SSH_BPP_PACKET_SIZE_MIN - 23;
	DSSH_PUT_U32(value_length, packet, &pos);
	for (size_t i = pos; i < capacity; i++) {
		uint8_t byte = (uint8_t)(i % 256);
		packet[i] = byte;
	}
	bool valid = parse_ext_info(packet, capacity, true) == 0
	    && parse_ext_info(packet, capacity, false) == 0;
	/* A large valid count also traverses the full packet without a smaller
	 * per-extension cap or any allocation proportional to a peer count. */
	uint32_t count = (SSH_BPP_PACKET_SIZE_MIN - 14) / 9;
	pos = 1;
	DSSH_PUT_U32(count, packet, &pos);
	for (uint32_t i = 0; i < count; i++) {
		DSSH_PUT_U32(1, packet, &pos);
		packet[pos++] = 'x';
		DSSH_PUT_U32(0, packet, &pos);
	}
	valid = valid && pos <= capacity && parse_ext_info(packet, pos, true) == 0;
	free(packet);
	printf("ext-info packet bound: binary=%zu entries=%" PRIu32 " valid=%u\n",
	    capacity, count, valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
ext_info_verifier(const uint8_t *key, size_t key_length, const uint8_t *signature,
    size_t signature_length, const uint8_t *data, size_t length)
{
	(void)key;
	(void)key_length;
	(void)signature;
	(void)signature_length;
	(void)data;
	(void)length;
	return DSSH_ERROR_REJECTED;
}

static int
ext_info_register_keys(void)
{
	const char *names[] = { "auth-only", "not-a-verifier" };
	int ret;
	for (size_t i = 0; i < 2; i++) {
		dssh_key_algo key = malloc(sizeof(*key) + 32);
		if (key == NULL)
			return DSSH_ERROR_ALLOC;
		*key = (struct dssh_key_algo_s){0};
		if (i == 0)
			key->verify = ext_info_verifier;
		else
			key->flags = DSSH_KEY_ALGO_FLAG_SIGNATURE_CAPABLE;
		memcpy(key->name, names[i], strlen(names[i]) + 1);
		ret = dssh_transport_register_key_algo(key);
		if (ret < 0) {
			free(key);
			return ret;
		}
	}
	return 0;
}

static int
test_ext_info_offers(void)
{
	int ret;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned mode = 0; mode < 3; mode++) {
			ASSERT_EQ(negotiation_setup(), TEST_PASS);
			ASSERT_EQ(add_test_kex("ext-info-c", 0), DSSH_ERROR_INVALID);
			ASSERT_EQ(add_test_kex("ext-info-s", 0), DSSH_ERROR_INVALID);
			bool client = role == 0;
			const char *correct = client ? "curve25519-sha256,ext-info-s" : "curve25519-sha256,ext-info-c";
			const char *wrong = client ? "curve25519-sha256,ext-info-c" : "curve25519-sha256,ext-info-s";
			const char *names[10] = { mode == 0 ? "curve25519-sha256" : mode == 1 ? correct : wrong,
			    "ssh-ed25519", "aes256-ctr", "aes256-ctr", "hmac-sha2-256", "hmac-sha2-256",
			    "none", "none", "", "" };
			uint8_t packet[512];
			size_t length = make_peer_offer(names, 0, packet, sizeof(packet));
			ASSERT_TRUE(length > 0);
			dssh_session sess = dssh_session_init(client, 0);
			ASSERT_NOT_NULL(sess);
			sess->trans.peer_kexinit = malloc(length);
			if (sess->trans.peer_kexinit == NULL) {
				dssh_session_cleanup(sess);
				dssh_test_reset_global_config();
				return TEST_FAIL;
			}
			memcpy(sess->trans.peer_kexinit, packet, length);
			sess->trans.peer_kexinit_sz = length;
			ret = kexinit(sess);
			struct dssh_name_list ours[10];
			bool follows;
			bool valid = ret == (mode == 2 ? DSSH_ERROR_PARSE : 0);
			if (ret == 0) {
				valid = valid && sess->trans.ext_info_receive && sess->trans.ext_info_peer == (mode == 1)
				    && parse_peer_kexinit(sess->trans.our_kexinit, sess->trans.our_kexinit_sz, ours, &follows) == 0;
				const char *indicator = client ? "ext-info-c" : "ext-info-s";
				valid = valid && ours[0].len > 10
				    && memcmp(&ours[0].data[ours[0].len - 10], indicator, 10) == 0;
				/* Negotiation-unit model of a subsequent exchange: no new
				 * indicator, and the original peer offer remains authoritative. */
				sess->trans.session_id = malloc(1);
				if (sess->trans.session_id == NULL)
					valid = false;
				else {
					sess->trans.session_id[0] = 1;
					sess->trans.session_id_sz = 1;
					ret = kexinit(sess);
					valid = valid && ret == 0
					    && parse_peer_kexinit(sess->trans.our_kexinit, sess->trans.our_kexinit_sz, ours, &follows) == 0
					    && ours[0].len == DSSH_STRLEN("curve25519-sha256")
					    && sess->trans.ext_info_peer == (mode == 1);
				}
			}
			dssh_session_cleanup(sess);
			dssh_test_reset_global_config();
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}

static int
test_ext_info_advertisement(void)
{
	int (*previous)(void) = extra_algorithm_registration;
	extra_algorithm_registration = ext_info_register_keys;
	struct handshake_ctx ctx;
	int ret = handshake_setup(&ctx);
	extra_algorithm_registration = previous;
	bool valid = ret == 0;
	uint8_t type;
	uint8_t *payload;
	size_t length;
	if (valid) {
		ret = recv_packet_raw(ctx.client, &type, &payload, &length);
		const char *expected = test_using_rsa512()
		    ? "rsa-sha2-512,rsa-sha2-256,ssh-ed25519,auth-only"
		    : test_using_rsa() ? "rsa-sha2-256,rsa-sha2-512,ssh-ed25519,auth-only"
		    : "ssh-ed25519,rsa-sha2-256,rsa-sha2-512,auth-only";
		size_t expected_length = strlen(expected);
		valid = ret == 0 && type == SSH_MSG_EXT_INFO && length == 28 + expected_length
		    && parse_ext_info(payload, length, true) == 0 && DSSH_GET_U32(&payload[1]) == 1
		    && DSSH_GET_U32(&payload[5]) == 15 && memcmp(&payload[9], "server-sig-algs", 15) == 0
		    && DSSH_GET_U32(&payload[24]) == expected_length && memcmp(&payload[28], expected, expected_length) == 0;
		uint8_t storage[256];
		size_t actual = 0;
		ret = build_server_ext_info(storage, sizeof(storage), &actual);
		valid = valid && ret == 0 && actual == length && memcmp(storage, payload, length) == 0;
		for (size_t capacity = 0; capacity < actual; capacity++) {
			memset(storage, 0xa5, sizeof(storage));
			size_t untouched = 777;
			ret = build_server_ext_info(storage, capacity, &untouched);
			valid = valid && ret == DSSH_ERROR_TOOLONG && untouched == 777 && storage[capacity] == 0xa5;
		}
		ret = build_server_ext_info(storage, actual, &length);
		valid = valid && ret == 0 && length == actual;
		uint8_t marker = SSH_MSG_SERVICE_ACCEPT;
		ret = send_packet(ctx.server, &marker, 1, NULL);
		if (ret == 0)
			ret = recv_packet(ctx.client, &type, &payload, &length);
		valid = valid && ret == 0 && type == marker;
		if (valid)
			valid = compression_rekey_pair(&ctx);
		if (valid) {
			ret = send_packet(ctx.server, &marker, 1, NULL);
			if (ret == 0)
				ret = recv_packet_raw(ctx.client, &type, &payload, &length);
			valid = ret == 0 && type == marker;
		}
	}
	handshake_cleanup(&ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

/* Use real initial handshake state and real authenticated packet framing.
 * Manual auth-service readiness only isolates the receive-position state
 * machine; independent fixtures verify full signed auth and channel data. */
static bool
ext_info_receive_case(unsigned mode)
{
	struct handshake_ctx ctx;
	int ret = handshake_setup(&ctx);
	bool valid = ret == 0;
	uint8_t type;
	uint8_t *payload;
	size_t length;
	const uint8_t empty[] = { 7, 0, 0, 0, 0 };
	uint8_t marker = SSH_MSG_SERVICE_ACCEPT;
	bool server_target = mode >= 4 && mode <= 6;
	dssh_session sender = server_target ? ctx.client : ctx.server;
	dssh_session receiver = server_target ? ctx.server : ctx.client;
	if (valid && !server_target) {
		/* Consume the initial server EXT_INFO through normal transparent RX. */
		ret = send_packet(sender, &marker, 1, NULL);
		if (ret == 0)
			ret = recv_packet(receiver, &type, &payload, &length);
		valid = ret == 0 && type == marker && receiver->trans.ext_info_state == DSSH_EXT_INFO_AUTH;
		if (mode != 2)
			receiver->auth_service_requested = true;
	}
	if (valid && mode == 5) {
		ret = send_packet(sender, &marker, 1, NULL);
		if (ret == 0)
			ret = recv_packet(receiver, &type, &payload, &length);
		valid = ret == 0 && type == marker;
	}
	if (valid && mode == 3) {
		uint8_t success = SSH_MSG_USERAUTH_SUCCESS;
		ret = send_packet(sender, &success, 1, NULL);
		if (ret == 0)
			ret = recv_packet(receiver, &type, &payload, &length);
		valid = ret == 0 && type == success && receiver->trans.ext_info_state == DSSH_EXT_INFO_CLOSED;
	}
	if (valid) {
		ret = send_packet(sender, empty, sizeof(empty), NULL);
		if (ret == 0 && (mode == 6 || mode == 9))
			ret = send_packet(sender, empty, sizeof(empty), NULL);
		if (ret == 0) {
			if (mode == 8) {
				uint8_t disconnect[13] = { SSH_MSG_DISCONNECT, 0, 0, 0, SSH_DISCONNECT_BY_APPLICATION };
				ret = send_packet(sender, disconnect, sizeof(disconnect), NULL);
			}
			else {
				uint8_t next = mode == 0 || mode == 10 ? SSH_MSG_USERAUTH_SUCCESS : marker;
				ret = send_packet(sender, &next, 1, NULL);
			}
		}
		if (ret == 0 && mode == 10) {
			/* EXT_INFO and the adjacent SUCCESS are physically received
			 * before the peer's rekey KEXINIT. Only SUCCESS may be queued;
			 * its replay must not apply extension-position state twice. */
			uint8_t original_id[64];
			size_t id_length = ctx.client->trans.session_id_sz;
			if (id_length > sizeof(original_id))
				ret = DSSH_ERROR_INVALID;
			else {
				memcpy(original_id, ctx.client->trans.session_id, id_length);
				if (!compression_rekey_pair(&ctx))
					ret = DSSH_ERROR_INIT;
				else if (ctx.client->trans.rekey_queue_count != 1
				    || ctx.client->trans.rekey_queue_head == NULL
				    || ctx.client->trans.rekey_queue_head->msg_type != SSH_MSG_USERAUTH_SUCCESS
				    || ctx.client->trans.session_id_sz != id_length
				    || memcmp(original_id, ctx.client->trans.session_id, id_length) != 0)
					ret = DSSH_ERROR_INVALID;
			}
		}
		if (ret == 0)
			ret = recv_packet(receiver, &type, &payload, &length);
		if (mode == 0 || mode == 4 || mode == 10)
			valid = ret == 0 && type == (mode == 4 ? marker : SSH_MSG_USERAUTH_SUCCESS)
			    && receiver->trans.ext_info_state == DSSH_EXT_INFO_CLOSED;
		else if (mode == 8)
			valid = ret == DSSH_ERROR_TERMINATED && dssh_session_is_terminated(receiver);
		else
			valid = ret == DSSH_ERROR_PARSE && dssh_session_is_terminated(receiver);
	}
	handshake_cleanup(&ctx);
	printf("ext-info receive case=%u result=%d valid=%u\n", mode, ret, valid ? 1U : 0U);
	return valid;
}

static int
test_ext_info_receive_positions(void)
{
	ASSERT_TRUE(ext_info_receive_case(0));
	ASSERT_TRUE(ext_info_receive_case(4));
	ASSERT_TRUE(ext_info_receive_case(8));
	ASSERT_TRUE(ext_info_receive_case(10));
	return TEST_PASS;
}

static int
test_ext_info_receive_rejections(void)
{
	ASSERT_TRUE(ext_info_receive_case(1));
	ASSERT_TRUE(ext_info_receive_case(2));
	ASSERT_TRUE(ext_info_receive_case(3));
	ASSERT_TRUE(ext_info_receive_case(5));
	ASSERT_TRUE(ext_info_receive_case(6));
	ASSERT_TRUE(ext_info_receive_case(9));
	return TEST_PASS;
}

static int
test_ext_info_send_failure(void)
{
	ext_info_fail_send = true;
	ext_info_send_failed = false;
	struct handshake_ctx ctx;
	int ret = handshake_setup(&ctx);
	ext_info_fail_send = false;
	bool valid = ret < 0 && ext_info_send_failed && ctx.server != NULL
	    && ctx.server_result == DSSH_ERROR_INIT && dssh_session_is_terminated(ctx.server);
	handshake_cleanup(&ctx);
	ext_info_send_failed = false;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
