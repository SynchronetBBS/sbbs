#ifndef DSSH_AUTH_CALLBACK_CASES_H
#define DSSH_AUTH_CALLBACK_CASES_H

static const uint8_t auth_callback_secret[] = "credential-value";

struct auth_callback_probe {
	struct auth_test_io_watch *io;
	unsigned calls;
	int result;
	unsigned mode;
	bool inputs_valid;
	bool checked;
	bool clean;
	bool rx_clean;
	bool freed;
	bool cleared;
	bool signed_call;
	int peer_result;
	unsigned tx_calls;
	unsigned result_releases;
	dssh_session sess;
};

static void
auth_callback_prompt_released(const void *storage, size_t size, void *data)
{
	struct auth_callback_probe *probe = data;
	probe->result_releases++;
	probe->freed = true;
	probe->cleared = auth_test_bytes_zero(storage, size);
}

struct auth_callback_peer {
	struct handshake_ctx *ctx;
	uint8_t request[2048];
	size_t request_len;
	unsigned kind;
	bool signed_client;
	bool unknown;
	bool mismatch;
	int result;
};

/* A scripted native peer waits for SERVICE_ACCEPT and, for KBI, the actual
 * INFO_REQUEST before answering. No response data is injected out of phase. */
static int
auth_callback_peer_thread(void *arg)
{
	struct auth_callback_peer *peer = arg;
	if (peer->signed_client) {
		peer->result = auth_publickey_impl(peer->ctx->client, "u", test_key_algo_name());
		mock_io_close_c2s_write(&peer->ctx->io);
		return 0;
	}
	uint8_t service[17] = { SSH_MSG_SERVICE_REQUEST };
	size_t pos = 1;
	if (!auth_frame_string(service, sizeof(service), &pos, "ssh-userauth", 12)) {
		peer->result = -999;
		mock_io_close_c2s_write(&peer->ctx->io);
		return 0;
	}
	int ret = send_packet(peer->ctx->client, service, pos, NULL);
	uint8_t type;
	uint8_t *payload;
	size_t length;
	if (ret == 0) {
		ret = recv_packet(peer->ctx->client, &type, &payload, &length);
		if (ret == 0 && type != SSH_MSG_SERVICE_ACCEPT)
			ret = DSSH_ERROR_PARSE;
	}
	if (ret == 0)
		ret = send_packet(peer->ctx->client, peer->request, peer->request_len, NULL);
	if (ret == 0 && peer->kind == 3) {
		ret = recv_packet(peer->ctx->client, &type, &payload, &length);
		if (ret == 0 && type != SSH_MSG_USERAUTH_INFO_REQUEST)
			ret = DSSH_ERROR_PARSE;
		if (ret == 0 && peer->unknown) {
			uint8_t unknown = 59;
			ret = send_packet(peer->ctx->client, &unknown, 1, NULL);
		}
		if (ret == 0) {
			uint8_t response[64] = { SSH_MSG_USERAUTH_INFO_RESPONSE };
			pos = 1;
			uint32_t count = peer->mismatch ? 0 : 1;
			DSSH_PUT_U32(count, response, &pos);
			if (!auth_frame_string(response, sizeof(response), &pos,
			    auth_callback_secret, sizeof(auth_callback_secret) - 1))
				ret = -999;
			else
				ret = send_packet(peer->ctx->client, response, pos, NULL);
		}
	}
	peer->result = ret;
	mock_io_close_c2s_write(&peer->ctx->io);
	return 0;
}

/* kind: 0 none, 1 password, 2 password change, 3 KBI, 4 probe, 5 signed key. */
static int
auth_callback_server(unsigned kind, struct dssh_auth_server_cbs *cbs,
    struct auth_callback_probe *probe, bool malformed, unsigned failure)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) < 0) {
		handshake_cleanup(&ctx);
		return -999;
	}
	struct auth_callback_peer peer = { .ctx = &ctx, .kind = kind, .signed_client = kind == 5,
	    .unknown = failure == 2, .mismatch = failure == 3, .result = -999 };
	uint8_t extra[1536];
	size_t length = 0;
	const char *method = kind == 0 ? "none" : kind <= 2 ? "password" :
	    kind == 3 ? "keyboard-interactive" : "publickey";
	bool valid = true;
	if (kind == 1 || kind == 2) {
		extra[length++] = kind == 2 ? 255 : 0;
		valid = auth_frame_string(extra, sizeof(extra), &length,
		    auth_callback_secret, sizeof(auth_callback_secret) - 1);
		if (valid && kind == 2)
			valid = auth_frame_string(extra, sizeof(extra), &length,
			    auth_callback_secret, sizeof(auth_callback_secret) - 1);
	}
	else if (kind == 3) {
		valid = auth_frame_string(extra, sizeof(extra), &length, "", 0)
		    && auth_frame_string(extra, sizeof(extra), &length, "", 0);
	}
	else if (kind == 4) {
		const char *algorithm = test_key_algo_name();
		dssh_key_algo ka = find_key_algo(algorithm);
		const uint8_t *blob = NULL;
		size_t blob_len = 0;
		valid = ka != NULL && ka->pubkey(&blob, &blob_len, ka->ctx) == 0;
		extra[length++] = 0;
		valid = valid && auth_frame_string(extra, sizeof(extra), &length, algorithm, strlen(algorithm))
		    && auth_frame_string(extra, sizeof(extra), &length, blob, blob_len);
	}
	if (malformed)
		extra[length++] = 0;
	if (!valid) {
		handshake_cleanup(&ctx);
		return -999;
	}
	peer.request_len = build_auth_request(method, strlen(method), extra, length,
	    peer.request, sizeof(peer.request));
	struct auth_test_io_watch watch = { .target = ctx.server, .clean = true,
	    .fail_enabled = failure != 0, .fail_at = failure == 1 ? 1 : 2 };
	probe->io = &watch;
	probe->sess = ctx.server;
	cbs->cbdata = probe;
	auth_test_io_watch = &watch;
	thrd_t thread;
	int created = thrd_create(&thread, auth_callback_peer_thread, &peer);
	if (created != thrd_success) {
		auth_test_io_watch = NULL;
		probe->io = NULL;
		handshake_cleanup(&ctx);
		return -999;
	}
	int ret = auth_server_impl(ctx.server, cbs, NULL, NULL);
	probe->rx_clean = auth_test_bytes_zero(ctx.server->trans.rx_packet, 4)
	    && auth_test_bytes_zero(&ctx.server->trans.rx_packet[4], ctx.server->trans.packet_buf_sz);
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	if (thrd_join(thread, NULL) != thrd_success)
		abort();
	auth_test_io_watch = NULL;
	probe->checked = watch.checked;
	probe->clean = watch.clean;
	probe->tx_calls = watch.calls;
	probe->peer_result = peer.result;
	probe->io = NULL;
	probe->sess = NULL;
	handshake_cleanup(&ctx);
	return ret;
}

static int
auth_callback_password(const uint8_t *user, size_t user_len, const uint8_t *password,
    size_t password_len, struct dssh_bytes_result *output_result, void *cbdata)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **prompt = &fixture_output;
	size_t *prompt_len = &fixture_length;
	(void)prompt;
	(void)prompt_len;

	(void)user;
	(void)user_len;
	struct auth_callback_probe *probe = cbdata;
	probe->calls++;
	probe->inputs_valid = probe->inputs_valid && password_len == sizeof(auth_callback_secret) - 1
	    && memcmp(password, auth_callback_secret, password_len) == 0;
	probe->io->secret = password;
	probe->io->secret_len = password_len;
	probe->io->armed = true;
	if (probe->result == DSSH_AUTH_CHANGE_PASSWORD && probe->mode != 7) {
		*prompt = test_auth_output_alloc(output_result, 1);
		if (*prompt == NULL)
			return test_auth_output_finish(output_result, fixture_output, fixture_length, DSSH_AUTH_DISCONNECT);
		(*prompt)[0] = 0;
		*prompt_len = 0;
		struct test_auth_output *owner = output_result->lease.owner;
		owner->observe_release = auth_callback_prompt_released;
		owner->observe_data = probe;
		probe->io->produced_results = &probe->calls;
		probe->io->released_results = &probe->result_releases;
	}
	return test_auth_output_finish(output_result, fixture_output, fixture_length, probe->result);
}

static int
auth_callback_password_change(const uint8_t *user, size_t user_len,
    const uint8_t *old_password, size_t old_len, const uint8_t *new_password, size_t new_len,
    struct dssh_bytes_result *output_result, void *cbdata)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **prompt = &fixture_output;
	size_t *prompt_len = &fixture_length;
	(void)prompt;
	(void)prompt_len;

	struct auth_callback_probe *probe = cbdata;
	probe->io->second_secret = new_password;
	probe->io->second_secret_len = new_len;
	probe->inputs_valid = probe->inputs_valid && new_len == sizeof(auth_callback_secret) - 1
	    && memcmp(new_password, auth_callback_secret, new_len) == 0;
	return auth_callback_password(user, user_len, old_password, old_len, output_result, cbdata);
}

static int
test_auth_callback_password_lifetime(void)
{
	const int outcomes[] = { DSSH_AUTH_SUCCESS, DSSH_AUTH_FAILURE, DSSH_AUTH_DISCONNECT,
	    42, DSSH_AUTH_SUCCESS, DSSH_AUTH_SUCCESS, DSSH_AUTH_CHANGE_PASSWORD, DSSH_AUTH_CHANGE_PASSWORD };
	for (unsigned mode = 0; mode < 8; mode++) {
		struct auth_callback_probe probe = { .result = outcomes[mode], .mode = mode, .inputs_valid = true };
		struct dssh_auth_server_cbs cbs = { .methods_str = "password", .password_cb = auth_callback_password };
		if (mode == 4) {
			cbs.password_cb = NULL;
			cbs.passwd_change_cb = auth_callback_password_change;
		}
		int ret = auth_callback_server(mode == 4 ? 2 : 1, &cbs, &probe, mode == 5, 0);
		if (mode == 0 || mode == 4)
			ASSERT_OK(ret);
		else if (mode == 3 || mode == 7)
			ASSERT_EQ(ret, DSSH_ERROR_INVALID);
		else if (mode == 5)
			ASSERT_EQ(ret, DSSH_ERROR_PARSE);
		else
			ASSERT_TRUE(ret < 0);
		ASSERT_EQ_U(probe.calls, mode == 5 ? 0 : 1);
		ASSERT_TRUE(probe.inputs_valid);
		ASSERT_TRUE(probe.rx_clean);
		if (mode == 6)
			ASSERT_TRUE(probe.freed && probe.cleared && probe.result_releases == 1);
		if (mode != 3 && mode != 5 && mode != 7) {
			ASSERT_TRUE(probe.checked);
			ASSERT_TRUE(probe.clean);
		}
	}
	return TEST_PASS;
}

static int
auth_callback_kbi(const uint8_t *user, size_t user_len, uint32_t count,
    const uint8_t **responses, const size_t *lengths, struct dssh_auth_challenge *out, void *cbdata)
{
	struct dssh_auth_prompt *items = NULL;

	(void)user;
	(void)user_len;
	(void)out;
	struct auth_callback_probe *probe = cbdata;
	probe->calls++;
	if (probe->calls == 1) {
		out->count = 1;
		items = test_auth_lease_calloc(&out->lease, 1, sizeof(*items));
		out->prompts = items;

		if (items == NULL)
			return DSSH_AUTH_DISCONNECT;
		items[0].text = test_auth_lease_text(&out->lease, "Code: ");
		if (items[0].text.data == NULL)
			return DSSH_AUTH_DISCONNECT;
		return DSSH_AUTH_KBI_PROMPT;
	}
	probe->inputs_valid = probe->inputs_valid && count == 1
	    && lengths[0] == sizeof(auth_callback_secret) - 1
	    && memcmp(responses[0], auth_callback_secret, lengths[0]) == 0;
	if (count == 1) {
		probe->io->secret = responses[0];
		probe->io->secret_len = lengths[0];
		probe->io->armed = true;
	}
	return probe->result;
}

static int
test_auth_callback_kbi_lifetime(void)
{
	const int results[] = { DSSH_AUTH_SUCCESS, DSSH_AUTH_FAILURE, DSSH_AUTH_DISCONNECT, 42 };
	for (size_t i = 0; i < sizeof(results) / sizeof(results[0]); i++) {
		struct auth_callback_probe probe = { .result = results[i], .inputs_valid = true };
		struct dssh_auth_server_cbs cbs = { .methods_str = "keyboard-interactive", .keyboard_interactive_cb = auth_callback_kbi };
		int ret = auth_callback_server(3, &cbs, &probe, false, 0);
		if (i == 0)
			ASSERT_OK(ret);
		else if (i == 3)
			ASSERT_EQ(ret, DSSH_ERROR_INVALID);
		else
			ASSERT_TRUE(ret < 0);
		ASSERT_EQ_U(probe.calls, 2);
		ASSERT_TRUE(probe.inputs_valid);
		ASSERT_TRUE(probe.rx_clean);
		if (i != 3) {
			ASSERT_TRUE(probe.checked);
			ASSERT_TRUE(probe.clean);
		}
	}
	return TEST_PASS;
}

static int
test_auth_callback_reply_failures(void)
{
	for (unsigned failure = 1; failure <= 3; failure++) {
		struct auth_callback_probe probe = { .result = DSSH_AUTH_SUCCESS, .inputs_valid = true };
		struct dssh_auth_server_cbs cbs = { .methods_str = "keyboard-interactive", .keyboard_interactive_cb = auth_callback_kbi };
		ASSERT_EQ(auth_callback_server(3, &cbs, &probe, false, failure), DSSH_ERROR_INIT);
		ASSERT_EQ_U(probe.calls, 1);
		ASSERT_EQ_U(probe.tx_calls, failure == 1 ? 2 : 3);
		ASSERT_TRUE(probe.rx_clean);
	}
	return TEST_PASS;
}

static int
auth_callback_none(const uint8_t *user, size_t user_len, void *cbdata)
{
	(void)user;
	(void)user_len;
	struct auth_callback_probe *probe = cbdata;
	probe->calls++;
	return probe->result;
}

static int
auth_callback_key(const uint8_t *user, size_t user_len, const char *algorithm,
    const uint8_t *blob, size_t blob_len, bool signed_request, void *cbdata)
{
	(void)user;
	(void)user_len;
	(void)algorithm;
	(void)blob;
	(void)blob_len;
	struct auth_callback_probe *probe = cbdata;
	probe->calls++;
	probe->signed_call = signed_request;
	return probe->result;
}

static int
test_auth_callback_results(void)
{
	const unsigned kinds[] = { 0, 4, 5 };
	const int invalid[] = { 42, DSSH_AUTH_KBI_PROMPT };
	for (size_t i = 0; i < sizeof(kinds) / sizeof(kinds[0]); i++) {
		for (size_t j = 0; j < sizeof(invalid) / sizeof(invalid[0]); j++) {
			struct auth_callback_probe probe = { .result = invalid[j], .inputs_valid = true };
			struct dssh_auth_server_cbs cbs = { .methods_str = "publickey", .none_cb = auth_callback_none,
			    .publickey_cb = auth_callback_key };
			ASSERT_EQ(auth_callback_server(kinds[i], &cbs, &probe, false, 0), DSSH_ERROR_INVALID);
			ASSERT_EQ_U(probe.calls, 1);
			ASSERT_EQ(probe.signed_call, kinds[i] == 5);
			ASSERT_TRUE(probe.rx_clean);
		}
	}
	return TEST_PASS;
}

static void
auth_callback_observe_output(const void *storage, size_t size, void *data)
{
	struct auth_callback_probe *probe = data;
	probe->freed = true;
	probe->cleared = auth_test_bytes_zero(storage, size);
}

static int
auth_callback_fill_response(uint8_t **value, size_t *length, struct auth_callback_probe *probe,
    struct dssh_result_lease *lease)
{
	size_t size = probe->mode == 3 ? probe->sess->trans.packet_buf_sz : sizeof(auth_callback_secret) - 1;
	if (probe->mode >= 5)
		size = 1;
	*value = test_auth_lease_alloc(lease, size);
	if (*value == NULL)
		return DSSH_ERROR_ALLOC;
	memset(*value, 's', size);
	*length = size;
	if (probe->mode >= 5)
		*length = probe->mode == 8 ? probe->sess->trans.packet_buf_sz + 1 : SIZE_MAX;
	{
		struct test_auth_output *owner = lease->owner;
		owner->observe_release = auth_callback_observe_output;
		owner->observe_data = probe;
	}
	return probe->mode == 1 || probe->mode == 6 ? -77 :
	    probe->mode == 2 || probe->mode == 7 ? 7 : 0;
}

static int
auth_callback_client_change(const uint8_t *prompt, size_t prompt_len, const uint8_t *language,
    size_t language_len, struct dssh_bytes_result *output_result, void *cbdata)
{
	uint8_t *fixture_output = NULL;
	size_t fixture_length = 0;
	uint8_t **value = &fixture_output;
	size_t *length = &fixture_length;
	(void)value;
	(void)length;

	(void)prompt;
	(void)prompt_len;
	(void)language;
	(void)language_len;
	struct auth_callback_probe *probe = cbdata;
	probe->calls++;
	int ret = auth_callback_fill_response(value, length, probe, &output_result->lease);
	return test_auth_output_finish(output_result, fixture_output, fixture_length, ret);
}

static int
auth_callback_client_prompt(const uint8_t *name, size_t name_len, const uint8_t *instruction,
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
	(void)prompts;
	(void)lengths;
	(void)echo;
	struct auth_callback_probe *probe = cbdata;
	probe->calls++;
	if (count != 1)
		return DSSH_ERROR_INVALID;
	uint8_t *response = NULL;
	size_t length = 0;
	int ret = auth_callback_fill_response(&response, &length, probe, &out->lease);
	answers[0].data = response;
	answers[0].len = length;
	return ret;
}

static int
test_auth_callback_client_cleanup(void)
{
	for (unsigned kbi = 0; kbi < 2; kbi++) {
		for (unsigned mode = 0; mode < 9; mode++) {
			struct handshake_ctx ctx;
			if (handshake_setup(&ctx) < 0) {
				handshake_cleanup(&ctx);
				return TEST_FAIL;
			}
			ctx.client->auth_service_requested = true;
			uint8_t packet[64] = { kbi != 0 ? SSH_MSG_USERAUTH_INFO_REQUEST : SSH_MSG_USERAUTH_PASSWD_CHANGEREQ };
			size_t length = 1;
			ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "", 0));
			ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "", 0));
			if (kbi != 0) {
				ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "", 0));
				DSSH_PUT_U32(1, packet, &length);
				ASSERT_TRUE(auth_frame_string(packet, sizeof(packet), &length, "x", 1));
				packet[length++] = 0;
			}
			ASSERT_OK(send_packet(ctx.server, packet, length, NULL));
			uint8_t success = SSH_MSG_USERAUTH_SUCCESS;
			ASSERT_OK(send_packet(ctx.server, &success, 1, NULL));
			mock_io_close_s2c_write(&ctx.io);
			struct auth_callback_probe probe = { .mode = mode, .sess = ctx.client };
			struct auth_test_io_watch watch = { .target = ctx.client, .clean = true,
			    .fail_enabled = mode == 4, .fail_at = 1 };
			auth_test_io_watch = &watch;
			int ret = kbi != 0 ? auth_kbi_impl(ctx.client, "u", auth_callback_client_prompt, &probe) :
			    auth_password_impl(ctx.client, "u", "password", auth_callback_client_change, &probe);
			auth_test_io_watch = NULL;
			dssh_test_alloc_reset();
			const int expected[] = { 0, -77, DSSH_ERROR_INVALID, DSSH_ERROR_TOOLONG, DSSH_ERROR_INIT,
			    DSSH_ERROR_INVALID, -77, DSSH_ERROR_INVALID, DSSH_ERROR_TOOLONG };
			ASSERT_EQ(ret, expected[mode]);
			ASSERT_EQ_U(probe.calls, 1);
			ASSERT_TRUE(probe.freed);
			ASSERT_TRUE(probe.cleared);
			ASSERT_FALSE(tx_owner_is_current(ctx.client));
			handshake_cleanup(&ctx);
		}
	}
	return TEST_PASS;
}

static int
test_auth_callback_banner_transaction(void)
{
	dssh_test_reset_global_config();
	dssh_session sess = dssh_session_init(false, 0);
	ASSERT_NOT_NULL(sess);
	ASSERT_OK(dssh_auth_set_banner(sess, "en-US", "fr"));
	for (int fail = 0; fail < 2; fail++) {
		char *old_message = sess->pending_banner;
		char *old_language = sess->pending_banner_lang;
		dssh_test_alloc_fail_after(fail);
		int ret = dssh_auth_set_banner(sess, "replacement", "de");
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
		ASSERT_TRUE(sess->pending_banner == old_message);
		ASSERT_TRUE(sess->pending_banner_lang == old_language);
		ASSERT_STR_EQ(sess->pending_banner, "en-US");
		ASSERT_STR_EQ(sess->pending_banner_lang, "fr");
	}
	ASSERT_EQ(dssh_auth_set_banner(sess, "", "de"), DSSH_ERROR_INVALID);
	ASSERT_STR_EQ(sess->pending_banner, "en-US");
	ASSERT_STR_EQ(sess->pending_banner_lang, "fr");
	ASSERT_OK(dssh_auth_set_banner(sess, sess->pending_banner, sess->pending_banner_lang));
	ASSERT_STR_EQ(sess->pending_banner, "en-US");
	ASSERT_STR_EQ(sess->pending_banner_lang, "fr");
	ASSERT_OK(dssh_auth_set_banner(sess, sess->pending_banner_lang, sess->pending_banner));
	ASSERT_STR_EQ(sess->pending_banner, "fr");
	ASSERT_STR_EQ(sess->pending_banner_lang, "en-US");
	ASSERT_OK(dssh_auth_set_banner(sess, sess->pending_banner + 1, sess->pending_banner_lang + 3));
	ASSERT_STR_EQ(sess->pending_banner, "r");
	ASSERT_STR_EQ(sess->pending_banner_lang, "US");
	ASSERT_OK(dssh_auth_set_banner(sess, NULL, sess->pending_banner));
	ASSERT_NULL(sess->pending_banner);
	ASSERT_NULL(sess->pending_banner_lang);
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

static int
test_auth_callback_output_pairs(void)
{
	dssh_test_reset_global_config();
	ASSERT_OK(dssh_transport_set_callbacks(capacity_io, capacity_io, NULL, NULL));
	dssh_session sess = dssh_session_init(false, 0);
	ASSERT_NOT_NULL(sess);
	size_t calls = 0;
	ASSERT_OK(dssh_session_set_cbdata(sess, &calls, &calls, &calls, NULL));
	struct dssh_auth_server_cbs cbs = { .methods_str = "password" };
	uint8_t output[8];
	size_t size = sizeof(output);
	ASSERT_EQ(dssh_auth_server(sess, &cbs, output, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_auth_server(sess, &cbs, NULL, &size), DSSH_ERROR_INVALID);
	ASSERT_EQ_U(calls, 0);
	ASSERT_EQ_U(size, sizeof(output));
	ASSERT_FALSE(sess->server_auth_active);
	ASSERT_FALSE(dssh_session_is_terminated(sess));
	dssh_session_cleanup(sess);
	dssh_test_reset_global_config();
	return TEST_PASS;
}

#endif
