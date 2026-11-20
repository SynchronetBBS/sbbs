/* Main-thread checkpoints for channel identity, EOF and setup ownership. */
#ifndef DSSH_TEST_CONN_LIFECYCLE_FIXTURE_H
#define DSSH_TEST_CONN_LIFECYCLE_FIXTURE_H

enum lifecycle_stage {
	LC_ID_LIVE, LC_ID_QUEUE_STARTED, LC_ID_QUEUED,
	LC_ID_OPEN_STARTED, LC_ID_DETACH_PAUSED, LC_ID_PRESSURE,
	LC_PEER_PHASE, LC_PEER_LATE, LC_PEER_ACCEPT_STARTED, LC_PEER_ACCEPT_PAUSED,
	LC_PEER_TERMINAL_STARTED, LC_PEER_ACQUIRING, LC_PEER_RETURNING, LC_PEER_REQUIRED,
	LC_HALF_OPEN_STARTED, LC_HALF_RETRY, LC_HALF_FIRST, LC_HALF_PAUSED,
	LC_HALF_SECOND, LC_HALF_WAITING, LC_HALF_FAILURE,
	LC_SETUP_STARTED, LC_SETUP_PAUSED, LC_SETUP_DETACH_STARTED, LC_SETUP_DETACH_WAITING, LC_SETUP_DONE
};
static bool (*lifecycle_stage_control)(enum lifecycle_stage);

static bool
lifecycle_checkpoint(enum lifecycle_stage stage)
{
	return lifecycle_stage_control == NULL || lifecycle_stage_control(stage);
}

#endif
