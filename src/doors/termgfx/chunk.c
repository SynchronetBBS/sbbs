// chunk.c -- the pure PCM chunk accumulator, split out of audio_stream.c.
//
// This is its own translation unit ON PURPOSE: it is plain arithmetic over a
// sample buffer -- no I/O, no external libs -- while the rest of
// audio_stream.c (the streaming state machine) calls audio.c's APC builders
// and, through them, libsndfile. Keeping the accumulator in a separate file
// lets test/CMakeLists.txt compile THIS source directly and link no termgfx
// library at all, so the riskiest pure code (raw pointer arithmetic over a
// sample buffer) gets a fast, dependency-free unit test instead of being
// hostage to the heaviest dependency in the tree. See audio_stream.h for the
// public contract; it declares this half alongside the state machine.
#include "audio_stream.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

int
termgfx_chunk_init(termgfx_chunk_t *c, size_t frames, int channels)
{
	if (channels < 1)
		channels = 1;
	else if (channels > 2)
		channels = 2;   // contract is 1 or 2; append()'s else branch relies on this
	c->channels = channels;
	c->len      = 0;
	// Overflow guard: frames * channels * sizeof must not wrap.
	if (frames > SIZE_MAX / ((size_t)channels * sizeof *c->buf)) {
		c->buf = NULL;
		c->cap = 0;
		return 0;
	}
	c->buf = malloc(frames * (size_t)channels * sizeof *c->buf);
	if (c->buf == NULL) {
		c->cap = 0;   // every other entry point then no-ops instead of
		              // writing through a NULL buf
		return 0;
	}
	c->cap = frames;
	return 1;
}

void
termgfx_chunk_free(termgfx_chunk_t *c)
{
	free(c->buf);
	c->buf      = NULL;
	c->cap      = c->len = 0;
	c->channels = 0;
}

size_t
termgfx_chunk_append(termgfx_chunk_t *c, const int16_t *pcm, size_t frames)
{
	size_t room = c->cap - c->len;
	size_t i;

	if (frames > room)
		frames = room;   // stop at the boundary; caller re-appends
	if (c->channels == 1) {
		for (i = 0; i < frames; i++)
			c->buf[c->len + i] = pcm[2 * i];   // LEFT: L == R by contract
	} else {
		memcpy(c->buf + c->len * 2, pcm, frames * 2 * sizeof *c->buf);
	}
	c->len += frames;
	return frames;
}

// A zero-cap chunk (init() OOM'd, or free()'d) is NOT full: `len >= cap` is
// true when both are 0, and that lie hangs the caller. feed()'s loop tests
// full() BEFORE its `used == 0` guard, so a full-reporting empty chunk sends it
// round forever -- append copies nothing, chunk_closed() sees len == 0 and
// returns, reset() no-ops, and the frame counter never moves. The state machine
// only avoids this today because caps() pairs chunk_free() with state = OFF,
// i.e. the guarantee lives in another function (now another translation unit).
// Keep it here, where the guard is.
int
termgfx_chunk_full(const termgfx_chunk_t *c)
{
	return c->cap > 0 && c->len >= c->cap;
}

int
termgfx_chunk_is_silent(const termgfx_chunk_t *c)
{
	size_t i;

	if (!termgfx_chunk_full(c))
		return 0;
	for (i = 0; i < c->len * (size_t)c->channels; i++)
		if (c->buf[i] != 0)
			return 0;
	return 1;
}

// ---- DC blocker -----------------------------------------------------------
//
// The classic one-pole:   y[n] = x[n] - x[n-1] + r * y[n-1]
//
// with the pole r just inside the unit circle. Its corner sits at
// (1 - r) * rate / 2pi, so r = 1 - 2pi * fc / rate places it at fc. See
// audio_stream.h for WHY this filter is here at all -- it is the encoder's
// requirement, not a tonal choice.
#define TERMGFX_DCBLOCK_HZ 10.0

void
termgfx_dcblock_init(termgfx_dcblock_t *d, int rate)
{
	if (rate < 8000)
		rate = 8000;   // a nonsense rate must not put the pole outside the circle
	d->r    = 1.0 - (2.0 * 3.14159265358979323846 * TERMGFX_DCBLOCK_HZ / rate);
	d->x[0] = d->x[1] = 0.0;
	d->y[0] = d->y[1] = 0.0;
}

// The state is DOUBLE, not int16: an integer accumulator at this pole rounds
// its own feedback every sample and settles into a limit cycle -- a low-level
// buzz replacing the one we came to remove.
static int16_t
dcblock_one(termgfx_dcblock_t *d, int ch, int16_t in)
{
	double y = (double)in - d->x[ch] + d->r * d->y[ch];
	double v;

	d->x[ch] = (double)in;
	d->y[ch] = y;

	v = floor(y + 0.5);
	if (v > 32767.0)
		v = 32767.0;
	else if (v < -32768.0)
		v = -32768.0;
	return (int16_t)v;
}

void
termgfx_dcblock_apply(termgfx_dcblock_t *d, int16_t *pcm, size_t frames)
{
	size_t i;

	if (d == NULL || pcm == NULL)
		return;
	for (i = 0; i < frames; i++) {
		pcm[2 * i]     = dcblock_one(d, 0, pcm[2 * i]);
		pcm[2 * i + 1] = dcblock_one(d, 1, pcm[2 * i + 1]);
	}
}

void
termgfx_chunk_reset(termgfx_chunk_t *c)
{
	c->len = 0;
}
