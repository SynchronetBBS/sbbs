/* Documented termination restrictions, real public channels, nested callback
 * scopes and a pre-free observer. Never retry a possibly consumed handle. */
struct close_termination_state {
	dssh_session session, other;
	dssh_channel channel;
	struct close_termination_state *nested;
	unsigned calls;
	int closed;
	bool outer_active, valid;
};

static void
close_termination_callback(dssh_session session, void *opaque)
{
	struct close_termination_state *state = opaque;
	state->calls++;
	bool valid = session == state->session && dssh_session_is_terminated(session)
	    && session_termination_callback_active(session)
	    && session_termination_callback_active(state->other) == state->outer_active;
	/* A different generic scope must neither hide the outer termination
	 * callback nor classify an ordinary operation as a termination callback. */
	struct dssh_session_scope generic;
	session_scope_enter(&generic, state->other);
	valid = session_termination_callback_active(session) && valid;
	valid = session_termination_callback_active(state->other) == state->outer_active && valid;
	if (state->channel != NULL) {
		state->closed = dssh_chan_close(state->channel, INT64_MAX);
		if (state->closed != DSSH_ERROR_INVALID)
			state->channel = NULL;
		valid = state->closed == DSSH_ERROR_INVALID && valid;
	}
	valid = dssh_session_set_log_level(session, DSSH_LOG_ERROR) == DSSH_ERROR_INVALID && valid;
	valid = dssh_session_set_break_cb(session, NULL, NULL) == DSSH_ERROR_INVALID && valid;
	if (state->nested != NULL) {
		bool terminated = dssh_session_terminate(state->other);
		valid = terminated && state->nested->valid && state->nested->calls == 1 && valid;
		valid = session_termination_callback_active(session)
		    && !session_termination_callback_active(state->other) && valid;
	}
	session_scope_leave(&generic);
	state->valid = valid;
}

static int
test_close_termination_callback(void)
{
	unsigned cases = 0, failures = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned gather = 0; gather < 2; gather++) {
			for (unsigned nested = 0; nested < 2; nested++) {
				struct conn_ctx ctx;
				window_test_gather = gather != 0;
				int setup = conn_setup_mode(&ctx, false);
				window_test_gather = false;
				ASSERT_OK(setup);
				ASSERT_TRUE(half_close_algorithms(&ctx));
				if (role != 0) {
					dssh_session swap = ctx.client;
					ctx.client = ctx.server;
					ctx.server = swap;
				}
				struct close_termination_state inner = {.session = ctx.client,
				    .other = ctx.server, .outer_active = true};
				struct close_termination_state state = {.session = ctx.server,
				    .other = ctx.client, .nested = nested != 0 ? &inner : NULL};
				ASSERT_OK(dssh_session_set_terminate_cb(ctx.server, close_termination_callback, &state));
				if (nested != 0)
					ASSERT_OK(dssh_session_set_terminate_cb(ctx.client, close_termination_callback, &inner));
				ASSERT_OK(dssh_session_start(ctx.server));
				state.channel = close_outcome_open(&ctx);
				ASSERT_NOT_NULL(state.channel);
				struct close_outcome_lifetime watch = {.handle = state.channel};
				dssh_test_alloc_observe_frees(close_outcome_observe_free, &watch);
				struct dssh_session_scope generic;
				session_scope_enter(&generic, ctx.server);
				bool valid = !session_termination_callback_active(ctx.server)
				    && !dssh_io_callback_active(ctx.server);
				valid = dssh_session_terminate(ctx.server) && valid;
				valid = state.valid && state.calls == 1 && watch.frees == 0 && valid;
				valid = !session_termination_callback_active(ctx.server)
				    && !session_termination_callback_active(ctx.client)
				    && !dssh_io_callback_active(ctx.server) && valid;
				valid = dssh_session_set_log_level(ctx.server, DSSH_LOG_ERROR) == 0 && valid;
				session_scope_leave(&generic);
				if (state.channel != NULL && watch.frees == 0) {
					int closed = dssh_chan_close(state.channel, INT64_MAX);
					state.channel = NULL;
					valid = closed == 0 && watch.frees == 1 && watch.handle == NULL && valid;
				}
				valid = !dssh_session_terminate(ctx.server) && state.calls == 1 && valid;
				conn_cleanup(&ctx);
				dssh_test_alloc_observe_frees(NULL, NULL);
				valid = watch.frees == 1 && watch.handle == NULL && valid;
				cases++;
				if (!valid)
					failures++;
				printf("close_termination role=%u gather=%u nested=%u calls=%u result=%d frees=%u valid=%u\n",
				    role, gather, nested, state.calls, state.closed, watch.frees, valid ? 1U : 0U);
			}
		}
	}
	printf("close_termination cases=%u failures=%u\n", cases, failures);
	return cases == 8 && failures == 0 ? TEST_PASS : TEST_FAIL;
}
