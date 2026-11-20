/* Private checkpoints and application buffer retirement for storage hosts. */
#ifndef DSSH_TEST_CONN_STORAGE_FIXTURE_H
#define DSSH_TEST_CONN_STORAGE_FIXTURE_H

enum storage_stage {
	ST_TEXT_VIEW, ST_TEXT_DONE, ST_TEXT_PREPARED, ST_TEXT_ALLOCATED, ST_TEXT_PACKET,
	ST_TEXT_STARTED, ST_TEXT_QUEUED, ST_TEXT_BORROWED,
	ST_PARAMS_EMPTY, ST_PARAMS_BUILT, ST_PARAMS_STARTED, ST_PARAMS_ATTEMPT,
	ST_PARAMS_JOINED, ST_PARAMS_COPIED, ST_PARAMS_INPUT,
	ST_OVERFLOW_PREPARED, ST_OVERFLOW_HELD, ST_OVERFLOW_STARTED,
	ST_OVERFLOW_WAITING, ST_OVERFLOW_DONE, ST_OVERFLOW_FINISHED
};
static bool (*storage_stage_control)(enum storage_stage);

static bool
storage_checkpoint(enum storage_stage stage)
{
	return storage_stage_control == NULL || storage_stage_control(stage);
}

struct storage_packet_fixture {
	struct flow_fixture flow;
	uint8_t *packet;
	unsigned frees;
};

static void
storage_packet_retire(void *data)
{
	struct flow_fixture *flow = data;
	struct storage_packet_fixture *fixture = flow->data;
	flow_retire(flow);
	if (fixture->packet != NULL) {
		free(fixture->packet);
		fixture->packet = NULL;
		fixture->frees++;
	}
}

#endif
