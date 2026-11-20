/* Each borrowed watch is cleared by the callback before the actual free. No
 * deallocated pointer value is retained for a later comparison. */
struct chan_unit_free_watch {
	const void *pointers[32];
	size_t count;
	size_t freed;
	size_t unexpected;
};

static void
chan_unit_watch_add(struct chan_unit_free_watch *watch, const void *pointer)
{
	if (pointer == NULL)
		return;
	if (watch->count == sizeof(watch->pointers) / sizeof(watch->pointers[0]))
		abort();
	for (size_t i = 0; i < watch->count; i++) {
		if (watch->pointers[i] == pointer)
			abort();
	}
	watch->pointers[watch->count++] = pointer;
}

static void
chan_unit_watch_free(const void *pointer, void *opaque)
{
	struct chan_unit_free_watch *watch = opaque;
	if (pointer == NULL)
		return;
	for (size_t i = 0; i < watch->count; i++) {
		if (watch->pointers[i] == pointer) {
			watch->pointers[i] = NULL;
			watch->freed++;
			return;
		}
	}
	if (watch->unexpected != SIZE_MAX)
		watch->unexpected++;
}

static bool
chan_unit_watch_complete(const struct chan_unit_free_watch *watch)
{
	if (watch->freed != watch->count || watch->unexpected != 0)
		return false;
	for (size_t i = 0; i < watch->count; i++) {
		if (watch->pointers[i] != NULL)
			return false;
	}
	return true;
}

/* Explicit model of a fixture temporary-allocation refusal. Successful controls
 * use the real malloc/free pair; library refusals use the existing injector. */
static void *
chan_unit_refuse_temporary(size_t size)
{
	(void)size;
	return NULL;
}

static int
chan_unit_owner_case(unsigned stage, struct chan_unit_report *report,
    struct chan_unit_free_watch *watch)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	fixture.report = report;
	fixture.observer = chan_unit_watch_free;
	fixture.observer_data = watch;
	struct dssh_bytebuf buffer;
	struct dssh_event_queue queue;
	struct dssh_chan_params params;
	uint8_t *temporary = NULL;
	const uint8_t data[] = {1, 2, 3, 4};
	const uint8_t text[96] = {1, 2, 3, 4};
	struct dssh_event_entry event = {.type = DSSH_EVENT_SIGNAL,
	    .text_a = {.data = text, .len = sizeof(text)}};
	int ret;
	int calls;
	if (stage >= 1) {
		if (stage == 1)
			dssh_test_alloc_fail_after(0);
		ret = chan_unit_bytebuf_init(&fixture, &buffer, 32);
		calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, stage == 1 ? DSSH_ERROR_ALLOC : 0);
		if (stage == 1)
			ASSERT_EQ(calls, 1);
		else
			ASSERT_EQ_U(bytebuf_write(&buffer, data, sizeof(data)), sizeof(data));
	}
	if (stage >= 3) {
		if (stage == 3)
			dssh_test_alloc_fail_after(0);
		ret = chan_unit_event_init(&fixture, &queue, 64, 1024);
		calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, stage == 3 ? DSSH_ERROR_ALLOC : 0);
		if (stage == 3)
			ASSERT_EQ(calls, 1);
		if (stage >= 5) {
			for (size_t i = 0; i < 9; i++)
				ASSERT_OK(event_queue_push(&queue, &event));
			ASSERT_TRUE(event_queue_freeze(&queue));
			ASSERT_EQ_U(queue.count, 8);
			ASSERT_EQ_U(queue.capacity, 16);
		}
	}
	if (stage >= 6) {
		if (stage == 6)
			dssh_test_alloc_fail_after(0);
		ret = chan_unit_params_init(&fixture, &params, DSSH_CHAN_SHELL);
		calls = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, stage == 6 ? DSSH_ERROR_ALLOC : 0);
		if (stage == 6)
			ASSERT_EQ(calls, 1);
		else {
			ASSERT_OK(dssh_chan_params_set_command(&params, "command"));
			ASSERT_OK(dssh_chan_params_set_subsystem(&params, "subsystem"));
			ASSERT_OK(dssh_chan_params_add_env(&params, "name", "value"));
			ASSERT_OK(dssh_chan_params_set_mode(&params, 53, 1));
		}
	}
	if (stage >= 8) {
		if (stage == 8)
			fixture.allocate = chan_unit_refuse_temporary;
		ret = chan_unit_alloc(&fixture, &temporary, 64);
		ASSERT_EQ(ret, stage == 8 ? DSSH_ERROR_ALLOC : 0);
		if (temporary != NULL)
			memset(temporary, 0x85, 64);
	}
	if (stage >= 1)
		chan_unit_watch_add(watch, buffer.data);
	if (stage >= 3) {
		chan_unit_watch_add(watch, queue.entries);
		for (size_t i = 0; i < queue.capacity; i++)
			chan_unit_watch_add(watch, queue.entries[i].storage);
		chan_unit_watch_add(watch, queue.frozen.storage);
	}
	if (stage >= 6) {
		chan_unit_watch_add(watch, params.term);
		chan_unit_watch_add(watch, params.command);
		chan_unit_watch_add(watch, params.subsystem);
		chan_unit_watch_add(watch, params.env);
		for (size_t i = 0; i < params.env_count; i++) {
			chan_unit_watch_add(watch, params.env[i].name);
			chan_unit_watch_add(watch, params.env[i].value);
		}
		chan_unit_watch_add(watch, params.modes);
	}
	chan_unit_watch_add(watch, temporary);
	if (stage != 11)
		ASSERT_TRUE(false);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
chan_unit_owner_main(const char *mode)
{
	static const char *const names[] = {"empty", "buffer-refusal", "buffer",
	    "queue-refusal", "queue", "queue-frozen", "params-refusal", "params",
	    "temporary-refusal", "all", "nested", "normal"};
	unsigned stage = 0;
	while (stage < sizeof(names) / sizeof(names[0]) && strcmp(mode, names[stage]) != 0)
		stage++;
	if (stage == sizeof(names) / sizeof(names[0]))
		return 2;
	struct chan_unit_report report = {0};
	struct chan_unit_free_watch watch = {0};
	struct chan_unit_fixture parent;
	struct chan_unit_report parent_report = {0};
	struct chan_unit_free_watch parent_watch = {0};
	struct dssh_bytebuf parent_buffer;
	bool valid = chan_unit_active == NULL;
	if (stage == 10) {
		chan_unit_begin(&parent, "parent");
		parent.report = &parent_report;
		parent.observer = chan_unit_watch_free;
		parent.observer_data = &parent_watch;
		int ret = chan_unit_bytebuf_init(&parent, &parent_buffer, 8);
		const uint8_t data[] = {8, 7, 6, 5};
		valid = valid && ret == 0;
		if (ret == 0)
			valid = bytebuf_write(&parent_buffer, data, sizeof(data)) == sizeof(data) && valid;
		chan_unit_watch_add(&parent_watch, parent_buffer.data);
	}
	int result = chan_unit_owner_case(stage == 10 ? 9 : stage, &report, &watch);
	if (stage == 10) {
		valid = valid && chan_unit_active == &parent && parent_buffer.used == 4
		    && parent_buffer.data != NULL && parent_buffer.data[0] == 8
		    && parent_buffer.data[3] == 5 && parent_watch.freed == 0;
		int finished = chan_unit_finish(&parent, TEST_PASS, false);
		valid = valid && finished == TEST_PASS && parent_report.registered == 1
		    && parent_report.retired == 1 && parent_report.valid
		    && !parent_report.assertion && chan_unit_watch_complete(&parent_watch);
	}
	const size_t owners[] = {0, 1, 1, 2, 2, 2, 3, 3, 4, 4, 4, 4};
	const size_t frees[] = {0, 0, 1, 1, 2, 11, 11, 18, 18, 19, 19, 19};
	valid = valid && result == (stage == 11 ? TEST_PASS : TEST_FAIL)
	    && report.valid && report.assertion == (stage != 11)
	    && report.registered == owners[stage] && report.retired == owners[stage]
	    && report.temporary_frees == (stage >= 9 ? 1U : 0U)
	    && watch.count == frees[stage] && chan_unit_watch_complete(&watch)
	    && chan_unit_active == NULL;
	int printed = printf("chan_unit_control mode=%s owners=%zu frees=%zu temporary=%zu nested=%u valid=%u\n",
	    mode, report.retired, watch.freed, report.temporary_frees,
	    stage == 10 ? 1U : 0U, valid ? 1U : 0U);
	if (!valid || printed < 0)
		return 2;
	return stage == 11 ? 0 : 1;
}
