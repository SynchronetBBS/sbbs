/* Application-side callback gates and transferred close handles. */
#ifndef DSSH_TEST_CONN_CALLBACK_FIXTURE_H
#define DSSH_TEST_CONN_CALLBACK_FIXTURE_H

enum callback_stage {
	CALLBACK_GATE, CALLBACK_CLOSE, CALLBACK_DEFAULT, CALLBACK_DISPATCH,
	CALLBACK_QUEUE, CALLBACK_SETUP, CALLBACK_INCOMING
};
static bool (*callback_stage_control)(enum callback_stage, unsigned);
/* Called on the owning frame only after actual session cleanup completes. */
static void (*callback_incoming_retirement_observer)(const struct conn_ctx *,
    const atomic_int *, const atomic_uint *);

static bool
callback_checkpoint(enum callback_stage stage, unsigned ordinal)
{
	return callback_stage_control == NULL || callback_stage_control(stage, ordinal);
}

static void
callback_release_flag(void *data)
{
	atomic_bool *flag = data;
	atomic_store(flag, true);
}

static void
callback_resume_dispatch(void *unused)
{
	(void)unused;
	dssh_test_thrd_resume_mutex();
}

static void
callback_retire_dispatch(void *unused)
{
	(void)unused;
	dssh_test_thrd_pause_mutex(NULL);
}

struct callback_tx_gate {
	dssh_session session;
	bool held;
	int result;
};

static void
callback_release_tx(void *data)
{
	struct callback_tx_gate *gate = data;
	if (gate->held) {
		gate->result = tx_unlock_with_result(gate->session, 0);
		gate->held = false;
	}
}

struct close_channel_ctx {
	dssh_channel ch;
	atomic_bool waiting;
	atomic_bool done;
	int result;
};

static int
close_channel_thread(void *data)
{
	struct close_channel_ctx *close = data;
	dssh_test_detach_observe(&close->waiting);
	close->result = dssh_chan_close(close->ch, -1);
	dssh_test_detach_observe(NULL);
	close->ch = NULL;
	atomic_store(&close->done, true);
	return close->result;
}

static int
callback_start_close(struct channel_workers *workers,
    struct close_channel_ctx *close, dssh_channel *channel)
{
	if (*channel == NULL)
		return -1;
	close->ch = *channel;
	close->result = -1;
	atomic_init(&close->waiting, false);
	atomic_init(&close->done, false);
	int ret = channel_worker_start(workers, 0, close_channel_thread, close);
	if (ret == 0)
		*channel = NULL;
	return ret;
}

#endif
