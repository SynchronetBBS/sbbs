/* test_audio_stream.c -- the chunk accumulator: the part of the streaming
 * module with edge cases and no I/O. Everything downstream (encode, upload,
 * queue) is exercised by a door's fake terminal, not here. */
#include "audio_stream.h"

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
	termgfx_chunk_t c;
	int16_t   *batch;
	size_t     used, i;

	/* A 4410-frame chunk is not closed by one 735-frame batch... */
	CHECK(termgfx_chunk_init(&c, 4410, 1) == 1);
	batch = stereo(735, 1000);
	used  = termgfx_chunk_append(&c, batch, 735);
	CHECK(used == 735);
	CHECK(c.len == 735);
	CHECK(!termgfx_chunk_full(&c));

	/* ...but six of them close it exactly, with no remainder. */
	for (i = 1; i < 6; i++)
		CHECK(termgfx_chunk_append(&c, batch, 735) == 735);
	CHECK(c.len == 4410);
	CHECK(termgfx_chunk_full(&c));
	free(batch);

	/* The mono downmix takes the LEFT sample of each frame. */
	CHECK(c.buf[0] == 1000 && c.buf[4409] == 1000);

	/* A batch that straddles the boundary is consumed only up to it, and the
	 * caller re-appends the remainder after resetting. */
	termgfx_chunk_reset(&c);
	CHECK(c.len == 0 && !termgfx_chunk_full(&c));
	batch = stereo(5000, 7);
	used  = termgfx_chunk_append(&c, batch, 5000);
	CHECK(used == 4410);            /* stops at the boundary */
	CHECK(termgfx_chunk_full(&c));
	termgfx_chunk_reset(&c);
	used = termgfx_chunk_append(&c, batch + 2 * 4410, 5000 - 4410);
	CHECK(used == 590);
	CHECK(c.len == 590 && !termgfx_chunk_full(&c));
	free(batch);

	/* Silence detection: an all-zero chunk is silent... */
	termgfx_chunk_reset(&c);
	batch = stereo(4410, 0);
	CHECK(termgfx_chunk_append(&c, batch, 4410) == 4410);
	CHECK(termgfx_chunk_full(&c));
	CHECK(termgfx_chunk_is_silent(&c) == 1);
	free(batch);

	/* ...and ONE non-zero sample anywhere defeats it, including the last. */
	termgfx_chunk_reset(&c);
	batch = stereo(4410, 0);
	batch[2 * 4409] = -1;
	CHECK(termgfx_chunk_append(&c, batch, 4410) == 4410);
	CHECK(termgfx_chunk_is_silent(&c) == 0);
	free(batch);

	termgfx_chunk_reset(&c);
	batch = stereo(4410, 0);
	batch[0] = 1;
	CHECK(termgfx_chunk_append(&c, batch, 4410) == 4410);
	CHECK(termgfx_chunk_is_silent(&c) == 0);
	free(batch);

	/* A zero-frame append is a no-op, not a crash. Pass a LIVE pointer: handing
	 * termgfx_chunk_append() a freed one would be undefined behaviour even though
	 * frames == 0 means it never dereferences it. */
	termgfx_chunk_reset(&c);
	batch = stereo(1, 123);
	CHECK(termgfx_chunk_append(&c, batch, 0) == 0);
	CHECK(c.len == 0);
	free(batch);

	/* An empty chunk is not "silent" -- silence is a property of a FULL chunk. */
	CHECK(termgfx_chunk_is_silent(&c) == 0);

	/* A failed init must leave a struct on which every other entry point is a
	 * safe no-op, not one that writes through a NULL buf. SIZE_MAX frames
	 * cannot be allocated. */
	{
		termgfx_chunk_t bad;
		int16_t    one[2] = { 1, 1 };

		CHECK(termgfx_chunk_init(&bad, (size_t)-1 / sizeof(int16_t), 1) == 0);
		CHECK(bad.buf == NULL);
		CHECK(bad.cap == 0);
		CHECK(termgfx_chunk_append(&bad, one, 1) == 0);   /* copies nothing */
		CHECK(bad.len == 0);
		termgfx_chunk_reset(&bad);
		termgfx_chunk_free(&bad);
		termgfx_chunk_free(&bad);                          /* idempotent */
	}

	termgfx_chunk_free(&c);

	/* --- stereo: both channels are kept, interleaved ------------------- */
	{
		termgfx_chunk_t c;
		int16_t    batch[4 * 2];
		size_t     used;
		int        i;

		/* Frame i = (100+i, 200+i): L and R differ, so a downmix that
		 * dropped the right channel would be caught. */
		for (i = 0; i < 4; i++) {
			batch[2 * i]     = (int16_t)(100 + i);
			batch[2 * i + 1] = (int16_t)(200 + i);
		}
		CHECK(termgfx_chunk_init(&c, 4, 2) == 1);
		used = termgfx_chunk_append(&c, batch, 4);
		CHECK(used == 4);              /* per-CHANNEL frames, not samples */
		CHECK(termgfx_chunk_full(&c) == 1);
		CHECK(c.buf[0] == 100 && c.buf[1] == 200);   /* frame 0: L, R */
		CHECK(c.buf[6] == 103 && c.buf[7] == 203);   /* frame 3: L, R */
		termgfx_chunk_free(&c);
	}

	/* A stereo chunk is silent only when BOTH channels are zero: a
	 * right-channel-only signal must defeat the silence path, which the
	 * mono scan (iterating len, not len*channels) would have missed. */
	{
		termgfx_chunk_t c;
		int16_t    batch[4 * 2];
		size_t     i;

		for (i = 0; i < 4 * 2; i++)
			batch[i] = 0;
		CHECK(termgfx_chunk_init(&c, 4, 2) == 1);
		termgfx_chunk_append(&c, batch, 4);
		CHECK(termgfx_chunk_is_silent(&c) == 1);
		termgfx_chunk_reset(&c);
		batch[7] = -1;                 /* last frame, RIGHT channel only */
		termgfx_chunk_append(&c, batch, 4);
		CHECK(termgfx_chunk_is_silent(&c) == 0);
		termgfx_chunk_free(&c);
	}

	/* Mono is unchanged: the left sample of each frame, as before. */
	{
		termgfx_chunk_t c;
		int16_t   *batch = stereo(4, 1234);

		CHECK(termgfx_chunk_init(&c, 4, 1) == 1);
		termgfx_chunk_append(&c, batch, 4);
		CHECK(c.buf[0] == 1234 && c.buf[3] == 1234);
		termgfx_chunk_free(&c);
		free(batch);
	}

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
