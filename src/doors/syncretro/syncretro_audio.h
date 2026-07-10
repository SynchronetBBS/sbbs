/* syncretro_audio.h -- the core's PCM stream -> SyncTERM's audio APC.
 *
 * A libretro core hands the door a continuous 44.1 kHz stream, not the discrete
 * SFX and music tracks the engine doors emit. SyncTERM's mixer channels have a
 * FIFO (A;Queue moves a slot's buffer onto it), so consecutive queues play back
 * to back: that FIFO is the live audio ring, and this module feeds it 100 ms
 * Opus chunks. See M4_AUDIO.md.
 */
#ifndef SYNCRETRO_AUDIO_H_
#define SYNCRETRO_AUDIO_H_

#include <stddef.h>
#include <stdint.h>

/* --- the streaming module ---------------------------------------------------
 * Fixed resources: mixer channel 2 (first APC-usable), patch slot 0 (A;Queue
 * empties the slot, and APCs are processed in stream order, so one slot serves
 * every chunk), cache names s/0..s/3 plus s/z for silence. */
#define SR_AUDIO_CH        2
#define SR_AUDIO_SLOT      0
#define SR_AUDIO_RING      4
#define SR_AUDIO_RATE      44100

/* Read config, size the accumulator. No I/O. Call after sr_config_apply(). */
void   sr_audio_init(void);
/* Emit the libsndfile capability query. Call from the io probe. */
void   sr_audio_probe(void);
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
