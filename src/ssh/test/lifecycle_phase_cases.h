/* Exercise public admission without a peer. Every unexpected I/O fails
 * immediately; even an erroneously started worker can be joined safely. */
static atomic_uint lifecycle_phase_io_calls;

static int
lifecycle_phase_io(uint8_t *buf, size_t length, dssh_session sess, void *cbdata)
{
	(void)buf;
	(void)length;
	(void)sess;
	(void)cbdata;
	atomic_fetch_add(&lifecycle_phase_io_calls, 1);
	return DSSH_ERROR_INIT;
}

static int
lifecycle_phase_line(uint8_t *buf, size_t length, size_t *received,
    dssh_session sess, void *cbdata)
{
	(void)received;
	return lifecycle_phase_io(buf, length, sess, cbdata);
}

static int
lifecycle_phase_call(dssh_session sess, unsigned operation)
{
	char methods[128];
	struct test_kbi_cbdata kbi = { .response = "answer" };
	const struct dssh_auth_server_cbs cbs = { .methods_str = "password" };
	switch (operation) {
	case 0:
		return dssh_session_start(sess);
	case 1:
		return test_auth_discover(sess, "user", methods, sizeof(methods));
	case 2:
		return dssh_auth_password(sess, "user", "password", NULL, NULL);
	case 3:
		return dssh_auth_keyboard_interactive(sess, "user", test_kbi_prompt_cb, &kbi);
	case 4:
		return dssh_auth_publickey(sess, "user", "ssh-ed25519");
	case 5:
		return dssh_auth_server(sess, &cbs, NULL, NULL);
	default:
		return dssh_transport_handshake(sess);
	}
}

static int
test_lifecycle_preflight(void)
{
	unsigned cases = 0;
	unsigned failures = 0;
	for (unsigned dead = 0; dead < 2; dead++) {
		for (unsigned client = 0; client < 2; client++) {
			for (unsigned operation = 0; operation < (dead ? 7u : 6u); operation++) {
				dssh_test_reset_global_config();
				ASSERT_EQ(dssh_transport_set_callbacks(lifecycle_phase_io,
				    lifecycle_phase_io, lifecycle_phase_line, NULL), 0);
				dssh_session sess = dssh_session_init(client != 0, 0);
				ASSERT_NOT_NULL(sess);
				if (dead)
					ASSERT_TRUE(dssh_session_terminate(sess));
				atomic_store(&lifecycle_phase_io_calls, 0);
				int ret = lifecycle_phase_call(sess, operation);
				int expected = dead ? DSSH_ERROR_TERMINATED : DSSH_ERROR_INVALID;
				bool initialized = atomic_load(&sess->conn_initialized);
				bool terminated = dssh_session_is_terminated(sess);
				/* Cleanup joins any erroneously created worker before observing
				 * its callback counter or starting the next fresh fixture. */
				dssh_session_cleanup(sess);
				unsigned calls = atomic_load(&lifecycle_phase_io_calls);
				bool valid = ret == expected && !initialized && calls == 0
				    && terminated == (dead != 0);
				printf("lifecycle_preflight dead=%u client=%u operation=%u ret=%d io=%u valid=%d\n",
				    dead, client, operation, ret, calls, valid);
				if (!valid)
					failures++;
				cases++;
			}
		}
	}
	dssh_test_reset_global_config();
	printf("lifecycle_preflight cases=%u failures=%u\n", cases, failures);
	ASSERT_EQ_U(cases, 26);
	ASSERT_EQ_U(failures, 0);
	return TEST_PASS;
}

struct lifecycle_phase_auth {
	unsigned mode;
	unsigned passwords;
};

static int
lifecycle_phase_none(const uint8_t *user, size_t length, void *opaque)
{
	(void)user;
	(void)length;
	struct lifecycle_phase_auth *state = opaque;
	return state->mode == 0 ? DSSH_AUTH_SUCCESS : DSSH_AUTH_FAILURE;
}

static int
lifecycle_phase_password(const uint8_t *user, size_t user_length,
    const uint8_t *password, size_t password_length,
    struct dssh_bytes_result *out, void *opaque)
{
	(void)user;
	(void)user_length;
	(void)password;
	(void)password_length;
	(void)out;
	struct lifecycle_phase_auth *state = opaque;
	unsigned attempt = state->passwords++;
	if (attempt == 0)
		return DSSH_AUTH_PARTIAL;
	if (attempt == 1)
		return DSSH_AUTH_FAILURE;
	return DSSH_AUTH_SUCCESS;
}

static int
lifecycle_phase_key(const uint8_t *user, size_t user_length, const char *name,
    const uint8_t *key, size_t key_length, bool signed_request, void *opaque)
{
	(void)user;
	(void)user_length;
	(void)name;
	(void)key;
	(void)key_length;
	(void)signed_request;
	(void)opaque;
	return DSSH_AUTH_SUCCESS;
}

static int
lifecycle_phase_kbi(const uint8_t *user, size_t user_length, uint32_t count,
    const uint8_t **answers, const size_t *lengths,
    struct dssh_auth_challenge *out, void *opaque)
{
	(void)user;
	(void)user_length;
	(void)count;
	(void)answers;
	(void)lengths;
	(void)out;
	(void)opaque;
	return DSSH_AUTH_SUCCESS;
}

/* Rejected operations must not consume wire data, emit bytes, allocate, or
 * change live transport state. The thread-local I/O refusal bounds a failed
 * check while keeping the peer's real authentication operation independent. */
static bool
lifecycle_phase_reject(dssh_session sess, unsigned operation)
{
	lifecycle_phase_deny_io = true;
	lifecycle_phase_unexpected_io = 0;
	dssh_test_alloc_fail_after(0);
	int ret = lifecycle_phase_call(sess, operation);
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	lifecycle_phase_deny_io = false;
	return ret == DSSH_ERROR_INVALID && allocations == 0
	    && lifecycle_phase_unexpected_io == 0 && !dssh_session_is_terminated(sess);
}

static int
test_lifecycle_progression(void)
{
	unsigned failures = 0;
	for (unsigned mode = 0; mode < 4; mode++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		bool valid = ctx.client->handshake_complete && ctx.server->handshake_complete
		    && !ctx.client->auth_complete && !ctx.server->auth_complete;
		/* A repeated initial handshake and the opposite role's auth API
		 * must be refused while leaving the actual exchange usable. */
		valid &= lifecycle_phase_reject(ctx.client, 6);
		valid &= lifecycle_phase_reject(ctx.server, 6);
		valid &= lifecycle_phase_reject(ctx.client, 5);
		for (unsigned operation = 1; operation <= 4; operation++)
			valid &= lifecycle_phase_reject(ctx.server, operation);
		valid &= lifecycle_phase_reject(ctx.client, 0);
		valid &= lifecycle_phase_reject(ctx.server, 0);
		struct lifecycle_phase_auth state = { .mode = mode };
		struct auth_server_arg server = {
			.ctx = &ctx,
			.cbs = {
				.methods_str = "publickey,password,keyboard-interactive",
				.none_cb = lifecycle_phase_none,
				.password_cb = lifecycle_phase_password,
				.publickey_cb = lifecycle_phase_key,
				.keyboard_interactive_cb = lifecycle_phase_kbi,
				.cbdata = &state,
			},
		};
		thrd_t worker;
		if (thrd_create(&worker, auth_server_thread, &server) != thrd_success) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		char methods[128];
		int ret = test_auth_discover(ctx.client, "user", methods, sizeof(methods));
		valid &= ret == (mode == 0 ? DSSH_AUTH_SUCCESS : DSSH_AUTH_CONTINUE);
		valid &= ctx.client->auth_complete == (mode == 0);
		if (ret == DSSH_AUTH_CONTINUE) {
			valid &= lifecycle_phase_reject(ctx.client, 0);
			if (mode == 1) {
				/* Both partial success and ordinary rejection preserve the
				 * authentication receive phase for the next valid attempt. */
				for (unsigned attempt = 0; attempt < 3; attempt++) {
					ret = dssh_auth_password(ctx.client, "user", "password", NULL, NULL);
					valid &= ret == (attempt == 0 ? DSSH_AUTH_PARTIAL : attempt == 1 ? DSSH_AUTH_CONTINUE : 0);
					if (attempt < 2) {
						valid &= !ctx.client->auth_complete;
						valid &= lifecycle_phase_reject(ctx.client, 0);
					}
				}
			}
			else if (mode == 2)
				ret = dssh_auth_publickey(ctx.client, "user", test_key_algo_name());
			else {
				struct test_kbi_cbdata kbi = { .response = "answer" };
				ret = dssh_auth_keyboard_interactive(ctx.client, "user", test_kbi_prompt_cb, &kbi);
			}
		}
		if (ret != 0) {
			mock_io_close_c2s(&ctx.io);
			mock_io_close_s2c(&ctx.io);
		}
		if (thrd_join(worker, NULL) != thrd_success)
			abort();
		valid &= ret == 0 && server.result == 0;
		valid &= ctx.client->auth_complete && ctx.server->auth_complete;
		if (ret == 0 && server.result == 0) {
			for (unsigned operation = 1; operation <= 6; operation++) {
				valid &= lifecycle_phase_reject(ctx.client, operation);
				valid &= lifecycle_phase_reject(ctx.server, operation);
			}
			/* Both mutexes, the condition and the worker creation can fail.
			 * A refused startup leaves the completed authentication available
			 * for a later successful startup, without retiring the session. */
			for (unsigned side = 0; side < 2; side++) {
				dssh_session sess = side == 0 ? ctx.client : ctx.server;
				for (int failure = 0; failure < 4; failure++) {
					dssh_test_ossl_fail_after(failure);
					int started = dssh_session_start(sess);
					dssh_test_ossl_reset();
					valid &= started == DSSH_ERROR_INIT && !sess->conn_initialized
					    && !sess->demux_running && !sess->terminate
					    && sess->handshake_complete && sess->auth_complete;
				}
			}
			valid &= dssh_session_start(ctx.client) == 0;
			valid &= dssh_session_start(ctx.server) == 0;
			valid &= dssh_session_start(ctx.client) == DSSH_ERROR_INIT;
			valid &= dssh_session_start(ctx.server) == DSSH_ERROR_INIT;
		}
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		dssh_session_stop(ctx.client);
		dssh_session_stop(ctx.server);
		valid &= dssh_session_start(ctx.client) == DSSH_ERROR_TERMINATED;
		valid &= dssh_session_start(ctx.server) == DSSH_ERROR_TERMINATED;
		printf("lifecycle_progression mode=%u passwords=%u valid=%d\n", mode, state.passwords, valid);
		if (!valid)
			failures++;
		handshake_cleanup(&ctx);
	}
	printf("lifecycle_progression modes=4 failures=%u\n", failures);
	ASSERT_EQ_U(failures, 0);
	return TEST_PASS;
}

static int
test_lifecycle_handshake_failure(void)
{
	dssh_test_reset_global_config();
	ASSERT_EQ(dssh_transport_set_callbacks(lifecycle_phase_io,
	    lifecycle_phase_io, lifecycle_phase_line, NULL), 0);
	dssh_session sess = dssh_session_init(true, 0);
	ASSERT_NOT_NULL(sess);
	atomic_store(&lifecycle_phase_io_calls, 0);
	/* Missing application trust policy is a recoverable prerequisite error. */
	int missing = dssh_transport_handshake(sess);
	bool valid = missing == DSSH_ERROR_INIT && !sess->handshake_complete
	    && !sess->auth_complete && !sess->terminate
	    && atomic_load(&lifecycle_phase_io_calls) == 0;
	valid &= dssh_session_set_hostkey_verify_cb(sess, dssh_test_accept_hostkey, NULL) == 0;
	int failed = dssh_transport_handshake(sess);
	unsigned calls = atomic_load(&lifecycle_phase_io_calls);
	valid &= failed == DSSH_ERROR_INIT && !sess->handshake_complete
	    && !sess->auth_complete && sess->terminate && calls == 1;
	int repeated = dssh_transport_handshake(sess);
	valid &= repeated == DSSH_ERROR_TERMINATED
	    && atomic_load(&lifecycle_phase_io_calls) == calls;
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	printf("lifecycle_handshake_failure missing=%d failed=%d repeated=%d io=%u valid=%d\n",
	    missing, failed, repeated, calls, valid);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}
