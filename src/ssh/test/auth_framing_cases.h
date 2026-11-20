#ifndef DSSH_AUTH_FRAMING_CASES_H
#define DSSH_AUTH_FRAMING_CASES_H

struct auth_frame_probe {
	unsigned calls;
	unsigned shape;
	bool callback_enabled;
	bool null_password;
	bool valid_inputs;
	bool output_preserved;
	bool tx_released;
	bool sensitive_cleared;
	uint32_t prompts;
	size_t message_len;
	uint8_t message[8];
};

static bool
auth_frame_string(uint8_t *packet, size_t capacity, size_t *pos, const void *value, size_t length)
{
#if SIZE_MAX > UINT32_MAX
	if (length > UINT32_MAX)
		return false;
#endif
	if (!dssh_buffer_has(capacity, *pos, 4))
		return false;
	uint32_t wire_length = (uint32_t)length;
	DSSH_PUT_U32(wire_length, packet, pos);
	if (!dssh_buffer_has(capacity, *pos, length))
		return false;
	if (length > 0)
		memcpy(&packet[*pos], value, length);
	*pos += length;
	return true;
}

static void
auth_frame_banner(const uint8_t *message, size_t length, const uint8_t *language,
    size_t language_len, void *cbdata)
{
	(void)language;
	(void)language_len;
	struct auth_frame_probe *probe = cbdata;
	probe->calls++;
	probe->message_len = length;
	if (length <= sizeof(probe->message))
		memcpy(probe->message, message, length);
}

static int
auth_frame_change(const uint8_t *prompt, size_t prompt_len, const uint8_t *language,
    size_t language_len, struct dssh_bytes_result *output_result, void *cbdata)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **password = &fixture_output;
	size_t *password_len = &fixture_length;
	(void)password;
	(void)password_len;

	(void)prompt;
	(void)prompt_len;
	(void)language;
	(void)language_len;
	struct auth_frame_probe *probe = cbdata;
	probe->calls++;
	*password = NULL;
	*password_len = probe->null_password ? 1 : 0;
	return test_auth_output_finish(output_result, fixture_output, fixture_length, 0);
}

static int
auth_frame_prompt(const uint8_t *name, size_t name_len, const uint8_t *instruction,
    size_t instruction_len, uint32_t count, const uint8_t **prompts, const size_t *lengths,
    const bool *echo, struct dssh_auth_responses *out, void *cbdata)
{
	struct dssh_bytes *answers = NULL;
	out->count = count;
	if (count > 0) {
#if SIZE_MAX < UINT32_MAX
		if (count > SIZE_MAX)
			return DSSH_ERROR_TOOLONG;
#endif
		size_t answer_count = (size_t)count;
		answers = test_auth_lease_calloc(&out->lease, answer_count, sizeof(*answers));
		if (answers == NULL)
			return DSSH_ERROR_ALLOC;
		out->items = answers;
	}

	(void)name;
	(void)name_len;
	(void)instruction;
	(void)instruction_len;
	struct auth_frame_probe *probe = cbdata;
	probe->calls++;
	probe->prompts = count;
	for (uint32_t i = 0; i < count; i++) {
		if (lengths[i] != 1 || prompts[i][0] != 'x' || !echo[i])
			probe->valid_inputs = false;
		answers[i].data = NULL;
		answers[i].len = 0;
	}
	return 0;
}

/* Prequeue the peer response and a valid completion marker. A malformed
 * message must not become a successful exchange or enter its callback. */
static int
auth_frame_client(unsigned method, const uint8_t *packet, size_t length, struct auth_frame_probe *probe)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return -999;
	}
	ctx.client->auth_service_requested = true;
	int ret = 0;
	if (probe->callback_enabled)
		ret = dssh_session_set_banner_cb(ctx.client, auth_frame_banner, probe);
	if (ret == 0)
		ret = send_packet(ctx.server, packet, length, NULL);
	uint8_t success = SSH_MSG_USERAUTH_SUCCESS;
	if (ret == 0)
		ret = send_packet(ctx.server, &success, 1, NULL);
	mock_io_close_s2c_write(&ctx.io);
	char methods[128] = "unchanged";
	if (ret == 0) {
		if (method == 0)
			ret = test_auth_discover_component(ctx.client, "user", methods, sizeof(methods));
		else if (method == 1)
			ret = auth_password_impl(ctx.client, "user", "password",
			    probe->callback_enabled ? auth_frame_change : NULL, probe);
		else if (method == 2)
			ret = auth_kbi_impl(ctx.client, "user", auth_frame_prompt, probe);
		else
			ret = auth_publickey_impl(ctx.client, "user", test_key_algo_name());
	}
	probe->output_preserved = strcmp(methods, "unchanged") == 0;
	probe->tx_released = !tx_owner_is_current(ctx.client);
	probe->sensitive_cleared = true;
	if (method == 2 && probe->calls > 0) {
		size_t response_len = 5 + 4 * probe->prompts;
		for (size_t i = 0; i < response_len; i++) {
			if (ctx.client->trans.tx_packet[9 + i] != 0)
				probe->sensitive_cleared = false;
		}
	}
	handshake_cleanup(&ctx);
	return ret;
}

static int
test_auth_frame_results(void)
{
	for (unsigned method = 0; method < 4; method++) {
		for (unsigned variant = 0; variant < 17; variant++) {
			uint8_t packet[128] = { SSH_MSG_USERAUTH_FAILURE };
			size_t length = 1;
			int expected = DSSH_ERROR_PARSE;
			if (variant < 2) {
				packet[0] = SSH_MSG_USERAUTH_SUCCESS;
				if (variant == 0)
					expected = 0;
				else
					packet[length++] = 0;
			}
			else if (variant == 2) {
				/* Missing string length. */
			}
			else if (variant == 3)
				DSSH_PUT_U32(UINT32_MAX, packet, &length);
			else {
				const char *names = variant == 4 ? "" : variant == 5 ? "password" :
				    variant == 6 ? "p," : variant == 7 ? ",p" : variant == 8 ? "p,,q" :
				    variant == 9 ? "none" : variant == 10 ? "p none" :
				    variant == 11 ? "p\177" : variant == 12 ? "p\200" : "password";
				char long_name[66];
				if (variant >= 15) {
					memset(long_name, 'a', variant == 15 ? 64 : 65);
					long_name[variant == 15 ? 64 : 65] = 0;
					names = long_name;
				}
				ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, names, strlen(names)));
				if (variant != 13)
					packet[length++] = variant == 5 ? 255 : 0;
				if (variant == 14)
					packet[length++] = 0;
				if (variant == 4 || variant == 5 || variant == 15)
					expected = variant == 5 ? DSSH_AUTH_PARTIAL : DSSH_AUTH_CONTINUE;
			}
			struct auth_frame_probe probe = { .valid_inputs = true };
			ASSERT_EQ(auth_frame_client(method, packet, length, &probe), expected);
			ASSERT_EQ_U(probe.calls, 0);
			ASSERT_TRUE(probe.tx_released);
			if (method == 0 && expected < 0)
				ASSERT_TRUE(probe.output_preserved);
		}
	}
	return TEST_PASS;
}

static int
test_auth_frame_text(void)
{
	for (unsigned method = 0; method < 4; method++) {
		for (unsigned enabled = 0; enabled < 2; enabled++) {
			for (unsigned variant = 0; variant < 7; variant++) {
				uint8_t packet[64] = { SSH_MSG_USERAUTH_BANNER };
				size_t length = 1;
				const uint8_t text[] = { 0, 10, 27 };
				if (variant == 1)
					DSSH_PUT_U32(UINT32_MAX, packet, &length);
				else if (variant >= 2) {
					ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, text, sizeof(text)));
					if (variant == 3)
						DSSH_PUT_U32(2, packet, &length);
					else if (variant >= 4) {
						ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "en", 2));
						if (variant == 5)
							length--;
						else if (variant == 6)
							packet[length++] = 0;
					}
				}
				struct auth_frame_probe probe = { .callback_enabled = enabled != 0, .valid_inputs = true };
				ASSERT_EQ(auth_frame_client(method, packet, length, &probe),
				    variant == 4 ? 0 : DSSH_ERROR_PARSE);
				ASSERT_EQ_U(probe.calls, variant == 4 && enabled != 0 ? 1 : 0);
				if (probe.calls > 0) {
					ASSERT_EQ_U(probe.message_len, sizeof(text));
					ASSERT_MEM_EQ(probe.message, text, sizeof(text));
				}
			}
		}
	}
	/* CHANGEREQ uses the same exact prompt/language framing. */
	for (unsigned enabled = 0; enabled < 2; enabled++) {
		for (unsigned variant = 0; variant < 6; variant++) {
			uint8_t packet[32] = { SSH_MSG_USERAUTH_PASSWD_CHANGEREQ };
			size_t length = 1;
			if (variant > 0) {
				ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "", 0));
				if (variant > 1) {
					ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "", 0));
					if (variant == 3)
						packet[length++] = 0;
					else if (variant == 4)
						length--;
				}
			}
			struct auth_frame_probe probe = { .callback_enabled = enabled != 0, .null_password = variant == 5 };
			int expected = variant == 2 || variant == 5 ?
			    enabled == 0 ? DSSH_ERROR_INIT : variant == 5 ? DSSH_ERROR_INVALID : 0 : DSSH_ERROR_PARSE;
			ASSERT_EQ(auth_frame_client(1, packet, length, &probe), expected);
			ASSERT_EQ_U(probe.calls, enabled != 0 && (variant == 2 || variant == 5) ? 1 : 0);
		}
	}
	return TEST_PASS;
}

static int
test_auth_frame_kbi(void)
{
	for (unsigned variant = 0; variant < 9; variant++) {
		uint8_t packet[2048] = { SSH_MSG_USERAUTH_INFO_REQUEST };
		size_t length = 1;
		ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "", 0));
		ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "", 0));
		ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "", 0));
		uint32_t count = variant == 0 || variant == 1 ? 0 : variant == 2 ? 256 :
		    variant == 3 ? 257 : variant == 4 ? UINT32_MAX : 1;
		DSSH_PUT_U32(count, packet, &length);
		if (variant == 1)
			packet[length++] = 0;
		else if (variant == 2) {
			for (uint32_t i = 0; i < count; i++) {
				ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "x", 1));
				packet[length++] = 255;
			}
		}
		else if (variant == 5) {
			ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "", 0));
			packet[length++] = 1;
		}
		else if (variant >= 6) {
			ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "x", 1));
			if (variant != 6)
				packet[length++] = 255;
			if (variant == 8)
				packet[length++] = 0;
		}
		struct auth_frame_probe probe = { .valid_inputs = true };
		bool valid = variant == 0 || variant == 2 || variant == 7;
		ASSERT_EQ(auth_frame_client(2, packet, length, &probe), valid ? 0 : DSSH_ERROR_PARSE);
		ASSERT_EQ_U(probe.calls, valid ? 1 : 0);
		ASSERT_TRUE(probe.valid_inputs);
		ASSERT_TRUE(probe.tx_released);
		if (valid) {
			ASSERT_EQ_U(probe.prompts, count);
			ASSERT_TRUE(probe.sensitive_cleared);
		}
	}
	return TEST_PASS;
}

static int
auth_frame_server_kbi(const uint8_t *user, size_t user_len, uint32_t count,
    const uint8_t **responses, const size_t *lengths, struct dssh_auth_challenge *out, void *cbdata)
{
	struct dssh_auth_prompt *items = NULL;

	(void)user;
	(void)user_len;
	(void)count;
	(void)responses;
	(void)lengths;
	struct auth_frame_probe *probe = cbdata;
	probe->calls++;
	if (probe->shape == 0)
		return DSSH_AUTH_SUCCESS;
	out->count = 1;
	if (probe->shape >= 2) {
		items = test_auth_lease_calloc(&out->lease, 1, sizeof(*items));
		if (items == NULL)
			return DSSH_AUTH_DISCONNECT;
		out->prompts = items;
		if (items == NULL)
			return DSSH_AUTH_DISCONNECT;
	}
	if (probe->shape == 3)
		items[0].text.len = 1;
	if (probe->shape == 4)
		items[0].text = test_auth_lease_text(&out->lease, "");
	return DSSH_AUTH_KBI_PROMPT;
}

static int
test_auth_frame_server(void)
{
	const char *methods[] = { "none", "password", "password", "keyboard-interactive", "publickey", "publickey" };
	for (unsigned variant = 0; variant < 6; variant++) {
		uint8_t extra[256];
		size_t pos = 0;
		if (variant == 1 || variant == 2) {
			extra[pos++] = variant == 1 ? 0 : 255;
			ASSERT_TRUE(auth_frame_string(extra, sizeof(extra), &pos, "", 0));
			if (variant == 2)
				ASSERT_TRUE(auth_frame_string(extra, sizeof(extra), &pos, "", 0));
		}
		else if (variant == 3) {
			ASSERT_TRUE(auth_frame_string(extra, sizeof(extra), &pos, "", 0));
			ASSERT_TRUE(auth_frame_string(extra, sizeof(extra), &pos, "", 0));
		}
		else if (variant >= 4) {
			extra[pos++] = variant == 4 ? 0 : 1;
			const char *algorithm = test_key_algo_name();
			ASSERT_TRUE(auth_frame_string(extra, sizeof(extra), &pos, algorithm, strlen(algorithm)));
			ASSERT_TRUE(auth_frame_string(extra, sizeof(extra), &pos, "key", 3));
			if (variant == 5)
				ASSERT_TRUE(auth_frame_string(extra, sizeof(extra), &pos, "signature", 9));
		}
		extra[pos++] = 0;
		uint8_t packet[512];
		size_t size = build_auth_request(methods[variant], strlen(methods[variant]), extra, pos, packet, sizeof(packet));
		ASSERT_TRUE(size > 0);
		struct test_auth_cbdata callbacks = { .none_result = DSSH_AUTH_SUCCESS, .password_result = DSSH_AUTH_SUCCESS,
		    .publickey_result = DSSH_AUTH_SUCCESS };
		struct dssh_auth_server_cbs cbs = { .methods_str = "password,publickey", .none_cb = test_none_cb,
		    .password_cb = test_password_cb, .publickey_cb = test_publickey_cb, .cbdata = &callbacks };
		struct auth_frame_probe probe = { 0 };
		if (variant == 3) {
			cbs.keyboard_interactive_cb = auth_frame_server_kbi;
			cbs.cbdata = &probe;
		}
		int auth_result = 0;
		ASSERT_OK(direct_server_test(packet, size, &cbs, NULL, NULL, &auth_result));
		ASSERT_EQ(auth_result, DSSH_ERROR_PARSE);
		ASSERT_FALSE(callbacks.none_called);
		ASSERT_FALSE(callbacks.password_called);
		ASSERT_FALSE(callbacks.publickey_called);
		ASSERT_EQ_U(probe.calls, 0);
	}
	for (unsigned shape = 1; shape < 5; shape++) {
		uint8_t extra[8] = { 0 };
		uint8_t packet[128];
		const char method[] = "keyboard-interactive";
		size_t size = build_auth_request(method, sizeof(method) - 1, extra, sizeof(extra), packet, sizeof(packet));
		ASSERT_TRUE(size > 0);
		struct auth_frame_probe probe = { .shape = shape };
		struct dssh_auth_server_cbs cbs = { .methods_str = method, .keyboard_interactive_cb = auth_frame_server_kbi,
		    .cbdata = &probe };
		int auth_result = 0;
		ASSERT_OK(direct_server_test(packet, size, &cbs, NULL, NULL, &auth_result));
		ASSERT_EQ(auth_result, DSSH_ERROR_INVALID);
		ASSERT_EQ_U(probe.calls, 1);
	}
	const char *invalid[] = { "none", "password,none", ",password", "password,", "p,,q", "p q", "p\177", "p\200" };
	dssh_test_reset_global_config();
	ASSERT_OK(dssh_transport_set_callbacks(capacity_io, capacity_io, NULL, NULL));
	dssh_session server = dssh_session_init(false, 0);
	ASSERT_NOT_NULL(server);
	size_t calls = 0;
	ASSERT_OK(dssh_session_set_cbdata(server, &calls, &calls, &calls, NULL));
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		struct dssh_auth_server_cbs cbs = { .methods_str = invalid[i] };
		ASSERT_EQ(dssh_auth_server(server, &cbs, NULL, NULL), DSSH_ERROR_INVALID);
		ASSERT_EQ_U(calls, 0);
		ASSERT_FALSE(dssh_session_is_terminated(server));
		ASSERT_FALSE(server->server_auth_active);
	}
	dssh_session_cleanup(server);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

struct auth_frame_config {
	struct dssh_auth_server_cbs *cbs;
	dssh_session sess;
	const char *replacement;
	unsigned calls;
	int banner_result;
};

static int
auth_frame_change_config(const uint8_t *user, size_t user_len, void *cbdata)
{
	(void)user;
	(void)user_len;
	struct auth_frame_config *config = cbdata;
	config->calls++;
	config->cbs->methods_str = config->replacement;
	config->banner_result = dssh_auth_set_banner(config->sess, "pending", "en");
	return DSSH_AUTH_FAILURE;
}

static int
test_auth_frame_config_update(void)
{
	const char *invalid[] = { NULL, "none", "p,,q" };
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
		struct handshake_ctx ctx;
		if (handshake_setup(&ctx) < 0) {
			handshake_cleanup(&ctx);
			return TEST_FAIL;
		}
		struct dssh_auth_server_cbs cbs = { .methods_str = "password", .none_cb = auth_frame_change_config };
		struct auth_frame_config config = { .cbs = &cbs, .sess = ctx.server, .replacement = invalid[i] };
		cbs.cbdata = &config;
		uint8_t packet[128] = { SSH_MSG_SERVICE_REQUEST };
		size_t length = 1;
		ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "ssh-userauth", 12));
		ASSERT_OK(send_packet(ctx.client, packet, length, NULL));
		length = build_auth_request("none", 4, NULL, 0, packet, sizeof(packet));
		ASSERT_TRUE(length > 0);
		ASSERT_OK(send_packet(ctx.client, packet, length, NULL));
		mock_io_close_c2s_write(&ctx.io);
		uint32_t sequence = ctx.server->trans.tx_seq;
		ASSERT_TRUE(sequence < UINT32_MAX);
		ASSERT_EQ(auth_server_impl(ctx.server, &cbs, NULL, NULL), DSSH_ERROR_INVALID);
		ASSERT_EQ_U(config.calls, 1);
		ASSERT_OK(config.banner_result);
		/* Invalid callback-updated configuration is rejected before the
		 * pending banner or any authentication reply is transmitted. */
		ASSERT_EQ_U(ctx.server->trans.tx_seq, sequence + 1);
		ASSERT_NOT_NULL(ctx.server->pending_banner);
		ASSERT_STR_EQ(ctx.server->pending_banner, "pending");
		ASSERT_FALSE(ctx.server->server_auth_active);
		ASSERT_FALSE(tx_owner_is_current(ctx.server));
		ASSERT_FALSE(dssh_session_is_terminated(ctx.server));
		handshake_cleanup(&ctx);
	}
	return TEST_PASS;
}

#endif
