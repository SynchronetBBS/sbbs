/* test_chunk.c -- the chunk accumulator: the only part of M4 with edge cases
 * and no I/O. Everything downstream (encode, upload, queue) is exercised by
 * the fake terminal, not here. */
#include "syncretro_chunk.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
			failures++; \
		} \
	} while (0)

/* Build `frames` of interleaved stereo where L == R == value. */
static int16_t *stereo(size_t frames, int16_t value)
{
	int16_t *p = malloc(frames * 2 * sizeof *p);
	size_t   i;

	for (i = 0; i < frames; i++) {
		p[2 * i]     = value;
		p[2 * i + 1] = value;
	}
	return p;
}

int main(void)
{
	sr_chunk_t c;
	int16_t   *batch;
	size_t     used, i;

	/* A 4410-frame chunk is not closed by one 735-frame batch... */
	CHECK(sr_chunk_init(&c, 4410) == 1);
	batch = stereo(735, 1000);
	used  = sr_chunk_append(&c, batch, 735);
	CHECK(used == 735);
	CHECK(c.len == 735);
	CHECK(!sr_chunk_full(&c));

	/* ...but six of them close it exactly, with no remainder. */
	for (i = 1; i < 6; i++)
		CHECK(sr_chunk_append(&c, batch, 735) == 735);
	CHECK(c.len == 4410);
	CHECK(sr_chunk_full(&c));
	free(batch);

	/* The mono downmix takes the LEFT sample of each frame. */
	CHECK(c.buf[0] == 1000 && c.buf[4409] == 1000);

	/* A batch that straddles the boundary is consumed only up to it, and the
	 * caller re-appends the remainder after resetting. */
	sr_chunk_reset(&c);
	CHECK(c.len == 0 && !sr_chunk_full(&c));
	batch = stereo(5000, 7);
	used  = sr_chunk_append(&c, batch, 5000);
	CHECK(used == 4410);            /* stops at the boundary */
	CHECK(sr_chunk_full(&c));
	sr_chunk_reset(&c);
	used = sr_chunk_append(&c, batch + 2 * 4410, 5000 - 4410);
	CHECK(used == 590);
	CHECK(c.len == 590 && !sr_chunk_full(&c));
	free(batch);

	/* Silence detection: an all-zero chunk is silent... */
	sr_chunk_reset(&c);
	batch = stereo(4410, 0);
	CHECK(sr_chunk_append(&c, batch, 4410) == 4410);
	CHECK(sr_chunk_full(&c));
	CHECK(sr_chunk_is_silent(&c) == 1);
	free(batch);

	/* ...and ONE non-zero sample anywhere defeats it, including the last. */
	sr_chunk_reset(&c);
	batch = stereo(4410, 0);
	batch[2 * 4409] = -1;
	CHECK(sr_chunk_append(&c, batch, 4410) == 4410);
	CHECK(sr_chunk_is_silent(&c) == 0);
	free(batch);

	sr_chunk_reset(&c);
	batch = stereo(4410, 0);
	batch[0] = 1;
	CHECK(sr_chunk_append(&c, batch, 4410) == 4410);
	CHECK(sr_chunk_is_silent(&c) == 0);
	free(batch);

	/* A zero-frame append is a no-op, not a crash. Pass a LIVE pointer: handing
	 * sr_chunk_append() a freed one would be undefined behaviour even though
	 * frames == 0 means it never dereferences it. */
	sr_chunk_reset(&c);
	batch = stereo(1, 123);
	CHECK(sr_chunk_append(&c, batch, 0) == 0);
	CHECK(c.len == 0);
	free(batch);

	/* An empty chunk is not "silent" -- silence is a property of a FULL chunk. */
	CHECK(sr_chunk_is_silent(&c) == 0);

	/* A failed init must leave a struct on which every other entry point is a
	 * safe no-op, not one that writes through a NULL buf. SIZE_MAX frames
	 * cannot be allocated. */
	{
		sr_chunk_t bad;
		int16_t    one[2] = { 1, 1 };

		CHECK(sr_chunk_init(&bad, (size_t)-1 / sizeof(int16_t)) == 0);
		CHECK(bad.buf == NULL);
		CHECK(bad.cap == 0);
		CHECK(sr_chunk_append(&bad, one, 1) == 0);   /* copies nothing */
		CHECK(bad.len == 0);
		sr_chunk_reset(&bad);
		sr_chunk_free(&bad);
		sr_chunk_free(&bad);                          /* idempotent */
	}

	sr_chunk_free(&c);
	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
