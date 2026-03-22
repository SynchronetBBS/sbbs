/*
 * test_chan.c — Unit tests for channel buffer primitives (ssh-chan.h).
 *
 * Tests bytebuf, msgqueue, signal queue, and accept queue.
 */

#include "dssh_test.h"
#include "deucessh.h"
#include "ssh-chan.h"

/* ================================================================
 * Bytebuf — circular byte buffer
 * ================================================================ */

static int test_bytebuf_init_free(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 256));
	ASSERT_NOT_NULL(b.data);
	ASSERT_EQ_U(b.capacity, 256);
	ASSERT_EQ_U(b.head, 0);
	ASSERT_EQ_U(b.tail, 0);
	ASSERT_EQ_U(b.used, 0);
	ASSERT_EQ_U(b.total, 0);
	dssh_bytebuf_free(&b);
	ASSERT_NULL(b.data);
	return TEST_PASS;
}

static int test_bytebuf_write_read_basic(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 64));

	const uint8_t msg[] = "Hello, world!";
	size_t wrote = dssh_bytebuf_write(&b, msg, sizeof(msg));
	ASSERT_EQ_U(wrote, sizeof(msg));
	ASSERT_EQ_U(dssh_bytebuf_available(&b), sizeof(msg));
	ASSERT_EQ_U(b.total, sizeof(msg));

	uint8_t out[64];
	size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, sizeof(msg));
	ASSERT_MEM_EQ(out, msg, sizeof(msg));
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 0);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_empty_read(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 32));

	uint8_t out[16];
	size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 0);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_write_full(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 8));

	uint8_t data[16];
	memset(data, 0xAA, sizeof(data));

	size_t wrote = dssh_bytebuf_write(&b, data, sizeof(data));
	ASSERT_EQ_U(wrote, 8);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 8);
	ASSERT_EQ_U(dssh_bytebuf_free_space(&b), 0);

	/* Write when completely full */
	size_t wrote2 = dssh_bytebuf_write(&b, data, 1);
	ASSERT_EQ_U(wrote2, 0);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_partial_write(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 10));

	uint8_t data[7];
	memset(data, 'A', sizeof(data));
	ASSERT_EQ_U(dssh_bytebuf_write(&b, data, 7), 7);
	ASSERT_EQ_U(dssh_bytebuf_free_space(&b), 3);

	/* Only 3 bytes of space left */
	uint8_t more[5];
	memset(more, 'B', sizeof(more));
	size_t wrote = dssh_bytebuf_write(&b, more, 5);
	ASSERT_EQ_U(wrote, 3);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 10);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_free_space(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 100));

	ASSERT_EQ_U(dssh_bytebuf_free_space(&b), 100);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 0);

	uint8_t data[40];
	memset(data, 'X', sizeof(data));
	dssh_bytebuf_write(&b, data, 40);
	ASSERT_EQ_U(dssh_bytebuf_free_space(&b), 60);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 40);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_total_monotonic(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 16));

	uint8_t data[4] = {1, 2, 3, 4};
	dssh_bytebuf_write(&b, data, 4);
	ASSERT_EQ_U(b.total, 4);

	uint8_t out[4];
	dssh_bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(b.total, 4);  /* total doesn't decrease on read */

	dssh_bytebuf_write(&b, data, 4);
	ASSERT_EQ_U(b.total, 8);

	dssh_bytebuf_write(&b, data, 4);
	ASSERT_EQ_U(b.total, 12);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_wrap_around(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 8));

	uint8_t data[6] = {1, 2, 3, 4, 5, 6};
	dssh_bytebuf_write(&b, data, 6);

	/* Read 4 bytes to advance head */
	uint8_t out[8];
	dssh_bytebuf_read(&b, out, 4, 0);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 2);

	/* Write 6 more bytes — wraps around */
	uint8_t data2[6] = {10, 20, 30, 40, 50, 60};
	size_t wrote = dssh_bytebuf_write(&b, data2, 6);
	ASSERT_EQ_U(wrote, 6);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 8);

	/* Read everything: should get {5, 6, 10, 20, 30, 40, 50, 60} */
	size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 8);
	uint8_t expected[] = {5, 6, 10, 20, 30, 40, 50, 60};
	ASSERT_MEM_EQ(out, expected, 8);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_wrap_write_split(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 8));

	/* Fill and drain to position tail near end */
	uint8_t fill[7];
	memset(fill, 'A', 7);
	dssh_bytebuf_write(&b, fill, 7);

	uint8_t out[8];
	dssh_bytebuf_read(&b, out, 7, 0);
	/* head=7, tail=7 */

	/* Write 5 bytes — 1 at end, 4 at start */
	uint8_t data[5] = {10, 20, 30, 40, 50};
	size_t wrote = dssh_bytebuf_write(&b, data, 5);
	ASSERT_EQ_U(wrote, 5);

	size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 5);
	ASSERT_MEM_EQ(out, data, 5);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 64));

	uint8_t data[20];
	for (int i = 0; i < 20; i++)
		data[i] = (uint8_t)i;
	dssh_bytebuf_write(&b, data, 20);

	/* Read with limit = 5 */
	uint8_t out[64];
	size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 5);
	ASSERT_EQ_U(got, 5);
	ASSERT_MEM_EQ(out, data, 5);

	/* Remaining 15 still available */
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 15);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit_vs_bufsz(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 64));

	uint8_t data[30];
	memset(data, 'Z', 30);
	dssh_bytebuf_write(&b, data, 30);

	/* bufsz=10 but limit=20 — bufsz should cap it */
	uint8_t out[10];
	size_t got = dssh_bytebuf_read(&b, out, 10, 20);
	ASSERT_EQ_U(got, 10);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit_zero(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 64));

	uint8_t data[15];
	memset(data, 'Q', 15);
	dssh_bytebuf_write(&b, data, 15);

	/* limit=0 means no limit */
	uint8_t out[64];
	size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 15);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_multiple_write_read_cycles(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 16));

	uint8_t out[16];
	for (int cycle = 0; cycle < 20; cycle++) {
		uint8_t val = (uint8_t)(cycle & 0xFF);
		uint8_t data[5];
		memset(data, val, 5);

		size_t wrote = dssh_bytebuf_write(&b, data, 5);
		ASSERT_EQ_U(wrote, 5);

		size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 0);
		ASSERT_EQ_U(got, 5);
		for (int i = 0; i < 5; i++)
			ASSERT_EQ(out[i], val);
	}
	ASSERT_EQ_U(b.total, 100);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_wrap_read_split(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 8));

	/* Position head near end */
	uint8_t fill[6];
	memset(fill, 'X', 6);
	dssh_bytebuf_write(&b, fill, 6);

	uint8_t out[8];
	dssh_bytebuf_read(&b, out, 6, 0);
	/* head=6, tail=6 */

	/* Write 6 bytes wrapping: 2 at end + 4 at start */
	uint8_t data[6] = {10, 20, 30, 40, 50, 60};
	dssh_bytebuf_write(&b, data, 6);

	/* Read all 6 — splits across wrap boundary */
	size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 6);
	ASSERT_MEM_EQ(out, data, 6);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_zero_len_write(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 16));

	size_t wrote = dssh_bytebuf_write(&b, (const uint8_t *)"x", 0);
	ASSERT_EQ_U(wrote, 0);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 0);
	ASSERT_EQ_U(b.total, 0);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_single_byte_ops(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 4));

	/* Write and read one byte at a time, cycling through */
	for (int i = 0; i < 20; i++) {
		uint8_t val = (uint8_t)(i + 1);
		ASSERT_EQ_U(dssh_bytebuf_write(&b, &val, 1), 1);

		uint8_t out;
		ASSERT_EQ_U(dssh_bytebuf_read(&b, &out, 1, 0), 1);
		ASSERT_EQ(out, val);
	}

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_exact_capacity(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 8));

	uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	ASSERT_EQ_U(dssh_bytebuf_write(&b, data, 8), 8);
	ASSERT_EQ_U(dssh_bytebuf_free_space(&b), 0);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 8);

	uint8_t out[8];
	ASSERT_EQ_U(dssh_bytebuf_read(&b, out, 8, 0), 8);
	ASSERT_MEM_EQ(out, data, 8);
	ASSERT_EQ_U(dssh_bytebuf_free_space(&b), 8);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit_less_than_available(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 32));

	uint8_t data[20];
	for (int i = 0; i < 20; i++)
		data[i] = (uint8_t)i;
	dssh_bytebuf_write(&b, data, 20);

	/* limit=3, bufsz=32 — should read exactly 3 */
	uint8_t out[32];
	size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 3);
	ASSERT_EQ_U(got, 3);
	ASSERT_MEM_EQ(out, data, 3);

	/* Read remainder with limit=0 */
	got = dssh_bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 17);
	ASSERT_MEM_EQ(out, data + 3, 17);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_fill_drain_refill(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 8));

	/* Fill completely */
	uint8_t data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	ASSERT_EQ_U(dssh_bytebuf_write(&b, data, 8), 8);

	/* Drain completely */
	uint8_t out[8];
	ASSERT_EQ_U(dssh_bytebuf_read(&b, out, 8, 0), 8);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 0);
	ASSERT_EQ_U(dssh_bytebuf_free_space(&b), 8);

	/* Refill completely */
	uint8_t data2[8] = {10, 20, 30, 40, 50, 60, 70, 80};
	ASSERT_EQ_U(dssh_bytebuf_write(&b, data2, 8), 8);
	ASSERT_EQ_U(dssh_bytebuf_read(&b, out, 8, 0), 8);
	ASSERT_MEM_EQ(out, data2, 8);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_available_after_partial_read(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 32));

	uint8_t data[20];
	memset(data, 'M', 20);
	dssh_bytebuf_write(&b, data, 20);

	uint8_t out[5];
	dssh_bytebuf_read(&b, out, 5, 0);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 15);

	dssh_bytebuf_read(&b, out, 5, 0);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 10);

	dssh_bytebuf_read(&b, out, 5, 0);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 5);

	dssh_bytebuf_read(&b, out, 5, 0);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 0);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_total_survives_wrap(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 8));

	/* Write/read cycles that wrap multiple times */
	uint8_t data[6], out[6];
	memset(data, 'W', 6);
	for (int i = 0; i < 10; i++) {
		dssh_bytebuf_write(&b, data, 6);
		dssh_bytebuf_read(&b, out, 6, 0);
	}
	ASSERT_EQ_U(b.total, 60);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_read_limit_with_wrap(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 8));

	/* Position near end */
	uint8_t fill[6];
	memset(fill, 'X', 6);
	dssh_bytebuf_write(&b, fill, 6);

	uint8_t out[8];
	dssh_bytebuf_read(&b, out, 6, 0);

	/* Write 7 bytes wrapping around */
	uint8_t data[7] = {1, 2, 3, 4, 5, 6, 7};
	dssh_bytebuf_write(&b, data, 7);

	/* Read with limit=3 — should cross wrap boundary correctly */
	size_t got = dssh_bytebuf_read(&b, out, sizeof(out), 3);
	ASSERT_EQ_U(got, 3);
	ASSERT_MEM_EQ(out, data, 3);

	/* Read remaining 4 */
	got = dssh_bytebuf_read(&b, out, sizeof(out), 0);
	ASSERT_EQ_U(got, 4);
	ASSERT_MEM_EQ(out, data + 3, 4);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_partial_write_at_wrap(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 8));

	/* Advance to tail=6 by writing 6 and reading 6 */
	uint8_t fill[6];
	memset(fill, 'F', 6);
	dssh_bytebuf_write(&b, fill, 6);
	uint8_t out[8];
	dssh_bytebuf_read(&b, out, 6, 0);

	/* Write 3 bytes to partially fill the end */
	uint8_t d1[3] = {1, 2, 3};
	ASSERT_EQ_U(dssh_bytebuf_write(&b, d1, 3), 3);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 3);
	ASSERT_EQ_U(dssh_bytebuf_free_space(&b), 5);

	/* Try to write 7 more — only 5 fit */
	uint8_t d2[7] = {10, 20, 30, 40, 50, 60, 70};
	ASSERT_EQ_U(dssh_bytebuf_write(&b, d2, 7), 5);
	ASSERT_EQ_U(dssh_bytebuf_available(&b), 8);

	/* Verify all data */
	ASSERT_EQ_U(dssh_bytebuf_read(&b, out, sizeof(out), 0), 8);
	uint8_t expected[8] = {1, 2, 3, 10, 20, 30, 40, 50};
	ASSERT_MEM_EQ(out, expected, 8);

	dssh_bytebuf_free(&b);
	return TEST_PASS;
}

static int test_bytebuf_head_tail_reset_after_free(void)
{
	struct dssh_bytebuf b;
	ASSERT_OK(dssh_bytebuf_init(&b, 16));

	uint8_t data[10];
	memset(data, 'R', 10);
	dssh_bytebuf_write(&b, data, 10);
	dssh_bytebuf_free(&b);

	ASSERT_NULL(b.data);
	ASSERT_EQ_U(b.used, 0);

	return TEST_PASS;
}

/* ================================================================
 * Msgqueue — linked list message queue
 * ================================================================ */

static int test_msgqueue_init_free(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);
	ASSERT_NULL(q.head);
	ASSERT_NULL(q.tail);
	ASSERT_EQ_U(q.total_bytes, 0);
	ASSERT_EQ_U(q.count, 0);
	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_push_pop_single(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t msg[] = "test message";
	ASSERT_OK(dssh_msgqueue_push(&q, msg, sizeof(msg)));
	ASSERT_EQ_U(q.count, 1);
	ASSERT_EQ_U(q.total_bytes, sizeof(msg));

	uint8_t buf[64];
	int64_t got = dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_EQ(got, (int64_t)sizeof(msg));
	ASSERT_MEM_EQ(buf, msg, sizeof(msg));
	ASSERT_EQ_U(q.count, 0);
	ASSERT_EQ_U(q.total_bytes, 0);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_pop_empty(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	uint8_t buf[16];
	int64_t got = dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_EQ(got, 0);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_peek_empty(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);
	ASSERT_EQ_U(dssh_msgqueue_peek_size(&q), 0);
	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_peek_size(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t msg[10] = {0};
	dssh_msgqueue_push(&q, msg, 10);
	ASSERT_EQ_U(dssh_msgqueue_peek_size(&q), 10);

	/* Peek doesn't consume */
	ASSERT_EQ_U(dssh_msgqueue_peek_size(&q), 10);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_pop_toolong(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t msg[20] = {0};
	dssh_msgqueue_push(&q, msg, 20);

	/* Buffer too small */
	uint8_t buf[10];
	int64_t got = dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_EQ(got, (int64_t)DSSH_ERROR_TOOLONG);

	/* Message should still be queued */
	ASSERT_EQ_U(q.count, 1);
	ASSERT_EQ_U(dssh_msgqueue_peek_size(&q), 20);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_fifo_order(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t m1[] = "first";
	const uint8_t m2[] = "second";
	const uint8_t m3[] = "third";
	dssh_msgqueue_push(&q, m1, sizeof(m1));
	dssh_msgqueue_push(&q, m2, sizeof(m2));
	dssh_msgqueue_push(&q, m3, sizeof(m3));
	ASSERT_EQ_U(q.count, 3);

	uint8_t buf[64];
	dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_MEM_EQ(buf, m1, sizeof(m1));
	dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_MEM_EQ(buf, m2, sizeof(m2));
	dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_MEM_EQ(buf, m3, sizeof(m3));
	ASSERT_EQ_U(q.count, 0);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_total_bytes(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t m1[5] = {0};
	const uint8_t m2[10] = {0};
	dssh_msgqueue_push(&q, m1, 5);
	dssh_msgqueue_push(&q, m2, 10);
	ASSERT_EQ_U(q.total_bytes, 15);

	uint8_t buf[16];
	dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_EQ_U(q.total_bytes, 10);

	dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_EQ_U(q.total_bytes, 0);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_many_messages(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	for (int i = 0; i < 100; i++) {
		uint8_t val = (uint8_t)i;
		ASSERT_OK(dssh_msgqueue_push(&q, &val, 1));
	}
	ASSERT_EQ_U(q.count, 100);
	ASSERT_EQ_U(q.total_bytes, 100);

	for (int i = 0; i < 100; i++) {
		uint8_t buf;
		int64_t got = dssh_msgqueue_pop(&q, &buf, 1);
		ASSERT_EQ(got, 1);
		ASSERT_EQ(buf, (uint8_t)i);
	}
	ASSERT_EQ_U(q.count, 0);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_free_nonempty(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t msg[8] = {1, 2, 3, 4, 5, 6, 7, 8};
	dssh_msgqueue_push(&q, msg, 8);
	dssh_msgqueue_push(&q, msg, 8);
	dssh_msgqueue_push(&q, msg, 8);

	/* Free without popping — should not leak */
	dssh_msgqueue_free(&q);
	ASSERT_NULL(q.head);
	ASSERT_NULL(q.tail);
	return TEST_PASS;
}

static int test_msgqueue_zero_len_message(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	ASSERT_OK(dssh_msgqueue_push(&q, NULL, 0));
	ASSERT_EQ_U(q.count, 1);
	ASSERT_EQ_U(dssh_msgqueue_peek_size(&q), 0);

	uint8_t buf[1];
	int64_t got = dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_EQ(got, 0);
	/* count should now be 0 after consuming the zero-length message */
	ASSERT_EQ_U(q.count, 0);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_pop_after_toolong(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t msg[] = "retained";
	dssh_msgqueue_push(&q, msg, sizeof(msg));

	/* Fail with small buffer */
	uint8_t small[2];
	int64_t got = dssh_msgqueue_pop(&q, small, sizeof(small));
	ASSERT_EQ(got, (int64_t)DSSH_ERROR_TOOLONG);

	/* Succeed with larger buffer */
	uint8_t big[64];
	got = dssh_msgqueue_pop(&q, big, sizeof(big));
	ASSERT_EQ(got, (int64_t)sizeof(msg));
	ASSERT_MEM_EQ(big, msg, sizeof(msg));

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_peek_after_pop(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t m1[3] = {1, 2, 3};
	const uint8_t m2[7] = {4, 5, 6, 7, 8, 9, 10};
	dssh_msgqueue_push(&q, m1, 3);
	dssh_msgqueue_push(&q, m2, 7);

	ASSERT_EQ_U(dssh_msgqueue_peek_size(&q), 3);

	uint8_t buf[16];
	dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_EQ_U(dssh_msgqueue_peek_size(&q), 7);

	dssh_msgqueue_pop(&q, buf, sizeof(buf));
	ASSERT_EQ_U(dssh_msgqueue_peek_size(&q), 0);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_push_pop_interleaved(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	uint8_t buf[16];
	for (int i = 0; i < 50; i++) {
		uint8_t data[4];
		memset(data, (uint8_t)i, 4);
		ASSERT_OK(dssh_msgqueue_push(&q, data, 4));

		int64_t got = dssh_msgqueue_pop(&q, buf, sizeof(buf));
		ASSERT_EQ(got, 4);
		for (int j = 0; j < 4; j++)
			ASSERT_EQ(buf[j], (uint8_t)i);
	}
	ASSERT_EQ_U(q.count, 0);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_varying_sizes(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	/* Push messages of increasing size */
	for (size_t i = 1; i <= 10; i++) {
		uint8_t data[10];
		memset(data, (uint8_t)i, i);
		ASSERT_OK(dssh_msgqueue_push(&q, data, i));
	}
	ASSERT_EQ_U(q.count, 10);
	ASSERT_EQ_U(q.total_bytes, 55);  /* 1+2+...+10 */

	/* Pop and verify sizes */
	for (size_t i = 1; i <= 10; i++) {
		ASSERT_EQ_U(dssh_msgqueue_peek_size(&q), i);
		uint8_t buf[16];
		int64_t got = dssh_msgqueue_pop(&q, buf, sizeof(buf));
		ASSERT_EQ((size_t)got, i);
		for (size_t j = 0; j < i; j++)
			ASSERT_EQ(buf[j], (uint8_t)i);
	}

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_exact_bufsz_pop(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t msg[5] = {10, 20, 30, 40, 50};
	dssh_msgqueue_push(&q, msg, 5);

	/* Pop with bufsz exactly equal to message length */
	uint8_t buf[5];
	int64_t got = dssh_msgqueue_pop(&q, buf, 5);
	ASSERT_EQ(got, 5);
	ASSERT_MEM_EQ(buf, msg, 5);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_count_tracking(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t msg[1] = {0};
	for (int i = 0; i < 5; i++) {
		dssh_msgqueue_push(&q, msg, 1);
		ASSERT_EQ_U(q.count, (size_t)(i + 1));
	}

	uint8_t buf[1];
	for (int i = 5; i > 0; i--) {
		ASSERT_EQ_U(q.count, (size_t)i);
		dssh_msgqueue_pop(&q, buf, 1);
	}
	ASSERT_EQ_U(q.count, 0);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_large_message(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	uint8_t big[4096];
	for (size_t i = 0; i < sizeof(big); i++)
		big[i] = (uint8_t)(i & 0xFF);

	ASSERT_OK(dssh_msgqueue_push(&q, big, sizeof(big)));
	ASSERT_EQ_U(q.total_bytes, 4096);

	uint8_t out[4096];
	int64_t got = dssh_msgqueue_pop(&q, out, sizeof(out));
	ASSERT_EQ(got, 4096);
	ASSERT_MEM_EQ(out, big, 4096);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

static int test_msgqueue_toolong_preserves_count(void)
{
	struct dssh_msgqueue q;
	dssh_msgqueue_init(&q);

	const uint8_t m1[10] = {0};
	const uint8_t m2[5] = {1, 2, 3, 4, 5};
	dssh_msgqueue_push(&q, m1, 10);
	dssh_msgqueue_push(&q, m2, 5);
	ASSERT_EQ_U(q.count, 2);
	ASSERT_EQ_U(q.total_bytes, 15);

	/* Fail to pop first (too small) */
	uint8_t small[4];
	ASSERT_EQ(dssh_msgqueue_pop(&q, small, sizeof(small)),
	    (int64_t)DSSH_ERROR_TOOLONG);

	/* Count and total_bytes unchanged */
	ASSERT_EQ_U(q.count, 2);
	ASSERT_EQ_U(q.total_bytes, 15);

	dssh_msgqueue_free(&q);
	return TEST_PASS;
}

/* ================================================================
 * Signal queue
 * ================================================================ */

static int test_sigqueue_init_free(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);
	ASSERT_NULL(q.head);
	ASSERT_NULL(q.tail);
	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_push_ready_pop(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	ASSERT_OK(dssh_sigqueue_push(&q, "INT", 100, 50));

	/* Not ready — stdout not consumed enough */
	ASSERT_FALSE(dssh_sigqueue_ready(&q, 99, 50));
	/* Not ready — stderr not consumed enough */
	ASSERT_FALSE(dssh_sigqueue_ready(&q, 100, 49));
	/* Not ready — neither consumed enough */
	ASSERT_FALSE(dssh_sigqueue_ready(&q, 50, 25));
	/* Ready — both consumed enough */
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 100, 50));
	/* Ready — both exceeded */
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 200, 100));

	char buf[32];
	const char *name = dssh_sigqueue_pop(&q, 100, 50, buf, sizeof(buf));
	ASSERT_NOT_NULL(name);
	ASSERT_STR_EQ(buf, "INT");

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_pop_not_ready(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	ASSERT_OK(dssh_sigqueue_push(&q, "TERM", 10, 10));

	char buf[32];
	const char *name = dssh_sigqueue_pop(&q, 5, 5, buf, sizeof(buf));
	ASSERT_NULL(name);

	/* Signal should still be queued */
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 10, 10));

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_pop_empty(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	char buf[32];
	const char *name = dssh_sigqueue_pop(&q, 1000, 1000, buf, sizeof(buf));
	ASSERT_NULL(name);

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_ready_empty(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	ASSERT_FALSE(dssh_sigqueue_ready(&q, 1000, 1000));

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_fifo_order(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	dssh_sigqueue_push(&q, "HUP", 0, 0);
	dssh_sigqueue_push(&q, "INT", 0, 0);
	dssh_sigqueue_push(&q, "TERM", 0, 0);

	char buf[32];
	dssh_sigqueue_pop(&q, 0, 0, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "HUP");
	dssh_sigqueue_pop(&q, 0, 0, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "INT");
	dssh_sigqueue_pop(&q, 0, 0, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "TERM");

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_ordered_positions(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	/* Signals at increasing stream positions */
	dssh_sigqueue_push(&q, "FIRST", 10, 5);
	dssh_sigqueue_push(&q, "SECOND", 20, 15);
	dssh_sigqueue_push(&q, "THIRD", 30, 25);

	/* Only first is ready */
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 10, 5));

	char buf[32];
	dssh_sigqueue_pop(&q, 10, 5, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "FIRST");

	/* Second not ready yet */
	ASSERT_FALSE(dssh_sigqueue_ready(&q, 15, 10));

	/* Second now ready */
	dssh_sigqueue_pop(&q, 20, 15, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "SECOND");

	/* Third ready */
	dssh_sigqueue_pop(&q, 30, 25, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "THIRD");

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_stdout_only_gate(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	dssh_sigqueue_push(&q, "USR1", 100, 0);

	/* stderr at 0 is fine (mark is 0), but stdout must reach 100 */
	ASSERT_FALSE(dssh_sigqueue_ready(&q, 99, 0));
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 100, 0));

	char buf[32];
	dssh_sigqueue_pop(&q, 100, 0, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "USR1");

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_stderr_only_gate(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	dssh_sigqueue_push(&q, "USR2", 0, 200);

	ASSERT_FALSE(dssh_sigqueue_ready(&q, 0, 199));
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 0, 200));

	char buf[32];
	dssh_sigqueue_pop(&q, 0, 200, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "USR2");

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_free_nonempty(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	dssh_sigqueue_push(&q, "HUP", 10, 10);
	dssh_sigqueue_push(&q, "TERM", 20, 20);

	/* Free without popping — should not leak */
	dssh_sigqueue_free(&q);
	ASSERT_NULL(q.head);
	ASSERT_NULL(q.tail);
	return TEST_PASS;
}

static int test_sigqueue_name_truncation(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	/* Name field is 32 bytes — push a long name */
	ASSERT_OK(dssh_sigqueue_push(&q, "AVERYLONGSIGNALNAMETHATISFORTYCHARS", 0, 0));

	char buf[32];
	dssh_sigqueue_pop(&q, 0, 0, buf, sizeof(buf));
	/* Should be truncated to 31 chars + NUL */
	ASSERT_EQ_U(strlen(buf), 31);

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_many_signals(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	for (int i = 0; i < 50; i++) {
		char name[8];
		snprintf(name, sizeof(name), "S%d", i);
		ASSERT_OK(dssh_sigqueue_push(&q, name, (size_t)i, 0));
	}

	char buf[32];
	for (int i = 0; i < 50; i++) {
		char expected[8];
		snprintf(expected, sizeof(expected), "S%d", i);
		const char *got = dssh_sigqueue_pop(&q, (size_t)i, 0, buf, sizeof(buf));
		ASSERT_NOT_NULL(got);
		ASSERT_STR_EQ(buf, expected);
	}

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_both_streams_must_drain(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	dssh_sigqueue_push(&q, "PIPE", 50, 75);

	/* stdout drained, stderr not */
	ASSERT_FALSE(dssh_sigqueue_ready(&q, 50, 74));
	/* stderr drained, stdout not */
	ASSERT_FALSE(dssh_sigqueue_ready(&q, 49, 75));
	/* Both drained */
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 50, 75));

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_zero_positions(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	dssh_sigqueue_push(&q, "ZERO", 0, 0);

	/* Immediately ready at position 0 */
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 0, 0));

	char buf[32];
	const char *name = dssh_sigqueue_pop(&q, 0, 0, buf, sizeof(buf));
	ASSERT_NOT_NULL(name);
	ASSERT_STR_EQ(buf, "ZERO");

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_pop_returns_buf(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	dssh_sigqueue_push(&q, "ALRM", 0, 0);

	char buf[32];
	const char *ret = dssh_sigqueue_pop(&q, 0, 0, buf, sizeof(buf));
	/* Return value should be the buf pointer */
	ASSERT_TRUE(ret == buf);

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_second_blocked_by_first(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	dssh_sigqueue_push(&q, "FIRST", 100, 100);
	dssh_sigqueue_push(&q, "SECOND", 50, 50);

	/* Second has lower positions but first must be consumed first.
	 * ready() checks the head, which is FIRST. */
	ASSERT_FALSE(dssh_sigqueue_ready(&q, 50, 50));
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 100, 100));

	char buf[32];
	dssh_sigqueue_pop(&q, 100, 100, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "FIRST");

	/* Now second is head and ready */
	ASSERT_TRUE(dssh_sigqueue_ready(&q, 50, 50));
	dssh_sigqueue_pop(&q, 50, 50, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "SECOND");

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

static int test_sigqueue_single_char_name(void)
{
	struct dssh_signal_queue q;
	dssh_sigqueue_init(&q);

	ASSERT_OK(dssh_sigqueue_push(&q, "X", 0, 0));

	char buf[32];
	dssh_sigqueue_pop(&q, 0, 0, buf, sizeof(buf));
	ASSERT_STR_EQ(buf, "X");

	dssh_sigqueue_free(&q);
	return TEST_PASS;
}

/* ================================================================
 * Accept queue
 * ================================================================ */

static int test_acceptqueue_init_free(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);
	ASSERT_NULL(q.head);
	ASSERT_NULL(q.tail);
	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_push_pop_single(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	const uint8_t type[] = "session";
	ASSERT_OK(dssh_acceptqueue_push(&q, 42, 65536, 32768,
	    type, sizeof(type) - 1));

	struct dssh_incoming_open *entry = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(entry);
	ASSERT_EQ(entry->peer_channel, 42);
	ASSERT_EQ(entry->peer_window, 65536);
	ASSERT_EQ(entry->peer_max_packet, 32768);
	ASSERT_EQ_U(entry->channel_type_len, 7);
	ASSERT_STR_EQ(entry->channel_type, "session");
	free(entry);

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_pop_empty(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	struct dssh_incoming_open *entry = dssh_acceptqueue_pop(&q);
	ASSERT_NULL(entry);

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_fifo_order(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	dssh_acceptqueue_push(&q, 1, 100, 50,
	    (const uint8_t *)"session", 7);
	dssh_acceptqueue_push(&q, 2, 200, 100,
	    (const uint8_t *)"direct-tcpip", 12);
	dssh_acceptqueue_push(&q, 3, 300, 150,
	    (const uint8_t *)"forwarded-tcpip", 15);

	struct dssh_incoming_open *e;

	e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ(e->peer_channel, 1);
	ASSERT_STR_EQ(e->channel_type, "session");
	free(e);

	e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ(e->peer_channel, 2);
	ASSERT_STR_EQ(e->channel_type, "direct-tcpip");
	free(e);

	e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ(e->peer_channel, 3);
	ASSERT_STR_EQ(e->channel_type, "forwarded-tcpip");
	free(e);

	/* Now empty */
	ASSERT_NULL(dssh_acceptqueue_pop(&q));

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_type_truncation(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	/* channel_type is 64 bytes — push a 70-byte type */
	uint8_t longtype[70];
	memset(longtype, 'A', sizeof(longtype));

	ASSERT_OK(dssh_acceptqueue_push(&q, 0, 0, 0,
	    longtype, sizeof(longtype)));

	struct dssh_incoming_open *e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ_U(e->channel_type_len, 63);
	ASSERT_EQ_U(strlen(e->channel_type), 63);
	/* All chars should be 'A' */
	for (size_t i = 0; i < 63; i++)
		ASSERT_EQ(e->channel_type[i], 'A');
	ASSERT_EQ(e->channel_type[63], '\0');
	free(e);

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_exact_63_type(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	uint8_t type63[63];
	memset(type63, 'B', 63);

	ASSERT_OK(dssh_acceptqueue_push(&q, 5, 500, 250,
	    type63, 63));

	struct dssh_incoming_open *e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ_U(e->channel_type_len, 63);
	ASSERT_EQ_U(strlen(e->channel_type), 63);
	free(e);

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_empty_type(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	ASSERT_OK(dssh_acceptqueue_push(&q, 10, 1000, 500,
	    (const uint8_t *)"", 0));

	struct dssh_incoming_open *e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ_U(e->channel_type_len, 0);
	ASSERT_EQ(e->channel_type[0], '\0');
	free(e);

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_many_entries(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	for (uint32_t i = 0; i < 30; i++) {
		char type[16];
		snprintf(type, sizeof(type), "ch%u", i);
		ASSERT_OK(dssh_acceptqueue_push(&q, i, i * 100, i * 50,
		    (const uint8_t *)type, strlen(type)));
	}

	for (uint32_t i = 0; i < 30; i++) {
		struct dssh_incoming_open *e = dssh_acceptqueue_pop(&q);
		ASSERT_NOT_NULL(e);
		ASSERT_EQ(e->peer_channel, i);
		ASSERT_EQ(e->peer_window, i * 100);
		ASSERT_EQ(e->peer_max_packet, i * 50);

		char expected[16];
		snprintf(expected, sizeof(expected), "ch%u", i);
		ASSERT_STR_EQ(e->channel_type, expected);
		free(e);
	}

	ASSERT_NULL(dssh_acceptqueue_pop(&q));

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_free_nonempty(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	dssh_acceptqueue_push(&q, 1, 100, 50,
	    (const uint8_t *)"session", 7);
	dssh_acceptqueue_push(&q, 2, 200, 100,
	    (const uint8_t *)"session", 7);

	dssh_acceptqueue_free(&q);
	ASSERT_NULL(q.head);
	ASSERT_NULL(q.tail);
	return TEST_PASS;
}

static int test_acceptqueue_peer_values(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	/* Use max uint32_t values */
	ASSERT_OK(dssh_acceptqueue_push(&q, UINT32_MAX, UINT32_MAX,
	    UINT32_MAX, (const uint8_t *)"x", 1));

	struct dssh_incoming_open *e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ_U(e->peer_channel, UINT32_MAX);
	ASSERT_EQ_U(e->peer_window, UINT32_MAX);
	ASSERT_EQ_U(e->peer_max_packet, UINT32_MAX);
	free(e);

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_push_pop_interleaved(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	for (uint32_t i = 0; i < 10; i++) {
		ASSERT_OK(dssh_acceptqueue_push(&q, i, 0, 0,
		    (const uint8_t *)"s", 1));
		struct dssh_incoming_open *e = dssh_acceptqueue_pop(&q);
		ASSERT_NOT_NULL(e);
		ASSERT_EQ(e->peer_channel, i);
		free(e);
	}

	ASSERT_NULL(dssh_acceptqueue_pop(&q));

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_type_nul_terminated(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	/* Push binary data as type — should be NUL-terminated */
	uint8_t binary[10] = {65, 66, 67, 0, 68, 69, 70, 0, 71, 72};
	dssh_acceptqueue_push(&q, 0, 0, 0, binary, 10);

	struct dssh_incoming_open *e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ_U(e->channel_type_len, 10);
	/* NUL terminator at position 63 max, but data has embedded NULs */
	ASSERT_EQ(e->channel_type[0], 'A');
	ASSERT_EQ(e->channel_type[1], 'B');
	ASSERT_EQ(e->channel_type[2], 'C');
	free(e);

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_zero_window_packet(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	ASSERT_OK(dssh_acceptqueue_push(&q, 0, 0, 0,
	    (const uint8_t *)"session", 7));

	struct dssh_incoming_open *e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ(e->peer_channel, 0);
	ASSERT_EQ(e->peer_window, 0);
	ASSERT_EQ(e->peer_max_packet, 0);
	free(e);

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_single_char_type(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	ASSERT_OK(dssh_acceptqueue_push(&q, 7, 1024, 512,
	    (const uint8_t *)"x", 1));

	struct dssh_incoming_open *e = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e);
	ASSERT_EQ_U(e->channel_type_len, 1);
	ASSERT_STR_EQ(e->channel_type, "x");
	free(e);

	dssh_acceptqueue_free(&q);
	return TEST_PASS;
}

static int test_acceptqueue_pop_does_not_affect_remaining(void)
{
	struct dssh_accept_queue q;
	dssh_acceptqueue_init(&q);

	dssh_acceptqueue_push(&q, 1, 100, 50,
	    (const uint8_t *)"alpha", 5);
	dssh_acceptqueue_push(&q, 2, 200, 100,
	    (const uint8_t *)"bravo", 5);

	struct dssh_incoming_open *e1 = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e1);
	ASSERT_STR_EQ(e1->channel_type, "alpha");
	free(e1);

	/* Second entry still intact */
	struct dssh_incoming_open *e2 = dssh_acceptqueue_pop(&q);
	ASSERT_NOT_NULL(e2);
	ASSERT_EQ(e2->peer_channel, 2);
	ASSERT_EQ(e2->peer_window, 200);
	ASSERT_EQ(e2->peer_max_packet, 100);
	ASSERT_STR_EQ(e2->channel_type, "bravo");
	free(e2);

	dssh_acceptqueue_free(&q);
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
	{ "msgqueue_init_free",               test_msgqueue_init_free },
	{ "msgqueue_push_pop_single",         test_msgqueue_push_pop_single },
	{ "msgqueue_pop_empty",               test_msgqueue_pop_empty },
	{ "msgqueue_peek_empty",              test_msgqueue_peek_empty },
	{ "msgqueue_peek_size",               test_msgqueue_peek_size },
	{ "msgqueue_pop_toolong",             test_msgqueue_pop_toolong },
	{ "msgqueue_fifo_order",              test_msgqueue_fifo_order },
	{ "msgqueue_total_bytes",             test_msgqueue_total_bytes },
	{ "msgqueue_many_messages",           test_msgqueue_many_messages },
	{ "msgqueue_free_nonempty",           test_msgqueue_free_nonempty },
	{ "msgqueue_zero_len_message",        test_msgqueue_zero_len_message },
	{ "msgqueue_pop_after_toolong",       test_msgqueue_pop_after_toolong },
	{ "msgqueue_peek_after_pop",          test_msgqueue_peek_after_pop },
	{ "msgqueue_push_pop_interleaved",    test_msgqueue_push_pop_interleaved },
	{ "msgqueue_varying_sizes",           test_msgqueue_varying_sizes },
	{ "msgqueue_exact_bufsz_pop",         test_msgqueue_exact_bufsz_pop },
	{ "msgqueue_count_tracking",          test_msgqueue_count_tracking },
	{ "msgqueue_large_message",           test_msgqueue_large_message },
	{ "msgqueue_toolong_preserves_count", test_msgqueue_toolong_preserves_count },

	/* Signal queue */
	{ "sigqueue_init_free",               test_sigqueue_init_free },
	{ "sigqueue_push_ready_pop",          test_sigqueue_push_ready_pop },
	{ "sigqueue_pop_not_ready",           test_sigqueue_pop_not_ready },
	{ "sigqueue_pop_empty",               test_sigqueue_pop_empty },
	{ "sigqueue_ready_empty",             test_sigqueue_ready_empty },
	{ "sigqueue_fifo_order",              test_sigqueue_fifo_order },
	{ "sigqueue_ordered_positions",       test_sigqueue_ordered_positions },
	{ "sigqueue_stdout_only_gate",        test_sigqueue_stdout_only_gate },
	{ "sigqueue_stderr_only_gate",        test_sigqueue_stderr_only_gate },
	{ "sigqueue_free_nonempty",           test_sigqueue_free_nonempty },
	{ "sigqueue_name_truncation",         test_sigqueue_name_truncation },
	{ "sigqueue_many_signals",            test_sigqueue_many_signals },
	{ "sigqueue_both_streams_must_drain", test_sigqueue_both_streams_must_drain },
	{ "sigqueue_zero_positions",          test_sigqueue_zero_positions },
	{ "sigqueue_pop_returns_buf",         test_sigqueue_pop_returns_buf },
	{ "sigqueue_second_blocked_by_first", test_sigqueue_second_blocked_by_first },
	{ "sigqueue_single_char_name",        test_sigqueue_single_char_name },

	/* Accept queue */
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
};

DSSH_TEST_MAIN(tests)
