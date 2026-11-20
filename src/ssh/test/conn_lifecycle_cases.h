/* Controls run the complete original hosts, with native created handles. */
#ifndef DSSH_TEST_CONN_LIFECYCLE_CASES_H
#define DSSH_TEST_CONN_LIFECYCLE_CASES_H

struct lifecycle_control_stage {
	enum lifecycle_stage stage;
	unsigned created, retired;
};
static const struct {
	const char *name;
	int (*function)(void);
	unsigned group, workers, owners, sizes[8], stages;
	struct lifecycle_control_stage checkpoints[24];
} lifecycle_consumers[] = {
	{"channel_id_lifetime", test_channel_id_lifetime, 0, 6, 3, {2, 2, 2}, 3,
	    {{LC_ID_LIVE, 2, 1}, {LC_ID_LIVE, 4, 2}, {LC_ID_LIVE, 6, 3}}},
	{"channel_id_late_reply", test_channel_id_late_reply, 0, 2, 2, {1, 1}, 4,
	    {{LC_ID_QUEUE_STARTED, 1, 1}, {LC_ID_QUEUED, 1, 1}, {LC_ID_OPEN_STARTED, 2, 2}, {LC_ID_DETACH_PAUSED, 2, 2}}},
	{"channel_id_backpressure", test_channel_id_backpressure, 0, 0, 0, {0}, 1,
	    {{LC_ID_PRESSURE, 0, 0}}},
	{"peer_id_queued", test_peer_id_queued, 1, 0, 0, {0}, 1,
	    {{LC_PEER_PHASE, 0, 0}}},
	{"peer_id_popped", test_peer_id_popped, 1, 0, 0, {0}, 1,
	    {{LC_PEER_PHASE, 0, 0}}},
	{"peer_id_confirmed", test_peer_id_confirmed, 1, 0, 0, {0}, 1,
	    {{LC_PEER_PHASE, 0, 0}}},
	{"peer_id_late_confirmation", test_peer_id_late_confirmation, 1, 0, 0, {0}, 1,
	    {{LC_PEER_LATE, 0, 0}}},
	{"peer_id_concurrent_accept", test_peer_id_concurrent_accept, 1, 2, 1, {2}, 2,
	    {{LC_PEER_ACCEPT_STARTED, 2, 1}, {LC_PEER_ACCEPT_PAUSED, 2, 1}}},
	{"peer_id_ordered_terminal", test_peer_id_ordered_terminal, 1, 4, 4, {1, 1, 1, 1}, 11,
	    {{LC_PEER_TERMINAL_STARTED, 1, 1}, {LC_PEER_RETURNING, 1, 1}, {LC_PEER_TERMINAL_STARTED, 2, 2}, {LC_PEER_ACQUIRING, 2, 2}, {LC_PEER_RETURNING, 2, 2}, {LC_PEER_TERMINAL_STARTED, 3, 3}, {LC_PEER_RETURNING, 3, 3}, {LC_PEER_TERMINAL_STARTED, 4, 4}, {LC_PEER_ACQUIRING, 4, 4}, {LC_PEER_RETURNING, 4, 4}, {LC_PEER_REQUIRED, 4, 4}}},
	{"half_close_retry", test_half_close_retry, 2, 4, 4, {1, 1, 1, 1}, 12,
	    {{LC_HALF_RETRY, 0, 0}, {LC_HALF_RETRY, 0, 0}, {LC_HALF_RETRY, 0, 0}, {LC_HALF_RETRY, 0, 0}, {LC_HALF_OPEN_STARTED, 1, 1}, {LC_HALF_RETRY, 1, 1}, {LC_HALF_OPEN_STARTED, 2, 2}, {LC_HALF_RETRY, 2, 2}, {LC_HALF_OPEN_STARTED, 3, 3}, {LC_HALF_RETRY, 3, 3}, {LC_HALF_OPEN_STARTED, 4, 4}, {LC_HALF_RETRY, 4, 4}}},
	{"half_close_concurrent", test_half_close_concurrent, 2, 10, 6, {2, 2, 1, 2, 1, 2}, 18,
	    {{LC_HALF_FIRST, 1, 1}, {LC_HALF_PAUSED, 1, 1}, {LC_HALF_SECOND, 2, 1}, {LC_HALF_WAITING, 2, 1}, {LC_HALF_FIRST, 3, 2}, {LC_HALF_PAUSED, 3, 2}, {LC_HALF_SECOND, 4, 2}, {LC_HALF_WAITING, 4, 2}, {LC_HALF_OPEN_STARTED, 5, 3}, {LC_HALF_FIRST, 6, 4}, {LC_HALF_PAUSED, 6, 4}, {LC_HALF_SECOND, 7, 4}, {LC_HALF_WAITING, 7, 4}, {LC_HALF_OPEN_STARTED, 8, 5}, {LC_HALF_FIRST, 9, 6}, {LC_HALF_PAUSED, 9, 6}, {LC_HALF_SECOND, 10, 6}, {LC_HALF_WAITING, 10, 6}}},
	{"half_close_failure", test_half_close_failure, 2, 2, 2, {1, 1}, 6,
	    {{LC_HALF_FAILURE, 0, 0}, {LC_HALF_FAILURE, 0, 0}, {LC_HALF_OPEN_STARTED, 1, 1}, {LC_HALF_FAILURE, 1, 1}, {LC_HALF_OPEN_STARTED, 2, 2}, {LC_HALF_FAILURE, 2, 2}}},
	{"half_close_publication", test_half_close_publication, 2, 2, 2, {1, 1}, 6,
	    {{LC_HALF_FAILURE, 0, 0}, {LC_HALF_FAILURE, 0, 0}, {LC_HALF_OPEN_STARTED, 1, 1}, {LC_HALF_FAILURE, 1, 1}, {LC_HALF_OPEN_STARTED, 2, 2}, {LC_HALF_FAILURE, 2, 2}}},
	{"setup_mailbox_reuse", test_setup_mailbox_reuse, 3, 2, 1, {2}, 1,
	    {{LC_SETUP_DONE, 2, 1}}},
	{"setup_mailbox_growth", test_setup_mailbox_growth, 3, 2, 1, {2}, 1,
	    {{LC_SETUP_DONE, 2, 1}}},
	{"setup_mailbox_rollback", test_setup_mailbox_rollback, 3, 2, 1, {2}, 1,
	    {{LC_SETUP_DONE, 2, 1}}},
	{"setup_mailbox_bound", test_setup_mailbox_bound, 3, 2, 1, {2}, 1,
	    {{LC_SETUP_DONE, 2, 1}}},
	{"setup_mailbox_no_allocation", test_setup_mailbox_no_allocation, 3, 2, 1, {2}, 1,
	    {{LC_SETUP_DONE, 2, 1}}},
	{"setup_terminal_storage", test_setup_terminal_storage, 3, 2, 1, {2}, 1,
	    {{LC_SETUP_DONE, 2, 1}}},
	{"setup_terminal_first_failure", test_setup_terminal_first_failure, 3, 2, 1, {2}, 1,
	    {{LC_SETUP_DONE, 2, 1}}},
	{"setup_terminal_second_failure", test_setup_terminal_second_failure, 3, 2, 1, {2}, 1,
	    {{LC_SETUP_DONE, 2, 1}}},
	{"setup_confirmation_cleanup", test_setup_confirmation_cleanup, 3, 0, 0, {0}, 1,
	    {{LC_SETUP_DONE, 0, 0}}},
	{"setup_gap_disable", test_setup_gap_disable, 3, 3, 2, {2, 1}, 2,
	    {{LC_SETUP_STARTED, 3, 2}, {LC_SETUP_PAUSED, 3, 2}}},
	{"setup_gap_detach", test_setup_gap_detach, 3, 4, 2, {2, 2}, 4,
	    {{LC_SETUP_STARTED, 3, 2}, {LC_SETUP_PAUSED, 3, 2}, {LC_SETUP_DETACH_STARTED, 4, 2}, {LC_SETUP_DETACH_WAITING, 4, 2}}},
	{"setup_gap_terminate", test_setup_gap_terminate, 3, 3, 2, {2, 1}, 2,
	    {{LC_SETUP_STARTED, 3, 2}, {LC_SETUP_PAUSED, 3, 2}}},
	{"setup_gap_reacquire", test_setup_gap_reacquire, 3, 3, 2, {2, 1}, 2,
	    {{LC_SETUP_STARTED, 3, 2}, {LC_SETUP_PAUSED, 3, 2}}},
	{"setup_packet_bounds", test_setup_packet_bounds, 3, 2, 1, {2}, 0,
	    {{LC_SETUP_DONE, 0, 0}}},
};

enum lifecycle_fault {LIFECYCLE_NATIVE, LIFECYCLE_CREATE, LIFECYCLE_STAGE};
static enum lifecycle_fault lifecycle_fault;
static size_t lifecycle_selected_consumer;
static unsigned lifecycle_parameter, lifecycle_seen, lifecycle_retired, lifecycle_disposed;
static bool lifecycle_fired, lifecycle_valid_order, lifecycle_valid_retirement;

static bool
lifecycle_control_checkpoint(enum lifecycle_stage stage)
{
	if (lifecycle_seen >= lifecycle_consumers[lifecycle_selected_consumer].stages) {
		lifecycle_valid_order = false;
		return true;
	}
	const struct lifecycle_control_stage *expected =
	    &lifecycle_consumers[lifecycle_selected_consumer].checkpoints[lifecycle_seen];
	lifecycle_valid_order = lifecycle_valid_order && stage == expected->stage
	    && channel_worker_probe != NULL && channel_worker_probe->created == expected->created;
	unsigned active = channel_fixture_owner != NULL ? 1U : 0U;
	lifecycle_valid_order = lifecycle_valid_order && lifecycle_retired <= UINT_MAX - active
	    && lifecycle_retired + active == expected->retired;
	unsigned index = lifecycle_seen++;
	if (lifecycle_fault == LIFECYCLE_STAGE && index == lifecycle_parameter) {
		lifecycle_fired = true;
		return false;
	}
	return true;
}

static void
lifecycle_control_retire(const struct channel_workers *workers)
{
	if (lifecycle_retired >= 8)
		abort();
	lifecycle_retired++;
	lifecycle_valid_retirement = lifecycle_valid_retirement && !workers->created[0]
	    && !workers->created[1] && workers->owner->workers == NULL
	    && workers->owner->client != NULL && workers->owner->server != NULL
	    && !tx_owner_is_current(workers->owner->client)
	    && !tx_owner_is_current(workers->owner->server);
	if (workers->release == NULL)
		return;
	if (workers->release != flow_release)
		abort();
	const struct flow_fixture *flow = workers->resource;
	lifecycle_valid_retirement = lifecycle_valid_retirement && !flow->tx.held
	    && flow->reserved == NULL && !flow->watching && !flow->paused && flow->cancels == 0;
	if (flow->unblock == channel_id_release_open) {
		const struct channel_id_open_job *job = flow->data;
		lifecycle_valid_retirement = lifecycle_valid_retirement && atomic_load(&job->cancelled);
	}
	if (flow->unblock == peer_id_release_accept) {
		const struct peer_id_accept_job *jobs = flow->data;
		lifecycle_valid_retirement = lifecycle_valid_retirement && atomic_load(&jobs[0].resume)
		    && atomic_load(&jobs[1].resume);
	}
	if (flow->unblock == peer_id_release_terminal) {
		const struct peer_id_terminal_job *job = flow->data;
		lifecycle_valid_retirement = lifecycle_valid_retirement && atomic_load(&job->resume);
	}
	if (flow->unblock == half_close_release) {
		const struct half_close_job *jobs = flow->data;
		lifecycle_valid_retirement = lifecycle_valid_retirement && atomic_load(&jobs[0].resume)
		    && atomic_load(&jobs[1].resume);
	}
	if (flow->unblock == setup_gap_release) {
		const struct setup_gap_lifetime *life = flow->data;
		lifecycle_valid_retirement = lifecycle_valid_retirement && atomic_load(&life->writer->release);
		if (life->closed) {
			lifecycle_disposed++;
			lifecycle_valid_retirement = lifecycle_valid_retirement
			    && life->writer->fixture->open.server_ch == NULL;
		}
	}
}

static bool
lifecycle_schedule(size_t consumer, enum lifecycle_fault fault, unsigned parameter)
{
	if (consumer >= sizeof(lifecycle_consumers) / sizeof(lifecycle_consumers[0]))
		return false;
	lifecycle_selected_consumer = consumer;
	lifecycle_fault = fault;
	lifecycle_parameter = parameter;
	lifecycle_fired = false;
	lifecycle_seen = 0;
	lifecycle_retired = 0;
	lifecycle_disposed = 0;
	lifecycle_valid_order = true;
	lifecycle_valid_retirement = true;
	lifecycle_stage_control = lifecycle_control_checkpoint;
	conn_actor_retirement_observer = lifecycle_control_retire;
	struct channel_worker_probe probe = {.fail_at = fault == LIFECYCLE_CREATE ? parameter : UINT_MAX,
	    .cancelled_before_join = true};
	channel_worker_probe_begin(&probe);
	int host = lifecycle_consumers[consumer].function();
	channel_worker_probe_end();
	lifecycle_stage_control = NULL;
	conn_actor_retirement_observer = NULL;
	unsigned expected = lifecycle_consumers[consumer].workers;
	unsigned retired = lifecycle_consumers[consumer].owners;
	unsigned stages = lifecycle_consumers[consumer].stages;
	unsigned disposed = fault == LIFECYCLE_NATIVE && strcmp(lifecycle_consumers[consumer].name, "setup_gap_detach") == 0 ? 1U : 0U;
	if (fault == LIFECYCLE_STAGE) {
		const struct lifecycle_control_stage *stage = &lifecycle_consumers[consumer].checkpoints[parameter];
		expected = stage->created;
		retired = stage->retired;
		stages = parameter + 1;
		disposed = stage->stage == LC_SETUP_DETACH_STARTED || stage->stage == LC_SETUP_DETACH_WAITING ? 1U : 0U;
	}
	if (fault == LIFECYCLE_CREATE) {
		expected = parameter;
		unsigned starts = 0;
		retired = 0;
		for (unsigned i = 0; i < lifecycle_consumers[consumer].owners; i++) {
			unsigned size = lifecycle_consumers[consumer].sizes[i];
			if (size > CHANNEL_WORKER_PROBE_CAP - starts)
				abort();
			if (parameter < starts + size) {
				retired = i + 1;
				break;
			}
			starts += size;
		}
		stages = 0;
		while (stages < lifecycle_consumers[consumer].stages
		    && lifecycle_consumers[consumer].checkpoints[stages].created <= parameter)
			stages++;
	}
	bool valid = host == (fault == LIFECYCLE_NATIVE ? TEST_PASS : TEST_FAIL)
	    && lifecycle_valid_order && lifecycle_seen == stages && lifecycle_valid_retirement
	    && lifecycle_retired == retired && lifecycle_disposed == disposed
	    && probe.created == expected && probe.joined == expected
	    && probe.cancelled_before_join && channel_fixture_owner == NULL && g_active_ctx == NULL
	    && conn_rx_gate == NULL && lifecycle_fired == (fault == LIFECYCLE_STAGE)
	    && probe.creation_failed == (fault == LIFECYCLE_CREATE)
	    && probe.calls == expected + (fault == LIFECYCLE_CREATE ? 1U : 0U);
	for (unsigned i = 0; i < probe.created; i++)
		valid = valid && atomic_load(&probe.jobs[i].done);
	unsigned fault_number = (unsigned)fault;
	if (printf("lifecycle_schedule consumer=%s fault=%u parameter=%u host=%d created=%u joined=%u retired=%u stages=%u disposed=%u valid=%u\n",
	    lifecycle_consumers[consumer].name, fault_number, parameter, host, probe.created,
	    probe.joined, lifecycle_retired, lifecycle_seen, lifecycle_disposed, valid ? 1U : 0U) < 0)
		return false;
	return valid;
}

static int
lifecycle_owner_cases(unsigned group)
{
	bool valid = true;
	unsigned schedules = 0, consumers = 0;
	for (size_t i = 0; i < sizeof(lifecycle_consumers) / sizeof(lifecycle_consumers[0]); i++) {
		if (lifecycle_consumers[i].group != group)
			continue;
		consumers++;
		valid = lifecycle_schedule(i, LIFECYCLE_NATIVE, 0);
		if (!valid)
			return TEST_FAIL;
		schedules++;
		for (unsigned fail = 0; fail < lifecycle_consumers[i].workers; fail++) {
			valid = lifecycle_schedule(i, LIFECYCLE_CREATE, fail);
			if (!valid)
				return TEST_FAIL;
			schedules++;
		}
		for (unsigned stage = 0; stage < lifecycle_consumers[i].stages; stage++) {
			valid = lifecycle_schedule(i, LIFECYCLE_STAGE, stage);
			if (!valid)
				return TEST_FAIL;
			schedules++;
		}
	}
	if (printf("lifecycle_control group=%u consumers=%u schedules=%u valid=%u\n",
	    group, consumers, schedules, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(consumers != 0 && valid);
	return TEST_PASS;
}

static int
lifecycle_fatal_child(const char *operation, const char *mode)
{
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	for (size_t i = 0; i < sizeof(lifecycle_consumers) / sizeof(lifecycle_consumers[0]); i++) {
		for (unsigned join = 1; join <= lifecycle_consumers[i].workers; join++) {
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
			lifecycle_consumers[i].function();
			channel_worker_probe_end();
			return 4;
		}
	}
	return 2;
}

static int
test_lifecycle_owner_ids(void)
{
	return lifecycle_owner_cases(0);
}
static int
test_lifecycle_owner_peer_ids(void)
{
	return lifecycle_owner_cases(1);
}
static int
test_lifecycle_owner_half_close(void)
{
	return lifecycle_owner_cases(2);
}
static int
test_lifecycle_owner_setup(void)
{
	return lifecycle_owner_cases(3);
}

#endif
