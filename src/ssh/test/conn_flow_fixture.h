/* Private owners for transport/credit fixtures with application gates. */
#ifndef DSSH_TEST_CONN_FLOW_FIXTURE_H
#define DSSH_TEST_CONN_FLOW_FIXTURE_H

enum flow_stage {
	FLOW_TX_STARTED, FLOW_TX_WAITING, FLOW_REFUSAL_STARTED, FLOW_REFUSAL_WAITING,
	FLOW_REKEY_PREPARED, FLOW_REKEY_STARTED, FLOW_REKEY_WAITING,
	FLOW_WINDOW_STARTED, FLOW_WINDOW_ACCEPTED, FLOW_WINDOW_RESERVED, FLOW_WINDOW_MARKER,
	FLOW_QUEUE_STARTED, FLOW_QUEUE_OWNED, FLOW_QUEUE_READY, FLOW_QUEUE_MERGER, FLOW_QUEUE_REFUSED
};
static bool (*flow_stage_control)(enum flow_stage);

static bool
flow_checkpoint(enum flow_stage stage)
{
	return flow_stage_control == NULL || flow_stage_control(stage);
}

struct flow_fixture {
	struct channel_workers workers;
	struct callback_tx_gate tx;
	dssh_channel reserved;
	void (*unblock)(void *);
	void *data;
	unsigned cancels;
	bool watching;
	bool paused;
};

static void
flow_release(void *data)
{
	struct flow_fixture *flow = data;
	if (flow->unblock != NULL)
		flow->unblock(flow->data);
	if (flow->paused)
		dssh_test_thrd_resume_mutex();
	if (flow->reserved != NULL) {
		if (dssh_chan_zc_cancel(flow->reserved) != 0)
			abort();
		flow->reserved = NULL;
		if (flow->cancels == UINT_MAX)
			abort();
		flow->cancels++;
	}
	callback_release_tx(&flow->tx);
}

static void
flow_retire(void *data)
{
	struct flow_fixture *flow = data;
	if (flow->watching) {
		dssh_test_thrd_watch_condition(NULL);
		flow->watching = false;
	}
	if (flow->paused) {
		(void)dssh_test_thrd_pause_mutex_after(NULL, 0);
		dssh_test_thrd_pause_mutex(NULL);
		flow->paused = false;
	}
}

static void
flow_init(struct flow_fixture *flow, struct conn_ctx *ctx,
    void (*unblock)(void *), void *data)
{
	flow->tx = (struct callback_tx_gate){0};
	flow->reserved = NULL;
	flow->unblock = unblock;
	flow->data = data;
	flow->cancels = 0;
	flow->watching = false;
	flow->paused = false;
	channel_workers_init(&flow->workers, ctx);
	flow->workers.release = flow_release;
	flow->workers.retire = flow_retire;
	flow->workers.resource = flow;
}

static void
flow_finish(struct flow_fixture *flow)
{
	/* Nested assertions may already have performed the owning cleanup. */
	if (flow->workers.owner->workers != NULL)
		channel_workers_finish(&flow->workers);
}

static bool
flow_bytes(const uint8_t *data, size_t length, size_t expected, uint8_t byte)
{
	if (expected > 24 || length != expected || (length != 0 && data == NULL))
		return false;
	for (size_t i = 0; i < length; i++) {
		if (data[i] != byte)
			return false;
	}
	return true;
}

#endif
