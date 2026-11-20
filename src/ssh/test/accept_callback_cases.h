/* Approved borrowed setup-handle contract, using authenticated native peers. */
struct accept_callback_fixture {
	struct conn_ctx ctx;
	struct dssh_chan_accept_cbs cbs;
	dssh_channel accepted;
	atomic_bool server_done;
	atomic_bool peer_done;
	atomic_bool normal_done;
	atomic_bool publish_release;
	bool normal_valid;
	unsigned grant_sends;
	unsigned kind;
	bool reply;
	bool negative;
	bool cancel;
	bool valid;
	bool peer_valid;
	unsigned callbacks;
	unsigned sends_in_callback;
	unsigned frees_in_callback;
	const void *borrowed;
};
static _Thread_local struct accept_callback_fixture *accept_callback_current;

static int
accept_callback_tx(dssh_session sess)
{
	(void)sess;
	accept_callback_current->sends_in_callback++;
	return 0;
}

static void
accept_callback_free(const void *ptr, void *opaque)
{
	struct accept_callback_fixture *f = opaque;
	if (ptr == f->borrowed)
		f->frees_in_callback++;
}

static void
accept_callback_normal(dssh_channel ch, const struct dssh_chan_event *event, void *opaque)
{
	struct accept_callback_fixture *f = opaque;
	const char *command = dssh_chan_get_command(ch);
	f->normal_valid = event->type == DSSH_EVENT_SIGNAL && command != NULL
	    && strcmp(command, "test") == 0 && dssh_chan_get_type(ch) == DSSH_CHAN_EXEC
	    && !atomic_load(&f->server_done) && !dssh_setup_callback_active(f->ctx.server)
	    && dssh_chan_close(ch, -1) == DSSH_ERROR_INVALID;
	atomic_store(&f->normal_done, true);
}

static int
accept_callback_grant(dssh_session sess)
{
	(void)sess;
	struct accept_callback_fixture *f = accept_callback_current;
	f->grant_sends++;
	if (f->grant_sends == 2 && (!wire_wait(f->ctx.server, &f->normal_done)
	    || !wire_wait(f->ctx.server, &f->publish_release)))
		return DSSH_ERROR_TERMINATED;
	return 0;
}

static void
accept_callback_terminate(dssh_session sess, void *opaque)
{
	(void)sess;
	struct accept_callback_fixture *f = opaque;
	mock_io_close_c2s(&f->ctx.io);
	mock_io_close_s2c(&f->ctx.io);
}

static int
accept_callback_request(dssh_channel ch, const struct dssh_chan_params *params, void *opaque)
{
	struct accept_callback_fixture *f = opaque;
	f->callbacks++;
	accept_callback_current = f;
	accept_phase_test_hook = accept_callback_tx;
	f->borrowed = ch;
	dssh_test_alloc_observe_frees(accept_callback_free, f);
	bool valid = dssh_setup_callback_active(f->ctx.server)
	    && !dssh_setup_callback_active(f->ctx.client)
	    && dssh_auth_set_banner(f->ctx.client, "independent", "") == 0
	    && dssh_chan_get_type(ch) == params->type
	    && dssh_chan_has_pty(ch) == ((params->flags & DSSH_PARAM_HAS_PTY) != 0)
	    && dssh_chan_get_pty(ch) == ((params->flags & DSSH_PARAM_HAS_PTY) != 0 ? params : NULL)
	    && dssh_chan_get_command(ch) == (params->type == DSSH_CHAN_EXEC ? params->command : NULL)
	    && dssh_chan_get_subsystem(ch) == (params->type == DSSH_CHAN_SUBSYSTEM ? params->subsystem : NULL)
	    && dssh_chan_set_event_cb(ch, f->kind == 5 ? accept_callback_normal : NULL, f) == 0
	    && dssh_chan_set_max_events(ch, 24) == 0;
	uint8_t byte = 0, *out = &byte;
	size_t capacity = 37;
	struct dssh_chan_event event = {.type = -77};
	struct dssh_chan_params open = {.type = DSSH_CHAN_SHELL};
	char methods[8] = "sentinel";
	valid = valid && dssh_chan_read(ch, 0, &byte, 1) == DSSH_ERROR_INVALID
	    && dssh_chan_write(ch, 0, &byte, 1) == DSSH_ERROR_INVALID
	    && dssh_chan_poll(ch, DSSH_POLL_READ, -1) == DSSH_ERROR_INVALID
	    && dssh_chan_read_event(ch, &event) == DSSH_ERROR_INVALID && event.type == -77
	    && dssh_chan_shutwr(ch) == DSSH_ERROR_INVALID
	    && dssh_chan_send_signal(ch, "TERM") == DSSH_ERROR_INVALID
	    && dssh_chan_send_window_change(ch, 1, 2, 3, 4) == DSSH_ERROR_INVALID
	    && dssh_chan_send_break(ch, 1) == DSSH_ERROR_INVALID
	    && dssh_chan_zc_getbuf(ch, 0, &out, &capacity) == DSSH_ERROR_INVALID
	    && out == &byte && capacity == 37
	    && dssh_chan_zc_send(ch, 0) == DSSH_ERROR_INVALID
	    && dssh_chan_zc_cancel(ch) == DSSH_ERROR_INVALID
	    && dssh_chan_open(f->ctx.server, &open) == NULL
	    && dssh_chan_zc_open(f->ctx.server, &open, NULL, NULL) == NULL
	    && dssh_chan_accept(f->ctx.server, &f->cbs, -1) == NULL
	    && dssh_session_start(f->ctx.server) == DSSH_ERROR_INVALID
	    && dssh_transport_handshake(f->ctx.server) == DSSH_ERROR_INVALID
	    && dssh_transport_disconnect(f->ctx.server, SSH_DISCONNECT_BY_APPLICATION, "setup") == DSSH_ERROR_INVALID
	    && test_auth_discover(f->ctx.server, "u", methods, sizeof(methods)) == DSSH_ERROR_INVALID
	    && memcmp(methods, "sentinel", sizeof(methods)) == 0
	    && dssh_auth_password(f->ctx.server, "u", "p", NULL, NULL) == DSSH_ERROR_INVALID
	    && dssh_auth_keyboard_interactive(f->ctx.server, "u", NULL, NULL) == DSSH_ERROR_INVALID
	    && dssh_auth_publickey(f->ctx.server, "u", "ssh-ed25519") == DSSH_ERROR_INVALID
	    && dssh_auth_set_banner(f->ctx.server, "setup", "") == DSSH_ERROR_INVALID
	    && dssh_auth_server(f->ctx.server, NULL, NULL, NULL) == DSSH_ERROR_INVALID;
	if (f->kind == 0)
		valid = valid && params->term != NULL && strcmp(params->term, "vt100") == 0
		    && params->cols == 80 && params->rows == 24;
	if (f->kind == 1)
		valid = valid && params->env_count == 1 && params->env != NULL
		    && params->env[0].name != NULL && strcmp(params->env[0].name, "A") == 0
		    && params->env[0].value != NULL && strcmp(params->env[0].value, "B") == 0;
	if (f->cancel)
		valid = valid && dssh_chan_close(ch, INT64_MAX) == 0;
	else if (f->kind == 6)
		dssh_session_terminate(f->ctx.server);
	else if (f->kind == 7)
		dssh_session_stop(f->ctx.server);
	else if (f->kind == 8)
		dssh_session_cleanup(f->ctx.server);
	/* No further use of the borrowed channel after recording cancellation. */
	accept_phase_test_hook = NULL;
	dssh_test_alloc_observe_frees(NULL, NULL);
	f->valid = f->valid && valid && f->sends_in_callback == 0 && f->frees_in_callback == 0;
	if (f->kind == 5)
		accept_phase_test_hook = accept_callback_grant;
	return f->negative ? DSSH_ERROR_REJECTED : 0;
}

static int
accept_callback_terminal(dssh_channel ch, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	struct accept_callback_fixture *f = opaque;
	result->max_window = 4096;
	if (f->callbacks != 0 || !f->cancel) {
		/* The second channel (or request-only rejection control) is accepted. */
		if (f->kind >= 2 && f->callbacks == 0)
			return accept_callback_request(ch, params, opaque);
		return 0;
	}
	return accept_callback_request(ch, params, opaque);
}

static int
accept_callback_server(void *opaque)
{
	struct accept_callback_fixture *f = opaque;
	f->accepted = dssh_chan_accept(f->ctx.server, &f->cbs, 3000);
	accept_phase_test_hook = NULL;
	f->valid = f->valid && !dssh_setup_callback_active(f->ctx.server);
	if (f->accepted != NULL) {
		const char *command = dssh_chan_get_command(f->accepted);
		f->valid = f->valid && dssh_chan_get_type(f->accepted) == DSSH_CHAN_EXEC
		    && command != NULL && strcmp(command, "test") == 0;
	}
	atomic_store(&f->server_done, true);
	return 0;
}

static int
accept_callback_send(struct accept_callback_fixture *f, uint32_t remote)
{
	uint8_t body[32];
	size_t pos = 0;
	const char *name;
	if (f->kind == 0) {
		name = "pty-req";
		DSSH_PUT_U32(5, body, &pos);
		memcpy(body + pos, "vt100", 5);
		pos += 5;
		DSSH_PUT_U32(80, body, &pos);
		DSSH_PUT_U32(24, body, &pos);
		DSSH_PUT_U32(0, body, &pos);
		DSSH_PUT_U32(0, body, &pos);
		DSSH_PUT_U32(1, body, &pos);
		body[pos++] = 0;
	}
	else if (f->kind == 1) {
		name = "env";
		DSSH_PUT_U32(1, body, &pos);
		body[pos++] = 'A';
		DSSH_PUT_U32(1, body, &pos);
		body[pos++] = 'B';
	}
	else {
		name = f->kind == 2 ? "shell" : f->kind == 4 ? "subsystem" : "exec";
		if (f->kind != 2) {
			DSSH_PUT_U32(4, body, &pos);
			memcpy(body + pos, "test", 4);
			pos += 4;
		}
	}
	const uint8_t *wire_name = (const uint8_t *)name;
	return request_frame_send(f->ctx.client, remote, wire_name, strlen(name), f->reply ? 1 : 0, body, pos);
}

static int
accept_callback_peer(void *opaque)
{
	struct accept_callback_fixture *f = opaque;
	dssh_session sess = f->ctx.client;
	uint32_t remote = 0;
	bool valid = accept_failure_open(sess, 42) == 0
	    && request_frame_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 42, &remote)
	    && accept_callback_send(f, remote) == 0;
	if (valid && f->kind >= 6) {
		uint8_t type;
		uint8_t *packet;
		size_t len;
		valid = recv_packet(sess, &type, &packet, &len) < 0;
		f->peer_valid = valid;
		atomic_store(&f->peer_done, true);
		return 0;
	}
	if (valid && f->kind == 5) {
		const uint8_t name[] = "signal";
		const uint8_t body[] = {0, 0, 0, 4, 'T', 'E', 'R', 'M'};
		valid = request_frame_receive(sess, SSH_MSG_CHANNEL_SUCCESS, 42, NULL)
		    && request_frame_send(sess, remote, name, sizeof(name) - 1, 0, body, sizeof(body)) == 0
		    && request_frame_receive(sess, SSH_MSG_CHANNEL_WINDOW_ADJUST, 42, NULL);
		f->peer_valid = valid;
		atomic_store(&f->peer_done, true);
		return 0;
	}
	if (valid && f->reply)
		valid = request_frame_receive(sess, SSH_MSG_CHANNEL_FAILURE, 42, NULL);
	if (valid && f->cancel)
		valid = request_frame_receive(sess, SSH_MSG_CHANNEL_CLOSE, 42, NULL)
		    && accept_failure_close(sess, remote) == 0
		    && accept_failure_open(sess, 43) == 0
		    && request_frame_receive(sess, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 43, &remote);
	if (valid)
		valid = request_frame_exec(sess, remote, f->cancel ? 43 : 42);
	f->peer_valid = valid;
	atomic_store(&f->peer_done, true);
	return 0;
}

static void
accept_callback_release(void *data)
{
	struct accept_callback_fixture *fixture = data;
	atomic_store(&fixture->publish_release, true);
}

static int
accept_callback_case(unsigned kind, bool reply, bool negative, bool cancel)
{
	struct accept_callback_fixture f = {.kind = kind, .reply = reply, .negative = negative,
	    .cancel = cancel, .valid = true};
	atomic_init(&f.server_done, false);
	atomic_init(&f.peer_done, false);
	atomic_init(&f.normal_done, false);
	atomic_init(&f.publish_release, false);
	ASSERT_EQ(conn_setup_no_start(&f.ctx), 0);
	ASSERT_EQ(dssh_session_set_timeout(f.ctx.server, 3000), 0);
	if (kind >= 6)
		ASSERT_EQ(dssh_session_set_terminate_cb(f.ctx.server, accept_callback_terminate, &f), 0);
	ASSERT_EQ(dssh_session_start(f.ctx.server), 0);
	f.cbs = accept_cbs_all;
	f.cbs.pty_req = accept_callback_request;
	f.cbs.env = accept_callback_request;
	f.cbs.shell = accept_callback_terminal;
	f.cbs.exec = accept_callback_terminal;
	f.cbs.subsystem = accept_callback_terminal;
	f.cbs.cbdata = &f;
	struct channel_workers workers;
	channel_workers_init(&workers, &f.ctx);
	workers.release = accept_callback_release;
	workers.resource = &f;
	ASSERT_EQ(channel_worker_start(&workers, 0, accept_callback_server, &f), 0);
	ASSERT_EQ(channel_worker_start(&workers, 1, accept_callback_peer, &f), 0);
	unsigned variant = kind * 8 + (reply ? 4U : 0U)
	    + (negative ? 2U : 0U) + (cancel ? 1U : 0U);
	ASSERT_TRUE(wire_checkpoint(WIRE_ACCEPT_CALLBACK, variant, WIRE_STARTED));
	if (kind == 5) {
		ASSERT_TRUE(wire_wait(f.ctx.server, &f.normal_done));
		ASSERT_FALSE(atomic_load(&f.server_done));
		ASSERT_TRUE(wire_checkpoint(WIRE_ACCEPT_CALLBACK, variant, WIRE_PUBLISHED));
	}
	accept_callback_release(&f);
	bool valid = dssh_test_wait_bool(&f.peer_done, true)
	    && dssh_test_wait_bool(&f.server_done, true);
	if (!valid || !f.peer_valid) {
		mock_io_close_c2s(&f.ctx.io);
		mock_io_close_s2c(&f.ctx.io);
	}
	channel_workers_finish(&workers);
	ASSERT_EQ(workers.results[0], 0);
	ASSERT_EQ(workers.results[1], 0);
	valid = valid && f.valid && f.peer_valid && (f.accepted != NULL) == (kind < 6) && f.callbacks == 1;
	if (kind < 6)
		valid = valid && !dssh_session_is_terminated(f.ctx.server) && !dssh_session_is_terminated(f.ctx.client);
	else
		valid = valid && dssh_session_is_terminated(f.ctx.server);
	if (kind == 5)
		valid = valid && f.normal_valid && atomic_load(&f.normal_done) && f.grant_sends == 2;
	if (!valid)
		fprintf(stderr, "setup callback kind=%u reply=%u negative=%u cancel=%u callback=%u peer=%u calls=%u accepted=%u\n",
		    kind, reply ? 1U : 0U, negative ? 1U : 0U, cancel ? 1U : 0U,
		    f.valid ? 1U : 0U, f.peer_valid ? 1U : 0U, f.callbacks, f.accepted != NULL ? 1U : 0U);
	if (f.accepted != NULL)
		ASSERT_EQ(dssh_chan_close(f.accepted, -1), 0);
	conn_cleanup(&f.ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int test_accept_callback_pty_silent_positive(void)
{
	return accept_callback_case(0, false, false, true);
}

static int test_accept_callback_pty_silent_negative(void)
{
	return accept_callback_case(0, false, true, true);
}

static int test_accept_callback_pty_reply_positive(void)
{
	return accept_callback_case(0, true, false, true);
}

static int test_accept_callback_pty_reply_negative(void)
{
	return accept_callback_case(0, true, true, true);
}

static int test_accept_callback_env_silent_positive(void)
{
	return accept_callback_case(1, false, false, true);
}

static int test_accept_callback_env_silent_negative(void)
{
	return accept_callback_case(1, false, true, true);
}

static int test_accept_callback_env_reply_positive(void)
{
	return accept_callback_case(1, true, false, true);
}

static int test_accept_callback_env_reply_negative(void)
{
	return accept_callback_case(1, true, true, true);
}

static int test_accept_callback_shell_silent_positive(void)
{
	return accept_callback_case(2, false, false, true);
}

static int test_accept_callback_shell_silent_negative(void)
{
	return accept_callback_case(2, false, true, true);
}

static int test_accept_callback_shell_reply_positive(void)
{
	return accept_callback_case(2, true, false, true);
}

static int test_accept_callback_shell_reply_negative(void)
{
	return accept_callback_case(2, true, true, true);
}

static int test_accept_callback_exec_silent_positive(void)
{
	return accept_callback_case(3, false, false, true);
}

static int test_accept_callback_exec_silent_negative(void)
{
	return accept_callback_case(3, false, true, true);
}

static int test_accept_callback_exec_reply_positive(void)
{
	return accept_callback_case(3, true, false, true);
}

static int test_accept_callback_exec_reply_negative(void)
{
	return accept_callback_case(3, true, true, true);
}

static int test_accept_callback_subsystem_silent_positive(void)
{
	return accept_callback_case(4, false, false, true);
}

static int test_accept_callback_subsystem_silent_negative(void)
{
	return accept_callback_case(4, false, true, true);
}

static int test_accept_callback_subsystem_reply_positive(void)
{
	return accept_callback_case(4, true, false, true);
}

static int test_accept_callback_subsystem_reply_negative(void)
{
	return accept_callback_case(4, true, true, true);
}

static int test_accept_callback_pty_request_only(void)
{
	return accept_callback_case(0, true, true, false);
}

static int test_accept_callback_env_request_only(void)
{
	return accept_callback_case(1, true, true, false);
}

static int test_accept_callback_publish_before_return(void)
{
	return accept_callback_case(5, true, false, false);
}

static int test_accept_callback_terminate_deferred(void)
{
	return accept_callback_case(6, true, false, false);
}

static int test_accept_callback_stop_deferred(void)
{
	return accept_callback_case(7, true, false, false);
}

static int test_accept_callback_cleanup_deferred(void)
{
	return accept_callback_case(8, true, false, false);
}
