#ifndef DSSH_TEST_CHANNEL_ID_CASES_H
#define DSSH_TEST_CHANNEL_ID_CASES_H

struct channel_id_free_probe {
	const void *records[DSSH_RETIRED_CHANNEL_ID_CAP];
	size_t count;
	size_t freed;
};

static void
channel_id_observe_free(const void *ptr, void *data)
{
	struct channel_id_free_probe *probe = data;
	if (ptr == NULL)
		return;
	for (size_t i = 0; i < probe->count; i++) {
		if (probe->records[i] == ptr) {
			probe->records[i] = NULL;
			probe->freed++;
			return;
		}
	}
}

/* The observer clears each live identity before the allocator releases it. */
static bool
channel_id_cleanup_checked(struct conn_ctx *ctx, size_t expected)
{
	struct channel_id_free_probe probe = {0};
	dssh_session sess = ctx->client;
	if (mtx_lock(&sess->channel_mtx) != thrd_success)
		abort();
	struct dssh_channel_id *lists[] = {sess->channel_ids, sess->free_channel_ids};
	bool complete = true;
	for (size_t i = 0; i < sizeof(lists) / sizeof(lists[0]); i++) {
		for (struct dssh_channel_id *id = lists[i]; id != NULL; id = id->next) {
			if (probe.count == DSSH_RETIRED_CHANNEL_ID_CAP) {
				complete = false;
				break;
			}
			probe.records[probe.count++] = id;
		}
	}
	if (mtx_unlock(&sess->channel_mtx) != thrd_success)
		abort();
	dssh_test_alloc_observe_frees(channel_id_observe_free, &probe);
	conn_cleanup(ctx);
	dssh_test_alloc_observe_frees(NULL, NULL);
	return complete && probe.count == expected && probe.freed == expected;
}

static int64_t
channel_id_candidate(dssh_session sess, uint32_t id)
{
	if (mtx_lock(&sess->channel_mtx) != thrd_success)
		abort();
	sess->next_channel_id = id;
	if (mtx_unlock(&sess->channel_mtx) != thrd_success)
		abort();
	return alloc_channel_id(sess);
}

static int
channel_id_lifetime_run(struct conn_ctx *ctx)
{
	struct open_exec_ctx first = {.client = ctx->client, .server = ctx->server,
	    .command = "first-id", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	struct open_exec_ctx marker = {.client = ctx->client, .server = ctx->server,
	    .command = "marker-id", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	ASSERT_EQ(open_exec_channel(ctx, &first), 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_ID_LIVE));
	ASSERT_EQ(open_exec_channel(ctx, &marker), 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_ID_LIVE));
	uint32_t id = first.client_ch->local_id;
	ASSERT_EQ(dssh_chan_close(first.client_ch, -1), 0);
	first.client_ch = NULL;
	bool observed_close = false;
	for (unsigned i = 0; i < 2; i++) {
		ASSERT_EQ(dssh_chan_poll(first.server_ch, DSSH_POLL_EVENT, 5000), DSSH_POLL_EVENT);
		struct dssh_chan_event event;
		ASSERT_EQ(dssh_chan_read_event(first.server_ch, &event), 0);
		observed_close = observed_close || event.type == DSSH_EVENT_CLOSE;
	}
	int64_t before_reply = channel_id_candidate(ctx->client, id);
	ASSERT_EQ(dssh_chan_close(first.server_ch, -1), 0);
	first.server_ch = NULL;
	/* This packet follows the reciprocal CLOSE on the same transport.
	 * Receiving it proves that the earlier CLOSE has been dispatched. */
	uint8_t expected = 47;
	uint8_t received = 0;
	ASSERT_EQ(dssh_chan_write(marker.server_ch, 0, &expected, 1), 1);
	int polled = dssh_chan_poll(marker.client_ch, DSSH_POLL_READ, 5000);
	ASSERT_TRUE(polled >= 0 && (polled & DSSH_POLL_READ) != 0);
	ASSERT_EQ(dssh_chan_read(marker.client_ch, 0, &received, 1), 1);
	int64_t after_reply = channel_id_candidate(ctx->client, id);
	ASSERT_EQ(mtx_lock(&ctx->client->channel_mtx), thrd_success);
	ctx->client->next_channel_id = id;
	if (mtx_unlock(&ctx->client->channel_mtx) != thrd_success)
		abort();
	struct open_exec_ctx reused = {.client = ctx->client, .server = ctx->server,
	    .command = "reused-id", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	int ret = open_exec_channel(ctx, &reused);
	ASSERT_EQ(ret, 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_ID_LIVE));
	bool valid = observed_close && before_reply >= 0 && before_reply != id
	    && after_reply == id && received == expected && ret == 0;
	if (ret == 0) {
		valid = valid && reused.client_ch->local_id == id
		    && dssh_chan_write(reused.client_ch, 0, &expected, 1) == 1;
		polled = valid ? dssh_chan_poll(reused.server_ch, DSSH_POLL_READ, 5000) : 0;
		valid = valid && polled >= 0 && (polled & DSSH_POLL_READ) != 0
		    && dssh_chan_read(reused.server_ch, 0, &received, 1) == 1 && received == expected;
	}
	ASSERT_TRUE(printf("channel_id_lifetime before=%" PRId64 " after=%" PRId64
	    " original=%" PRIu32 " peer_close=%d reused=%d data=%d\n",
	    before_reply, after_reply, id, observed_close, ret == 0, received == expected) >= 0);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

static int
test_channel_id_lifetime(void)
{
	struct conn_ctx ctx;
	if (conn_setup(&ctx) != 0)
		return TEST_FAIL;
	int ret = channel_id_lifetime_run(&ctx);
	if (ret != TEST_PASS) {
		conn_cleanup(&ctx);
		return ret;
	}
	ASSERT_TRUE(channel_id_cleanup_checked(&ctx, 2));
	return TEST_PASS;
}

/* Keep the server synchronous so the fixture controls every OPEN response. */
static int
channel_id_open_without_reply(struct conn_ctx *ctx)
{
	struct dssh_chan_params params;
	int ret = dssh_chan_params_init(&params, DSSH_CHAN_SHELL);
	if (ret != 0)
		return ret;
	params.max_window = 128;
	dssh_channel ch = dssh_chan_open(ctx->client, &params);
	dssh_chan_params_free(&params);
	return ch == NULL && !dssh_session_is_terminated(ctx->client) ? 0 : DSSH_ERROR_INIT;
}

static size_t
channel_id_pending_count(dssh_session sess)
{
	if (mtx_lock(&sess->channel_mtx) != thrd_success)
		abort();
	size_t count = sess->retired_channel_ids;
	if (mtx_unlock(&sess->channel_mtx) != thrd_success)
		abort();
	return count;
}

static bool
channel_id_receive_open(dssh_session peer, uint32_t *id)
{
	uint8_t type;
	uint8_t *payload;
	size_t len;
	if (recv_packet(peer, &type, &payload, &len) != 0 || type != SSH_MSG_CHANNEL_OPEN
	    || len != 24 || DSSH_GET_U32(&payload[1]) != 7
	    || memcmp(&payload[5], "session", 7) != 0)
		return false;
	*id = DSSH_GET_U32(&payload[12]);
	return true;
}

static const uint8_t channel_id_probe_request[] = {
	SSH_MSG_GLOBAL_REQUEST, 0, 0, 0, 2, 'i', 'd', 1
};

static bool
channel_id_receive_probe_reply(dssh_session peer)
{
	uint8_t type;
	uint8_t *payload;
	size_t len;
	return recv_packet(peer, &type, &payload, &len) == 0
	    && type == SSH_MSG_REQUEST_FAILURE && len == 1;
}

/* The reply proves that the demux finished every preceding packet. */
static bool
channel_id_wire_barrier(dssh_session peer)
{
	return send_packet(peer, channel_id_probe_request, sizeof(channel_id_probe_request), NULL) == 0
	    && channel_id_receive_probe_reply(peer);
}

static size_t
channel_id_build_reply(uint8_t reply[17], uint32_t id, bool accept, uint32_t peer_id)
{
	size_t pos = 0;
	reply[pos++] = accept ? SSH_MSG_CHANNEL_OPEN_CONFIRMATION : SSH_MSG_CHANNEL_OPEN_FAILURE;
	DSSH_PUT_U32(id, reply, &pos);
	DSSH_PUT_U32(accept ? peer_id : 4, reply, &pos);
	DSSH_PUT_U32(0, reply, &pos);
	DSSH_PUT_U32(accept ? 32768 : 0, reply, &pos);
	return pos;
}

struct channel_id_reply_job {
	dssh_session peer;
	const uint8_t *reply;
	size_t len;
};

static int
channel_id_send_reply_and_probe(void *arg)
{
	struct channel_id_reply_job *job = arg;
	int ret = send_packet(job->peer, job->reply, job->len, NULL);
	if (ret != 0)
		return ret;
	return send_packet(job->peer, channel_id_probe_request, sizeof(channel_id_probe_request), NULL);
}

static bool
channel_id_queue_reply(struct conn_ctx *ctx, const uint8_t *reply, size_t len)
{
	if (tx_acquire(ctx->client, SSH_MSG_IGNORE) != 0)
		return false;
	struct channel_id_reply_job job = {.peer = ctx->server, .reply = reply, .len = len};
	struct flow_fixture flow;
	flow_init(&flow, ctx, NULL, NULL);
	flow.tx.session = ctx->client;
	flow.tx.held = true;
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, channel_id_send_reply_and_probe, &job), 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_ID_QUEUE_STARTED));
	channel_worker_join(&flow.workers, 0);
	bool queued = flow.workers.results[0] == 0
	    && dssh_test_wait_bool(&ctx->client->trans.global_reply_slot.ready, true)
	    && atomic_load(&ctx->client->trans.open_fail_slot.ready);
	ASSERT_TRUE(queued);
	ASSERT_TRUE(lifecycle_checkpoint(LC_ID_QUEUED));
	callback_release_tx(&flow.tx);
	flow_finish(&flow);
	return queued && flow.tx.result == 0 && channel_id_receive_probe_reply(ctx->server);
}

static bool
channel_id_acknowledge_close(struct conn_ctx *ctx, uint32_t local, uint32_t remote)
{
	uint8_t type;
	uint8_t *payload;
	size_t len;
	if (recv_packet(ctx->server, &type, &payload, &len) != 0
	    || type != SSH_MSG_CHANNEL_CLOSE || len != 5 || DSSH_GET_U32(&payload[1]) != remote)
		return false;
	int64_t candidate = channel_id_candidate(ctx->client, local);
	if (candidate < 0 || candidate == local)
		return false;
	uint8_t close[5];
	size_t pos = 0;
	close[pos++] = SSH_MSG_CHANNEL_CLOSE;
	DSSH_PUT_U32(local, close, &pos);
	return send_packet(ctx->server, close, pos, NULL) == 0;
}

static int
channel_id_late_reply_run(struct conn_ctx *ctx, bool accept, bool queue)
{
	ASSERT_EQ(dssh_session_set_timeout(ctx->client, 10), 0);
	ASSERT_EQ(dssh_session_start(ctx->client), 0);
	if (!accept) {
		/* The packet engine refuses an OPEN before changing stream state. */
		uint_fast32_t saved = atomic_exchange(&ctx->client->trans.tx_since_rekey, UINT_FAST32_MAX);
		uint32_t seq = ctx->client->trans.tx_seq;
		int ret = channel_id_open_without_reply(ctx);
		atomic_store(&ctx->client->trans.tx_since_rekey, saved);
		ASSERT_EQ(ret, 0);
		ASSERT_EQ_U(ctx->client->trans.tx_seq, seq);
		ASSERT_EQ_U(channel_id_pending_count(ctx->client), 0);
		ASSERT_EQ(channel_id_candidate(ctx->client, 0), 0);
	}
	ASSERT_EQ(channel_id_open_without_reply(ctx), 0);
	ASSERT_EQ_U(channel_id_pending_count(ctx->client), 1);
	uint32_t id;
	ASSERT_TRUE(channel_id_receive_open(ctx->server, &id));
	int64_t candidate = channel_id_candidate(ctx->client, id);
	ASSERT_TRUE(candidate >= 0 && candidate != id);
	uint8_t reply[17];
	uint32_t peer_id = UINT32_MAX;
	size_t len = channel_id_build_reply(reply, id, accept, peer_id);
	/* A truncated response must not release the pending number. */
	ASSERT_EQ(send_packet(ctx->server, reply, len - 1, NULL), 0);
	ASSERT_TRUE(channel_id_wire_barrier(ctx->server));
	candidate = channel_id_candidate(ctx->client, id);
	ASSERT_TRUE(candidate >= 0 && candidate != id);
	if (queue) {
		ASSERT_TRUE(channel_id_queue_reply(ctx, reply, len));
	}
	else {
		ASSERT_EQ(send_packet(ctx->server, reply, len, NULL), 0);
	}
	if (accept)
		ASSERT_TRUE(channel_id_acknowledge_close(ctx, id, peer_id));
	ASSERT_TRUE(channel_id_wire_barrier(ctx->server));
	ASSERT_EQ(channel_id_candidate(ctx->client, id), id);
	ASSERT_EQ_U(channel_id_pending_count(ctx->client), 0);
	ASSERT_FALSE(dssh_session_is_terminated(ctx->client));
	return TEST_PASS;
}

struct channel_id_open_job {
	struct conn_ctx *ctx;
	atomic_bool armed;
	atomic_bool cancelled;
	atomic_bool *detaching;
};

static _Thread_local struct channel_id_open_job *channel_id_open_job;

/* Skip the packet engine's final drain after transmitting OPEN. The next
 * registry acquisition is detachment after the wait expires. */
static int
channel_id_pause_detach(dssh_session sess)
{
	struct channel_id_open_job *job = channel_id_open_job;
	job->detaching = dssh_test_thrd_pause_mutex_after(&sess->channel_mtx, 1);
	atomic_store(&job->armed, true);
	if (atomic_load(&job->cancelled))
		dssh_test_thrd_resume_mutex();
	return 0;
}

static int
channel_id_open_worker(void *arg)
{
	struct channel_id_open_job *job = arg;
	channel_id_open_job = job;
	window_tx_return_test_hook = channel_id_pause_detach;
	int ret = channel_id_open_without_reply(job->ctx);
	window_tx_return_test_hook = NULL;
	channel_id_open_job = NULL;
	(void)dssh_test_thrd_pause_mutex_after(NULL, 0);
	return ret;
}

static void
channel_id_release_open(void *data)
{
	struct channel_id_open_job *job = data;
	atomic_store(&job->cancelled, true);
	dssh_test_thrd_resume_mutex();
}

static int
channel_id_confirm_before_detach(struct conn_ctx *ctx)
{
	ASSERT_EQ(dssh_session_set_timeout(ctx->client, 10), 0);
	ASSERT_EQ(dssh_session_start(ctx->client), 0);
	struct channel_id_open_job job = {.ctx = ctx};
	atomic_init(&job.armed, false);
	atomic_init(&job.cancelled, false);
	struct flow_fixture flow;
	flow_init(&flow, ctx, channel_id_release_open, &job);
	flow.paused = true;
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, channel_id_open_worker, &job), 0);
	ASSERT_TRUE(lifecycle_checkpoint(LC_ID_OPEN_STARTED));
	bool valid = dssh_test_wait_bool(&job.armed, true)
	    && dssh_test_wait_bool(job.detaching, true);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(lifecycle_checkpoint(LC_ID_DETACH_PAUSED));
	uint32_t id = 0;
	uint32_t peer_id = UINT32_MAX;
	if (valid) {
		dssh_session client = ctx->client;
		if (mtx_lock(&client->channel_mtx) != thrd_success)
			abort();
		valid = client->channel_count == 1 && atomic_load(&client->channels[0]->closing)
		    && client->retired_channel_ids == 0 && !atomic_load(&client->trans.tx_owner_active);
		if (mtx_unlock(&client->channel_mtx) != thrd_success)
			abort();
		valid = valid && channel_id_receive_open(ctx->server, &id);
	}
	if (valid) {
		uint8_t reply[17];
		size_t len = channel_id_build_reply(reply, id, true, peer_id);
		valid = send_packet(ctx->server, reply, len, NULL) == 0
		    && channel_id_wire_barrier(ctx->server);
	}
	channel_id_release_open(&job);
	flow_finish(&flow);
	ASSERT_TRUE(valid);
	ASSERT_EQ(flow.workers.results[0], 0);
	ASSERT_EQ_U(channel_id_pending_count(ctx->client), 1);
	ASSERT_TRUE(channel_id_acknowledge_close(ctx, id, peer_id));
	ASSERT_TRUE(channel_id_wire_barrier(ctx->server));
	ASSERT_EQ(channel_id_candidate(ctx->client, id), id);
	ASSERT_EQ_U(channel_id_pending_count(ctx->client), 0);
	return TEST_PASS;
}

static int
test_channel_id_late_reply(void)
{
	for (unsigned mode = 0; mode < 4; mode++) {
		struct conn_ctx ctx;
		if (conn_setup_mode(&ctx, false) != 0)
			return TEST_FAIL;
		int ret = mode == 3 ? channel_id_confirm_before_detach(&ctx)
		    : channel_id_late_reply_run(&ctx, mode != 0, mode == 2);
		if (ret != TEST_PASS) {
			conn_cleanup(&ctx);
			return ret;
		}
		ASSERT_TRUE(channel_id_cleanup_checked(&ctx, 1));
	}
	return TEST_PASS;
}

static int
channel_id_backpressure_run(struct conn_ctx *ctx)
{
	ASSERT_EQ(dssh_session_set_timeout(ctx->client, 1), 0);
	ASSERT_EQ(dssh_session_start(ctx->client), 0);
	uint32_t first_id = 0;
	for (size_t i = 0; i < DSSH_RETIRED_CHANNEL_ID_CAP; i++) {
		ASSERT_EQ(channel_id_open_without_reply(ctx), 0);
		ASSERT_EQ_U(channel_id_pending_count(ctx->client), i + 1);
		uint32_t id;
		ASSERT_TRUE(channel_id_receive_open(ctx->server, &id));
		if (i == 0)
			first_id = id;
	}
	ASSERT_TRUE(lifecycle_checkpoint(LC_ID_PRESSURE));
	uint32_t seq = ctx->client->trans.tx_seq;
	struct dssh_chan_params params;
	ASSERT_EQ(dssh_chan_params_init(&params, DSSH_CHAN_SHELL), 0);
	params.max_window = 128;
	dssh_test_alloc_fail_after(0);
	dssh_channel refused = dssh_chan_open(ctx->client, &params);
	int calls = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	dssh_chan_params_free(&params);
	ASSERT_NULL(refused);
	ASSERT_FALSE(dssh_session_is_terminated(ctx->client));
	ASSERT_EQ(calls, 0);
	ASSERT_EQ_U(channel_id_pending_count(ctx->client), DSSH_RETIRED_CHANNEL_ID_CAP);
	ASSERT_EQ_U(ctx->client->trans.tx_seq, seq);
	uint8_t reply[17];
	size_t len = channel_id_build_reply(reply, first_id, false, 0);
	ASSERT_EQ(send_packet(ctx->server, reply, len, NULL), 0);
	ASSERT_TRUE(channel_id_wire_barrier(ctx->server));
	ASSERT_EQ_U(channel_id_pending_count(ctx->client), DSSH_RETIRED_CHANNEL_ID_CAP - 1);
	ASSERT_TRUE(dssh_test_wait_bool(&ctx->client->trans.tx_owner_active, false));
	ASSERT_EQ(channel_id_candidate(ctx->client, first_id), first_id);
	ASSERT_EQ(mtx_lock(&ctx->client->channel_mtx), thrd_success);
	ctx->client->next_channel_id = first_id;
	if (mtx_unlock(&ctx->client->channel_mtx) != thrd_success)
		abort();
	ASSERT_EQ(channel_id_open_without_reply(ctx), 0);
	ASSERT_EQ_U(channel_id_pending_count(ctx->client), DSSH_RETIRED_CHANNEL_ID_CAP);
	uint32_t reopened;
	ASSERT_TRUE(channel_id_receive_open(ctx->server, &reopened));
	ASSERT_EQ_U(reopened, first_id);
	return TEST_PASS;
}

static int
test_channel_id_backpressure(void)
{
	struct conn_ctx ctx;
	if (conn_setup_mode(&ctx, false) != 0)
		return TEST_FAIL;
	int ret = channel_id_backpressure_run(&ctx);
	if (ret != TEST_PASS) {
		conn_cleanup(&ctx);
		return ret;
	}
	ASSERT_TRUE(channel_id_cleanup_checked(&ctx, DSSH_RETIRED_CHANNEL_ID_CAP));
	return TEST_PASS;
}

#endif
