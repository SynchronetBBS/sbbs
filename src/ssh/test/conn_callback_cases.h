/* Invoke complete original hosts; keep only scalar observations after return. */
#ifndef DSSH_TEST_CONN_CALLBACK_CASES_H
#define DSSH_TEST_CONN_CALLBACK_CASES_H

static int
test_callback_incoming_lifetime(void)
{
	return incoming_control_probe() == 0 ? TEST_PASS : TEST_FAIL;
}

struct callback_control_stage {
	enum callback_stage stage;
	unsigned ordinal, created, retired;
};
static const struct {
	const char *name;
	int (*function)(void);
	unsigned workers, owners;
	unsigned sizes[16];
	unsigned stages;
	struct callback_control_stage checkpoints[12];
} callback_consumers[] = {
	{"close", test_event_close_race, 3, 2, {2, 1}, 2,
	    {{CALLBACK_GATE, 0, 2, 2}, {CALLBACK_CLOSE, 0, 3, 2}}},
	{"defaults", test_event_client_defaults, 4, 2, {2, 2}, 2,
	    {{CALLBACK_DEFAULT, 0, 2, 1}, {CALLBACK_DEFAULT, 1, 4, 2}}},
	{"atomic", test_event_atomic_state, 9, 6, {2, 1, 2, 1, 2, 1}, 3,
	    {{CALLBACK_DISPATCH, 0, 3, 2}, {CALLBACK_DISPATCH, 1, 6, 4}, {CALLBACK_DISPATCH, 2, 9, 6}}},
	{"lifetime", test_break_handler_lifetime, 6, 10, {1, 0, 1, 0, 1, 1, 1, 0, 1, 0}, 4,
	    {{CALLBACK_GATE, 0, 1, 2}, {CALLBACK_GATE, 1, 2, 4},
	     {CALLBACK_GATE, 2, 3, 6}, {CALLBACK_CLOSE, 2, 4, 6}}},
	{"queue", test_break_handler_queue, 16, 16, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 8,
	    {{CALLBACK_QUEUE, 0, 2, 2}, {CALLBACK_QUEUE, 1, 4, 4},
	     {CALLBACK_QUEUE, 2, 6, 6}, {CALLBACK_QUEUE, 3, 8, 8},
	     {CALLBACK_QUEUE, 4, 10, 10}, {CALLBACK_QUEUE, 5, 12, 12},
	     {CALLBACK_QUEUE, 6, 14, 14}, {CALLBACK_QUEUE, 7, 16, 16}}},
	{"setup", test_break_handler_setup, 4, 4, {1, 1, 1, 1}, 4,
	    {{CALLBACK_SETUP, 0, 1, 1}, {CALLBACK_SETUP, 1, 2, 2},
	     {CALLBACK_SETUP, 2, 3, 3}, {CALLBACK_SETUP, 3, 4, 4}}},
	{"replacement", test_event_replacement, 2, 2, {2, 0}, 1,
	    {{CALLBACK_GATE, 0, 2, 2}}},
	{"incoming", test_callback_incoming_lifetime, 12, 12, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, 12,
	    {{CALLBACK_INCOMING, 0, 1, 1}, {CALLBACK_INCOMING, 1, 2, 2},
	     {CALLBACK_INCOMING, 2, 3, 3}, {CALLBACK_INCOMING, 3, 4, 4},
	     {CALLBACK_INCOMING, 4, 5, 5}, {CALLBACK_INCOMING, 5, 6, 6},
	     {CALLBACK_INCOMING, 6, 7, 7}, {CALLBACK_INCOMING, 7, 8, 8},
	     {CALLBACK_INCOMING, 8, 9, 9}, {CALLBACK_INCOMING, 9, 10, 10},
	     {CALLBACK_INCOMING, 10, 11, 11}, {CALLBACK_INCOMING, 11, 12, 12}}},
};

enum callback_fault {CALLBACK_NATIVE, CALLBACK_CREATE, CALLBACK_STAGE};
static enum callback_fault callback_fault;
static struct callback_control_stage callback_selected;
static bool callback_fired, callback_retired_valid;
static unsigned callback_retired;
static unsigned callback_incoming_retired;
static struct callback_incoming_observation {
	bool clean;
	int events;
	unsigned actions;
} callback_incoming_observations[12];

static bool
callback_control_checkpoint(enum callback_stage stage, unsigned ordinal)
{
	if (callback_fault != CALLBACK_STAGE || callback_selected.stage != stage
	    || callback_selected.ordinal != ordinal)
		return true;
	if (callback_fired)
		abort();
	callback_fired = true;
	return false;
}

static void
callback_control_retire(const struct channel_workers *workers)
{
	if (callback_retired >= 16)
		abort();
	callback_retired++;
	callback_retired_valid = callback_retired_valid && !workers->created[0]
	    && !workers->created[1] && workers->owner->workers == NULL
	    && workers->owner->client != NULL && workers->owner->server != NULL;
	if (workers->release == callback_release_flag) {
		const atomic_bool *flag = workers->resource;
		callback_retired_valid = callback_retired_valid && atomic_load(flag);
	}
	if (workers->release == callback_release_tx) {
		const struct callback_tx_gate *gate = workers->resource;
		callback_retired_valid = callback_retired_valid && !gate->held
		    && !tx_owner_is_current(gate->session);
	}
}

static void
callback_control_incoming_retire(const struct conn_ctx *ctx,
    const atomic_int *events, const atomic_uint *actions)
{
	if (callback_incoming_retired >= 12)
		abort();
	struct callback_incoming_observation *observation =
	    &callback_incoming_observations[callback_incoming_retired++];
	observation->clean = ctx->client == NULL && ctx->server == NULL
	    && !ctx->io_initialized && ctx->workers == NULL && g_active_ctx == NULL;
	observation->events = atomic_load(events);
	observation->actions = atomic_load(actions);
}

static bool
callback_schedule(size_t consumer, enum callback_fault fault, unsigned parameter)
{
	if (consumer >= sizeof(callback_consumers) / sizeof(callback_consumers[0]))
		return false;
	callback_fault = fault;
	callback_fired = false;
	callback_retired = 0;
	callback_incoming_retired = 0;
	callback_retired_valid = true;
	callback_incoming_retirement_observer = callback_control_incoming_retire;
	callback_stage_control = callback_control_checkpoint;
	conn_actor_retirement_observer = callback_control_retire;
	struct channel_worker_probe probe = {.fail_at = fault == CALLBACK_CREATE ? parameter : UINT_MAX,
	    .cancelled_before_join = true};
	channel_worker_probe_begin(&probe);
	int host = callback_consumers[consumer].function();
	channel_worker_probe_end();
	callback_stage_control = NULL;
	callback_incoming_retirement_observer = NULL;
	conn_actor_retirement_observer = NULL;
	unsigned expected = callback_consumers[consumer].workers;
	unsigned retired = callback_consumers[consumer].owners;
	if (fault == CALLBACK_STAGE) {
		expected = callback_selected.created;
		retired = callback_selected.retired;
	}
	if (fault == CALLBACK_CREATE) {
		expected = parameter;
		unsigned starts = 0;
		retired = 0;
		for (unsigned i = 0; i < callback_consumers[consumer].owners; i++) {
			unsigned size = callback_consumers[consumer].sizes[i];
			if (size > 16 - starts)
				abort();
			if (parameter < starts + size) {
				retired = i + 1;
				break;
			}
			starts += size;
		}
	}
	bool valid = host == (fault == CALLBACK_NATIVE ? TEST_PASS : TEST_FAIL)
	    && callback_retired_valid && callback_retired == retired && retired != 0
	    && probe.created == expected && probe.joined == expected
	    && probe.cancelled_before_join && channel_fixture_owner == NULL && g_active_ctx == NULL
	    && callback_fired == (fault == CALLBACK_STAGE)
	    && probe.creation_failed == (fault == CALLBACK_CREATE)
	    && probe.calls == expected + (fault == CALLBACK_CREATE ? 1U : 0U);
	unsigned incoming = callback_consumers[consumer].function == test_callback_incoming_lifetime
	    ? fault == CALLBACK_NATIVE ? 12U : fault == CALLBACK_CREATE
	        ? parameter + 1 : callback_selected.ordinal + 1 : 0U;
	valid = valid && callback_incoming_retired == incoming;
	for (unsigned i = 0; i < callback_incoming_retired; i++) {
		const struct callback_incoming_observation *observation = &callback_incoming_observations[i];
		bool partial = fault != CALLBACK_NATIVE && i + 1 == incoming;
		int events = i % 3 == 2 ? 0 : partial ? fault == CALLBACK_CREATE ? 0 : 1 : 28;
		unsigned actions = partial ? 0U : 21U;
		valid = valid && observation->clean && observation->events == events
		    && observation->actions == actions;
	}
	for (unsigned i = 0; i < probe.created; i++)
		valid = valid && atomic_load(&probe.jobs[i].done);
	unsigned fault_number = (unsigned)fault;
	if (printf("callback_schedule consumer=%s fault=%u parameter=%u host=%d created=%u joined=%u retired=%u capture_retired=%u valid=%u\n",
	    callback_consumers[consumer].name, fault_number, parameter, host,
	    probe.created, probe.joined, callback_retired, callback_incoming_retired, valid ? 1U : 0U) < 0)
		return false;
	return valid;
}

static int
callback_owner_cases(size_t consumer)
{
	if (consumer >= sizeof(callback_consumers) / sizeof(callback_consumers[0]))
		return TEST_FAIL;
	bool valid = callback_schedule(consumer, CALLBACK_NATIVE, 0);
	unsigned schedules = 1;
	for (unsigned i = 0; i < callback_consumers[consumer].workers; i++) {
		valid = callback_schedule(consumer, CALLBACK_CREATE, i) && valid;
		schedules++;
	}
	for (unsigned i = 0; i < callback_consumers[consumer].stages; i++) {
		callback_selected = callback_consumers[consumer].checkpoints[i];
		valid = callback_schedule(consumer, CALLBACK_STAGE, i) && valid;
		schedules++;
	}
	if (printf("callback_control consumer=%s schedules=%u valid=%u\n",
	    callback_consumers[consumer].name, schedules, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
callback_fatal_child(const char *operation, const char *mode)
{
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	for (size_t i = 0; i < sizeof(callback_consumers) / sizeof(callback_consumers[0]); i++) {
		for (unsigned join = 1; join <= callback_consumers[i].workers; join++) {
			char name[64];
			int length = snprintf(name, sizeof(name), "%s-%u", callback_consumers[i].name, join);
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
			callback_consumers[i].function();
			channel_worker_probe_end();
			return 4;
		}
	}
	return 2;
}

static int
test_callback_capture_bounds(void)
{
	struct event_capture capture = {0};
	uint8_t name[128], error[512], language[128];
	memset(name, 'N', sizeof(name));
	memset(error, 'E', sizeof(error));
	memset(language, 'L', sizeof(language));
	struct dssh_chan_event event = {.type = DSSH_EVENT_EXIT_SIGNAL};
	event.exit_signal.signal_name = (struct dssh_bytes){name, sizeof(name)};
	event.exit_signal.error_message = (struct dssh_bytes){error, sizeof(error)};
	event.exit_signal.language = (struct dssh_bytes){language, sizeof(language)};
	ASSERT_TRUE(event_capture_store(&capture, 0, &event));
	ASSERT_TRUE(capture.events[0].exit_signal.signal_name.data == capture.names[0]);
	ASSERT_TRUE(capture.events[0].exit_signal.error_message.data == capture.errors[0]);
	ASSERT_TRUE(capture.events[0].exit_signal.language.data == capture.languages[0]);
	ASSERT_MEM_EQ(capture.names[0], name, sizeof(name));
	ASSERT_MEM_EQ(capture.errors[0], error, sizeof(error));
	ASSERT_MEM_EQ(capture.languages[0], language, sizeof(language));
	for (unsigned kind = 0; kind < 8; kind++) {
		struct dssh_chan_event invalid = event;
		if (kind == 0)
			invalid.exit_signal.signal_name.data = NULL;
		if (kind == 1)
			invalid.exit_signal.signal_name.len++;
		if (kind == 2)
			invalid.exit_signal.error_message.data = NULL;
		if (kind == 3)
			invalid.exit_signal.error_message.len++;
		if (kind == 4)
			invalid.exit_signal.language.data = NULL;
		if (kind == 5)
			invalid.exit_signal.language.len++;
		if (kind == 6)
			invalid.type = 0;
		if (kind == 7)
			invalid.type = INT_MAX;
		capture.events[1].type = DSSH_EVENT_CLOSE;
		capture.events[1].stdout_pos = 77;
		ASSERT_FALSE(event_capture_store(&capture, 1, &invalid));
		ASSERT_EQ(capture.events[1].type, DSSH_EVENT_CLOSE);
		ASSERT_EQ_U(capture.events[1].stdout_pos, 77);
	}
	ASSERT_FALSE(event_capture_store(&capture, EVENT_CAPTURE_CAP, &event));
	ASSERT_FALSE(event_capture_store(&capture, SIZE_MAX, &event));
	ASSERT_FALSE(event_capture_store(&capture, 0, NULL));
	event.exit_signal.signal_name.len = 0;
	event.exit_signal.error_message.len = 0;
	event.exit_signal.language.len = 0;
	ASSERT_TRUE(event_capture_store(&capture, 0, &event));
	ASSERT_NULL(capture.events[0].exit_signal.signal_name.data);
	ASSERT_NULL(capture.events[0].exit_signal.error_message.data);
	ASSERT_NULL(capture.events[0].exit_signal.language.data);
	return TEST_PASS;
}

static int
test_callback_packet_bounds(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	const int types[] = {DSSH_EVENT_SIGNAL, DSSH_EVENT_WINDOW_CHANGE, DSSH_EVENT_EOF,
	    DSSH_EVENT_CLOSE, DSSH_EVENT_EXIT_STATUS, DSSH_EVENT_EXIT_SIGNAL, TEST_CHANNEL_BREAK_REQUEST};
	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		uint8_t packet[128];
		size_t length = 0;
		ASSERT_EQ(event_packet(fixture.open.server_ch, types[i], packet, sizeof(packet), &length), 0);
		ASSERT_TRUE(length > 0 && length <= sizeof(packet));
		for (size_t capacity = 0; capacity < length; capacity++) {
			uint8_t guarded[130];
			memset(guarded, 0xa5, sizeof(guarded));
			size_t rejected_length = 42;
			ASSERT_EQ(event_packet(fixture.open.server_ch, types[i], guarded + 1, capacity,
			    &rejected_length), DSSH_ERROR_TOOLONG);
			ASSERT_EQ_U(rejected_length, 42);
			ASSERT_EQ(guarded[0], 0xa5);
			for (size_t j = capacity + 1; j < sizeof(guarded); j++)
				ASSERT_EQ(guarded[j], 0xa5);
		}
	}
	uint8_t packet[128];
	size_t length = 42;
	ASSERT_EQ(event_packet(NULL, DSSH_EVENT_SIGNAL, packet, sizeof(packet), &length), DSSH_ERROR_INVALID);
	ASSERT_EQ(event_packet(fixture.open.server_ch, DSSH_EVENT_SIGNAL, NULL, 0, &length), DSSH_ERROR_INVALID);
	ASSERT_EQ(event_packet(fixture.open.server_ch, DSSH_EVENT_SIGNAL, packet, sizeof(packet), NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(event_packet(fixture.open.server_ch, 0, packet, sizeof(packet), &length), DSSH_ERROR_INVALID);
	ASSERT_EQ(event_packet(fixture.open.server_ch, INT_MAX, packet, sizeof(packet), &length), DSSH_ERROR_INVALID);
	ASSERT_EQ_U(length, 42);
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

static int
test_callback_owner_close(void)
{
	return callback_owner_cases(0);
}
static int
test_callback_owner_defaults(void)
{
	return callback_owner_cases(1);
}
static int
test_callback_owner_atomic(void)
{
	return callback_owner_cases(2);
}
static int
test_callback_owner_lifetime(void)
{
	return callback_owner_cases(3);
}
static int
test_callback_owner_queue(void)
{
	return callback_owner_cases(4);
}
static int
test_callback_owner_setup(void)
{
	return callback_owner_cases(5);
}
static int
test_callback_owner_replacement(void)
{
	return callback_owner_cases(6);
}
static int
test_callback_owner_incoming(void)
{
	return callback_owner_cases(7);
}

#endif
