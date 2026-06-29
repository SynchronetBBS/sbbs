#ifndef TERMGFX_AUDIO_H_
#define TERMGFX_AUDIO_H_

#include <stddef.h>
#include <stdint.h>

// audio.h -- SyncTERM "SyncTERM:A" audio-APC transport for terminal doors
// (SyncDuke, SyncDOOM, ...). Same contract as sixel.c / apc.c / caps.c: every
// function is I/O-free. Each builder writes one escape sequence into a
// caller-owned *buf (grown via realloc, *cap updated) and returns its byte
// length; the door sends those bytes down the same socket it sends graphics
// on. The capability probe is a query string plus a parser over the bytes that
// come back, exactly like caps.h -- the door owns the emit + bounded read loop.
//
// Wire model (SyncTERM cterm.adoc "Audio APCs"): 256 patch slots hold decoded
// PCM; channels 2..15 mix them concurrently (0/1 are cterm-owned). The flow:
//   1. cache a sample once     -- termgfx_audio_cache_pcm/_file  [C;S Store]
//   2. decode it into a slot   -- termgfx_audio_load             [A;Load]
//   3. play the slot on a chan -- termgfx_audio_queue            [A;Queue]
// termgfx_audio_synth() is the no-sample tone fallback; termgfx_audio_flush()
// stops a channel (voice-stealing). Slot/channel allocation, dedup and
// voice-stealing are the door's policy (see audio_mgr.h) -- these builders only
// encode one verb each.

// ---- capability probe ----------------------------------------------------

// Emit once at startup (out_put(s, strlen(s))) and feed replies to the parser.
// Asks whether SyncTERM has libsndfile (the A;Load digital-decode path).
extern const char *const termgfx_audio_query;

// Scan `len` accumulated input bytes for the audio feature reply
// "ESC [ = 7 ; 100 ; {0,1} n".  Returns:
//   1  -- audio APC + libsndfile present: full digital-SFX path available
//   0  -- audio APC present but no libsndfile: only the Synth tone path works
//  -1  -- no complete reply yet (keep reading until the deadline; a terminal
//         that never answers has no audio-APC support at all)
// Any 0/1 answer also identifies the peer as an audio-APC-capable SyncTERM.
// Idempotent over a growing buffer -- call it after each read.
int termgfx_audio_parse_caps(const uint8_t *acc, int len);

// ---- builders (buf/cap realloc contract, return bytes written) -----------

// C;S Store: cache `data`/`bytes` verbatim under `file`. Use when the bytes are
// already a complete sound file (WAV/VOC/OGG/FLAC) -- e.g. Duke's in-memory VOC
// samples -- since SyncTERM's libsndfile sniffs the format from the content, not
// the name.
size_t termgfx_audio_cache_file(uint8_t **buf, size_t *cap, const char *file,
                                const void *data, size_t bytes);

// C;S Store: wrap `data` (raw PCM -- bits=8 unsigned or bits=16 signed,
// `channels`, `rate` Hz) in a canonical WAV and cache it under `file`. SyncTERM
// converts to the mixer's S16/44100 on Load, so native game formats ship as-is.
size_t termgfx_audio_cache_pcm(uint8_t **buf, size_t *cap, const char *file,
                               const void *data, size_t bytes,
                               int bits, int channels, int rate);

// A;Load -- decode the cached `file` into patch slot `slot` (0..255).
size_t termgfx_audio_load(uint8_t **buf, size_t *cap, int slot, const char *file);

// A;Queue -- play slot `slot` on channel `ch` (2..15). `vol` is 0..100; `pan`
// is -100 (full left) .. 0 (center) .. +100 (full right), mapped linearly to
// the VL/VR keys; `loop` != 0 sets the L flag.
size_t termgfx_audio_queue(uint8_t **buf, size_t *cap, int ch, int slot,
                           int vol, int pan, int loop);

// A;Synth -- generate a `ms`-millisecond `freq`-Hz tone of waveform `shape`
// ("SINE","SQUARE","SAWTOOTH",...) into slot `slot`. The fallback cue source
// when libsndfile is absent.
size_t termgfx_audio_synth(uint8_t **buf, size_t *cap, int slot,
                           const char *shape, int freq, int ms);

// A;Flush -- stop channel `ch`, with an optional `fade_ms` fade-out
// (0 = abrupt). Used to free a channel for voice-stealing.
size_t termgfx_audio_flush(uint8_t **buf, size_t *cap, int ch, int fade_ms);

#endif // TERMGFX_AUDIO_H_
