/* Owned ordinary stream tests; every result is checked before narrowing. */
#ifndef DSSH_TEST_CONN_STREAM_FIXTURE_H
#define DSSH_TEST_CONN_STREAM_FIXTURE_H

enum stream_body_kind {
	STREAM_FORWARD, STREAM_REVERSE, STREAM_STDERR_REVERSE, STREAM_STDERR_FORWARD,
	STREAM_POLL_READ, STREAM_POLL_TIMEOUT, STREAM_LARGE, STREAM_MULTIPLE,
	STREAM_CLOSE, STREAM_HALF_CLOSE, STREAM_NOMORE, STREAM_NOWAIT,
	STREAM_SUBSYS_OPEN, STREAM_SUBSYS_FORWARD, STREAM_SUBSYS_MULTIPLE,
	STREAM_SUBSYS_POLL, STREAM_SUBSYS_LARGE, STREAM_SUBSYS_CLOSE,
	STREAM_SUBSYS_AFTER_EOF, STREAM_SUBSYS_POLL_WRITE, STREAM_SUBSYS_POLL_EMPTY,
	STREAM_SUBSYS_ROUNDTRIP, STREAM_SUBSYS_ZERO
};
enum stream_body_fault {
	STREAM_NO_FAULT, STREAM_AFTER_OPEN, STREAM_BAD_WRITE, STREAM_BAD_READ,
	STREAM_BAD_POLL, STREAM_LONG_WRITE, STREAM_LONG_READ, STREAM_EXTRA_POLL,
	STREAM_SHORT_IO, STREAM_ALLOC_FIRST, STREAM_ALLOC_SECOND
};
/* These controls are owner-thread only. Library workers never read them. */
static enum stream_body_fault stream_body_fault;
static unsigned stream_body_seen;
static unsigned stream_body_allocations;
static unsigned stream_body_live;

static void *
stream_body_alloc(size_t size)
{
	stream_body_allocations++;
	if ((stream_body_fault == STREAM_ALLOC_FIRST && stream_body_allocations == 1)
	    || (stream_body_fault == STREAM_ALLOC_SECOND && stream_body_allocations == 2)) {
		stream_body_seen++;
		return NULL;
	}
	void *buffer = malloc(size);
	if (buffer != NULL)
		stream_body_live++;
	return buffer;
}

static void
stream_body_free(void *buffer)
{
	if (buffer != NULL) {
		if (stream_body_live == 0)
			abort();
		stream_body_live--;
		free(buffer);
	}
}

static int64_t
stream_body_write(dssh_channel ch, int stream, const uint8_t *data, size_t size)
{
	if (stream_body_fault == STREAM_BAD_WRITE || stream_body_fault == STREAM_LONG_WRITE) {
		stream_body_seen++;
		int64_t count = (int64_t)size;
		return stream_body_fault == STREAM_BAD_WRITE ? DSSH_ERROR_INIT : count + 1;
	}
	if (stream_body_fault == STREAM_SHORT_IO && size > 3) {
		stream_body_seen++;
		size = size > 1024 ? 1024 : 3;
	}
	return dssh_chan_write(ch, stream, data, size);
}

static int
stream_body_poll(dssh_channel ch, int events, int timeout)
{
	if (stream_body_fault == STREAM_BAD_POLL || stream_body_fault == STREAM_EXTRA_POLL) {
		stream_body_seen++;
		return stream_body_fault == STREAM_BAD_POLL ? DSSH_ERROR_TERMINATED
		    : events | DSSH_POLL_EVENT;
	}
	return dssh_chan_poll(ch, events, timeout);
}

static int64_t
stream_body_read(dssh_channel ch, int stream, uint8_t *data, size_t size)
{
	if (stream_body_fault == STREAM_BAD_READ || stream_body_fault == STREAM_LONG_READ) {
		stream_body_seen++;
		int64_t count = (int64_t)size;
		return stream_body_fault == STREAM_BAD_READ ? DSSH_ERROR_TERMINATED : count + 1;
	}
	if (stream_body_fault == STREAM_SHORT_IO && size > 3) {
		stream_body_seen++;
		size = size > 1024 ? 1024 : 3;
	}
	return dssh_chan_read(ch, stream, data, size);
}

static bool
stream_body_write_all(dssh_channel ch, int stream, const uint8_t *data, size_t size)
{
	struct timespec start;
	if (!conn_zc_start_clock(&start, 5))
		return false;
	size_t used = 0;
	while (used < size) {
		int64_t ret = stream_body_write(ch, stream, data + used, size - used);
		if (ret == DSSH_ERROR_NOMORE) {
			if (stream_body_poll(ch, DSSH_POLL_WRITE, 1000) != DSSH_POLL_WRITE
			    || !conn_zc_tick(&start, 5))
				return false;
			continue;
		}
		size_t count = 0;
		if (!conn_zc_io_count(ret, size - used, &count) || count == 0)
			return false;
		used += count;
	}
	return true;
}

static bool
stream_body_read_all(dssh_channel ch, int stream, uint8_t *data, size_t size)
{
	int events = stream == 0 ? DSSH_POLL_READ : DSSH_POLL_READEXT;
	struct timespec start;
	if (!conn_zc_start_clock(&start, 5))
		return false;
	size_t used = 0;
	while (used < size) {
		if (stream_body_poll(ch, events, 1000) != events)
			return false;
		int64_t ret = stream_body_read(ch, stream, data + used, size - used);
		if (ret == DSSH_ERROR_NOMORE) {
			if (!conn_zc_tick(&start, 5))
				return false;
			continue;
		}
		size_t count = 0;
		if (!conn_zc_io_count(ret, size - used, &count) || count == 0)
			return false;
		used += count;
	}
	return true;
}

static int
stream_body_run(enum stream_body_kind kind)
{
	enum stream_body_kind requested = kind;
	bool subsystem = kind >= STREAM_SUBSYS_OPEN;
	if (subsystem) {
		switch (kind) {
			case STREAM_SUBSYS_FORWARD:
			case STREAM_SUBSYS_POLL:
			case STREAM_SUBSYS_ROUNDTRIP:
				kind = STREAM_FORWARD;
				break;
			case STREAM_SUBSYS_MULTIPLE:
				kind = STREAM_MULTIPLE;
				break;
			case STREAM_SUBSYS_LARGE:
				kind = STREAM_LARGE;
				break;
			default:
				break;
		}
	}
	struct conn_ctx ctx;
	stream_body_allocations = 0;
	if (stream_body_live != 0)
		abort();
	if (conn_setup(&ctx) < 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {.client = ctx.client, .server = ctx.server,
	    .command = "stream-body", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	uint8_t *large = NULL;
	uint8_t *received = NULL;
	struct open_subsys_ctx sc = {.client = ctx.client, .server = ctx.server,
	    .subsystem = "test-sub"};
	bool valid;
	if (subsystem) {
		valid = open_channel_pair(&ctx, client_open_subsys_thread, &sc, &sc.client_ch,
		    server_accept_subsys_thread, &sc, &sc.server_ch) == 0;
		oc.client_ch = sc.client_ch;
		oc.server_ch = sc.server_ch;
		if (valid) {
			const char *client_name = dssh_chan_get_subsystem(oc.client_ch);
			const char *server_name = dssh_chan_get_subsystem(oc.server_ch);
			valid = dssh_chan_get_type(oc.client_ch) == DSSH_CHAN_SUBSYSTEM
			    && dssh_chan_get_type(oc.server_ch) == DSSH_CHAN_SUBSYSTEM
			    && client_name != NULL && server_name != NULL
			    && strcmp(client_name, "test-sub") == 0 && strcmp(server_name, "test-sub") == 0;
		}
	}
	else
		valid = open_exec_channel(&ctx, &oc) == 0;
	if (!valid)
		goto done;
	if (stream_body_fault == STREAM_AFTER_OPEN) {
		stream_body_seen++;
		valid = false;
		goto done;
	}
	if (kind == STREAM_SUBSYS_OPEN)
		goto done;
	if (kind == STREAM_SUBSYS_POLL_WRITE || kind == STREAM_SUBSYS_POLL_EMPTY) {
		valid = kind == STREAM_SUBSYS_POLL_WRITE
		    ? stream_body_poll(oc.client_ch, DSSH_POLL_WRITE, 0) == DSSH_POLL_WRITE
		        && stream_body_poll(oc.client_ch, DSSH_POLL_WRITE, 100) == DSSH_POLL_WRITE
		    : stream_body_poll(oc.server_ch, DSSH_POLL_READ, 0) == 0;
		goto done;
	}
	uint8_t empty = 0xa5;
	if (kind == STREAM_SUBSYS_CLOSE) {
		int result = dssh_chan_close(oc.client_ch, -1);
		oc.client_ch = NULL;
		valid = result == 0 && stream_body_poll(oc.server_ch, DSSH_POLL_READ, 1000) == DSSH_POLL_READ
		    && stream_body_read(oc.server_ch, 0, &empty, 1) == 0 && empty == 0xa5;
		goto done;
	}
	if (kind == STREAM_SUBSYS_AFTER_EOF) {
		const uint8_t after[] = "after";
		valid = dssh_chan_shutwr(oc.server_ch) == 0
		    && stream_body_poll(oc.client_ch, DSSH_POLL_READ, 1000) == DSSH_POLL_READ
		    && stream_body_read(oc.client_ch, 0, &empty, 1) == 0 && empty == 0xa5
		    && stream_body_write(oc.server_ch, 0, after, sizeof(after) - 1) == DSSH_ERROR_TERMINATED
		    && !dssh_session_is_terminated(ctx.client) && !dssh_session_is_terminated(ctx.server);
		goto done;
	}
	if (kind == STREAM_SUBSYS_ZERO) {
		valid = stream_body_write(oc.server_ch, 0, &empty, 0) == 0
		    && stream_body_poll(oc.client_ch, DSSH_POLL_READ, 0) == 0
		    && stream_body_poll(oc.server_ch, DSSH_POLL_READ, 0) == 0
		    && stream_body_read(oc.client_ch, 0, &empty, 1) == DSSH_ERROR_NOMORE
		    && stream_body_read(oc.server_ch, 0, &empty, 1) == DSSH_ERROR_NOMORE && empty == 0xa5;
		goto done;
	}
	dssh_channel writer = kind == STREAM_REVERSE || kind == STREAM_STDERR_REVERSE
	    || kind == STREAM_CLOSE || kind == STREAM_HALF_CLOSE || kind == STREAM_NOMORE
	    ? oc.server_ch : oc.client_ch;
	dssh_channel reader = writer == oc.client_ch ? oc.server_ch : oc.client_ch;
	int stream = kind == STREAM_STDERR_FORWARD || kind == STREAM_STDERR_REVERSE ? 1 : 0;
	if (requested == STREAM_SUBSYS_POLL) {
		valid = stream_body_poll(reader, DSSH_POLL_READ, 100) == 0;
		if (!valid)
			goto done;
	}
	if (kind == STREAM_POLL_TIMEOUT || kind == STREAM_NOWAIT) {
		valid = stream_body_poll(reader, kind == STREAM_NOWAIT ? DSSH_POLL_READ | DSSH_POLL_WRITE
		    : DSSH_POLL_READ, kind == STREAM_NOWAIT ? 0 : 100) == (kind == STREAM_NOWAIT ? DSSH_POLL_WRITE : 0);
		goto done;
	}
	uint8_t output[64];
	memset(output, 0xa5, sizeof(output));
	const uint8_t *message = (const uint8_t *)"stream payload";
	size_t length = sizeof("stream payload") - 1;
	if (kind == STREAM_NOMORE) {
		valid = stream_body_read(reader, 0, output, sizeof(output)) == DSSH_ERROR_NOMORE
		    && dssh_chan_read(reader, 0, NULL, 0) == 0
		    && dssh_chan_write(reader, 0, message, 0) == 0;
		if (!valid)
			goto done;
		uint_fast32_t saved = atomic_exchange(&reader->remote_window, 0);
		int64_t refused = dssh_chan_write(reader, 0, message, length);
		atomic_store(&reader->remote_window, saved);
		valid = refused == DSSH_ERROR_NOMORE;
		if (!valid)
			goto done;
	}
	if (kind == STREAM_HALF_CLOSE) {
		valid = dssh_chan_shutwr(reader) == 0
		    && stream_body_poll(writer, DSSH_POLL_READ, 1000) == DSSH_POLL_READ
		    && stream_body_read(writer, 0, output, sizeof(output)) == 0;
		if (!valid)
			goto done;
	}
	if (kind == STREAM_LARGE) {
		size_t total = 32768;
		large = stream_body_alloc(total);
		received = stream_body_alloc(total);
		valid = large != NULL && received != NULL;
		if (!valid)
			goto done;
		for (size_t i = 0; i < total; i++)
			large[i] = (uint8_t)(i & 0xff);
		valid = stream_body_write_all(writer, 0, large, total)
		    && stream_body_read_all(reader, 0, received, total)
		    && memcmp(large, received, total) == 0;
		goto done;
	}
	if (kind == STREAM_MULTIPLE) {
		const uint8_t *joined = subsystem ? (const uint8_t *)"firstsecondthird" : (const uint8_t *)"AAABBBCCC";
		message = joined;
		length = subsystem ? 16 : 9;
		size_t first = subsystem ? 5 : 3;
		size_t second = subsystem ? 6 : 3;
		valid = stream_body_write_all(writer, stream, message, first)
		    && stream_body_write_all(writer, stream, message + first, second)
		    && stream_body_write_all(writer, stream, message + first + second, length - first - second);
	}
	else
		valid = stream_body_write_all(writer, stream, message, length);
	if (!valid)
		goto done;
	if (kind == STREAM_CLOSE || kind == STREAM_NOMORE) {
		int result = dssh_chan_close(writer, 0);
		if (writer == oc.client_ch)
			oc.client_ch = NULL;
		else
			oc.server_ch = NULL;
		valid = result == 0;
	}
	valid = valid && stream_body_read_all(reader, stream, output, length)
	    && memcmp(output, message, length) == 0 && output[length] == 0xa5;
	if (valid && (kind == STREAM_CLOSE || kind == STREAM_NOMORE))
		valid = stream_body_poll(reader, DSSH_POLL_READ, 1000) == DSSH_POLL_READ
		    && stream_body_read(reader, 0, output, sizeof(output)) == 0;
	else if (valid)
		valid = stream_body_read(reader, stream, output, sizeof(output)) == DSSH_ERROR_NOMORE;
	if (valid && requested == STREAM_SUBSYS_ROUNDTRIP) {
		const uint8_t reply[] = {'r', 0, 'e', 0xff, 'p'};
		memset(output, 0xa5, sizeof(output));
		valid = stream_body_write_all(reader, 0, reply, sizeof(reply))
		    && stream_body_read_all(writer, 0, output, sizeof(reply))
		    && memcmp(output, reply, sizeof(reply)) == 0 && output[sizeof(reply)] == 0xa5;
	}
 done:
	stream_body_free(received);
	stream_body_free(large);
	conn_cleanup(&ctx);
	return valid && ctx.client == NULL && ctx.server == NULL && !ctx.io_initialized
	    && stream_body_live == 0 ? TEST_PASS : TEST_FAIL;
}
#endif
