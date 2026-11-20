#ifndef DSSH_TEST_TRANSPORT_WIRE_TRACE_CASES_H
#define DSSH_TEST_TRANSPORT_WIRE_TRACE_CASES_H

struct wire_trace_probe {
	dssh_session sess;
	const char *expected;
	unsigned calls;
	bool valid;
};

static dssh_log_action
wire_trace_capture(const struct dssh_log_record *record, void *opaque)
{
	struct wire_trace_probe *probe = opaque;
	probe->calls++;
	probe->valid = probe->valid && record != NULL
	    && record->level == DSSH_LOG_DEBUG && record->source == DSSH_LOG_SOURCE_LIBRARY
	    && record->error_code == DSSH_ERROR_NONE && record->ssh_reason_code == 0
	    && !record->always_display && !record->truncated && record->language_len == 0
	    && record->message != NULL && record->message_len == strlen(probe->expected);
	if (probe->valid)
		probe->valid = memcmp(record->message, probe->expected, record->message_len) == 0;
	mtx_t *locks[] = {&probe->sess->mtx, &probe->sess->trans.tx_mtx,
	    &probe->sess->trans.tx_queue_mtx, &probe->sess->log_forward->mtx};
	for (size_t i = 0; i < sizeof(locks) / sizeof(locks[0]); i++) {
		if (mtx_trylock(locks[i]) != thrd_success) {
			probe->valid = false;
			continue;
		}
		if (mtx_unlock(locks[i]) != thrd_success)
			abort();
	}
	/* Packet metadata stays local even when the application requests debug
	 * forwarding. Ordinary forwardable diagnostic controls are separate. */
	return DSSH_LOG_SEND_DEBUG_ALWAYS_DISPLAY;
}

static int
test_transport_wire_trace(void)
{
	struct handshake_ctx ctx;
	if (handshake_setup(&ctx) != 0) {
		handshake_cleanup(&ctx);
		return TEST_FAIL;
	}
	struct wire_trace_probe tx = {.sess = ctx.client,
	    .expected = "[dssh-wire] tx msg_type=5 len=17", .valid = true};
	struct wire_trace_probe rx = {.sess = ctx.server,
	    .expected = "[dssh-wire] rx msg_type=5 len=17", .valid = true};
	bool valid = dssh_session_set_log_cb(ctx.client, wire_trace_capture, &tx) == 0
	    && dssh_session_set_log_cb(ctx.server, wire_trace_capture, &rx) == 0;
	unsigned unevaluated = 0;
	DSSH_TRACE_W(NULL, "suppressed=%u", ++unevaluated);
	valid = valid && unevaluated == 0;
	const uint8_t request[] = {SSH_MSG_SERVICE_REQUEST, 0, 0, 0, 12,
	    's', 's', 'h', '-', 'u', 's', 'e', 'r', 'a', 'u', 't', 'h'};
	for (unsigned enabled = 0; enabled < 2 && valid; enabled++) {
		if (enabled != 0)
			valid = dssh_session_set_log_level(ctx.client, DSSH_LOG_DEBUG) == 0
			    && dssh_session_set_log_level(ctx.server, DSSH_LOG_DEBUG) == 0;
		uint32_t before = ctx.client->trans.tx_seq;
		int ret = valid ? send_packet(ctx.client, request, sizeof(request), NULL) : -999;
		uint8_t type = 0;
		uint8_t *payload = NULL;
		size_t length = 0;
		if (ret == 0)
			ret = recv_packet(ctx.server, &type, &payload, &length);
		unsigned expected = 0;
#ifdef DSSH_TRACE_WIRE
		expected = enabled;
#endif
		valid = valid && ret == 0 && type == SSH_MSG_SERVICE_REQUEST
		    && length == sizeof(request) && memcmp(payload, request, sizeof(request)) == 0
		    && ctx.client->trans.tx_seq - before == 1
		    && tx.valid && rx.valid && tx.calls == expected && rx.calls == expected
		    && ctx.client->log_forward->count == 0 && ctx.server->log_forward->count == 0;
	}
	/* Observers borrow stack storage: detach before lifecycle diagnostics. */
	valid = dssh_session_set_log_cb(ctx.client, NULL, NULL) == 0 && valid;
	valid = dssh_session_set_log_cb(ctx.server, NULL, NULL) == 0 && valid;
	handshake_cleanup(&ctx);
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#endif
