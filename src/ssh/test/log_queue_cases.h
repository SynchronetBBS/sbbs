#ifndef DSSH_TEST_LOG_QUEUE_CASES_H
#define DSSH_TEST_LOG_QUEUE_CASES_H

struct log_queue_probe {
	const uint8_t *message;
	unsigned local;
	unsigned peer;
	bool valid;
};

static dssh_log_action
log_queue_capture(const struct dssh_log_record *record, void *opaque)
{
	struct log_queue_probe *probe = opaque;
	bool peer = record->source == DSSH_LOG_SOURCE_PEER_DEBUG;
	if (!peer && (record->source != DSSH_LOG_SOURCE_LIBRARY || record->level != DSSH_LOG_ERROR))
		return DSSH_LOG_LOCAL_ONLY;
	if (peer)
		probe->peer++;
	else
		probe->local++;
	probe->valid = probe->valid && record->message != NULL
	    && record->message_len == DSSH_LOG_MESSAGE_MAX && record->language_len == 0
	    && record->error_code == DSSH_ERROR_NONE && record->ssh_reason_code == 0
	    && record->truncated == !peer && record->always_display == peer;
	if (probe->valid)
		probe->valid = memcmp(record->message, probe->message, DSSH_LOG_MESSAGE_MAX) == 0;
	return DSSH_LOG_SEND_DEBUG_ALWAYS_DISPLAY;
}

struct log_queue_holder {
	mtx_t *mutex;
	atomic_bool ready;
	atomic_bool release;
	int acquired;
	int released;
};

static int
log_queue_hold(void *opaque)
{
	struct log_queue_holder *holder = opaque;
	holder->acquired = mtx_lock(holder->mutex);
	atomic_store_explicit(&holder->ready, true, memory_order_release);
	if (holder->acquired == thrd_success) {
		/* Short atomic handoff; no callback, I/O, wait or yield while the
		 * real queue mutex belongs to this worker. */
		while (!atomic_load_explicit(&holder->release, memory_order_acquire)) {
		}
		holder->released = mtx_unlock(holder->mutex);
	}
	return 0;
}

static bool
log_queue_retained(struct dssh_log_forward_queue *queue, const uint8_t *message)
{
	return queue->head == 0 && queue->count == 1
	    && queue->entries[0].always_display
	    && queue->entries[0].message_len == DSSH_LOG_MESSAGE_MAX
	    && memcmp(queue->entries[0].message, message, DSSH_LOG_MESSAGE_MAX) == 0;
}

static bool
log_queue_case(bool drain, bool inject)
{
	struct handshake_ctx ctx;
	bool valid = handshake_setup(&ctx) == 0;
	uint8_t message[DSSH_LOG_MESSAGE_MAX + 17];
	memset(message, 'Q', sizeof(message));
	message[0] = 'A';
	message[DSSH_LOG_MESSAGE_MAX - 1] = 'Z';
	struct log_queue_probe probe = {.message = message, .valid = true};
	if (valid)
		valid = dssh_session_set_log_cb(ctx.client, log_queue_capture, &probe) == 0
		    && dssh_session_set_log_cb(ctx.server, log_queue_capture, &probe) == 0
		    && dssh_session_set_log_level(ctx.server, DSSH_LOG_DEBUG) == 0;
	if (valid) {
		dssh_log_emit(ctx.client, DSSH_LOG_ERROR, DSSH_LOG_SOURCE_LIBRARY,
		    DSSH_ERROR_NONE, 0, false, false, message, sizeof(message), NULL, 0);
		valid = probe.valid && probe.local == 1 && probe.peer == 0
		    && log_queue_retained(ctx.client->log_forward, message);
	}
	bool engine = false;
	if (valid && drain) {
		engine = tx_acquire(ctx.client, SSH_MSG_DEBUG) == 0;
		valid = engine;
	}
	struct log_queue_holder holder = {0};
	atomic_init(&holder.ready, false);
	atomic_init(&holder.release, false);
	thrd_t worker;
	bool created = false;
	if (valid && !inject) {
		holder.mutex = &ctx.client->log_forward->mtx;
		created = fixture_thread_create(&worker, log_queue_hold, &holder) == thrd_success;
		valid = created;
		if (created) {
			while (!atomic_load_explicit(&holder.ready, memory_order_acquire))
				thrd_yield();
			valid = holder.acquired == thrd_success;
		}
	}
	struct dssh_test_trylock_stats refused = {0};
	uint32_t sequence = valid ? ctx.client->trans.tx_seq : 0;
	int ret = 0;
	if (valid) {
		dssh_test_trylock_begin(&ctx.client->log_forward->mtx, inject);
		if (drain)
			ret = drain_log_forwards(ctx.client);
		else
			dssh_log_emit(ctx.client, DSSH_LOG_ERROR, DSSH_LOG_SOURCE_LIBRARY,
			    DSSH_ERROR_NONE, 0, false, false, message, sizeof(message), NULL, 0);
		refused = dssh_test_trylock_end();
	}
	if (created) {
		atomic_store_explicit(&holder.release, true, memory_order_release);
		int result;
		if (thrd_join(worker, &result) != thrd_success)
			abort();
		valid = valid && result == 0 && holder.released == thrd_success;
	}
	if (engine)
		valid = tx_unlock_with_result(ctx.client, ret) == 0 && valid;
	if (valid)
		valid = refused.attempts == 1 && refused.native_calls == (inject ? 0U : 1U)
		    && refused.injected == inject
		    && refused.last_result == (inject ? thrd_error : thrd_busy)
		    && dssh_test_mtx_rejected_unlocks() == 0 && ret == 0
		    && !dssh_session_is_terminated(ctx.client)
		    && ctx.client->trans.tx_seq == sequence && probe.valid
		    && probe.local == (drain ? 1U : 2U) && probe.peer == 0
		    && log_queue_retained(ctx.client->log_forward, message);
	struct dssh_test_trylock_stats resumed = {0};
	if (valid) {
		static const uint8_t request[] = {SSH_MSG_SERVICE_REQUEST, 0, 0, 0, 12,
		    's', 's', 'h', '-', 'u', 's', 'e', 'r', 'a', 'u', 't', 'h'};
		dssh_test_trylock_begin(&ctx.client->log_forward->mtx, false);
		ret = send_packet(ctx.client, request, sizeof(request), NULL);
		resumed = dssh_test_trylock_end();
		uint8_t type = 0;
		uint8_t *payload = NULL;
		size_t length = 0;
		if (ret == 0)
			ret = recv_packet(ctx.server, &type, &payload, &length);
		valid = ret == 0 && type == SSH_MSG_SERVICE_REQUEST
		    && length == sizeof(request) && memcmp(payload, request, sizeof(request)) == 0
		    && resumed.attempts == 2 && resumed.native_calls == 2
		    && !resumed.injected && resumed.last_result == thrd_success
		    && dssh_test_mtx_rejected_unlocks() == 0
		    && probe.valid && probe.peer == 1 && ctx.client->log_forward->count == 0
		    && ctx.server->log_forward->count == 0
		    && ctx.client->trans.tx_seq - sequence == 2;
	}
	/* Both handshake workers and the optional holder have joined. Keep probe
	 * storage alive until all session callbacks and native resources retire. */
	handshake_cleanup(&ctx);
	int printed = printf("log_queue drain=%d inject=%d attempts=%u native=%u result=%d resumed=%u peer=%u valid=%d\n",
	    drain, inject, refused.attempts, refused.native_calls, refused.last_result,
	    resumed.native_calls, probe.peer, valid);
	return printed > 0 && valid;
}

static int
test_log_queue_outcomes(void)
{
	for (unsigned mode = 0; mode < 4; mode++) {
		ASSERT_TRUE(log_queue_case(mode >= 2, mode % 2 != 0));
	}
	return TEST_PASS;
}

#endif
