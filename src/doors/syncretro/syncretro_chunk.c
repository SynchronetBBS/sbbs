/* syncretro_chunk.c -- see syncretro_chunk.h. */
#include "syncretro_chunk.h"

#include <stdlib.h>
#include <string.h>

/* --- the chunk accumulator -------------------------------------------------- */

int sr_chunk_init(sr_chunk_t *c, size_t frames)
{
	c->buf = malloc(frames * sizeof *c->buf);
	c->len = 0;
	if (c->buf == NULL) {
		c->cap = 0;   /* every other entry point then no-ops instead of
		               * writing through a NULL buf */
		return 0;
	}
	c->cap = frames;
	return 1;
}

void sr_chunk_free(sr_chunk_t *c)
{
	free(c->buf);
	c->buf = NULL;
	c->cap = c->len = 0;
}

size_t sr_chunk_append(sr_chunk_t *c, const int16_t *pcm, size_t frames)
{
	size_t room = c->cap - c->len;
	size_t i;

	if (frames > room)
		frames = room;            /* stop at the boundary; caller re-appends */
	for (i = 0; i < frames; i++)
		c->buf[c->len + i] = pcm[2 * i];   /* LEFT sample: L == R by contract */
	c->len += frames;
	return frames;
}

int sr_chunk_full(const sr_chunk_t *c)
{
	return c->len >= c->cap;
}

int sr_chunk_is_silent(const sr_chunk_t *c)
{
	size_t i;

	if (!sr_chunk_full(c))
		return 0;
	for (i = 0; i < c->len; i++)
		if (c->buf[i] != 0)
			return 0;
	return 1;
}

void sr_chunk_reset(sr_chunk_t *c)
{
	c->len = 0;
}
