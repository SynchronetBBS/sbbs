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

/* ================================================================
 * Bytebuf -- circular byte buffer
 * ================================================================ */

static int test_bytebuf_init_free(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 256));
	ASSERT_NOT_NULL(b.data);
	ASSERT_EQ_U(b.capacity, 256);
	ASSERT_EQ_U(b.head, 0);
	ASSERT_EQ_U(b.tail, 0);
	ASSERT_EQ_U(b.used, 0);
	ASSERT_EQ_U(b.total, 0);
	bytebuf_free(&b);
	ASSERT_NULL(b.data);
	return TEST_PASS;
}

static int test_bytebuf_write_read_basic(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 64));

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

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_empty_read(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 32));

	uint8_t out[16];
	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 0);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_write_full(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 8));

	uint8_t data[16];
	memset(data, 0xAA, sizeof(data));

	size_t wrote = bytebuf_write(&b, data, sizeof(data));
	ASSERT_EQ_U(wrote, 8);
	ASSERT_EQ_U(bytebuf_available(&b), 8);
	ASSERT_EQ_U(bytebuf_free_space(&b), 0);

	/* Write when completely full */
	size_t wrote2 = bytebuf_write(&b, data, 1);
	ASSERT_EQ_U(wrote2, 0);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_partial_write(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 10));

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

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_free_space(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 100));

	ASSERT_EQ_U(bytebuf_free_space(&b), 100);
	ASSERT_EQ_U(bytebuf_available(&b), 0);

	uint8_t data[40];
	memset(data, 'X', sizeof(data));
	bytebuf_write(&b, data, 40);
	ASSERT_EQ_U(bytebuf_free_space(&b), 60);
	ASSERT_EQ_U(bytebuf_available(&b), 40);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_total_monotonic(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 16));

	uint8_t data[4] = {1, 2, 3, 4};
	bytebuf_write(&b, data, 4);
	ASSERT_EQ_U(b.total, 4);

	uint8_t out[4];
	bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(b.total, 4);  /* total doesn't decrease on read */

	bytebuf_write(&b, data, 4);
	ASSERT_EQ_U(b.total, 8);

	bytebuf_write(&b, data, 4);
	ASSERT_EQ_U(b.total, 12);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_wrap_around(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 8));

	uint8_t data[6] = {1, 2, 3, 4, 5, 6};
	bytebuf_write(&b, data, 6);

	/* Read 4 bytes to advance head */
	uint8_t out[8];
	bytebuf_read(&b, out, 4, 0);
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

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_wrap_write_split(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 8));

	/* Fill and drain to position tail near end */
	uint8_t fill[7];
	memset(fill, 'A', 7);
	bytebuf_write(&b, fill, 7);

	uint8_t out[8];
	bytebuf_read(&b, out, 7, 0);
	/* head=7, tail=7 */

	/* Write 5 bytes -- 1 at end, 4 at start */
	uint8_t data[5] = {10, 20, 30, 40, 50};
	size_t wrote = bytebuf_write(&b, data, 5);
	ASSERT_EQ_U(wrote, 5);

	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 5);
	ASSERT_MEM_EQ(out, data, 5);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 64));

	uint8_t data[20];
	for (int i = 0; i < 20; i++)
		data[i] = (uint8_t)i;
	bytebuf_write(&b, data, 20);

	/* Read with limit = 5 */
	uint8_t out[64];
	size_t got = bytebuf_read(&b, out, sizeof(out), 5);
	ASSERT_EQ_U(got, 5);
	ASSERT_MEM_EQ(out, data, 5);

	/* Remaining 15 still available */
	ASSERT_EQ_U(bytebuf_available(&b), 15);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit_vs_bufsz(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 64));

	uint8_t data[30];
	memset(data, 'Z', 30);
	bytebuf_write(&b, data, 30);

	/* bufsz=10 but limit=20 -- bufsz should cap it */
	uint8_t out[10];
	size_t got = bytebuf_read(&b, out, 10, 20);
	ASSERT_EQ_U(got, 10);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit_zero(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 64));

	uint8_t data[15];
	memset(data, 'Q', 15);
	bytebuf_write(&b, data, 15);

	/* limit=0 means no limit */
	uint8_t out[64];
	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 15);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_multiple_write_read_cycles(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 16));

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

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_wrap_read_split(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 8));

	/* Position head near end */
	uint8_t fill[6];
	memset(fill, 'X', 6);
	bytebuf_write(&b, fill, 6);

	uint8_t out[8];
	bytebuf_read(&b, out, 6, 0);
	/* head=6, tail=6 */

	/* Write 6 bytes wrapping: 2 at end + 4 at start */
	uint8_t data[6] = {10, 20, 30, 40, 50, 60};
	bytebuf_write(&b, data, 6);

	/* Read all 6 -- splits across wrap boundary */
	size_t got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 6);
	ASSERT_MEM_EQ(out, data, 6);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_zero_len_write(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 16));

	size_t wrote = bytebuf_write(&b, (const uint8_t *)"x", 0);
	ASSERT_EQ_U(wrote, 0);
	ASSERT_EQ_U(bytebuf_available(&b), 0);
	ASSERT_EQ_U(b.total, 0);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_single_byte_ops(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 4));

	/* Write and read one byte at a time, cycling through */
	for (int i = 0; i < 20; i++) {
		uint8_t val = (uint8_t)(i + 1);
		ASSERT_EQ_U(bytebuf_write(&b, &val, 1), 1);

		uint8_t out;
		ASSERT_EQ_U(bytebuf_read(&b, &out, 1, 0), 1);
		ASSERT_EQ(out, val);
	}

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_exact_capacity(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 8));

	uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	ASSERT_EQ_U(bytebuf_write(&b, data, 8), 8);
	ASSERT_EQ_U(bytebuf_free_space(&b), 0);
	ASSERT_EQ_U(bytebuf_available(&b), 8);

	uint8_t out[8];
	ASSERT_EQ_U(bytebuf_read(&b, out, 8, 0), 8);
	ASSERT_MEM_EQ(out, data, 8);
	ASSERT_EQ_U(bytebuf_free_space(&b), 8);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit_less_than_available(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 32));

	uint8_t data[20];
	for (int i = 0; i < 20; i++)
		data[i] = (uint8_t)i;
	bytebuf_write(&b, data, 20);

	/* limit=3, bufsz=32 -- should read exactly 3 */
	uint8_t out[32];
	size_t got = bytebuf_read(&b, out, sizeof(out), 3);
	ASSERT_EQ_U(got, 3);
	ASSERT_MEM_EQ(out, data, 3);

	/* Read remainder with limit=0 */
	got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 17);
	ASSERT_MEM_EQ(out, data + 3, 17);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_fill_drain_refill(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 8));

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

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_available_after_partial_read(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 32));

	uint8_t data[20];
	memset(data, 'M', 20);
	bytebuf_write(&b, data, 20);

	uint8_t out[5];
	bytebuf_read(&b, out, 5, 0);
	ASSERT_EQ_U(bytebuf_available(&b), 15);

	bytebuf_read(&b, out, 5, 0);
	ASSERT_EQ_U(bytebuf_available(&b), 10);

	bytebuf_read(&b, out, 5, 0);
	ASSERT_EQ_U(bytebuf_available(&b), 5);

	bytebuf_read(&b, out, 5, 0);
	ASSERT_EQ_U(bytebuf_available(&b), 0);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_total_survives_wrap(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 8));

	/* Write/read cycles that wrap multiple times */
	uint8_t data[6], out[6];
	memset(data, 'W', 6);
	for (int i = 0; i < 10; i++) {
		bytebuf_write(&b, data, 6);
		bytebuf_read(&b, out, 6, 0);
	}
	ASSERT_EQ_U(b.total, 60);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit_with_wrap(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 8));

	/* Position near end */
	uint8_t fill[6];
	memset(fill, 'X', 6);
	bytebuf_write(&b, fill, 6);

	uint8_t out[8];
	bytebuf_read(&b, out, 6, 0);

	/* Write 7 bytes wrapping around */
	uint8_t data[7] = {1, 2, 3, 4, 5, 6, 7};
	bytebuf_write(&b, data, 7);

	/* Read with limit=3 -- should cross wrap boundary correctly */
	size_t got = bytebuf_read(&b, out, sizeof(out), 3);
	ASSERT_EQ_U(got, 3);
	ASSERT_MEM_EQ(out, data, 3);

	/* Read remaining 4 */
	got = bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 4);
	ASSERT_MEM_EQ(out, data + 3, 4);

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_partial_write_at_wrap(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 8));

	/* Advance to tail=6 by writing 6 and reading 6 */
	uint8_t fill[6];
	memset(fill, 'F', 6);
	bytebuf_write(&b, fill, 6);
	uint8_t out[8];
	bytebuf_read(&b, out, 6, 0);

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

	bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_head_tail_reset_after_free(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(bytebuf_init(&b, 16));

	uint8_t data[10];
	memset(data, 'R', 10);
	bytebuf_write(&b, data, 10);
	bytebuf_free(&b);

	ASSERT_NULL(b.data);
	ASSERT_EQ_U(b.used, 0);

	return TEST_PASS;
}

/* ================================================================
 * Accept queue
 * ================================================================ */

static int test_acceptqueue_init_free(void)
{
	struct dssh_accept_queue q;
	acceptqueue_init(&q);
	ASSERT_EQ_U(q.count, 0);
	acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_push_pop_single(void)
{
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
	return TEST_PASS;
}

static int test_acceptqueue_pop_empty(void)
{
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	struct dssh_incoming_open entry;
	ASSERT_EQ(acceptqueue_pop(&q, &entry), -1);

	acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_fifo_order(void)
{
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	acceptqueue_push(&q, 1, 100, 50,
	    (const uint8_t *)"session", 7);
	acceptqueue_push(&q, 2, 200, 100,
	    (const uint8_t *)"direct-tcpip", 12);
	acceptqueue_push(&q, 3, 300, 150,
	    (const uint8_t *)"forwarded-tcpip", 15);

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
	return TEST_PASS;
}

static int test_acceptqueue_type_truncation(void)
{
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
	return TEST_PASS;
}

static int test_acceptqueue_exact_63_type(void)
{
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
	return TEST_PASS;
}

static int test_acceptqueue_empty_type(void)
{
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
	return TEST_PASS;
}

static int test_acceptqueue_many_entries(void)
{
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	/* Ring buffer capacity is DSSH_ACCEPT_QUEUE_CAP (8).
	 * Push/pop in batches to exercise wrap-around. */
	uint32_t seq = 0;
	for (int batch = 0; batch < 4; batch++) {
		/* Fill to capacity */
		for (uint32_t i = 0; i < DSSH_ACCEPT_QUEUE_CAP; i++) {
			char type[16];
			snprintf(type, sizeof(type), "ch%u", seq + i);
			ASSERT_OK(acceptqueue_push(&q, seq + i,
			    (seq + i) * 100, (seq + i) * 50,
			    (const uint8_t *)type, strlen(type)));
		}

		/* Drain all */
		for (uint32_t i = 0; i < DSSH_ACCEPT_QUEUE_CAP; i++) {
			struct dssh_incoming_open e;
			int pop_ret = acceptqueue_pop(&q, &e);
			ASSERT_EQ(pop_ret, 0);
			ASSERT_EQ(e.peer_channel, seq + i);
			ASSERT_EQ(e.peer_window, (seq + i) * 100);
			ASSERT_EQ(e.peer_max_packet, (seq + i) * 50);

			char expected[16];
			snprintf(expected, sizeof(expected), "ch%u",
			    seq + i);
			ASSERT_STR_EQ(e.channel_type, expected);
		}

		seq += DSSH_ACCEPT_QUEUE_CAP;
	}

	struct dssh_incoming_open e;
	ASSERT_EQ(acceptqueue_pop(&q, &e), -1);

	acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_free_nonempty(void)
{
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	acceptqueue_push(&q, 1, 100, 50,
	    (const uint8_t *)"session", 7);
	acceptqueue_push(&q, 2, 200, 100,
	    (const uint8_t *)"session", 7);

	acceptqueue_free(&q);
	ASSERT_EQ_U(q.count, 0);
	return TEST_PASS;
}

static int test_acceptqueue_peer_values(void)
{
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
	return TEST_PASS;
}

static int test_acceptqueue_push_pop_interleaved(void)
{
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
	return TEST_PASS;
}

static int test_acceptqueue_type_nul_terminated(void)
{
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	/* Push binary data as type -- should be NUL-terminated */
	uint8_t binary[10] = {65, 66, 67, 0, 68, 69, 70, 0, 71, 72};
	acceptqueue_push(&q, 0, 0, 0, binary, 10);

	struct dssh_incoming_open e;
	int pop_ret = acceptqueue_pop(&q, &e);
	ASSERT_EQ(pop_ret, 0);
	ASSERT_EQ_U(e.channel_type_len, 10);
	/* NUL terminator at position 63 max, but data has embedded NULs */
	ASSERT_EQ(e.channel_type[0], 'A');
	ASSERT_EQ(e.channel_type[1], 'B');
	ASSERT_EQ(e.channel_type[2], 'C');

	acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_zero_window_packet(void)
{
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
	return TEST_PASS;
}

static int test_acceptqueue_single_char_type(void)
{
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
	return TEST_PASS;
}

static int test_acceptqueue_pop_does_not_affect_remaining(void)
{
	struct dssh_accept_queue q;
	acceptqueue_init(&q);

	acceptqueue_push(&q, 1, 100, 50,
	    (const uint8_t *)"alpha", 5);
	acceptqueue_push(&q, 2, 200, 100,
	    (const uint8_t *)"bravo", 5);

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
	return TEST_PASS;
}


/* ================================================================
 * RFC 4251 wire format: uint32 parse/serialize (formerly test_arch.c)
 * ================================================================ */

static int
test_parse_uint32_basic(void)
{
	uint8_t buf[] = { 0x01, 0x02, 0x03, 0x04 };
	uint32_t val = 0;
	int64_t ret = dssh_parse_uint32(buf, sizeof(buf), &val);
	ASSERT_EQ(ret, 4);
	ASSERT_EQ_U(val, 0x01020304);
	return TEST_PASS;
}

static int
test_parse_uint32_zero(void)
{
	uint8_t buf[] = { 0x00, 0x00, 0x00, 0x00 };
	uint32_t val = 1;
	dssh_parse_uint32(buf, sizeof(buf), &val);
	ASSERT_EQ_U(val, 0);
	return TEST_PASS;
}

static int
test_parse_uint32_max(void)
{
	uint8_t buf[] = { 0xFF, 0xFF, 0xFF, 0xFF };
	uint32_t val = 0;
	dssh_parse_uint32(buf, sizeof(buf), &val);
	ASSERT_EQ_U(val, UINT32_MAX);
	return TEST_PASS;
}

static int
test_parse_uint32_short_buffer(void)
{
	uint8_t buf[] = { 0x01, 0x02, 0x03 };
	uint32_t val = 0;
	ASSERT_ERR(dssh_parse_uint32(buf, sizeof(buf), &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_parse_uint32_empty_buffer(void)
{
	uint8_t buf[] = { 0 };
	uint32_t val = 0;
	ASSERT_ERR(dssh_parse_uint32(buf, 0, &val), DSSH_ERROR_PARSE);
	return TEST_PASS;
}

static int
test_serialize_uint32(void)
{
	uint8_t buf[8] = { 0 };
	size_t pos = 0;
	ASSERT_EQ(dssh_serialize_uint32(0xDEADBEEF, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 4);
	uint8_t expected[] = { 0xDE, 0xAD, 0xBE, 0xEF };
	ASSERT_MEM_EQ(buf, expected, 4);
	return TEST_PASS;
}

static int
test_serialize_uint32_overflow(void)
{
	uint8_t buf[3];
	size_t pos = 0;
	ASSERT_ERR(dssh_serialize_uint32(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	return TEST_PASS;
}

static int
test_uint32_roundtrip(void)
{
	uint8_t buf[8];
	size_t pos = 0;
	uint32_t orig = 0x12345678;
	uint32_t parsed = 0;

	ASSERT_EQ(dssh_serialize_uint32(orig, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ(dssh_parse_uint32(buf, pos, &parsed), 4);
	ASSERT_EQ_U(parsed, orig);
	return TEST_PASS;
}

static int
test_serialize_at_nonzero_offset(void)
{
	uint8_t buf[16] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };
	size_t pos = 4;

	ASSERT_EQ(dssh_serialize_uint32(0xAABBCCDD, buf, sizeof(buf), &pos), 0);
	ASSERT_EQ_U(pos, 8);

	uint8_t expected_prefix[] = { 0xCC, 0xCC, 0xCC, 0xCC };
	ASSERT_MEM_EQ(buf, expected_prefix, 4);

	uint8_t expected_val[] = { 0xAA, 0xBB, 0xCC, 0xDD };
	ASSERT_MEM_EQ(&buf[4], expected_val, 4);
	return TEST_PASS;
}

static int
test_serialize_overflow_at_offset(void)
{
	uint8_t buf[8];
	size_t pos = 6;

	ASSERT_ERR(dssh_serialize_uint32(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	ASSERT_EQ_U(pos, 6);
	return TEST_PASS;
}

static int
test_serialize_pos_past_bufsz(void)
{
	uint8_t buf[8];
	size_t pos = 10;

	ASSERT_ERR(dssh_serialize_uint32(1, buf, sizeof(buf), &pos), DSSH_ERROR_TOOLONG);
	ASSERT_EQ_U(pos, 10);
	return TEST_PASS;
}

static int
test_cleanse_basic(void)
{
	uint8_t buf[16];

	memset(buf, 0xAA, sizeof(buf));
	dssh_cleanse(buf, sizeof(buf));

	for (size_t i = 0; i < sizeof(buf); i++)
		ASSERT_EQ(buf[i], 0);
	return TEST_PASS;
}

static int
test_cleanse_null(void)
{
	dssh_cleanse(NULL, 0);
	dssh_cleanse(NULL, 100);
	return TEST_PASS;
}

static int
test_cleanse_zero_len(void)
{
	uint8_t buf[4] = {0xAA, 0xBB, 0xCC, 0xDD};

	dssh_cleanse(buf, 0);

	ASSERT_EQ(buf[0], 0xAA);
	ASSERT_EQ(buf[3], 0xDD);
	return TEST_PASS;
}

static int
test_parse_uint32_null(void)
{
	uint8_t buf[4] = {0, 0, 0, 1};
	uint32_t val;

	ASSERT_EQ(dssh_parse_uint32(buf, 4, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_parse_uint32(NULL, 4, &val), DSSH_ERROR_INVALID);
	return TEST_PASS;
}

static int
test_serialize_uint32_null(void)
{
	uint8_t buf[4];
	size_t pos = 0;

	ASSERT_EQ(dssh_serialize_uint32(1, buf, 4, NULL), DSSH_ERROR_INVALID);
	ASSERT_EQ(dssh_serialize_uint32(1, NULL, 4, &pos), DSSH_ERROR_INVALID);
	return TEST_PASS;
}

/* ================================================================
 * Params builder tests
 * ================================================================ */

static int test_params_init_shell(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SHELL));
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
	dssh_chan_params_free(&p);
	return TEST_PASS;
}

static int test_params_init_exec(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_EXEC));
	ASSERT_EQ(p.type, DSSH_CHAN_EXEC);
	ASSERT_FALSE(p.flags & DSSH_PARAM_HAS_PTY);
	dssh_chan_params_free(&p);
	return TEST_PASS;
}

static int test_params_init_subsystem(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SUBSYSTEM));
	ASSERT_EQ(p.type, DSSH_CHAN_SUBSYSTEM);
	ASSERT_FALSE(p.flags & DSSH_PARAM_HAS_PTY);
	dssh_chan_params_free(&p);
	return TEST_PASS;
}

static int test_params_set_pty(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_EXEC));
	ASSERT_FALSE(p.flags & DSSH_PARAM_HAS_PTY);

	ASSERT_OK(dssh_chan_params_set_pty(&p, true));
	ASSERT_TRUE(p.flags & DSSH_PARAM_HAS_PTY);

	ASSERT_OK(dssh_chan_params_set_pty(&p, false));
	ASSERT_FALSE(p.flags & DSSH_PARAM_HAS_PTY);

	dssh_chan_params_free(&p);
	return TEST_PASS;
}

static int test_params_set_term(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SHELL));
	ASSERT_OK(dssh_chan_params_set_term(&p, "xterm-256color"));
	ASSERT_STR_EQ(p.term, "xterm-256color");

	/* Long term string preserved in full (dynamic allocation) */
	char longterm[128];
	memset(longterm, 'x', sizeof(longterm) - 1);
	longterm[sizeof(longterm) - 1] = '\0';
	ASSERT_OK(dssh_chan_params_set_term(&p, longterm));
	ASSERT_EQ_U(strlen(p.term), 127);

	dssh_chan_params_free(&p);
	return TEST_PASS;
}

static int test_params_set_size(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SHELL));
	ASSERT_OK(dssh_chan_params_set_size(&p, 80, 24, 640, 480));
	ASSERT_EQ_U(p.cols, 80);
	ASSERT_EQ_U(p.rows, 24);
	ASSERT_EQ_U(p.wpx, 640);
	ASSERT_EQ_U(p.hpx, 480);
	dssh_chan_params_free(&p);
	return TEST_PASS;
}

static int test_params_set_command(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_EXEC));
	ASSERT_OK(dssh_chan_params_set_command(&p, "ls -la"));
	ASSERT_NOT_NULL(p.command);
	ASSERT_STR_EQ(p.command, "ls -la");

	/* Replace */
	ASSERT_OK(dssh_chan_params_set_command(&p, "uname -a"));
	ASSERT_STR_EQ(p.command, "uname -a");

	dssh_chan_params_free(&p);
	ASSERT_NULL(p.command);
	return TEST_PASS;
}

static int test_params_set_subsystem(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SUBSYSTEM));
	ASSERT_OK(dssh_chan_params_set_subsystem(&p, "sftp"));
	ASSERT_STR_EQ(p.subsystem, "sftp");
	dssh_chan_params_free(&p);
	ASSERT_NULL(p.subsystem);
	return TEST_PASS;
}

static int test_params_add_env(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SHELL));

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

	dssh_chan_params_free(&p);
	ASSERT_NULL(p.env);
	ASSERT_EQ_U(p.env_count, 0);
	return TEST_PASS;
}

static int test_params_set_mode(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SHELL));

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

	dssh_chan_params_free(&p);
	ASSERT_NULL(p.modes);
	return TEST_PASS;
}

static int test_params_mode_dedup(void)
{
	struct dssh_chan_params p;

	ASSERT_OK(dssh_chan_params_init(&p, DSSH_CHAN_SHELL));

	ASSERT_OK(dssh_chan_params_set_mode(&p, 1, 100));
	ASSERT_OK(dssh_chan_params_set_mode(&p, 42, 200));
	/* Overwrite opcode 1 -- last-wins */
	ASSERT_OK(dssh_chan_params_set_mode(&p, 1, 999));

	ASSERT_EQ_U(p.mode_count, 2);
	ASSERT_EQ(p.modes[0].opcode, 1);
	ASSERT_EQ_U(p.modes[0].value, 999);
	ASSERT_EQ(p.modes[1].opcode, 42);
	ASSERT_EQ_U(p.modes[1].value, 200);

	dssh_chan_params_free(&p);
	return TEST_PASS;
}

static int test_params_free_null(void)
{
	/* Must not crash */
	dssh_chan_params_free(NULL);
	return TEST_PASS;
}

static int test_params_init_null(void)
{
	ASSERT_ERR(dssh_chan_params_init(NULL, DSSH_CHAN_SHELL),
	    DSSH_ERROR_INVALID);
	return TEST_PASS;
}

/* ================================================================
 * Event queue cap
 * ================================================================ */

static int
test_event_queue_cap_enforced(void)
{
	struct dssh_event_queue q;
	int res = event_queue_init(&q, 4);

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

	event_queue_free(&q);
	return TEST_PASS;
}

static int
test_event_queue_no_cap(void)
{
	struct dssh_event_queue q;
	int res = event_queue_init(&q, 0);

	ASSERT_EQ(res, 0);

	struct dssh_event_entry ev = { .type = 1 };

	/* Should grow past initial capacity (8) without limit */
	for (int i = 0; i < 16; i++) {
		res = event_queue_push(&q, &ev);
		ASSERT_EQ(res, 0);
	}

	ASSERT_EQ_U(q.count, 16);

	event_queue_free(&q);
	return TEST_PASS;
}

static int
test_event_queue_cap_at_initial_capacity(void)
{
	/* Cap exactly at initial capacity (8) — no realloc needed */
	struct dssh_event_queue q;
	int res = event_queue_init(&q, 8);

	ASSERT_EQ(res, 0);

	struct dssh_event_entry ev = { .type = 1 };

	for (int i = 0; i < 8; i++) {
		res = event_queue_push(&q, &ev);
		ASSERT_EQ(res, 0);
	}

	res = event_queue_push(&q, &ev);
	ASSERT_EQ(res, DSSH_ERROR_TOOMANY);

	event_queue_free(&q);
	return TEST_PASS;
}

/* ================================================================
 * Test table
 * ================================================================ */

static struct dssh_test_entry tests[] = {
	/* Bytebuf */
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

	/* dssh_cleanse */
	{ "cleanse_basic",                  test_cleanse_basic },
	{ "cleanse_null",                   test_cleanse_null },
	{ "cleanse_zero_len",               test_cleanse_zero_len },

	/* Params builder */
	{ "params_init_shell",              test_params_init_shell },
	{ "params_init_exec",               test_params_init_exec },
	{ "params_init_subsystem",          test_params_init_subsystem },
	{ "params_set_pty",                 test_params_set_pty },
	{ "params_set_term",                test_params_set_term },
	{ "params_set_size",                test_params_set_size },
	{ "params_set_command",             test_params_set_command },
	{ "params_set_subsystem",           test_params_set_subsystem },
	{ "params_add_env",                 test_params_add_env },
	{ "params_set_mode",                test_params_set_mode },
	{ "params_mode_dedup",              test_params_mode_dedup },
	{ "params_free_null",               test_params_free_null },
	{ "params_init_null",               test_params_init_null },
};

DSSH_TEST_NO_CLEANUP
DSSH_TEST_MAIN(tests)
