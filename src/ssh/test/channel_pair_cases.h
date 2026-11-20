/* Actual ordinary host consumers of the shared two-worker owner. */
#ifndef DSSH_TEST_CHANNEL_PAIR_CASES_H
#define DSSH_TEST_CHANNEL_PAIR_CASES_H

static const struct {
	const char *name;
	int (*function)(void);
	unsigned workers;
} channel_pair_consumers[] = {
	{ "chan_get_pty", test_chan_get_pty, 4 },
	{ "channel_close_subsys", test_channel_close_subsys, 2 },
	{ "env_callback_rejection", test_env_callback_rejection, 2 },
	{ "env_roundtrip", test_env_roundtrip, 2 },
	{ "open_shell", test_open_shell, 2 },
	{ "open_subsystem", test_open_subsystem, 2 },
	{ "subsys_large_write", test_subsys_large_write, 2 },
	{ "subsys_multiple_writes", test_subsys_multiple_writes, 2 },
	{ "subsys_poll", test_subsys_poll, 2 },
	{ "subsys_poll_timeout_zero", test_subsys_poll_timeout_zero, 2 },
	{ "subsys_poll_write", test_subsys_poll_write, 2 },
	{ "subsys_write_after_close", test_subsys_write_after_close, 2 },
	{ "subsys_write_read", test_subsys_write_read, 2 },
	{ "subsys_write_zero", test_subsys_write_zero, 2 },
	{ "subsystem_roundtrip", test_subsystem_roundtrip, 2 },
	{ "window_change", test_window_change, 2 },
};

static atomic_bool channel_pair_poll_fired;

static int
channel_pair_poll_failure(dssh_channel channel, int events, int timeout)
{
	int ret = dssh_chan_poll(channel, events, timeout);
	if (ret == DSSH_POLL_WRITE && channel->sess->trans.client
	    && !atomic_exchange(&channel_pair_poll_fired, true))
		return -1;
	return ret;
}

static int
channel_pair_control(size_t index)
{
	if (index >= sizeof(channel_pair_consumers) / sizeof(channel_pair_consumers[0]))
		return TEST_FAIL;
	unsigned workers = channel_pair_consumers[index].workers;
	unsigned creations = 0;
	bool valid = wait_stream_writable(NULL) == -1;
	for (unsigned fail_at = 0; fail_at < workers; fail_at++) {
		struct channel_worker_probe probe = {
			.fail_at = fail_at, .cancelled_before_join = true,
		};
		channel_worker_probe_begin(&probe);
		int ret = channel_pair_consumers[index].function();
		channel_worker_probe_end();
		valid = valid && ret == TEST_FAIL && probe.calls == fail_at + 1
		    && probe.created == fail_at && probe.joined == fail_at
		    && probe.creation_failed && probe.cancelled_before_join
		    && channel_fixture_owner == NULL && g_active_ctx == NULL;
		for (unsigned i = 0; i < probe.created; i++)
			valid = valid && atomic_load(&probe.jobs[i].done);
		creations++;
	}
	for (unsigned fault = 0; fault < 2; fault++) {
		struct channel_worker_probe probe = {
			.fail_at = 4, .cancelled_before_join = true,
		};
		atomic_init(&channel_pair_poll_fired, false);
		if (fault != 0)
			channel_fixture_poll = channel_pair_poll_failure;
		channel_worker_probe_begin(&probe);
		int ret = channel_pair_consumers[index].function();
		channel_worker_probe_end();
		channel_fixture_poll = dssh_chan_poll;
		unsigned expected_workers = fault == 0 ? workers : 2;
		valid = valid && ret == (fault == 0 ? TEST_PASS : TEST_FAIL)
		    && atomic_load(&channel_pair_poll_fired) == (fault != 0)
		    && probe.calls == expected_workers && probe.created == expected_workers
		    && probe.joined == expected_workers && !probe.creation_failed
		    && channel_fixture_owner == NULL && g_active_ctx == NULL;
		for (unsigned i = 0; i < probe.created; i++)
			valid = valid && atomic_load(&probe.jobs[i].done);
	}
	if (printf("channel_pair_ownership consumer=%s creations=%u native=1 poll=1 valid=%u\n",
	    channel_pair_consumers[index].name, creations, valid ? 1U : 0U) < 0)
		return TEST_FAIL;
	ASSERT_TRUE(valid);
	return TEST_PASS;
}

#define CHANNEL_PAIR_CASE(name, index) \
	static int pair_##name(void) \
	{ \
		return channel_pair_control(index); \
	}
CHANNEL_PAIR_CASE(chan_get_pty, 0)
CHANNEL_PAIR_CASE(channel_close_subsys, 1)
CHANNEL_PAIR_CASE(env_callback_rejection, 2)
CHANNEL_PAIR_CASE(env_roundtrip, 3)
CHANNEL_PAIR_CASE(open_shell, 4)
CHANNEL_PAIR_CASE(open_subsystem, 5)
CHANNEL_PAIR_CASE(subsys_large_write, 6)
CHANNEL_PAIR_CASE(subsys_multiple_writes, 7)
CHANNEL_PAIR_CASE(subsys_poll, 8)
CHANNEL_PAIR_CASE(subsys_poll_timeout_zero, 9)
CHANNEL_PAIR_CASE(subsys_poll_write, 10)
CHANNEL_PAIR_CASE(subsys_write_after_close, 11)
CHANNEL_PAIR_CASE(subsys_write_read, 12)
CHANNEL_PAIR_CASE(subsys_write_zero, 13)
CHANNEL_PAIR_CASE(subsystem_roundtrip, 14)
CHANNEL_PAIR_CASE(window_change, 15)
#undef CHANNEL_PAIR_CASE

static int
channel_pair_fatal_child(const char *consumer, const char *mode)
{
	bool second = strcmp(consumer, "chan_get_pty_second") == 0;
	if (second)
		consumer = "chan_get_pty";
	if (strcmp(mode, "retained") != 0 && strcmp(mode, "released") != 0)
		return 2;
	for (size_t i = 0; i < sizeof(channel_pair_consumers) / sizeof(channel_pair_consumers[0]); i++) {
		if (strcmp(consumer, channel_pair_consumers[i].name) != 0)
			continue;
		struct channel_worker_probe probe = {
			.fail_at = 4, .cancelled_before_join = true,
			.fatal_join = second ? 3U : 1U,
			.fatal_released = strcmp(mode, "released") == 0,
		};
		channel_worker_probe_begin(&probe);
		channel_pair_consumers[i].function();
		channel_worker_probe_end();
		return 4;
	}
	return 2;
}
#endif
