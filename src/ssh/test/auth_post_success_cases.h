#include <stdatomic.h>

struct auth_post_observer {
	atomic_uint unimplemented;
	atomic_uint globals;
	atomic_bool valid;
};

static void
auth_post_unimplemented(uint32_t sequence, void *opaque)
{
	(void)sequence;
	struct auth_post_observer *observer = opaque;
	atomic_fetch_add(&observer->unimplemented, 1);
}

static int
auth_post_global(const uint8_t *name, size_t name_length, bool reply,
    const uint8_t *data, size_t data_length, void *opaque)
{
	(void)data;
	struct auth_post_observer *observer = opaque;
	if (!reply || name_length != 10 || memcmp(name, "audit-sync", 10) != 0 || data_length != 0)
		atomic_store(&observer->valid, false);
	atomic_fetch_add(&observer->globals, 1);
	return DSSH_ERROR_REJECTED;
}

/* Use actual public completion; do not manufacture the session's phase. */
static int
auth_post_complete(struct handshake_ctx *ctx)
{
	struct test_auth_cbdata data = { .none_result = DSSH_AUTH_SUCCESS };
	struct auth_server_arg server = { .ctx = ctx };
	server.cbs.methods_str = "";
	server.cbs.none_cb = test_none_cb;
	server.cbs.cbdata = &data;
	thrd_t worker;
	if (thrd_create(&worker, auth_server_thread, &server) != thrd_success)
		return DSSH_ERROR_INIT;
	int result = dssh_auth_begin(ctx->client, "fixture");
	if (result != DSSH_AUTH_SUCCESS) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	if (thrd_join(worker, NULL) != thrd_success)
		abort();
	if (result != DSSH_AUTH_SUCCESS || server.result != DSSH_AUTH_SUCCESS || !data.none_called)
		return DSSH_ERROR_INIT;
	return 0;
}

static int
auth_post_one(bool wire, bool complete, bool client_target)
{
	struct handshake_ctx ctx;
	int result = handshake_setup(&ctx);
	if (result < 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	struct auth_post_observer observer;
	atomic_init(&observer.unimplemented, 0);
	atomic_init(&observer.globals, 0);
	atomic_init(&observer.valid, true);
	dssh_session target = client_target ? ctx.client : ctx.server;
	dssh_session peer = client_target ? ctx.server : ctx.client;
	bool valid = dssh_session_set_unimplemented_cb(peer, auth_post_unimplemented, &observer) == 0
	    && dssh_session_set_global_request_cb(target, auth_post_global, &observer) == 0;
	if (valid && complete)
		valid = auth_post_complete(&ctx) == 0;
	if (valid && wire)
		valid = complete && dssh_session_start(target) == 0;

	/* Exercise every authentication message number, both minimal and opaque
	 * bodies. No credential material or method-specific success is involved. */
	unsigned packets = 0;
	bool bodies_valid = true;
	for (unsigned body = 0; body < 2 && valid; body++) {
		for (unsigned type = SSH_MSG_USERAUTH_REQUEST; type < SSH_MSG_GLOBAL_REQUEST && valid; type++) {
			uint8_t packet[33], original[33];
			memset(packet, 'x', sizeof(packet));
			uint8_t message = (uint8_t)type;
			packet[0] = message;
			memcpy(original, packet, sizeof(original));
			size_t length = body == 0 ? 1 : sizeof(packet);
			result = wire ? send_packet(peer, packet, length, NULL)
			    : dssh_test_demux_dispatch(target, message, packet, length);
			valid = result == 0;
			if (!wire) {
				bool cleared = complete && !client_target && message == SSH_MSG_USERAUTH_REQUEST;
				for (size_t i = 0; i < length; i++)
					bodies_valid &= packet[i] == (cleared ? 0 : original[i]);
			}
			packets++;
		}
	}
	/* Neighboring numbers retain the ordinary unknown-message route. The
	 * transport-owned GLOBAL_REQUEST itself is checked by the wire barrier. */
	if (!wire && valid) {
		const uint8_t boundaries[] = {49, 80, 255};
		for (size_t i = 0; i < sizeof(boundaries); i++) {
			uint8_t packet[] = {boundaries[i]};
			valid &= dssh_test_demux_dispatch(target, packet[0], packet, sizeof(packet)) == 0;
		}
	}
	if (valid) {
		if (wire) {
			const uint8_t barrier[] = {
				SSH_MSG_GLOBAL_REQUEST, 0, 0, 0, 10,
				'a', 'u', 'd', 'i', 't', '-', 's', 'y', 'n', 'c', 1,
			};
			valid = send_packet(peer, barrier, sizeof(barrier), NULL) == 0;
		}
		else {
			uint8_t response = SSH_MSG_REQUEST_FAILURE;
			valid = send_packet(target, &response, 1, NULL) == 0;
		}
	}
	if (valid) {
		uint8_t type, *payload;
		size_t length;
		valid = recv_packet(peer, &type, &payload, &length) == 0
		    && type == SSH_MSG_REQUEST_FAILURE && length == 1;
	}
	unsigned replies = atomic_load(&observer.unimplemented);
	unsigned expected = complete && !client_target ? 58 : 60;
	if (!wire)
		expected += 3;
	valid &= bodies_valid && packets == 60 && replies == expected && atomic_load(&observer.valid)
	    && atomic_load(&observer.globals) == (wire ? 1u : 0u)
	    && !dssh_session_is_terminated(target) && !dssh_session_is_terminated(peer);
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	handshake_cleanup(&ctx);
	printf("auth_post_success wire=%d complete=%d client_target=%d packets=%u replies=%u expected=%u valid=%d\n",
	    wire, complete, client_target, packets, replies, expected, valid);
	return valid ? TEST_PASS : TEST_FAIL;
}

static int
test_auth_post_success_direct(void)
{
	bool valid = true;
	for (unsigned complete = 0; complete < 2; complete++) {
		for (unsigned client = 0; client < 2; client++) {
			int result = auth_post_one(false, complete != 0, client != 0);
			valid &= result == TEST_PASS;
		}
	}
	return valid ? TEST_PASS : TEST_FAIL;
}

static int
test_auth_post_success_wire(void)
{
	bool valid = true;
	for (unsigned client = 0; client < 2; client++) {
		int result = auth_post_one(true, true, client != 0);
		valid &= result == TEST_PASS;
	}
	return valid ? TEST_PASS : TEST_FAIL;
}
