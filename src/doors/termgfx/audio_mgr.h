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

typedef void (*termgfx_audio_emit_fn)(void *ctx, const void *buf, size_t len);

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

// Start looping music `id` on the reserved music channel, replacing any track
// already playing (typically a single rendered loop iteration as raw PCM).
// No-op if `id` is already the current track.
void termgfx_audio_music(termgfx_audio_t *m, int id,
                         const void *pcm, size_t bytes, int bits, int channels,
                         int rate, int vol);

// Stop the music channel (fade out).
void termgfx_audio_music_stop(termgfx_audio_t *m);

#endif // TERMGFX_AUDIO_MGR_H_
