/* Audit-only current-behavior observations. Structural/ownership assertions are
 * acceptance checks; observed role/text/reply policy is NOT RFC acceptance. */
struct incoming_control_open {
	dssh_session session;
	dssh_channel channel;
	bool zc;
};

static int
incoming_control_terminal(dssh_channel channel, const struct dssh_chan_params *params,
    struct dssh_chan_accept_result *result, void *opaque)
{
	(void)channel;
	(void)params;
	struct incoming_control_open *job = opaque;
	result->max_window = 4096;
	if (job->zc)
		result->zc_cb = event_zc_data;
	return 0;
}

static int
incoming_control_open_worker(void *opaque)
{
	struct incoming_control_open *job = opaque;
	if (!job->session->trans.client) {
		struct dssh_chan_accept_cbs cbs = {.exec = incoming_control_terminal, .cbdata = job};
		job->channel = dssh_chan_accept(job->session, &cbs, 30000);
		return job->channel != NULL ? 0 : DSSH_ERROR_INIT;
	}
	struct dssh_chan_params params;
	int result = dssh_chan_params_init(&params, DSSH_CHAN_EXEC);
	if (result != 0)
		return result;
	result = dssh_chan_params_set_command(&params, "half-close");
	if (result == 0)
		result = dssh_chan_params_set_pty(&params, false);
	if (result == 0) {
		job->channel = job->zc ? dssh_chan_zc_open(job->session, &params, event_zc_data, NULL)
		    : dssh_chan_open(job->session, &params);
		if (job->channel == NULL)
			result = DSSH_ERROR_INIT;
	}
	dssh_chan_params_free(&params);
	return result;
}

static dssh_channel
incoming_control_open(struct conn_ctx *ctx, bool zc)
{
	struct incoming_control_open job = {.session = ctx->server, .zc = zc};
	struct channel_workers workers;
	channel_workers_init(&workers, ctx);
	if (channel_worker_start(&workers, 0, incoming_control_open_worker, &job) != 0) {
		channel_workers_finish(&workers);
		return NULL;
	}
	bool valid;
	if (!ctx->server->trans.client) {
		uint32_t remote;
		valid = accept_failure_open(ctx->client, 42) == 0
		    && request_frame_receive(ctx->client, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 42, &remote)
		    && request_frame_exec(ctx->client, remote, 42);
	}
	else {
		uint32_t local;
		valid = channel_id_receive_open(ctx->client, &local);
		uint8_t reply[17];
		size_t reply_length = valid ? channel_id_build_reply(reply, local, true, 42) : 0;
		uint8_t type;
		uint8_t *payload;
		size_t length;
		if (valid)
			valid = send_packet(ctx->client, reply, reply_length, NULL) == 0
			    && recv_packet(ctx->client, &type, &payload, &length) == 0
			    && type == SSH_MSG_CHANNEL_REQUEST && length == 28
			    && DSSH_GET_U32(payload + 1) == 42 && DSSH_GET_U32(payload + 5) == 4
			    && memcmp(payload + 9, "exec", 4) == 0 && payload[13] == 1
			    && DSSH_GET_U32(payload + 14) == 10 && memcmp(payload + 18, "half-close", 10) == 0;
		if (valid) {
			size_t pos = 0;
			reply[pos++] = SSH_MSG_CHANNEL_SUCCESS;
			DSSH_PUT_U32(local, reply, &pos);
			valid = send_packet(ctx->client, reply, pos, NULL) == 0
			    && peer_id_receive_control(ctx->client, SSH_MSG_CHANNEL_WINDOW_ADJUST, 9, 42);
		}
	}
	if (!valid) {
		mock_io_close_c2s(&ctx->io);
		mock_io_close_s2c(&ctx->io);
	}
	channel_workers_finish(&workers);
	return valid && workers.results[0] == 0 ? job.channel : NULL;
}

static bool
incoming_control_barrier(dssh_session peer, unsigned *success, unsigned *failure)
{
	if (send_packet(peer, channel_id_probe_request, sizeof(channel_id_probe_request), NULL) != 0)
		return false;
	for (unsigned i = 0; i < 3; i++) {
		uint8_t type;
		uint8_t *payload;
		size_t length;
		if (recv_packet(peer, &type, &payload, &length) != 0)
			return false;
		if (type == SSH_MSG_REQUEST_FAILURE && length == 1)
			return true;
		if (length != 5 || DSSH_GET_U32(payload + 1) != 42)
			return false;
		if (type == SSH_MSG_CHANNEL_SUCCESS)
			(*success)++;
		else if (type == SSH_MSG_CHANNEL_FAILURE)
			(*failure)++;
		else
			return false;
	}
	return false;
}

struct incoming_control_case {
	const char *name;
	int type;
	struct dssh_bytes signal, error, language;
	uint32_t values[4];
};

static bool
incoming_control_event(const struct dssh_chan_event *event, const struct incoming_control_case *test)
{
	if (event->type != test->type || event->stdout_pos != 0 || event->stderr_pos != 0)
		return false;
	if (test->type == DSSH_EVENT_SIGNAL || test->type == DSSH_EVENT_EXIT_SIGNAL) {
		struct dssh_bytes signal = test->type == DSSH_EVENT_SIGNAL ? event->signal.name
		    : event->exit_signal.signal_name;
		if (!dssh_test_bytes_equal(signal, test->signal.data, test->signal.len))
			return false;
		if (test->type == DSSH_EVENT_EXIT_SIGNAL)
			return event->exit_signal.core_dumped
			    && dssh_test_bytes_equal(event->exit_signal.error_message, test->error.data, test->error.len)
			    && dssh_test_bytes_equal(event->exit_signal.language, test->language.data, test->language.len);
		return true;
	}
	if (test->type == DSSH_EVENT_WINDOW_CHANGE)
		return event->window_change.cols == test->values[0] && event->window_change.rows == test->values[1]
		    && event->window_change.wpx == test->values[2] && event->window_change.hpx == test->values[3];
	return test->type == DSSH_EVENT_EXIT_STATUS && event->exit_status.exit_code == test->values[0];
}

static bool
incoming_control_packet(dssh_channel channel, const struct incoming_control_case *test,
    uint8_t flag, uint8_t *packet, size_t capacity, size_t *length)
{
	if (event_packet(channel, test->type, packet, capacity, length) != 0)
		return false;
	uint32_t name_wire = DSSH_GET_U32(packet + 5);
	/* The fixture's five fixed request names are all shorter than 16. */
	if (name_wire > 16)
		return false;
	size_t name_len = (size_t)name_wire;
	packet[9 + name_len] = flag;
	size_t pos = 10 + name_len;
	int result;
	if (test->type == DSSH_EVENT_SIGNAL || test->type == DSSH_EVENT_EXIT_SIGNAL) {
		const struct dssh_bytes *fields[] = {&test->signal, &test->error, &test->language};
		unsigned count = test->type == DSSH_EVENT_SIGNAL ? 1U : 3U;
		for (unsigned i = 0; i < count; i++) {
			if (fields[i]->len > UINT32_MAX)
				return false;
			uint32_t wire = (uint32_t)fields[i]->len;
			result = dssh_serialize_uint32(wire, packet, capacity, &pos);
			if (result != 0 || !dssh_buffer_has(capacity, pos, fields[i]->len))
				return false;
			if (fields[i]->len != 0)
				memcpy(packet + pos, fields[i]->data, fields[i]->len);
			pos += fields[i]->len;
			if (test->type == DSSH_EVENT_EXIT_SIGNAL && i == 0) {
				if (!dssh_buffer_has(capacity, pos, 1))
					return false;
				packet[pos++] = 0xff;
			}
		}
	}
	else {
		unsigned count = test->type == DSSH_EVENT_WINDOW_CHANGE ? 4U : 1U;
		for (unsigned i = 0; i < count; i++) {
			result = dssh_serialize_uint32(test->values[i], packet, capacity, &pos);
			if (result != 0)
				return false;
		}
	}
	*length = pos;
	return true;
}

struct incoming_break_capture {
	struct dssh_chan_break_request requests[EVENT_CAPTURE_CAP];
	atomic_uint count;
};

static bool
incoming_break_perform(dssh_channel channel, const struct dssh_chan_break_request *request, void *opaque)
{
	(void)channel;
	struct incoming_break_capture *capture = opaque;
	unsigned index = atomic_load(&capture->count);
	if (index >= EVENT_CAPTURE_CAP)
		return false;
	capture->requests[index] = *request;
	atomic_store(&capture->count, index + 1);
	/* This fixture reports an application action outcome; no physical
	 * serial device or terminal operation is claimed. */
	return true;
}

static int
incoming_control_run(struct conn_ctx *ctx, unsigned role, unsigned gather, unsigned delivery)
{
	/* delivery 0: stream callback; 1: ZC callback; 2: stream poll. */
	struct event_capture capture = {0};
	event_capture_init(&capture, true);
	struct incoming_break_capture actions = {0};
	atomic_init(&actions.count, 0);
	if (dssh_session_set_break_cb(ctx->server, incoming_break_perform, &actions) != 0)
		goto failed;
	const char *expected_mac = test_using_hmac256() ? "hmac-sha2-256" : "hmac-sha2-512";
	for (unsigned direction = 0; direction < 2; direction++) {
		enum dssh_direction selected = direction == 0 ? DSSH_CLIENT_TO_SERVER : DSSH_SERVER_TO_CLIENT;
		const char *mac = dssh_transport_get_mac_name(ctx->server, selected);
		if (mac == NULL || strcmp(mac, expected_mac) != 0)
			goto failed;
	}
	if (printf("incoming_control_algorithms mac=%s\n", expected_mac) < 0)
		goto failed;
	if (delivery != 2 && dssh_session_set_event_cb(ctx->server, capture_event, &capture) != 0)
		goto failed;
	if (dssh_session_start(ctx->server) != 0)
		goto failed;
	dssh_channel channel = incoming_control_open(ctx, delivery == 1);
	if (channel == NULL)
		goto failed;
	static const uint8_t nul_signal[] = {'S', 0, 'G'};
	static const uint8_t high_signal[] = {'S', 0xff, 'G'};
	static const uint8_t binary_error[] = {0, 13, 10, 27, 0xff};
	static const uint8_t utf8_error[] = {0xe2, 0x82, 0xac};
	static const uint8_t invalid_utf8[] = {0xc0, 0xaf};
	static const uint8_t binary_language[] = {'e', 'n', 0};
#define VIEW(s) { .data = (const uint8_t *)(s), .len = sizeof(s) - 1 }
	struct incoming_control_case cases[] = {
		{.name = "ABRT", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("ABRT")},
		{.name = "ALRM", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("ALRM")},
		{.name = "FPE", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("FPE")},
		{.name = "HUP", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("HUP")},
		{.name = "ILL", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("ILL")},
		{.name = "INT", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("INT")},
		{.name = "KILL", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("KILL")},
		{.name = "PIPE", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("PIPE")},
		{.name = "QUIT", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("QUIT")},
		{.name = "SEGV", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("SEGV")},
		{.name = "TERM", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("TERM")},
		{.name = "USR1", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("USR1")},
		{.name = "USR2", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("USR2")},
		{.name = "extension", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("STOP@audit.example")},
		{.name = "empty", .type = DSSH_EVENT_SIGNAL, .signal = {0}},
		{.name = "two_at", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("S@a@b")},
		{.name = "nul", .type = DSSH_EVENT_SIGNAL, .signal = {nul_signal, sizeof(nul_signal)}},
		{.name = "high", .type = DSSH_EVENT_SIGNAL, .signal = {high_signal, sizeof(high_signal)}},
		{.name = "unknown", .type = DSSH_EVENT_SIGNAL, .signal = VIEW("UNKNOWN")},
		{.name = "break0", .type = TEST_CHANNEL_BREAK_REQUEST, .values = {0}},
		{.name = "break1", .type = TEST_CHANNEL_BREAK_REQUEST, .values = {1}},
		{.name = "break499", .type = TEST_CHANNEL_BREAK_REQUEST, .values = {499}},
		{.name = "break500", .type = TEST_CHANNEL_BREAK_REQUEST, .values = {500}},
		{.name = "break3000", .type = TEST_CHANNEL_BREAK_REQUEST, .values = {3000}},
		{.name = "break3001", .type = TEST_CHANNEL_BREAK_REQUEST, .values = {3001}},
		{.name = "breakmax", .type = TEST_CHANNEL_BREAK_REQUEST, .values = {UINT32_MAX}},
		{.name = "window0", .type = DSSH_EVENT_WINDOW_CHANGE, .values = {0}},
		{.name = "windowmax", .type = DSSH_EVENT_WINDOW_CHANGE,
		    .values = {UINT32_MAX, UINT32_MAX, UINT32_MAX, UINT32_MAX}},
		{.name = "windowordinary", .type = DSSH_EVENT_WINDOW_CHANGE, .values = {80, 24, 640, 480}},
		{.name = "status0", .type = DSSH_EVENT_EXIT_STATUS, .values = {0}},
		{.name = "statusmax", .type = DSSH_EVENT_EXIT_STATUS, .values = {UINT32_MAX}},
		{.name = "exit_text", .type = DSSH_EVENT_EXIT_SIGNAL, .signal = VIEW("TERM"),
		    .error = VIEW("message"), .language = VIEW("en-US")},
		{.name = "exit_binary", .type = DSSH_EVENT_EXIT_SIGNAL, .signal = VIEW("TERM"),
		    .error = {binary_error, sizeof(binary_error)}, .language = {binary_language, sizeof(binary_language)}},
		{.name = "exit_utf8", .type = DSSH_EVENT_EXIT_SIGNAL, .signal = VIEW("TERM"),
		    .error = {utf8_error, sizeof(utf8_error)}, .language = VIEW("en")},
		{.name = "exit_invalid_utf8", .type = DSSH_EVENT_EXIT_SIGNAL, .signal = VIEW("TERM"),
		    .error = {invalid_utf8, sizeof(invalid_utf8)}, .language = VIEW("not a tag")},
	};
#undef VIEW
	static const uint8_t flags[] = {0, 1, 0xff};
	unsigned observations = 0, event_observations = 0, break_observations = 0;
	uint_least32_t remote_window = atomic_load(&channel->remote_window);
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		for (size_t j = 0; j < sizeof(flags) / sizeof(flags[0]); j++) {
			uint8_t packet[256];
			size_t length;
			if (!incoming_control_packet(channel, &cases[i], flags[j], packet, sizeof(packet), &length)
			    || send_packet(ctx->client, packet, length, NULL) != 0)
				goto failed;
			unsigned success = 0, failure = 0;
			if (!incoming_control_barrier(ctx->client, &success, &failure))
				goto failed;
			bool verified, observed = false;
			if (cases[i].type == TEST_CHANNEL_BREAK_REQUEST) {
				unsigned count = atomic_load(&actions.count);
				observed = count == break_observations + 1;
				verified = observed && count <= EVENT_CAPTURE_CAP;
				if (verified) {
					const struct dssh_chan_break_request *request = &actions.requests[break_observations];
					verified = request->length == cases[i].values[0]
					    && request->stdout_pos == 0 && request->stderr_pos == 0
					    && request->want_reply == (flags[j] != 0);
				}
				if (delivery == 2)
					verified = dssh_chan_poll(channel, DSSH_POLL_EVENT, 0) == 0 && verified;
				else {
					int events = atomic_load(&capture.count);
					if (events < 0)
						verified = false;
					else {
						unsigned event_count = (unsigned)events;
						verified = event_count == event_observations && verified;
					}
				}
				break_observations++;
			}
			else if (flags[j] != 0) {
				if (delivery == 2) {
					int ready = dssh_chan_poll(channel, DSSH_POLL_EVENT, 0);
					observed = ready == DSSH_POLL_EVENT;
					verified = ready == 0;
				}
				else {
					int count = atomic_load(&capture.count);
					verified = count >= 0 && !atomic_load(&capture.bad);
					if (count >= 0) {
						unsigned received = (unsigned)count;
						observed = received > event_observations;
						verified = verified && received == event_observations;
					}
				}
			}
			else if (delivery == 2) {
				struct dssh_chan_event event;
				observed = dssh_chan_poll(channel, DSSH_POLL_EVENT, 0) == DSSH_POLL_EVENT;
				verified = observed && dssh_chan_read_event(channel, &event) == 0
				    && incoming_control_event(&event, &cases[i])
				    && dssh_chan_poll(channel, DSSH_POLL_EVENT, 0) == 0;
			}
			else {
				int count = atomic_load(&capture.count);
				verified = event_observations < EVENT_CAPTURE_CAP && count > 0
				    && count <= EVENT_CAPTURE_CAP && !atomic_load(&capture.bad)
				    && incoming_control_event(&capture.events[event_observations], &cases[i]);
				if (verified) {
					unsigned count_u = (unsigned)count;
					observed = count_u > event_observations;
					verified = count_u == event_observations + 1;
				}
			}
			bool action = cases[i].type == TEST_CHANNEL_BREAK_REQUEST;
			if (!action && flags[j] == 0)
				event_observations++;
			unsigned expected_success = action && flags[j] != 0 ? 1U : 0U;
			unsigned expected_failure = !action && flags[j] != 0 ? 1U : 0U;
			bool valid = verified && success == expected_success && failure == expected_failure
			    && !dssh_session_is_terminated(ctx->server) && !dssh_session_is_terminated(ctx->client)
			    && atomic_load(&channel->remote_window) == remote_window;
			unsigned flag = flags[j];
			if (printf("incoming_control role=%u gather=%u delivery=%u name=%s flag=%u delivered=%u success=%u failure=%u structural_valid=%u policy_accepted=0\n",
			    role, gather, delivery, cases[i].name, flag, observed ? 1U : 0U,
			    success, failure, valid ? 1U : 0U) < 0)
				goto failed;
			if (!valid || (observations == 0
			    && !callback_checkpoint(CALLBACK_INCOMING, role * 6 + gather * 3 + delivery)))
				goto failed;
			observations++;
		}
	}
	int result = observations == 105 ? TEST_PASS : TEST_FAIL;
	conn_cleanup(ctx);
	if (callback_incoming_retirement_observer != NULL)
		callback_incoming_retirement_observer(ctx, &capture.count, &actions.count);
	return result;
failed:
	conn_cleanup(ctx);
	if (callback_incoming_retirement_observer != NULL)
		callback_incoming_retirement_observer(ctx, &capture.count, &actions.count);
	return TEST_FAIL;
}

static int
incoming_control_probe(void)
{
	unsigned pairs = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned gather = 0; gather < 2; gather++) {
			for (unsigned delivery = 0; delivery < 3; delivery++) {
				struct conn_ctx ctx;
				window_test_gather = gather != 0;
				int setup = conn_setup_mode(&ctx, false);
				window_test_gather = false;
				if (setup != 0)
					return 2;
				bool algorithms = half_close_algorithms(&ctx);
				if (role != 0) {
					dssh_session swap = ctx.client;
					ctx.client = ctx.server;
					ctx.server = swap;
				}
				int result = TEST_FAIL;
				if (algorithms)
					result = incoming_control_run(&ctx, role, gather, delivery);
				else
					conn_cleanup(&ctx);
				if (result != TEST_PASS)
					return 1;
				pairs++;
			}
		}
	}
	if (printf("incoming_controls pairs=%u observations=%u structural_valid=1 policy_accepted=0\n", pairs, pairs * 105) < 0)
		return 1;
	if (fflush(stdout) != 0)
		return 1;
	return pairs == 12 ? 0 : 1;
}
