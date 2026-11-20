/* Actual ordinary host consumers with bounded fixture failure controls. */
#ifndef DSSH_TEST_STREAM_BODY_CASES_H
#define DSSH_TEST_STREAM_BODY_CASES_H

static int
test_stream_body_controls(void)
{
	int (*hosts[])(void) = {
		test_session_write_read, test_session_read_write_reverse,
		test_session_write_ext, test_session_read_ext, test_session_poll_read,
		test_session_poll_timeout, test_session_large_write, test_session_multiple_writes,
		test_eof_detection, test_eof_half_close, test_read_nomore_vs_eof,
		test_session_poll_timeout_zero,
		test_open_subsystem, test_subsys_write_read, test_subsys_multiple_writes,
		test_subsys_poll, test_subsys_large_write, test_channel_close_subsys,
		test_subsys_write_after_close, test_subsys_poll_write, test_subsys_poll_timeout_zero,
		test_subsystem_roundtrip, test_subsys_write_zero
	};
	unsigned cases = 0;
	for (size_t host = 0; host < sizeof(hosts) / sizeof(hosts[0]); host++) {
		bool polling_only = host == STREAM_POLL_TIMEOUT || host == STREAM_NOWAIT
		    || host == STREAM_SUBSYS_POLL_WRITE || host == STREAM_SUBSYS_POLL_EMPTY;
		for (unsigned fault = STREAM_NO_FAULT; fault <= STREAM_ALLOC_SECOND; fault++) {
			if ((host == STREAM_SUBSYS_OPEN && fault != STREAM_NO_FAULT && fault != STREAM_AFTER_OPEN)
			    || (host == STREAM_SUBSYS_CLOSE && (fault == STREAM_BAD_WRITE
			        || fault == STREAM_LONG_WRITE || fault == STREAM_SHORT_IO))
			    || (host == STREAM_SUBSYS_ZERO && fault == STREAM_SHORT_IO)
			    || (polling_only && (fault == STREAM_BAD_WRITE || fault == STREAM_LONG_WRITE
			    || fault == STREAM_BAD_READ || fault == STREAM_LONG_READ || fault == STREAM_SHORT_IO))
			    || (host != STREAM_LARGE && host != STREAM_SUBSYS_LARGE && (fault == STREAM_ALLOC_FIRST || fault == STREAM_ALLOC_SECOND)))
				continue;
			enum stream_body_fault selected = (enum stream_body_fault)fault;
			stream_body_fault = selected;
			stream_body_seen = 0;
			int result = hosts[host]();
			stream_body_fault = STREAM_NO_FAULT;
			bool positive = fault == STREAM_NO_FAULT || fault == STREAM_SHORT_IO;
			bool valid = result == (positive ? TEST_PASS : TEST_FAIL)
			    && (fault == STREAM_NO_FAULT || stream_body_seen > 0)
			    && stream_body_live == 0 && g_active_ctx == NULL && channel_fixture_owner == NULL;
			if (printf("stream_body host=%zu fault=%u result=%d observed=%u valid=%u\n",
			    host, fault, result, stream_body_seen, valid ? 1U : 0U) < 0 || !valid)
				return TEST_FAIL;
			cases++;
		}
		for (unsigned prefix = 0; prefix < 2; prefix++) {
			struct channel_worker_probe probe = {.fail_at = prefix, .cancelled_before_join = true};
			channel_worker_probe_begin(&probe);
			int result = hosts[host]();
			channel_worker_probe_end();
			bool valid = result == TEST_FAIL && probe.creation_failed
			    && probe.created == prefix && probe.joined == prefix && probe.cancelled_before_join
			    && stream_body_live == 0 && g_active_ctx == NULL && channel_fixture_owner == NULL;
			if (printf("stream_body host=%zu prefix=%u created=%u joined=%u valid=%u\n",
			    host, prefix, probe.created, probe.joined, valid ? 1U : 0U) < 0 || !valid)
				return TEST_FAIL;
			cases++;
		}
	}
	return printf("stream_body cases=%u valid=1\n", cases) >= 0 ? TEST_PASS : TEST_FAIL;
}
#endif
