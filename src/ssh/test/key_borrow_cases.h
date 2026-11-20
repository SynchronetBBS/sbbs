/* A key module may reuse its own non-leased public-export scratch on its next
 * callback. Signed/emitted bytes must come from the same captured value. */
#ifndef DSSH_TEST_KEY_BORROW_CASES_H
#define DSSH_TEST_KEY_BORROW_CASES_H

struct key_borrow_probe {
	dssh_key_algo_pubkey pubkey;
	dssh_key_algo_sign sign;
	const uint8_t *canonical;
	size_t length;
	uint8_t *storage;
	size_t capacity;
	unsigned exports, signatures, authorized;
	bool reuse;
};
static struct key_borrow_probe *key_borrow_probe;

static int
key_borrow_pubkey(const uint8_t **out, size_t *length, dssh_key_algo_ctx *context)
{
	struct key_borrow_probe *probe = key_borrow_probe;
	const uint8_t *source = NULL;
	size_t size = 0;
	int ret = probe->pubkey(&source, &size, context);
	if (ret != 0)
		return ret;
	/* All three configured native keys fit this private fixture bound. */
	if (source == NULL || size == 0 || size > 16384)
		return DSSH_ERROR_INVALID;
	if (size > probe->capacity) {
		uint8_t *storage = realloc(probe->storage, size);
		if (storage == NULL)
			return DSSH_ERROR_ALLOC;
		probe->storage = storage;
		probe->capacity = size;
	}
	memcpy(probe->storage, source, size);
	probe->canonical = source;
	probe->length = size;
	probe->exports++;
	*out = probe->storage;
	*length = size;
	return 0;
}

static int
key_borrow_sign(struct dssh_bytes_result *out, const uint8_t *data,
    size_t length, dssh_key_algo_ctx *context)
{
	struct key_borrow_probe *probe = key_borrow_probe;
	int ret = probe->sign(out, data, length, context);
	probe->signatures++;
	if (probe->reuse)
		dssh_cleanse(probe->storage, probe->capacity);
	return ret;
}

static int
key_borrow_register(void)
{
	dssh_key_algo key = find_key_algo(test_key_algo_name());
	if (key == NULL || key->pubkey == NULL || key->sign == NULL)
		return DSSH_ERROR_INIT;
	key_borrow_probe->pubkey = key->pubkey;
	key_borrow_probe->sign = key->sign;
	/* Registration setup is serialized, before either session exists. */
	key->pubkey = key_borrow_pubkey;
	key->sign = key_borrow_sign;
	return 0;
}

static int
key_borrow_authorize(const uint8_t *user, size_t user_len, const char *algorithm,
    const uint8_t *blob, size_t length, bool signed_request, void *data)
{
	struct key_borrow_probe *probe = data;
	probe->authorized++;
	return signed_request && user_len == 4 && memcmp(user, "user", 4) == 0
	    && strcmp(algorithm, test_key_algo_name()) == 0 && length == probe->length
	    && memcmp(blob, probe->canonical, length) == 0 ? DSSH_AUTH_SUCCESS : DSSH_AUTH_FAILURE;
}

static bool
key_borrow_case(bool authentication, bool reuse)
{
	struct key_borrow_probe probe = {.reuse = !authentication && reuse};
	key_borrow_probe = &probe;
	extra_algorithm_registration = key_borrow_register;
	struct handshake_ctx ctx;
	int setup = handshake_setup(&ctx);
	extra_algorithm_registration = NULL;
	bool valid = setup == 0 && probe.exports == 1 && probe.signatures == 1;
	if (valid && authentication) {
		/* The completed handshake leaves this manual peer as sole reader.
		 * Isolate request generation, then run the actual server verifier. */
		ctx.client->auth_service_requested = true;
		probe.reuse = reuse;
		uint8_t success = SSH_MSG_USERAUTH_SUCCESS;
		valid = send_packet(ctx.server, &success, 1, NULL) == 0;
		int result = valid ? dssh_auth_publickey(ctx.client, "user", test_key_algo_name())
		    : DSSH_ERROR_INIT;
		valid = valid && result == DSSH_AUTH_SUCCESS;
		uint8_t type = 0;
		uint8_t *payload = NULL;
		size_t length = 0;
		if (valid)
			valid = recv_packet(ctx.server, &type, &payload, &length) == 0
			    && type == SSH_MSG_USERAUTH_REQUEST;
		if (valid) {
			const uint8_t *user = NULL, *method = NULL;
			size_t user_len = 0, method_len = 0;
			int64_t at = parse_userauth_prefix(payload, length, &user, &user_len, &method, &method_len);
			valid = at >= 0 && user_len == 4 && method_len == 9
			    && memcmp(user, "user", 4) == 0 && memcmp(method, "publickey", 9) == 0;
#if SIZE_MAX < INT64_MAX
			if (at > SIZE_MAX)
				valid = false;
#endif
			if (valid) {
				size_t offset = (size_t)at;
				struct dssh_auth_server_cbs callbacks = {.methods_str = "publickey",
				    .publickey_cb = key_borrow_authorize, .cbdata = &probe};
				valid = handle_auth_publickey(ctx.server, &callbacks, user, user_len,
				    payload, length, offset) == 1;
			}
		}
		if (valid)
			valid = recv_packet(ctx.client, &type, &payload, &length) == 0
			    && type == SSH_MSG_USERAUTH_SUCCESS && length == 1;
		valid = valid && probe.exports == 2 && probe.signatures == 2 && probe.authorized == 1;
	}
	if (printf("key_borrow auth=%u reuse=%u setup=%d exports=%u signatures=%u authorized=%u valid=%u\n",
	    authentication ? 1U : 0U, reuse ? 1U : 0U, setup, probe.exports,
	    probe.signatures, probe.authorized, valid ? 1U : 0U) < 0)
		valid = false;
	/* No assertion can bypass the callback borrow or descriptor/session owner.
	 * Both native handshake workers have joined on every setup return. */
	handshake_cleanup(&ctx);
	key_borrow_probe = NULL;
	free(probe.storage);
	return valid;
}

static int
test_key_borrow_kex(void)
{
	for (unsigned reuse = 0; reuse < 2; reuse++) {
		bool valid = key_borrow_case(false, reuse != 0);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

static int
test_key_borrow_auth(void)
{
	for (unsigned reuse = 0; reuse < 2; reuse++) {
		bool valid = key_borrow_case(true, reuse != 0);
		ASSERT_TRUE(valid);
	}
	return TEST_PASS;
}

#endif
