#include "conn.h"

#include <stdio.h>
#include <string.h>

static unsigned failures;

struct conn_buffer conn_inbuf;

#define CHECK(expression) do {                                      \
	if (!(expression)) {                                          \
		fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, \
		    __LINE__, #expression);                               \
		failures++;                                               \
	}                                                             \
} while (0)

struct io_thread {
	struct conn_buffer *buf;
	xpevent_t           started;
	xpevent_t           done;
	const unsigned char *input;
	size_t              input_len;
	unsigned char       output[16];
	size_t              output_len;
	size_t              count;
	size_t              found;
};

struct reset_thread {
	struct conn_buffer *buf;
	xpevent_t           started;
	xpevent_t           done;
};

void
doterm_wake(void)
{
}

static bool
start_thread(void (*entry)(void *), void *arg)
{
	if (_beginthread(entry, 0, arg) == (unsigned long)-1) {
		CHECK(false);
		return false;
	}
	return true;
}

static void
wait_for_start(xpevent_t started)
{
	CHECK(WaitForEvent(started, 1000) == WAIT_OBJECT_0);
}

static void
wait_for_done(xpevent_t done)
{
	DWORD result = WaitForEvent(done, 1500);

	CHECK(result == WAIT_OBJECT_0);
}

static void
reader_thread(void *arg)
{
	struct io_thread *io = arg;

	assert_pthread_mutex_lock(&io->buf->read_mutex);
	SetEvent(io->started);
	io->found = conn_buf_wait_bytes(io->buf, io->count, 1000);
	if (io->found != 0)
		io->output_len = conn_buf_get(io->buf, io->output, io->found);
	assert_pthread_mutex_unlock(&io->buf->read_mutex);
	SetEvent(io->done);
}

static void
writer_thread(void *arg)
{
	struct io_thread *io = arg;

	assert_pthread_mutex_lock(&io->buf->write_mutex);
	SetEvent(io->started);
	io->found = conn_buf_wait_free(io->buf, io->count, 1000);
	if (io->found != 0) {
		size_t write_len = io->input_len;

		if (write_len > io->found)
			write_len = io->found;
		io->output_len = conn_buf_put(io->buf, io->input, write_len);
	}
	assert_pthread_mutex_unlock(&io->buf->write_mutex);
	SetEvent(io->done);
}

static void
reset_buffer_thread(void *arg)
{
	struct reset_thread *reset = arg;

	SetEvent(reset->started);
	conn_buf_reset(reset->buf);
	SetEvent(reset->done);
}

static bool
init_io_thread(struct io_thread *io, struct conn_buffer *buf)
{
	memset(io, 0, sizeof(*io));
	io->buf = buf;
	io->started = CreateEvent(NULL, FALSE, FALSE, NULL);
	io->done = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (io->started == NULL || io->done == NULL) {
		if (io->started != NULL)
			CloseEvent(io->started);
		if (io->done != NULL)
			CloseEvent(io->done);
		CHECK(false);
		return false;
	}
	return true;
}

static void
destroy_io_thread(struct io_thread *io)
{
	CloseEvent(io->done);
	CloseEvent(io->started);
}

static void
test_wrap_and_peek(void)
{
	struct conn_buffer buf = {0};
	unsigned char      output[8];

	if (create_conn_buf(&buf, 8) == NULL) {
		CHECK(false);
		return;
	}

	assert_pthread_mutex_lock(&buf.write_mutex);
	CHECK(conn_buf_put(&buf, "abcdef", 6) == 6);
	assert_pthread_mutex_unlock(&buf.write_mutex);

	assert_pthread_mutex_lock(&buf.read_mutex);
	CHECK(conn_buf_get(&buf, output, 4) == 4);
	CHECK(memcmp(output, "abcd", 4) == 0);
	assert_pthread_mutex_unlock(&buf.read_mutex);

	assert_pthread_mutex_lock(&buf.write_mutex);
	CHECK(conn_buf_put(&buf, "GHIJKL", 6) == 6);
	CHECK(conn_buf_free(&buf) == 0);
	assert_pthread_mutex_unlock(&buf.write_mutex);

	assert_pthread_mutex_lock(&buf.read_mutex);
	CHECK(conn_buf_peek(&buf, output, sizeof(output)) == sizeof(output));
	CHECK(memcmp(output, "efGHIJKL", sizeof(output)) == 0);
	CHECK(conn_buf_bytes(&buf) == sizeof(output));
	CHECK(conn_buf_get(&buf, output, sizeof(output)) == sizeof(output));
	CHECK(memcmp(output, "efGHIJKL", sizeof(output)) == 0);
	assert_pthread_mutex_unlock(&buf.read_mutex);
	CHECK(conn_buf_bytes(&buf) == 0);

	destroy_conn_buf(&buf);
}

static void
test_wait_for_data(void)
{
	struct conn_buffer buf = {0};
	struct io_thread   reader;

	if (create_conn_buf(&buf, 8) == NULL || !init_io_thread(&reader, &buf)) {
		CHECK(false);
		return;
	}
	reader.count = 4;
	if (start_thread(reader_thread, &reader)) {
		wait_for_start(reader.started);

		assert_pthread_mutex_lock(&buf.write_mutex);
		CHECK(conn_buf_put(&buf, "data", 4) == 4);
		assert_pthread_mutex_unlock(&buf.write_mutex);

		wait_for_done(reader.done);
		CHECK(reader.found == 4);
		CHECK(reader.output_len == 4);
		CHECK(memcmp(reader.output, "data", 4) == 0);
	}
	destroy_io_thread(&reader);
	destroy_conn_buf(&buf);
}

static void
test_wait_for_space(void)
{
	struct conn_buffer buf = {0};
	struct io_thread   writer;
	unsigned char      output[8];

	if (create_conn_buf(&buf, 8) == NULL || !init_io_thread(&writer, &buf)) {
		CHECK(false);
		return;
	}
	assert_pthread_mutex_lock(&buf.write_mutex);
	CHECK(conn_buf_put(&buf, "ABCDEFGH", 8) == 8);
	assert_pthread_mutex_unlock(&buf.write_mutex);

	writer.input = (const unsigned char *)"xyz";
	writer.input_len = 3;
	writer.count = 3;
	if (start_thread(writer_thread, &writer)) {
		wait_for_start(writer.started);

		assert_pthread_mutex_lock(&buf.read_mutex);
		CHECK(conn_buf_get(&buf, output, 3) == 3);
		assert_pthread_mutex_unlock(&buf.read_mutex);

		wait_for_done(writer.done);
		CHECK(writer.found == 3);
		CHECK(writer.output_len == 3);

		assert_pthread_mutex_lock(&buf.read_mutex);
		CHECK(conn_buf_get(&buf, output, sizeof(output)) == sizeof(output));
		assert_pthread_mutex_unlock(&buf.read_mutex);
		CHECK(memcmp(output, "DEFGHxyz", sizeof(output)) == 0);
	}
	destroy_io_thread(&writer);
	destroy_conn_buf(&buf);
}

static void
test_split_locks(void)
{
	struct conn_buffer buf = {0};
	struct io_thread   reader;
	struct io_thread   writer;

	if (create_conn_buf(&buf, 8) == NULL
	    || !init_io_thread(&reader, &buf)
	    || !init_io_thread(&writer, &buf)) {
		CHECK(false);
		return;
	}

	writer.input = (const unsigned char *)"W";
	writer.input_len = 1;
	writer.count = 1;
	assert_pthread_mutex_lock(&buf.read_mutex);
	if (start_thread(writer_thread, &writer)) {
		wait_for_start(writer.started);
		wait_for_done(writer.done);
		CHECK(writer.output_len == 1);
	}
	assert_pthread_mutex_unlock(&buf.read_mutex);

	reader.count = 1;
	assert_pthread_mutex_lock(&buf.write_mutex);
	if (start_thread(reader_thread, &reader)) {
		wait_for_start(reader.started);
		wait_for_done(reader.done);
		CHECK(reader.output_len == 1);
		CHECK(reader.output[0] == 'W');
	}
	assert_pthread_mutex_unlock(&buf.write_mutex);

	destroy_io_thread(&writer);
	destroy_io_thread(&reader);
	destroy_conn_buf(&buf);
}

static void
test_reset_takes_both_locks(void)
{
	struct conn_buffer buf = {0};
	struct reset_thread reset = {0};

	if (create_conn_buf(&buf, 8) == NULL) {
		CHECK(false);
		return;
	}
	reset.buf = &buf;
	reset.started = CreateEvent(NULL, FALSE, FALSE, NULL);
	reset.done = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (reset.started == NULL || reset.done == NULL) {
		CHECK(false);
		destroy_conn_buf(&buf);
		return;
	}

	assert_pthread_mutex_lock(&buf.write_mutex);
	CHECK(conn_buf_put(&buf, "pending", 7) == 7);
	assert_pthread_mutex_unlock(&buf.write_mutex);

	assert_pthread_mutex_lock(&buf.read_mutex);
	if (start_thread(reset_buffer_thread, &reset)) {
		wait_for_start(reset.started);
		CHECK(WaitForEvent(reset.done, 25) == WAIT_TIMEOUT);
		assert_pthread_mutex_unlock(&buf.read_mutex);
		wait_for_done(reset.done);
		CHECK(conn_buf_bytes(&buf) == 0);
		CHECK(conn_buf_free(&buf) == buf.bufsize);
	}
	else {
		assert_pthread_mutex_unlock(&buf.read_mutex);
	}

	CloseEvent(reset.done);
	CloseEvent(reset.started);
	destroy_conn_buf(&buf);
}

static void
test_reset_wakes_writer(void)
{
	struct conn_buffer buf = {0};
	struct io_thread   writer;
	unsigned char      output;

	if (create_conn_buf(&buf, 1) == NULL || !init_io_thread(&writer, &buf)) {
		CHECK(false);
		return;
	}
	assert_pthread_mutex_lock(&buf.write_mutex);
	CHECK(conn_buf_put(&buf, "A", 1) == 1);
	assert_pthread_mutex_unlock(&buf.write_mutex);

	writer.input = (const unsigned char *)"B";
	writer.input_len = 1;
	writer.count = 1;
	if (start_thread(writer_thread, &writer)) {
		wait_for_start(writer.started);
		conn_buf_reset(&buf);
		wait_for_done(writer.done);
		CHECK(writer.output_len == 1);
		assert_pthread_mutex_lock(&buf.read_mutex);
		CHECK(conn_buf_get(&buf, &output, 1) == 1);
		assert_pthread_mutex_unlock(&buf.read_mutex);
		CHECK(output == 'B');
	}

	destroy_io_thread(&writer);
	destroy_conn_buf(&buf);
}

static void
test_multiple_waiters(void)
{
	struct conn_buffer buf = {0};
	struct io_thread   readers[2];
	struct io_thread   writers[2];
	unsigned char      output[2];

	if (create_conn_buf(&buf, 2) == NULL
	    || !init_io_thread(&readers[0], &buf)
	    || !init_io_thread(&readers[1], &buf)
	    || !init_io_thread(&writers[0], &buf)
	    || !init_io_thread(&writers[1], &buf)) {
		CHECK(false);
		return;
	}

	readers[0].count = readers[1].count = 1;
	if (start_thread(reader_thread, &readers[0])
	    && start_thread(reader_thread, &readers[1])) {
		wait_for_start(readers[0].started);
		wait_for_start(readers[1].started);
		assert_pthread_mutex_lock(&buf.write_mutex);
		CHECK(conn_buf_put(&buf, "XY", 2) == 2);
		assert_pthread_mutex_unlock(&buf.write_mutex);
		wait_for_done(readers[0].done);
		wait_for_done(readers[1].done);
		CHECK(readers[0].output_len == 1);
		CHECK(readers[1].output_len == 1);
		CHECK((readers[0].output[0] == 'X' && readers[1].output[0] == 'Y')
		    || (readers[0].output[0] == 'Y' && readers[1].output[0] == 'X'));
	}

	assert_pthread_mutex_lock(&buf.write_mutex);
	CHECK(conn_buf_put(&buf, "AB", 2) == 2);
	assert_pthread_mutex_unlock(&buf.write_mutex);
	writers[0].input = (const unsigned char *)"M";
	writers[1].input = (const unsigned char *)"N";
	writers[0].input_len = writers[1].input_len = 1;
	writers[0].count = writers[1].count = 1;
	if (start_thread(writer_thread, &writers[0])
	    && start_thread(writer_thread, &writers[1])) {
		wait_for_start(writers[0].started);
		wait_for_start(writers[1].started);
		assert_pthread_mutex_lock(&buf.read_mutex);
		CHECK(conn_buf_get(&buf, output, sizeof(output)) == sizeof(output));
		assert_pthread_mutex_unlock(&buf.read_mutex);
		wait_for_done(writers[0].done);
		wait_for_done(writers[1].done);
		assert_pthread_mutex_lock(&buf.read_mutex);
		CHECK(conn_buf_get(&buf, output, sizeof(output)) == sizeof(output));
		assert_pthread_mutex_unlock(&buf.read_mutex);
		CHECK((output[0] == 'M' && output[1] == 'N')
		    || (output[0] == 'N' && output[1] == 'M'));
	}

	destroy_io_thread(&writers[1]);
	destroy_io_thread(&writers[0]);
	destroy_io_thread(&readers[1]);
	destroy_io_thread(&readers[0]);
	destroy_conn_buf(&buf);
}

static void
test_stale_event(void)
{
	struct conn_buffer buf = {0};
	unsigned char      output;

	if (create_conn_buf(&buf, 1) == NULL) {
		CHECK(false);
		return;
	}
	assert_pthread_mutex_lock(&buf.write_mutex);
	CHECK(conn_buf_put(&buf, "X", 1) == 1);
	assert_pthread_mutex_unlock(&buf.write_mutex);
	assert_pthread_mutex_lock(&buf.read_mutex);
	CHECK(conn_buf_get(&buf, &output, 1) == 1);
	CHECK(conn_buf_wait_bytes(&buf, 1, 10) == 0);
	assert_pthread_mutex_unlock(&buf.read_mutex);
	destroy_conn_buf(&buf);
}

int
main(void)
{
	test_wrap_and_peek();
	test_wait_for_data();
	test_wait_for_space();
	test_split_locks();
	test_reset_takes_both_locks();
	test_reset_wakes_writer();
	test_multiple_waiters();
	test_stale_event();

	if (failures != 0) {
		fprintf(stderr, "%u conn buffer test(s) failed\n", failures);
		return 1;
	}
	puts("conn buffer tests passed");
	return 0;
}
