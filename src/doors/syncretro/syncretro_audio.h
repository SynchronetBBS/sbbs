/* syncretro_audio.h -- the core's PCM stream -> SyncTERM's audio APC.
 *
 * A libretro core hands the door a continuous PCM stream, not the discrete SFX
 * and music tracks the engine doors emit. SyncTERM's mixer channels have a
 * FIFO (A;Queue moves a slot's buffer onto it), so consecutive queues play back
 * to back: that FIFO is the live audio ring, and this module feeds it 100 ms
 * Opus chunks. See M4_AUDIO.md.
 */
#ifndef SYNCRETRO_AUDIO_H_
#define SYNCRETRO_AUDIO_H_

#include <stddef.h>
#include <stdint.h>

/* --- the streaming module ---------------------------------------------------
 * The machinery is termgfx's (audio_stream.h) and shared with the other doors;
 * these are the resources THIS door hands it: mixer channel 2 (first
 * APC-usable), patch slot 0 (A;Queue empties the slot, and APCs are processed
 * in stream order, so one slot serves every chunk), and the "s" cache prefix
 * (names s/0..s/3 plus s/z for silence). */
#define SR_AUDIO_CH        2
#define SR_AUDIO_SLOT      0

/* Read config. No allocation, no I/O, and no rate (the core is not loaded yet).
 * Call after sr_config_apply(). */
void   sr_audio_init(void);
/* Size the chunk to the core's sample rate. Call ONCE, from main(), after
 * rc_core_load_game() -- before this, the module emits nothing. */
void   sr_audio_start(int rate);
/* Emit the libsndfile capability query. Call from the io probe. */
void   sr_audio_probe(void);
/* Enable inline A;LoadBlob streaming (no cache file). Call from the DA1 probe
 * reply when the client's CTerm version is >= 1329. Off by default. */
void   sr_audio_set_blob_ok(int ok);
/* 1 when the audio stream is shipping inline via A;LoadBlob (for the stats). */
int    sr_audio_blob_active(void);
/* The probe reply: tier 1 = libsndfile (stream), 0 = tone-only, -1 = none.
 * Anything but 1 disables the module permanently -- a Load the terminal ignores
 * would leave a Queue playing an empty slot. */
void   sr_audio_caps(int tier);
/* `CSI = 7 ; ch ; 0 n`: that channel's FIFO drained. */
void   sr_audio_underrun(int ch);
/* A door screen is up (or came down): the core is not running, so no PCM is
 * produced and the FIFO drains on purpose. Suppresses the spurious underrun and
 * re-primes on resume. */
void   sr_audio_pause(int on);
/* '+' / '-': step the terminal's mixer-channel volume, 0..100, and return the
 * new value. At ZERO the door stops sending audio altogether -- no encode, no
 * upload, no queue -- and flushes what is already queued, so a muted player
 * costs zero uplink rather than streaming Opus a terminal would discard. */
int    sr_audio_volume_step(int delta);
int    sr_audio_volume(void);
int    sr_audio_muted(void);
/* Ctrl-R restarted the console: stop the channel, drop what is queued, re-prime.
 * The audio still in the terminal's FIFO belongs to a game that no longer runs. */
void   sr_audio_reset(void);
/* A;Flush the channel and report telemetry. Idempotent; safe after carrier loss. */
void   sr_audio_shutdown(void);

/* The retro_audio_sample_batch body. `pcm` is INTERLEAVED STEREO, `frames`
 * counts per-channel frames. Always returns `frames` -- the core must believe
 * we consumed everything, whatever we do with it. */
size_t sr_audio_feed(const int16_t *pcm, size_t frames);

#endif /* SYNCRETRO_AUDIO_H_ */
