struct auth_continuation_case {
	struct handshake_ctx *ctx;
	const char *previous;
	bool direct;
	bool key_first;
	bool kbi_first;
	bool change_password;
	unsigned failure_mode;
	bool valid;
	unsigned banners;
	unsigned prompts;
	unsigned changes, releases;
	int peer_result;
};

static const char *const auth_continuation_lists[] = {
	"password,keyboard-interactive,publickey", "keyboard-interactive", "publickey", "",
};

static const char *
auth_continuation_list(const struct auth_continuation_case *state, unsigned step)
{
	if (state->failure_mode != 0)
		return step == 0 ? "password" : auth_continuation_lists[0];
	return auth_continuation_lists[step];
}

static void
auth_continuation_check_snapshot(struct auth_continuation_case *state)
{
	char output[128] = "unchanged";
	dssh_test_alloc_fail_after(0);
	int64_t queried = dssh_auth_get_methods(state->ctx->client, NULL, 0);
	int64_t zero_capacity = dssh_auth_get_methods(state->ctx->client, output, 0);
	state->valid &= strcmp(output, "unchanged") == 0;
	int64_t copied = dssh_auth_get_methods(state->ctx->client, output, sizeof(output));
	state->valid &= zero_capacity == queried;
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	if (state->previous == NULL)
		state->valid &= queried == DSSH_ERROR_INIT && copied == DSSH_ERROR_INIT
		    && strcmp(output, "unchanged") == 0;
	else {
		size_t needed = strlen(state->previous) + 1;
		/* previous is one of the four fixed fixture lists, all below 128. */
		int64_t expected = (int64_t)needed;
		state->valid &= queried > 0 && queried == copied && queried == expected
		    && strcmp(output, state->previous) == 0;
	}
	state->valid &= allocations == 0;
}

static void
auth_continuation_banner(const uint8_t *message, size_t length,
    const uint8_t *language, size_t language_len, void *opaque)
{
	(void)language;
	struct auth_continuation_case *state = opaque;
	state->valid &= length == 4 && memcmp(message, "note", 4) == 0 && language_len == 0;
	state->banners++;
	auth_continuation_check_snapshot(state);
	if (state->failure_mode == 2 && state->banners == 2)
		dssh_test_alloc_fail_after(0);
	else if (state->failure_mode == 3 && state->banners == 2)
		dssh_test_alloc_fail_after(INT_MAX);
	else if (state->failure_mode == 0 && (state->banners == 2 || state->banners == 3))
		dssh_test_alloc_fail_after(0);
}

static int
auth_continuation_prompt(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len, uint32_t count,
    const uint8_t **prompts, const size_t *lengths, const bool *echo,
    struct dssh_auth_responses *out, void *opaque)
{
	(void)name;
	(void)instruction;
	(void)out;
	struct auth_continuation_case *state = opaque;
	state->valid &= name_len == 0 && instruction_len == 0 && count == 0
	    && prompts == NULL && lengths == NULL && echo == NULL;
	state->prompts++;
	auth_continuation_check_snapshot(state);
	if (state->failure_mode == 0)
		dssh_test_alloc_fail_after(0);
	return state->valid ? 0 : DSSH_ERROR_INVALID;
}

static void
auth_continuation_password_release(void *opaque)
{
	struct auth_continuation_case *state = opaque;
	state->releases++;
	state->valid &= dssh_auth_get_methods(state->ctx->client, NULL, 0) == DSSH_ERROR_INVALID;
}

static int
auth_continuation_password_change(const uint8_t *prompt, size_t prompt_len,
    const uint8_t *language, size_t language_len, struct dssh_bytes_result *out, void *opaque)
{
	(void)language;
	struct auth_continuation_case *state = opaque;
	static const uint8_t replacement[] = "replacement";
	state->changes++;
	state->valid &= state->change_password && prompt_len == 6 && memcmp(prompt, "change", 6) == 0
	    && language_len == 0;
	auth_continuation_check_snapshot(state);
	out->bytes.data = replacement;
	out->bytes.len = sizeof(replacement) - 1;
	out->lease.owner = state;
	out->lease.release = auth_continuation_password_release;
	dssh_test_alloc_fail_after(0);
	return state->valid ? 0 : DSSH_ERROR_INVALID;
}

static int
auth_continuation_peer(void *opaque)
{
	struct auth_continuation_case *state = opaque;
	dssh_session peer = state->ctx->server;
	dssh_test_alloc_exclude_thread();
	uint8_t type;
	uint8_t *payload;
	size_t length;
	int ret = recv_packet(peer, &type, &payload, &length);
	if (ret == 0 && type != SSH_MSG_SERVICE_REQUEST)
		ret = DSSH_ERROR_PARSE;
	if (ret == 0) {
		payload[0] = SSH_MSG_SERVICE_ACCEPT;
		ret = send_packet(peer, payload, length, NULL);
	}
	unsigned steps = state->failure_mode == 0 ? 4 : 2;
	for (unsigned step = 0; ret == 0 && step < steps; step++) {
		ret = recv_packet(peer, &type, &payload, &length);
		if (ret < 0)
			break;
		const uint8_t *user, *method;
		size_t user_len, method_len;
		int64_t parsed = parse_userauth_prefix(payload, length, &user, &user_len, &method, &method_len);
		const char *expected = step == 0 ? state->key_first ? "publickey" : state->kbi_first ? "keyboard-interactive"
		    : state->direct ? "password" : "none"
		    : step == 1 ? "password" : step == 2 ? "keyboard-interactive" : "publickey";
		if (type != SSH_MSG_USERAUTH_REQUEST || parsed < 0 || user_len != 4
		    || memcmp(user, "user", 4) != 0 || method_len != strlen(expected)
		    || memcmp(method, expected, method_len) != 0) {
			ret = DSSH_ERROR_PARSE;
			break;
		}
		uint8_t message[128] = { SSH_MSG_USERAUTH_BANNER };
		size_t pos = 1;
		if (!auth_frame_string(message, sizeof(message), &pos, "note", 4)
		    || !auth_frame_string(message, sizeof(message), &pos, "", 0)) {
			ret = DSSH_ERROR_INVALID;
			break;
		}
		ret = send_packet(peer, message, pos, NULL);
		if (ret == 0 && step == 1 && state->failure_mode == 4) {
			mock_io_close_s2c_write(&state->ctx->io);
			break;
		}
		if (ret == 0 && step == 1 && state->change_password) {
			message[0] = SSH_MSG_USERAUTH_PASSWD_CHANGEREQ;
			pos = 1;
			if (!auth_frame_string(message, sizeof(message), &pos, "change", 6)
			    || !auth_frame_string(message, sizeof(message), &pos, "", 0)) {
				ret = DSSH_ERROR_INVALID;
				break;
			}
			ret = send_packet(peer, message, pos, NULL);
			if (ret == 0)
				ret = recv_packet(peer, &type, &payload, &length);
			uint8_t expected_change[128] = { SSH_MSG_USERAUTH_REQUEST };
			size_t expected_len = 1;
			if (!auth_frame_string(expected_change, sizeof(expected_change), &expected_len, "user", 4)
			    || !auth_frame_string(expected_change, sizeof(expected_change), &expected_len, "ssh-connection", 14)
			    || !auth_frame_string(expected_change, sizeof(expected_change), &expected_len, "password", 8)) {
				ret = DSSH_ERROR_INVALID;
				break;
			}
			expected_change[expected_len++] = 1;
			if (!auth_frame_string(expected_change, sizeof(expected_change), &expected_len, "password", 8)
			    || !auth_frame_string(expected_change, sizeof(expected_change), &expected_len, "replacement", 11)) {
				ret = DSSH_ERROR_INVALID;
				break;
			}
			if (ret == 0 && (type != SSH_MSG_USERAUTH_REQUEST || length != expected_len
			    || memcmp(payload, expected_change, expected_len) != 0))
				ret = DSSH_ERROR_PARSE;
		}
		if (ret == 0 && step == 2) {
			const uint8_t challenge[17] = { SSH_MSG_USERAUTH_INFO_REQUEST };
			ret = send_packet(peer, challenge, sizeof(challenge), NULL);
			if (ret == 0) {
				ret = recv_packet(peer, &type, &payload, &length);
				if (ret == 0 && (type != SSH_MSG_USERAUTH_INFO_RESPONSE || length != 5
				    || DSSH_GET_U32(payload + 1) != 0))
					ret = DSSH_ERROR_PARSE;
			}
		}
		if (ret < 0)
			break;
		if (step == 3) {
			message[0] = SSH_MSG_USERAUTH_SUCCESS;
			pos = 1;
		}
		else {
			message[0] = SSH_MSG_USERAUTH_FAILURE;
			pos = 1;
			const char *names = auth_continuation_list(state, step);
			if (!auth_frame_string(message, sizeof(message), &pos, names, strlen(names))) {
				ret = DSSH_ERROR_INVALID;
				break;
			}
			message[pos++] = step == 0 ? 0 : step == 1 ? 1 : 255;
			if (step == 1 && state->failure_mode == 1)
				message[pos++] = 0;
		}
		ret = send_packet(peer, message, pos, NULL);
	}
	state->peer_result = ret;
	if (ret < 0)
		mock_io_close_s2c_write(&state->ctx->io);
	return 0;
}

static int
test_auth_continuation_snapshots(void)
{
	for (unsigned direct = 0; direct < 5; direct++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		struct auth_continuation_case state = {
			.ctx = &ctx, .direct = direct != 0, .key_first = direct == 2,
			.kbi_first = direct == 3, .change_password = direct == 4, .valid = true,
		};
		auth_continuation_check_snapshot(&state);
		if (dssh_session_set_banner_cb(ctx.client, auth_continuation_banner, &state) != 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		thrd_t worker;
		if (thrd_create(&worker, auth_continuation_peer, &state) != thrd_success) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		for (unsigned step = 0; step < 4; step++) {
			int ret;
			if (step == 0)
				ret = direct == 3 ? dssh_auth_keyboard_interactive(ctx.client, "user", auth_continuation_prompt, &state)
				    : direct == 2 ? dssh_auth_publickey(ctx.client, "user", test_key_algo_name())
				    : direct ? dssh_auth_password(ctx.client, "user", "password", NULL, NULL)
				    : dssh_auth_begin(ctx.client, "user");
			else if (step == 1)
				ret = dssh_auth_password(ctx.client, "user", "password",
				    state.change_password ? auth_continuation_password_change : NULL, &state);
			else if (step == 2)
				ret = dssh_auth_keyboard_interactive(ctx.client, "user", auth_continuation_prompt, &state);
			else
				ret = dssh_auth_publickey(ctx.client, "user", test_key_algo_name());
			int expected = step == 0 ? DSSH_AUTH_CONTINUE : step == 3 ? DSSH_AUTH_SUCCESS : DSSH_AUTH_PARTIAL;
			state.valid &= ret == expected && ctx.client->auth_complete == (step == 3);
			if (ret < 0)
				break;
			state.previous = auth_continuation_lists[step];
			auth_continuation_check_snapshot(&state);
			char short_output[1] = { 'X' };
			int64_t copied = dssh_auth_get_methods(ctx.client, short_output, sizeof(short_output));
			state.valid &= step == 3 ? copied == 1 && short_output[0] == 0
			    : copied == DSSH_ERROR_TOOLONG && short_output[0] == 'X';
		}
		mock_io_close_c2s_write(&ctx.io);
		if (thrd_join(worker, NULL) != thrd_success)
			abort();
		bool valid = state.valid && state.peer_result == 0 && state.banners == 4 && state.prompts == 1
		    && state.changes == (direct == 4 ? 1U : 0U) && state.releases == state.changes;
		printf("auth_continuation direct=%u actions=4 banners=%u prompts=%u valid=%d\n",
		    direct, state.banners, state.prompts, valid);
		handshake_cleanup(&ctx);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_auth_continuation_storage(void)
{
	for (unsigned mode = 1; mode <= 4; mode++) {
		for (unsigned direct = 0; direct < 2; direct++) {
			struct handshake_ctx ctx;
			if (handshake_setup(&ctx) < 0) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			struct auth_continuation_case state = {
				.ctx = &ctx, .direct = direct != 0, .failure_mode = mode, .valid = true,
			};
			if (dssh_session_set_banner_cb(ctx.client, auth_continuation_banner, &state) != 0) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			thrd_t worker;
			if (thrd_create(&worker, auth_continuation_peer, &state) != thrd_success) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			int ret = direct ? dssh_auth_password(ctx.client, "user", "password", NULL, NULL)
			    : dssh_auth_begin(ctx.client, "user");
			state.valid &= ret == DSSH_AUTH_CONTINUE;
			size_t original_capacity = ctx.client->auth_methods_capacity;
			state.previous = "password";
			auth_continuation_check_snapshot(&state);
			if (ret >= 0)
				ret = dssh_auth_password(ctx.client, "user", "password", NULL, NULL);
			int allocations = dssh_test_alloc_count();
			dssh_test_alloc_reset();
			int expected = mode == 1 ? DSSH_ERROR_PARSE
			    : mode == 2 || mode == 4 ? DSSH_ERROR_TERMINATED : DSSH_AUTH_PARTIAL;
			state.valid &= ret == expected && !ctx.client->auth_complete
			    && dssh_session_is_terminated(ctx.client) == (mode == 2 || mode == 4);
			state.valid &= mode == 1 || mode == 4 ? allocations == 0 : allocations == 1;
			if (mode == 3) {
				state.previous = auth_continuation_lists[0];
				state.valid &= ctx.client->auth_methods_capacity > original_capacity;
			}
			else
				state.valid &= ctx.client->auth_methods_capacity == original_capacity;
			auth_continuation_check_snapshot(&state);
			mock_io_close_c2s_write(&ctx.io);
			if (thrd_join(worker, NULL) != thrd_success)
				abort();
			bool valid = state.valid && state.peer_result == 0 && state.banners == 2;
			dssh_test_alloc_watch_free(ctx.client->auth_methods, ctx.client->auth_methods_capacity);
			handshake_cleanup(&ctx);
			bool cleared;
			valid &= dssh_test_alloc_watched_free(&cleared);
			dssh_test_alloc_reset();
			printf("auth_continuation_storage mode=%u direct=%u allocations=%d valid=%d\n",
			    mode, direct, allocations, valid);
			ASSERT_TRUE(valid);
		}
	}
	return TEST_PASS;
}
