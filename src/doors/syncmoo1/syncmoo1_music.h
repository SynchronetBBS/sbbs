/* syncmoo1_music.h -- 1oom's hw_audio_music_* contract over termgfx's music
 * channel. hw_sbbs.c forwards the engine's hooks here, exactly as it forwards
 * hw_audio_sfx_* to syncmoo1_audio.c.
 *
 * Design: docs/superpowers/specs/2026-07-09-syncmoo1-music-design.md
 */
#ifndef SYNCMOO1_MUSIC_H_
#define SYNCMOO1_MUSIC_H_

#include <stddef.h>
#include <stdint.h>

#include "audio_mgr.h"   /* termgfx: termgfx_audio_t */

/* Adopt the session's audio manager (may be NULL -- then everything below is a
 * silent no-op, which is what a terminal with no audio APC gets). */
void sm_music_attach(termgfx_audio_t *m);

/* The 1oom hw_audio_music_* contract. `index` is 1oom's music slot; uisound.c
 * only ever uses 0, but the contract is indexed, so we honor it. */
int  sm_music_init(int index, const uint8_t *data, uint32_t len);
void sm_music_release(int index);
void sm_music_play(int index);
void sm_music_stop(void);      /* also serves hw_audio_music_fadeout() */
void sm_music_set_volume(int moo_vol);   /* 0..128 */

/* Ship a finished async render, and replay a track that was requested before
 * the terminal's audio tier was known. Called from sm_audio_pump(). */
void sm_music_pump(void);

/* Test accessor: the cache leaf registered at `index`, or NULL. */
const char *sm_music_slot(int index);

#endif /* SYNCMOO1_MUSIC_H_ */
