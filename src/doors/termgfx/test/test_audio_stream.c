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
	int16_t *       batch;
	size_t          used, i;

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
		int16_t         one[2] = { 1, 1 };

		CHECK(termgfx_chunk_init(&bad, (size_t)-1 / sizeof(int16_t), 1) == 0);
		CHECK(bad.buf == NULL);
		CHECK(bad.cap == 0);
		CHECK(termgfx_chunk_append(&bad, one, 1) == 0);   /* copies nothing */
		CHECK(bad.len == 0);
		/* A zero-cap chunk must NOT report full: len >= cap is true when both
		 * are 0, and the state machine's feed() loop tests full() before its
		 * used == 0 guard -- a full-reporting empty chunk spins it forever,
		 * hanging the mixer tick and the game. Nor is it silent: silence is a
		 * property of a FULL chunk (audio_stream.h). */
		CHECK(termgfx_chunk_full(&bad) == 0);
		CHECK(termgfx_chunk_is_silent(&bad) == 0);
		termgfx_chunk_reset(&bad);
		termgfx_chunk_free(&bad);
		termgfx_chunk_free(&bad);                          /* idempotent */
		/* ...and the same holds for a FREED chunk, which is how the state
		 * machine actually reaches cap == 0 (caps() frees on a tier it cannot
		 * stream). */
		CHECK(termgfx_chunk_full(&bad) == 0);
		CHECK(termgfx_chunk_is_silent(&bad) == 0);
	}

	termgfx_chunk_free(&c);

	/* --- stereo: both channels are kept, interleaved ------------------- */
	{
		termgfx_chunk_t c;
		int16_t         batch[4 * 2];
		size_t          used;
		int             i;

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
		int16_t         batch[4 * 2];
		size_t          i;

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
		int16_t *       batch = stereo(4, 1234);

		CHECK(termgfx_chunk_init(&c, 4, 1) == 1);
		termgfx_chunk_append(&c, batch, 4);
		CHECK(c.buf[0] == 1234 && c.buf[3] == 1234);
		termgfx_chunk_free(&c);
		free(batch);
	}

	/* channels is CLAMPED to 1..2, and the clamp is load-bearing: append()'s
	 * else branch memcpy's frames * 2 samples, so a chunk that recorded
	 * channels == 6 while its buf was sized for 2 would be written past its
	 * end. Under-range clamps up to mono for the same reason -- buf is sized
	 * cap * channels either way. */
	{
		termgfx_chunk_t c;
		int16_t *       batch;

		CHECK(termgfx_chunk_init(&c, 4, 6) == 1);
		CHECK(c.channels == 2);        /* not 6: buf holds cap * 2 samples */
		batch = stereo(4, 321);
		CHECK(termgfx_chunk_append(&c, batch, 4) == 4);
		CHECK(c.buf[0] == 321 && c.buf[7] == 321);   /* both channels kept */
		termgfx_chunk_free(&c);
		free(batch);

		CHECK(termgfx_chunk_init(&c, 4, 0) == 1);
		CHECK(c.channels == 1);        /* not 0: that would size buf at zero */
		termgfx_chunk_free(&c);

		CHECK(termgfx_chunk_init(&c, 4, -3) == 1);
		CHECK(c.channels == 1);
		termgfx_chunk_free(&c);
	}

	/* ---- the DC blocker -------------------------------------------------
	 * Ogg/Opus cannot carry DC: the encoder high-passes it away, so a chunk
	 * of a constant level decodes as a decay from that level toward zero and
	 * every chunk boundary is a step -- an impulse train at 1/chunk_ms. The
	 * blocker removes the offset before the PCM is ever chunked. */
	{
		termgfx_dcblock_t d;
		int16_t *         batch;
		size_t            n;
		int               settled;

		/* A constant level -- Xenophobe's idle DAC, which is silence at the
		 * wrong level: peak-to-peak zero, and NOT caught by the all-zeros
		 * silence test. It must decay to EXACTLY zero, because that is what
		 * puts it back on the free cached-silence path.
		 *
		 * "Settled" is asserted as a TIME, not just an end state, because the
		 * time is the cost: until it settles the chunks still hold a (smoothly
		 * decaying, so inaudible) ramp rather than the zeros that cost nothing.
		 * A 10 Hz corner is a 15.9 ms time constant, and reaching zero from
		 * full scale takes ln(65536) ~ 11 of them, so ~180 ms is the ceiling
		 * for ANY offset. Failing this means the corner moved. */
		termgfx_dcblock_init(&d, 48000);
		n     = 48000 / 2;                    /* 500 ms: twice the ceiling */
		batch = stereo(n, -17536);
		termgfx_dcblock_apply(&d, batch, n);
		CHECK(batch[0] == -17536);            /* the first sample passes through */
		settled = -1;
		for (i = n; i-- > 0; )
			if (batch[2 * i] != 0 || batch[2 * i + 1] != 0) {
				settled = (int)(i + 1);       /* first index that is zero for good */
				break;
			}
		CHECK(settled >= 0 && settled < 48000 / 5);   /* under 200 ms */
		free(batch);

		/* Joust idles POSITIVE (+7710): the sign must not matter. */
		termgfx_dcblock_init(&d, 48000);
		batch = stereo(n, 7710);
		termgfx_dcblock_apply(&d, batch, n);
		settled = -1;
		for (i = n; i-- > 0; )
			if (batch[2 * i] != 0) {
				settled = (int)(i + 1);
				break;
			}
		CHECK(settled >= 0 && settled < 48000 / 5);
		free(batch);

		/* Digital silence in stays digital silence out -- a well-behaved
		 * source must not be pushed OFF the cached-silence path. */
		termgfx_dcblock_init(&d, 48000);
		batch = stereo(n, 0);
		termgfx_dcblock_apply(&d, batch, n);
		settled = 1;
		for (i = 0; i < n; i++)
			if (batch[2 * i] != 0 || batch[2 * i + 1] != 0)
				settled = 0;
		CHECK(settled);
		free(batch);

		/* Audio survives. A 1 kHz square at 48 kHz is two orders of magnitude
		 * above the ~10 Hz corner, so its amplitude must come through
		 * essentially intact -- a blocker that ate the signal would "fix" the
		 * rumble by removing the game's sound. */
		{
			int16_t peak = 0;

			termgfx_dcblock_init(&d, 48000);
			batch = stereo(n, 0);
			for (i = 0; i < n; i++) {
				int16_t v = (i / 24) % 2 ? 8000 : -8000;   /* 1 kHz square */

				batch[2 * i] = batch[2 * i + 1] = v;
			}
			termgfx_dcblock_apply(&d, batch, n);
			for (i = n / 2; i < n; i++)
				if (batch[2 * i] > peak)
					peak = batch[2 * i];
			CHECK(peak > 7800);           /* within ~2.5% of the 8000 input */
			free(batch);
		}

		/* The two channels are filtered INDEPENDENTLY: a right-channel-only
		 * offset must not leak into the left. */
		termgfx_dcblock_init(&d, 48000);
		batch = stereo(n, 0);
		for (i = 0; i < n; i++)
			batch[2 * i + 1] = -20000;
		termgfx_dcblock_apply(&d, batch, n);
		settled = 1;
		for (i = 0; i < n; i++)
			if (batch[2 * i] != 0)
				settled = 0;
		CHECK(settled);
		free(batch);
	}

	printf("%s: %d failure(s)\n", failures ? "FAIL" : "ok", failures);
	return failures != 0;
}
