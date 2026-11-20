/* Real public ZC handles. The fill/close/send-or-cancel worker always finishes
 * before the owning frame cleans up either session or its callback storage. */
struct close_reservation_open {
	struct open_exec_ctx channels;
	struct dssh_chan_accept_cbs callbacks;
	bool client_zc;
};

static int
close_reservation_client(void *opaque)
{
	struct close_reservation_open *job = opaque;
	struct dssh_chan_params params;
	int ret = dssh_chan_params_init(&params, DSSH_CHAN_EXEC);
	if (ret != 0)
		return ret;
	ret = dssh_chan_params_set_command(&params, job->channels.command);
	if (ret == 0)
		ret = dssh_chan_params_set_pty(&params, false);
	if (ret == 0) {
		if (job->client_zc)
			job->channels.client_ch = dssh_chan_zc_open(job->channels.client, &params, event_zc_data, NULL);
		else
			job->channels.client_ch = dssh_chan_open(job->channels.client, &params);
		if (job->channels.client_ch == NULL)
			ret = DSSH_ERROR_INIT;
	}
	dssh_chan_params_free(&params);
	return ret == 0 ? wait_stream_writable(job->channels.client_ch) : ret;
}

static bool
close_reservation_pair(struct conn_ctx *ctx, struct close_reservation_open *job, unsigned role)
{
	job->callbacks = accept_cbs_all;
	job->client_zc = role == 0;
	if (role != 0)
		job->callbacks.exec = event_accept_zc;
	job->channels = (struct open_exec_ctx){.client = ctx->client, .server = ctx->server,
	    .command = "close-reservation", .cbs = &job->callbacks, .accept_timeout = 30000};
	struct channel_workers workers;
	channel_workers_init(&workers, ctx);
	if (channel_worker_start(&workers, 0, close_reservation_client, job) != 0
	    || channel_worker_start(&workers, 1, server_accept_exec_thread, &job->channels) != 0) {
		channel_workers_finish(&workers);
		return false;
	}
	channel_worker_join(&workers, 0);
	if (workers.results[0] != 0 || job->channels.client_ch == NULL)
		channel_workers_cancel(&workers);
	channel_workers_finish(&workers);
	return workers.results[0] == 0 && workers.results[1] == 0
	    && job->channels.client_ch != NULL && job->channels.server_ch != NULL;
}

static bool
close_reservation_credit(dssh_channel channel)
{
	struct timespec start;
	if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
		return false;
	while (atomic_load(&channel->remote_window) == 0) {
		if (dssh_session_is_terminated(channel->sess) || !dssh_test_wait_tick(&start))
			return false;
	}
	return true;
}

static const uint8_t close_reservation_bytes[] = {0, 13, 255, 1, 2, 3, 4};
struct close_reservation_job {
	dssh_channel owner, target;
	bool same, send, valid;
	int rejected, released, closed;
	unsigned frees, cancels;
	atomic_bool acquired, reserved, resume, rejected_ready, finish, cancelled, done;
};

static void
close_reservation_release(void *opaque)
{
	struct close_reservation_job *job = opaque;
	atomic_store(&job->cancelled, true);
	atomic_store(&job->resume, true);
	atomic_store(&job->finish, true);
}

/* A ZC reservation must be cancelled by its actual owning thread. */
static void
close_reservation_cancel(struct close_reservation_job *job)
{
	if (job->cancels != 0 || dssh_chan_zc_cancel(job->owner) != 0)
		abort();
	job->cancels++;
	atomic_store(&job->reserved, false);
}

static int
close_reservation_worker(void *opaque)
{
	struct close_reservation_job *job = opaque;
	dssh_session session = job->owner->sess;
	uint8_t *buffer = NULL;
	size_t capacity = 0;
	int ret = dssh_chan_zc_getbuf(job->owner, 0, &buffer, &capacity);
	if (ret != 0)
		goto finished;
	atomic_store(&job->acquired, true);
	if (buffer == NULL || capacity < sizeof(close_reservation_bytes)) {
		ret = DSSH_ERROR_TOOLONG;
		goto cancel;
	}
	memcpy(buffer, close_reservation_bytes, sizeof(close_reservation_bytes));
	uint_least32_t window = atomic_load(&job->owner->remote_window);
	atomic_store(&job->reserved, true);
	if (!wire_wait(session, &job->resume) || atomic_load(&job->cancelled)) {
		ret = DSSH_ERROR_TERMINATED;
		goto cancel;
	}
	struct close_outcome_lifetime watch = {.handle = job->target};
	dssh_test_alloc_observe_frees(close_outcome_observe_free, &watch);
	job->rejected = dssh_chan_close(job->target, INT64_MAX);
	bool valid = job->rejected == DSSH_ERROR_INVALID && watch.frees == 0
	    && tx_owner_is_current(session) && !dssh_session_is_terminated(session);
	if (!valid) {
		dssh_test_alloc_observe_frees(NULL, NULL);
		/* A consumed reservation owner or lost engine is indeterminate;
		 * never dereference a possibly consumed handle to attempt recovery. */
		if ((job->same && watch.frees != 0) || !tx_owner_is_current(session))
			abort();
		ret = DSSH_ERROR_INIT;
		goto cancel;
	}
	valid = memcmp(buffer, close_reservation_bytes, sizeof(close_reservation_bytes)) == 0
	    && atomic_load(&job->owner->remote_window) == window
	    && dssh_chan_get_command(job->target) != NULL;
	if (!valid) {
		dssh_test_alloc_observe_frees(NULL, NULL);
		ret = DSSH_ERROR_INIT;
		goto cancel;
	}
	atomic_store(&job->rejected_ready, true);
	if (!wire_wait(session, &job->finish) || atomic_load(&job->cancelled)) {
		dssh_test_alloc_observe_frees(NULL, NULL);
		ret = DSSH_ERROR_TERMINATED;
		goto cancel;
	}
	job->released = job->send ? dssh_chan_zc_send(job->owner, sizeof(close_reservation_bytes))
	    : dssh_chan_zc_cancel(job->owner);
	if (!job->send)
		job->cancels++;
	atomic_store(&job->reserved, false);
	if (tx_owner_is_current(session))
		abort();
	if (job->released != 0) {
		dssh_test_alloc_observe_frees(NULL, NULL);
		ret = job->released;
		goto finished;
	}
	/* Once the reservation ends, the out-of-range value omits exit status
	 * and close consumes the valid target on every return. */
	job->closed = dssh_chan_close(job->target, INT64_MAX);
	job->target = NULL;
	if (job->same)
		job->owner = NULL;
	job->frees = watch.frees;
	valid = valid && job->closed == 0 && watch.frees == 1 && watch.handle == NULL;
	dssh_test_alloc_observe_frees(NULL, NULL);
	if (!job->same) {
		int result = dssh_chan_close(job->owner, -1);
		job->owner = NULL;
		valid = valid && result == 0;
	}
	job->valid = valid && !dssh_session_is_terminated(session);
	ret = job->valid ? 0 : DSSH_ERROR_INIT;
	goto finished;

cancel:
	close_reservation_cancel(job);
finished:
	if (tx_owner_is_current(session))
		abort();
	atomic_store(&job->done, true);
	return ret;
}

static int
close_reservation_case(unsigned role, unsigned gather, unsigned same, unsigned send)
{
	struct conn_ctx ctx;
	window_test_gather = gather != 0;
	int setup = conn_setup(&ctx);
	window_test_gather = false;
	ASSERT_EQ(setup, 0);
	ASSERT_TRUE(half_close_algorithms(&ctx));
	struct close_reservation_open owner = {0};
	ASSERT_TRUE(close_reservation_pair(&ctx, &owner, role));
	struct open_exec_ctx other = {.client = ctx.client, .server = ctx.server,
	    .command = "close-other", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	ASSERT_EQ(open_exec_channel(&ctx, &other), 0);
	ASSERT_TRUE(consume_checkpoint(CO_RES_PAIRS));
	dssh_channel owner_channel = role == 0 ? owner.channels.client_ch : owner.channels.server_ch;
	dssh_channel peer = role == 0 ? owner.channels.server_ch : owner.channels.client_ch;
	dssh_channel target = same != 0 ? owner_channel : role == 0 ? other.client_ch : other.server_ch;
	ASSERT_TRUE(close_reservation_credit(owner_channel));
	struct close_reservation_job job = {.owner = owner_channel, .target = target,
	    .same = same != 0, .send = send != 0};
	atomic_init(&job.acquired, false);
	atomic_init(&job.reserved, false);
	atomic_init(&job.resume, false);
	atomic_init(&job.rejected_ready, false);
	atomic_init(&job.finish, false);
	atomic_init(&job.cancelled, false);
	atomic_init(&job.done, false);
	struct flow_fixture flow;
	flow_init(&flow, &ctx, close_reservation_release, &job);
	ASSERT_TRUE(consume_checkpoint(CO_RES_PREPARED));
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, close_reservation_worker, &job), 0);
	ASSERT_TRUE(consume_checkpoint(CO_RES_STARTED));
	ASSERT_TRUE(wire_wait(owner_channel->sess, &job.reserved));
	ASSERT_TRUE(consume_checkpoint(CO_RES_HELD));
	atomic_store(&job.resume, true);
	ASSERT_TRUE(wire_wait(owner_channel->sess, &job.rejected_ready));
	ASSERT_TRUE(consume_checkpoint(CO_RES_REJECTED));
	atomic_store(&job.finish, true);
	flow_finish(&flow);
	ASSERT_EQ(flow.workers.results[0], 0);
	ASSERT_TRUE(job.valid && atomic_load(&job.done));
	ASSERT_NULL(job.owner);
	ASSERT_NULL(job.target);
	if (send != 0) {
		uint8_t received[sizeof(close_reservation_bytes)];
		int polled = dssh_chan_poll(peer, DSSH_POLL_READ, 5000);
		ASSERT_TRUE(polled >= 0 && (polled & DSSH_POLL_READ) != 0);
		ASSERT_EQ(dssh_chan_read(peer, 0, received, sizeof(received)), sizeof(received));
		ASSERT_MEM_EQ(received, close_reservation_bytes, sizeof(received));
	}
	ASSERT_TRUE(consume_checkpoint(CO_RES_CHECKED));
	ASSERT_TRUE(printf("close_reservation role=%u gather=%u same=%u send=%u rejected=%d released=%d closed=%d frees=%u valid=1\n",
	    role, gather, same, send, job.rejected, job.released, job.closed, job.frees) >= 0);
	conn_cleanup(&ctx);
	return TEST_PASS;
}

static int
test_close_reservation(void)
{
	unsigned cases = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned gather = 0; gather < 2; gather++) {
			for (unsigned same = 0; same < 2; same++) {
				for (unsigned send = 0; send < 2; send++) {
					if (close_reservation_case(role, gather, same, send) != TEST_PASS)
						return TEST_FAIL;
					cases++;
				}
			}
		}
	}
	ASSERT_TRUE(printf("close_reservation cases=%u failures=0\n", cases) >= 0);
	ASSERT_EQ_U(cases, 16);
	return TEST_PASS;
}
