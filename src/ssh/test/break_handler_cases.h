/* Approved action contract: public channels and a native authenticated peer.
 * Device execution is an application outcome in this fixture, not hardware. */
struct break_handler_capture {
	struct dssh_chan_break_request requests[128];
	atomic_uint count;
	atomic_bool bad;
	atomic_bool entered, release;
	bool performed, pause, self_remove, lifecycle, reject_reacquire;
};

static void
break_handler_capture_init(struct break_handler_capture *capture, bool performed)
{
	atomic_init(&capture->count, 0);
	atomic_init(&capture->bad, false);
	atomic_init(&capture->entered, false);
	atomic_init(&capture->release, false);
	capture->performed = performed;
}

static bool
break_handler_action(dssh_channel channel, const struct dssh_chan_break_request *request, void *opaque)
{
	struct break_handler_capture *capture = opaque;
	unsigned index = atomic_load(&capture->count);
	if (request == NULL || index >= sizeof(capture->requests) / sizeof(capture->requests[0])) {
		atomic_store(&capture->bad, true);
		return false;
	}
	capture->requests[index] = *request;
	uint8_t byte = 0, *output = NULL;
	size_t capacity = 0;
	struct dssh_chan_event ignored;
	bool valid = dssh_chan_get_type(channel) == DSSH_CHAN_EXEC
	    && dssh_chan_get_command(channel) != NULL
	    && dssh_chan_read(channel, 0, &byte, 1) == DSSH_ERROR_INVALID
	    && dssh_chan_write(channel, 0, &byte, 1) == DSSH_ERROR_INVALID
	    && dssh_chan_poll(channel, DSSH_POLL_EVENT, 0) == DSSH_ERROR_INVALID
	    && dssh_chan_read_event(channel, &ignored) == DSSH_ERROR_INVALID
	    && dssh_chan_zc_getbuf(channel, 0, &output, &capacity) == DSSH_ERROR_INVALID
	    && dssh_chan_zc_cancel(channel) == DSSH_ERROR_INVALID
	    && dssh_chan_zc_send(channel, 0) == DSSH_ERROR_INVALID
	    && dssh_chan_shutwr(channel) == DSSH_ERROR_INVALID
	    && dssh_chan_send_signal(channel, "TERM") == DSSH_ERROR_INVALID
	    && dssh_chan_send_window_change(channel, 1, 2, 3, 4) == DSSH_ERROR_INVALID
	    && dssh_chan_send_break(channel, 500) == DSSH_ERROR_INVALID
	    && dssh_chan_close(channel, INT64_MAX) == DSSH_ERROR_INVALID
	    && dssh_chan_set_max_events(channel, 1) == 0;
	if (capture->self_remove)
		valid = dssh_chan_set_break_cb(channel, NULL, NULL) == 0 && valid;
	if (capture->pause) {
		atomic_store(&capture->entered, true);
		valid = dssh_test_wait_bool(&capture->release, true) && valid;
	}
	if (capture->lifecycle)
		dssh_session_cleanup(channel->sess);
	if (capture->reject_reacquire)
		dssh_test_mtx_reject_once(&channel->buf_mtx);
	if (!valid)
		atomic_store(&capture->bad, true);
	atomic_store(&capture->count, index + 1);
	return capture->performed;
}

static bool
break_handler_other(dssh_channel channel, const struct dssh_chan_break_request *request, void *opaque)
{
	return break_handler_action(channel, request, opaque);
}

struct break_handler_fixture {
	struct conn_ctx ctx;
	dssh_channel channel;
	struct event_capture events;
};

static bool
break_handler_setup(struct break_handler_fixture *fixture, struct break_handler_capture *capture,
    unsigned role, unsigned gather, bool zc)
{
	window_test_gather = gather != 0;
	int result = conn_setup_mode(&fixture->ctx, false);
	window_test_gather = false;
	if (result != 0)
		return false;
	if (!half_close_algorithms(&fixture->ctx))
		return false;
	if (role != 0) {
		dssh_session swap = fixture->ctx.client;
		fixture->ctx.client = fixture->ctx.server;
		fixture->ctx.server = swap;
	}
	event_capture_init(&fixture->events, true);
	if (dssh_session_set_event_cb(fixture->ctx.server, capture_event, &fixture->events) != 0
	    || dssh_session_set_break_cb(fixture->ctx.server, capture != NULL ? break_handler_action : NULL, capture) != 0
	    || dssh_session_start(fixture->ctx.server) != 0)
		return false;
	fixture->channel = incoming_control_open(&fixture->ctx, zc);
	return fixture->channel != NULL;
}

static bool
break_handler_send(struct break_handler_fixture *fixture, uint32_t duration, uint8_t flag)
{
	uint8_t packet[64];
	size_t length;
	struct incoming_control_case test = {.name = "break", .type = TEST_CHANNEL_BREAK_REQUEST,
	    .values = {duration}};
	return incoming_control_packet(fixture->channel, &test, flag, packet, sizeof(packet), &length)
	    && send_packet(fixture->ctx.client, packet, length, NULL) == 0;
}

static bool
break_handler_reply(struct break_handler_fixture *fixture, uint8_t flag, bool performed)
{
	unsigned success = 0, failure = 0;
	return incoming_control_barrier(fixture->ctx.client, &success, &failure)
	    && success == (flag != 0 && performed ? 1U : 0U)
	    && failure == (flag != 0 && !performed ? 1U : 0U);
}

static int
test_break_handler_outcomes(void)
{
	ASSERT_EQ(dssh_chan_set_break_cb(NULL, NULL, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_session_set_break_cb(NULL, NULL, NULL), DSSH_ERROR_INVALID);
	unsigned observations = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned gather = 0; gather < 2; gather++) {
			for (unsigned zc = 0; zc < 2; zc++) {
				struct break_handler_fixture fixture = {0};
				struct break_handler_capture first = {0}, second = {0};
				break_handler_capture_init(&first, true);
				break_handler_capture_init(&second, false);
				ASSERT_TRUE(break_handler_setup(&fixture, &first, role, gather, zc != 0));
				ASSERT_EQ(dssh_session_set_break_cb(fixture.ctx.server, NULL, NULL), DSSH_ERROR_TOOLATE);
				static const uint32_t durations[] = {0, 1, 499, 500, 3000, 3001, UINT32_MAX};
				static const uint8_t flags[] = {0, 1, 255};
				bool valid = true;
				for (unsigned mode = 0; mode < 4 && valid; mode++) {
					struct break_handler_capture *capture = mode == 1 ? &second : &first;
					if (mode != 0)
						valid = dssh_chan_set_break_cb(fixture.channel,
						    mode == 2 ? NULL : mode == 1 ? break_handler_other : break_handler_action,
						    mode == 2 ? NULL : capture) == 0;
					for (size_t i = 0; i < sizeof(durations) / sizeof(durations[0]) && valid; i++) {
						for (size_t j = 0; j < sizeof(flags) / sizeof(flags[0]) && valid; j++) {
							unsigned before = atomic_load(&capture->count);
							bool performed = mode == 0 || mode == 3;
							valid = break_handler_send(&fixture, durations[i], flags[j])
							    && break_handler_reply(&fixture, flags[j], performed);
							unsigned after = atomic_load(&capture->count);
							valid = valid && after == before + (mode == 2 ? 0U : 1U)
							    && atomic_load(&fixture.events.count) == 0;
							if (mode != 2 && after == before + 1 && before < 128) {
								const struct dssh_chan_break_request *request = &capture->requests[before];
								valid = valid && request->length == durations[i]
								    && request->want_reply == (flags[j] != 0)
								    && request->stdout_pos == 0 && request->stderr_pos == 0;
							}
							if (zc == 0)
								valid = valid && dssh_chan_poll(fixture.channel, DSSH_POLL_EVENT, 0) == 0;
							observations++;
						}
					}
				}
				valid = valid && !atomic_load(&first.bad) && !atomic_load(&second.bad)
				    && atomic_load(&first.count) == 42 && atomic_load(&second.count) == 21
				    && !dssh_session_is_terminated(fixture.ctx.server);
				conn_cleanup(&fixture.ctx);
				ASSERT_TRUE(valid);
			}
		}
	}
	if (printf("break_handler_outcomes observations=%u valid=1\n", observations) < 0)
		return TEST_FAIL;
	return observations == 672 ? TEST_PASS : TEST_FAIL;
}

static void
break_handler_unblock(dssh_session session, void *opaque)
{
	(void)session;
	struct mock_io_state *io = opaque;
	mock_io_close_c2s(io);
	mock_io_close_s2c(io);
}

static bool
break_handler_wait_count(struct break_handler_capture *capture, unsigned count)
{
	struct timespec start;
	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
		return false;
	while (atomic_load(&capture->count) != count) {
		if (!dssh_test_wait_tick(&start))
			return false;
	}
	return true;
}

static int
test_break_handler_lifetime(void)
{
	unsigned cases = 0;
	for (unsigned kind = 0; kind < 5; kind++) {
		struct break_handler_fixture fixture = {0};
		struct break_handler_capture first = {0}, second = {0};
		break_handler_capture_init(&first, true);
		break_handler_capture_init(&second, false);
		first.pause = kind < 3;
		first.self_remove = kind == 3;
		first.lifecycle = kind == 4;
		ASSERT_TRUE(break_handler_setup(&fixture, &first, 0, 0, false));
		struct channel_workers workers;
		channel_workers_init(&workers, &fixture.ctx);
		workers.release = callback_release_flag;
		workers.resource = &first.release;
		bool valid = break_handler_send(&fixture, 500, 1);
		struct close_channel_ctx close;
		bool started = false;
		if (kind < 3) {
			valid = dssh_test_wait_bool(&first.entered, true) && valid;
			ASSERT_TRUE(valid);
			ASSERT_TRUE(callback_checkpoint(CALLBACK_GATE, kind));
			mtx_t *locks[] = {&fixture.channel->buf_mtx, &fixture.channel->cb_mtx,
			    &fixture.ctx.server->channel_mtx, &fixture.ctx.server->accept_mtx,
			    &fixture.ctx.server->trans.tx_mtx, &fixture.ctx.server->trans.tx_queue_mtx};
			for (size_t i = 0; i < sizeof(locks) / sizeof(locks[0]); i++) {
				int result = mtx_trylock(locks[i]);
				if (result == thrd_success) {
					if (mtx_unlock(locks[i]) != thrd_success)
						abort();
				}
				else
					valid = false;
			}
			if (kind == 0)
				valid = dssh_chan_set_break_cb(fixture.channel, break_handler_other, &second) == 0 && valid;
			else if (kind == 1)
				valid = dssh_chan_set_break_cb(fixture.channel, NULL, NULL) == 0 && valid;
			else {
				ASSERT_TRUE(valid);
				ASSERT_EQ(callback_start_close(&workers, &close, &fixture.channel), 0);
				started = true;
				ASSERT_TRUE(dssh_test_wait_bool(&close.waiting, true));
				ASSERT_FALSE(atomic_load(&close.done));
				ASSERT_TRUE(callback_checkpoint(CALLBACK_CLOSE, kind));
			}
			atomic_store(&first.release, true);
		}
		if (!valid)
			break_handler_unblock(fixture.ctx.server, &fixture.ctx.io);
		if (kind == 2) {
			if (started) {
				channel_worker_join(&workers, 0);
				ASSERT_EQ(workers.results[0], 0);
				ASSERT_NULL(close.ch);
				valid = close.result == 0 && valid;
				uint8_t type;
				uint8_t *payload;
				size_t length;
				for (unsigned i = 0; i < 2; i++) {
					int result = recv_packet(fixture.ctx.client, &type, &payload, &length);
					valid = result == 0 && length == 5 && DSSH_GET_U32(payload + 1) == 42
					    && type == (i == 0 ? SSH_MSG_CHANNEL_EOF : SSH_MSG_CHANNEL_CLOSE) && valid;
				}
			}
			else
				valid = false;
		}
		else if (kind == 4) {
			valid = dssh_test_wait_bool(&fixture.ctx.server->terminate, true)
			    && break_handler_wait_count(&first, 1) && valid;
			break_handler_unblock(fixture.ctx.server, &fixture.ctx.io);
		}
		else {
			valid = break_handler_reply(&fixture, 1, true) && valid;
			valid = break_handler_send(&fixture, 0, 255)
			    && break_handler_reply(&fixture, 255, false) && valid;
			valid = atomic_load(&second.count) == (kind == 0 ? 1U : 0U) && valid;
		}
		callback_release_flag(&first.release);
		channel_workers_finish(&workers);
		conn_cleanup(&fixture.ctx);
		valid = atomic_load(&first.count) == 1 && !atomic_load(&first.bad)
		    && !atomic_load(&second.bad) && valid;
		ASSERT_TRUE(valid);
		cases++;
	}
	if (printf("break_handler_lifetime cases=%u valid=1\n", cases) < 0)
		return TEST_FAIL;
	return cases == 5 ? TEST_PASS : TEST_FAIL;
}

static int
test_break_handler_failures(void)
{
	unsigned cases = 0;
	for (unsigned kind = 0; kind < 5; kind++) {
		struct break_handler_fixture fixture = {0};
		struct break_handler_capture capture = {0};
		break_handler_capture_init(&capture, true);
		capture.reject_reacquire = kind == 1;
		ASSERT_TRUE(break_handler_setup(&fixture, &capture, 0, 0, false));
		uint8_t packet[64];
		size_t length;
		struct incoming_control_case test = {.name = "break", .type = TEST_CHANNEL_BREAK_REQUEST};
		ASSERT_TRUE(incoming_control_packet(fixture.channel, &test, kind == 3 ? 0 : 1,
		    packet, sizeof(packet), &length));
		if (kind == 0 || kind == 4)
			dssh_test_mtx_reject_once(&fixture.channel->cb_mtx);
		if (kind == 2)
			setup_tx_test_hook = half_close_fail_tx;
		if (kind == 3)
			dssh_test_alloc_fail_after(0);
		int result = kind == 4 ? dssh_chan_set_break_cb(fixture.channel, NULL, NULL)
		    : dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, length);
		int allocations = kind == 3 ? dssh_test_alloc_count() : 0;
		dssh_test_alloc_reset();
		setup_tx_test_hook = NULL;
		unsigned unowned = dssh_test_mtx_rejected_unlocks();
		dssh_test_thrd_reset();
		bool valid = kind == 3 ? result == 0 && allocations == 0
		    && !dssh_session_is_terminated(fixture.ctx.server)
		    : result < 0 && dssh_session_is_terminated(fixture.ctx.server);
		valid = valid && unowned == 0 && !atomic_load(&capture.bad)
		    && atomic_load(&capture.count) == (kind == 0 || kind == 4 ? 0U : 1U)
		    && atomic_load(&fixture.events.count) == 0;
		conn_cleanup(&fixture.ctx);
		ASSERT_TRUE(valid);
		cases++;
	}
	if (printf("break_handler_failures cases=%u valid=1\n", cases) < 0)
		return TEST_FAIL;
	return cases == 5 ? TEST_PASS : TEST_FAIL;
}

static int
test_break_handler_positions(void)
{
	unsigned cases = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned zc = 0; zc < 2; zc++) {
			for (unsigned gather = 0; gather < 2; gather++) {
				for (unsigned saturate = 0; saturate < 2; saturate++) {
					struct conn_ctx ctx;
					window_test_gather = gather != 0;
					int setup = conn_setup(&ctx);
					window_test_gather = false;
					ASSERT_EQ(setup, 0);
					struct open_exec_ctx channels = {.client = ctx.client, .server = ctx.server,
					    .command = "break-positions", .cbs = &accept_cbs_all, .accept_timeout = 30000};
					if (zc != 0) {
						struct close_reservation_open job = {0};
						ASSERT_TRUE(close_reservation_pair(&ctx, &job, role == 0 ? 1 : 0));
						channels = job.channels;
					}
					else
						ASSERT_EQ(open_exec_channel(&ctx, &channels), 0);
					dssh_channel target = role == 0 ? channels.server_ch : channels.client_ch;
					dssh_channel writer = role == 0 ? channels.client_ch : channels.server_ch;
					struct break_handler_capture capture = {0};
					break_handler_capture_init(&capture, true);
					ASSERT_EQ(dssh_chan_set_break_cb(target, break_handler_action, &capture), 0);
					if (saturate != 0) {
						if (mtx_lock(&target->buf_mtx) != thrd_success)
							abort();
						target->buf.stdout_buf.total = SIZE_MAX - 1;
						target->buf.stderr_buf.total = SIZE_MAX - 1;
						if (mtx_unlock(&target->buf_mtx) != thrd_success)
							abort();
					}
					static const uint8_t stdout_bytes[] = {0, 255, 13};
					static const uint8_t stderr_bytes[] = {27, 10};
					ASSERT_TRUE(control_frame_window_ready(writer));
					bool valid = dssh_chan_write(writer, 0, stdout_bytes, sizeof(stdout_bytes)) == 3
					    && dssh_chan_write(writer, 1, stderr_bytes, sizeof(stderr_bytes)) == 2
					    && dssh_chan_send_break(writer, 500) == 0
					    && break_handler_wait_count(&capture, 1);
					valid = valid && capture.requests[0].stdout_pos == (saturate != 0 ? SIZE_MAX : 3)
					    && capture.requests[0].stderr_pos == (saturate != 0 ? SIZE_MAX : 2)
					    && capture.requests[0].length == 500 && !capture.requests[0].want_reply;
					if (zc == 0 && valid) {
						uint8_t buffer[3];
						valid = dssh_chan_read(target, 0, buffer, sizeof(buffer)) == 3
						    && memcmp(buffer, stdout_bytes, sizeof(stdout_bytes)) == 0
						    && dssh_chan_read(target, 1, buffer, sizeof(buffer)) == 2
						    && memcmp(buffer, stderr_bytes, sizeof(stderr_bytes)) == 0
						    && dssh_chan_poll(target, DSSH_POLL_EVENT, 0) == 0;
					}
					conn_cleanup(&ctx);
					ASSERT_TRUE(valid && !atomic_load(&capture.bad));
					cases++;
				}
			}
		}
	}
	if (printf("break_handler_positions cases=%u valid=1\n", cases) < 0)
		return TEST_FAIL;
	return cases == 16 ? TEST_PASS : TEST_FAIL;
}

static bool
break_handler_alternate(dssh_channel channel, const struct dssh_chan_break_request *request, void *opaque)
{
	bool valid = break_handler_action(channel, request, opaque);
	return valid && request->length == 500;
}

struct break_handler_queue_job {
	struct break_handler_fixture *fixture;
	bool valid;
};

static int
break_handler_queue_peer(void *opaque)
{
	struct break_handler_queue_job *job = opaque;
	struct break_handler_fixture *fixture = job->fixture;
	static const uint8_t unknown[] = "break-order-marker";
	bool valid = break_handler_send(fixture, 500, 1) && break_handler_send(fixture, 0, 255)
	    && request_frame_send(fixture->ctx.client, fixture->channel->local_id,
	        unknown, sizeof(unknown) - 1, 1, NULL, 0) == 0;
	for (unsigned i = 0; i < 3 && valid; i++)
		valid = request_frame_receive(fixture->ctx.client,
		    i == 0 ? SSH_MSG_CHANNEL_SUCCESS : SSH_MSG_CHANNEL_FAILURE, 42, NULL);
	if (valid)
		valid = channel_id_wire_barrier(fixture->ctx.client);
	job->valid = valid;
	if (!valid)
		break_handler_unblock(fixture->ctx.server, &fixture->ctx.io);
	return valid ? 0 : 1;
}

static int
test_break_handler_queue(void)
{
	unsigned cases = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned gather = 0; gather < 2; gather++) {
			for (unsigned zc = 0; zc < 2; zc++) {
				struct break_handler_fixture fixture = {0};
				struct break_handler_capture capture = {0};
				break_handler_capture_init(&capture, true);
				ASSERT_TRUE(break_handler_setup(&fixture, NULL, role, gather, zc != 0));
				ASSERT_EQ(dssh_chan_set_break_cb(fixture.channel, break_handler_alternate, &capture), 0);
				struct callback_tx_gate gate = {.session = fixture.ctx.server, .result = -1};
				struct channel_workers workers;
				channel_workers_init(&workers, &fixture.ctx);
				workers.release = callback_release_tx;
				workers.resource = &gate;
				ASSERT_EQ(tx_acquire(fixture.ctx.server, SSH_MSG_IGNORE), 0);
				gate.held = true;
				struct break_handler_queue_job job = {.fixture = &fixture};
				ASSERT_EQ(channel_worker_start(&workers, 0, break_handler_queue_peer, &job), 0);
				bool valid = break_handler_wait_count(&capture, 2)
				    && dssh_test_wait_bool(&fixture.channel->chan_reply_slot.ready, true);
				ASSERT_TRUE(valid);
				ASSERT_TRUE(callback_checkpoint(CALLBACK_QUEUE, role * 4 + gather * 2 + zc));
				callback_release_tx(&gate);
				int released = gate.result;
				if (!valid || released != 0)
					break_handler_unblock(fixture.ctx.server, &fixture.ctx.io);
				channel_workers_finish(&workers);
				valid = valid && released == 0 && workers.results[0] == 0 && job.valid
				    && atomic_load(&capture.count) == 2 && !atomic_load(&capture.bad)
				    && capture.requests[0].length == 500 && capture.requests[1].length == 0
				    && !dssh_session_is_terminated(fixture.ctx.server)
				    && !tx_owner_is_current(fixture.ctx.server)
				    && atomic_load(&fixture.events.count) == 0;
				conn_cleanup(&fixture.ctx);
				ASSERT_TRUE(valid);
				cases++;
			}
		}
	}
	if (printf("break_handler_queue cases=%u valid=1\n", cases) < 0)
		return TEST_FAIL;
	return cases == 8 ? TEST_PASS : TEST_FAIL;
}

static int
test_break_handler_setup(void)
{
	unsigned cases = 0;
	for (unsigned gather = 0; gather < 2; gather++) {
		for (unsigned zc = 0; zc < 2; zc++) {
			struct conn_ctx ctx;
			window_test_gather = gather != 0;
			int setup = conn_setup_mode(&ctx, false);
			window_test_gather = false;
			ASSERT_EQ(setup, 0);
			struct break_handler_capture capture = {0};
			break_handler_capture_init(&capture, true);
			ASSERT_EQ(dssh_session_set_break_cb(ctx.server, break_handler_action, &capture), 0);
			ASSERT_EQ(dssh_session_start(ctx.server), 0);
			struct incoming_control_open job = {.session = ctx.server, .zc = zc != 0};
			struct channel_workers workers;
			channel_workers_init(&workers, &ctx);
			ASSERT_EQ(channel_worker_start(&workers, 0, incoming_control_open_worker, &job), 0);
			uint32_t remote = 0;
			bool valid = accept_failure_open(ctx.client, 42) == 0
			    && request_frame_receive(ctx.client, SSH_MSG_CHANNEL_OPEN_CONFIRMATION, 42, &remote);
			ASSERT_TRUE(valid);
			ASSERT_TRUE(callback_checkpoint(CALLBACK_SETUP, gather * 2 + zc));
			static const uint8_t name[] = "break";
			uint8_t body[4];
			size_t pos = 0;
			DSSH_PUT_U32(500, body, &pos);
			if (valid)
				valid = request_frame_send(ctx.client, remote, name, sizeof(name) - 1, 0, body, sizeof(body)) == 0
				    && request_frame_send(ctx.client, remote, name, sizeof(name) - 1, 1, body, sizeof(body)) == 0
				    && request_frame_receive(ctx.client, SSH_MSG_CHANNEL_FAILURE, 42, NULL)
				    && request_frame_send(ctx.client, remote, name, sizeof(name) - 1, 255, body, sizeof(body)) == 0
				    && request_frame_receive(ctx.client, SSH_MSG_CHANNEL_FAILURE, 42, NULL)
				    && atomic_load(&capture.count) == 0;
			if (valid)
				valid = request_frame_exec(ctx.client, remote, 42);
			if (!valid)
				break_handler_unblock(ctx.server, &ctx.io);
			channel_workers_finish(&workers);
			valid = workers.results[0] == 0 && job.channel != NULL && valid;
			if (valid) {
				unsigned success = 0, failure = 0;
				valid = request_frame_send(ctx.client, remote, name, sizeof(name) - 1, 1, body, sizeof(body)) == 0
				    && incoming_control_barrier(ctx.client, &success, &failure)
				    && success == 1 && failure == 0
				    && atomic_load(&capture.count) == 1 && !atomic_load(&capture.bad);
			}
			conn_cleanup(&ctx);
			ASSERT_TRUE(valid);
			cases++;
		}
	}
	if (printf("break_handler_setup cases=%u valid=1\n", cases) < 0)
		return TEST_FAIL;
	return cases == 4 ? TEST_PASS : TEST_FAIL;
}
