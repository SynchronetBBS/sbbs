// audio_stream.h -- a continuous PCM stream -> SyncTERM's audio APC FIFO.
//
// Some doors have discrete sounds to play (a gunshot, a music track) and want
// audio_mgr.h. Others have one continuous mixed PCM stream and no useful
// per-sound boundary at all: a libretro core's output, ScummVM's mixer. This
// module is for those -- it cuts the stream into fixed-size chunks, Opus-encodes
// each, and feeds SyncTERM's channel FIFO (A;Queue appends, so consecutive
// chunks play back to back).
//
// WHY A CUSHION. Chunks are produced in real time and consumed in real time, so
// the FIFO's depth is whatever was established at the start and never grows on
// its own. A FIFO fed one chunk at a time from empty stays one deep and
// underruns on the first jitter. That is why playback waits for `prebuffer`
// chunks: those chunks ARE the latency budget and the entire jitter tolerance.
// On an underrun we re-prime rather than resume, for the same reason.
//
// This layer is I/O-free in the same sense as caps.h: the door supplies a put/
// flush/backlog vtable and feeds it the parsed capability replies. It holds
// state (unlike audio.h) but touches no socket.
#ifndef TERMGFX_AUDIO_STREAM_H_
#define TERMGFX_AUDIO_STREAM_H_

#include <stddef.h>
#include <stdint.h>

// ---- chunk accumulator ---------------------------------------------------

// Holds `channels` interleaved samples per frame. Mono (channels == 1) keeps the
// LEFT sample of each input frame: a libretro core like FreeIntv emits
// Audio(c, c), so L == R and the downmix is lossless and halves everything. A
// source with genuine stereo content (ScummVM's mixer) passes channels == 2.
// Input is ALWAYS interleaved stereo either way -- `channels` describes what we
// keep, not what we are handed.
typedef struct {
	int16_t *buf;        // cap * channels interleaved samples
	size_t cap;          // FRAMES per chunk (not samples: see channels)
	size_t len;          // frames accumulated so far
	int channels;        // 1 = downmix to left, 2 = keep both
} termgfx_chunk_t;

// 1 = ok, 0 = OOM (leaves a safe struct whose every entry point no-ops).
int  termgfx_chunk_init(termgfx_chunk_t *c, size_t frames, int channels);
void termgfx_chunk_free(termgfx_chunk_t *c);

// Append from interleaved stereo; returns the per-channel frames CONSUMED,
// which stops at the chunk boundary. The caller resets and re-appends the
// remainder.
size_t termgfx_chunk_append(termgfx_chunk_t *c, const int16_t *pcm, size_t frames);
int    termgfx_chunk_full(const termgfx_chunk_t *c);

// 1 only when the chunk is FULL and every sample is zero. An empty or partial
// chunk is never "silent" -- silence is a property the encoder acts on.
int    termgfx_chunk_is_silent(const termgfx_chunk_t *c);
void   termgfx_chunk_reset(termgfx_chunk_t *c);

// ---- the streaming state machine -----------------------------------------

// Cache names the ring rotates through: "<prefix>/0".."<prefix>/3", plus
// "<prefix>/z" for the silence chunk. Four is enough because A;Queue empties
// the slot and APCs are processed in stream order, so a name is free again long
// before the ring comes back around to it.
#define TERMGFX_STREAM_RING 4

// Door-supplied output seam. `ctx` is opaque to this module.
typedef struct {
	void (*put)(void *ctx, const void *buf, size_t len);
	int (*flush)(void *ctx);
	size_t (*backlog)(void *ctx);
	void *ctx;
} termgfx_stream_io_t;

// Everything the module needs to know about the door, taken BY VALUE at
// create() and clamped there once. `name` and `cache_prefix` are copied into
// the stream, so the caller need not keep them alive.
typedef struct {
	int enabled;                // 0 = module never emits a byte
	double quality;             // Opus VBR 0.01..1.0 (TERMGFX_MUSIC_QUALITY_DEFAULT)
	float volume_db;            // channel level in dB (0 = unity); at or below
	                            // TERMGFX_DB_MUTE the module stops sending entirely
	                            // (a muted player pays ZERO uplink for sound)
	int chunk_ms;               // 50..250
	int prebuffer;              // 2..8 chunks held before playback starts
	int channels;               // 1 or 2 (what the accumulator keeps)
	int rate;                   // source sample rate, Hz
	int ch;                     // SyncTERM mixer channel (2..15)
	int slot;                   // patch slot 0..255
	const char *name;           // door name for diagnostics ("syncscumm")
	const char *cache_prefix;   // cache-name prefix, e.g. "s" -> "s/0".."s/z"
} termgfx_stream_cfg_t;

typedef struct termgfx_stream termgfx_stream_t;

// NULL when cfg->enabled is 0, or on OOM. Every entry point below is NULL-safe,
// so a door can create unconditionally and forward blindly. The rate is the
// SOURCE's and is needed up front: a chunk's length in FRAMES is chunk_ms of
// audio at that rate, and the cushion is counted in chunks.
termgfx_stream_t *termgfx_stream_create(const termgfx_stream_cfg_t *cfg,
                                        const termgfx_stream_io_t *io);
// A;Flush the channel (see _stop) and report telemetry to stderr. Idempotent.
void   termgfx_stream_destroy(termgfx_stream_t *s);

// Emit the libsndfile capability query; feed the parsed answer back to _caps.
void   termgfx_stream_probe(termgfx_stream_t *s);
// The probe reply: tier 1 = libsndfile (stream), 0 = tone-only, -1 = none.
// Anything but 1 disables the stream permanently -- a Load the terminal ignores
// would leave a Queue playing an empty slot.
void   termgfx_stream_caps(termgfx_stream_t *s, int tier);

// Enable the inline A;LoadBlob path (CTerm >= 1.329). Off by default; set from
// the door's DA1 reply once the client's version is known.
void   termgfx_stream_set_blob_ok(termgfx_stream_t *s, int ok);
int    termgfx_stream_blob_active(const termgfx_stream_t *s);

// `CSI = 7 ; ch ; 0 n`: that channel's FIFO drained.
void   termgfx_stream_underrun(termgfx_stream_t *s, int ch);

// The source stopped (or restarted) producing PCM. This and _reset/_volume_step
// below came from syncretro and are live there -- a door screen pauses, Ctrl-R
// resets, '+'/'-' step the volume -- so they are exercised every session, not
// merely implemented. The two GETTERS are the exception: nothing calls _volume
// or _muted, so they are untested by use.
void   termgfx_stream_pause(termgfx_stream_t *s, int on);
void   termgfx_stream_reset(termgfx_stream_t *s);
float  termgfx_stream_volume(const termgfx_stream_t *s);   // dB (0 = unity)
int    termgfx_stream_muted(const termgfx_stream_t *s);
// Step the channel level by `delta_db` and return the new dB. Capped at unity
// (0 dB -- +/- never boosts a pre-mixed stream); stepping down past a
// near-silent floor snaps to full mute, which stops the uplink altogether.
float  termgfx_stream_volume_step(termgfx_stream_t *s, float delta_db);

// Feed interleaved STEREO PCM; `frames` counts per-channel frames. Always
// returns `frames` -- the source must believe we consumed everything, whatever
// we do with it.
size_t termgfx_stream_feed(termgfx_stream_t *s, const int16_t *pcm, size_t frames);

// Stop the channel dead and drop everything queued behind it.
void   termgfx_stream_stop(termgfx_stream_t *s);

// Counters for a door's own trace/overlay. Any pointer may be NULL.
void   termgfx_stream_stats(const termgfx_stream_t *s,
                            unsigned *chunks, unsigned *underruns, unsigned *dropped);

#endif // TERMGFX_AUDIO_STREAM_H_
