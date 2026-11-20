/* A real library server constructs each challenge; its real client must
 * consume the complete prompt array, including arrays larger than 256. */
struct kbi_count_state {
	struct dssh_auth_prompt *prompts;
	struct dssh_bytes *answers;
	uint32_t count;
	unsigned server_rounds;
	unsigned client_calls;
	unsigned server_releases;
	unsigned client_releases;
	bool server_valid;
	bool client_valid;
};

static void
kbi_count_server_release(void *opaque)
{
	struct kbi_count_state *state = opaque;
	state->server_releases++;
}

static void
kbi_count_client_release(void *opaque)
{
	struct kbi_count_state *state = opaque;
	state->client_releases++;
}

static int
kbi_count_server(const uint8_t *user, size_t user_len, uint32_t count,
    const uint8_t **responses, const size_t *lengths,
    struct dssh_auth_challenge *out, void *opaque)
{
	struct kbi_count_state *state = opaque;
	state->server_valid &= user_len == 4 && memcmp(user, "user", 4) == 0;
	if (state->server_rounds++ == 0) {
		state->server_valid &= count == 0 && responses == NULL && lengths == NULL;
		out->count = state->count;
		out->prompts = state->prompts;
		out->lease.owner = state;
		out->lease.release = kbi_count_server_release;
		return DSSH_AUTH_KBI_PROMPT;
	}
	state->server_valid &= count == state->count && responses != NULL && lengths != NULL;
	if (count == state->count && lengths != NULL) {
		for (uint32_t i = 0; i < count; i++)
			state->server_valid &= lengths[i] == 0;
	}
	return state->server_valid ? DSSH_AUTH_SUCCESS : DSSH_AUTH_FAILURE;
}

static int
kbi_count_client(const uint8_t *name, size_t name_len,
    const uint8_t *instruction, size_t instruction_len, uint32_t count,
    const uint8_t **prompts, const size_t *lengths, const bool *echo,
    struct dssh_auth_responses *out, void *opaque)
{
	(void)name;
	(void)instruction;
	struct kbi_count_state *state = opaque;
	state->client_calls++;
	state->client_valid &= name_len == 0 && instruction_len == 0
	    && count == state->count && prompts != NULL && lengths != NULL && echo != NULL;
	if (!state->client_valid)
		return DSSH_ERROR_INVALID;
	for (uint32_t i = 0; i < count; i++) {
		state->client_valid &= lengths[i] == 1 && prompts[i][0] == '?'
		    && echo[i] == ((i % 2) != 0);
	}
	out->count = count;
	out->items = state->answers;
	out->lease.owner = state;
	out->lease.release = kbi_count_client_release;
	return state->client_valid ? 0 : DSSH_ERROR_INVALID;
}

static int
test_kbi_prompt_counts(void)
{
	const size_t counts[] = { 1, 256, 257, 1000 };
	unsigned failures = 0;
	for (size_t profile = 0; profile < sizeof(counts) / sizeof(counts[0]); profile++) {
		size_t count = counts[profile];
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		struct kbi_count_state state = { .server_valid = true, .client_valid = true };
		ASSERT_TRUE(count <= UINT32_MAX && count <= SIZE_MAX / sizeof(*state.prompts)
		    && count <= SIZE_MAX / sizeof(*state.answers));
		uint32_t wire_count = (uint32_t)count;
		state.count = wire_count;
		state.prompts = calloc(count, sizeof(*state.prompts));
		state.answers = calloc(count, sizeof(*state.answers));
		if (state.prompts == NULL || state.answers == NULL) {
			free(state.prompts);
			free(state.answers);
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		static const uint8_t question[] = "?";
		for (size_t i = 0; i < count; i++) {
			state.prompts[i].text.data = question;
			state.prompts[i].text.len = 1;
			state.prompts[i].echo = (i % 2) != 0;
		}
		struct auth_server_arg server = {
			.ctx = &ctx,
			.cbs = {
				.methods_str = "keyboard-interactive",
				.keyboard_interactive_cb = kbi_count_server,
				.cbdata = &state,
			},
		};
		thrd_t worker;
		if (thrd_create(&worker, auth_server_thread, &server) != thrd_success) {
			free(state.prompts);
			free(state.answers);
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		int ret = dssh_auth_keyboard_interactive(ctx.client, "user", kbi_count_client, &state);
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
		if (thrd_join(worker, NULL) != thrd_success)
			abort();
		bool valid = ret == 0 && server.result == 0 && state.server_valid && state.client_valid
		    && state.client_calls == 1 && state.server_rounds == 2
		    && state.server_releases == 1 && state.client_releases == 1;
		printf("kbi_prompt_count count=%zu client=%d server=%d valid=%d\n",
		    count, ret, server.result, valid);
		if (!valid)
			failures++;
		handshake_cleanup(&ctx);
		free(state.prompts);
		free(state.answers);
	}
	printf("kbi_prompt_counts cases=4 failures=%u\n", failures);
	ASSERT_EQ_U(failures, 0);
	return TEST_PASS;
}
