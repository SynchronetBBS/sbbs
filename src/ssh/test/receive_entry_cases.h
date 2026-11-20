/* Public receive/start entries must respect the existing serialized-call model.
 * All sessions/mutexes/workers are real. A thread-local I/O refusal makes the
 * old nested entry finite without a second receiver consuming the same stream. */

static int
receive_entry_none(const uint8_t *username, size_t length, void *opaque)
{
	(void)username;
	(void)length;
	(void)opaque;
	return DSSH_AUTH_SUCCESS;
}

static int
receive_entry_kbi(const uint8_t *name, size_t name_len, const uint8_t *instruction,
    size_t instruction_len, uint32_t count, const uint8_t **prompts,
    const size_t *lengths, const bool *echo, struct dssh_auth_responses *out, void *opaque)
{
	(void)name;
	(void)name_len;
	(void)instruction;
	(void)instruction_len;
	(void)count;
	(void)prompts;
	(void)lengths;
	(void)echo;
	(void)out;
	(void)opaque;
	return DSSH_ERROR_REJECTED;
}

static bool
receive_entry_check(dssh_session sess, unsigned kind, bool scoped)
{
	struct receive_entry_io_probe io = {.sess = sess};
	struct receive_entry_io_probe *previous = receive_entry_io_probe;
	receive_entry_io_probe = &io;
	char methods[9];
	uint8_t username[9];
	memset(methods, 0x5a, sizeof(methods));
	memset(username, 0x5a, sizeof(username));
	size_t size = sizeof(username);
	uint32_t sequence = sess->trans.tx_seq;
	bool connected = sess->conn_initialized;
	bool terminated = dssh_session_is_terminated(sess);
	bool scope_valid = session_scope_active(sess) == scoped;
	struct dssh_auth_server_cbs cbs = {.methods_str = "", .none_cb = receive_entry_none};
	int ret = DSSH_ERROR_INIT;
	int native_calls = 0;
	switch (kind) {
		case 0:
			ret = dssh_transport_handshake(sess);
			break;
		case 1:
			ret = test_auth_discover(sess, "u", methods, sizeof(methods));
			break;
		case 2:
			ret = dssh_auth_password(sess, "u", "p", NULL, NULL);
			break;
		case 3:
			ret = dssh_auth_keyboard_interactive(sess, "u", receive_entry_kbi, NULL);
			break;
		case 4:
			ret = dssh_auth_publickey(sess, "u", test_key_algo_name());
			break;
		case 5:
			ret = dssh_auth_server(sess, &cbs, username, &size);
			break;
		case 6:
			/* Failure on the first native initializer guarantees the old
			 * path cannot create a competing demux in this negative case. */
			dssh_test_ossl_fail_after(0);
			ret = dssh_session_start(sess);
			native_calls = dssh_test_ossl_count();
			dssh_test_ossl_reset();
			break;
	}
	receive_entry_io_probe = previous;
	bool valid = ret == DSSH_ERROR_INVALID && io.tx_calls == 0 && io.rx_calls == 0
	    && native_calls == 0 && sequence == sess->trans.tx_seq
	    && connected == sess->conn_initialized && size == sizeof(username)
	    && dssh_session_is_terminated(sess) == terminated && scope_valid;
	for (size_t i = 0; i < sizeof(username); i++)
		valid = valid && username[i] == 0x5a && methods[i] == 0x5a;
	if (!valid)
		fprintf(stderr, "receive entry kind=%u result=%d tx=%u rx=%u native=%d scoped=%u\n",
		    kind, ret, io.tx_calls, io.rx_calls, native_calls, scoped ? 1U : 0U);
	return valid;
}

struct receive_entry_notification {
	dssh_session sess;
	unsigned kind;
	unsigned calls;
	bool valid;
};

static void
receive_entry_debug(bool display, const uint8_t *message, size_t length, void *opaque)
{
	struct receive_entry_notification *p = opaque;
	p->calls++;
	p->valid = display && length == DSSH_STRLEN("entry") && memcmp(message, "entry", length) == 0;
	bool checked = receive_entry_check(p->sess, p->kind, true);
	p->valid = p->valid && checked;
}

static void
receive_entry_unimplemented(uint32_t sequence, void *opaque)
{
	struct receive_entry_notification *p = opaque;
	p->calls++;
	p->valid = sequence == 501;
	bool checked = receive_entry_check(p->sess, p->kind, true);
	p->valid = p->valid && checked;
}

static int
receive_entry_global(const uint8_t *name, size_t name_len, bool reply,
    const uint8_t *data, size_t length, void *opaque)
{
	(void)data;
	struct receive_entry_notification *p = opaque;
	p->calls++;
	p->valid = name_len == DSSH_STRLEN("entry-probe")
	    && memcmp(name, "entry-probe", name_len) == 0 && !reply && length == 0;
	bool checked = receive_entry_check(p->sess, p->kind, true);
	p->valid = p->valid && checked;
	return DSSH_ERROR_REJECTED;
}

static bool
receive_entry_send_control(dssh_session peer, unsigned control)
{
	uint8_t packet[32];
	size_t pos = 0;
	if (control == 0) {
		packet[pos++] = SSH_MSG_DEBUG;
		packet[pos++] = 1;
		DSSH_PUT_U32(DSSH_STRLEN("entry"), packet, &pos);
		memcpy(packet + pos, "entry", DSSH_STRLEN("entry"));
		pos += DSSH_STRLEN("entry");
		DSSH_PUT_U32(0, packet, &pos);
	}
	else if (control == 1) {
		packet[pos++] = SSH_MSG_UNIMPLEMENTED;
		DSSH_PUT_U32(501, packet, &pos);
	}
	else {
		packet[pos++] = SSH_MSG_GLOBAL_REQUEST;
		DSSH_PUT_U32(DSSH_STRLEN("entry-probe"), packet, &pos);
		memcpy(packet + pos, "entry-probe", DSSH_STRLEN("entry-probe"));
		pos += DSSH_STRLEN("entry-probe");
		packet[pos++] = 0;
	}
	uint8_t marker = SSH_MSG_REQUEST_FAILURE;
	return send_packet(peer, packet, pos, NULL) == 0 && send_packet(peer, &marker, 1, NULL) == 0;
}

static int
test_receive_entry_notifications(void)
{
	struct handshake_ctx ctx;
	bool ready = false;
	unsigned passed = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned control = 0; control < 3; control++) {
			for (unsigned kind = 0; kind < 7; kind++) {
				if (!ready) {
					if (handshake_setup(&ctx) < 0) {
						handshake_cleanup(&ctx);
						return TEST_FAIL;
					}
					ready = true;
				}
				dssh_session sess = role == 0 ? ctx.client : ctx.server;
				dssh_session peer = role == 0 ? ctx.server : ctx.client;
				struct receive_entry_notification probe = {.sess = sess, .kind = kind};
				bool valid = dssh_session_set_debug_cb(sess, receive_entry_debug, &probe) == 0
				    && dssh_session_set_unimplemented_cb(sess, receive_entry_unimplemented, &probe) == 0
				    && dssh_session_set_global_request_cb(sess, receive_entry_global, &probe) == 0
				    && receive_entry_send_control(peer, control);
				uint8_t type;
				uint8_t *payload;
				size_t length;
				if (valid)
					valid = recv_packet(sess, &type, &payload, &length) == 0
					    && type == SSH_MSG_REQUEST_FAILURE && length == 1;
				valid = valid && probe.calls == 1 && probe.valid && !session_scope_active(sess);
				valid = dssh_session_set_debug_cb(sess, NULL, NULL) == 0 && valid;
				valid = dssh_session_set_unimplemented_cb(sess, NULL, NULL) == 0 && valid;
				valid = dssh_session_set_global_request_cb(sess, NULL, NULL) == 0 && valid;
				if (valid)
					passed++;
				if (dssh_session_is_terminated(sess) || dssh_session_is_terminated(peer)) {
					handshake_cleanup(&ctx);
					ready = false;
				}
			}
		}
	}
	if (ready)
		handshake_cleanup(&ctx);
	printf("receive entry notifications: %u/42 role/control/API cases passed\n", passed);
	ASSERT_EQ_U(passed, 42);
	return TEST_PASS;
}

struct receive_entry_auth {
	struct handshake_ctx *pair;
	dssh_session banner_target;
	bool server_probe;
	bool banner_probe;
	unsigned server_checks;
	unsigned banner_checks;
	unsigned banner_calls;
	int banner_result;
	int server_result;
	bool banner_valid;
	uint8_t username[8];
	size_t username_len;
};

static int
receive_entry_auth_none(const uint8_t *username, size_t length, void *opaque)
{
	struct receive_entry_auth *p = opaque;
	if (p->server_probe) {
		/* The client is waiting for this callback's reply. Re-enable this
		 * worker only for its own controlled native-initializer probe. */
		dssh_test_ossl_reset();
		for (unsigned kind = 0; kind < 7; kind++) {
			if (receive_entry_check(p->pair->server, kind, true))
				p->server_checks++;
		}
		dssh_test_ossl_exclude_thread();
	}
	p->banner_result = length == 1 && username[0] == 'u'
	    ? dssh_auth_set_banner(p->pair->server, "entry allowed", "en") : DSSH_ERROR_INVALID;
	return DSSH_AUTH_SUCCESS;
}

static void
receive_entry_banner(const uint8_t *message, size_t length,
    const uint8_t *language, size_t language_length, void *opaque)
{
	struct receive_entry_auth *p = opaque;
	p->banner_calls++;
	p->banner_valid = length == DSSH_STRLEN("entry allowed") && language_length == 2
	    && memcmp(message, "entry allowed", length) == 0 && memcmp(language, "en", 2) == 0;
	if (p->banner_probe) {
		for (unsigned kind = 0; kind < 7; kind++) {
			if (receive_entry_check(p->banner_target, kind, true))
				p->banner_checks++;
		}
	}
}

static int
receive_entry_auth_server(void *opaque)
{
	struct receive_entry_auth *p = opaque;
	dssh_test_ossl_exclude_thread();
	struct dssh_auth_server_cbs cbs = {.methods_str = "", .none_cb = receive_entry_auth_none, .cbdata = p};
	p->server_result = dssh_auth_server(p->pair->server, &cbs, p->username, &p->username_len);
	if (p->server_result != 0) {
		dssh_session_terminate(p->pair->client);
		dssh_session_terminate(p->pair->server);
		mock_io_close_c2s(&p->pair->io);
		mock_io_close_s2c(&p->pair->io);
	}
	return 0;
}

/* The caller owns a real established pair, and controls whether the actual
 * server/banner callbacks probe an active outer session. */
static bool
receive_entry_authenticate(struct receive_entry_auth *p)
{
	memset(p->username, 0x5a, sizeof(p->username));
	p->username_len = sizeof(p->username);
	bool valid = dssh_session_set_banner_cb(p->pair->client, receive_entry_banner, p) == 0;
	struct rekey_control_probe watch = {.pair = p->pair};
	atomic_init(&watch.finished, false);
	atomic_init(&watch.timed_out, false);
	thrd_t watchdog, server;
	bool watched = valid && thrd_create(&watchdog, rekey_control_watchdog, &watch) == thrd_success;
	bool started = watched && thrd_create(&server, receive_entry_auth_server, p) == thrd_success;
	char methods[9];
	memset(methods, 0x5a, sizeof(methods));
	int ret = started ? test_auth_discover(p->pair->client, "u", methods, sizeof(methods)) : DSSH_ERROR_INIT;
	if (ret != DSSH_AUTH_SUCCESS) {
		mock_io_close_c2s(&p->pair->io);
		mock_io_close_s2c(&p->pair->io);
	}
	if (started && thrd_join(server, NULL) != thrd_success)
		abort();
	atomic_store(&watch.finished, true);
	if (watched && thrd_join(watchdog, NULL) != thrd_success)
		abort();
	valid = valid && started && ret == DSSH_AUTH_SUCCESS && methods[0] == 0
	    && p->server_result == 0 && p->banner_result == 0 && p->banner_calls == 1 && p->banner_valid
	    && p->username_len == 1 && p->username[0] == 'u' && !atomic_load(&watch.timed_out)
	    && !session_scope_active(p->pair->client) && !session_scope_active(p->pair->server);
	if (p->server_probe)
		valid = valid && p->server_checks == 7;
	if (p->banner_probe)
		valid = valid && p->banner_checks == 7;
	valid = dssh_session_set_banner_cb(p->pair->client, NULL, NULL) == 0 && valid;
	return valid;
}

static int
test_receive_entry_auth_callbacks(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	struct receive_entry_auth probe = {.pair = &ctx, .banner_target = ctx.client,
	    .server_probe = true, .banner_probe = true, .server_result = -1};
	bool valid = receive_entry_authenticate(&probe);
	if (valid)
		valid = dssh_session_start(ctx.client) == 0 && dssh_session_start(ctx.server) == 0;
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	printf("receive entry auth: server=%u/7 banner=%u/7, ordinary-auth/start=%u\n",
	    probe.server_checks, probe.banner_checks, valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct receive_entry_hostkey {
	struct handshake_ctx *pair;
	unsigned calls;
	unsigned checks;
};

static dssh_hostkey_decision
receive_entry_hostkey(const char *name, unsigned bits, const uint8_t *fingerprint,
    const uint8_t *blob, size_t length, void *opaque)
{
	struct receive_entry_hostkey *p = opaque;
	p->calls++;
	for (unsigned kind = 0; kind < 7; kind++) {
		if (receive_entry_check(p->pair->client, kind, true))
			p->checks++;
	}
	return dssh_test_accept_hostkey(name, bits, fingerprint, blob, length, NULL);
}

static int
test_receive_entry_hostkey(void)
{
	struct handshake_ctx ctx;
	struct receive_entry_hostkey probe = {.pair = &ctx};
	handshake_verify_callback = receive_entry_hostkey;
	handshake_verify_data = &probe;
	handshake_exclude_server = true;
	int ret = handshake_setup(&ctx);
	handshake_verify_callback = NULL;
	handshake_verify_data = NULL;
	handshake_exclude_server = false;
	bool valid = ret == 0 && probe.calls == 1 && probe.checks == 7 && key_material_roundtrip(&ctx)
	    && !session_scope_active(ctx.client) && !session_scope_active(ctx.server);
	handshake_cleanup(&ctx);
	printf("receive entry host trust: %u/7 APIs, ordinary-handshake/traffic=%u\n",
	    probe.checks, valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

struct receive_entry_nested {
	struct receive_entry_auth auth;
	dssh_session outer;
	bool valid;
	unsigned calls;
};

static void
receive_entry_nested_debug(bool display, const uint8_t *message, size_t length, void *opaque)
{
	struct receive_entry_nested *p = opaque;
	p->calls++;
	p->valid = display && length == DSSH_STRLEN("entry") && memcmp(message, "entry", length) == 0
	    && session_scope_active(p->outer);
	bool normal = receive_entry_authenticate(&p->auth);
	bool restored = receive_entry_check(p->outer, 1, true);
	p->valid = p->valid && normal && restored;
}

static int
test_receive_entry_nested(void)
{
	struct handshake_ctx outer, inner;
	if (handshake_setup(&outer) < 0) {
		handshake_cleanup(&outer);
		return TEST_FAIL;
	}
	if (handshake_setup_configured(&inner, false, NULL, NULL, NULL, false) < 0) {
		handshake_cleanup_sessions(&inner);
		handshake_cleanup(&outer);
		return TEST_FAIL;
	}
	struct receive_entry_nested p = {.outer = outer.client,
	    .auth = {.pair = &inner, .banner_target = outer.client, .banner_probe = true, .server_result = -1}};
	bool valid = dssh_session_set_debug_cb(outer.client, receive_entry_nested_debug, &p) == 0
	    && receive_entry_send_control(outer.server, 0);
	uint8_t type;
	uint8_t *payload;
	size_t length;
	if (valid)
		valid = recv_packet(outer.client, &type, &payload, &length) == 0
		    && type == SSH_MSG_REQUEST_FAILURE && length == 1;
	valid = valid && p.valid && p.calls == 1 && p.auth.banner_checks == 7
	    && !session_scope_active(outer.client) && !session_scope_active(inner.client)
	    && key_material_roundtrip(&outer) && key_material_roundtrip(&inner);
	if (valid)
		valid = dssh_session_start(inner.client) == 0 && dssh_session_start(inner.server) == 0;
	mock_io_close_c2s(&inner.io);
	mock_io_close_s2c(&inner.io);
	handshake_cleanup_sessions(&inner);
	handshake_cleanup(&outer);
	printf("receive entry nested: %u/7 outer APIs plus restoration, independent-auth/start=%u\n",
	    p.auth.banner_checks, valid ? 1U : 0U);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_receive_entry_demux(void)
{
	unsigned passed = 0;
	for (unsigned role = 0; role < 2; role++) {
		struct handshake_ctx ctx;
		bool ready = false;
		for (unsigned kind = 0; kind < 6; kind++) {
			if (!ready) {
				if (handshake_setup(&ctx) < 0) {
					handshake_cleanup(&ctx);
					return TEST_FAIL;
				}
				struct receive_entry_auth auth = {.pair = &ctx, .banner_target = ctx.client, .server_result = -1};
				bool prepared = receive_entry_authenticate(&auth);
				dssh_session target = role == 0 ? ctx.client : ctx.server;
				atomic_store(&receive_entry_rx_entered, false);
				atomic_store(&receive_entry_watched_rx, target);
				ready = prepared && dssh_session_start(ctx.client) == 0 && dssh_session_start(ctx.server) == 0
				    && dssh_test_wait_bool(&receive_entry_rx_entered, true);
				if (!ready) {
					mock_io_close_c2s(&ctx.io);
					mock_io_close_s2c(&ctx.io);
					handshake_cleanup(&ctx);
					atomic_store(&receive_entry_watched_rx, NULL);
					return TEST_FAIL;
				}
			}
			dssh_session target = role == 0 ? ctx.client : ctx.server;
			/* Actual demux receive is blocked on an empty stream. The TLS
			 * refusal prevents the old public entry from doing packet I/O. */
			bool valid = receive_entry_check(target, kind, false);
			if (valid)
				passed++;
			if (dssh_session_is_terminated(ctx.client) || dssh_session_is_terminated(ctx.server)) {
				mock_io_close_c2s(&ctx.io);
				mock_io_close_s2c(&ctx.io);
				handshake_cleanup(&ctx);
				atomic_store(&receive_entry_watched_rx, NULL);
				ready = false;
			}
		}
		if (ready) {
			mock_io_close_c2s(&ctx.io);
			mock_io_close_s2c(&ctx.io);
			handshake_cleanup(&ctx);
			atomic_store(&receive_entry_watched_rx, NULL);
		}
	}
	printf("receive entry demux: %u/12 role/API cases passed with real auth/receiver/cleanup\n", passed);
	ASSERT_EQ_U(passed, 12);
	return TEST_PASS;
}

static int
test_receive_entry_joinable(void)
{
	unsigned passed = 0;
	for (unsigned role = 0; role < 2; role++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		struct receive_entry_auth auth = {.pair = &ctx, .banner_target = ctx.client, .server_result = -1};
		bool valid = receive_entry_authenticate(&auth)
		    && dssh_session_start(ctx.client) == 0 && dssh_session_start(ctx.server) == 0;
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		if (valid)
			valid = dssh_test_wait_bool(&ctx.client->demux_running, false)
			    && dssh_test_wait_bool(&ctx.server->demux_running, false)
			    && ctx.client->conn_initialized && ctx.server->conn_initialized;
		dssh_session target = role == 0 ? ctx.client : ctx.server;
		for (unsigned kind = 0; kind < 6 && valid; kind++) {
			bool checked = receive_entry_check(target, kind, false);
			if (checked)
				passed++;
		}
		/* No test consumes the worker handle. Library cleanup must still
		 * join that actual completed worker and retire its resources. */
		handshake_cleanup(&ctx);
		ASSERT_TRUE(valid);
	}
	printf("receive entry joinable: %u/12 role/API cases, completed owned workers retained until cleanup\n", passed);
	ASSERT_EQ_U(passed, 12);
	return TEST_PASS;
}
