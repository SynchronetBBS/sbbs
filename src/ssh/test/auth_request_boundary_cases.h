/* Selected server request contract over actual encrypted sessions. */
struct auth_boundary_observer {
	unsigned calls;
	unsigned releases;
	unsigned identities[4];
	bool zero_shapes[4];
	bool valid;
	dssh_session session;
	struct dssh_auth_server_cbs *cbs;
	unsigned requests;
	bool new_identities[8];
	size_t lengths[8];
	size_t capacities[8];
	unsigned released_at_request[8];
	unsigned requests_at_method[4];
	unsigned behavior;
	bool factor;
	bool fail_identity;
	bool allocation_failed;
	bool cleanup_requested;
};

enum {
	AUTH_BOUNDARY_NORMAL,
	AUTH_BOUNDARY_NEGATIVE,
	AUTH_BOUNDARY_POSITIVE,
	AUTH_BOUNDARY_TERMINATE,
	AUTH_BOUNDARY_BAD_METHODS,
	AUTH_BOUNDARY_CLEANUP,
	AUTH_BOUNDARY_PARTIAL,
	AUTH_BOUNDARY_STORAGE,
};

static int
auth_boundary_hook(const uint8_t *username, size_t length, const uint8_t *method,
    size_t method_length, bool new_identity, void *opaque)
{
	struct auth_boundary_observer *observer = opaque;
	unsigned index = observer->requests++;
	if (index >= 8 || username == NULL || method == NULL || method_length == 0)
		return DSSH_ERROR_INVALID;
	observer->valid &= username == observer->session->server_auth_user
	    && length == observer->session->server_auth_user_len;
	observer->new_identities[index] = new_identity;
	observer->lengths[index] = length;
	observer->capacities[index] = observer->session->server_auth_user_capacity;
	observer->released_at_request[index] = observer->releases;
	if (new_identity)
		observer->factor = false;
	switch (observer->behavior) {
		case AUTH_BOUNDARY_NEGATIVE:
			return DSSH_ERROR_REJECTED;
		case AUTH_BOUNDARY_POSITIVE:
			return 1;
		case AUTH_BOUNDARY_TERMINATE:
			dssh_session_terminate(observer->session);
			break;
		case AUTH_BOUNDARY_BAD_METHODS:
			observer->cbs->methods_str = "password,,publickey";
			break;
		case AUTH_BOUNDARY_CLEANUP:
			dssh_test_alloc_watch_free(observer->session->server_auth_user,
			    observer->session->server_auth_user_capacity);
			observer->cleanup_requested = true;
			dssh_session_cleanup(observer->session);
			break;
		case AUTH_BOUNDARY_PARTIAL:
			observer->cbs->methods_str = "publickey";
			break;
		default:
			break;
	}
	if (observer->fail_identity && index == 0)
		dssh_test_alloc_fail_after(0);
	return 0;
}

static unsigned
auth_boundary_identity(const uint8_t *username, size_t length)
{
	if (length == 5 && memcmp(username, "alpha", 5) == 0)
		return 1;
	if (length == 4 && memcmp(username, "beta", 4) == 0)
		return 2;
	return 0;
}

static int
auth_boundary_none(const uint8_t *username, size_t length, void *opaque)
{
	struct auth_boundary_observer *observer = opaque;
	if (observer->calls >= 4) {
		observer->valid = false;
		return DSSH_AUTH_DISCONNECT;
	}
	unsigned index = observer->calls++;
	observer->identities[index] = auth_boundary_identity(username, length);
	observer->requests_at_method[index] = observer->requests;
	observer->valid &= username == observer->session->server_auth_user
	    && length == observer->session->server_auth_user_len;
	if (observer->behavior == AUTH_BOUNDARY_STORAGE && index == 3)
		return DSSH_AUTH_SUCCESS;
	if (observer->behavior == AUTH_BOUNDARY_PARTIAL) {
		if (observer->factor)
			return DSSH_AUTH_SUCCESS;
		observer->factor = true;
		return DSSH_AUTH_PARTIAL;
	}
	return DSSH_AUTH_FAILURE;
}

static void
auth_boundary_release(void *opaque)
{
	struct auth_boundary_observer *observer = opaque;
	observer->releases++;
}

static int
auth_boundary_kbi(const uint8_t *username, size_t length, uint32_t count,
    const uint8_t **responses, const size_t *lengths, struct dssh_auth_challenge *out, void *opaque)
{
	struct auth_boundary_observer *observer = opaque;
	if (observer->calls >= 3) {
		observer->valid = false;
		return DSSH_AUTH_DISCONNECT;
	}
	unsigned index = observer->calls++;
	observer->identities[index] = auth_boundary_identity(username, length);
	observer->requests_at_method[index] = observer->requests;
	observer->zero_shapes[index] = count == 0 && responses == NULL && lengths == NULL;
	out->lease.owner = observer;
	out->lease.release = auth_boundary_release;
	/* An empty challenge is valid and still requires a response. */
	return DSSH_AUTH_KBI_PROMPT;
}

static int
auth_boundary_request(dssh_session client, const char *username, const char *method)
{
	uint8_t packet[1024];
	size_t pos = 1;
	packet[0] = SSH_MSG_USERAUTH_REQUEST;
	if (!auth_frame_string(packet, sizeof(packet), &pos, username, strlen(username))
	    || !auth_frame_string(packet, sizeof(packet), &pos, "ssh-connection", 14)
	    || !auth_frame_string(packet, sizeof(packet), &pos, method, strlen(method)))
		return DSSH_ERROR_INVALID;
	if (strcmp(method, "keyboard-interactive") == 0
	    && (!auth_frame_string(packet, sizeof(packet), &pos, "", 0)
	    || !auth_frame_string(packet, sizeof(packet), &pos, "", 0)))
		return DSSH_ERROR_INVALID;
	return send_packet(client, packet, pos, NULL);
}

static bool
auth_boundary_receive(dssh_session client, uint8_t wanted)
{
	uint8_t type, *payload;
	size_t length;
	if (recv_packet(client, &type, &payload, &length) != 0 || type != wanted)
		return false;
	if (wanted == SSH_MSG_SERVICE_ACCEPT)
		return length == 17 && DSSH_GET_U32(payload + 1) == 12
		    && memcmp(payload + 5, "ssh-userauth", 12) == 0;
	if (wanted == SSH_MSG_USERAUTH_FAILURE)
		return length == 14 && DSSH_GET_U32(payload + 1) == 8
		    && memcmp(payload + 5, "password", 8) == 0 && payload[13] == 0;
	if (wanted == SSH_MSG_USERAUTH_INFO_REQUEST) {
		if (length != 17)
			return false;
		for (size_t i = 1; i < length; i++) {
			if (payload[i] != 0)
				return false;
		}
		return true;
	}
	return false;
}

static bool
auth_boundary_one(bool kbi, unsigned shape)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) != 0) {
		handshake_cleanup(&ctx);
		return false;
	}
	struct auth_boundary_observer observer = { .valid = true };
	struct auth_server_arg server = { .ctx = &ctx };
	observer.session = ctx.server;
	observer.cbs = &server.cbs;
	server.cbs.request_cb = auth_boundary_hook;
	server.cbs.methods_str = "password";
	server.cbs.cbdata = &observer;
	server.cbs.none_cb = auth_boundary_none;
	if (kbi)
		server.cbs.keyboard_interactive_cb = auth_boundary_kbi;
	thrd_t worker;
	bool worker_owned = thrd_create(&worker, auth_server_thread, &server) == thrd_success;
	bool valid = worker_owned && inject_auth_service_request(ctx.client) == 0
	    && auth_boundary_receive(ctx.client, SSH_MSG_SERVICE_ACCEPT);
	unsigned requests = 0;
	if (kbi) {
		if (valid) {
			valid = auth_boundary_request(ctx.client, "alpha", "keyboard-interactive") == 0
			    && auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_INFO_REQUEST);
			requests++;
		}
		if (valid) {
			uint8_t response[] = {SSH_MSG_USERAUTH_INFO_RESPONSE, 0, 0, 0, 0};
			valid = send_packet(ctx.client, response, sizeof(response), NULL) == 0
			    && auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_INFO_REQUEST);
		}
		if (valid && shape == 2) {
			valid = auth_boundary_request(ctx.client, "alpha", "opaque") == 0
			    && auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_FAILURE);
			requests++;
		}
		if (valid) {
			valid = auth_boundary_request(ctx.client, shape == 1 ? "beta" : "alpha",
			    "keyboard-interactive") == 0
			    && auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_INFO_REQUEST);
			requests++;
		}
	}
	else {
		static const struct {
			const char *users[3];
			const char *methods[3];
			unsigned count;
		} sequences[] = {
			{{"alpha", "alpha"}, {"none", "none"}, 2},
			{{"alpha", "beta"}, {"none", "none"}, 2},
			{{"alpha", "beta", "alpha"}, {"none", "opaque", "none"}, 3},
			{{"alpha", "beta"}, {"opaque", "none"}, 2},
		};
		for (unsigned i = 0; i < sequences[shape].count && valid; i++) {
			valid = auth_boundary_request(ctx.client, sequences[shape].users[i], sequences[shape].methods[i]) == 0
			    && auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_FAILURE);
			requests++;
		}
	}
	/* End without granting any identity or completing authentication. */
	mock_io_close_c2s(&ctx.io);
	mock_io_close_s2c(&ctx.io);
	if (worker_owned && thrd_join(worker, NULL) != thrd_success)
		abort();
	valid &= observer.valid && observer.requests == requests
	    && observer.new_identities[0]
	    && server.result == DSSH_ERROR_TERMINATED
	    && !ctx.server->auth_complete && !ctx.client->auth_complete;
	if (kbi) {
		valid &= observer.new_identities[1] == (shape == 1)
		    && observer.requests_at_method[0] == 1 && observer.requests_at_method[1] == 1
		    && observer.requests_at_method[2] == requests
		    && observer.released_at_request[1] == 2;
		if (shape == 2)
			valid &= !observer.new_identities[2];
		valid &= observer.calls == 3 && observer.releases == 3
		    && observer.identities[0] == 1 && observer.identities[1] == 1
		    && observer.identities[2] == (shape == 1 ? 2u : 1u)
		    && observer.zero_shapes[0] && observer.zero_shapes[1] && observer.zero_shapes[2];
	}
	else {
		valid &= observer.new_identities[1] == (shape != 0)
		    && observer.requests_at_method[0] == (shape == 3 ? 2u : 1u);
		if (shape == 2)
			valid &= observer.new_identities[2] && observer.requests_at_method[1] == 3;
		else if (shape != 3)
			valid &= observer.requests_at_method[1] == 2;
		static const unsigned expected[][3] = {{2, 1, 1}, {2, 1, 2}, {2, 1, 1}, {1, 2, 0}};
		valid &= observer.calls == expected[shape][0] && observer.identities[0] == expected[shape][1]
		    && observer.identities[1] == expected[shape][2] && observer.releases == 0;
	}
	handshake_cleanup(&ctx);
	if (!valid)
		fprintf(stderr, "request boundary failed: kbi=%d shape=%u\n", kbi, shape);
	return valid;
}

static int
test_auth_request_boundaries(void)
{
	unsigned failures = 0;
	for (unsigned shape = 0; shape < 4; shape++) {
		if (!auth_boundary_one(false, shape))
			failures++;
	}
	for (unsigned shape = 0; shape < 3; shape++) {
		if (!auth_boundary_one(true, shape))
			failures++;
	}
	return failures == 0 ? TEST_PASS : TEST_FAIL;
}

/* Reuse the public-operation worker, with one explicit retry after a malformed
 * common prefix. The successful service exchange and identity must survive it. */
struct auth_boundary_server {
	struct auth_server_arg auth;
	struct auth_boundary_observer *observer;
	bool resume;
	int first_result;
	bool identity_cleared;
};

static void
auth_boundary_allocation(size_t count, size_t size, void *opaque)
{
	struct auth_boundary_observer *observer = opaque;
	if (count == 1 && size == 300 && observer->session->server_auth_user_capacity == 5)
		observer->allocation_failed = true;
}

static int
auth_boundary_server_thread(void *opaque)
{
	struct auth_boundary_server *server = opaque;
	if (server->observer->fail_identity)
		dssh_test_alloc_observe_allocations(auth_boundary_allocation, server->observer);
	(void)auth_server_thread(&server->auth);
	server->first_result = server->auth.result;
	if (server->resume && server->first_result == DSSH_ERROR_PARSE)
		(void)auth_server_thread(&server->auth);
	if (server->observer->cleanup_requested) {
		bool cleared = false;
		bool deferred = !dssh_test_alloc_watched_free(&cleared);
		dssh_session_cleanup(server->observer->session);
		server->identity_cleared = deferred && dssh_test_alloc_watched_free(&cleared) && cleared;
	}
	dssh_test_alloc_observe_allocations(NULL, NULL);
	return 0;
}

static bool
auth_boundary_failure(dssh_session client, const char *methods, bool partial)
{
	uint8_t type, *payload;
	size_t length;
	size_t methods_len = strlen(methods);
	if (recv_packet(client, &type, &payload, &length) != 0 || type != SSH_MSG_USERAUTH_FAILURE)
		return false;
	return length == 6 + methods_len && DSSH_GET_U32(payload + 1) == methods_len
	    && memcmp(payload + 5, methods, methods_len) == 0 && payload[5 + methods_len] == partial;
}

enum {
	AUTH_POLICY_EMPTY_SAME,
	AUTH_POLICY_CHANGED_UNKNOWN,
	AUTH_POLICY_LONG_CHANGED,
	AUTH_POLICY_EMPTY_CHANGED,
	AUTH_POLICY_STORAGE,
	AUTH_POLICY_RESUME,
	AUTH_POLICY_FACTORS,
	AUTH_POLICY_SERVICE,
	AUTH_POLICY_ALLOC,
	AUTH_POLICY_NEGATIVE,
	AUTH_POLICY_POSITIVE,
	AUTH_POLICY_TERMINATE,
	AUTH_POLICY_BAD_METHODS,
	AUTH_POLICY_CLEANUP,
	AUTH_POLICY_AUTO_NONE,
	AUTH_POLICY_HOOK_AUTO_NONE,
	AUTH_POLICY_COUNT,
};

static bool
auth_boundary_policy_one(unsigned shape)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) != 0) {
		handshake_cleanup(&ctx);
		return false;
	}
	struct auth_boundary_observer observer = { .valid = true, .session = ctx.server };
	struct auth_boundary_server server = { .auth.ctx = &ctx, .observer = &observer,
	    .resume = shape == AUTH_POLICY_RESUME };
	observer.cbs = &server.auth.cbs;
	server.auth.cbs.methods_str = "password";
	server.auth.cbs.cbdata = &observer;
	server.auth.cbs.none_cb = auth_boundary_none;
	if (shape >= AUTH_POLICY_STORAGE && shape != AUTH_POLICY_AUTO_NONE)
		server.auth.cbs.request_cb = auth_boundary_hook;
	if (shape >= AUTH_POLICY_AUTO_NONE)
		server.auth.cbs.none_cb = NULL;
	if (shape == AUTH_POLICY_STORAGE)
		observer.behavior = AUTH_BOUNDARY_STORAGE;
	if (shape == AUTH_POLICY_FACTORS || shape == AUTH_POLICY_SERVICE)
		observer.behavior = AUTH_BOUNDARY_PARTIAL;
	if (shape == AUTH_POLICY_ALLOC)
		observer.fail_identity = true;
	static const unsigned controls[] = {
	    AUTH_BOUNDARY_NEGATIVE, AUTH_BOUNDARY_POSITIVE, AUTH_BOUNDARY_TERMINATE,
	    AUTH_BOUNDARY_BAD_METHODS, AUTH_BOUNDARY_CLEANUP,
	};
	if (shape >= AUTH_POLICY_NEGATIVE && shape <= AUTH_POLICY_CLEANUP)
		observer.behavior = controls[shape - AUTH_POLICY_NEGATIVE];
	memset(server.auth.username_out, 0xa5, sizeof(server.auth.username_out));
	thrd_t worker;
	bool worker_owned = thrd_create(&worker, auth_boundary_server_thread, &server) == thrd_success;
	bool valid = worker_owned && inject_auth_service_request(ctx.client) == 0
	    && auth_boundary_receive(ctx.client, SSH_MSG_SERVICE_ACCEPT);
	char long_user[321];
	memset(long_user, 'x', sizeof(long_user) - 1);
	long_user[sizeof(long_user) - 1] = 0;
	if (valid && shape <= AUTH_POLICY_EMPTY_CHANGED) {
		const char *first = shape == AUTH_POLICY_CHANGED_UNKNOWN ? "alpha" : "";
		const char *method = shape == AUTH_POLICY_EMPTY_SAME || shape == AUTH_POLICY_LONG_CHANGED
		    ? "none" : "opaque";
		if (shape == AUTH_POLICY_LONG_CHANGED) {
			long_user[300] = 0;
			first = long_user;
		}
		valid = auth_boundary_request(ctx.client, first, method) == 0
		    && auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_FAILURE);
		if (shape == AUTH_POLICY_LONG_CHANGED)
			long_user[299] = 'y';
		const char *second = shape == AUTH_POLICY_LONG_CHANGED ? long_user
		    : shape == AUTH_POLICY_EMPTY_SAME ? "" : "beta";
		if (valid)
			valid = auth_boundary_request(ctx.client, second, "none") == 0;
		if (valid && shape == AUTH_POLICY_EMPTY_SAME)
			valid = auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_FAILURE);
		else if (valid) {
			uint8_t type, *payload;
			size_t length;
			valid = recv_packet(ctx.client, &type, &payload, &length) == DSSH_ERROR_TERMINATED;
		}
	}
	else if (valid && shape == AUTH_POLICY_STORAGE) {
		for (unsigned i = 0; i < 4 && valid; i++) {
			long_user[300] = i < 2 ? 0 : 'x';
			valid = auth_boundary_request(ctx.client, i == 1 ? "alpha" : long_user, "none") == 0;
			if (valid && i < 3)
				valid = auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_FAILURE);
			else if (valid) {
				uint8_t type, *payload;
				size_t length;
				valid = recv_packet(ctx.client, &type, &payload, &length) == 0
				    && type == SSH_MSG_USERAUTH_SUCCESS && length == 1;
			}
		}
	}
	else if (valid && shape == AUTH_POLICY_RESUME) {
		static const uint8_t bad[] = { SSH_MSG_USERAUTH_REQUEST, 0, 0, 0, 1, 'x' };
		valid = auth_boundary_request(ctx.client, "alpha", "none") == 0
		    && auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_FAILURE)
		    && send_packet(ctx.client, bad, sizeof(bad), NULL) == 0
		    && auth_boundary_request(ctx.client, "alpha", "none") == 0
		    && auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_FAILURE);
	}
	else if (valid && shape == AUTH_POLICY_FACTORS) {
		static const char *users[] = { "alpha", "alpha", "beta", "alpha", "alpha" };
		for (unsigned i = 0; i < 5 && valid; i++) {
			valid = auth_boundary_request(ctx.client, users[i], i == 1 || i == 2 ? "opaque" : "none") == 0;
			if (valid && i < 4)
				valid = auth_boundary_failure(ctx.client, "publickey", i == 0 || i == 3);
			else if (valid) {
				uint8_t type, *payload;
				size_t length;
				valid = recv_packet(ctx.client, &type, &payload, &length) == 0
				    && type == SSH_MSG_USERAUTH_SUCCESS && length == 1;
			}
		}
	}
	else if (valid && shape == AUTH_POLICY_SERVICE) {
		valid = auth_boundary_request(ctx.client, "alpha", "none") == 0
		    && auth_boundary_failure(ctx.client, "publickey", true);
		uint8_t packet[128] = { SSH_MSG_USERAUTH_REQUEST };
		size_t pos = 1;
		if (valid)
			valid = auth_frame_string(packet, sizeof(packet), &pos, "alpha", 5)
			    && auth_frame_string(packet, sizeof(packet), &pos, "other-service", 13)
			    && auth_frame_string(packet, sizeof(packet), &pos, "none", 4)
			    && send_packet(ctx.client, packet, pos, NULL) == 0;
	}
	else if (valid && shape == AUTH_POLICY_ALLOC) {
		/* Keep the client outside the library allocation countdown. The server
		 * hook arms it after the first identity has been stored. */
		dssh_test_alloc_exclude_thread();
		valid = auth_boundary_request(ctx.client, "alpha", "none") == 0
		    && auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_FAILURE);
		long_user[300] = 0;
		if (valid)
			valid = auth_boundary_request(ctx.client, long_user, "none") == 0;
	}
	else if (valid) {
		valid = auth_boundary_request(ctx.client, "alpha", "none") == 0;
		if (valid && shape >= AUTH_POLICY_AUTO_NONE)
			valid = auth_boundary_receive(ctx.client, SSH_MSG_USERAUTH_FAILURE);
		else if (valid && (shape == AUTH_POLICY_NEGATIVE || shape == AUTH_POLICY_POSITIVE)) {
			uint8_t type, *payload;
			size_t length;
			valid = recv_packet(ctx.client, &type, &payload, &length) == DSSH_ERROR_TERMINATED;
		}
	}
	/* Terminal hooks and identity-allocation failure return without a reply.
	 * Join their worker before closing I/O, so EOF cannot replace delivery of
	 * the request being checked. Other cases need EOF to finish the loop. */
	bool close_first = !valid || shape < AUTH_POLICY_SERVICE || shape > AUTH_POLICY_CLEANUP;
	if (close_first) {
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
	}
	if (worker_owned && thrd_join(worker, NULL) != thrd_success)
		abort();
	if (!close_first) {
		mock_io_close_c2s(&ctx.io);
		mock_io_close_s2c(&ctx.io);
	}
	dssh_test_alloc_reset();
	if (observer.cleanup_requested)
		ctx.server = NULL;
	bool success = shape == AUTH_POLICY_STORAGE || shape == AUTH_POLICY_FACTORS;
	valid &= observer.valid && server.auth.result == (success ? 0 : DSSH_ERROR_TERMINATED);
	if (ctx.server != NULL)
		valid &= ctx.server->auth_complete == success;
	if (shape == AUTH_POLICY_EMPTY_SAME)
		valid &= observer.calls == 2 && ctx.server->server_auth_user_bound
		    && ctx.server->server_auth_user_len == 0 && ctx.server->server_auth_user_capacity == 1;
	else if (shape == AUTH_POLICY_CHANGED_UNKNOWN || shape == AUTH_POLICY_EMPTY_CHANGED)
		valid &= observer.calls == 0;
	else if (shape == AUTH_POLICY_LONG_CHANGED)
		valid &= observer.calls == 1 && ctx.server->server_auth_user_len == 300;
	else if (shape == AUTH_POLICY_STORAGE) {
		valid &= observer.requests == 4 && observer.calls == 4
		    && observer.new_identities[0] && observer.new_identities[1]
		    && observer.new_identities[2] && !observer.new_identities[3]
		    && observer.lengths[0] == 300 && observer.lengths[1] == 5
		    && observer.lengths[2] == 320 && observer.lengths[3] == 320
		    && observer.capacities[0] == 300 && observer.capacities[1] == 300
		    && observer.capacities[2] == 320 && observer.capacities[3] == 320
		    && server.auth.username_out_len == 255 && server.auth.username_out[255] == 0xa5;
		for (size_t i = 0; i < 255; i++)
			valid &= server.auth.username_out[i] == 'x';
	}
	else if (shape == AUTH_POLICY_RESUME)
		valid &= server.first_result == DSSH_ERROR_PARSE && observer.requests == 2 && observer.calls == 2
		    && observer.new_identities[0] && !observer.new_identities[1];
	else if (shape == AUTH_POLICY_FACTORS)
		valid &= observer.requests == 5 && observer.calls == 3 && observer.new_identities[0]
		    && !observer.new_identities[1] && observer.new_identities[2]
		    && observer.new_identities[3] && !observer.new_identities[4];
	else if (shape == AUTH_POLICY_SERVICE)
		valid &= observer.requests == 1 && observer.calls == 1;
	else if (shape == AUTH_POLICY_ALLOC)
		valid &= observer.allocation_failed && observer.requests == 1 && observer.calls == 1
		    && ctx.server->server_auth_user_capacity == 5;
	else if (shape <= AUTH_POLICY_CLEANUP) {
		valid &= observer.requests == 1 && observer.calls == 0;
		if (shape == AUTH_POLICY_CLEANUP)
			valid &= server.identity_cleared;
	}
	else
		valid &= observer.calls == 0 && observer.requests == (shape == AUTH_POLICY_AUTO_NONE ? 0u : 1u);
	if (ctx.server != NULL && ctx.server->server_auth_user != NULL) {
		dssh_test_alloc_watch_free(ctx.server->server_auth_user, ctx.server->server_auth_user_capacity);
		dssh_session_cleanup(ctx.server);
		ctx.server = NULL;
		bool cleared = false;
		valid &= dssh_test_alloc_watched_free(&cleared) && cleared;
	}
	handshake_cleanup(&ctx);
	if (!valid)
		fprintf(stderr, "request policy failed: shape=%u result=%d hooks=%u methods=%u\n",
		    shape, server.auth.result, observer.requests, observer.calls);
	return valid;
}

static int
test_auth_request_identity(void)
{
	bool valid = true;
	for (unsigned shape = 0; shape < AUTH_POLICY_NEGATIVE; shape++)
		valid &= auth_boundary_policy_one(shape);
	return valid ? TEST_PASS : TEST_FAIL;
}

static int
test_auth_request_results(void)
{
	bool valid = true;
	for (unsigned shape = AUTH_POLICY_NEGATIVE; shape < AUTH_POLICY_COUNT; shape++)
		valid &= auth_boundary_policy_one(shape);
	return valid ? TEST_PASS : TEST_FAIL;
}
