/* Private case checkpoints; no public channel contract is declared here. */
#ifndef DSSH_TEST_CONN_CONSUME_FIXTURE_H
#define DSSH_TEST_CONN_CONSUME_FIXTURE_H

enum consume_stage {
	CO_OPEN_STARTED, CO_CLOSE_LIVE, CO_CLOSE_LIMIT, CO_CLOSE_PREPARED,
	CO_CLOSE_CONSUMED, CO_CLOSE_CHECKED,
	CO_RES_PAIRS, CO_RES_PREPARED, CO_RES_STARTED, CO_RES_HELD,
	CO_RES_REJECTED, CO_RES_CHECKED,
	CO_MODE_VALIDATED, CO_MODE_EMPTY, CO_MODE_BUILT, CO_MODE_FIRST,
	CO_MODE_STARTED, CO_MODE_JOINED, CO_MODE_COPY, CO_MODE_DATA,
	CO_MODE_WIRE, CO_MODE_DONE, CO_MODE_CAPTURE
};
static bool (*consume_stage_control)(enum consume_stage);
struct close_outcome_lifetime;
static void (*consume_close_retirement_observer)(const struct conn_ctx *,
    const struct close_outcome_lifetime *);

static bool
consume_checkpoint(enum consume_stage stage)
{
	return consume_stage_control == NULL || consume_stage_control(stage);
}

struct consume_params_fixture {
	struct flow_fixture flow;
	struct dssh_chan_params params;
	bool retired;
};

static void
consume_params_retire(void *data)
{
	struct flow_fixture *flow = data;
	struct consume_params_fixture *fixture = flow->data;
	flow_retire(flow);
	dssh_test_alloc_reset();
	dssh_test_ossl_reset();
	dssh_chan_params_free(&fixture->params);
	fixture->retired = true;
}

static void
consume_params_own(struct consume_params_fixture *fixture, struct conn_ctx *ctx)
{
	flow_init(&fixture->flow, ctx, NULL, fixture);
	fixture->flow.workers.retire = consume_params_retire;
}

#endif
