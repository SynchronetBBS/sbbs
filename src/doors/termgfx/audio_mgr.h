#ifndef TERMGFX_AUDIO_MGR_H_
#define TERMGFX_AUDIO_MGR_H_

#include <stddef.h>
#include <stdint.h>

// audio_mgr.h -- stateful policy layer over audio.h's SyncTERM audio-APC
// builders. One instance per door session. It owns the parts every framebuffer
// door would otherwise re-implement:
//   * the capability tier (probe SyncTERM, decide digital / tone / none),
//   * an upload-once sample cache (each unique sound is C;S-Stored just once),
//   * a 256-entry patch-slot LRU (Load a sample into a slot, evict oldest),
//   * a round-robin SFX channel pool (music gets a reserved channel).
// The door supplies an emit callback (its socket writer) and feeds inbound
// bytes so the capability reply can be parsed -- this layer does no I/O itself
// (same split as caps.h). All entry points are NULL-safe: a NULL manager (or
// tier < 1) makes sfx()/music() silent no-ops.
//
//   m = termgfx_audio_create(out_emit, ctx);
//   termgfx_audio_probe(m);                          // emit libsndfile query
//   ...read socket, termgfx_audio_feed(m, buf, n)... // until tier known / deadline
//   termgfx_audio_sfx_file(m, id, voc, voclen, vol, pan);       // Duke VOC/WAV
//   termgfx_audio_sfx(m, id, pcm, n, 8,1,11025, vol, pan);      // raw PCM (Doom)

// The door's socket writer. `stream` lets it prioritize: TERMGFX_AUDIO_PRIO (0) =
// small, latency-critical APCs (SFX Load/Queue/Flush + the small SFX upload) that
// should reach the terminal AHEAD of video frames so a gunshot doesn't trail what's
// on screen; TERMGFX_AUDIO_BULK (1) = a music track's bulk PCM/OGG upload, which the
// door can drain at lower priority so it never delays SFX or stalls video. A door
// that doesn't separate streams can simply ignore `stream`.
#define TERMGFX_AUDIO_PRIO 0
#define TERMGFX_AUDIO_BULK 1
typedef void (*termgfx_audio_emit_fn)(void *ctx, const void *buf, size_t len, int stream);

typedef struct termgfx_audio termgfx_audio_t;

termgfx_audio_t *termgfx_audio_create(termgfx_audio_emit_fn emit, void *ctx);
void             termgfx_audio_destroy(termgfx_audio_t *m);

// Emit the capability query (SyncTERM:Q;libsndfile). Call once at startup.
void termgfx_audio_probe(termgfx_audio_t *m);

// Feed inbound terminal bytes so the probe reply can be parsed. Harmless to
// pass all session input through it -- it only scans for the audio DSR and
// stops looking once the tier is known.
void termgfx_audio_feed(termgfx_audio_t *m, const uint8_t *buf, int len);

// Capability tier: -1 none/unknown (no audio APC reply yet), 0 tone-only
// (audio APC but no libsndfile), 1 digital (full sample path).
int  termgfx_audio_tier(const termgfx_audio_t *m);
void termgfx_audio_set_tier(termgfx_audio_t *m, int tier);

// Play sound `id` (0..1023) as a one-shot SFX on the next pooled channel from
// raw PCM (`bits` 8|16, `channels`, `rate`); the manager wraps it in a WAV and
// Stores it once. `vol` 0..100, `pan` -100..+100.
void termgfx_audio_sfx(termgfx_audio_t *m, int id,
                       const void *pcm, size_t bytes, int bits, int channels,
                       int rate, int vol, int pan);

// Like termgfx_audio_sfx, but `filedata`/`filelen` is a complete sound FILE
// (WAV/VOC/OGG/FLAC) cached verbatim -- e.g. Duke's in-memory VOC samples.
// SyncTERM's libsndfile sniffs the format. `vol` 0..100, `pan` -100..+100.
void termgfx_audio_sfx_file(termgfx_audio_t *m, int id,
                            const void *filedata, size_t filelen, int vol, int pan);

// Name-addressed SFX. `leaf` is a caller-supplied cache-name component -- use a
// content hash (see hash.h) so identical bytes reuse one cache entry. Unlike the
// id-addressed pair above, Store and dispatch are separate: _store uploads once
// (no sound), _play_named dispatches an already-Stored leaf. Both no-op below
// tier 1, so a terminal without the audio APC costs nothing and stays silent.
void termgfx_audio_sfx_store(termgfx_audio_t *m, const char *leaf,
                             const void *filedata, size_t filelen);
void termgfx_audio_sfx_play_named(termgfx_audio_t *m, const char *leaf,
                                  int vol, int pan);

// Start a LOOPING SFX (Duke ambience: wind, machinery, rushing water, ...).
// Uploads the file once (VOC transcoded to WAV) like termgfx_audio_sfx_file, then
// loops it on a dedicated channel. Returns a positive voice handle to pass to
// termgfx_audio_loop_stop, or 0 if tier < 1 / no looping channel (then nothing
// plays). `vol` 0..100.
int termgfx_audio_loop_start(termgfx_audio_t *m, int id,
                             const void *filedata, size_t filelen, int vol);

// Update the live volume (0..100) and stereo pan (-100 left .. +100 right) of a
// running looping voice -- for 3D distance + direction tracking as the player
// moves relative to an ambient source. No-op (no APC sent) if the resulting L/R
// is unchanged from the last update or the handle is unknown, so it's safe to
// call every frame.
int  termgfx_audio_loop_volume(termgfx_audio_t *m, int handle, int vol, int pan);   /* 1 = handle matched a live channel, 0 = no such loop */

// Stop the looping voice `handle` from termgfx_audio_loop_start (no-op if it's
// already stopped/unknown -- safe against a stale handle).
void termgfx_audio_loop_stop(termgfx_audio_t *m, int handle);

// Set the consumer's name (e.g. "syncdoom" / "syncduke"). SyncTERM client-cache files
// are then named "<name>/music/.." and "<name>/sfx/..", so two doors sharing one BBS's
// cache dir stay distinct + attributable (and a future SFX cache-skip is collision-safe).
// Unset -> bare "music/.." / "sfx/.." (fine for a single consumer). Call once at init.
void termgfx_audio_set_cache_prefix(termgfx_audio_t *m, const char *name);

// Set the door-side OGG disk-cache directory (e.g. data/<door>/audio). When set,
// termgfx_audio_music() writes each encoded track there, and termgfx_audio_music_play()
// can ship it on a later session WITHOUT re-rendering. "" / NULL disables (render-only).
void termgfx_audio_set_music_cache_dir(termgfx_audio_t *m, const char *dir);

// Ogg/Opus VBR quality (0.0..1.0) for encoded music. libsndfile maps this ~linearly
// to bitrate (q0.06~=40, q0.15~=88, q0.3~=165 kbps) -- NOT content-adaptively like
// Vorbis. Higher = fuller/cleaner + bigger. Affects only the bytes of a FRESH encode;
// it is deliberately not part of the cache key, so a client's existing file is never
// re-fetched to change quality. Doors expose this as syncXXX.ini [audio] music_quality.
#define TERMGFX_MUSIC_QUALITY_DEFAULT 0.15
void termgfx_audio_set_music_quality(termgfx_audio_t *m, double quality);

// termgfx_audio_music_play() return values (0 = the door must render; nonzero = played):
#define TERMGFX_MUSIC_RENDER 0   // not cached anywhere -> door renders + supplies
#define TERMGFX_MUSIC_CACHED 1   // shipped from the door-side disk cache (no render; uploaded)
#define TERMGFX_MUSIC_CLIENT 2   // client already had it (C;L hit): no render AND no upload

// Play looping music track `name` from a CACHE without rendering -- the client's own
// persistent cache (C;L, queried at tier-ready) or the door-side OGG disk cache. `name`
// is the track's content-addressed identity (e.g. a source-hash like d_<hash>); termgfx
// adds a render-version tag to form the cache key. Returns one of the TERMGFX_MUSIC_*
// codes above: a nonzero value means it shipped the track (door skips rendering);
// TERMGFX_MUSIC_RENDER (0) means the door must render the MIDI to PCM and hand it to
// termgfx_audio_music() below. (The distinct codes let the door log which path it took.)
int termgfx_audio_music_play(termgfx_audio_t *m, const char *name, int vol);

// Supply a freshly-rendered track: loop `pcm` on the music channel, replacing any
// playing track, and (when libsndfile is present) encode it to OGG/Vorbis -- uploaded
// to the client AND written to the door-side disk cache so the next play skips the
// render. `name` is the same content-addressed identity passed to _music_play. Call
// this only after _music_play() returned 0. No-op if `name` is already playing.
void termgfx_audio_music(termgfx_audio_t *m, const char *name,
                         const void *pcm, size_t bytes, int bits, int channels,
                         int rate, int vol);

// Async alternative to (render + termgfx_audio_music) for a cold cache miss: hand the raw MIDI/MUS
// bytes to _submit() and call _poll() once per frame.  A worker thread renders + encodes + caches +
// pre-builds the upload off the game thread; the game keeps running and the track fades in when
// _poll() ships it.  On a build without threads (e.g. Windows) _submit() renders synchronously (the
// old blocking path) and _poll() is a no-op, so callers need no #ifdef.  Call after _music_play()
// returned TERMGFX_MUSIC_RENDER; no-op if `name` is already playing.  _poll() returns:
#define TERMGFX_MUSIC_ASYNC_IDLE    0   // nothing ready this call
#define TERMGFX_MUSIC_ASYNC_SHIPPED 1   // a finished track was uploaded + started this call
void termgfx_audio_music_async_submit(termgfx_audio_t *m, const char *name,
                                      const void *music, size_t len, int rate, int vol);
int  termgfx_audio_music_async_poll(termgfx_audio_t *m);

// Stop the music channel (fade out).
void termgfx_audio_music_stop(termgfx_audio_t *m);

// Set the music channel's live volume (0..100), adjusting the currently-looping
// track without restarting it -- for a music-volume control.
void termgfx_audio_music_volume(termgfx_audio_t *m, int vol);

// Stop every SFX channel at once (e.g. a "sound off" toggle, or a level change):
// flushes the pooled SFX channels. Music is on its own channel and is unaffected.
void termgfx_audio_sfx_stop_all(termgfx_audio_t *m);

#endif // TERMGFX_AUDIO_MGR_H_
