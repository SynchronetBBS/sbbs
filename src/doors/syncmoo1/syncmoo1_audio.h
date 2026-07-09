/* syncmoo1_audio.h -- 1oom's hw_audio_sfx_* contract over termgfx's audio
 * manager. hw_sbbs.c forwards the engine's hooks here, the way hw_video_*
 * forwards to syncmoo1_io.c; nothing else in the door knows about audio.
 *
 * Design: docs/superpowers/specs/2026-07-08-syncmoo1-sfx-audio-design.md
 */
#ifndef SYNCMOO1_AUDIO_H
#define SYNCMOO1_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#include "audio_mgr.h"   /* termgfx: termgfx_audio_t */

/* Cache leaf for `len` bytes: "s_" + 8 lower-case hex of FNV-1a. `out` must be
 * >= 16 bytes. Content-addressed, so identical samples share one cache entry
 * and the name never depends on 1oom's index numbering. */
void sm_audio_leaf(const void *data, size_t len, char out[16]);

/* 1oom hands hw_audio_sfx_init() a raw LBX item, not a bare Creative VOC: an
 * LBXVOC item carries a 16-byte LBX sub-header before the VOC bytes start
 * (1oom/src/fmt_id.h HDRID_LBXVOC/HDR_LBXVOC_LEN, 1oom/src/fmt_sfx.c:406-428
 * fmt_sfx_detect()/fmt_sfx_convert()). Returns the byte offset of the actual
 * VOC payload within `data`: 16 if the wrapper is present, else 0. Never
 * underflows `len`. */
size_t sm_audio_payload_offset(const void *data, size_t len);

/* 1oom reports SFX volume as 0..128; termgfx wants 0..100. Clamped. */
int sm_audio_vol(int moo_vol);

/* Adopt the session's audio manager (may be NULL -- then everything below is a
 * silent no-op, which is exactly what a terminal with no audio APC gets). */
void sm_audio_attach(termgfx_audio_t *m);

/* The 1oom hw_audio_sfx_* contract. Indices are 1oom's global SFX ids. */
int  sm_audio_batch_start(int max);   /* capacity hint. NEVER clears slots. */
int  sm_audio_init(int index, const uint8_t *data, uint32_t len);
void sm_audio_play(int index);
void sm_audio_stop(void);
void sm_audio_release(int index);
void sm_audio_set_volume(int moo_vol);

/* Drain the pending-store queue once the terminal's audio tier is known.
 * Called from hw_event_handle(); cheap no-op once drained or below tier 1. */
void sm_audio_pump(void);

/* Test accessors: the leaf registered at `index` (NULL if none/out of range),
 * and how many distinct samples are still queued for Store. */
const char *sm_audio_slot(int index);
int         sm_audio_pending(void);

#endif /* SYNCMOO1_AUDIO_H */
