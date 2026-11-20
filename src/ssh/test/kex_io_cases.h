#ifndef DSSH_TEST_KEX_IO_CASES_H
#define DSSH_TEST_KEX_IO_CASES_H

#define KEX_IO_NAME "checked-curve25519-sha256"
struct kex_io_probe {
	dssh_kex delegate;
	unsigned calls[2];
	unsigned checks[2];
	bool valid[2];
};
static struct kex_io_probe *kex_io_current;

static int
kex_io_handler(const struct dssh_kex_context *ctx, struct dssh_kex_result *out)
{
	struct kex_io_probe *probe = kex_io_current;
	unsigned role = ctx->client ? 0U : 1U;
	probe->calls[role]++;
	dssh_session sess = ctx->io_ctx;
	uint32_t tx_seq = sess->trans.tx_seq;
	uint32_t rx_seq = sess->trans.rx_seq;
	uint8_t byte = SSH_MSG_KEX_ECDH_INIT;
	int ret;
	for (unsigned mode = 0; mode < 8; mode++) {
		uint8_t type = 0xA5;
		uint8_t *payload = &byte;
		size_t length = 17;
		switch (mode) {
			case 0:
				ret = ctx->send(NULL, 1, ctx->io_ctx);
				break;
			case 1:
				ret = ctx->send(&byte, 0, ctx->io_ctx);
				break;
			case 2:
				ret = ctx->send(NULL, 0, ctx->io_ctx);
				break;
			case 3:
				ret = ctx->send(&byte, 1, NULL);
				break;
			case 4:
				ret = ctx->recv(NULL, &payload, &length, ctx->io_ctx);
				break;
			case 5:
				ret = ctx->recv(&type, NULL, &length, ctx->io_ctx);
				break;
			case 6:
				ret = ctx->recv(&type, &payload, NULL, ctx->io_ctx);
				break;
			default:
				ret = ctx->recv(&type, &payload, &length, NULL);
				break;
		}
		probe->checks[role]++;
		probe->valid[role] = probe->valid[role] && ret == DSSH_ERROR_INVALID
		    && type == 0xA5 && payload == &byte && length == 17
		    && sess->trans.tx_seq == tx_seq && sess->trans.rx_seq == rx_seq
		    && !dssh_session_is_terminated(sess);
		if (!probe->valid[role])
			return DSSH_ERROR_INVALID;
	}
	return probe->delegate->handler(ctx, out);
}

static int
kex_io_register(void)
{
	int ret;
	dssh_kex delegate = gconf.kex_head;
	while (delegate != NULL && strcmp(delegate->name, "curve25519-sha256") != 0)
		delegate = delegate->next;
	if (delegate == NULL) {
		ret = dssh_register_curve25519_sha256();
		if (ret != 0)
			return ret;
		delegate = gconf.kex_tail;
	}
	kex_io_current->delegate = delegate;
	if (sizeof(struct dssh_kex_s) > SIZE_MAX - sizeof(KEX_IO_NAME))
		return DSSH_ERROR_INVALID;
	dssh_kex proxy = malloc(sizeof(*proxy) + sizeof(KEX_IO_NAME));
	if (proxy == NULL)
		return DSSH_ERROR_ALLOC;
	proxy->next = NULL;
	proxy->init = delegate->init;
	proxy->handler = kex_io_handler;
	proxy->cleanup = delegate->cleanup;
	proxy->hash_name = delegate->hash_name;
	proxy->flags = delegate->flags;
	proxy->config = delegate->config;
	memcpy(proxy->name, KEX_IO_NAME, sizeof(KEX_IO_NAME));
	ret = dssh_transport_register_kex(proxy);
	if (ret != 0)
		free(proxy);
	return ret;
}

static bool
kex_io_exchange(struct handshake_ctx *pair, bool from_client, const uint8_t *packet, size_t length)
{
	dssh_session tx = from_client ? pair->client : pair->server;
	dssh_session rx = from_client ? pair->server : pair->client;
	uint8_t type = 0;
	uint8_t *received = NULL;
	size_t received_len = 0;
	int ret = send_packet(tx, packet, length, NULL);
	if (ret == 0)
		ret = recv_packet(rx, &type, &received, &received_len);
	return ret == 0 && type == packet[0] && received != NULL
	    && received_len == length && memcmp(received, packet, length) == 0;
}

static int
test_kex_io_arguments(void)
{
	struct kex_io_probe probe = {.valid = {true, true}};
	struct handshake_ctx pair;
	kex_io_current = &probe;
	extra_algorithm_registration = kex_io_register;
	handshake_kex_filter = KEX_IO_NAME;
	int ret = handshake_setup(&pair);
	extra_algorithm_registration = NULL;
	handshake_kex_filter = NULL;
	bool valid = ret == 0;
	uint8_t request[] = {SSH_MSG_SERVICE_REQUEST, 0, 0, 0, 12,
	    's', 's', 'h', '-', 'u', 's', 'e', 'r', 'a', 'u', 't', 'h'};
	if (valid)
		valid = kex_io_exchange(&pair, true, request, sizeof(request))
		    && compression_rekey_pair(&pair);
	request[0] = SSH_MSG_SERVICE_ACCEPT;
	if (valid)
		valid = kex_io_exchange(&pair, false, request, sizeof(request));
	valid = valid && probe.valid[0] && probe.valid[1]
	    && probe.calls[0] == 2 && probe.calls[1] == 2
	    && probe.checks[0] == 16 && probe.checks[1] == 16;
	handshake_cleanup(&pair);
	kex_io_current = NULL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#endif
