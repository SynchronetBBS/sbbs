/* Owned event/control consumers on actual authenticated stream channels. */
#ifndef DSSH_TEST_CONN_EVENT_FIXTURE_H
#define DSSH_TEST_CONN_EVENT_FIXTURE_H

enum event_body_kind {
	EVENT_BODY_SIGNAL, EVENT_BODY_AFTER_DATA, EVENT_BODY_WINDOW, EVENT_BODY_EXIT,
	EVENT_BODY_EMPTY, EVENT_BODY_INTERLEAVE, EVENT_BODY_INTERLEAVE_EXT
};
enum event_body_fault {
	EVENT_BODY_NORMAL, EVENT_BODY_AFTER_OPEN, EVENT_BODY_SEND_NEGATIVE,
	EVENT_BODY_SEND_POSITIVE, EVENT_BODY_POLL_NEGATIVE, EVENT_BODY_POLL_EXTRA,
	EVENT_BODY_READ_NEGATIVE, EVENT_BODY_READ_POSITIVE, EVENT_BODY_TYPE,
	EVENT_BODY_VALUE, EVENT_BODY_POSITION, EVENT_BODY_WRITE_NEGATIVE,
	EVENT_BODY_WRITE_LONG, EVENT_BODY_DATA_NEGATIVE, EVENT_BODY_DATA_LONG,
	EVENT_BODY_SHORT
};
static enum event_body_fault event_body_fault;
static unsigned event_body_seen;
static bool event_body_gather;

static bool
event_body_pair_is(dssh_channel ch, dssh_chan_event_cb callback, void *data)
{
	if (mtx_lock(&ch->cb_mtx) != thrd_success)
		return false;
	bool matches = ch->event_cb == callback && ch->event_cbdata == data;
	if (mtx_unlock(&ch->cb_mtx) != thrd_success)
		abort();
	return matches;
}

static bool
event_body_limit_is(dssh_channel ch, size_t expected)
{
	if (mtx_lock(&ch->buf_mtx) != thrd_success)
		return false;
	bool matches = ch->events.max_events == expected;
	if (mtx_unlock(&ch->buf_mtx) != thrd_success)
		abort();
	return matches;
}

static int
event_body_send(dssh_channel *sender, enum event_body_kind kind)
{
	int ret;
	if (kind == EVENT_BODY_EXIT) {
		ret = dssh_chan_close(*sender, 42);
		*sender = NULL;
	}
	else if (kind == EVENT_BODY_WINDOW)
		ret = dssh_chan_send_window_change(*sender, 132, 50, 1056, 800);
	else
		ret = dssh_chan_send_signal(*sender, kind == EVENT_BODY_AFTER_DATA
		    || kind == EVENT_BODY_INTERLEAVE_EXT ? "TERM" : "INT");
	if (ret == 0 && (event_body_fault == EVENT_BODY_SEND_NEGATIVE
	    || event_body_fault == EVENT_BODY_SEND_POSITIVE)) {
		event_body_seen++;
		return event_body_fault == EVENT_BODY_SEND_NEGATIVE ? DSSH_ERROR_INIT : 1;
	}
	return ret;
}

static int
event_body_poll(dssh_channel ch, int events, int timeout)
{
	int ret = dssh_chan_poll(ch, events, timeout);
	if (ret >= 0 && (event_body_fault == EVENT_BODY_POLL_NEGATIVE
	    || event_body_fault == EVENT_BODY_POLL_EXTRA)) {
		event_body_seen++;
		int extra = (events & DSSH_POLL_EVENT) != 0 ? DSSH_POLL_READ : DSSH_POLL_EVENT;
		return event_body_fault == EVENT_BODY_POLL_NEGATIVE ? DSSH_ERROR_INIT : events | extra;
	}
	return ret;
}

static int
event_body_read_event(dssh_channel ch, struct dssh_chan_event *event, int expected_type)
{
	int ret = dssh_chan_read_event(ch, event);
	if (ret == 0 && event->type != expected_type)
		return DSSH_ERROR_PARSE;
	if ((ret == 0 || ret == DSSH_ERROR_NOMORE)
	    && (event_body_fault == EVENT_BODY_READ_NEGATIVE || event_body_fault == EVENT_BODY_READ_POSITIVE)) {
		event_body_seen++;
		return event_body_fault == EVENT_BODY_READ_NEGATIVE ? DSSH_ERROR_INIT : 1;
	}
	if (ret != 0)
		return ret;
	if (event_body_fault == EVENT_BODY_TYPE) {
		event_body_seen++;
		event->type = 0;
	}
	else if (event_body_fault == EVENT_BODY_VALUE) {
		event_body_seen++;
		if (event->type == DSSH_EVENT_SIGNAL) {
			struct dssh_bytes value = {.data = (const uint8_t *)"different", .len = 9};
			event->signal.name = value;
		}
		else if (event->type == DSSH_EVENT_WINDOW_CHANGE)
			event->window_change.cols = 133;
		else if (event->type == DSSH_EVENT_EXIT_STATUS)
			event->exit_status.exit_code = 43;
	}
	else if (event_body_fault == EVENT_BODY_POSITION) {
		event_body_seen++;
		event->stdout_pos = SIZE_MAX;
		event->stderr_pos = SIZE_MAX;
	}
	return ret;
}

static bool
event_body_available(dssh_channel reader, int stream, size_t expected)
{
	struct timespec start;
	if (!conn_zc_start_clock(&start, 5))
		return false;
	for (;;) {
		int64_t ret = dssh_chan_read(reader, stream, NULL, 0);
		size_t count = 0;
		if (!conn_zc_io_count(ret, expected, &count))
			return false;
		if (count == expected)
			return true;
		if (!conn_zc_tick(&start, 5))
			return false;
	}
}

static bool
event_body_value(const struct dssh_chan_event *event, enum event_body_kind kind,
    size_t stdout_pos, size_t stderr_pos)
{
	if (event->stdout_pos != stdout_pos || event->stderr_pos != stderr_pos)
		return false;
	if (kind == EVENT_BODY_WINDOW)
		return event->type == DSSH_EVENT_WINDOW_CHANGE && event->window_change.cols == 132
		    && event->window_change.rows == 50 && event->window_change.wpx == 1056
		    && event->window_change.hpx == 800;
	if (kind == EVENT_BODY_EXIT)
		return event->type == DSSH_EVENT_EXIT_STATUS && event->exit_status.exit_code == 42;
	bool term = kind == EVENT_BODY_AFTER_DATA || kind == EVENT_BODY_INTERLEAVE_EXT;
	return event->type == DSSH_EVENT_SIGNAL && dssh_test_bytes_equal(event->signal.name,
	    term ? "TERM" : "INT", term ? 4 : 3);
}

static int
event_body_run(enum event_body_kind kind)
{
	struct conn_ctx ctx;
	window_test_gather = event_body_gather;
	int setup = conn_setup(&ctx);
	window_test_gather = false;
	if (setup != 0)
		return TEST_FAIL;
	struct open_exec_ctx oc = {.client = ctx.client, .server = ctx.server,
	    .command = "event-body", .cbs = &accept_cbs_all, .accept_timeout = 30000};
	struct open_shell_ctx shell = {.client = ctx.client, .server = ctx.server,
	    .term = "vt100", .cols = 80, .rows = 24, .cbs = &accept_cbs_all};
	bool valid;
	if (kind == EVENT_BODY_WINDOW) {
		valid = open_channel_pair(&ctx, client_open_shell_thread, &shell, &shell.client_ch,
		    server_accept_shell_thread, &shell, &shell.server_ch) == 0;
		oc.client_ch = shell.client_ch;
		oc.server_ch = shell.server_ch;
	}
	else
		valid = open_exec_channel(&ctx, &oc) == 0;
	if (!valid)
		goto done;
	if (event_body_fault == EVENT_BODY_AFTER_OPEN) {
		event_body_seen++;
		valid = false;
		goto done;
	}
	dssh_channel writer = kind == EVENT_BODY_EXIT || kind == EVENT_BODY_INTERLEAVE_EXT
	    ? oc.server_ch : oc.client_ch;
	dssh_channel reader = writer == oc.client_ch ? oc.server_ch : oc.client_ch;
	struct dssh_chan_event event = {.type = 77, .stdout_pos = 19, .stderr_pos = 23};
	if (kind == EVENT_BODY_EMPTY) {
		valid = event_body_poll(reader, DSSH_POLL_EVENT, 0) == 0
		    && event_body_read_event(reader, &event, 0) == DSSH_ERROR_NOMORE
		    && event.type == 77 && event.stdout_pos == 19 && event.stderr_pos == 23;
		goto done;
	}
	int stream = kind == EVENT_BODY_INTERLEAVE_EXT ? 1 : 0;
	bool interleave = kind == EVENT_BODY_INTERLEAVE || kind == EVENT_BODY_INTERLEAVE_EXT;
	const uint8_t *data = kind == EVENT_BODY_AFTER_DATA ? (const uint8_t *)"before signal"
	    : stream == 0 ? (const uint8_t *)"BEFOREAFTER" : (const uint8_t *)"ERR1ERR2";
	size_t length = kind == EVENT_BODY_AFTER_DATA ? 13 : stream == 0 ? 11 : 8;
	bool has_data = interleave || kind == EVENT_BODY_AFTER_DATA;
	if (has_data) {
		size_t first = interleave ? stream == 0 ? 6 : 4 : length;
		valid = stream_body_write_all(writer, stream, data, first);
		if (!valid)
			goto done;
	}
	int sent = event_body_send(&writer, kind);
	if (kind == EVENT_BODY_EXIT)
		oc.server_ch = writer;
	valid = sent == 0;
	if (!valid)
		goto done;
	uint8_t received[64];
	memset(received, 0xa5, sizeof(received));
	if (interleave) {
		size_t first = stream == 0 ? 6 : 4;
		valid = stream_body_write_all(writer, stream, data + first, length - first)
		    && event_body_available(reader, stream, length);
	}
	else if (has_data)
		valid = stream_body_read_all(reader, stream, received, length)
		    && memcmp(received, data, length) == 0 && received[length] == 0xa5;
	int expected_type = kind == EVENT_BODY_WINDOW ? DSSH_EVENT_WINDOW_CHANGE
	    : kind == EVENT_BODY_EXIT ? DSSH_EVENT_EXIT_STATUS : DSSH_EVENT_SIGNAL;
	valid = valid && event_body_poll(reader, DSSH_POLL_EVENT, 1000) == DSSH_POLL_EVENT
	    && event_body_read_event(reader, &event, expected_type) == 0
	    && event_body_value(&event, kind, interleave && stream == 0 ? length : 0,
	        interleave && stream == 1 ? length : 0);
	if (!valid)
		goto done;
	if (interleave)
		valid = stream_body_read_all(reader, stream, received, length)
		    && memcmp(received, data, length) == 0 && received[length] == 0xa5;
	if (kind == EVENT_BODY_EXIT) {
		const int types[] = {DSSH_EVENT_EOF, DSSH_EVENT_CLOSE};
		for (size_t i = 0; valid && i < sizeof(types) / sizeof(types[0]); i++)
			valid = event_body_poll(reader, DSSH_POLL_EVENT, 1000) == DSSH_POLL_EVENT
			    && event_body_read_event(reader, &event, types[i]) == 0 && event.type == types[i]
			    && event.stdout_pos == 0 && event.stderr_pos == 0;
		uint8_t sentinel = 0xa5;
		valid = valid && event_body_poll(reader, DSSH_POLL_READ | DSSH_POLL_READEXT, 0)
		    == (DSSH_POLL_READ | DSSH_POLL_READEXT)
		    && dssh_chan_read(reader, 0, &sentinel, 1) == 0
		    && dssh_chan_read(reader, 1, &sentinel, 1) == 0 && sentinel == 0xa5;
	}
	valid = valid && event_body_poll(reader, DSSH_POLL_EVENT, 0) == 0
	    && event_body_read_event(reader, &event, 0) == DSSH_ERROR_NOMORE;
 done:
	conn_cleanup(&ctx);
	return valid && ctx.client == NULL && ctx.server == NULL && !ctx.io_initialized ? TEST_PASS : TEST_FAIL;
}
#endif
