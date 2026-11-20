#ifndef DSSH_TEST_EVENT_BODY_CASES_H
#define DSSH_TEST_EVENT_BODY_CASES_H
static int
test_event_body_controls(void)
{
	int (*hosts[])(void) = {test_send_signal, test_signal_after_data, test_window_change,
	    test_session_close_exit_code, test_read_event_empty, test_signal_interleave_read,
	    test_signal_interleave_read_ext};
	unsigned cases = 0;
	for (unsigned gather = 0; gather < 2; gather++) {
		event_body_gather = gather != 0;
		for (size_t host = 0; host < sizeof(hosts) / sizeof(hosts[0]); host++) {
			bool data = host == EVENT_BODY_AFTER_DATA || host == EVENT_BODY_INTERLEAVE
			    || host == EVENT_BODY_INTERLEAVE_EXT;
			for (unsigned fault = EVENT_BODY_NORMAL; fault <= EVENT_BODY_SHORT; fault++) {
				if ((!data && fault >= EVENT_BODY_WRITE_NEGATIVE)
				    || (host == EVENT_BODY_EMPTY && (fault == EVENT_BODY_SEND_NEGATIVE
				        || fault == EVENT_BODY_SEND_POSITIVE || fault == EVENT_BODY_TYPE
				        || fault == EVENT_BODY_VALUE || fault == EVENT_BODY_POSITION)))
					continue;
				enum event_body_fault selected = (enum event_body_fault)fault;
				event_body_fault = selected;
				event_body_seen = 0;
				stream_body_seen = 0;
				stream_body_fault = fault == EVENT_BODY_WRITE_NEGATIVE ? STREAM_BAD_WRITE
				    : fault == EVENT_BODY_WRITE_LONG ? STREAM_LONG_WRITE
				    : fault == EVENT_BODY_DATA_NEGATIVE ? STREAM_BAD_READ
				    : fault == EVENT_BODY_DATA_LONG ? STREAM_LONG_READ
				    : fault == EVENT_BODY_SHORT ? STREAM_SHORT_IO : STREAM_NO_FAULT;
				int result = hosts[host]();
				stream_body_fault = STREAM_NO_FAULT;
				event_body_fault = EVENT_BODY_NORMAL;
				unsigned seen = fault >= EVENT_BODY_WRITE_NEGATIVE ? stream_body_seen : event_body_seen;
				bool positive = fault == EVENT_BODY_NORMAL || fault == EVENT_BODY_SHORT;
				bool valid = result == (positive ? TEST_PASS : TEST_FAIL)
				    && (fault == EVENT_BODY_NORMAL || seen > 0)
				    && g_active_ctx == NULL && channel_fixture_owner == NULL && stream_body_live == 0;
				if (printf("event_body host=%zu gather=%u fault=%u result=%d observed=%u valid=%u\n",
				    host, gather, fault, result, seen, valid ? 1U : 0U) < 0 || !valid) {
					event_body_gather = false;
					return TEST_FAIL;
				}
				cases++;
			}
			for (unsigned prefix = 0; prefix < 2; prefix++) {
				struct channel_worker_probe probe = {.fail_at = prefix, .cancelled_before_join = true};
				channel_worker_probe_begin(&probe);
				int result = hosts[host]();
				channel_worker_probe_end();
				bool valid = result == TEST_FAIL && probe.creation_failed && probe.calls == prefix + 1
				    && probe.created == prefix && probe.joined == prefix && probe.cancelled_before_join
				    && g_active_ctx == NULL && channel_fixture_owner == NULL && stream_body_live == 0;
				if (printf("event_body host=%zu gather=%u prefix=%u created=%u joined=%u valid=%u\n",
				    host, gather, prefix, probe.created, probe.joined, valid ? 1U : 0U) < 0 || !valid) {
					event_body_gather = false;
					return TEST_FAIL;
				}
				cases++;
			}
		}
	}
	event_body_gather = false;
	return printf("event_body cases=%u valid=1\n", cases) >= 0 ? TEST_PASS : TEST_FAIL;
}
#endif
