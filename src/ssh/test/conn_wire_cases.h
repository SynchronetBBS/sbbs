/* Controls call the original registered wire hosts with real worker handles. */
#ifndef DSSH_TEST_CONN_WIRE_CASES_H
#define DSSH_TEST_CONN_WIRE_CASES_H

static const struct {
	const char *name;
	int (*function)(void);
	enum wire_suite suite;
	unsigned variant;
	enum wire_stage extra_stage;
} wire_consumers[] = {
	{"accept_channel_storage", test_accept_channel_storage, WIRE_ACCEPT_FAILURE, 0, WIRE_STARTED},
	{"accept_event_storage", test_accept_event_storage, WIRE_ACCEPT_FAILURE, 1, WIRE_STARTED},
	{"accept_param_storage", test_accept_param_storage, WIRE_ACCEPT_FAILURE, 2, WIRE_CLOSE_WAIT},
	{"accept_stream_first", test_accept_stream_first, WIRE_ACCEPT_FAILURE, 3, WIRE_CLOSE_WAIT},
	{"accept_stream_second", test_accept_stream_second, WIRE_ACCEPT_FAILURE, 4, WIRE_CLOSE_WAIT},
	{"accept_exec_reject", test_accept_exec_reject, WIRE_ACCEPT_FAILURE, 5, WIRE_CLOSE_WAIT},
	{"accept_exec_no_reply", test_accept_exec_no_reply, WIRE_ACCEPT_FAILURE, 6, WIRE_CLOSE_WAIT},
	{"accept_peer_close", test_accept_peer_close, WIRE_ACCEPT_FAILURE, 7, WIRE_CLOSE_WAIT},
	{"accept_shell_reject", test_accept_shell_reject, WIRE_ACCEPT_FAILURE, 8, WIRE_CLOSE_WAIT},
	{"accept_subsystem_reject", test_accept_subsystem_reject, WIRE_ACCEPT_FAILURE, 9, WIRE_CLOSE_WAIT},
	{"accept_buf_init", test_accept_buf_init, WIRE_ACCEPT_FAILURE, 10, WIRE_STARTED},
	{"accept_cb_init", test_accept_cb_init, WIRE_ACCEPT_FAILURE, 11, WIRE_STARTED},
	{"accept_cnd_init", test_accept_cnd_init, WIRE_ACCEPT_FAILURE, 12, WIRE_STARTED},
	{"accept_slot_first", test_accept_slot_first, WIRE_ACCEPT_FAILURE, 13, WIRE_STARTED},
	{"accept_slot_second", test_accept_slot_second, WIRE_ACCEPT_FAILURE, 14, WIRE_STARTED},
	{"accept_stream_first_no_reply", test_accept_stream_first_no_reply, WIRE_ACCEPT_FAILURE, 15, WIRE_CLOSE_WAIT},
	{"accept_stream_second_no_reply", test_accept_stream_second_no_reply, WIRE_ACCEPT_FAILURE, 16, WIRE_CLOSE_WAIT},
	{"accept_notify_failure", test_accept_notify_failure, WIRE_ACCEPT_FAILURE, 18, WIRE_STARTED},
	{"accept_success_send_failure", test_accept_success_send_failure, WIRE_ACCEPT_FAILURE, 19, WIRE_STARTED},
	{"accept_window_send_failure", test_accept_window_send_failure, WIRE_ACCEPT_FAILURE, 20, WIRE_STARTED},
	{"request_setup_short_length", test_request_setup_short_length, WIRE_REQUEST_FRAME, 1, WIRE_STARTED},
	{"request_setup_short_name", test_request_setup_short_name, WIRE_REQUEST_FRAME, 3, WIRE_STARTED},
	{"request_setup_short_reply", test_request_setup_short_reply, WIRE_REQUEST_FRAME, 5, WIRE_STARTED},
	{"request_setup_empty", test_request_setup_empty, WIRE_REQUEST_FRAME, 7, WIRE_STARTED},
	{"request_setup_nonascii", test_request_setup_nonascii, WIRE_REQUEST_FRAME, 9, WIRE_STARTED},
	{"request_setup_zero", test_request_setup_zero, WIRE_REQUEST_FRAME, 11, WIRE_STARTED},
	{"request_setup_comma", test_request_setup_comma, WIRE_REQUEST_FRAME, 13, WIRE_STARTED},
	{"request_setup_long", test_request_setup_long, WIRE_REQUEST_FRAME, 15, WIRE_STARTED},
	{"request_normal_short_length", test_request_normal_short_length, WIRE_REQUEST_FRAME, 0, WIRE_STARTED},
	{"request_normal_short_name", test_request_normal_short_name, WIRE_REQUEST_FRAME, 2, WIRE_STARTED},
	{"request_normal_short_reply", test_request_normal_short_reply, WIRE_REQUEST_FRAME, 4, WIRE_STARTED},
	{"request_normal_empty", test_request_normal_empty, WIRE_REQUEST_FRAME, 6, WIRE_STARTED},
	{"request_normal_nonascii", test_request_normal_nonascii, WIRE_REQUEST_FRAME, 8, WIRE_STARTED},
	{"request_normal_zero", test_request_normal_zero, WIRE_REQUEST_FRAME, 10, WIRE_STARTED},
	{"request_normal_comma", test_request_normal_comma, WIRE_REQUEST_FRAME, 12, WIRE_STARTED},
	{"request_normal_long", test_request_normal_long, WIRE_REQUEST_FRAME, 14, WIRE_STARTED},
	{"request_break_short", test_request_break_short, WIRE_REQUEST_FRAME, 16, WIRE_STARTED},
	{"request_break_trailer", test_request_break_trailer, WIRE_REQUEST_FRAME, 18, WIRE_STARTED},
	{"request_shell_reply", test_request_shell_reply, WIRE_REQUEST_FRAME, 21, WIRE_STARTED},
	{"request_shell_no_reply", test_request_shell_no_reply, WIRE_REQUEST_FRAME, 20, WIRE_STARTED},
	{"request_setup_unknown", test_request_setup_unknown, WIRE_REQUEST_FRAME, 23, WIRE_STARTED},
	{"request_normal_unknown", test_request_normal_unknown, WIRE_REQUEST_FRAME, 22, WIRE_STARTED},
	{"control_frame_wire_normal_window", test_control_frame_wire_normal_window, WIRE_CONTROL_FRAME, 0, WIRE_STARTED},
	{"control_frame_wire_normal_eof", test_control_frame_wire_normal_eof, WIRE_CONTROL_FRAME, 4, WIRE_STARTED},
	{"control_frame_wire_normal_close", test_control_frame_wire_normal_close, WIRE_CONTROL_FRAME, 8, WIRE_STARTED},
	{"control_frame_wire_normal_success", test_control_frame_wire_normal_success, WIRE_CONTROL_FRAME, 12, WIRE_STARTED},
	{"control_frame_wire_normal_failure", test_control_frame_wire_normal_failure, WIRE_CONTROL_FRAME, 16, WIRE_STARTED},
	{"control_frame_wire_setup_window", test_control_frame_wire_setup_window, WIRE_CONTROL_FRAME, 2, WIRE_STARTED},
	{"control_frame_wire_setup_eof", test_control_frame_wire_setup_eof, WIRE_CONTROL_FRAME, 6, WIRE_STARTED},
	{"control_frame_wire_setup_close", test_control_frame_wire_setup_close, WIRE_CONTROL_FRAME, 10, WIRE_STARTED},
	{"control_frame_wire_setup_success", test_control_frame_wire_setup_success, WIRE_CONTROL_FRAME, 14, WIRE_STARTED},
	{"control_frame_wire_setup_failure", test_control_frame_wire_setup_failure, WIRE_CONTROL_FRAME, 18, WIRE_STARTED},
	{"control_frame_wire_pending_success", test_control_frame_wire_pending_success, WIRE_CONTROL_FRAME, 13, WIRE_STARTED},
	{"control_frame_wire_pending_failure", test_control_frame_wire_pending_failure, WIRE_CONTROL_FRAME, 17, WIRE_STARTED},
	{"accept_callback_pty_silent_positive", test_accept_callback_pty_silent_positive, WIRE_ACCEPT_CALLBACK, 1, WIRE_STARTED},
	{"accept_callback_pty_silent_negative", test_accept_callback_pty_silent_negative, WIRE_ACCEPT_CALLBACK, 3, WIRE_STARTED},
	{"accept_callback_pty_reply_positive", test_accept_callback_pty_reply_positive, WIRE_ACCEPT_CALLBACK, 5, WIRE_STARTED},
	{"accept_callback_pty_reply_negative", test_accept_callback_pty_reply_negative, WIRE_ACCEPT_CALLBACK, 7, WIRE_STARTED},
	{"accept_callback_env_silent_positive", test_accept_callback_env_silent_positive, WIRE_ACCEPT_CALLBACK, 9, WIRE_STARTED},
	{"accept_callback_env_silent_negative", test_accept_callback_env_silent_negative, WIRE_ACCEPT_CALLBACK, 11, WIRE_STARTED},
	{"accept_callback_env_reply_positive", test_accept_callback_env_reply_positive, WIRE_ACCEPT_CALLBACK, 13, WIRE_STARTED},
	{"accept_callback_env_reply_negative", test_accept_callback_env_reply_negative, WIRE_ACCEPT_CALLBACK, 15, WIRE_STARTED},
	{"accept_callback_shell_silent_positive", test_accept_callback_shell_silent_positive, WIRE_ACCEPT_CALLBACK, 17, WIRE_STARTED},
	{"accept_callback_shell_silent_negative", test_accept_callback_shell_silent_negative, WIRE_ACCEPT_CALLBACK, 19, WIRE_STARTED},
	{"accept_callback_shell_reply_positive", test_accept_callback_shell_reply_positive, WIRE_ACCEPT_CALLBACK, 21, WIRE_STARTED},
	{"accept_callback_shell_reply_negative", test_accept_callback_shell_reply_negative, WIRE_ACCEPT_CALLBACK, 23, WIRE_STARTED},
	{"accept_callback_exec_silent_positive", test_accept_callback_exec_silent_positive, WIRE_ACCEPT_CALLBACK, 25, WIRE_STARTED},
	{"accept_callback_exec_silent_negative", test_accept_callback_exec_silent_negative, WIRE_ACCEPT_CALLBACK, 27, WIRE_STARTED},
	{"accept_callback_exec_reply_positive", test_accept_callback_exec_reply_positive, WIRE_ACCEPT_CALLBACK, 29, WIRE_STARTED},
	{"accept_callback_exec_reply_negative", test_accept_callback_exec_reply_negative, WIRE_ACCEPT_CALLBACK, 31, WIRE_STARTED},
	{"accept_callback_subsystem_silent_positive", test_accept_callback_subsystem_silent_positive, WIRE_ACCEPT_CALLBACK, 33, WIRE_STARTED},
	{"accept_callback_subsystem_silent_negative", test_accept_callback_subsystem_silent_negative, WIRE_ACCEPT_CALLBACK, 35, WIRE_STARTED},
	{"accept_callback_subsystem_reply_positive", test_accept_callback_subsystem_reply_positive, WIRE_ACCEPT_CALLBACK, 37, WIRE_STARTED},
	{"accept_callback_subsystem_reply_negative", test_accept_callback_subsystem_reply_negative, WIRE_ACCEPT_CALLBACK, 39, WIRE_STARTED},
	{"accept_callback_pty_request_only", test_accept_callback_pty_request_only, WIRE_ACCEPT_CALLBACK, 6, WIRE_STARTED},
	{"accept_callback_env_request_only", test_accept_callback_env_request_only, WIRE_ACCEPT_CALLBACK, 14, WIRE_STARTED},
	{"accept_callback_publish_before_return", test_accept_callback_publish_before_return, WIRE_ACCEPT_CALLBACK, 44, WIRE_PUBLISHED},
	{"accept_callback_terminate_deferred", test_accept_callback_terminate_deferred, WIRE_ACCEPT_CALLBACK, 52, WIRE_STARTED},
	{"accept_callback_stop_deferred", test_accept_callback_stop_deferred, WIRE_ACCEPT_CALLBACK, 60, WIRE_STARTED},
	{"accept_callback_cleanup_deferred", test_accept_callback_cleanup_deferred, WIRE_ACCEPT_CALLBACK, 68, WIRE_STARTED},
};

enum wire_fault {WIRE_NATIVE, WIRE_CREATE, WIRE_STAGE};
static enum wire_fault wire_fault;
static enum wire_stage wire_selected_stage;
static size_t wire_selected_consumer;
static unsigned wire_retired;
static bool wire_fired, wire_retired_valid;

static bool
wire_control_checkpoint(enum wire_suite suite, unsigned variant, enum wire_stage stage)
{
	if (wire_fault != WIRE_STAGE || wire_selected_stage != stage
	    || wire_consumers[wire_selected_consumer].suite != suite
	    || wire_consumers[wire_selected_consumer].variant != variant)
		return true;
	if (wire_fired)
		abort();
	wire_fired = true;
	return false;
}

static void
wire_control_retire(const struct channel_workers *workers)
{
	if (wire_retired != 0)
		abort();
	wire_retired++;
	wire_retired_valid = !workers->created[0] && !workers->created[1]
	    && workers->owner->workers == NULL && workers->owner->client != NULL
	    && workers->owner->server != NULL;
	if (workers->release == accept_failure_release) {
		const struct accept_failure_fixture *fixture = workers->resource;
		wire_retired_valid = wire_retired_valid && atomic_load(&fixture->acknowledge);
	}
	if (workers->release == accept_callback_release) {
		const struct accept_callback_fixture *fixture = workers->resource;
		wire_retired_valid = wire_retired_valid && atomic_load(&fixture->publish_release);
	}
}

static bool
wire_schedule(size_t consumer, enum wire_fault fault, unsigned parameter)
{
	if (consumer >= sizeof(wire_consumers) / sizeof(wire_consumers[0]))
		return false;
	wire_selected_consumer = consumer;
	wire_fault = fault;
	wire_fired = false;
	wire_retired = 0;
	wire_retired_valid = false;
	wire_stage_control = wire_control_checkpoint;
	conn_actor_retirement_observer = wire_control_retire;
	struct channel_worker_probe probe = {.fail_at = fault == WIRE_CREATE ? parameter : UINT_MAX,
	    .cancelled_before_join = true};
	channel_worker_probe_begin(&probe);
	int host = wire_consumers[consumer].function();
	channel_worker_probe_end();
	wire_stage_control = NULL;
	conn_actor_retirement_observer = NULL;
	unsigned expected = fault == WIRE_CREATE ? parameter : 2;
	bool valid = host == (fault == WIRE_NATIVE ? TEST_PASS : TEST_FAIL)
	    && wire_retired_valid && wire_retired == 1 && probe.created == expected
	    && probe.joined == expected && probe.cancelled_before_join
	    && channel_fixture_owner == NULL && g_active_ctx == NULL
	    && wire_fired == (fault == WIRE_STAGE)
	    && probe.creation_failed == (fault == WIRE_CREATE)
	    && probe.calls == expected + (fault == WIRE_CREATE ? 1U : 0U);
	for (unsigned i = 0; i < probe.created; i++)
		valid = valid && atomic_load(&probe.jobs[i].done);
	unsigned fault_number = (unsigned)fault;
	if (printf("wire_schedule consumer=%s fault=%u parameter=%u host=%d created=%u joined=%u retired=%u valid=%u\n",
	    wire_consumers[consumer].name, fault_number, parameter, host,
	    probe.created, probe.joined, wire_retired, valid ? 1U : 0U) < 0)
		return false;
	return valid;
}

static int
wire_owner_cases(enum wire_suite suite)
{
	bool valid = true;
	unsigned schedules = 0, consumers = 0;
	for (size_t i = 0; i < sizeof(wire_consumers) / sizeof(wire_consumers[0]); i++) {
		if (wire_consumers[i].suite != suite)
			continue;
		consumers++;
		valid = wire_schedule(i, WIRE_NATIVE, 0) && valid;
		schedules++;
		for (unsigned fail = 0; fail < 2; fail++) {
			valid = wire_schedule(i, WIRE_CREATE, fail) && valid;
			schedules++;
		}
		wire_selected_stage = WIRE_STARTED;
		valid = wire_schedule(i, WIRE_STAGE, WIRE_STARTED) && valid;
		schedules++;
		if (wire_consumers[i].extra_stage != WIRE_STARTED) {
			wire_selected_stage = wire_consumers[i].extra_stage;
			unsigned stage = (unsigned)wire_selected_stage;
			valid = wire_schedule(i, WIRE_STAGE, stage) && valid;
			schedules++;
		}
	}
	unsigned suite_number = (unsigned)suite;
	if (printf("wire_control suite=%u consumers=%u schedules=%u valid=%u\n",
	    suite_number, consumers, schedules, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(consumers != 0 && valid);
	return TEST_PASS;
}

static int
wire_fatal_child(const char *operation, const char *mode)
{
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	for (size_t i = 0; i < sizeof(wire_consumers) / sizeof(wire_consumers[0]); i++) {
		for (unsigned join = 1; join <= 2; join++) {
			char name[32];
			int length = snprintf(name, sizeof(name), "%zu-%u", i, join);
			if (length < 0)
				return 2;
			unsigned length_u = (unsigned)length;
			if (length_u >= sizeof(name))
				return 2;
			if (strcmp(operation, name) != 0)
				continue;
			struct channel_worker_probe probe = {.fail_at = UINT_MAX,
			    .cancelled_before_join = true, .fatal_join = join,
			    .fatal_released = strcmp(mode, "released") == 0};
			channel_worker_probe_begin(&probe);
			wire_consumers[i].function();
			channel_worker_probe_end();
			return 4;
		}
	}
	return 2;
}

static struct {
	unsigned mode, calls, native_successes;
} wire_clock_probe;

static int
wire_clock_control(clockid_t clock, struct timespec *value)
{
	wire_clock_probe.calls++;
	int ret = clock_gettime(clock, value);
	if (ret != 0)
		return ret;
	wire_clock_probe.native_successes++;
	if (wire_clock_probe.mode == 1)
		return -1;
	if (wire_clock_probe.mode == 2)
		return 1;
	if (wire_clock_probe.mode == 3)
		value->tv_nsec = -1;
	if (wire_clock_probe.mode == 4)
		value->tv_nsec = 1000000000L;
	return 0;
}

static int
test_wire_wait_contract(void)
{
	struct conn_ctx ctx;
	ASSERT_EQ(conn_setup_no_start(&ctx), 0);
	atomic_bool ready;
	atomic_init(&ready, true);
	bool valid = true;
	for (unsigned mode = 0; mode < 5; mode++) {
		wire_clock_probe.mode = mode;
		wire_clock_probe.calls = 0;
		wire_clock_probe.native_successes = 0;
		wire_clock = wire_clock_control;
		bool result = wire_wait(ctx.server, &ready);
		wire_clock = clock_gettime;
		valid = valid && result == (mode == 0) && wire_clock_probe.calls == 1
		    && wire_clock_probe.native_successes == 1;
	}
	wire_clock_probe.calls = 0;
	wire_clock = wire_clock_control;
	bool null_session = wire_wait(NULL, &ready);
	bool null_flag = wire_wait(ctx.server, NULL);
	wire_clock = clock_gettime;
	valid = valid && !null_session && !null_flag && wire_clock_probe.calls == 0;
	dssh_session_terminate(ctx.server);
	atomic_store(&ready, false);
	valid = valid && !wire_wait(ctx.server, &ready);
	atomic_store(&ready, true);
	valid = valid && wire_wait(ctx.server, &ready);
	conn_cleanup(&ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_wire_owner_accept_failure(void)
{
	return wire_owner_cases(WIRE_ACCEPT_FAILURE);
}
static int
test_wire_owner_request_frame(void)
{
	return wire_owner_cases(WIRE_REQUEST_FRAME);
}
static int
test_wire_owner_control_frame(void)
{
	return wire_owner_cases(WIRE_CONTROL_FRAME);
}
static int
test_wire_owner_accept_callback(void)
{
	return wire_owner_cases(WIRE_ACCEPT_CALLBACK);
}

#endif
