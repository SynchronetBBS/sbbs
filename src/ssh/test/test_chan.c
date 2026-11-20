/*
 * test_chan.c -- Unit tests for channel buffer primitives and
 * RFC 4251 wire format functions.
 *
 * Tests bytebuf, event queue, accept queue,
 * dssh_parse_uint32, dssh_serialize_uint32, and dssh_cleanse.
 */

#include "dssh_test.h"
#include "dssh_test_internal.h"
#include "deucessh-conn.h"
#include "dssh_test_alloc.h"
#include "channel_unit_fixture.h"

/* ================================================================
 * Bytebuf -- circular byte buffer
 * ================================================================ */

static int test_bytebuf_init_free(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 256));
	ASSERT_NOT_NULL(b.data);
	ASSERT_EQ_U(b.capacity, 256);
	ASSERT_EQ_U(b.head, 0);
	ASSERT_EQ_U(b.tail, 0);
	ASSERT_EQ_U(b.used, 0);
	ASSERT_EQ_U(b.total, 0);
	chan_unit_bytebuf_free(&fixture, &b);
	ASSERT_NULL(b.data);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_write_read_basic(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 64));

	const uint8_t msg[] = "Hello, world!";
	size_t wrote = bytebuf_write(&b, msg, sizeof(msg));
	ASSERT_EQ_U(wrote, sizeof(msg));
	ASSERT_EQ_U(bytebuf_available(&b), sizeof(msg));
	ASSERT_EQ_U(b.total, sizeof(msg));

	uint8_t out[64];
	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, sizeof(msg));
	ASSERT_MEM_EQ(out, msg, sizeof(msg));
	ASSERT_EQ_U(bytebuf_available(&b), 0);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_empty_read(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 32));

	uint8_t out[16];
	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 0);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_write_full(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 8));

	uint8_t data[16];
	memset(data, 0xAA, sizeof(data));

	size_t wrote = bytebuf_write(&b, data, sizeof(data));
	ASSERT_EQ_U(wrote, 8);
	ASSERT_EQ_U(bytebuf_available(&b), 8);
	ASSERT_EQ_U(bytebuf_free_space(&b), 0);

	/* Write when completely full */
	size_t wrote2 = bytebuf_write(&b, data, 1);
	ASSERT_EQ_U(wrote2, 0);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_partial_write(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 10));

	uint8_t data[7];
	memset(data, 'A', sizeof(data));
	ASSERT_EQ_U(bytebuf_write(&b, data, 7), 7);
	ASSERT_EQ_U(bytebuf_free_space(&b), 3);

	/* Only 3 bytes of space left */
	uint8_t more[5];
	memset(more, 'B', sizeof(more));
	size_t wrote = bytebuf_write(&b, more, 5);
	ASSERT_EQ_U(wrote, 3);
	ASSERT_EQ_U(bytebuf_available(&b), 10);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_free_space(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 100));

	ASSERT_EQ_U(bytebuf_free_space(&b), 100);
	ASSERT_EQ_U(bytebuf_available(&b), 0);

	uint8_t data[40];
	memset(data, 'X', sizeof(data));
	ASSERT_EQ_U(bytebuf_write(&b, data, 40), 40);
	ASSERT_EQ_U(bytebuf_free_space(&b), 60);
	ASSERT_EQ_U(bytebuf_available(&b), 40);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_total_monotonic(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 16));

	uint8_t data[4] = {1, 2, 3, 4};
	ASSERT_EQ_U(bytebuf_write(&b, data, 4), 4);
	ASSERT_EQ_U(b.total, 4);

	uint8_t out[4];
	ASSERT_EQ_U(bytebuf_read(&b, out, sizeof(out), 0), sizeof(out));
	ASSERT_EQ_U(b.total, 4);  /* total doesn't decrease on read */

	ASSERT_EQ_U(bytebuf_write(&b, data, 4), 4);
	ASSERT_EQ_U(b.total, 8);

	ASSERT_EQ_U(bytebuf_write(&b, data, 4), 4);
	ASSERT_EQ_U(b.total, 12);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_wrap_around(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 8));

	uint8_t data[6] = {1, 2, 3, 4, 5, 6};
	ASSERT_EQ_U(bytebuf_write(&b, data, 6), 6);

	/* Read 4 bytes to advance head */
	uint8_t out[8];
	ASSERT_EQ_U(bytebuf_read(&b, out, 4, 0), 4);
	ASSERT_EQ_U(bytebuf_available(&b), 2);

	/* Write 6 more bytes -- wraps around */
	uint8_t data2[6] = {10, 20, 30, 40, 50, 60};
	size_t wrote = bytebuf_write(&b, data2, 6);
	ASSERT_EQ_U(wrote, 6);
	ASSERT_EQ_U(bytebuf_available(&b), 8);

	/* Read everything: should get {5, 6, 10, 20, 30, 40, 50, 60} */
	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 8);
	uint8_t expected[] = {5, 6, 10, 20, 30, 40, 50, 60};
	ASSERT_MEM_EQ(out, expected, 8);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_wrap_write_split(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 8));

	/* Fill and drain to position tail near end */
	uint8_t fill[7];
	memset(fill, 'A', 7);
	ASSERT_EQ_U(bytebuf_write(&b, fill, 7), 7);

	uint8_t out[8];
	ASSERT_EQ_U(bytebuf_read(&b, out, 7, 0), 7);
	/* head=7, tail=7 */

	/* Write 5 bytes -- 1 at end, 4 at start */
	uint8_t data[5] = {10, 20, 30, 40, 50};
	size_t wrote = bytebuf_write(&b, data, 5);
	ASSERT_EQ_U(wrote, 5);

	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 5);
	ASSERT_MEM_EQ(out, data, 5);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_read_limit(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 64));

	uint8_t data[20];
	for (int i = 0; i < 20; i++)
		data[i] = (uint8_t)i;
	ASSERT_EQ_U(bytebuf_write(&b, data, 20), 20);

	/* Read with limit = 5 */
	uint8_t out[64];
	size_t got = bytebuf_read(&b, out, sizeof(out), 5);
	ASSERT_EQ_U(got, 5);
	ASSERT_MEM_EQ(out, data, 5);

	/* Remaining 15 still available */
	ASSERT_EQ_U(bytebuf_available(&b), 15);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_read_limit_vs_bufsz(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 64));

	uint8_t data[30];
	memset(data, 'Z', 30);
	ASSERT_EQ_U(bytebuf_write(&b, data, 30), 30);

	/* bufsz=10 but limit=20 -- bufsz should cap it */
	uint8_t out[10];
	size_t got = bytebuf_read(&b, out, 10, 20);
	ASSERT_EQ_U(got, 10);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_read_limit_zero(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 64));

	uint8_t data[15];
	memset(data, 'Q', 15);
	ASSERT_EQ_U(bytebuf_write(&b, data, 15), 15);

	/* limit=0 means no limit */
	uint8_t out[64];
	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 15);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_multiple_write_read_cycles(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 16));

	uint8_t out[16];
	for (int cycle = 0; cycle < 20; cycle++) {
		uint8_t val = (uint8_t)(cycle & 0xFF);
		uint8_t data[5];
		memset(data, val, 5);

		size_t wrote = bytebuf_write(&b, data, 5);
		ASSERT_EQ_U(wrote, 5);

		size_t got = bytebuf_read(&b, out, sizeof(out), 0);
		ASSERT_EQ_U(got, 5);
		for (int i = 0; i < 5; i++)
			ASSERT_EQ(out[i], val);
	}
	ASSERT_EQ_U(b.total, 100);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_wrap_read_split(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 8));

	/* Position head near end */
	uint8_t fill[6];
	memset(fill, 'X', 6);
	ASSERT_EQ_U(bytebuf_write(&b, fill, 6), 6);

	uint8_t out[8];
	ASSERT_EQ_U(bytebuf_read(&b, out, 6, 0), 6);
	/* head=6, tail=6 */

	/* Write 6 bytes wrapping: 2 at end + 4 at start */
	uint8_t data[6] = {10, 20, 30, 40, 50, 60};
	ASSERT_EQ_U(bytebuf_write(&b, data, 6), 6);

	/* Read all 6 -- splits across wrap boundary */
	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 6);
	ASSERT_MEM_EQ(out, data, 6);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_zero_len_write(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 16));

	size_t wrote = bytebuf_write(&b, (const uint8_t *)"x", 0);
	ASSERT_EQ_U(wrote, 0);
	ASSERT_EQ_U(bytebuf_available(&b), 0);
	ASSERT_EQ_U(b.total, 0);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_single_byte_ops(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 4));

	/* Write and read one byte at a time, cycling through */
	for (int i = 0; i < 20; i++) {
		uint8_t val = (uint8_t)(i + 1);
		ASSERT_EQ_U(bytebuf_write(&b, &val, 1), 1);

		uint8_t out;
		ASSERT_EQ_U(bytebuf_read(&b, &out, 1, 0), 1);
		ASSERT_EQ(out, val);
	}

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_exact_capacity(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 8));

	uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	ASSERT_EQ_U(bytebuf_write(&b, data, 8), 8);
	ASSERT_EQ_U(bytebuf_free_space(&b), 0);
	ASSERT_EQ_U(bytebuf_available(&b), 8);

	uint8_t out[8];
	ASSERT_EQ_U(bytebuf_read(&b, out, 8, 0), 8);
	ASSERT_MEM_EQ(out, data, 8);
	ASSERT_EQ_U(bytebuf_free_space(&b), 8);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_read_limit_less_than_available(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 32));

	uint8_t data[20];
	for (int i = 0; i < 20; i++)
		data[i] = (uint8_t)i;
	ASSERT_EQ_U(bytebuf_write(&b, data, 20), 20);

	/* limit=3, bufsz=32 -- should read exactly 3 */
	uint8_t out[32];
	size_t got = bytebuf_read(&b, out, sizeof(out), 3);
	ASSERT_EQ_U(got, 3);
	ASSERT_MEM_EQ(out, data, 3);

	/* Read remainder with limit=0 */
	got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 17);
	ASSERT_MEM_EQ(out, data + 3, 17);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_fill_drain_refill(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 8));

	/* Fill completely */
	uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	ASSERT_EQ_U(bytebuf_write(&b, data, 8), 8);

	/* Drain completely */
	uint8_t out[8];
	ASSERT_EQ_U(bytebuf_read(&b, out, 8, 0), 8);
	ASSERT_EQ_U(bytebuf_available(&b), 0);
	ASSERT_EQ_U(bytebuf_free_space(&b), 8);

	/* Refill completely */
	uint8_t data2[8] = {10, 20, 30, 40, 50, 60, 70, 80};
	ASSERT_EQ_U(bytebuf_write(&b, data2, 8), 8);
	ASSERT_EQ_U(bytebuf_read(&b, out, 8, 0), 8);
	ASSERT_MEM_EQ(out, data2, 8);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_available_after_partial_read(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 32));

	uint8_t data[20];
	memset(data, 'M', 20);
	ASSERT_EQ_U(bytebuf_write(&b, data, 20), 20);

	uint8_t out[5];
	ASSERT_EQ_U(bytebuf_read(&b, out, 5, 0), 5);
	ASSERT_EQ_U(bytebuf_available(&b), 15);

	ASSERT_EQ_U(bytebuf_read(&b, out, 5, 0), 5);
	ASSERT_EQ_U(bytebuf_available(&b), 10);

	ASSERT_EQ_U(bytebuf_read(&b, out, 5, 0), 5);
	ASSERT_EQ_U(bytebuf_available(&b), 5);

	ASSERT_EQ_U(bytebuf_read(&b, out, 5, 0), 5);
	ASSERT_EQ_U(bytebuf_available(&b), 0);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_total_survives_wrap(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 8));

	/* Write/read cycles that wrap multiple times */
	uint8_t data[6], out[6];
	memset(data, 'W', 6);
	for (int i = 0; i < 10; i++) {
		ASSERT_EQ_U(bytebuf_write(&b, data, 6), 6);
		ASSERT_EQ_U(bytebuf_read(&b, out, 6, 0), 6);
	}
	ASSERT_EQ_U(b.total, 60);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_read_limit_with_wrap(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 8));

	/* Position near end */
	uint8_t fill[6];
	memset(fill, 'X', 6);
	ASSERT_EQ_U(bytebuf_write(&b, fill, 6), 6);

	uint8_t out[8];
	ASSERT_EQ_U(bytebuf_read(&b, out, 6, 0), 6);

	/* Write 7 bytes wrapping around */
	uint8_t data[7] = {1, 2, 3, 4, 5, 6, 7};
	ASSERT_EQ_U(bytebuf_write(&b, data, 7), 7);

	/* Read with limit=3 -- should cross wrap boundary correctly */
	size_t got = bytebuf_read(&b, out, sizeof(out), 3);
	ASSERT_EQ_U(got, 3);
	ASSERT_MEM_EQ(out, data, 3);

	/* Read remaining 4 */
	got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 4);
	ASSERT_MEM_EQ(out, data + 3, 4);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_partial_write_at_wrap(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 8));

	/* Advance to tail=6 by writing 6 and reading 6 */
	uint8_t fill[6];
	memset(fill, 'F', 6);
	ASSERT_EQ_U(bytebuf_write(&b, fill, 6), 6);
	uint8_t out[8];
	ASSERT_EQ_U(bytebuf_read(&b, out, 6, 0), 6);

	/* Write 3 bytes to partially fill the end */
	uint8_t d1[3] = {1, 2, 3};
	ASSERT_EQ_U(bytebuf_write(&b, d1, 3), 3);
	ASSERT_EQ_U(bytebuf_available(&b), 3);
	ASSERT_EQ_U(bytebuf_free_space(&b), 5);

	/* Try to write 7 more -- only 5 fit */
	uint8_t d2[7] = {10, 20, 30, 40, 50, 60, 70};
	ASSERT_EQ_U(bytebuf_write(&b, d2, 7), 5);
	ASSERT_EQ_U(bytebuf_available(&b), 8);

	/* Verify all data */
	ASSERT_EQ_U(bytebuf_read(&b, out, sizeof(out), 0), 8);
	uint8_t expected[8] = {1, 2, 3, 10, 20, 30, 40, 50};
	ASSERT_MEM_EQ(out, expected, 8);

	chan_unit_bytebuf_free(&fixture, &b);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_bytebuf_head_tail_reset_after_free(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_bytebuf b;
	ASSERT_OK(chan_unit_bytebuf_init(&fixture, &b, 16));

	uint8_t data[10];
	memset(data, 'R', 10);
	ASSERT_EQ_U(bytebuf_write(&b, data, 10), 10);
	chan_unit_bytebuf_free(&fixture, &b);

	ASSERT_NULL(b.data);
	ASSERT_EQ_U(b.used, 0);

	return chan_unit_finish(&fixture, TEST_PASS, false);
}

/* ================================================================
 * Accept queue
 * ================================================================ */

static int test_acceptqueue_init_free(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);
	ASSERT_EQ_U(q.count, 0);
	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_push_pop_single(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	const uint8_t type[] = "session";
	ASSERT_OK(acceptqueue_push(&q, 42, 65536, 32768,
	    type, sizeof(type) - 1));

	struct dssh_incoming_open entry;
	int pop_ret = acceptqueue_pop(&q, &entry);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ(entry.peer_channel, 42);
	ASSERT_EQ(entry.peer_window, 65536);
	ASSERT_EQ(entry.peer_max_packet, 32768);
	ASSERT_EQ_U(entry.channel_type_len, 7);
	ASSERT_STR_EQ(entry.channel_type, "session");

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_pop_empty(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	struct dssh_incoming_open entry;
	ASSERT_EQ(acceptqueue_pop(&q, &entry), -1);

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_fifo_order(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	ASSERT_OK(acceptqueue_push(&q, 1, 100, 50,
	    (const uint8_t *)"session", 7));
	ASSERT_OK(acceptqueue_push(&q, 2, 200, 100,
	    (const uint8_t *)"direct-tcpip", 12));
	ASSERT_OK(acceptqueue_push(&q, 3, 300, 150,
	    (const uint8_t *)"forwarded-tcpip", 15));

	struct dssh_incoming_open e;
	int pop_ret;

	pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ(e.peer_channel, 1);
	ASSERT_STR_EQ(e.channel_type, "session");

	pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ(e.peer_channel, 2);
	ASSERT_STR_EQ(e.channel_type, "direct-tcpip");

	pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ(e.peer_channel, 3);
	ASSERT_STR_EQ(e.channel_type, "forwarded-tcpip");

	/* Now empty */
	ASSERT_EQ(acceptqueue_pop(&q, &e), -1);

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_type_truncation(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	/* channel_type is 64 bytes -- push a 70-byte type */
	uint8_t longtype[70];
	memset(longtype, 'A', sizeof(longtype));

	ASSERT_OK(acceptqueue_push(&q, 0, 0, 0,
	    longtype, sizeof(longtype)));

	struct dssh_incoming_open e;
	int pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ_U(e.channel_type_len, 63);
	ASSERT_EQ_U(strlen(e.channel_type), 63);
	/* All chars should be 'A' */
	for (size_t i = 0; i < 63; i++)
		ASSERT_EQ(e.channel_type[i], 'A');
	ASSERT_EQ(e.channel_type[63], '\0');

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_exact_63_type(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	uint8_t type63[63];
	memset(type63, 'B', 63);

	ASSERT_OK(acceptqueue_push(&q, 5, 500, 250,
	    type63, 63));

	struct dssh_incoming_open e;
	int pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ_U(e.channel_type_len, 63);
	ASSERT_EQ_U(strlen(e.channel_type), 63);

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_empty_type(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	ASSERT_OK(acceptqueue_push(&q, 10, 1000, 500,
	    (const uint8_t *)"", 0));

	struct dssh_incoming_open e;
	int pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ_U(e.channel_type_len, 0);
	ASSERT_EQ(e.channel_type[0], '\0');

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_many_entries(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	/* Fill and drain from every head position. A refused ninth entry must
	 * leave the complete FIFO intact, including when its storage wraps. */
	uint32_t seq = 0;
	for (size_t batch = 0; batch < DSSH_ACCEPT_QUEUE_CAP; batch++) {
		const uint8_t advance[] = "advance";
		ASSERT_EQ(acceptqueue_push(&q, UINT32_MAX, 0, 0,
		    advance, sizeof(advance) - 1), 0);
		struct dssh_incoming_open skipped;
		ASSERT_EQ(acceptqueue_pop(&q, &skipped), 0);
		ASSERT_EQ_U(skipped.peer_channel, UINT32_MAX);
		ASSERT_EQ_U(q.head, (batch + 1) % DSSH_ACCEPT_QUEUE_CAP);
		ASSERT_EQ_U(q.count, 0);

		/* Fill to capacity */
		for (uint32_t i = 0; i < DSSH_ACCEPT_QUEUE_CAP; i++) {
			char type[16];
			int formatted = snprintf(type, sizeof(type), "ch%" PRIu32, seq + i);
			ASSERT_TRUE(formatted >= 0);
			uintmax_t formatted_size = (uintmax_t)formatted;
			ASSERT_TRUE(formatted_size < sizeof(type));
			ASSERT_OK(acceptqueue_push(&q, seq + i,
			    (seq + i) * 100, (seq + i) * 50,
			    (const uint8_t *)type, strlen(type)));
		}
		ASSERT_EQ_U(q.count, DSSH_ACCEPT_QUEUE_CAP);
		const uint8_t refused[] = "refused";
		ASSERT_ERR(acceptqueue_push(&q, UINT32_MAX, 1, 1,
		    refused, sizeof(refused) - 1), DSSH_ERROR_TOOMANY);
		ASSERT_EQ_U(q.count, DSSH_ACCEPT_QUEUE_CAP);

		/* Drain all */
		for (uint32_t i = 0; i < DSSH_ACCEPT_QUEUE_CAP; i++) {
			struct dssh_incoming_open e;
			int pop_ret = acceptqueue_pop(&q, &e);
			ASSERT_EQ(pop_ret, 0);
			ASSERT_EQ(e.peer_channel, seq + i);
			ASSERT_EQ(e.peer_window, (seq + i) * 100);
			ASSERT_EQ(e.peer_max_packet, (seq + i) * 50);
			ASSERT_NULL(e.protocol_id);

			char expected[16];
			int formatted = snprintf(expected, sizeof(expected), "ch%" PRIu32,
			    seq + i);
			ASSERT_TRUE(formatted >= 0);
			uintmax_t formatted_size = (uintmax_t)formatted;
			ASSERT_TRUE(formatted_size < sizeof(expected));
			ASSERT_STR_EQ(e.channel_type, expected);
		}

		seq += DSSH_ACCEPT_QUEUE_CAP;
	}

	struct dssh_incoming_open e;
	ASSERT_EQ(acceptqueue_pop(&q, &e), -1);

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_free_nonempty(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	ASSERT_OK(acceptqueue_push(&q, 1, 100, 50,
	    (const uint8_t *)"session", 7));
	ASSERT_OK(acceptqueue_push(&q, 2, 200, 100,
	    (const uint8_t *)"session", 7));

	acceptqueue_free(&q);
	ASSERT_EQ_U(q.count, 0);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_peer_values(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	/* Use max uint32_t values */
	ASSERT_OK(acceptqueue_push(&q, UINT32_MAX, UINT32_MAX,
	    UINT32_MAX, (const uint8_t *)"x", 1));

	struct dssh_incoming_open e;
	int pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ_U(e.peer_channel, UINT32_MAX);
	ASSERT_EQ_U(e.peer_window, UINT32_MAX);
	ASSERT_EQ_U(e.peer_max_packet, UINT32_MAX);

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_push_pop_interleaved(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	for (uint32_t i = 0; i < 10; i++) {
		ASSERT_OK(acceptqueue_push(&q, i, 0, 0,
		    (const uint8_t *)"s", 1));
		struct dssh_incoming_open e;
		int pop_ret = acceptqueue_pop(&q, &e);
		ASSERT_EQ(pop_ret, 0);
		ASSERT_EQ(e.peer_channel, i);
	}

	struct dssh_incoming_open e;
	ASSERT_EQ(acceptqueue_pop(&q, &e), -1);

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_type_nul_terminated(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	/* Preserve embedded zeros and append a terminator after all ten bytes. */
	uint8_t binary[10] = {65, 66, 67, 0, 68, 69, 70, 0, 71, 72};
	ASSERT_OK(acceptqueue_push(&q, 0, 0, 0, binary, 10));

	struct dssh_incoming_open e;
	int pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ_U(e.channel_type_len, 10);
	ASSERT_MEM_EQ(e.channel_type, binary, sizeof(binary));
	ASSERT_EQ(e.channel_type[sizeof(binary)], '\0');

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_zero_window_packet(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	ASSERT_OK(acceptqueue_push(&q, 0, 0, 0,
	    (const uint8_t *)"session", 7));

	struct dssh_incoming_open e;
	int pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ(e.peer_channel, 0);
	ASSERT_EQ(e.peer_window, 0);
	ASSERT_EQ(e.peer_max_packet, 0);

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_single_char_type(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	ASSERT_OK(acceptqueue_push(&q, 7, 1024, 512,
	    (const uint8_t *)"x", 1));

	struct dssh_incoming_open e;
	int pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ_U(e.channel_type_len, 1);
	ASSERT_STR_EQ(e.channel_type, "x");

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_acceptqueue_pop_does_not_affect_remaining(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	ASSERT_OK(acceptqueue_push(&q, 1, 100, 50,
	    (const uint8_t *)"alpha", 5));
	ASSERT_OK(acceptqueue_push(&q, 2, 200, 100,
	    (const uint8_t *)"bravo", 5));

	struct dssh_incoming_open e1;
	int pop_ret = acceptqueue_pop(&q, &e1);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_STR_EQ(e1.channel_type, "alpha");

	/* Second entry still intact */
	struct dssh_incoming_open e2;
	pop_ret = acceptqueue_pop(&q, &e2);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ(e2.peer_channel, 2);
	ASSERT_EQ(e2.peer_window, 200);
	ASSERT_EQ(e2.peer_max_packet, 100);
	ASSERT_STR_EQ(e2.channel_type, "bravo");

	acceptqueue_free(&q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}


/* ================================================================
 * RFC 4251 wire format: uint32 parse/serialize (formerly test_arch.c)
 * ================================================================ */

static int
test_parse_uint32_basic(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[] = { 0x01, 0x02, 0x03, 0x04 };
	uint32_t val = 0;
	int64_t ret = dssh_parse_uint32(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 4);
	ASSERT_EQ_U(val, 0x01020304);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_parse_uint32_zero(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x00 };
	uint32_t val = 1;
	ASSERT_EQ(dssh_parse_uint32(buf, sizeof(buf), &val), 4);
	ASSERT_EQ_U(val, 0);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_parse_uint32_max(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[] = { 0xFF, 0xFF, 0xFF, 0xFF };
	uint32_t val = 0;
	ASSERT_EQ(dssh_parse_uint32(buf, sizeof(buf), &val), 4);
	ASSERT_EQ_U(val, UINT32_MAX);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_parse_uint32_short_buffer(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[] = { 0x01, 0x02, 0x03 };
	uint32_t val = UINT32_C(0xa5a5a5a5);
	ASSERT_ERR(dssh_parse_uint32(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	ASSERT_EQ_U(val, UINT32_C(0xa5a5a5a5));
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_parse_uint32_empty_buffer(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[] = { 0 };
	uint32_t val = UINT32_C(0xa5a5a5a5);
	ASSERT_ERR(dssh_parse_uint32(buf, 0, &val), DSSH_ERROR_PARSE);
	ASSERT_EQ_U(val, UINT32_C(0xa5a5a5a5));
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_serialize_uint32(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[8] = { 0 };
	size_t pos = 0;
	ASSERT_EQ(dssh_serialize_uint32(0xDEADBEEF, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 4);
	uint8_t expected[] = { 0xDE, 0xAD, 0xBE, 0xEF };
	ASSERT_MEM_EQ(buf, expected, 4);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_serialize_uint32_overflow(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[3];
	size_t pos = 0;
	ASSERT_ERR(dssh_serialize_uint32(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_uint32_roundtrip(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[8];
	size_t pos = 0;
	uint32_t orig = 0x12345678;
	uint32_t parsed = 0;

	ASSERT_EQ(dssh_serialize_uint32(orig, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ(dssh_parse_uint32(buf, pos, &parsed), 4);
	ASSERT_EQ_U(parsed, orig);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_serialize_at_nonzero_offset(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[16] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
	size_t pos = 4;

	ASSERT_EQ(dssh_serialize_uint32(0xAABBCCDD, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 8);

	uint8_t expected_prefix[] = { 0xCC, 0xCC, 0xCC, 0xCC };
	ASSERT_MEM_EQ(buf, expected_prefix, 4);

	uint8_t expected_val[] = { 0xAA, 0xBB, 0xCC, 0xDD };
	ASSERT_MEM_EQ(&buf[4], expected_val, 4);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_serialize_overflow_at_offset(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[8];
	size_t pos = 6;

	ASSERT_ERR(dssh_serialize_uint32(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	ASSERT_EQ_U(pos, 6);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_serialize_pos_past_bufsz(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[8];
	size_t pos = 10;

	ASSERT_ERR(dssh_serialize_uint32(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	ASSERT_EQ_U(pos, 10);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_serialize_uint32_position_storage(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	const size_t capacity = sizeof(size_t[4]);
	uint8_t before[sizeof(size_t[4])];
	uint8_t *bytes = NULL;
	ASSERT_OK(chan_unit_alloc(&fixture, &bytes, capacity));
	ASSERT_NOT_NULL(bytes);
	size_t *pos = (size_t *)(bytes + sizeof(size_t));
	size_t owner_start = sizeof(size_t);
	size_t owner_end = owner_start + sizeof(*pos);
	const uint32_t values[] = {0, UINT32_C(0x12345678), UINT32_MAX};
	const uint8_t encodings[][4] = {
		{0, 0, 0, 0}, {0x12, 0x34, 0x56, 0x78}, {0xff, 0xff, 0xff, 0xff}
	};
	size_t rejected = 0;
	size_t accepted = 0;
	bool valid = true;
	/* Untyped allocated storage permits a real size_t object and disjoint
	 * uint8_t outputs even when uint8_t is not a character type. The offset
	 * is one sizeof(size_t) stride from the malloc-aligned base. */
	for (size_t v = 0; valid && v < sizeof(values) / sizeof(values[0]); v++) {
		for (size_t offset = 0; valid && offset <= capacity - 4; offset++) {
			memset(bytes, 0xa5, capacity);
			*pos = offset;
			memcpy(before, bytes, capacity);
			int ret = dssh_serialize_uint32(values[v], bytes, capacity, pos);
			/* Known allocation offsets give the oracle exact bounds, without
			 * depending on byte order or the production pointer comparisons. */
			if (offset < owner_end && offset + 4 > owner_start) {
				valid = ret == DSSH_ERROR_INVALID && *pos == offset
				    && memcmp(bytes, before, capacity) == 0;
				rejected++;
				continue;
			}
			valid = ret == 0 && *pos == offset + 4
			    && memcmp(bytes + offset, encodings[v], 4) == 0;
			for (size_t i = 0; valid && i < capacity; i++) {
				if ((i < offset || i >= offset + 4)
				    && (i < owner_start || i >= owner_end))
					valid = bytes[i] == before[i];
			}
			accepted++;
		}
	}
	chan_unit_free(&fixture, &bytes);
	ASSERT_TRUE(valid);
	ASSERT_TRUE(rejected > 0);
	ASSERT_TRUE(accepted > 0);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_serialize_uint32_bounds(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t bytes[12];
	uint8_t expected[sizeof(bytes)];
	const uint8_t encoded[] = {0x12, 0x34, 0x56, 0x78};
	for (size_t capacity = 0; capacity <= sizeof(bytes); capacity++) {
		for (size_t offset = 0; offset <= sizeof(bytes) + 4; offset++) {
			memset(bytes, 0xa5, sizeof(bytes));
			memcpy(expected, bytes, sizeof(bytes));
			size_t pos = offset;
			int ret = dssh_serialize_uint32(UINT32_C(0x12345678), bytes, capacity, &pos);
			if (offset > capacity || capacity - offset < 4) {
				ASSERT_EQ(ret, DSSH_ERROR_TOOLONG);
				ASSERT_EQ_U(pos, offset);
			}
			else {
				ASSERT_OK(ret);
				ASSERT_EQ_U(pos, offset + 4);
				memcpy(expected + offset, encoded, sizeof(encoded));
			}
			ASSERT_MEM_EQ(bytes, expected, sizeof(bytes));
		}
	}
	size_t pos = SIZE_MAX;
	memset(bytes, 0xa5, sizeof(bytes));
	memcpy(expected, bytes, sizeof(bytes));
	ASSERT_EQ(dssh_serialize_uint32(0, bytes, sizeof(bytes), &pos), DSSH_ERROR_TOOLONG);
	ASSERT_EQ_U(pos, SIZE_MAX);
	ASSERT_MEM_EQ(bytes, expected, sizeof(bytes));
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_parse_uint32_overlapping_output(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	const size_t capacity = sizeof(uint32_t[4]);
	uint8_t *bytes = NULL;
	ASSERT_OK(chan_unit_alloc(&fixture, &bytes, capacity));
	ASSERT_NOT_NULL(bytes);
	uint32_t *value = (uint32_t *)(bytes + sizeof(uint32_t));
	const uint8_t encoded[] = {0x12, 0x34, 0x56, 0x78};
	bool valid = true;
	for (size_t offset = 0; valid && offset <= capacity - 4; offset++) {
		memset(bytes, 0, capacity);
		memcpy(bytes + offset, encoded, sizeof(encoded));
		int64_t ret = dssh_parse_uint32(bytes + offset, 4, value);
		valid = ret == 4 && *value == UINT32_C(0x12345678);
	}
	chan_unit_free(&fixture, &bytes);
	ASSERT_TRUE(valid);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_cleanse_basic(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[16];

	memset(buf, 0xAA, sizeof(buf));
	dssh_cleanse(buf, sizeof(buf));

	for (size_t i = 0; i < sizeof(buf); i++)
		ASSERT_EQ(buf[i], 0);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_cleanse_null(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	dssh_cleanse(NULL, 0);
	dssh_cleanse(NULL, 100);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_cleanse_zero_len(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[4] = {0xAA, 0xBB, 0xCC, 0xDD};

	dssh_cleanse(buf, 0);

	ASSERT_EQ(buf[0], 0xAA);
	ASSERT_EQ(buf[3], 0xDD);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_parse_uint32_null(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[4] = {0, 0, 0, 1};
	uint32_t val;

	ASSERT_EQ(dssh_parse_uint32(buf, 4, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_parse_uint32(NULL, 4, &val), DSSH_ERROR_INVALID);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_serialize_uint32_null(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	uint8_t buf[4];
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_uint32(1, buf, 4, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_serialize_uint32(1, NULL, 4, &pos), DSSH_ERROR_INVALID);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

/* ================================================================
 * Params builder tests
 * ================================================================ */

static int test_params_init_shell(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SHELL));
	ASSERT_EQ(p.type, DSSH_CHAN_SHELL);
	ASSERT_TRUE(p.flags & DSSH_PARAM_HAS_PTY);
	ASSERT_STR_EQ(p.term, "dumb");
	ASSERT_EQ_U(p.cols, 0);
	ASSERT_EQ_U(p.max_window, 0);
	ASSERT_NULL(p.command);
	ASSERT_NULL(p.subsystem);
	ASSERT_NULL(p.env);
	ASSERT_EQ_U(p.env_count, 0);
	ASSERT_NULL(p.modes);
	ASSERT_EQ_U(p.mode_count, 0);
	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_init_exec(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC));
	ASSERT_EQ(p.type, DSSH_CHAN_EXEC);
	ASSERT_FALSE(p.flags & DSSH_PARAM_HAS_PTY);
	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_init_subsystem(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SUBSYSTEM));
	ASSERT_EQ(p.type, DSSH_CHAN_SUBSYSTEM);
	ASSERT_FALSE(p.flags & DSSH_PARAM_HAS_PTY);
	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_set_pty(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC));
	ASSERT_FALSE(p.flags & DSSH_PARAM_HAS_PTY);

	ASSERT_OK(dssh_chan_params_set_pty(&p, true));
	ASSERT_TRUE(p.flags & DSSH_PARAM_HAS_PTY);

	ASSERT_OK(dssh_chan_params_set_pty(&p, false));
	ASSERT_FALSE(p.flags & DSSH_PARAM_HAS_PTY);

	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_set_accept_early_data(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SHELL));
	ASSERT_FALSE(p.flags & DSSH_PARAM_ACCEPT_EARLY_DATA);

	ASSERT_OK(dssh_chan_params_set_accept_early_data(&p, true));
	ASSERT_TRUE(p.flags & DSSH_PARAM_ACCEPT_EARLY_DATA);

	ASSERT_OK(dssh_chan_params_set_accept_early_data(&p, false));
	ASSERT_FALSE(p.flags & DSSH_PARAM_ACCEPT_EARLY_DATA);

	/* NULL params */
	ASSERT_EQ(dssh_chan_params_set_accept_early_data(NULL, true),
	    DSSH_ERROR_INVALID);

	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_set_term(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SHELL));
	ASSERT_OK(dssh_chan_params_set_term(&p, "xterm-256color"));
	ASSERT_STR_EQ(p.term, "xterm-256color");
	ASSERT_OK(dssh_chan_params_set_term(&p, p.term));
	ASSERT_STR_EQ(p.term, "xterm-256color");
	ASSERT_OK(dssh_chan_params_set_term(&p, p.term + 6));
	ASSERT_STR_EQ(p.term, "256color");

	/* Long term string preserved in full (dynamic allocation) */
	char longterm[128];
	memset(longterm, 'x', sizeof(longterm) - 1);
	longterm[sizeof(longterm) - 1] = '\0';
	ASSERT_OK(dssh_chan_params_set_term(&p, longterm));
	ASSERT_EQ_U(strlen(p.term), 127);

	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_set_size(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SHELL));
	ASSERT_OK(dssh_chan_params_set_size(&p, 80, 24, 640, 480));
	ASSERT_EQ_U(p.cols, 80);
	ASSERT_EQ_U(p.rows, 24);
	ASSERT_EQ_U(p.wpx, 640);
	ASSERT_EQ_U(p.hpx, 480);
	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_set_command(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC));
	ASSERT_OK(dssh_chan_params_set_command(&p, "ls -la"));
	ASSERT_NOT_NULL(p.command);
	ASSERT_STR_EQ(p.command, "ls -la");

	/* Replace */
	ASSERT_OK(dssh_chan_params_set_command(&p, "uname -a"));
	ASSERT_STR_EQ(p.command, "uname -a");
	ASSERT_OK(dssh_chan_params_set_command(&p, p.command));
	ASSERT_STR_EQ(p.command, "uname -a");
	ASSERT_OK(dssh_chan_params_set_command(&p, p.command + 6));
	ASSERT_STR_EQ(p.command, "-a");

	chan_unit_params_free(&fixture, &p);
	ASSERT_NULL(p.command);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_set_subsystem(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SUBSYSTEM));
	ASSERT_OK(dssh_chan_params_set_subsystem(&p, "sftp"));
	ASSERT_STR_EQ(p.subsystem, "sftp");
	ASSERT_OK(dssh_chan_params_set_subsystem(&p, p.subsystem));
	ASSERT_STR_EQ(p.subsystem, "sftp");
	ASSERT_OK(dssh_chan_params_set_subsystem(&p, p.subsystem + 1));
	ASSERT_STR_EQ(p.subsystem, "ftp");
	chan_unit_params_free(&fixture, &p);
	ASSERT_NULL(p.subsystem);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_add_env(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SHELL));

	ASSERT_OK(dssh_chan_params_add_env(&p, "LANG", "en_US.UTF-8"));
	ASSERT_OK(dssh_chan_params_add_env(&p, "TERM", "xterm"));
	ASSERT_OK(dssh_chan_params_add_env(&p, "FOO", "bar"));

	ASSERT_EQ_U(p.env_count, 3);
	ASSERT_STR_EQ(p.env[0].name, "LANG");
	ASSERT_STR_EQ(p.env[0].value, "en_US.UTF-8");
	ASSERT_STR_EQ(p.env[1].name, "TERM");
	ASSERT_STR_EQ(p.env[1].value, "xterm");
	ASSERT_STR_EQ(p.env[2].name, "FOO");
	ASSERT_STR_EQ(p.env[2].value, "bar");

	chan_unit_params_free(&fixture, &p);
	ASSERT_NULL(p.env);
	ASSERT_EQ_U(p.env_count, 0);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_params_env_failure_atomic(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;
	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC));
	for (int n = 0; n < 3; n++) {
		dssh_test_alloc_fail_after(n);
		int ret = dssh_chan_params_add_env(&p, "name", "value");
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
		ASSERT_EQ_U(p.env_count, 0);
		ASSERT_EQ_U(p.env_capacity, 0);
		ASSERT_NULL(p.env);
	}
	for (int n = 0; n < 4; n++)
		ASSERT_OK(dssh_chan_params_add_env(&p, "name", "value"));
	for (int n = 0; n < 3; n++) {
		struct dssh_chan_env *original = p.env;
		dssh_test_alloc_fail_after(n);
		int ret = dssh_chan_params_add_env(&p, p.env[0].name, p.env[0].value);
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
		ASSERT_EQ_U(p.env_count, 4);
		ASSERT_EQ_U(p.env_capacity, 4);
		ASSERT_TRUE(p.env == original);
		ASSERT_STR_EQ(p.env[0].name, "name");
		ASSERT_STR_EQ(p.env[0].value, "value");
	}
	ASSERT_OK(dssh_chan_params_add_env(&p, p.env[0].name, p.env[0].value));
	ASSERT_EQ_U(p.env_count, 5);
	ASSERT_STR_EQ(p.env[4].name, "name");
	ASSERT_STR_EQ(p.env[4].value, "value");
	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

struct env_decision {
	int result;
	size_t calls;
	size_t count;
};

static int
record_env_decision(dssh_channel ch, const struct dssh_chan_params *p, void *cbdata)
{
	(void)ch;
	struct env_decision *decision = cbdata;
	decision->calls++;
	decision->count = p->env_count;
	return decision->result;
}

static int
test_accept_env_validation(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	bool cancelled = false;
	const uint8_t valid[] = { 0, 0, 0, 1, 'A', 0, 0, 0, 1, 'B' };
	uint8_t packet[sizeof(valid) + 1];
	struct dssh_chan_params p;
	struct env_decision decision = { .result = DSSH_ERROR_REJECTED };
	struct dssh_chan_accept_cbs cbs = {
		.env = record_env_decision,
		.cbdata = &decision,
	};
	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC));
	for (size_t len = 0; len < sizeof(valid); len++)
		ASSERT_EQ(accept_env_request(NULL, valid, len, &p, &cbs, &cancelled), DSSH_ERROR_PARSE);
	memcpy(packet, valid, sizeof(valid));
	packet[sizeof(valid)] = 1;
	ASSERT_EQ(accept_env_request(NULL, packet, sizeof(packet), &p, &cbs, &cancelled), DSSH_ERROR_PARSE);
	packet[4] = 0;
	ASSERT_EQ(accept_env_request(NULL, packet, sizeof(valid), &p, &cbs, &cancelled), DSSH_ERROR_PARSE);
	memcpy(packet, valid, sizeof(valid));
	packet[9] = 0;
	ASSERT_EQ(accept_env_request(NULL, packet, sizeof(valid), &p, &cbs, &cancelled), DSSH_ERROR_PARSE);
	static const uint32_t large_lengths[] = {UINT32_C(65536), UINT32_C(65537), UINT32_MAX};
	for (size_t field = 0; field < 2; field++) {
		for (size_t i = 0; i < sizeof(large_lengths) / sizeof(large_lengths[0]); i++) {
			memcpy(packet, valid, sizeof(valid));
			size_t offset = field == 0 ? 0 : 5;
			DSSH_PUT_U32(large_lengths[i], packet, &offset);
			dssh_test_alloc_fail_after(0);
			int result = accept_env_request(NULL, packet, sizeof(valid), &p, &cbs, &cancelled);
			int allocations = dssh_test_alloc_count();
			dssh_test_alloc_reset();
			ASSERT_EQ(result, DSSH_ERROR_PARSE);
			ASSERT_EQ(allocations, 0);
		}
	}
	ASSERT_EQ_U(decision.calls, 0);
	ASSERT_EQ_U(p.env_count, 0);

	/* Every rejected request releases its provisional entry. */
	for (int n = 0; n < 256; n++) {
		ASSERT_EQ(accept_env_request(NULL, valid, sizeof(valid), &p, &cbs, &cancelled), DSSH_ERROR_REJECTED);
		ASSERT_EQ_U(p.env_count, 0);
		ASSERT_EQ_U(decision.count, 1);
	}
	decision.result = 0;
	for (int n = 0; n < 128; n++)
		ASSERT_OK(accept_env_request(NULL, valid, sizeof(valid), &p, &cbs, &cancelled));
	ASSERT_EQ(accept_env_request(NULL, valid, sizeof(valid), &p, &cbs, &cancelled), DSSH_ERROR_TOOMANY);
	ASSERT_EQ_U(p.env_count, 128);
	ASSERT_EQ_U(decision.calls, 384);
	ASSERT_STR_EQ(p.env[0].name, "A");
	ASSERT_STR_EQ(p.env[127].value, "B");
	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_accept_env_byte_limit(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	bool cancelled = false;
#if SIZE_MAX < 1048576
	size_t limit = SIZE_MAX;
#else
	size_t limit = 1048576;
#endif
	/* Each retained entry includes a one-byte name and two terminators. */
	size_t value_len = limit / 2 - 3;
	ASSERT_TRUE(value_len <= SIZE_MAX - 9);
	ASSERT_TRUE(value_len <= UINT32_MAX);
	size_t packet_len = value_len + 9;
	uint32_t wire_len = (uint32_t)value_len;
	uint8_t *packet = NULL;
	ASSERT_OK(chan_unit_alloc(&fixture, &packet, packet_len));
	ASSERT_NOT_NULL(packet);
	size_t pos = 0;
	ASSERT_OK(dssh_serialize_uint32(1, packet, packet_len, &pos));
	packet[pos++] = 'A';
	ASSERT_OK(dssh_serialize_uint32(wire_len, packet, packet_len, &pos));
	memset(&packet[pos], 'B', value_len);
	struct dssh_chan_params p;
	struct env_decision decision = { 0 };
	struct dssh_chan_accept_cbs cbs = {
		.env = record_env_decision,
		.cbdata = &decision,
	};
	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC));
	ASSERT_OK(accept_env_request(NULL, packet, packet_len, &p, &cbs, &cancelled));
	ASSERT_OK(accept_env_request(NULL, packet, packet_len, &p, &cbs, &cancelled));
	ASSERT_EQ(accept_env_request(NULL, packet, packet_len, &p, &cbs, &cancelled), DSSH_ERROR_TOOLONG);
	ASSERT_EQ_U(decision.calls, 2);
	ASSERT_EQ_U(p.env_count, 2);
	chan_unit_params_free(&fixture, &p);
	chan_unit_free(&fixture, &packet);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_accept_string_validation(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	const uint8_t valid[] = { 0, 0, 0, 3, 'a', 'b', 'c' };
	const uint8_t empty[] = { 0, 0, 0, 0 };
	const uint8_t large[] = { 255, 255, 255, 255 };
	uint8_t packet[sizeof(valid) + 1];
	struct dssh_chan_params p;
	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC));
	ASSERT_OK(dssh_chan_params_set_command(&p, "original"));
	ASSERT_OK(dssh_chan_params_set_subsystem(&p, "original"));
	for (size_t len = 0; len < sizeof(valid); len++) {
		ASSERT_EQ(accept_parse_exec(valid, len, &p), DSSH_ERROR_PARSE);
		ASSERT_EQ(accept_parse_subsystem(valid, len, &p), DSSH_ERROR_PARSE);
	}
	ASSERT_EQ(accept_parse_exec(large, sizeof(large), &p), DSSH_ERROR_PARSE);
	ASSERT_EQ(accept_parse_subsystem(large, sizeof(large), &p), DSSH_ERROR_PARSE);
	memcpy(packet, valid, sizeof(valid));
	packet[sizeof(valid)] = 1;
	ASSERT_EQ(accept_parse_exec(packet, sizeof(packet), &p), DSSH_ERROR_PARSE);
	ASSERT_EQ(accept_parse_subsystem(packet, sizeof(packet), &p), DSSH_ERROR_PARSE);
	packet[5] = 0;
	ASSERT_EQ(accept_parse_exec(packet, sizeof(valid), &p), DSSH_ERROR_PARSE);
	ASSERT_EQ(accept_parse_subsystem(packet, sizeof(valid), &p), DSSH_ERROR_PARSE);
	for (int which = 0; which < 2; which++) {
		dssh_test_alloc_fail_after(0);
		int ret = which == 0 ? accept_parse_exec(valid, sizeof(valid), &p)
		    : accept_parse_subsystem(valid, sizeof(valid), &p);
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	}
	ASSERT_STR_EQ(p.command, "original");
	ASSERT_STR_EQ(p.subsystem, "original");
	ASSERT_OK(accept_parse_exec(valid, sizeof(valid), &p));
	ASSERT_OK(accept_parse_subsystem(valid, sizeof(valid), &p));
	ASSERT_STR_EQ(p.command, "abc");
	ASSERT_STR_EQ(p.subsystem, "abc");
	ASSERT_OK(accept_parse_exec(empty, sizeof(empty), &p));
	ASSERT_OK(accept_parse_subsystem(empty, sizeof(empty), &p));
	ASSERT_STR_EQ(p.command, "");
	ASSERT_STR_EQ(p.subsystem, "");
	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_accept_pty_validation(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	/* TERM="x", 80x24 characters, 640x480 pixels, one mode and END. */
	const uint8_t valid[] = {
		0, 0, 0, 1, 'x',
		0, 0, 0, 80, 0, 0, 0, 24,
		0, 0, 2, 128, 0, 0, 1, 224,
		0, 0, 0, 6, 53, 0, 0, 0, 1, 0,
	};
	uint8_t packet[sizeof(valid) + 1];
	struct dssh_chan_params p;
	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC));
	ASSERT_OK(dssh_chan_params_set_mode(&p, 1, 42));
	for (size_t len = 0; len < sizeof(valid); len++)
		ASSERT_EQ(accept_parse_pty(valid, len, &p), DSSH_ERROR_PARSE);
	memcpy(packet, valid, sizeof(valid));
	packet[sizeof(valid)] = 1;
	ASSERT_EQ(accept_parse_pty(packet, sizeof(packet), &p), DSSH_ERROR_PARSE);
	packet[4] = 0;
	ASSERT_EQ(accept_parse_pty(packet, sizeof(valid), &p), DSSH_ERROR_PARSE);
	memcpy(packet, valid, sizeof(valid));
	/* Missing opcode argument and missing stream terminator. */
	for (uint8_t n = 1; n <= 5; n++) {
		packet[24] = n;
		size_t len = 25 + n;
		ASSERT_EQ(accept_parse_pty(packet, len, &p), DSSH_ERROR_PARSE);
	}
	static const uint32_t large_terms[] = {UINT32_C(65536), UINT32_C(65537), UINT32_MAX};
	for (size_t i = 0; i < sizeof(large_terms) / sizeof(large_terms[0]); i++) {
		memcpy(packet, valid, sizeof(valid));
		size_t offset = 0;
		DSSH_PUT_U32(large_terms[i], packet, &offset);
		dssh_test_alloc_fail_after(0);
		int result = accept_parse_pty(packet, sizeof(valid), &p);
		int attempts = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		ASSERT_EQ(result, DSSH_ERROR_PARSE);
		ASSERT_EQ(attempts, 0);
		ASSERT_STR_EQ(p.term, "dumb");
		ASSERT_EQ_U(p.mode_count, 1);
	}
	for (int n = 0; n < 2; n++) {
		dssh_test_alloc_fail_after(n);
		int ret = accept_parse_pty(valid, sizeof(valid), &p);
		dssh_test_alloc_reset();
		ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
		ASSERT_STR_EQ(p.term, "dumb");
		ASSERT_EQ_U(p.mode_count, 1);
		ASSERT_EQ(p.modes[0].opcode, 1);
		ASSERT_EQ_U(p.modes[0].value, 42);
		ASSERT_FALSE(p.flags & DSSH_PARAM_HAS_PTY);
	}
	ASSERT_OK(accept_parse_pty(valid, sizeof(valid), &p));
	ASSERT_STR_EQ(p.term, "x");
	ASSERT_EQ_U(p.cols, 80);
	ASSERT_EQ_U(p.rows, 24);
	ASSERT_EQ_U(p.wpx, 640);
	ASSERT_EQ_U(p.hpx, 480);
	ASSERT_EQ_U(p.mode_count, 1);
	ASSERT_EQ(p.modes[0].opcode, 53);
	ASSERT_EQ_U(p.modes[0].value, 1);
	ASSERT_TRUE(p.flags & DSSH_PARAM_HAS_PTY);
	memcpy(packet, valid, sizeof(valid));
	/* Future opcode stops parsing; bytes after it are uninterpreted. */
	packet[25] = 160;
	ASSERT_OK(accept_parse_pty(packet, sizeof(valid), &p));
	ASSERT_EQ_U(p.mode_count, 0);
	/* END has the same stopping behavior. */
	packet[25] = 0;
	ASSERT_OK(accept_parse_pty(packet, sizeof(valid), &p));
	ASSERT_EQ_U(p.mode_count, 0);
	/* An empty mode string supplies no modes. */
	packet[24] = 0;
	ASSERT_OK(accept_parse_pty(packet, 25, &p));
	ASSERT_EQ_U(p.mode_count, 0);
	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_set_mode(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SHELL));

	/* Add several modes */
	ASSERT_OK(dssh_chan_params_set_mode(&p, 1, 100));
	ASSERT_OK(dssh_chan_params_set_mode(&p, 42, 200));
	ASSERT_OK(dssh_chan_params_set_mode(&p, 160, 300));

	ASSERT_EQ_U(p.mode_count, 3);
	ASSERT_EQ(p.modes[0].opcode, 1);
	ASSERT_EQ_U(p.modes[0].value, 100);
	ASSERT_EQ(p.modes[1].opcode, 42);
	ASSERT_EQ_U(p.modes[1].value, 200);
	ASSERT_EQ(p.modes[2].opcode, 160);
	ASSERT_EQ_U(p.modes[2].value, 300);

	chan_unit_params_free(&fixture, &p);
	ASSERT_NULL(p.modes);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_mode_dedup(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;

	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SHELL));

	ASSERT_OK(dssh_chan_params_set_mode(&p, 1, 100));
	ASSERT_OK(dssh_chan_params_set_mode(&p, 42, 200));
	/* Overwrite opcode 1 -- last-wins */
	ASSERT_OK(dssh_chan_params_set_mode(&p, 1, 999));

	ASSERT_EQ_U(p.mode_count, 2);
	ASSERT_EQ(p.modes[0].opcode, 1);
	ASSERT_EQ_U(p.modes[0].value, 999);
	ASSERT_EQ(p.modes[1].opcode, 42);
	ASSERT_EQ_U(p.modes[1].value, 200);

	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_params_mode_domain(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params params;
	ASSERT_OK(chan_unit_params_init(&fixture, &params, DSSH_CHAN_SHELL));
	for (uint16_t code = 1; code <= UINT8_MAX; code++) {
		uint8_t opcode = (uint8_t)code;
		ASSERT_OK(dssh_chan_params_set_mode(&params, opcode, UINT32_MAX));
	}
	ASSERT_EQ_U(params.mode_count, UINT8_MAX);
	struct dssh_chan_mode_entry *storage = params.modes;
	size_t capacity = params.mode_capacity;
	dssh_test_alloc_fail_after(0);
	bool valid = true;
	for (uint16_t code = 1; code <= UINT8_MAX; code++) {
		uint8_t opcode = (uint8_t)code;
		int result = dssh_chan_params_set_mode(&params, opcode, code);
		valid = result == 0 && params.modes[code - 1].opcode == opcode
		    && params.modes[code - 1].value == code && valid;
	}
	int result = dssh_chan_params_set_mode(&params, 0, UINT32_MAX);
	valid = valid && result == DSSH_ERROR_INVALID && dssh_test_alloc_count() == 0
	    && params.modes == storage && params.mode_capacity == capacity && params.mode_count == UINT8_MAX;
	dssh_test_alloc_reset();
	chan_unit_params_free(&fixture, &params);
	ASSERT_TRUE(valid);
	printf("terminal_mode_domain opcodes=255 updates=255 allocations=0 valid=1\n");
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_free_null(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	/* Must not crash */
	chan_unit_params_free(&fixture, NULL);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_init_null(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	ASSERT_ERR(chan_unit_params_init(&fixture, NULL, DSSH_CHAN_SHELL),
	    DSSH_ERROR_INVALID);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int test_params_set_max_window(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;
	ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_SHELL));

	/* Default is 0 (library default) */
	ASSERT_EQ_U(p.max_window, 0);

	/* Set custom value */
	ASSERT_OK(dssh_chan_params_set_max_window(&p, 65536));
	ASSERT_EQ_U(p.max_window, 65536);

	/* Set to 0 (back to default) */
	ASSERT_OK(dssh_chan_params_set_max_window(&p, 0));
	ASSERT_EQ_U(p.max_window, 0);

	/* Set to UINT32_MAX */
	ASSERT_OK(dssh_chan_params_set_max_window(&p, UINT32_MAX));
	ASSERT_EQ_U(p.max_window, UINT32_MAX);

	/* NULL returns DSSH_ERROR_INVALID */
	ASSERT_ERR(dssh_chan_params_set_max_window(NULL, 1000),
	    DSSH_ERROR_INVALID);

	chan_unit_params_free(&fixture, &p);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

/* ================================================================
 * Event queue cap
 * ================================================================ */

static int
test_event_queue_cap_enforced(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_event_queue q;
	int res = chan_unit_event_init(&fixture, &q, 4, 32768);

	ASSERT_EQ(res, 0);

	struct dssh_event_entry ev = { .type = 1 };

	for (int i = 0; i < 4; i++) {
		res = event_queue_push(&q, &ev);
		ASSERT_EQ(res, 0);
	}

	/* 5th push should fail */
	res = event_queue_push(&q, &ev);
	ASSERT_EQ(res, DSSH_ERROR_TOOMANY);

	/* Queue still has 4 entries */
	ASSERT_EQ_U(q.count, 4);

	chan_unit_event_free(&fixture, &q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_event_queue_no_cap(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_event_queue q;
	int res = chan_unit_event_init(&fixture, &q, 0, 32768);

	ASSERT_EQ(res, 0);

	struct dssh_event_entry ev = { .type = 1 };

	/* Zero removes the event-count cap; the separate memory bound remains.
	 * This modest case grows past the initial capacity of eight. */
	for (int i = 0; i < 16; i++) {
		res = event_queue_push(&q, &ev);
		ASSERT_EQ(res, 0);
	}

	ASSERT_EQ_U(q.count, 16);

	chan_unit_event_free(&fixture, &q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

static int
test_event_queue_cap_at_initial_capacity(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	/* Cap exactly at initial capacity (8) — no realloc needed */
	struct dssh_event_queue q;
	int res = chan_unit_event_init(&fixture, &q, 8, 32768);

	ASSERT_EQ(res, 0);

	struct dssh_event_entry ev = { .type = 1 };

	for (int i = 0; i < 8; i++) {
		res = event_queue_push(&q, &ev);
		ASSERT_EQ(res, 0);
	}

	res = event_queue_push(&q, &ev);
	ASSERT_EQ(res, DSSH_ERROR_TOOMANY);

	chan_unit_event_free(&fixture, &q);
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

/* ================================================================
 * Test table
 * ================================================================ */

static int
test_params_copy_failure(void)
{
	struct chan_unit_fixture fixture;
	chan_unit_begin(&fixture, __func__);
	struct dssh_chan_params p;
	dssh_test_alloc_fail_after(0);
	int ret = chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC);
	int count = dssh_test_alloc_count();
	dssh_test_alloc_reset();
	chan_unit_params_free(&fixture, &p);
	ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
	ASSERT_EQ(count, 1);
	for (unsigned field = 0; field < 3; field++) {
		ASSERT_OK(chan_unit_params_init(&fixture, &p, DSSH_CHAN_EXEC));
		ASSERT_OK(dssh_chan_params_set_term(&p, "old-term"));
		ASSERT_OK(dssh_chan_params_set_command(&p, "old-command"));
		ASSERT_OK(dssh_chan_params_set_subsystem(&p, "old-subsystem"));
		const char *old = field == 0 ? p.term : field == 1 ? p.command : p.subsystem;
		dssh_test_alloc_fail_after(0);
		ret = field == 0 ? dssh_chan_params_set_term(&p, "replacement")
		    : field == 1 ? dssh_chan_params_set_command(&p, "replacement")
		    : dssh_chan_params_set_subsystem(&p, "replacement");
		count = dssh_test_alloc_count();
		dssh_test_alloc_reset();
		const char *current = field == 0 ? p.term : field == 1 ? p.command : p.subsystem;
		bool intact = current == old && strcmp(current,
		    field == 0 ? "old-term" : field == 1 ? "old-command" : "old-subsystem") == 0;
		chan_unit_params_free(&fixture, &p);
		ASSERT_EQ(ret, DSSH_ERROR_ALLOC);
		ASSERT_EQ(count, 1);
		ASSERT_TRUE(intact);
	}
	return chan_unit_finish(&fixture, TEST_PASS, false);
}

#include "event_text_queue_cases.h"
#include "bytebuf_ring_cases.h"
#include "channel_unit_owner_cases.h"

static struct dssh_test_entry tests[] = {
	{ "params_copy_failure", test_params_copy_failure },
	/* Bytebuf */
	{ "bytebuf_ring_positions", test_bytebuf_ring_positions },
	{ "bytebuf_init_free",                test_bytebuf_init_free },
	{ "bytebuf_write_read_basic",         test_bytebuf_write_read_basic },
	{ "bytebuf_empty_read",               test_bytebuf_empty_read },
	{ "bytebuf_write_full",               test_bytebuf_write_full },
	{ "bytebuf_partial_write",            test_bytebuf_partial_write },
	{ "bytebuf_free_space",               test_bytebuf_free_space },
	{ "bytebuf_total_monotonic",          test_bytebuf_total_monotonic },
	{ "bytebuf_wrap_around",              test_bytebuf_wrap_around },
	{ "bytebuf_wrap_write_split",         test_bytebuf_wrap_write_split },
	{ "bytebuf_read_limit",               test_bytebuf_read_limit },
	{ "bytebuf_read_limit_vs_bufsz",      test_bytebuf_read_limit_vs_bufsz },
	{ "bytebuf_read_limit_zero",          test_bytebuf_read_limit_zero },
	{ "bytebuf_multiple_cycles",          test_bytebuf_multiple_write_read_cycles },
	{ "bytebuf_wrap_read_split",          test_bytebuf_wrap_read_split },
	{ "bytebuf_zero_len_write",           test_bytebuf_zero_len_write },
	{ "bytebuf_single_byte_ops",          test_bytebuf_single_byte_ops },
	{ "bytebuf_exact_capacity",           test_bytebuf_exact_capacity },
	{ "bytebuf_read_limit_less_avail",    test_bytebuf_read_limit_less_than_available },
	{ "bytebuf_fill_drain_refill",        test_bytebuf_fill_drain_refill },
	{ "bytebuf_avail_after_partial_read", test_bytebuf_available_after_partial_read },
	{ "bytebuf_total_survives_wrap",      test_bytebuf_total_survives_wrap },
	{ "bytebuf_read_limit_with_wrap",     test_bytebuf_read_limit_with_wrap },
	{ "bytebuf_partial_write_at_wrap",    test_bytebuf_partial_write_at_wrap },
	{ "bytebuf_head_tail_reset_free",     test_bytebuf_head_tail_reset_after_free },

	/* Msgqueue */
	{ "acceptqueue_init_free",            test_acceptqueue_init_free },
	{ "acceptqueue_push_pop_single",      test_acceptqueue_push_pop_single },
	{ "acceptqueue_pop_empty",            test_acceptqueue_pop_empty },
	{ "acceptqueue_fifo_order",           test_acceptqueue_fifo_order },
	{ "acceptqueue_type_truncation",      test_acceptqueue_type_truncation },
	{ "acceptqueue_exact_63_type",        test_acceptqueue_exact_63_type },
	{ "acceptqueue_empty_type",           test_acceptqueue_empty_type },
	{ "acceptqueue_many_entries",         test_acceptqueue_many_entries },
	{ "acceptqueue_free_nonempty",        test_acceptqueue_free_nonempty },
	{ "acceptqueue_peer_values",          test_acceptqueue_peer_values },
	{ "acceptqueue_push_pop_interleaved", test_acceptqueue_push_pop_interleaved },
	{ "acceptqueue_type_nul_terminated",  test_acceptqueue_type_nul_terminated },
	{ "acceptqueue_zero_window_packet",   test_acceptqueue_zero_window_packet },
	{ "acceptqueue_single_char_type",     test_acceptqueue_single_char_type },
	{ "acceptqueue_pop_no_affect_rest",   test_acceptqueue_pop_does_not_affect_remaining },

	/* Event queue */
	{ "event_text_queue_lifetime", test_event_text_queue_lifetime },
	{ "event_text_queue_rollback", test_event_text_queue_rollback },
	{ "event_text_queue_envelope", test_event_text_queue_envelope },
	{ "event_queue_cap_enforced",         test_event_queue_cap_enforced },
	{ "event_queue_no_cap",               test_event_queue_no_cap },
	{ "event_queue_cap_at_initial",       test_event_queue_cap_at_initial_capacity },

	/* Coverage tests */

	/* uint32 wire format */
	{ "parse_uint32_basic",             test_parse_uint32_basic },
	{ "parse_uint32_zero",              test_parse_uint32_zero },
	{ "parse_uint32_max",               test_parse_uint32_max },
	{ "parse_uint32_short_buffer",      test_parse_uint32_short_buffer },
	{ "parse_uint32_empty_buffer",      test_parse_uint32_empty_buffer },
	{ "serialize_uint32",               test_serialize_uint32 },
	{ "serialize_uint32_overflow",      test_serialize_uint32_overflow },
	{ "parse_uint32_null",              test_parse_uint32_null },
	{ "serialize_uint32_null",          test_serialize_uint32_null },
	{ "uint32_roundtrip",              test_uint32_roundtrip },
	{ "serialize_at_nonzero_offset",    test_serialize_at_nonzero_offset },
	{ "serialize_overflow_at_offset",   test_serialize_overflow_at_offset },
	{ "serialize_pos_past_bufsz",       test_serialize_pos_past_bufsz },
	{ "serialize_uint32_position_storage", test_serialize_uint32_position_storage },
	{ "serialize_uint32_bounds", test_serialize_uint32_bounds },
	{ "parse_uint32_overlapping_output", test_parse_uint32_overlapping_output },

	/* dssh_cleanse */
	{ "cleanse_basic",                  test_cleanse_basic },
	{ "cleanse_null",                   test_cleanse_null },
	{ "cleanse_zero_len",               test_cleanse_zero_len },

	/* Params builder */
	{ "params_init_shell",              test_params_init_shell },
	{ "params_init_exec",               test_params_init_exec },
	{ "params_init_subsystem",          test_params_init_subsystem },
	{ "params_set_pty",                 test_params_set_pty },
	{ "params_set_accept_early_data",   test_params_set_accept_early_data },
	{ "params_set_term",                test_params_set_term },
	{ "params_set_size",                test_params_set_size },
	{ "params_set_command",             test_params_set_command },
	{ "params_set_subsystem",           test_params_set_subsystem },
	{ "params_add_env",                 test_params_add_env },
	{ "params_env_failure_atomic",      test_params_env_failure_atomic },
	{ "accept_env_validation",          test_accept_env_validation },
	{ "accept_env_byte_limit",          test_accept_env_byte_limit },
	{ "accept_string_validation",       test_accept_string_validation },
	{ "accept_pty_validation",          test_accept_pty_validation },
	{ "params_set_mode",                test_params_set_mode },
	{ "params_mode_domain",             test_params_mode_domain },
	{ "params_mode_dedup",              test_params_mode_dedup },
	{ "params_free_null",               test_params_free_null },
	{ "params_init_null",               test_params_init_null },
	{ "params_set_max_window",          test_params_set_max_window },
};

#define main chan_unit_test_main
DSSH_TEST_MAIN(tests)
#undef main

int
main(int argc, char *argv[])
{
	if (argc == 3 && strcmp(argv[1], "--unit-owner") == 0)
		return chan_unit_owner_main(argv[2]);
	return chan_unit_test_main(argc, argv);
}
