/* Public language fields, with separate raw-offer and real KEX controls. */
static bool
language_value(dssh_session sess, bool peer, enum dssh_direction direction,
    const char *expected)
{
	char guarded[130];
	memset(guarded, 0x5a, sizeof(guarded));
	size_t length = strlen(expected);
	if (length >= sizeof(guarded) - 2)
		return false;
	size_t needed = length + 1;
	int64_t required = (int64_t)needed;
	int64_t got = peer ? dssh_session_get_peer_languages(sess, direction, guarded + 1, needed)
	    : dssh_session_get_languages(sess, direction, guarded + 1, needed);
	return got == required && guarded[0] == 0x5a
	    && guarded[needed + 1] == 0x5a && strcmp(guarded + 1, expected) == 0;
}

static int
test_language_local(void)
{
	dssh_test_reset_global_config();
	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL)
		return TEST_FAIL;
	const char *names[] = {"en-US", "fr", "x-private"};
	const char *other[] = {"ja-JP"};
	const char expected[] = "en-US,fr,x-private";
	const int64_t expected_size = sizeof(expected);
	bool valid = language_value(sess, false, DSSH_CLIENT_TO_SERVER, "")
	    && language_value(sess, false, DSSH_SERVER_TO_CLIENT, "")
	    && dssh_session_get_peer_languages(sess, DSSH_CLIENT_TO_SERVER, NULL, 0) == DSSH_ERROR_INIT
	    && dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, names, 3) == 0
	    && dssh_session_set_languages(sess, DSSH_SERVER_TO_CLIENT, other, 1) == 0
	    && language_value(sess, false, DSSH_CLIENT_TO_SERVER, expected)
	    && language_value(sess, false, DSSH_SERVER_TO_CLIENT, "ja-JP")
	    && dssh_session_get_languages(sess, DSSH_CLIENT_TO_SERVER, NULL, 0) == expected_size
	    && dssh_session_get_languages(sess, DSSH_CLIENT_TO_SERVER, NULL, 99) == expected_size;
	char buffer[sizeof(expected)];
	memset(buffer, 0x5a, sizeof(buffer));
	valid = valid && dssh_session_get_languages(sess, DSSH_CLIENT_TO_SERVER,
	    buffer, 0) == expected_size && buffer[0] == 0x5a
	    && dssh_session_get_languages(sess, DSSH_CLIENT_TO_SERVER,
	        buffer, sizeof(buffer) - 1) == DSSH_ERROR_TOOLONG;
	for (size_t i = 0; i < sizeof(buffer); i++)
		valid = valid && buffer[i] == 0x5a;
	const char *bad[] = {"", " en", "en,fr", "en_US", "1en", "-en", "en-",
	    "en--US", "abcdefghi", "en-abcdefghi", "en-\001", "en-\200"};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		valid = valid && dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER,
		    &bad[i], 1) == DSSH_ERROR_INVALID;
	}
	const char *missing[] = {NULL};
	enum dssh_direction invalid = (enum dssh_direction)-1;
	valid = valid && dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER,
	    missing, 1) == DSSH_ERROR_INVALID
	    && dssh_session_set_languages(NULL, DSSH_CLIENT_TO_SERVER, names, 3) == DSSH_ERROR_INVALID
	    && dssh_session_set_languages(sess, invalid, names, 3) == DSSH_ERROR_INVALID
	    && dssh_session_get_languages(sess, invalid, NULL, 0) == DSSH_ERROR_INVALID
	    && dssh_session_get_peer_languages(sess, invalid, NULL, 0) == DSSH_ERROR_INVALID
	    && dssh_session_get_languages(NULL, DSSH_CLIENT_TO_SERVER, NULL, 0) == DSSH_ERROR_INVALID
	    && dssh_session_get_peer_languages(NULL, DSSH_CLIENT_TO_SERVER, NULL, 0) == DSSH_ERROR_INVALID
	    && dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, names, SIZE_MAX) == DSSH_ERROR_TOOLONG
	    && language_value(sess, false, DSSH_CLIENT_TO_SERVER, expected)
	    && dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, NULL, SIZE_MAX) == 0
	    && language_value(sess, false, DSSH_CLIENT_TO_SERVER, "")
	    && language_value(sess, false, DSSH_SERVER_TO_CLIENT, "ja-JP")
	    && dssh_session_set_languages(sess, DSSH_SERVER_TO_CLIENT, other, 0) == 0
	    && language_value(sess, false, DSSH_SERVER_TO_CLIENT, "");
	dssh_session_terminate(sess);
	valid = valid && dssh_session_set_languages(sess, DSSH_SERVER_TO_CLIENT, other, 1)
	    == DSSH_ERROR_TERMINATED && language_value(sess, false, DSSH_SERVER_TO_CLIENT, "");
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static void
language_reject_growth(size_t count, size_t size, void *opaque)
{
	(void)count;
	(void)size;
	dssh_session sess = opaque;
	dssh_test_mtx_reject_once(&sess->mtx);
}

static int
test_language_storage(void)
{
	dssh_test_reset_global_config();
	dssh_session sess = dssh_session_init(true, 0);
	if (sess == NULL)
		return TEST_FAIL;
	const char *longer[] = {"en-US", "fr", "de-DE", "sv-SE"};
	const char *shorter[] = {"en"};
	bool valid = dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, longer, 4) == 0;
	char *allocation = sess->local_languages[0].data;
	dssh_test_alloc_fail_after(0);
	valid = valid && dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, shorter, 1) == 0
	    && language_value(sess, false, DSSH_CLIENT_TO_SERVER, "en")
	    && dssh_test_alloc_count() == 0 && sess->local_languages[0].data == allocation
	    && dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, NULL, 0) == 0
	    && sess->local_languages[0].data == allocation;
	dssh_test_alloc_reset();
	valid = valid && dssh_session_set_languages(sess, DSSH_SERVER_TO_CLIENT, shorter, 1) == 0;
	dssh_test_alloc_fail_after(0);
	int ret = dssh_session_set_languages(sess, DSSH_SERVER_TO_CLIENT, longer, 4);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	valid = valid && ret == DSSH_ERROR_ALLOC && allocations == 1
	    && language_value(sess, false, DSSH_SERVER_TO_CLIENT, "en");
	dssh_test_alloc_observe_allocations(language_reject_growth, sess);
	ret = dssh_session_set_languages(sess, DSSH_SERVER_TO_CLIENT, longer, 4);
	dssh_test_alloc_observe_allocations(NULL, NULL);
	valid = valid && ret == DSSH_ERROR_TERMINATED && dssh_test_mtx_rejected_unlocks() == 0
	    && language_value(sess, false, DSSH_SERVER_TO_CLIENT, "en");
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct language_capture {
	uint8_t bytes[1024];
	size_t len;
	bool unlocked;
};

static int
language_capture_tx(uint8_t *data, size_t len, dssh_session sess, void *opaque)
{
	(void)sess;
	struct language_capture *capture = opaque;
	if (len > sizeof(capture->bytes))
		return DSSH_ERROR_TOOLONG;
	memcpy(capture->bytes, data, len);
	capture->len = len;
	capture->unlocked = !dssh_test_mtx_ownership();
	return 0;
}

static bool
language_wire_fields(const struct language_capture *capture, const char *first, const char *second)
{
	if (capture->len < 22 || capture->bytes[5] != SSH_MSG_KEXINIT || !capture->unlocked)
		return false;
	uint32_t packet_length = DSSH_GET_U32(capture->bytes);
	if (packet_length > capture->len - 4)
		return false;
	size_t packet = (size_t)packet_length;
	if (capture->bytes[4] > packet)
		return false;
	size_t end = packet + 4 - capture->bytes[4];
	size_t pos = 22;
	for (size_t i = 0; i < 10; i++) {
		if (!dssh_buffer_has(end, pos, 4))
			return false;
		uint32_t raw = DSSH_GET_U32(&capture->bytes[pos]);
		pos += 4;
		if (raw > end - pos)
			return false;
		size_t length = (size_t)raw;
		if (i >= 8) {
			const char *expected = i == 8 ? first : second;
			if (length != strlen(expected) || memcmp(&capture->bytes[pos], expected, length) != 0)
				return false;
		}
		pos += length;
	}
	return end - pos == 5;
}

static int
language_peer_offer(dssh_session sess, const char *first, const char *second)
{
	const char *names[10] = {gconf.kex_head->name, gconf.key_algo_head->name,
	    gconf.enc_head->name, gconf.enc_head->name, gconf.mac_head->name,
	    gconf.mac_head->name, "none", "none", first, second};
	uint8_t offer[1024];
	size_t length = make_peer_offer(names, 0, offer, sizeof(offer));
	if (length == 0)
		return DSSH_ERROR_INIT;
	uint8_t *copy = malloc(length);
	if (copy == NULL)
		return DSSH_ERROR_ALLOC;
	memcpy(copy, offer, length);
	free(sess->trans.peer_kexinit);
	sess->trans.peer_kexinit = copy;
	sess->trans.peer_kexinit_sz = length;
	return 0;
}

static dssh_session
language_offer_session(struct language_capture *capture)
{
	dssh_test_reset_global_config();
	if (register_all_algorithms() != 0
	    || dssh_transport_set_callbacks(language_capture_tx, boundary_rx, boundary_line, NULL) != 0)
		return NULL;
	dssh_session sess = dssh_session_init(true, 0);
	if (sess != NULL && dssh_session_set_cbdata(sess, capture, NULL, NULL, NULL) != 0) {
		dssh_session_cleanup(sess);
		return NULL;
	}
	return sess;
}

static int
test_language_bounds(void)
{
	struct language_capture capture = {0};
	dssh_session sess = language_offer_session(&capture);
	if (sess == NULL) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	/* Each field fits separately, but their combined wire offer cannot fit. */
	size_t count = sess->trans.packet_buf_sz / 2;
	if (count > SIZE_MAX / sizeof(const char *)) {
		dssh_session_cleanup(sess);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	const char **names = malloc(count * sizeof(*names));
	if (names == NULL) {
		dssh_session_cleanup(sess);
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	for (size_t i = 0; i < count; i++)
		names[i] = "a";
	bool valid = dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, names, count) == 0
	    && dssh_session_set_languages(sess, DSSH_SERVER_TO_CLIENT, names, count) == 0
	    && kexinit(sess) == DSSH_ERROR_TOOLONG && capture.len == 0;
	/* A longer tag makes even a single field exceed the configured bound. */
	for (size_t i = 0; i < count; i++)
		names[i] = "en";
	valid = valid && dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER,
	    names, count) == DSSH_ERROR_TOOLONG;
	free(names);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct language_publish_failure {
	dssh_session sess;
	size_t size;
	bool armed;
};

static void
language_reject_publication(size_t count, size_t size, void *opaque)
{
	struct language_publish_failure *failure = opaque;
	if (count == 1 && size == failure->size) {
		failure->armed = true;
		dssh_test_mtx_reject_once(&failure->sess->mtx);
	}
}

static int
test_language_wire(void)
{
	struct language_capture capture = {0};
	dssh_session sess = language_offer_session(&capture);
	if (sess == NULL) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	const char *first[] = {"en-US", "fr"};
	const char *second[] = {"x-private"};
	dssh_test_mtx_watch_ownership(&sess->mtx);
	bool valid = dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, first, 2) == 0
	    && dssh_session_set_languages(sess, DSSH_SERVER_TO_CLIENT, second, 1) == 0
	    && language_peer_offer(sess, "de-CH,fr", "ja-JP,x-private") == 0 && kexinit(sess) == 0
	    && language_wire_fields(&capture, "en-US,fr", "x-private")
	    && language_value(sess, true, DSSH_CLIENT_TO_SERVER, "de-CH,fr")
	    && language_value(sess, true, DSSH_SERVER_TO_CLIENT, "ja-JP,x-private");
	valid = valid && language_peer_offer(sess, "x_opaque", "Some.WireValue") == 0 && kexinit(sess) == 0
	    && language_value(sess, true, DSSH_CLIENT_TO_SERVER, "x_opaque")
	    && language_value(sess, true, DSSH_SERVER_TO_CLIENT, "Some.WireValue");
	char *storage = sess->peer_languages;
	size_t capacity = sess->peer_languages_capacity;
	valid = valid && language_peer_offer(sess, "es-ES", "sv-SE") == 0 && kexinit(sess) == 0
	    && sess->peer_languages == storage && sess->peer_languages_capacity == capacity
	    && language_value(sess, true, DSSH_CLIENT_TO_SERVER, "es-ES")
	    && language_value(sess, true, DSSH_SERVER_TO_CLIENT, "sv-SE");
	valid = valid && language_peer_offer(sess, "", "") == 0 && kexinit(sess) == 0
	    && sess->peer_languages == storage
	    && language_value(sess, true, DSSH_CLIENT_TO_SERVER, "")
	    && language_value(sess, true, DSSH_SERVER_TO_CLIENT, "");
	valid = valid && language_peer_offer(sess, "de-CH,fr,x-private,en-US", "ja-JP,x-private,en-US,fr") == 0;
	dssh_test_alloc_fail_after(1);
	int ret = kexinit(sess);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	valid = valid && ret == DSSH_ERROR_ALLOC && allocations == 2
	    && sess->peer_languages == storage
	    && language_value(sess, true, DSSH_CLIENT_TO_SERVER, "")
	    && language_value(sess, true, DSSH_SERVER_TO_CLIENT, "");
	valid = valid && language_peer_offer(sess, "en,,fr", "ja-JP") == 0
	    && kexinit(sess) == DSSH_ERROR_PARSE
	    && language_value(sess, true, DSSH_CLIENT_TO_SERVER, "")
	    && language_value(sess, true, DSSH_SERVER_TO_CLIENT, "");
	const char *large_first = "de-CH,fr,x-private,en-US";
	const char *large_second = "ja-JP,x-private,en-US,fr";
	valid = valid && language_peer_offer(sess, large_first, large_second) == 0;
	struct language_publish_failure failure = {
	    .sess = sess, .size = strlen(large_first) + strlen(large_second) + 2 };
	dssh_test_alloc_observe_allocations(language_reject_publication, &failure);
	ret = kexinit(sess);
	dssh_test_alloc_observe_allocations(NULL, NULL);
	valid = valid && failure.armed && ret == DSSH_ERROR_TERMINATED
	    && dssh_test_mtx_rejected_unlocks() == 0
	    && language_value(sess, true, DSSH_CLIENT_TO_SERVER, "")
	    && language_value(sess, true, DSSH_SERVER_TO_CLIENT, "");
	dssh_test_mtx_watch_ownership(NULL);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
language_configure_pair(dssh_session client, dssh_session server)
{
	const char *client_first[] = {"en-US", "fr"};
	const char *client_second[] = {"x-client"};
	const char *server_first[] = {"de-DE"};
	const char *server_second[] = {"ja-JP", "x-server"};
	int ret = dssh_session_set_languages(client, DSSH_CLIENT_TO_SERVER, client_first, 2);
	if (ret == 0)
		ret = dssh_session_set_languages(client, DSSH_SERVER_TO_CLIENT, client_second, 1);
	if (ret == 0)
		ret = dssh_session_set_languages(server, DSSH_CLIENT_TO_SERVER, server_first, 1);
	if (ret == 0)
		ret = dssh_session_set_languages(server, DSSH_SERVER_TO_CLIENT, server_second, 2);
	return ret;
}

static bool
language_pair_values(struct test_session_pair *pair, bool local_updated, bool peer_updated)
{
	return language_value(pair->client, false, DSSH_CLIENT_TO_SERVER, "en-US,fr")
	    && language_value(pair->client, false, DSSH_SERVER_TO_CLIENT, local_updated ? "sv-SE" : "x-client")
	    && language_value(pair->server, false, DSSH_CLIENT_TO_SERVER, local_updated ? "" : "de-DE")
	    && language_value(pair->server, false, DSSH_SERVER_TO_CLIENT, "ja-JP,x-server")
	    && language_value(pair->client, true, DSSH_CLIENT_TO_SERVER, peer_updated ? "" : "de-DE")
	    && language_value(pair->client, true, DSSH_SERVER_TO_CLIENT, "ja-JP,x-server")
	    && language_value(pair->server, true, DSSH_CLIENT_TO_SERVER, "en-US,fr")
	    && language_value(pair->server, true, DSSH_SERVER_TO_CLIENT, peer_updated ? "sv-SE" : "x-client");
}

static int
language_pair_open(struct test_session_pair *pair)
{
	int ret = transport_pair_prepare(pair, 4);
	if (ret == 0)
		ret = language_configure_pair(pair->client, pair->server);
	if (ret == 0)
		ret = dssh_session_set_rekey_seconds(pair->client, 0);
	if (ret == 0)
		ret = dssh_session_set_rekey_seconds(pair->server, 0);
	if (ret == 0)
		ret = test_pair_run(pair, dssh_transport_handshake);
	if (ret != 0)
		return ret;
	if (pair->created != 2 || pair->joined != 2 || !transport_pair_versions(pair)
	    || !transport_pair_algorithms(pair) || !language_pair_values(pair, false, false)
	    || dssh_test_rekeys_completed(pair->client) != 0 || dssh_test_rekeys_completed(pair->server) != 0)
		return DSSH_ERROR_INVALID;
	if (!transport_pair_exchange(pair->client, pair->server, false, SSH_MSG_SERVICE_REQUEST)
	    || !transport_pair_exchange(pair->server, pair->client, false, SSH_MSG_SERVICE_ACCEPT))
		return DSSH_ERROR_INVALID;
	return 0;
}

static int
language_pair_assertion_failure(void)
{
	ASSERT_TRUE(false);
	return TEST_PASS;
}

static struct transport_rekey_result
language_pair_exercise(int (*ready)(struct test_session_pair *), unsigned assertion_phase)
{
	struct transport_rekey_result result = {.ret = DSSH_ERROR_INIT};
	struct test_session_pair pair;
	uint8_t *saved = NULL;
	result.ret = language_pair_open(&pair);
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
	const char *replacement[] = {"sv-SE"};
	result.ret = dssh_session_set_languages(pair.client, DSSH_SERVER_TO_CLIENT, replacement, 1);
	if (result.ret == 0)
		result.ret = dssh_session_set_languages(pair.server, DSSH_CLIENT_TO_SERVER, NULL, 0);
	if (result.ret != 0)
		goto done;
	if (!language_pair_values(&pair, true, false)) {
		result.ret = DSSH_ERROR_INVALID;
		goto done;
	}
	uint32_t before[] = {pair.client->trans.tx_seq, pair.client->trans.rx_seq,
	    pair.server->trans.tx_seq, pair.server->trans.rx_seq};
	if (ready != NULL) {
		result.ret = ready(&pair);
		if (result.ret != 0)
			goto done;
	}
	result.ready = true;
	if (ready != NULL && assertion_phase < 3) {
		/* The assertion hook owns the pair; release these bytes first. */
		free(saved);
		saved = NULL;
		pair.operation = transport_rekey_operation;
		for (unsigned i = 0; result.ret == 0 && i < assertion_phase; i++)
			result.ret = test_pair_start(&pair, i);
		if (result.ret != 0)
			goto done;
		result.ret = language_pair_assertion_failure();
		result.valid = result.ret == TEST_FAIL && transport_pair_retired(&pair);
		goto done;
	}
	result.ret = test_pair_run(&pair, transport_rekey_operation);
	if (result.ret != 0)
		goto done;
	uint32_t steps = test_using_dhgex() ? UINT32_C(4) : UINT32_C(3);
	uint32_t marker_delta = (uint32_t)(steps + 1);
	result.valid = pair.client_result == 0 && pair.server_result == 0
	    && transport_rekey_sequence(before[0], pair.client->trans.tx_seq, marker_delta)
	    && transport_rekey_sequence(before[1], pair.client->trans.rx_seq, steps)
	    && transport_rekey_sequence(before[2], pair.server->trans.tx_seq, steps)
	    && transport_rekey_sequence(before[3], pair.server->trans.rx_seq, marker_delta)
	    && pair.client->trans.tx_since_rekey == 1 && pair.client->trans.rx_since_rekey == 0
	    && pair.server->trans.tx_since_rekey == 0 && pair.server->trans.rx_since_rekey == 1;
	if (result.valid)
		result.valid = transport_pair_exchange(pair.client, pair.server, false, SSH_MSG_SERVICE_REQUEST)
		    && transport_pair_exchange(pair.server, pair.client, false, SSH_MSG_SERVICE_ACCEPT);
	uint32_t outbound = (uint32_t)(steps + 2);
	result.valid = result.valid && pair.created == 4 && pair.joined == 4
	    && transport_pair_algorithms(&pair) && language_pair_values(&pair, true, true)
	    && dssh_test_rekeys_completed(pair.client) == 1 && dssh_test_rekeys_completed(pair.server) == 1
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
test_language_handshake_rekey(void)
{
	struct transport_rekey_result result = language_pair_exercise(NULL, 3);
	int printed = printf("language_pair ready=%d created=%u joined=%u result=%d retired=%d valid=%d\n",
	    result.ready, result.created, result.joined, result.ret, result.retired, result.valid);
	ASSERT_TRUE(result.ret == 0 && result.valid && printed > 0);
	return TEST_PASS;
}

#ifdef DSSH_TRANSPORT_REKEY_CONTROLS
static int
test_transport_all_rekey_ownership(void)
{
	const char *selection = getenv("DSSH_LANGUAGE_OWNER");
	if (selection == NULL)
		return test_transport_rekey_selected_ownership();
	if (strcmp(selection, "1") != 0)
		return TEST_FAIL;
	struct transport_rekey_result result = language_pair_exercise(transport_rekey_ready, 3);
	const char *mode = getenv("DSSH_PEER_FAILURE");
	bool valid = result.ready && result.retired && result.created >= 2 && result.joined >= 2;
	if (mode != NULL && strcmp(mode, "plain") == 0)
		valid = valid && result.ret == 0 && result.valid;
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
transport_all_rekey_assertion_control(unsigned phase)
{
	const char *selection = getenv("DSSH_LANGUAGE_OWNER");
	if (selection == NULL)
		return transport_rekey_selected_assertion_control(phase);
	if (phase > 2 || strcmp(selection, "1") != 0)
		return 2;
	struct transport_rekey_result result = language_pair_exercise(transport_rekey_ready, phase);
	bool valid = result.ready && result.valid && result.ret == TEST_FAIL
	    && result.created >= 2 && result.joined >= 2;
	unsigned created = result.created >= 2 ? result.created - 2 : 0;
	unsigned joined = result.joined >= 2 ? result.joined - 2 : 0;
	int printed = printf("fixture_assertion=1 phase=%u created=%u joined=%u valid=%d\n",
	    phase, created, joined, valid);
	return valid && printed > 0 ? 1 : 2;
}
#endif

struct language_reader {
	dssh_session sess;
	atomic_bool ready;
	atomic_bool stop;
	atomic_bool bad;
	atomic_uint reads;
};

static int
language_read_fields(void *opaque)
{
	struct language_reader *reader = opaque;
	while (!atomic_load(&reader->stop)) {
		char local[80], peer[80];
		int64_t first = dssh_session_get_languages(reader->sess,
		    DSSH_CLIENT_TO_SERVER, local, sizeof(local));
		int64_t second = dssh_session_get_peer_languages(reader->sess,
		    DSSH_CLIENT_TO_SERVER, peer, sizeof(peer));
		if (first < 0 || second < 0
		    || (strcmp(local, "en-US,fr") != 0 && strcmp(local, "de-DE") != 0)
		    || (strcmp(peer, "de-CH,fr") != 0 && strcmp(peer, "es-ES") != 0))
			atomic_store(&reader->bad, true);
		atomic_fetch_add(&reader->reads, 1);
		atomic_store(&reader->ready, true);
	}
	return 0;
}

static int
test_language_concurrent(void)
{
	struct language_capture capture = {0};
	dssh_session sess = language_offer_session(&capture);
	if (sess == NULL) {
		dssh_test_reset_global_config();
		return TEST_FAIL;
	}
	const char *first[] = {"en-US", "fr"};
	const char *second[] = {"de-DE"};
	bool valid = dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, first, 2) == 0
	    && language_peer_offer(sess, "de-CH,fr", "ja-JP") == 0 && kexinit(sess) == 0;
	struct language_reader reader = {.sess = sess};
	atomic_init(&reader.ready, false);
	atomic_init(&reader.stop, false);
	atomic_init(&reader.bad, false);
	atomic_init(&reader.reads, 0);
	thrd_t thread;
	bool started = valid && thrd_create(&thread, language_read_fields, &reader) == thrd_success;
	valid = valid && started && dssh_test_wait_bool(&reader.ready, true);
	for (unsigned i = 0; i < 1000 && started && valid; i++) {
		bool alternate = i % 2 == 0;
		valid = dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER,
		    alternate ? first : second, alternate ? 2 : 1) == 0
		    && language_peer_offer(sess, alternate ? "de-CH,fr" : "es-ES", "ja-JP") == 0
		    && kexinit(sess) == 0;
	}
	atomic_store(&reader.stop, true);
	if (started && thrd_join(thread, NULL) != thrd_success)
		abort();
	unsigned reads = atomic_load(&reader.reads);
	valid = valid && started && reads != 0 && !atomic_load(&reader.bad);
	printf("language_publication updates=1000 reads=%u valid=%u\n", reads, valid ? 1U : 0U);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct language_release_probe {
	dssh_session sess;
	bool valid;
};

static void
language_release_entries(void *opaque)
{
	struct language_release_probe *probe = opaque;
	const char *names[] = {"en"};
	probe->valid = dssh_session_set_languages(probe->sess, DSSH_CLIENT_TO_SERVER,
	    names, 1) == DSSH_ERROR_INVALID
	    && dssh_session_get_languages(probe->sess, DSSH_CLIENT_TO_SERVER,
	        NULL, 0) == DSSH_ERROR_INVALID
	    && dssh_session_get_peer_languages(probe->sess, DSSH_CLIENT_TO_SERVER,
	        NULL, 0) == DSSH_ERROR_INVALID;
}

static int
test_language_entry_failures(void)
{
	bool valid = true;
	const char *names[] = {"en"};
	for (unsigned operation = 0; operation < 4; operation++) {
		dssh_test_reset_global_config();
		dssh_session sess = dssh_session_init(true, 0);
		if (sess == NULL) {
			valid = false;
			break;
		}
		valid = valid && dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, names, 1) == 0;
		if (operation == 3) {
			struct language_release_probe probe = {.sess = sess};
			struct dssh_result_lease lease = {.release = language_release_entries, .owner = &probe};
			dssh_result_release(&lease);
			valid = valid && probe.valid && lease.release == NULL
			    && language_value(sess, false, DSSH_CLIENT_TO_SERVER, "en");
		}
		else {
			dssh_test_mtx_reject_once(&sess->mtx);
			int64_t ret;
			if (operation == 0)
				ret = dssh_session_set_languages(sess, DSSH_CLIENT_TO_SERVER, names, 1);
			else if (operation == 1)
				ret = dssh_session_get_languages(sess, DSSH_CLIENT_TO_SERVER, NULL, 0);
			else
				ret = dssh_session_get_peer_languages(sess, DSSH_CLIENT_TO_SERVER, NULL, 0);
			valid = valid && ret == DSSH_ERROR_TERMINATED
			    && dssh_test_mtx_rejected_unlocks() == 0
			    && dssh_session_is_terminated(sess)
			    && language_value(sess, false, DSSH_CLIENT_TO_SERVER, "en");
		}
		dssh_session_cleanup(sess);
	}
	dssh_test_reset_global_config();
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
