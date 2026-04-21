#ifndef _XPBEEP_H_
#define _XPBEEP_H_

#include "gen_defs.h"  /* int16_t etc. via the synchronet-wide shim — never
                        * <stdint.h> directly: Borland's stdint.h clashes
                        * with gen_defs.h's own typedefs. */
#include "wrapdll.h"

/* Native playback format: signed 16-bit stereo, 44100 Hz.
 * One frame = S_CHANNELS * S_BYTEDEPTH bytes (two int16_t values: L, R). */
#define XPBEEP_SAMPLE_RATE  44100
#define XPBEEP_CHANNELS     2
#define XPBEEP_BYTEDEPTH    2
#define XPBEEP_FRAMESIZE    (XPBEEP_CHANNELS * XPBEEP_BYTEDEPTH)

#if defined(_WIN32)
	#if 0
	#define BEEP(freq,dur)	xpbeep((double)(freq),(DWORD)(dur))
	#else
	#define BEEP(freq,dur)	Beep((DWORD)(freq),(DWORD)(dur))
	#endif
#elif defined(__OS2__)
	#define BEEP(freq,dur)	DosBeep(freq,dur)
#elif defined(__unix__) || defined(__APPLE__)
	#define BEEP(freq,dur)	xpbeep((double)(freq),(DWORD)(dur))
#else
	#error "Unsupported Target: Need some macros and/or function prototypes here."
#endif

enum WAVE_SHAPE {
	 WAVE_SHAPE_SINE
	,WAVE_SHAPE_SAWTOOTH
	,WAVE_SHAPE_SQUARE
	,WAVE_SHAPE_SINE_SAW
	,WAVE_SHAPE_SINE_HARM
	,WAVE_SHAPE_SINE_SAW_CHORD
	,WAVE_SHAPE_SINE_SAW_HARM
};

#define WAVE_SHAPE_MASK     0x7FFFFFFFU
#define WAVE_SHAPE_NO_CLEAN 0x80000000U

#define XPBEEP_DEVICE_WIN32      (1U<<0)
#define XPBEEP_DEVICE_ALSA       (1U<<1)
#define XPBEEP_DEVICE_OSS        (1U<<2)
#define XPBEEP_DEVICE_SDL        (1U<<3)
#define XPBEEP_DEVICE_PORTAUDIO  (1U<<4)
#define XPBEEP_DEVICE_PULSEAUDIO (1U<<5)
#define XPBEEP_DEVICE_COREAUDIO  (1U<<6)
#define XPBEEP_DEVICE_DEFAULT (XPBEEP_DEVICE_WIN32 | XPBEEP_DEVICE_ALSA | XPBEEP_DEVICE_OSS | XPBEEP_DEVICE_PORTAUDIO | XPBEEP_DEVICE_PULSEAUDIO | XPBEEP_DEVICE_COREAUDIO)
extern uint32_t xpbeep_sound_devices_enabled;

#ifdef __cplusplus
extern "C" {
#endif
/* Generate a waveform into `wave` (stereo-interleaved int16_t, `samples` frames,
 * total buffer size samples * XPBEEP_FRAMESIZE bytes). L and R carry the same
 * mono signal. */
DLLEXPORT void xptone_makewave(double freq, int16_t *wave, int samples, DWORD shape);
DLLEXPORT bool xptone_open(void);
DLLEXPORT bool xptone_close(void);
DLLEXPORT void xpbeep(double freq, DWORD duration);
/* Legacy U8 mono 22050 Hz entrypoint. Converts internally to S16 stereo 44100.
 * `sample_size` is bytes of U8 mono data. */
DLLEXPORT bool xp_play_sample(unsigned char *sample, size_t sample_size, bool background);
/* Convert U8 mono 22050 Hz data into a newly-allocated S16 stereo 44100 Hz
 * frame buffer. 2:1 linear-interpolation upsample, L==R. Caller owns the
 * returned buffer and must free() it. Returns NULL on allocation failure. */
DLLEXPORT int16_t *xp_u8mono22k_to_s16stereo44k(const unsigned char *in, size_t in_bytes,
                                                size_t *nframes_out);
/* Native S16 stereo 44100 Hz entrypoint. `nframes` is frame count; the buffer
 * is nframes * XPBEEP_FRAMESIZE bytes. */
DLLEXPORT bool xp_play_sample16s(const int16_t *frames, size_t nframes, bool background);
DLLEXPORT void xptone_complete(void);
DLLEXPORT bool xptone(double freq, DWORD duration, DWORD shape);

/* ================================================================
 * Streaming mixer API (S16 stereo 44100 Hz).
 *
 * Multiple streams play concurrently; the mixer sums them into the device
 * output. A stream is a FIFO of producer-supplied PCM buffers — append
 * hands ownership of a malloc()d frame buffer to the mixer, which frees
 * the buffer once its frames have been fully consumed. Append is
 * non-blocking in the steady state; it only waits when the per-node
 * metadata allocation itself cannot be satisfied.
 *
 * Handles are small non-negative integers; -1 indicates failure.
 * ================================================================ */

typedef int xp_audio_handle_t;

/* Per-append options.  Pass NULL to xp_audio_append / xp_audio_play for
 * "plain playback, no envelope, no loop, no per-entry gain change".
 *
 * All volume fields (both per-entry here and the stream base on
 * xp_audio_open / xp_audio_set_volume) are expressed in decibels:
 * 0 dB is unity, positive boosts, negative attenuates.  Per-entry
 * volume SUMS with the stream base for this buf's playback only —
 * it does NOT modify the stream base.  So `volume_l = 0.0f` means
 * "play this buf at exactly the stream level"; `volume_l = -6.0f`
 * means "6 dB below the stream level for this buf only".
 *
 * Because 0 dB is the no-op default, a zero-initialized struct
 * is safe — no XP_AUDIO_OPTS_INIT needed unless you prefer the
 * self-documenting form.
 *
 * Envelopes are evaluated by the mixer at pull time (dB-linear,
 * -60 dB → 0 dB across the ramp).
 *
 *   fade_in_frames   — rise from silence over the first N frames.  On a
 *                      looping buffer, applies only to iteration 0.
 *   fade_out_frames  — fall to silence over the last N frames.  On a
 *                      looping buffer, applies only when the loop ends
 *                      (via crossfade, xp_audio_stop, or xp_audio_finish
 *                      with nothing queued behind it).
 *   crossfade        — when true, the previous tail's playback is
 *                      overlaid with a fade-out of length fade_in_frames
 *                      starting immediately at append time; both
 *                      buffers mix concurrently during the overlap.
 *                      No-op when the stream has no prior content.
 *   loop             — mixer cursor wraps at nframes.  Subsequent append
 *                      ends the loop (plain or crossfade).
 */
typedef struct {
    float  volume_l;
    float  volume_r;
    size_t fade_in_frames;
    size_t fade_out_frames;
    bool   crossfade;
    bool   loop;
} xp_audio_opts_t;

/* Self-documenting preset: 0 dB (unity) per-entry volume, no envelope,
 * no loop, no crossfade.  Equivalent to `= {0}` since 0 dB is the
 * no-op default. */
#define XP_AUDIO_OPTS_INIT { 0.0f, 0.0f, 0, 0, false, false }

/* Open a new stream with the given base dB levels (0 dB = unity,
 * negative values attenuate).  Returns -1 if the stream table is full. */
DLLEXPORT xp_audio_handle_t xp_audio_open(float volume_l, float volume_r);
DLLEXPORT void              xp_audio_close(xp_audio_handle_t h);

/* Append `frames` (nframes S16 stereo frames) to the stream's FIFO.
 * Ownership of `frames` transfers to the channel: the caller must have
 * obtained it via malloc(), and the channel free()s it once the buffer
 * has been fully consumed.  Returns false if the stream is finished/
 * closed or the handle is invalid; `frames` is freed in that case so
 * the caller never owns it after a failed call.
 *
 * `opts` may be NULL for plain playback.  See xp_audio_opts_t for the
 * envelope / loop / crossfade semantics. */
DLLEXPORT bool xp_audio_append(xp_audio_handle_t h, int16_t *frames, size_t nframes,
                               const xp_audio_opts_t *opts);

/* One-shot convenience: opens a stream, copies `frames` into a fresh
 * buffer, enqueues it with `opts`, signals finished.  Caller is
 * responsible for xp_audio_close (or xp_audio_drain + close).  Because
 * the input is duplicated, `frames` may be stack-allocated, read-only
 * data, or freed immediately after the call. */
DLLEXPORT xp_audio_handle_t xp_audio_play(const int16_t *frames, size_t nframes,
                                          const xp_audio_opts_t *opts);

/* Update per-channel base dB on a live stream (0 dB = unity). */
DLLEXPORT void xp_audio_set_volume(xp_audio_handle_t h, float volume_l, float volume_r);

/* Signal that no more data will be appended. The stream will drain naturally. */
DLLEXPORT void xp_audio_finish(xp_audio_handle_t h);

/* Stop playback immediately: discard buffered frames, mark done. */
DLLEXPORT void xp_audio_stop(xp_audio_handle_t h);

/* End-of-phrase barrier: block until the mixer has consumed every frame
 * appended up to this call AND the device's kernel buffer has had time to
 * play them (one device-latency sleep). Use when subsequent output needs
 * to stay time-ordered after the audio — e.g. at the end of an MML block
 * so following terminal text doesn't race past the last note. */
DLLEXPORT void xp_audio_drain(xp_audio_handle_t h);
#ifdef __unix__
DLLEXPORT void unix_beep(int freq, int dur);
#endif
#ifdef __cplusplus
}
#endif

#endif
