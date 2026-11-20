/* Private ownership checkpoints for complete authenticated wire fixtures. */
#ifndef DSSH_TEST_CONN_WIRE_FIXTURE_H
#define DSSH_TEST_CONN_WIRE_FIXTURE_H

enum wire_suite {
	WIRE_ACCEPT_FAILURE, WIRE_REQUEST_FRAME, WIRE_CONTROL_FRAME, WIRE_ACCEPT_CALLBACK
};
enum wire_stage {WIRE_STARTED, WIRE_CLOSE_WAIT, WIRE_PUBLISHED};
static int (*wire_clock)(clockid_t, struct timespec *) = clock_gettime;
static bool (*wire_stage_control)(enum wire_suite, unsigned, enum wire_stage);

static bool
wire_checkpoint(enum wire_suite suite, unsigned variant, enum wire_stage stage)
{
	return wire_stage_control == NULL || wire_stage_control(suite, variant, stage);
}

/* Application gate waits stop when the owning session is cancelled. */
static bool
wire_wait(dssh_session session, const atomic_bool *flag)
{
	if (session == NULL || flag == NULL)
		return false;
	struct timespec start;
	if (wire_clock(CLOCK_MONOTONIC, &start) != 0
	    || start.tv_nsec < 0 || start.tv_nsec >= 1000000000L)
		return false;
	while (!atomic_load(flag)) {
		if (dssh_session_is_terminated(session) || !dssh_test_wait_tick(&start))
			return false;
	}
	return true;
}

#endif
