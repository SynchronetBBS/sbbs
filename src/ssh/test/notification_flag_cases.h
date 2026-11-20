/* Known RFC 4254 notifications require a false reply flag. Native authenticated
 * peer barriers expose replies and completed callback/poll publication. */
static int
test_notification_reply_flags(void)
{
	unsigned cases = 0, failures = 0;
	for (unsigned role = 0; role < 2; role++) {
		for (unsigned gather = 0; gather < 2; gather++) {
			for (unsigned delivery = 0; delivery < 3; delivery++) {
				struct break_handler_fixture fixture = {0};
				struct break_handler_capture actions = {0};
				break_handler_capture_init(&actions, true);
				ASSERT_TRUE(break_handler_setup(&fixture, &actions, role, gather, delivery == 1));
				if (delivery == 2)
					ASSERT_OK(dssh_chan_set_event_cb(fixture.channel, NULL, NULL));
				static const int types[] = {DSSH_EVENT_SIGNAL, DSSH_EVENT_WINDOW_CHANGE,
				    DSSH_EVENT_EXIT_STATUS, DSSH_EVENT_EXIT_SIGNAL};
				static const uint8_t flags[] = {1, 255, 0};
				uint_least32_t window = atomic_load(&fixture.channel->remote_window);
				for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
					for (size_t j = 0; j < sizeof(flags) / sizeof(flags[0]); j++) {
						uint8_t packet[128];
						size_t length = 0;
						ASSERT_OK(event_packet(fixture.channel, types[i], packet, sizeof(packet), &length));
						uint32_t name_length = DSSH_GET_U32(packet + 5);
						ASSERT_TRUE(name_length <= 13);
						size_t name_size = (size_t)name_length;
						packet[9 + name_size] = flags[j];
						int before = atomic_load(&fixture.events.count);
						ASSERT_OK(send_packet(fixture.ctx.client, packet, length, NULL));
						unsigned success = 0, failure = 0;
						bool valid = incoming_control_barrier(fixture.ctx.client, &success, &failure)
						    && success == 0 && failure == (flags[j] != 0 ? 1U : 0U);
						bool present;
						if (delivery == 2) {
							int ready = dssh_chan_poll(fixture.channel, DSSH_POLL_EVENT, 0);
							present = ready == DSSH_POLL_EVENT;
							valid = valid && ready >= 0;
							if (present) {
								struct dssh_chan_event event;
								valid = dssh_chan_read_event(fixture.channel, &event) == 0
								    && event.type == types[i] && valid;
							}
						}
						else {
							int after = atomic_load(&fixture.events.count);
							present = after > before;
							valid = valid && before >= 0 && before < EVENT_CAPTURE_CAP
							    && after == before + (present ? 1 : 0) && !atomic_load(&fixture.events.bad);
							if (present && before >= 0 && before < EVENT_CAPTURE_CAP)
								valid = valid && fixture.events.events[before].type == types[i];
						}
						valid = valid && present == (flags[j] == 0)
						    && atomic_load(&fixture.channel->remote_window) == window
						    && atomic_load(&actions.count) == 0
						    && !dssh_session_is_terminated(fixture.ctx.server);
						if (types[i] == DSSH_EVENT_EXIT_STATUS && flags[j] != 0) {
							if (mtx_lock(&fixture.channel->buf_mtx) != thrd_success)
								abort();
							bool exit_received = fixture.channel->exit_code_received;
							if (mtx_unlock(&fixture.channel->buf_mtx) != thrd_success)
								abort();
							valid = valid && !exit_received;
						}
						if (!valid)
							failures++;
						cases++;
					}
				}
				/* BREAK uses the independently selected action/result contract. */
				bool controls = break_handler_send(&fixture, 500, 1) && break_handler_reply(&fixture, 1, true)
				    && break_handler_send(&fixture, 0, 255) && break_handler_reply(&fixture, 255, true)
				    && atomic_load(&actions.count) == 2 && !atomic_load(&actions.bad);
				/* Ignoring unsupported local-flow-control notifications must not
				 * create an unsolicited response or an application event. */
				static const uint8_t flow_name[] = "xon-xoff";
				static const uint8_t booleans[] = {0, 1, 255};
				int prior_events = atomic_load(&fixture.events.count);
				for (size_t value = 0; value < sizeof(booleans) / sizeof(booleans[0]); value++) {
					for (size_t flag = 0; flag < sizeof(booleans) / sizeof(booleans[0]); flag++) {
						unsigned success = 0, failure = 0;
						controls = request_frame_send(fixture.ctx.client, fixture.channel->local_id,
						    flow_name, sizeof(flow_name) - 1, booleans[flag], &booleans[value], 1) == 0
						    && incoming_control_barrier(fixture.ctx.client, &success, &failure)
						    && success == 0 && failure == (booleans[flag] != 0 ? 1U : 0U) && controls;
					}
				}
				controls = controls && atomic_load(&fixture.events.count) == prior_events
				    && (delivery != 2 || dssh_chan_poll(fixture.channel, DSSH_POLL_EVENT, 0) == 0);
				if (!controls)
					failures++;
				conn_cleanup(&fixture.ctx);
			}
		}
	}
	printf("notification_reply_flags cases=%u failures=%u break_controls=24 flow_controls=108\n", cases, failures);
	return cases == 144 && failures == 0 ? TEST_PASS : TEST_FAIL;
}

static int
test_notification_flag_failures(void)
{
	unsigned cases = 0, failures = 0;
	static const int types[] = {DSSH_EVENT_SIGNAL, DSSH_EVENT_WINDOW_CHANGE,
	    DSSH_EVENT_EXIT_STATUS, DSSH_EVENT_EXIT_SIGNAL};
	for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
		for (unsigned kind = 0; kind < 3; kind++) {
			struct break_handler_fixture fixture = {0};
			ASSERT_TRUE(break_handler_setup(&fixture, NULL, 0, 0, false));
			uint8_t packet[128];
			size_t length = 0;
			ASSERT_OK(event_packet(fixture.channel, types[i], packet, sizeof(packet), &length));
			uint32_t name_length = DSSH_GET_U32(packet + 5);
			ASSERT_TRUE(name_length <= 13);
			size_t name_size = (size_t)name_length;
			packet[9 + name_size] = 255;
			if (kind == 0)
				dssh_test_alloc_fail_after(0);
			else if (kind == 1)
				dssh_test_mtx_reject_once(&fixture.channel->cb_mtx);
			else
				setup_tx_test_hook = half_close_fail_tx;
			int result = dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, length);
			int attempts = kind == 0 ? dssh_test_alloc_count() : 0;
			dssh_test_alloc_reset();
			setup_tx_test_hook = NULL;
			unsigned unowned = dssh_test_mtx_rejected_unlocks();
			dssh_test_thrd_reset();
			bool valid = atomic_load(&fixture.events.count) == 0 && !atomic_load(&fixture.events.bad)
			    && unowned == 0 && !tx_owner_is_current(fixture.ctx.server);
			if (kind != 2) {
				valid = valid && result == 0 && attempts == 0 && !dssh_session_is_terminated(fixture.ctx.server);
				if (result == 0)
					valid = request_frame_receive(fixture.ctx.client, SSH_MSG_CHANNEL_FAILURE, 42, NULL)
					    && channel_id_wire_barrier(fixture.ctx.client) && valid;
			}
			else
				valid = valid && result == DSSH_ERROR_INIT && dssh_session_is_terminated(fixture.ctx.server);
			if (mtx_lock(&fixture.channel->buf_mtx) != thrd_success)
				abort();
			valid = valid && !fixture.channel->exit_code_received;
			if (mtx_unlock(&fixture.channel->buf_mtx) != thrd_success)
				abort();
			conn_cleanup(&fixture.ctx);
			if (!valid)
				failures++;
			cases++;
		}
	}
	printf("notification_flag_failures cases=%u failures=%u\n", cases, failures);
	return cases == 12 && failures == 0 ? TEST_PASS : TEST_FAIL;
}
