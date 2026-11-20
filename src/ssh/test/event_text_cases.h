struct event_text_capture {
	struct dssh_bytes name;
	struct dssh_bytes error;
	struct dssh_bytes language;
	atomic_int count;
	atomic_bool bad;
	bool identity;
};

static void
event_text_capture(dssh_channel ch, const struct dssh_chan_event *event, void *data)
{
	(void)ch;
	struct event_text_capture *capture = data;
	if (event == NULL || (event->type != DSSH_EVENT_SIGNAL && event->type != DSSH_EVENT_EXIT_SIGNAL)) {
		atomic_store(&capture->bad, true);
		return;
	}
	struct dssh_bytes name = event->type == DSSH_EVENT_SIGNAL
	    ? event->signal.name : event->exit_signal.signal_name;
	bool valid = dssh_test_bytes_equal(name, capture->name.data, capture->name.len);
	if (capture->identity)
		valid = valid && name.data == capture->name.data;
	if (event->type == DSSH_EVENT_EXIT_SIGNAL) {
		valid = valid && dssh_test_bytes_equal(event->exit_signal.error_message,
		    capture->error.data, capture->error.len)
		    && dssh_test_bytes_equal(event->exit_signal.language,
		        capture->language.data, capture->language.len);
		if (capture->identity)
			valid = valid && event->exit_signal.error_message.data == capture->error.data
			    && event->exit_signal.language.data == capture->language.data;
	}
	if (!valid)
		atomic_store(&capture->bad, true);
	atomic_fetch_add(&capture->count, 1);
}

/* Test packets deliberately contain embedded zero, multibyte UTF-8 and fields
 * larger than the former internal arrays. Expected views point into the packet. */
static int
event_text_packet(struct dssh_channel_s *ch, int type, uint8_t *packet,
    size_t capacity, size_t *length, struct event_text_capture *capture, bool empty)
{
	if (capture == NULL || (type != DSSH_EVENT_SIGNAL && type != DSSH_EVENT_EXIT_SIGNAL))
		return DSSH_ERROR_INVALID;
	int ret = event_packet(ch, type, packet, capacity, length);
	if (ret != 0)
		return ret;
	size_t pos = type == DSSH_EVENT_SIGNAL ? 16 : 21;
	size_t lengths[] = {empty ? 0 : 100, empty ? 0 : 300, empty ? 0 : 12};
	struct dssh_bytes *views[] = {&capture->name, &capture->error, &capture->language};
	size_t fields = type == DSSH_EVENT_SIGNAL ? 1 : 3;
	for (size_t field = 0; field < fields; field++) {
		size_t bytes = lengths[field];
		uint32_t wire = (uint32_t)bytes;
		ret = dssh_serialize_uint32(wire, packet, capacity, &pos);
		if (ret != 0 || !dssh_buffer_has(capacity, pos, bytes))
			return DSSH_ERROR_TOOLONG;
		for (size_t i = 0; i < bytes; i++) {
			uint8_t value = (uint8_t)((i + field) % 127);
			packet[pos + i] = value;
		}
		if (bytes > 3) {
			packet[pos] = 0;
			packet[pos + 1] = 0xc3;
			packet[pos + 2] = 0xa9;
		}
		*views[field] = (struct dssh_bytes){.data = bytes != 0 ? packet + pos : NULL, .len = bytes};
		pos += bytes;
		if (type == DSSH_EVENT_EXIT_SIGNAL && field == 0) {
			if (!dssh_buffer_has(capacity, pos, 1))
				return DSSH_ERROR_TOOLONG;
			packet[pos++] = 1;
		}
	}
	*length = pos;
	return 0;
}

/* This independent read-side oracle checks every retained byte after the
 * source packet changes; a nonempty view must own a valid extent. */
static bool
event_text_pattern(struct dssh_bytes view, size_t field, bool empty)
{
	const size_t lengths[] = {100, 300, 12};
	if (field >= sizeof(lengths) / sizeof(lengths[0]))
		return false;
	size_t length = empty ? 0 : lengths[field];
	if (view.len != length)
		return false;
	if (length == 0)
		return view.data == NULL;
	if (view.data == NULL)
		return false;
	for (size_t i = 0; i < length; i++) {
		uint8_t expected = i == 0 ? 0 : i == 1 ? 0xc3 : i == 2 ? 0xa9 : (uint8_t)((i + field) % 127);
		if (view.data[i] != expected)
			return false;
	}
	return true;
}

static int
event_text_case(bool callback, bool empty)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	struct event_text_capture capture = {.identity = callback};
	atomic_init(&capture.count, 0);
	atomic_init(&capture.bad, false);
	if (callback)
		ASSERT_EQ(dssh_chan_set_event_cb(fixture.open.server_ch, event_text_capture, &capture), 0);
	int types[] = {DSSH_EVENT_SIGNAL, DSSH_EVENT_EXIT_SIGNAL};
	for (size_t type = 0; type < sizeof(types) / sizeof(types[0]); type++) {
		uint8_t packet[1024];
		size_t length = 0;
		ASSERT_EQ(event_text_packet(fixture.open.server_ch, types[type], packet,
		    sizeof(packet), &length, &capture, empty), 0);
		if (callback)
			dssh_test_alloc_fail_after(0);
		int ret = dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, length);
		int allocations = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, 0);
		if (callback)
			ASSERT_EQ(allocations, 0);
		else {
			ASSERT_EQ(dssh_chan_poll(fixture.open.server_ch, DSSH_POLL_EVENT, 0), DSSH_POLL_EVENT);
			struct dssh_chan_event event;
			ASSERT_EQ(dssh_chan_read_event(fixture.open.server_ch, &event), 0);
			event_text_capture(fixture.open.server_ch, &event, &capture);
			/* Packet storage can change immediately after dispatch in poll mode. */
			memset(packet, 0xee, sizeof(packet));
			struct dssh_bytes name = types[type] == DSSH_EVENT_SIGNAL
			    ? event.signal.name : event.exit_signal.signal_name;
			ASSERT_TRUE(event_text_pattern(name, 0, empty));
			if (types[type] == DSSH_EVENT_EXIT_SIGNAL) {
				ASSERT_TRUE(event_text_pattern(event.exit_signal.error_message, 1, empty));
				ASSERT_TRUE(event_text_pattern(event.exit_signal.language, 2, empty));
			}
			if (!empty) {
				ASSERT_EQ(name.data[0], 0);
				ASSERT_EQ(name.data[1], 0xc3);
				ASSERT_EQ(name.data[99], 99);
				if (types[type] == DSSH_EVENT_EXIT_SIGNAL) {
					ASSERT_EQ_U(event.exit_signal.error_message.len, 300);
					ASSERT_EQ(event.exit_signal.error_message.data[299], 46);
					ASSERT_EQ_U(event.exit_signal.language.len, 12);
					ASSERT_EQ(event.exit_signal.language.data[11], 13);
				}
			}
			else {
				ASSERT_NULL(name.data);
				if (types[type] == DSSH_EVENT_EXIT_SIGNAL) {
					ASSERT_NULL(event.exit_signal.error_message.data);
					ASSERT_NULL(event.exit_signal.language.data);
				}
			}
		}
		ASSERT_EQ(atomic_load(&capture.count), type + 1);
		ASSERT_FALSE(atomic_load(&capture.bad));
		ASSERT_TRUE(storage_checkpoint(ST_TEXT_VIEW));
	}
	bool valid = atomic_load(&capture.count) == 2 && !atomic_load(&capture.bad);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_DONE));
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

static int
test_event_text_binary_callback(void)
{
	return event_text_case(true, false);
}
static int
test_event_text_binary_poll(void)
{
	return event_text_case(false, false);
}
static int
test_event_text_empty_callback(void)
{
	return event_text_case(true, true);
}
static int
test_event_text_empty_poll(void)
{
	return event_text_case(false, true);
}

static int
test_event_text_packet_boundary(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	size_t capacity = tx_payload_capacity(fixture.ctx.client);
	ASSERT_TRUE(capacity > 20 && capacity <= UINT32_MAX);
	struct storage_packet_fixture storage = {0};
	flow_init(&storage.flow, &fixture.ctx, NULL, &storage);
	storage.flow.workers.retire = storage_packet_retire;
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_PREPARED));
	storage.packet = malloc(capacity);
	ASSERT_NOT_NULL(storage.packet);
	uint8_t *packet = storage.packet;
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_ALLOCATED));
	size_t length = 0;
	ASSERT_EQ(event_packet(fixture.open.server_ch, DSSH_EVENT_SIGNAL, packet, capacity, &length), 0);
	size_t pos = 16;
	size_t bytes = capacity - pos - 4;
	uint32_t wire = (uint32_t)bytes;
	ASSERT_EQ(dssh_serialize_uint32(wire, packet, capacity, &pos), 0);
	memset(packet + pos, 0xb5, bytes);
	packet[pos] = 0;
	ASSERT_EQ(send_packet(fixture.ctx.client, packet, capacity, NULL), 0);
	int polled = dssh_chan_poll(fixture.open.server_ch, DSSH_POLL_EVENT, 30000);
	ASSERT_TRUE(polled >= 0 && (polled & DSSH_POLL_EVENT) != 0);
	struct dssh_chan_event event;
	ASSERT_EQ(dssh_chan_read_event(fixture.open.server_ch, &event), 0);
	ASSERT_EQ(event.type, DSSH_EVENT_SIGNAL);
	ASSERT_TRUE(dssh_test_bytes_equal(event.signal.name, packet + pos, bytes));
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_PACKET));
	flow_finish(&storage.flow);
	ASSERT_NULL(storage.packet);
	ASSERT_EQ_U(storage.frees, 1);
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

struct event_text_free_check {
	dssh_channel ch;
	const void *metadata;
	bool seen;
	bool unlocked;
};

static void
event_text_free_check(const void *ptr, void *opaque)
{
	struct event_text_free_check *check = opaque;
	if (ptr == NULL || ptr != check->metadata)
		return;
	check->metadata = NULL;
	check->seen = true;
	int ret = mtx_trylock(&check->ch->buf_mtx);
	check->unlocked = ret == thrd_success;
	if (ret == thrd_success && mtx_unlock(&check->ch->buf_mtx) != thrd_success)
		abort();
}

static int
test_event_text_free_observer(void)
{
	struct dssh_channel_s ch = {0};
	if (mtx_init(&ch.buf_mtx, mtx_plain) != thrd_success)
		return TEST_FAIL;
	unsigned char metadata = 0;
	struct event_text_free_check check = {.ch = &ch, .metadata = &metadata};
	event_text_free_check(NULL, &check);
	bool valid = !check.seen;
	event_text_free_check(&metadata, &check);
	valid = valid && check.metadata == NULL && check.seen && check.unlocked;
	check.seen = false;
	check.unlocked = false;
	event_text_free_check(NULL, &check);
	valid = valid && !check.seen && !check.unlocked;
	mtx_destroy(&ch.buf_mtx);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_DONE));
	return TEST_PASS;
}

static int
test_event_text_growth_unlocked(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	for (int i = 0; i < 8; i++)
		ASSERT_EQ(event_send(&fixture, DSSH_EVENT_SIGNAL, true), 0);
	ASSERT_EQ(mtx_lock(&fixture.open.server_ch->buf_mtx), thrd_success);
	struct event_text_free_check check = {.ch = fixture.open.server_ch,
	    .metadata = fixture.open.server_ch->events.entries};
	if (mtx_unlock(&fixture.open.server_ch->buf_mtx) != thrd_success)
		abort();
	dssh_test_alloc_observe_frees(event_text_free_check, &check);
	int ret = event_send(&fixture, DSSH_EVENT_SIGNAL, true);
	dssh_test_alloc_observe_frees(NULL, NULL);
	bool valid = ret == 0 && check.seen && check.unlocked;
	ASSERT_TRUE(valid);
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_DONE));
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

struct event_text_poll_job {
	struct event_fixture *fixture;
	atomic_bool done, queued, resume, cancelled;
	int result;
};

static void
event_text_poll_release(void *opaque)
{
	struct event_text_poll_job *job = opaque;
	atomic_store(&job->cancelled, true);
	atomic_store(&job->resume, true);
}

static int
event_text_poll_writer(void *opaque)
{
	struct event_text_poll_job *job = opaque;
	uint8_t packet[256];
	size_t length = 0;
	job->result = event_packet(job->fixture->open.server_ch, DSSH_EVENT_SIGNAL,
	    packet, sizeof(packet), &length);
	for (size_t i = 0; i < 512 && job->result == 0; i++) {
		if (atomic_load(&job->cancelled)) {
			job->result = DSSH_ERROR_TERMINATED;
			break;
		}
		size_t pos = 16;
		job->result = dssh_serialize_uint32(100, packet, sizeof(packet), &pos);
		if (job->result != 0)
			break;
		uint8_t byte = (uint8_t)(i % 251);
		if (!dssh_buffer_has(sizeof(packet), pos, 100)) {
			job->result = DSSH_ERROR_TOOLONG;
			break;
		}
		memset(packet + pos, byte, 100);
		job->result = dssh_test_demux_dispatch(job->fixture->ctx.server,
		    packet[0], packet, pos + 100);
		if (i == 0 && job->result == 0) {
			atomic_store(&job->queued, true);
			if (!wire_wait(job->fixture->ctx.server, &job->resume))
				job->result = DSSH_ERROR_TERMINATED;
		}
	}
	atomic_store(&job->done, true);
	return job->result;
}

static int
test_event_text_poll_race(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	ASSERT_EQ(dssh_chan_set_max_events(fixture.open.server_ch, 0), 0);
	struct event_text_poll_job job = {.fixture = &fixture};
	atomic_init(&job.done, false);
	atomic_init(&job.queued, false);
	atomic_init(&job.resume, false);
	atomic_init(&job.cancelled, false);
	struct flow_fixture flow;
	flow_init(&flow, &fixture.ctx, event_text_poll_release, &job);
	ASSERT_EQ(channel_worker_start(&flow.workers, 0, event_text_poll_writer, &job), 0);
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_STARTED));
	ASSERT_TRUE(wire_wait(fixture.ctx.server, &job.queued));
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_QUEUED));
	bool valid = true;
	size_t count = 0;
	struct timespec start;
	ASSERT_EQ(clock_gettime(CLOCK_MONOTONIC, &start), 0);
	while (valid && count < 512) {
		int ready = dssh_chan_poll(fixture.open.server_ch, DSSH_POLL_EVENT, 0);
		if (ready == 0) {
			if (atomic_load(&job.done) && job.result != 0)
				break;
			if (!dssh_test_wait_tick(&start)) {
				valid = false;
				break;
			}
			continue;
		}
		struct dssh_chan_event event;
		valid = ready == DSSH_POLL_EVENT
		    && dssh_chan_read_event(fixture.open.server_ch, &event) == 0;
		if (valid) {
			uint8_t expected[100];
			uint8_t byte = (uint8_t)(count % 251);
			memset(expected, byte, sizeof(expected));
			valid = event.type == DSSH_EVENT_SIGNAL
			    && dssh_test_bytes_equal(event.signal.name, expected, sizeof(expected));
			if (count == 0 && valid) {
				ASSERT_TRUE(storage_checkpoint(ST_TEXT_BORROWED));
				atomic_store(&job.resume, true);
			}
			/* Writer can grow/wrap the ring while this borrowed view is live. */
			thrd_yield();
			valid = valid && dssh_test_bytes_equal(event.signal.name, expected, sizeof(expected));
			count++;
		}
	}
	if (!valid || count != 512)
		event_text_poll_release(&job);
	flow_finish(&flow);
	valid = valid && flow.workers.results[0] == 0 && job.result == 0
	    && atomic_load(&job.done) && count == 512;
	ASSERT_TRUE(valid);
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_DONE));
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

static int
test_event_text_poll_reuse(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	ASSERT_EQ(dssh_chan_set_max_events(fixture.open.server_ch, 1), 0);
	uint8_t packet[1024];
	size_t length = 0;
	struct event_text_capture capture = {0};
	atomic_init(&capture.count, 0);
	atomic_init(&capture.bad, false);
	ASSERT_EQ(event_text_packet(fixture.open.server_ch, DSSH_EVENT_EXIT_SIGNAL,
	    packet, sizeof(packet), &length, &capture, false), 0);
	int ret = 0;
	bool valid = true;
	for (int i = 0; i < 160 && ret == 0 && valid; i++) {
		if (i == 16)
			dssh_test_alloc_fail_after(0);
		ret = dssh_test_demux_dispatch(fixture.ctx.server, packet[0], packet, length);
		struct dssh_chan_event event;
		valid = ret == 0
		    && dssh_chan_poll(fixture.open.server_ch, DSSH_POLL_EVENT, 0) == DSSH_POLL_EVENT
		    && dssh_chan_read_event(fixture.open.server_ch, &event) == 0;
		if (valid)
			event_text_capture(fixture.open.server_ch, &event, &capture);
	}
	int allocations = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	valid = valid && ret == 0 && atomic_load(&capture.count) == 160
	    && !atomic_load(&capture.bad) && allocations == 0;
	ASSERT_TRUE(valid);
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_DONE));
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}

static int
test_storage_text_pattern(void)
{
	struct event_fixture fixture = {0};
	ASSERT_TRUE(event_setup(&fixture, NULL, false));
	uint8_t packet[1024];
	struct event_text_capture capture = {0};
	size_t length = 0;
	const int types[] = {DSSH_EVENT_SIGNAL, DSSH_EVENT_EXIT_SIGNAL};
	for (unsigned empty = 0; empty < 2; empty++) {
		for (size_t type = 0; type < sizeof(types) / sizeof(types[0]); type++) {
			ASSERT_EQ(event_text_packet(fixture.open.server_ch, types[type], packet,
			    sizeof(packet), &length, &capture, empty != 0), 0);
			struct dssh_bytes views[] = {capture.name, capture.error, capture.language};
			size_t fields = types[type] == DSSH_EVENT_SIGNAL ? 1 : 3;
			for (size_t field = 0; field < fields; field++) {
				struct dssh_bytes view = views[field];
				ASSERT_TRUE(event_text_pattern(view, field, empty != 0));
				struct dssh_bytes invalid = view;
				invalid.len = SIZE_MAX;
				ASSERT_FALSE(event_text_pattern(invalid, field, empty != 0));
				if (empty != 0) {
					invalid = (struct dssh_bytes){.data = packet, .len = 0};
					ASSERT_FALSE(event_text_pattern(invalid, field, true));
					continue;
				}
				invalid = (struct dssh_bytes){.data = NULL, .len = view.len};
				ASSERT_FALSE(event_text_pattern(invalid, field, false));
				/* The view points into our mutable source packet. */
				size_t offset = 0;
				if (field == 0)
					offset = types[type] == DSSH_EVENT_SIGNAL ? 20 : 25;
				else
					offset = field == 1 ? 130 : 434;
				ASSERT_TRUE(dssh_buffer_has(length, offset, view.len));
				ASSERT_TRUE(view.data == packet + offset);
				for (size_t i = 0; i < view.len; i++) {
					uint8_t saved = packet[offset + i];
					packet[offset + i] ^= 0xff;
					ASSERT_FALSE(event_text_pattern(view, field, false));
					packet[offset + i] = saved;
				}
				ASSERT_TRUE(event_text_pattern(view, field, false));
			}
		}
	}
	struct dssh_bytes invalid = {0};
	ASSERT_FALSE(event_text_pattern(invalid, 3, true));
	ASSERT_FALSE(event_text_pattern(invalid, SIZE_MAX, true));
	ASSERT_EQ(event_text_packet(fixture.open.server_ch, DSSH_EVENT_EOF, packet,
	    sizeof(packet), &length, &capture, false), DSSH_ERROR_INVALID);
	ASSERT_EQ(event_text_packet(fixture.open.server_ch, DSSH_EVENT_SIGNAL, packet,
	    sizeof(packet), &length, NULL, false), DSSH_ERROR_INVALID);
	ASSERT_TRUE(storage_checkpoint(ST_TEXT_DONE));
	conn_cleanup(&fixture.ctx);
	return TEST_PASS;
}
