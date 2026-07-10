/* syncretro_chunk.h -- cut the core's PCM stream into fixed-size mono chunks.
 *
 * PURE: no I/O, no config, no termgfx. Its own translation unit so test_chunk
 * links it alone. The streaming that consumes these chunks is syncretro_audio.c
 * (see M4_AUDIO.md).
 */
#ifndef SYNCRETRO_CHUNK_H_
#define SYNCRETRO_CHUNK_H_

#include <stddef.h>
#include <stdint.h>

/* --- the chunk accumulator (test_chunk.c covers it) ------------------------
 * Fills with the LEFT sample of each interleaved-stereo frame. FreeIntv emits
 * Audio(c, c), so L == R and the downmix is lossless. */
typedef struct {
	int16_t *buf;   /* cap mono samples */
	size_t   cap;   /* frames per chunk */
	size_t   len;   /* frames accumulated so far */
} sr_chunk_t;

int    sr_chunk_init(sr_chunk_t *c, size_t frames);   /* 1 = ok, 0 = OOM */
void   sr_chunk_free(sr_chunk_t *c);
/* Append from interleaved stereo; returns the per-channel frames CONSUMED,
 * which stops at the chunk boundary. The caller resets and re-appends the
 * remainder. */
size_t sr_chunk_append(sr_chunk_t *c, const int16_t *pcm, size_t frames);
int    sr_chunk_full(const sr_chunk_t *c);
/* 1 only when the chunk is FULL and every sample is zero. An empty or partial
 * chunk is never "silent" -- silence is a property the encoder acts on. */
int    sr_chunk_is_silent(const sr_chunk_t *c);
void   sr_chunk_reset(sr_chunk_t *c);

#endif /* SYNCRETRO_CHUNK_H_ */
