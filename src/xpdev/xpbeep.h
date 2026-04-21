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
/* Native S16 stereo 44100 Hz entrypoint. `nframes` is frame count; the buffer
 * is nframes * XPBEEP_FRAMESIZE bytes. */
DLLEXPORT bool xp_play_sample16s(const int16_t *frames, size_t nframes, bool background);
DLLEXPORT void xptone_complete(void);
DLLEXPORT bool xptone(double freq, DWORD duration, DWORD shape);

/* ================================================================
 * Streaming mixer API (S16 stereo 44100 Hz).
 *
 * Multiple streams play concurrently; the mixer sums them into the device
 * output. A stream is an appendable ring buffer — the producer appends PCM
 * frames over time, the mixer drains them at the device rate. Finished
 * streams drain out and signal completion. Volume is per-channel so hard
 * L/R panning is free.
 *
 * Handles are small non-negative integers; -1 indicates failure.
 * ================================================================ */

typedef int xp_audio_handle_t;

/* Optional fade specification for xp_audio_append_faded.
 * fade_in_frames / fade_out_frames == 0 → no fade on that edge.
 * Envelope is dB-linear (perceptually linear): amplitude = 10^(dB/20)
 * with dB ramping between -60 dB (silent) and 0 dB (unity).
 *
 * crossfade: when true AND fade_in_frames > 0, the fade-in overlaps the
 * tail of the previously-appended data. The mixer's read_pos is not
 * allowed to have passed the overlap region — if it has, the crossfade
 * degrades to a plain fade-in from silence.
 */
typedef struct {
    size_t fade_in_frames;
    size_t fade_out_frames;
    bool   crossfade;
} xp_audio_fade_t;

DLLEXPORT xp_audio_handle_t xp_audio_open(float volume_l, float volume_r);
DLLEXPORT void              xp_audio_close(xp_audio_handle_t h);

/* Append raw frames with no fade shaping. May block if the ring buffer is
 * full (waits until the mixer has consumed enough space). Returns false
 * on error (bad handle, stream already finished). */
DLLEXPORT bool xp_audio_append(xp_audio_handle_t h, const int16_t *frames, size_t nframes);

/* Append with optional fade-in, fade-out, or crossfade envelope applied
 * in-place before writing to the ring. Pass NULL for no shaping. */
DLLEXPORT bool xp_audio_append_faded(xp_audio_handle_t h, const int16_t *frames,
                                     size_t nframes, const xp_audio_fade_t *fade);

/* One-shot convenience: opens a stream, appends the full buffer, marks it
 * finished. Returns the handle so the caller can xp_audio_wait / close,
 * or set volume mid-playback. Caller is responsible for xp_audio_close. */
DLLEXPORT xp_audio_handle_t xp_audio_play(const int16_t *frames, size_t nframes,
                                          float volume_l, float volume_r, bool loop);

/* Update per-channel volume on a live stream. */
DLLEXPORT void xp_audio_set_volume(xp_audio_handle_t h, float volume_l, float volume_r);

/* Signal that no more data will be appended. The stream will drain naturally. */
DLLEXPORT void xp_audio_finish(xp_audio_handle_t h);

/* Stop playback immediately: discard buffered frames, mark done. */
DLLEXPORT void xp_audio_stop(xp_audio_handle_t h);

/* Mixer-level wait: block until the mixer has consumed every frame appended
 * up to this call. Returns while the device's kernel buffer may still hold
 * a tail of those frames — appropriate between successive appends in the
 * same sequence (e.g. MF notes in an MML phrase) where inserting a device
 * drain would create audible gaps. */
DLLEXPORT void xp_audio_wait(xp_audio_handle_t h);

/* Full drain: wait as xp_audio_wait, then additionally sleep for the current
 * device-buffer latency so the data reaches the speaker before returning.
 * Use at the end of a logical audio phrase (e.g. end of an MML block) to
 * keep downstream output (text, prompts) from racing ahead of the audio. */
DLLEXPORT void xp_audio_drain(xp_audio_handle_t h);
#ifdef __unix__
DLLEXPORT void unix_beep(int freq, int dur);
#endif
#ifdef __cplusplus
}
#endif

#endif
