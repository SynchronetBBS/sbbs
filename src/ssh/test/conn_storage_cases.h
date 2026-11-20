/* Controls invoke the complete text, snapshot and overflow hosts. */
#ifndef DSSH_TEST_CONN_STORAGE_CASES_H
#define DSSH_TEST_CONN_STORAGE_CASES_H

struct storage_control_stage {
	enum storage_stage stage;
	unsigned created, retired;
};
static const struct {
	const char *name;
	int (*function)(void);
	unsigned group, workers, owners, sizes[8], stages;
	struct storage_control_stage checkpoints[24];
} storage_consumers[] = {
	{"event_text_binary_callback", test_event_text_binary_callback, 0, 2, 1, {2}, 3,
	    {{ST_TEXT_VIEW, 2, 1}, {ST_TEXT_VIEW, 2, 1}, {ST_TEXT_DONE, 2, 1}}},
	{"event_text_binary_poll", test_event_text_binary_poll, 0, 2, 1, {2}, 3,
	    {{ST_TEXT_VIEW, 2, 1}, {ST_TEXT_VIEW, 2, 1}, {ST_TEXT_DONE, 2, 1}}},
	{"event_text_empty_callback", test_event_text_empty_callback, 0, 2, 1, {2}, 3,
	    {{ST_TEXT_VIEW, 2, 1}, {ST_TEXT_VIEW, 2, 1}, {ST_TEXT_DONE, 2, 1}}},
	{"event_text_empty_poll", test_event_text_empty_poll, 0, 2, 1, {2}, 3,
	    {{ST_TEXT_VIEW, 2, 1}, {ST_TEXT_VIEW, 2, 1}, {ST_TEXT_DONE, 2, 1}}},
	{"event_text_packet_boundary", test_event_text_packet_boundary, 0, 2, 2, {2, 0}, 3,
	    {{ST_TEXT_PREPARED, 2, 2}, {ST_TEXT_ALLOCATED, 2, 2}, {ST_TEXT_PACKET, 2, 2}}},
	{"event_text_free_observer", test_event_text_free_observer, 0, 0, 0, {0}, 1,
	    {{ST_TEXT_DONE, 0, 0}}},
	{"event_text_growth_unlocked", test_event_text_growth_unlocked, 0, 2, 1, {2}, 1,
	    {{ST_TEXT_DONE, 2, 1}}},
	{"event_text_poll_race", test_event_text_poll_race, 0, 3, 2, {2, 1}, 4,
	    {{ST_TEXT_STARTED, 3, 2}, {ST_TEXT_QUEUED, 3, 2}, {ST_TEXT_BORROWED, 3, 2}, {ST_TEXT_DONE, 3, 2}}},
	{"event_text_poll_reuse", test_event_text_poll_reuse, 0, 2, 1, {2}, 1,
	    {{ST_TEXT_DONE, 2, 1}}},
	{"storage_text_pattern", test_storage_text_pattern, 0, 2, 1, {2}, 1,
	    {{ST_TEXT_DONE, 2, 1}}},
	{"params_stream_snapshot", test_params_stream_snapshot, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_sync", test_params_stream_sync, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_term", test_params_stream_term, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_command", test_params_stream_command, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_subsystem", test_params_stream_subsystem, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_modes", test_params_stream_modes, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_env_array", test_params_stream_env_array, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_env_name1", test_params_stream_env_name1, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_env_value1", test_params_stream_env_value1, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_env_name2", test_params_stream_env_name2, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_env_value2", test_params_stream_env_value2, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_stream_validation", test_params_stream_validation, 1, 0, 1, {0}, 3,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_INPUT, 0, 1}}},
	{"params_zc_snapshot", test_params_zc_snapshot, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_sync", test_params_zc_sync, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_term", test_params_zc_term, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_command", test_params_zc_command, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_subsystem", test_params_zc_subsystem, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_modes", test_params_zc_modes, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_env_array", test_params_zc_env_array, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_env_name1", test_params_zc_env_name1, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_env_value1", test_params_zc_env_value1, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_env_name2", test_params_zc_env_name2, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_env_value2", test_params_zc_env_value2, 1, 1, 1, {1}, 6,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_STARTED, 1, 1}, {ST_PARAMS_ATTEMPT, 1, 1}, {ST_PARAMS_JOINED, 1, 1}, {ST_PARAMS_COPIED, 1, 1}}},
	{"params_zc_validation", test_params_zc_validation, 1, 0, 1, {0}, 3,
	    {{ST_PARAMS_EMPTY, 0, 1}, {ST_PARAMS_BUILT, 0, 1}, {ST_PARAMS_INPUT, 0, 1}}},
	{"overflow_request_notification", test_overflow_request_notification, 2, 6, 4, {2, 1, 2, 1}, 10,
	    {{ST_OVERFLOW_PREPARED, 2, 2}, {ST_OVERFLOW_HELD, 2, 2}, {ST_OVERFLOW_STARTED, 3, 2}, {ST_OVERFLOW_DONE, 3, 2}, {ST_OVERFLOW_FINISHED, 3, 2}, {ST_OVERFLOW_PREPARED, 5, 4}, {ST_OVERFLOW_HELD, 5, 4}, {ST_OVERFLOW_STARTED, 6, 4}, {ST_OVERFLOW_DONE, 6, 4}, {ST_OVERFLOW_FINISHED, 6, 4}}},
	{"overflow_request_close_failure", test_overflow_request_close_failure, 2, 3, 2, {2, 1}, 5,
	    {{ST_OVERFLOW_PREPARED, 2, 2}, {ST_OVERFLOW_HELD, 2, 2}, {ST_OVERFLOW_STARTED, 3, 2}, {ST_OVERFLOW_DONE, 3, 2}, {ST_OVERFLOW_FINISHED, 3, 2}}},
	{"overflow_request_normal", test_overflow_request_normal, 2, 3, 2, {2, 1}, 5,
	    {{ST_OVERFLOW_PREPARED, 2, 2}, {ST_OVERFLOW_HELD, 2, 2}, {ST_OVERFLOW_STARTED, 3, 2}, {ST_OVERFLOW_WAITING, 3, 2}, {ST_OVERFLOW_FINISHED, 3, 2}}},
	{"overflow_eof_notification", test_overflow_eof_notification, 2, 6, 4, {2, 1, 2, 1}, 10,
	    {{ST_OVERFLOW_PREPARED, 2, 2}, {ST_OVERFLOW_HELD, 2, 2}, {ST_OVERFLOW_STARTED, 3, 2}, {ST_OVERFLOW_DONE, 3, 2}, {ST_OVERFLOW_FINISHED, 3, 2}, {ST_OVERFLOW_PREPARED, 5, 4}, {ST_OVERFLOW_HELD, 5, 4}, {ST_OVERFLOW_STARTED, 6, 4}, {ST_OVERFLOW_DONE, 6, 4}, {ST_OVERFLOW_FINISHED, 6, 4}}},
	{"overflow_eof_close_failure", test_overflow_eof_close_failure, 2, 3, 2, {2, 1}, 5,
	    {{ST_OVERFLOW_PREPARED, 2, 2}, {ST_OVERFLOW_HELD, 2, 2}, {ST_OVERFLOW_STARTED, 3, 2}, {ST_OVERFLOW_DONE, 3, 2}, {ST_OVERFLOW_FINISHED, 3, 2}}},
	{"overflow_eof_normal", test_overflow_eof_normal, 2, 3, 2, {2, 1}, 5,
	    {{ST_OVERFLOW_PREPARED, 2, 2}, {ST_OVERFLOW_HELD, 2, 2}, {ST_OVERFLOW_STARTED, 3, 2}, {ST_OVERFLOW_WAITING, 3, 2}, {ST_OVERFLOW_FINISHED, 3, 2}}},
	{"overflow_close_notification", test_overflow_close_notification, 2, 6, 4, {2, 1, 2, 1}, 10,
	    {{ST_OVERFLOW_PREPARED, 2, 2}, {ST_OVERFLOW_HELD, 2, 2}, {ST_OVERFLOW_STARTED, 3, 2}, {ST_OVERFLOW_DONE, 3, 2}, {ST_OVERFLOW_FINISHED, 3, 2}, {ST_OVERFLOW_PREPARED, 5, 4}, {ST_OVERFLOW_HELD, 5, 4}, {ST_OVERFLOW_STARTED, 6, 4}, {ST_OVERFLOW_DONE, 6, 4}, {ST_OVERFLOW_FINISHED, 6, 4}}},
	{"overflow_close_close_failure", test_overflow_close_close_failure, 2, 3, 2, {2, 1}, 5,
	    {{ST_OVERFLOW_PREPARED, 2, 2}, {ST_OVERFLOW_HELD, 2, 2}, {ST_OVERFLOW_STARTED, 3, 2}, {ST_OVERFLOW_DONE, 3, 2}, {ST_OVERFLOW_FINISHED, 3, 2}}},
	{"overflow_close_normal", test_overflow_close_normal, 2, 3, 2, {2, 1}, 5,
	    {{ST_OVERFLOW_PREPARED, 2, 2}, {ST_OVERFLOW_HELD, 2, 2}, {ST_OVERFLOW_STARTED, 3, 2}, {ST_OVERFLOW_WAITING, 3, 2}, {ST_OVERFLOW_FINISHED, 3, 2}}},
};

enum storage_fault {STORAGE_NATIVE, STORAGE_CREATE, STORAGE_STAGE};
static enum storage_fault storage_fault;
static size_t storage_selected_consumer;
static unsigned storage_parameter, storage_seen, storage_retired, storage_buffers, storage_builders;
static bool storage_fired, storage_valid_order, storage_valid_retirement;

static bool
storage_control_checkpoint(enum storage_stage stage)
{
	if (storage_seen >= storage_consumers[storage_selected_consumer].stages) {
		storage_valid_order = false;
		return true;
	}
	const struct storage_control_stage *expected =
	    &storage_consumers[storage_selected_consumer].checkpoints[storage_seen];
	storage_valid_order = storage_valid_order && stage == expected->stage
	    && channel_worker_probe != NULL && channel_worker_probe->created == expected->created;
	unsigned active = channel_fixture_owner != NULL ? 1U : 0U;
	storage_valid_order = storage_valid_order && storage_retired <= UINT_MAX - active
	    && storage_retired + active == expected->retired;
	unsigned index = storage_seen++;
	if (storage_fault == STORAGE_STAGE && index == storage_parameter) {
		storage_fired = true;
		return false;
	}
	return true;
}

static void
storage_control_retire(const struct channel_workers *workers)
{
	if (storage_retired >= 8)
		abort();
	storage_retired++;
	storage_valid_retirement = storage_valid_retirement && !workers->created[0]
	    && !workers->created[1] && workers->owner->workers == NULL
	    && workers->owner->client != NULL && workers->owner->server != NULL
	    && !tx_owner_is_current(workers->owner->client)
	    && !tx_owner_is_current(workers->owner->server);
	if (workers->release == NULL)
		return;
	if (workers->release != flow_release)
		abort();
	const struct flow_fixture *flow = workers->resource;
	storage_valid_retirement = storage_valid_retirement && !flow->tx.held
	    && flow->reserved == NULL && !flow->watching && !flow->paused && flow->cancels == 0;
	if (flow->unblock == event_text_poll_release) {
		const struct event_text_poll_job *job = flow->data;
		storage_valid_retirement = storage_valid_retirement && atomic_load(&job->resume);
		if (storage_fired || channel_worker_probe->creation_failed)
			storage_valid_retirement = storage_valid_retirement && atomic_load(&job->cancelled);
	}
	if (workers->retire == storage_packet_retire) {
		const struct storage_packet_fixture *fixture = flow->data;
		storage_valid_retirement = storage_valid_retirement && fixture->packet == NULL;
		if (fixture->frees > 1)
			abort();
		storage_buffers += fixture->frees;
	}
	if (workers->retire == params_snapshot_retire) {
		const struct params_snapshot_fixture *fixture = flow->data;
		storage_valid_retirement = storage_valid_retirement && fixture->retired
		    && fixture->params.term == NULL && fixture->params.command == NULL
		    && fixture->params.subsystem == NULL && fixture->params.env == NULL
		    && fixture->params.env_count == 0 && fixture->params.env_capacity == 0
		    && fixture->params.modes == NULL && fixture->params.mode_count == 0
		    && fixture->params.mode_capacity == 0 && params_snapshot_current == NULL
		    && accept_phase_test_hook == NULL;
		storage_builders++;
	}
}

static bool
storage_schedule(size_t consumer, enum storage_fault fault, unsigned parameter)
{
	if (consumer >= sizeof(storage_consumers) / sizeof(storage_consumers[0]))
		return false;
	storage_selected_consumer = consumer;
	storage_fault = fault;
	storage_parameter = parameter;
	storage_fired = false;
	storage_seen = 0;
	storage_retired = 0;
	storage_buffers = 0;
	storage_builders = 0;
	storage_valid_order = true;
	storage_valid_retirement = true;
	storage_stage_control = storage_control_checkpoint;
	conn_actor_retirement_observer = storage_control_retire;
	struct channel_worker_probe probe = {.fail_at = fault == STORAGE_CREATE ? parameter : UINT_MAX,
	    .cancelled_before_join = true};
	channel_worker_probe_begin(&probe);
	int host = storage_consumers[consumer].function();
	channel_worker_probe_end();
	storage_stage_control = NULL;
	conn_actor_retirement_observer = NULL;
	unsigned expected = storage_consumers[consumer].workers;
	unsigned retired = storage_consumers[consumer].owners;
	unsigned stages = storage_consumers[consumer].stages;
	unsigned buffers = fault == STORAGE_NATIVE && strcmp(storage_consumers[consumer].name, "event_text_packet_boundary") == 0 ? 1U : 0U;
	unsigned builders = storage_consumers[consumer].group == 1 ? 1U : 0U;
	if (fault == STORAGE_STAGE) {
		const struct storage_control_stage *stage = &storage_consumers[consumer].checkpoints[parameter];
		expected = stage->created;
		retired = stage->retired;
		stages = parameter + 1;
		buffers = stage->stage == ST_TEXT_ALLOCATED || stage->stage == ST_TEXT_PACKET ? 1U : 0U;
	}
	if (fault == STORAGE_CREATE) {
		expected = parameter;
		unsigned starts = 0;
		retired = 0;
		for (unsigned i = 0; i < storage_consumers[consumer].owners; i++) {
			unsigned size = storage_consumers[consumer].sizes[i];
			if (size > CHANNEL_WORKER_PROBE_CAP - starts)
				abort();
			if (parameter < starts + size) {
				retired = i + 1;
				break;
			}
			starts += size;
		}
		stages = 0;
		while (stages < storage_consumers[consumer].stages
		    && storage_consumers[consumer].checkpoints[stages].created <= parameter)
			stages++;
	}
	bool valid = host == (fault == STORAGE_NATIVE ? TEST_PASS : TEST_FAIL)
	    && storage_valid_order && storage_seen == stages && storage_valid_retirement
	    && storage_retired == retired && storage_buffers == buffers && storage_builders == builders
	    && probe.created == expected && probe.joined == expected
	    && probe.cancelled_before_join && channel_fixture_owner == NULL && g_active_ctx == NULL
	    && conn_rx_gate == NULL && storage_fired == (fault == STORAGE_STAGE)
	    && probe.creation_failed == (fault == STORAGE_CREATE)
	    && probe.calls == expected + (fault == STORAGE_CREATE ? 1U : 0U);
	for (unsigned i = 0; i < probe.created; i++)
		valid = valid && atomic_load(&probe.jobs[i].done);
	unsigned fault_number = (unsigned)fault;
	if (printf("storage_schedule consumer=%s fault=%u parameter=%u host=%d created=%u joined=%u retired=%u stages=%u buffers=%u builders=%u valid=%u\n",
	    storage_consumers[consumer].name, fault_number, parameter, host, probe.created,
	    probe.joined, storage_retired, storage_seen, storage_buffers, storage_builders, valid ? 1U : 0U) < 0)
		return false;
	return valid;
}

static int
storage_owner_cases(unsigned group)
{
	bool valid = true;
	unsigned schedules = 0, consumers = 0;
	for (size_t i = 0; i < sizeof(storage_consumers) / sizeof(storage_consumers[0]); i++) {
		if (storage_consumers[i].group != group)
			continue;
		consumers++;
		valid = storage_schedule(i, STORAGE_NATIVE, 0);
		if (!valid)
			return TEST_FAIL;
		schedules++;
		for (unsigned fail = 0; fail < storage_consumers[i].workers; fail++) {
			valid = storage_schedule(i, STORAGE_CREATE, fail);
			if (!valid)
				return TEST_FAIL;
			schedules++;
		}
		for (unsigned stage = 0; stage < storage_consumers[i].stages; stage++) {
			valid = storage_schedule(i, STORAGE_STAGE, stage);
			if (!valid)
				return TEST_FAIL;
			schedules++;
		}
	}
	if (printf("storage_control group=%u consumers=%u schedules=%u valid=%u\n",
	    group, consumers, schedules, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(consumers != 0 && valid);
	return TEST_PASS;
}

static int
storage_fatal_child(const char *operation, const char *mode)
{
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	for (size_t i = 0; i < sizeof(storage_consumers) / sizeof(storage_consumers[0]); i++) {
		for (unsigned join = 1; join <= storage_consumers[i].workers; join++) {
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
			storage_consumers[i].function();
			channel_worker_probe_end();
			return 4;
		}
	}
	return 2;
}

static int
test_storage_owner_text(void)
{
	return storage_owner_cases(0);
}
static int
test_storage_owner_params(void)
{
	return storage_owner_cases(1);
}
static int
test_storage_owner_overflow(void)
{
	return storage_owner_cases(2);
}

#endif
