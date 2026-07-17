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

void
termgfx_chunk_reset(termgfx_chunk_t *c)
{
	c->len = 0;
}
