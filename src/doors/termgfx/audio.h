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

// C;S Store: encode `pcm` (interleaved S16, `frames` per-channel frames,
// `channels`, `rate` Hz) to Ogg/Opus and cache it under `file`. Much smaller than
// cache_pcm's raw WAV, shrinking both the upload and the player's on-disk SyncTERM
// cache; SyncTERM decodes it with the same libsndfile. `rate` must be an Opus rate
// (8k/12k/16k/24k/48k -- use 48000). `quality` is the VBR quality (0.0..1.0); note
// libsndfile maps Opus quality ~linearly to bitrate (q0.06~=40 kbps, q0.3~=165
// kbps), unlike Vorbis's content-adaptive VBR. Only does real work when termgfx
// was built WITH libsndfile -- check termgfx_audio_have_ogg() and fall back to
// cache_pcm when it returns 0. Returns bytes written, or 0 on failure/unavailable.
size_t termgfx_audio_cache_ogg(uint8_t **buf, size_t *cap, const char *file,
                               const int16_t *pcm, size_t frames,
                               int channels, int rate, double quality);

// Encode 16-bit PCM to a raw Ogg/Opus stream in memory: malloc'd buffer in *out
// (caller frees), returns its length or 0. Lets the manager write the Ogg to its
// door-side disk cache AND base64 it to the client from a single encode.
size_t termgfx_audio_encode_ogg(const int16_t *pcm, size_t frames, int channels,
                                int rate, double quality, uint8_t **out);

// 1 if termgfx was built with libsndfile (termgfx_audio_cache_ogg works), else 0.
int termgfx_audio_have_ogg(void);

// Decode a Creative VOC (the Sound Blaster format Duke ships its SFX in) to
// interleaved 8-bit *unsigned* PCM. libsndfile rejects some of Duke's multi-block
// VOCs ("incompatible VOC sections" -- e.g. a second type-1 block instead of a
// type-2 continuation), so the door transcodes those to a clean WAV rather than
// shipping the VOC verbatim. Returns the PCM byte count and sets *pcm (malloc'd,
// caller frees) + *rate; returns 0 if the data isn't a VOC or can't be parsed
// (caller then ships it verbatim -- it's already WAV/OGG/etc).
size_t termgfx_audio_voc_to_pcm(const void *voc, size_t len, uint8_t **pcm, int *rate);

// A;Load -- decode the cached `file` into patch slot `slot` (0..255).
size_t termgfx_audio_load(uint8_t **buf, size_t *cap, int slot, const char *file);

// A;Queue -- play slot `slot` on channel `ch` (2..15). `vol` is 0..100; `pan`
// is -100 (full left) .. 0 (center) .. +100 (full right), mapped linearly to
// the VL/VR keys; `loop` != 0 sets the L flag.
size_t termgfx_audio_queue(uint8_t **buf, size_t *cap, int ch, int slot,
                           int vol, int pan, int loop);

// A;Volume -- set channel `ch`'s live mix volume (0..100, 100 = unity). Unlike the
// per-Queue VL/VR (which scale one buffer as it's queued), this sets the channel's
// base level, so it adjusts a sound that's already playing/looping -- e.g. a live
// music-volume slider on the reserved music channel.
size_t termgfx_audio_volume(uint8_t **buf, size_t *cap, int ch, int vol);

// A;Volume with separate left/right -- set channel `ch`'s live per-side mix
// volume (each 0..100). Like termgfx_audio_volume but carries stereo balance, so
// a looping voice can be panned in real time (3D distance + direction tracking).
size_t termgfx_audio_volume_lr(uint8_t **buf, size_t *cap, int ch, int vl, int vr);

// A;Synth -- generate a `ms`-millisecond `freq`-Hz tone of waveform `shape`
// ("SINE","SQUARE","SAWTOOTH",...) into slot `slot`. The fallback cue source
// when libsndfile is absent.
size_t termgfx_audio_synth(uint8_t **buf, size_t *cap, int slot,
                           const char *shape, int freq, int ms);

// A;Flush -- stop channel `ch`, with an optional `fade_ms` fade-out
// (0 = abrupt). Used to free a channel for voice-stealing.
size_t termgfx_audio_flush(uint8_t **buf, size_t *cap, int ch, int fade_ms);

#endif // TERMGFX_AUDIO_H_
