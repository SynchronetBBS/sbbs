/* Complete borrowed fields, retained ownership, wrap/grow/freeze and explicit
 * memory accounting. These cases use unshared queues, so allocation injection
 * cannot affect a peer thread or external crypto operation. */
static bool
text_entry_matches(const struct dssh_event_entry *entry, const uint8_t *bytes, size_t len)
{
	return dssh_test_bytes_equal(entry->text_a, bytes, len)
	    && dssh_test_bytes_equal(entry->text_b, bytes, len)
	    && dssh_test_bytes_equal(entry->language, bytes, len);
}

static int
test_event_text_queue_lifetime(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t bytes[300];
	for (size_t i = 0; i < sizeof(bytes); i++) {
		uint8_t value = (uint8_t)(i % 251);
		bytes[i] = value;
	}
	struct dssh_event_queue q;
	ASSERT_OK(chan_unit_event_init(&fixture, &q, 64, 32768));
	struct dssh_event_entry e = {.type = DSSH_EVENT_EXIT_SIGNAL,
	    .text_a = {.data = bytes, .len = sizeof(bytes)},
	    .text_b = {.data = bytes, .len = sizeof(bytes)},
	    .language = {.data = bytes, .len = sizeof(bytes)}};
	for (int round = 0; round < 8; round++) {
		for (int i = 0; i < 24; i++)
			ASSERT_OK(event_queue_push(&q, &e));
		ASSERT_TRUE(event_queue_freeze(&q));
		struct dssh_event_entry frozen = q.frozen;
		ASSERT_TRUE(text_entry_matches(&frozen, bytes, sizeof(bytes)));
		/* More ring growth and wrap must not mutate the last returned view. */
		for (int i = 0; i < 24; i++)
			ASSERT_OK(event_queue_push(&q, &e));
		ASSERT_TRUE(text_entry_matches(&frozen, bytes, sizeof(bytes)));
		ASSERT_TRUE(frozen.storage == q.frozen.storage);
		while (event_queue_freeze(&q))
			ASSERT_TRUE(text_entry_matches(&q.frozen, bytes, sizeof(bytes)));
	}
	/* Every slot plus frozen storage is warm: no allocation is now needed. */
	dssh_test_alloc_fail_after(0);
	int ret = 0;
	bool valid = true;
	for (int i = 0; i < 256 && ret == 0; i++) {
		ret = event_queue_push(&q, &e);
		valid = valid && event_queue_freeze(&q)
		    && text_entry_matches(&q.frozen, bytes, sizeof(bytes));
	}
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	chan_unit_event_free(&fixture, &q);
	ASSERT_EQ(ret, 0);
	ASSERT_TRUE(valid);
	ASSERT_EQ(allocations, 0);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_event_text_queue_rollback(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t bytes[300];
	memset(bytes, 0x85, sizeof(bytes));
	struct dssh_event_entry e = {.type = DSSH_EVENT_SIGNAL,
	    .text_a = {.data = bytes, .len = sizeof(bytes)}};
	for (int fail = 0; fail < 2; fail++) {
		struct dssh_event_queue q;
		ASSERT_OK(chan_unit_event_init(&fixture, &q, 64, 32768));
		for (int i = 0; i < 8; i++)
			ASSERT_OK(event_queue_push(&q, &e));
		size_t old_text_capacity = q.text_capacity;
		dssh_test_alloc_fail_after(fail);
		int ret = event_queue_push(&q, &e);
		dssh_test_alloc_reset();
		bool unchanged = q.count == 8 && q.capacity == 8
		    && q.text_capacity == old_text_capacity;
		for (size_t i = 0; i < 8; i++)
			unchanged = unchanged && dssh_test_bytes_equal(q.entries[i].text_a, bytes, sizeof(bytes));
		ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
		ASSERT_TRUE(unchanged);
		ASSERT_OK(event_queue_push(&q, &e));
		for (int i = 0; i < 9; i++) {
			ASSERT_TRUE(event_queue_freeze(&q));
			ASSERT_TRUE(dssh_test_bytes_equal(q.frozen.text_a, bytes, sizeof(bytes)));
		}
		chan_unit_event_free(&fixture, &q);
	}
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_event_text_queue_envelope(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_event_queue q;
	ASSERT_OK(chan_unit_event_init(&fixture, &q, 0, 64));
	struct dssh_event_entry e = {.type = DSSH_EVENT_EOF};
	int ret = 0;
	size_t pushed = 0;
	while (ret == 0 && pushed < 4096) {
		ret = event_queue_push(&q, &e);
		if (ret == 0)
			pushed++;
	}
	ASSERT_EQ(ret, DSSH_ERROR_TOOMANY);
	ASSERT_TRUE(pushed >= 64 && pushed < 4096);
	ASSERT_EQ_U(q.count, pushed);
	while (event_queue_freeze(&q)) {
		ASSERT_NULL(q.frozen.text_a.data);
		ASSERT_EQ_U(q.frozen.text_a.len, 0);
	}
	uint8_t bytes[65] = {0};
	e = (struct dssh_event_entry){.type = DSSH_EVENT_SIGNAL,
	    .text_a = {.data = bytes, .len = sizeof(bytes)}};
	ASSERT_EQ(event_queue_push(&q, &e), DSSH_ERROR_TOOMANY);
	ASSERT_EQ_U(q.count, 0);
	e.text_a.len = 64;
	ASSERT_OK(event_queue_push(&q, &e));
	ASSERT_TRUE(event_queue_freeze(&q));
	ASSERT_TRUE(dssh_test_bytes_equal(q.frozen.text_a, bytes, 64));
	chan_unit_event_free(&fixture, &q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}
