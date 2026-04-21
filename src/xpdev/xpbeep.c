/* TODO: USE PORTAUDIO! */

/* standard headers */
#include <math.h>
#include <stdlib.h>
#include "xp_dl.h"

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <mmsystem.h>
#elif defined(__unix__)
	#include <fcntl.h>
	#include <sys/ioctl.h>
	#ifndef __EMSCRIPTEN__
		#ifdef SOUNDCARD_H_IN
			#if SOUNDCARD_H_IN == 1
				#include <sys/soundcard.h>
			#elif SOUNDCARD_H_IN == 2
				#include <soundcard.h>
			#elif SOUNDCARD_H_IN == 3
				#include <linux/soundcard.h>
			#endif
		#endif
		#ifdef USE_ALSA_SOUND
			#include <dlfcn.h>
			#include <alsa/asoundlib.h>
		#endif
/* KIOCSOUND */
		#if defined(__FreeBSD__)
			#include <sys/kbio.h>
		#elif defined(__linux__)
			#include <sys/kd.h>
		#elif defined(__solaris__)
			#include <sys/kbio.h>
			#include <sys/kbd.h>
		#endif
		#if (defined(__OpenBSD__) || defined(__NetBSD__)) && defined(HAS_MACHINE_SPKR_H)
			#include <machine/spkr.h>
		#elif defined(__FreeBSD__)
			#if defined(HAS_DEV_SPEAKER_SPEAKER_H)
				#include <dev/speaker/speaker.h>
			#elif defined(HAS_MACHINE_SPEAKER_H)
				#include <machine/speaker.h>
			#endif
		#endif
	#endif
#endif

/* xpdev headers */
#ifdef WITH_PORTAUDIO
#include <portaudio.h>
#endif

#ifdef WITH_PULSEAUDIO
#ifdef XPDEV_THREAD_SAFE
/* Pull-native via libpulse async API + threaded mainloop. */
#include <pulse/pulseaudio.h>
#else
/* Legacy synchronous path: libpulse-simple. */
#include <pulse/simple.h>
#endif
#endif

#ifdef WITH_SDL_AUDIO
#include "sdlfuncs.h"
#endif

#ifdef WITH_COREAUDIO
#include <AudioToolbox/AudioToolbox.h>
#include "semwrap.h"
#endif

#include "genwrap.h"
#include "xpbeep.h"

#define S_RATE      XPBEEP_SAMPLE_RATE
#define S_CHANNELS  XPBEEP_CHANNELS
#define S_BYTEDEPTH XPBEEP_BYTEDEPTH
#define S_FRAMESIZE XPBEEP_FRAMESIZE

#ifdef XPDEV_THREAD_SAFE
/* MSVC defines __STDC_NO_ATOMICS__=1 even when /experimental:c11atomics is
 * enabled and <stdatomic.h> works. threadwrap.h skips the include when the
 * macro is set, so force it off here (matching the same workaround in
 * syncterm/conn.h). */
#ifdef _MSC_VER
#undef __STDC_NO_ATOMICS__
#endif
#include "threadwrap.h"
#include "eventwrap.h"

static pthread_mutex_t      handle_mutex;

/* Device thread: for push-based backends (Win32 waveOut, ALSA, OSS) a
 * dedicated thread loops xp_mixer_pull → device_write. Pull-native backends
 * (SDL, CoreAudio, PortAudio, PulseAudio async) don't use this thread —
 * their own callbacks call xp_mixer_pull directly. */
static bool                 device_thread_running = false;
/* One-shot shutdown flag: producer writes true once, device thread reads
 * each iteration. Prefer _Atomic where the compiler supports it. Borland C
 * (bcc32, used for the SBBS Win32 build) is stuck at C99 and doesn't
 * recognize the _Atomic keyword — volatile is semantically sufficient for
 * a single-writer / single-reader false→true transition. */
#ifdef __BORLANDC__
static volatile bool        device_thread_should_exit = false;
#else
static _Atomic bool         device_thread_should_exit = false;
#endif
static sem_t                device_thread_stopped;

#endif

#ifdef _MSC_VER
#pragma warning(disable : 4244 4267 4018)
#endif

static bool sound_device_open_failed = false;
#ifdef USE_ALSA_SOUND
static bool alsa_device_open_failed = false;
#endif
#ifdef WITH_SDL_AUDIO
static bool sdl_device_open_failed = false;
#endif
#ifdef WITH_PORTAUDIO
static bool portaudio_device_open_failed = false;
#endif
#ifdef WITH_PULSEAUDIO
static bool pulseaudio_device_open_failed = false;
#endif
#ifdef WITH_COREAUDIO
static bool coreaudio_device_open_failed = false;
#endif

enum {
	SOUND_DEVICE_CLOSED
	, SOUND_DEVICE_WIN32
	, SOUND_DEVICE_ALSA
	, SOUND_DEVICE_OSS
	, SOUND_DEVICE_SDL
	, SOUND_DEVICE_PORTAUDIO
	, SOUND_DEVICE_PULSEAUDIO
	, SOUND_DEVICE_COREAUDIO
};

static int handle_type = SOUND_DEVICE_CLOSED;
static int handle_rc;
uint32_t xpbeep_sound_devices_enabled =  XPBEEP_DEVICE_DEFAULT;

#ifdef WITH_PULSEAUDIO
struct pulseaudio_api_struct {
	dll_handle dl;
#ifdef XPDEV_THREAD_SAFE
	/* Threaded mainloop */
	pa_threaded_mainloop *(*threaded_mainloop_new)(void);
	void                  (*threaded_mainloop_free)(pa_threaded_mainloop *m);
	int                   (*threaded_mainloop_start)(pa_threaded_mainloop *m);
	void                  (*threaded_mainloop_stop)(pa_threaded_mainloop *m);
	void                  (*threaded_mainloop_lock)(pa_threaded_mainloop *m);
	void                  (*threaded_mainloop_unlock)(pa_threaded_mainloop *m);
	void                  (*threaded_mainloop_wait)(pa_threaded_mainloop *m);
	void                  (*threaded_mainloop_signal)(pa_threaded_mainloop *m, int wait_for_accept);
	pa_mainloop_api *     (*threaded_mainloop_get_api)(pa_threaded_mainloop *m);
	/* Context */
	pa_context *          (*context_new)(pa_mainloop_api *mainloop, const char *name);
	int                   (*context_connect)(pa_context *c, const char *server, pa_context_flags_t flags, const pa_spawn_api *api);
	void                  (*context_disconnect)(pa_context *c);
	void                  (*context_unref)(pa_context *c);
	void                  (*context_set_state_callback)(pa_context *c, pa_context_notify_cb_t cb, void *userdata);
	pa_context_state_t    (*context_get_state)(const pa_context *c);
	/* Stream */
	pa_stream *           (*stream_new)(pa_context *c, const char *name, const pa_sample_spec *ss, const pa_channel_map *map);
	int                   (*stream_connect_playback)(pa_stream *s, const char *dev, const pa_buffer_attr *attr, pa_stream_flags_t flags, const pa_cvolume *volume, pa_stream *sync_stream);
	int                   (*stream_disconnect)(pa_stream *s);
	void                  (*stream_unref)(pa_stream *s);
	void                  (*stream_set_state_callback)(pa_stream *s, pa_stream_notify_cb_t cb, void *userdata);
	void                  (*stream_set_write_callback)(pa_stream *s, pa_stream_request_cb_t cb, void *userdata);
	pa_stream_state_t     (*stream_get_state)(const pa_stream *s);
	int                   (*stream_write)(pa_stream *s, const void *data, size_t bytes, pa_free_cb_t free_cb, int64_t offset, pa_seek_mode_t seek);
	int                   (*stream_get_latency)(pa_stream *s, pa_usec_t *r_usec, int *negative);  /* optional */
#else
	/* Legacy synchronous simple API */
	pa_simple* (*simple_new)(const char * server, const char * name, pa_stream_direction_t dir, const char * dev, const char * stream_name, const pa_sample_spec * ss, const pa_channel_map * map, const pa_buffer_attr * attr, int * error);
	int (*simple_write)(pa_simple * s, const void * data, size_t bytes, int * error);
	int (*simple_drain)(pa_simple * s, int * error);
	void (*simple_free)(pa_simple * s);
	pa_usec_t (*simple_get_latency)(pa_simple * s, int * error);  /* optional */
#endif
};
struct pulseaudio_api_struct *pu_api = NULL;
#ifdef XPDEV_THREAD_SAFE
static pa_threaded_mainloop * pu_mainloop = NULL;
static pa_context *           pu_context = NULL;
static pa_stream *            pu_stream = NULL;
#else
static pa_simple *            pu_handle;
#endif
static bool                   pulseaudio_initialized = false;
#endif

#ifdef WITH_PORTAUDIO
static PaStream *             portaudio_stream;
#ifndef XPDEV_THREAD_SAFE
static int                    portaudio_buf_len = 0;
static int                    portaudio_buf_pos = 0;
#endif
static const unsigned char *  pawave = NULL;
static bool                   portaudio_initialized = false;
/* PortAudio v1.8 (the pre-v19 "pablio" API) was deprecated in 2002 and is
 * not shipped by any current distribution. We target v19 exclusively. */
struct portaudio_api_struct {
	dll_handle dl;
	PaError (*init)(void);
	PaError (*open)(PaStream **stream,
	                int numInputChannels,
	                int numOutputChannels,
	                PaSampleFormat sampleFormat,
	                double sampleRate,
	                unsigned long framesPerBuffer,
	                PaStreamCallback *streamCallback,
	                void *userData);
	PaError (*close)(PaStream *);
	PaError (*start)(PaStream *stream);
	PaError (*stop)(PaStream *stream);
	PaError (*active)(PaStream *stream);
	const PaStreamInfo *(*get_stream_info)(PaStream *stream);  /* optional */
};
struct portaudio_api_struct *pa_api = NULL;
#endif

#ifdef WITH_SDL_AUDIO
static SDL_AudioSpec         spec;
static int                   sdl_audio_buf_len = 0;
static int                   sdl_audio_buf_pos = 0;
#ifndef XPDEV_THREAD_SAFE
static const unsigned char * swave;
#endif
static SDL_sem *             sdlToneDone;
#endif

#ifdef _WIN32
static HWAVEOUT              waveOut;
/* waveOut playback is a push model — we submit WAVEHDRs and the driver plays
 * them in order. A deeper ring of pre-prepared buffers absorbs scheduling
 * jitter (15+ms timer resolution on many Windows builds) so the queue stays
 * ahead of the hardware consumer. 4 × 11.6 ms ≈ 46 ms of headroom. */
#define WIN32_NBUFS 4
static WAVEHDR               wh[WIN32_NBUFS];
static int                   curr_wh;
static HANDLE                waveOut_done;  /* auto-reset event the driver
                                             * signals on each buffer
                                             * completion (CALLBACK_EVENT). */
#ifdef XPDEV_THREAD_SAFE
/* Running count of frames successfully submitted to the driver via
 * waveOutWrite. Paired with waveOutGetPosition(TIME_SAMPLES) to derive
 * queued-but-not-yet-played latency in xp_device_latency_ms. */
static uint64_t              waveOut_frames_written;
#endif
#endif

#ifdef USE_ALSA_SOUND
static snd_pcm_t *           playback_handle;
static snd_pcm_hw_params_t * hw_params = NULL;
#endif

#ifdef WITH_COREAUDIO
#define CA_NUM_BUFS 3
/* 1024 frames of S16 stereo = 4 KB per buffer, ~23 ms. 3 buffers ≈ 70 ms
 * total queue — comparable to SDL/PortAudio default sizing. Keeps the
 * initial-silence priming short. */
#define CA_BUF_SIZE (1024 * XPBEEP_FRAMESIZE)
static AudioQueueRef         ca_queue;
static AudioQueueBufferRef   ca_bufs[CA_NUM_BUFS];
static sem_t                 ca_buf_sem;
static bool                  ca_initialized = false;
#ifdef XPDEV_THREAD_SAFE
/* Frames enqueued into the AudioQueue. Paired with AudioQueueGetCurrentTime
 * (queue's own sample timeline, starting at 0) to derive enqueued-but-not-
 * yet-played latency in xp_device_latency_ms. */
static uint64_t              ca_frames_enqueued;
#endif

#ifdef XPDEV_THREAD_SAFE
/* The CoreAudio output callback calls the mixer pull; forward-declare
 * here because the full mixer declaration sits further down the file. */
size_t xp_mixer_pull(int16_t *out, size_t frames);
#endif

static void
ca_output_callback(void *userdata, AudioQueueRef queue, AudioQueueBufferRef buf)
{
#ifdef XPDEV_THREAD_SAFE
	size_t frames = CA_BUF_SIZE / S_FRAMESIZE;
	xp_mixer_pull((int16_t *)buf->mAudioData, frames);
	buf->mAudioDataByteSize = CA_BUF_SIZE;
	if (AudioQueueEnqueueBuffer(queue, buf, 0, NULL) == noErr)
		ca_frames_enqueued += frames;
#else
	(void)queue; (void)userdata;
	buf->mAudioDataByteSize = 0;
	sem_post(&ca_buf_sem);
#endif
}
#endif

#ifdef AFMT_S16_NE
static int                   dsp;
#endif

#define WAVE_PI 3.14159265358979323846
#define WAVE_TPI 6.28318530717958647692

#ifdef USE_ALSA_SOUND
struct alsa_api_struct {
	dll_handle dl;
	int     (*snd_pcm_open)
	(snd_pcm_t * *pcm, const char *name, snd_pcm_stream_t stream, int mode);
	int     (*snd_pcm_hw_params_malloc)
	(snd_pcm_hw_params_t * *ptr);
	int     (*snd_pcm_hw_params_any)
	(snd_pcm_t * pcm, snd_pcm_hw_params_t *params);
	int     (*snd_pcm_hw_params_set_access)
	(snd_pcm_t * pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t _access);
	int     (*snd_pcm_hw_params_set_format)
	(snd_pcm_t * pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val);
	int     (*snd_pcm_hw_params_set_rate_near)
	(snd_pcm_t * pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
	int     (*snd_pcm_hw_params_set_channels)
	(snd_pcm_t * pcm, snd_pcm_hw_params_t *params, unsigned int val);
	int     (*snd_pcm_hw_params)
	(snd_pcm_t * pcm, snd_pcm_hw_params_t *params);
	int     (*snd_pcm_prepare)
	(snd_pcm_t * pcm);
	void    (*snd_pcm_hw_params_free)
	(snd_pcm_hw_params_t * obj);
	int     (*snd_pcm_close)
	(snd_pcm_t * pcm);
	snd_pcm_sframes_t (*snd_pcm_writei)
	    (snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
	int     (*snd_pcm_drain)
	(snd_pcm_t * pcm);
	int     (*snd_pcm_delay)                 /* optional */
	(snd_pcm_t *pcm, snd_pcm_sframes_t *delayp);
};

struct alsa_api_struct *alsa_api = NULL;
#endif

#ifdef XPDEV_THREAD_SAFE
static void     init_sample(void);
static bool     start_device_thread_locked(void);
static void     stop_device_thread_locked(void);
static bool     backend_is_push(int h);
static bool     post_open_setup_locked(void);
static uint32_t xp_device_latency_ms(void);
size_t          xp_mixer_pull(int16_t *out, size_t frames);

/* Mixer state (defined in full later in the file). Hoisted here so
 * xptone_complete_locked can drain streams on completion. */
#define XP_AUDIO_MAX_STREAMS        32
#define XP_AUDIO_DEFAULT_RING_FRAMES (S_RATE)
#define XP_AUDIO_FADE_FLOOR_DB      -60.0
#define XP_AUDIO_FADE_TABLE_SIZE    1024
#define XP_AUDIO_DEVICE_CHUNK       512

struct xp_audio_stream;
static struct xp_audio_stream *mixer_streams[XP_AUDIO_MAX_STREAMS];
static pthread_mutex_t         mixer_lock;
#endif

/********************************************************************************/
/* Calculate and generate a sound wave pattern (thanks to Deuce!)				*/
/********************************************************************************/
/*
 * Produces `samples` frames of S16 stereo-interleaved PCM into `wave`.
 *
 * SINE/SAWTOOTH/SQUARE compute natively in S16 for a clean high-resolution
 * signal. SINE_HARM, SINE_SAW, SINE_SAW_CHORD and SINE_SAW_HARM preserve
 * their original U8 numeric artifacts (intentional overwrites with `=`, missing
 * `+ 128` bias on harmonics, unsigned-char truncation from `+=`) and are
 * expanded to S16 only after the U8 math runs — this keeps those shapes'
 * distinctive noisy character identical to the old playback.
 *
 * Both L and R channels carry the same mono value per frame.
 */
void xptone_makewave(double freq, int16_t *wave, int samples, DWORD shape)
{
	int    i;
	int    midpoint;
	double inc;
	double pos;
	bool   endhigh;
	int    crossings;
	int    numcross = 0;
	bool   noisy;
	int16_t *mono;

	if (freq == 0) {
		memset(wave, 0, (size_t)samples * S_FRAMESIZE);
		return;
	}

	midpoint = samples / 2;
	inc = 8.0 * atan(1.0);
	inc *= ((double)freq / (double)S_RATE);

	/* Use the first `samples` int16_t slots of `wave` as a mono scratch area.
	 * The full stereo layout needs samples*2 slots; we expand to stereo at
	 * the end by walking high→low. */
	mono = wave;

	switch (shape & WAVE_SHAPE_MASK) {
		case WAVE_SHAPE_SINE_HARM:
		case WAVE_SHAPE_SINE_SAW:
		case WAVE_SHAPE_SINE_SAW_CHORD:
		case WAVE_SHAPE_SINE_SAW_HARM:
			noisy = true;
			break;
		default:
			noisy = false;
			break;
	}

	for (i = 0; i < samples; i++) {
		pos = (inc * (double)i);
		pos -= (int)(pos / WAVE_TPI) * WAVE_TPI;
		if (noisy) {
			/* Preserve the exact U8-wrapping arithmetic of the original.
			 * Do not "fix" the `=` overwrites or the missing `+ 128` bias. */
			unsigned char u8 = 0;
			switch (shape & WAVE_SHAPE_MASK) {
				case WAVE_SHAPE_SINE_HARM:
					u8 = (sin (pos)) * 64 + 128;
					u8 = (sin ((inc * 2) * (double)i)) * 24;
					u8 = (sin ((inc * 3) * (double)i)) * 16;
					break;
				case WAVE_SHAPE_SINE_SAW:
					u8 = (((sin (pos)) * 127 + 128) + ((WAVE_TPI - pos) * 40.5)) / 2;
					break;
				case WAVE_SHAPE_SINE_SAW_CHORD:
					u8 = (((sin (pos)) * 64 + 128) + ((WAVE_TPI - pos) * 6.2)) / 2;
					u8 += (sin ((inc / 2) * (double)i)) * 24;
					u8 += (sin ((inc / 3) * (double)i)) * 16;
					break;
				case WAVE_SHAPE_SINE_SAW_HARM:
					u8 = (((sin (pos)) * 64 + 128) + ((WAVE_TPI - pos) * 6.2)) / 2;
					u8 += (sin ((inc * 2) * (double)i)) * 24;
					u8 += (sin ((inc * 3) * (double)i)) * 16;
					break;
			}
			/* 12dB reduction in U8 space, matching the historical coefficient. */
			u8 = ((((double)u8) - 128) * 0.251) + 128;
			mono[i] = (int16_t)(((int)u8 - 128) << 8);
		}
		else {
			double v = 0.0;
			switch (shape & WAVE_SHAPE_MASK) {
				case WAVE_SHAPE_SINE:
					v = sin(pos) * 32767.0;
					break;
				case WAVE_SHAPE_SAWTOOTH:
					/* Downward ramp, matching the original direction. */
					v = 32767.0 - (pos / WAVE_TPI) * 65535.0;
					break;
				case WAVE_SHAPE_SQUARE:
					v = (pos < WAVE_PI) ? 32767.0 : -32768.0;
					break;
			}
			v *= 0.251;
			mono[i] = (int16_t)v;
		}
	}

	if ((shape & WAVE_SHAPE_NO_CLEAN) == 0) {
		/* Click-prevention cleanup: trim the trailing partial wave, then
		 * ramp the first/last few zero crossings. Silence is 0 in S16. */
		endhigh = (mono[samples - 1] > 0);
		i = samples - 1;
		if (mono[i] != 0) {
			for (; i > midpoint; i--) {
				if (endhigh && mono[i] < 0)
					break;
				if (!endhigh && mono[i] > 0)
					break;
				mono[i] = 0;
			}
		}

		/* ~5 ms worth of zero crossings per ARRL; a full wave crosses twice. */
		numcross = freq / 100;
		if (numcross) {
			int d;
			crossings = 0;
			for (i = 0; i < (samples - 1); i++) {
				if (((mono[i] < 0 && mono[i + 1] >= 0) || (mono[i] > 0 && mono[i + 1] <= 0)) && i > 2) {
					crossings++;
					if (crossings >= numcross)
						break;
				}
				d = numcross - crossings;
				mono[i] = (int16_t)(mono[i] / (d * d));
			}
			crossings = 0;
			for (i = samples - 1; i > 0; i--) {
				if (((mono[i] < 0 && mono[i - 1] >= 0) || (mono[i] > 0 && mono[i - 1] <= 0)) && i > 2) {
					crossings++;
					if (crossings >= numcross)
						break;
				}
				d = numcross - crossings;
				mono[i] = (int16_t)(mono[i] / (d * d));
			}
		}
	}

	/* Expand mono→stereo in-place. Walk high→low so each source slot is
	 * read before the destination write overwrites it. */
	for (i = samples - 1; i >= 0; i--) {
		int16_t v = mono[i];
		wave[i * 2] = v;
		wave[i * 2 + 1] = v;
	}
}

#ifdef WITH_PORTAUDIO
/*
 * PortAudio v19 stream callback: in threaded builds, fills the output buffer
 * by pulling from the mixer. In non-threaded builds, copies from a pending
 * buffer set by do_xp_play_sample (legacy synchronous path).
 */
static int portaudio_callback(const void *input,
                              void *output,
                              unsigned long frameCount,
                              const PaStreamCallbackTimeInfo *timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void *userData)
{
	(void)input; (void)timeInfo; (void)statusFlags;
#ifdef XPDEV_THREAD_SAFE
	(void)userData;
	xp_mixer_pull((int16_t *)output, frameCount);
	return paContinue;
#else
	size_t bytes    = (size_t)frameCount * S_FRAMESIZE;
	size_t maxbytes = portaudio_buf_len - portaudio_buf_pos;
	size_t copybytes = bytes;

	if (copybytes > maxbytes) {
		copybytes = maxbytes;
		memset(((char *)output) + copybytes, 0, bytes - copybytes);
	}
	if (copybytes) {
		memcpy(output, (*((unsigned char **)userData)) + portaudio_buf_pos, copybytes);
		portaudio_buf_pos += copybytes;
	}
	if (portaudio_buf_pos >= portaudio_buf_len)
		return paComplete;
	return paContinue;
#endif
}
#endif

#if defined(WITH_PULSEAUDIO) && defined(XPDEV_THREAD_SAFE)
/* Context/stream state callbacks just wake the open/close thread waiting on
 * the mainloop condition variable; the actual state is read by the waiter. */
static void
pa_ctx_state_cb(pa_context *c, void *userdata)
{
	(void)c; (void)userdata;
	pu_api->threaded_mainloop_signal(pu_mainloop, 0);
}

static void
pa_stream_state_cb(pa_stream *s, void *userdata)
{
	(void)s; (void)userdata;
	pu_api->threaded_mainloop_signal(pu_mainloop, 0);
}

/* Pull-driver: the mainloop invokes this with the mainloop lock held whenever
 * the stream has writable space. Fill `nbytes` from the mixer in chunks. */
static void
pa_stream_write_cb(pa_stream *s, size_t nbytes, void *userdata)
{
	int16_t      buf[1024 * S_CHANNELS]; /* 4 KB on stack, ≈23 ms */
	const size_t max_bytes = sizeof buf;

	(void)userdata;
	while (nbytes > 0) {
		size_t want   = (nbytes < max_bytes) ? nbytes : max_bytes;
		size_t frames = want / S_FRAMESIZE;
		size_t bytes;

		if (frames == 0)
			break;
		xp_mixer_pull(buf, frames);
		bytes = frames * S_FRAMESIZE;
		if (pu_api->stream_write(s, buf, bytes, NULL, 0, PA_SEEK_RELATIVE) < 0)
			return;
		nbytes -= bytes;
	}
}
#endif

#ifdef WITH_SDL_AUDIO
void sdl_fillbuf(void *userdata, Uint8 *stream, int len)
{
	(void)userdata;
#ifdef XPDEV_THREAD_SAFE
	xp_mixer_pull((int16_t *)stream, (size_t)len / S_FRAMESIZE);
#else
	{
		int copylen = len;
		int maxlen  = sdl_audio_buf_len - sdl_audio_buf_pos;

		if (copylen > maxlen)
			copylen = maxlen;
		if (len > copylen)
			memset(stream + copylen, spec.silence, len - copylen);
		if (copylen) {
			memcpy(stream, swave + sdl_audio_buf_pos, copylen);
			sdl_audio_buf_pos += copylen;
			if (sdl_audio_buf_pos >= sdl_audio_buf_len) {
				xpbeep_sdl.SemPost(sdlToneDone);
				sdl_audio_buf_len = 0;
				sdl_audio_buf_pos = 0;
				free((void *)swave);
			}
		}
	}
#endif
}
#endif

#ifdef XPDEV_THREAD_SAFE
pthread_once_t sample_initialized_pto = PTHREAD_ONCE_INIT;
#endif
static bool
xptone_open_locked(void)
{
#ifdef _WIN32
	WAVEFORMATEX w;
#endif

#ifdef AFMT_S16_NE
	int          format = AFMT_S16_NE;
	int          channels = S_CHANNELS;
	int          rate = S_RATE;
	/* 4 fragments × 1024 bytes = 4 KB total kernel buffer ≈ 23 ms at
	 * S16 stereo 44100. Caps how much pre-queued silence can stack up
	 * before the first note reaches the speaker. */
	int          fragsize = 0x0004000a;
#endif

	/* Already open */

	if (handle_type != SOUND_DEVICE_CLOSED) {
		handle_rc++;
		return true;
	}

#ifdef WITH_COREAUDIO
	if (xpbeep_sound_devices_enabled & XPBEEP_DEVICE_COREAUDIO) {
		if (!coreaudio_device_open_failed) {
			AudioStreamBasicDescription fmt = {0};
			fmt.mSampleRate = S_RATE;
			fmt.mFormatID = kAudioFormatLinearPCM;
			fmt.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
			fmt.mBytesPerPacket = S_FRAMESIZE;
			fmt.mFramesPerPacket = 1;
			fmt.mBytesPerFrame = S_FRAMESIZE;
			fmt.mChannelsPerFrame = S_CHANNELS;
			fmt.mBitsPerChannel = 16;

			if (!ca_initialized) {
				sem_init(&ca_buf_sem, 0, CA_NUM_BUFS);
				ca_initialized = true;
			}
			if (AudioQueueNewOutput(&fmt, ca_output_callback, NULL,
			    NULL, NULL, 0, &ca_queue) == noErr) {
#ifdef XPDEV_THREAD_SAFE
				ca_frames_enqueued = 0;
#endif
				for (int i = 0; i < CA_NUM_BUFS; i++) {
					if (AudioQueueAllocateBuffer(ca_queue,
					    CA_BUF_SIZE, &ca_bufs[i]) != noErr) {
						coreaudio_device_open_failed = true;
						AudioQueueDispose(ca_queue, true);
						ca_queue = NULL;
						break;
					}
					/* Prime each buffer with silence and enqueue it.
					 * AudioQueueStart needs enqueued buffers to play;
					 * without this the queue has nothing to consume,
					 * the callback never fires, the mixer is never
					 * pulled, and xp_audio_append blocks forever
					 * waiting for ring-buffer space. */
					memset(ca_bufs[i]->mAudioData, 0, CA_BUF_SIZE);
					ca_bufs[i]->mAudioDataByteSize = CA_BUF_SIZE;
					if (AudioQueueEnqueueBuffer(ca_queue, ca_bufs[i], 0, NULL) == noErr) {
#ifdef XPDEV_THREAD_SAFE
						ca_frames_enqueued += CA_BUF_SIZE / S_FRAMESIZE;
#endif
					}
				}
				if (!coreaudio_device_open_failed) {
					if (AudioQueueStart(ca_queue, NULL) == noErr) {
						handle_type = SOUND_DEVICE_COREAUDIO;
						handle_rc++;
						return true;
					}
					coreaudio_device_open_failed = true;
					AudioQueueDispose(ca_queue, true);
					ca_queue = NULL;
				}
			}
			else {
				coreaudio_device_open_failed = true;
			}
		}
	}
#endif

#ifdef WITH_PULSEAUDIO
	if (xpbeep_sound_devices_enabled & XPBEEP_DEVICE_PULSEAUDIO) {
		if (!pulseaudio_device_open_failed) {
			if (pu_api == NULL) {
#ifdef XPDEV_THREAD_SAFE
				const char *libnames[] = {"pulse", NULL};
				if (((pu_api = (struct pulseaudio_api_struct *)malloc(sizeof(struct pulseaudio_api_struct))) == NULL)
				    || ((pu_api->dl = xp_dlopen(libnames, RTLD_LAZY, 0)) == NULL)
				    || ((pu_api->threaded_mainloop_new = xp_dlsym(pu_api->dl, pa_threaded_mainloop_new)) == NULL)
				    || ((pu_api->threaded_mainloop_free = xp_dlsym(pu_api->dl, pa_threaded_mainloop_free)) == NULL)
				    || ((pu_api->threaded_mainloop_start = xp_dlsym(pu_api->dl, pa_threaded_mainloop_start)) == NULL)
				    || ((pu_api->threaded_mainloop_stop = xp_dlsym(pu_api->dl, pa_threaded_mainloop_stop)) == NULL)
				    || ((pu_api->threaded_mainloop_lock = xp_dlsym(pu_api->dl, pa_threaded_mainloop_lock)) == NULL)
				    || ((pu_api->threaded_mainloop_unlock = xp_dlsym(pu_api->dl, pa_threaded_mainloop_unlock)) == NULL)
				    || ((pu_api->threaded_mainloop_wait = xp_dlsym(pu_api->dl, pa_threaded_mainloop_wait)) == NULL)
				    || ((pu_api->threaded_mainloop_signal = xp_dlsym(pu_api->dl, pa_threaded_mainloop_signal)) == NULL)
				    || ((pu_api->threaded_mainloop_get_api = xp_dlsym(pu_api->dl, pa_threaded_mainloop_get_api)) == NULL)
				    || ((pu_api->context_new = xp_dlsym(pu_api->dl, pa_context_new)) == NULL)
				    || ((pu_api->context_connect = xp_dlsym(pu_api->dl, pa_context_connect)) == NULL)
				    || ((pu_api->context_disconnect = xp_dlsym(pu_api->dl, pa_context_disconnect)) == NULL)
				    || ((pu_api->context_unref = xp_dlsym(pu_api->dl, pa_context_unref)) == NULL)
				    || ((pu_api->context_set_state_callback = xp_dlsym(pu_api->dl, pa_context_set_state_callback)) == NULL)
				    || ((pu_api->context_get_state = xp_dlsym(pu_api->dl, pa_context_get_state)) == NULL)
				    || ((pu_api->stream_new = xp_dlsym(pu_api->dl, pa_stream_new)) == NULL)
				    || ((pu_api->stream_connect_playback = xp_dlsym(pu_api->dl, pa_stream_connect_playback)) == NULL)
				    || ((pu_api->stream_disconnect = xp_dlsym(pu_api->dl, pa_stream_disconnect)) == NULL)
				    || ((pu_api->stream_unref = xp_dlsym(pu_api->dl, pa_stream_unref)) == NULL)
				    || ((pu_api->stream_set_state_callback = xp_dlsym(pu_api->dl, pa_stream_set_state_callback)) == NULL)
				    || ((pu_api->stream_set_write_callback = xp_dlsym(pu_api->dl, pa_stream_set_write_callback)) == NULL)
				    || ((pu_api->stream_get_state = xp_dlsym(pu_api->dl, pa_stream_get_state)) == NULL)
				    || ((pu_api->stream_write = xp_dlsym(pu_api->dl, pa_stream_write)) == NULL)
				    ) {
					if (pu_api && pu_api->dl)
						xp_dlclose(pu_api->dl);
					free(pu_api);
					pu_api = NULL;
				}
				else {
					/* Optional — available everywhere in practice, used by
					 * xp_device_latency_ms for accurate device drain sleep. */
					pu_api->stream_get_latency = xp_dlsym(pu_api->dl, pa_stream_get_latency);
				}
#else
				const char *libnames[] = {"pulse-simple", NULL};
				if (((pu_api = (struct pulseaudio_api_struct *)malloc(sizeof(struct pulseaudio_api_struct))) == NULL)
				    || ((pu_api->dl = xp_dlopen(libnames, RTLD_LAZY, 0)) == NULL)
				    || ((pu_api->simple_new = xp_dlsym(pu_api->dl, pa_simple_new)) == NULL)
				    || ((pu_api->simple_write = xp_dlsym(pu_api->dl, pa_simple_write)) == NULL)
				    || ((pu_api->simple_drain = xp_dlsym(pu_api->dl, pa_simple_drain)) == NULL)
				    || ((pu_api->simple_free = xp_dlsym(pu_api->dl, pa_simple_free)) == NULL)
				    ) {
					if (pu_api && pu_api->dl)
						xp_dlclose(pu_api->dl);
					free(pu_api);
					pu_api = NULL;
				}
				else {
					pu_api->simple_get_latency = xp_dlsym(pu_api->dl, pa_simple_get_latency);
				}
#endif
				if (pu_api == NULL) {
					pulseaudio_device_open_failed = true;
				}
			}
			if (pu_api != NULL) {
				pa_sample_spec ss;
				ss.format   = PA_SAMPLE_S16NE;
				ss.rate     = S_RATE;
				ss.channels = S_CHANNELS;
#ifdef XPDEV_THREAD_SAFE
				{
					pa_mainloop_api *api = NULL;
					bool             ok  = false;

					pu_mainloop = pu_api->threaded_mainloop_new();
					if (pu_mainloop) {
						api = pu_api->threaded_mainloop_get_api(pu_mainloop);
						pu_context = pu_api->context_new(api, "XPBeep");
					}
					if (pu_context) {
						pu_api->context_set_state_callback(pu_context, pa_ctx_state_cb, NULL);
						if (pu_api->context_connect(pu_context, NULL,
						                            PA_CONTEXT_NOFLAGS, NULL) == 0
						    && pu_api->threaded_mainloop_start(pu_mainloop) == 0) {
							pa_context_state_t cs;
							pu_api->threaded_mainloop_lock(pu_mainloop);
							for (;;) {
								cs = pu_api->context_get_state(pu_context);
								if (cs == PA_CONTEXT_READY
								    || cs == PA_CONTEXT_FAILED
								    || cs == PA_CONTEXT_TERMINATED)
									break;
								pu_api->threaded_mainloop_wait(pu_mainloop);
							}
							if (cs == PA_CONTEXT_READY) {
								pu_stream = pu_api->stream_new(pu_context,
								                               "Beeps and Boops",
								                               &ss, NULL);
								if (pu_stream) {
									pa_buffer_attr    battr;
									pa_stream_state_t sts;
									/* ~46 ms target buffer, server picks the
									 * rest based on ADJUST_LATENCY. */
									battr.maxlength = (uint32_t)-1;
									battr.tlength   = 2048 * S_FRAMESIZE;
									battr.prebuf    = (uint32_t)-1;
									battr.minreq    = (uint32_t)-1;
									battr.fragsize  = (uint32_t)-1;
									pu_api->stream_set_state_callback(pu_stream, pa_stream_state_cb, NULL);
									pu_api->stream_set_write_callback(pu_stream, pa_stream_write_cb, NULL);
									if (pu_api->stream_connect_playback(pu_stream,
									                                    NULL, &battr,
									                                    PA_STREAM_ADJUST_LATENCY,
									                                    NULL, NULL) == 0) {
										for (;;) {
											sts = pu_api->stream_get_state(pu_stream);
											if (sts == PA_STREAM_READY
											    || sts == PA_STREAM_FAILED
											    || sts == PA_STREAM_TERMINATED)
												break;
											pu_api->threaded_mainloop_wait(pu_mainloop);
										}
										if (sts == PA_STREAM_READY)
											ok = true;
									}
								}
							}
							pu_api->threaded_mainloop_unlock(pu_mainloop);
							if (!ok)
								pu_api->threaded_mainloop_stop(pu_mainloop);
						}
					}
					if (ok) {
						pulseaudio_initialized = true;
						handle_type = SOUND_DEVICE_PULSEAUDIO;
						handle_rc++;
						return true;
					}
					/* Cleanup on failure. */
					if (pu_stream) {
						pu_api->stream_unref(pu_stream);
						pu_stream = NULL;
					}
					if (pu_context) {
						pu_api->context_disconnect(pu_context);
						pu_api->context_unref(pu_context);
						pu_context = NULL;
					}
					if (pu_mainloop) {
						pu_api->threaded_mainloop_free(pu_mainloop);
						pu_mainloop = NULL;
					}
					pulseaudio_device_open_failed = true;
				}
#else
				{
					int err;
					pu_handle = pu_api->simple_new(NULL, "XPBeep", PA_STREAM_PLAYBACK,
					                               NULL, "Beeps and Boops",
					                               &ss, NULL, NULL, &err);
					if (pu_handle == NULL) {
						pulseaudio_device_open_failed = true;
					}
					else {
						pulseaudio_initialized = true;
						handle_type = SOUND_DEVICE_PULSEAUDIO;
						handle_rc++;
						return true;
					}
				}
#endif
			}
		}
	}
#endif

#ifdef WITH_PORTAUDIO
	if (xpbeep_sound_devices_enabled & XPBEEP_DEVICE_PORTAUDIO) {
		if (!portaudio_device_open_failed) {
			if (pa_api == NULL) {
				const char *libnames[] = {"portaudio", NULL};
				if (((pa_api = (struct portaudio_api_struct *)malloc(sizeof(struct portaudio_api_struct))) == NULL)
				    || ((pa_api->dl = xp_dlopen(libnames, RTLD_LAZY, 0)) == NULL)
				    || ((pa_api->init = xp_dlsym(pa_api->dl, Pa_Initialize)) == NULL)
				    || ((pa_api->open = xp_dlsym(pa_api->dl, Pa_OpenDefaultStream)) == NULL)
				    || ((pa_api->close = xp_dlsym(pa_api->dl, Pa_CloseStream)) == NULL)
				    || ((pa_api->start = xp_dlsym(pa_api->dl, Pa_StartStream)) == NULL)
				    ||
					(
						((pa_api->active = xp_dlsym(pa_api->dl, Pa_StreamActive)) == NULL)
						&& ((pa_api->active = xp_dlsym(pa_api->dl, Pa_IsStreamActive)) == NULL)
				    )
				    || ((pa_api->stop = xp_dlsym(pa_api->dl, Pa_StopStream)) == NULL)
				    ) {
					if (pa_api->dl)
						xp_dlclose(pa_api->dl);
					free(pa_api);
					pa_api = NULL;
				}
				else {
					/* Optional — available on all v19 builds; used for
					 * latency reporting in xp_device_latency_ms. */
					pa_api->get_stream_info = xp_dlsym(pa_api->dl, Pa_GetStreamInfo);
				}
				if (pa_api == NULL) {
					portaudio_device_open_failed = true;
				}
			}
			if (pa_api != NULL) {
				if (!portaudio_initialized) {
					if (pa_api->init() != paNoError)
						portaudio_device_open_failed = true;
					else
						portaudio_initialized = true;
				}
				if (portaudio_initialized) {
					if (pa_api->open(&portaudio_stream, 0, S_CHANNELS,
					                 paInt16, S_RATE, 1024,
					                 portaudio_callback, &pawave) != paNoError)
						portaudio_device_open_failed = true;
					else {
						handle_type = SOUND_DEVICE_PORTAUDIO;
						handle_rc++;
						return true;
					}
				}
			}
		}
	}
#endif

#ifdef WITH_SDL_AUDIO
	if (xpbeep_sound_devices_enabled & XPBEEP_DEVICE_SDL) {
		if (!sdl_device_open_failed) {
			if (init_sdl_audio() == -1) {
				sdl_device_open_failed = true;
			}
			else {
				spec.freq = S_RATE;
				spec.format = AUDIO_S16SYS;
				spec.channels = S_CHANNELS;
				spec.samples = 1024;      /* frames per callback */
				spec.size = 1024 * S_FRAMESIZE;
				spec.callback = sdl_fillbuf;
				spec.userdata = NULL;
				if (xpbeep_sdl.OpenAudio(&spec, NULL) == -1) {
					sdl_device_open_failed = true;
				}
				else {
					sdlToneDone = xpbeep_sdl.SDL_CreateSemaphore(0);
					sdl_audio_buf_len = 0;
					sdl_audio_buf_pos = 0;
					xpbeep_sdl.PauseAudio(false);
					handle_type = SOUND_DEVICE_SDL;
					handle_rc++;
					return true;
				}
			}
		}
	}
#endif

#ifdef _WIN32
	if (xpbeep_sound_devices_enabled & XPBEEP_DEVICE_WIN32) {
		if (!sound_device_open_failed) {
			memset(&w, 0, sizeof(w));
			w.wFormatTag = WAVE_FORMAT_PCM;
			w.nChannels = S_CHANNELS;
			w.nSamplesPerSec = S_RATE;
			w.wBitsPerSample = S_BYTEDEPTH * 8;
			w.nBlockAlign = (w.wBitsPerSample * w.nChannels) / 8;
			w.nAvgBytesPerSec = w.nSamplesPerSec * w.nBlockAlign;

			waveOut_done = CreateEvent(NULL, FALSE, FALSE, NULL);
			if (waveOut_done == NULL) {
				sound_device_open_failed = true;
				return false;
			}
			if (waveOutOpen(&waveOut, WAVE_MAPPER, &w,
			                (DWORD_PTR)waveOut_done, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
				CloseHandle(waveOut_done);
				waveOut_done = NULL;
				sound_device_open_failed = true;
				return false;
			}
			memset(&wh, 0, sizeof(wh));
#ifdef XPDEV_THREAD_SAFE
			/* Pre-allocate and prepare all buffers up front for the mixer
			 * device thread. It just refills lpData and re-submits — no
			 * per-chunk malloc/prepare/unprepare cycle, which was adding
			 * per-iteration latency and (combined with coarse SLEEP(1)
			 * timing) causing queue underruns audible as a ~7-8 Hz warble.
			 *
			 * Non-thread-safe builds don't use the mixer; do_xp_play_sample
			 * allocates per-write as before, so no pre-alloc is needed here. */
			{
				int i;
				size_t chunk_bytes = XP_AUDIO_DEVICE_CHUNK * S_FRAMESIZE;
				bool ok = true;
				for (i = 0; i < WIN32_NBUFS; i++) {
					wh[i].lpData = malloc(chunk_bytes);
					if (wh[i].lpData == NULL) {
						ok = false;
						break;
					}
					wh[i].dwBufferLength = chunk_bytes;
					if (waveOutPrepareHeader(waveOut, &wh[i], sizeof(wh[i])) != MMSYSERR_NOERROR) {
						ok = false;
						break;
					}
				}
				if (!ok) {
					int j;
					for (j = 0; j < i; j++) {
						waveOutUnprepareHeader(waveOut, &wh[j], sizeof(wh[j]));
						free(wh[j].lpData);
						wh[j].lpData = NULL;
					}
					if (i < WIN32_NBUFS && wh[i].lpData) {
						free(wh[i].lpData);
						wh[i].lpData = NULL;
					}
					waveOutClose(waveOut);
					CloseHandle(waveOut_done);
					waveOut_done = NULL;
					sound_device_open_failed = true;
					return false;
				}
			}
#endif
			curr_wh = 0;
#ifdef XPDEV_THREAD_SAFE
			waveOut_frames_written = 0;
#endif
			handle_type = SOUND_DEVICE_WIN32;
			handle_rc++;
			return true;
		}
	}
#endif

#ifdef USE_ALSA_SOUND
	if (xpbeep_sound_devices_enabled & XPBEEP_DEVICE_ALSA) {
		if (!alsa_device_open_failed) {
			if (alsa_api == NULL) {
				const char *libnames[] = {"asound", NULL};
				if (((alsa_api = (struct alsa_api_struct *)malloc(sizeof(struct alsa_api_struct))) == NULL)
				    || ((alsa_api->dl = xp_dlopen(libnames, RTLD_LAZY, 2)) == NULL)
				    || ((alsa_api->snd_pcm_open = xp_dlsym(alsa_api->dl, snd_pcm_open)) == NULL)
				    || ((alsa_api->snd_pcm_hw_params_malloc = xp_dlsym(alsa_api->dl, snd_pcm_hw_params_malloc)) == NULL)
				    || ((alsa_api->snd_pcm_hw_params_any = xp_dlsym(alsa_api->dl, snd_pcm_hw_params_any)) == NULL)
				    || ((alsa_api->snd_pcm_hw_params_set_access = xp_dlsym(alsa_api->dl, snd_pcm_hw_params_set_access)) == NULL)
				    || ((alsa_api->snd_pcm_hw_params_set_format = xp_dlsym(alsa_api->dl, snd_pcm_hw_params_set_format)) == NULL)
				    || ((alsa_api->snd_pcm_hw_params_set_rate_near = xp_dlsym(alsa_api->dl, snd_pcm_hw_params_set_rate_near)) == NULL)
				    || ((alsa_api->snd_pcm_hw_params_set_channels = xp_dlsym(alsa_api->dl, snd_pcm_hw_params_set_channels)) == NULL)
				    || ((alsa_api->snd_pcm_hw_params = xp_dlsym(alsa_api->dl, snd_pcm_hw_params)) == NULL)
				    || ((alsa_api->snd_pcm_prepare = xp_dlsym(alsa_api->dl, snd_pcm_prepare)) == NULL)
				    || ((alsa_api->snd_pcm_hw_params_free = xp_dlsym(alsa_api->dl, snd_pcm_hw_params_free)) == NULL)
				    || ((alsa_api->snd_pcm_close = xp_dlsym(alsa_api->dl, snd_pcm_close)) == NULL)
				    || ((alsa_api->snd_pcm_writei = xp_dlsym(alsa_api->dl, snd_pcm_writei)) == NULL)
				    || ((alsa_api->snd_pcm_drain = xp_dlsym(alsa_api->dl, snd_pcm_drain)) == NULL)
				    ) {
					if (alsa_api->dl)
						xp_dlclose(alsa_api->dl);
					free(alsa_api);
					alsa_api = NULL;
					alsa_device_open_failed = true;
				}
				else {
					/* Optional — available since ALSA ~1.0, but treat as
					 * optional so old libraries still load. */
					alsa_api->snd_pcm_delay = xp_dlsym(alsa_api->dl, snd_pcm_delay);
				}
				if (alsa_api == NULL)
					alsa_device_open_failed = true;
			}
			if (alsa_api != NULL) {
				unsigned int rate = S_RATE;
				if ((alsa_api->snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0)
				    || (alsa_api->snd_pcm_hw_params_malloc(&hw_params) < 0)
				    || (alsa_api->snd_pcm_hw_params_any(playback_handle, hw_params) < 0)
				    || (alsa_api->snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
				    || (alsa_api->snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_S16) < 0)
				    || (alsa_api->snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0) < 0)
				    || (alsa_api->snd_pcm_hw_params_set_channels(playback_handle, hw_params, S_CHANNELS) < 0)
				    || (alsa_api->snd_pcm_hw_params(playback_handle, hw_params) < 0)
				    || (alsa_api->snd_pcm_prepare(playback_handle) < 0)) {
					alsa_device_open_failed = true;
					if (hw_params != NULL)
						alsa_api->snd_pcm_hw_params_free(hw_params);
					if (playback_handle != NULL) {
						alsa_api->snd_pcm_close(playback_handle);
						playback_handle = NULL;
					}
				}
				else {
					alsa_api->snd_pcm_hw_params_free(hw_params);
					handle_type = SOUND_DEVICE_ALSA;
					handle_rc++;
					return true;
				}
			}
		}
	}
#endif

#ifdef AFMT_S16_NE
	if (xpbeep_sound_devices_enabled & XPBEEP_DEVICE_OSS) {
		if (!sound_device_open_failed) {
			if ((dsp = open("/dev/dsp", O_WRONLY, 0)) < 0) {
				sound_device_open_failed = true;
			}
			else  {
				ioctl(dsp, SNDCTL_DSP_SETFRAGMENT, &fragsize);
				if ((ioctl(dsp, SNDCTL_DSP_SETFMT, &format) == -1) || format != AFMT_S16_NE) {
					sound_device_open_failed = true;
					close(dsp);
				}
				else if ((ioctl(dsp, SNDCTL_DSP_CHANNELS, &channels) == -1) || channels != S_CHANNELS) {
					sound_device_open_failed = true;
					close(dsp);
				}
				else if ((ioctl(dsp, SNDCTL_DSP_SPEED, &rate) == -1) || rate != S_RATE) {
					sound_device_open_failed = true;
					close(dsp);
				}
			}
		}
		if (sound_device_open_failed) {
			return false;
		}
		handle_type = SOUND_DEVICE_OSS;
		if (!sound_device_open_failed) {
			handle_rc++;
			return true;
		}
	}
#endif

	return false;
}

bool
xptone_open(void)
{
	bool ret;
#ifdef XPDEV_THREAD_SAFE
	pthread_once(&sample_initialized_pto, init_sample);
	assert_pthread_mutex_lock(&handle_mutex);
#endif
	ret = xptone_open_locked();
#ifdef XPDEV_THREAD_SAFE
	if (ret)
		ret = post_open_setup_locked();
	assert_pthread_mutex_unlock(&handle_mutex);
#endif
	return ret;
}

static void
xptone_complete_locked(void)
{
	if (handle_type == SOUND_DEVICE_CLOSED) {
		return;
	}

#ifdef XPDEV_THREAD_SAFE
	/* "Complete" in the mixer world means: wait for every currently-
	 * submitted stream to fully drain through the mixer, then add one
	 * device-latency sleep so the last written frames actually reach
	 * the speaker. The device feeder keeps running throughout and emits
	 * silence once streams are drained. */
	{
		int      i;
		uint32_t extra_ms;
		for (i = 0; i < XP_AUDIO_MAX_STREAMS; i++) {
			struct xp_audio_stream *s;
			assert_pthread_mutex_lock(&mixer_lock);
			s = mixer_streams[i];
			assert_pthread_mutex_unlock(&mixer_lock);
			if (s)
				xp_audio_wait(i);
		}
		extra_ms = xp_device_latency_ms();
		if (extra_ms > 0)
			SLEEP(extra_ms);
	}
#else

#ifdef WITH_COREAUDIO
	if (handle_type == SOUND_DEVICE_COREAUDIO) {
		for (int i = 0; i < CA_NUM_BUFS; i++)
			sem_wait(&ca_buf_sem);
		for (int i = 0; i < CA_NUM_BUFS; i++)
			sem_post(&ca_buf_sem);
	}
#endif

#ifdef WITH_PULSEAUDIO
	else if (handle_type == SOUND_DEVICE_PULSEAUDIO) {
		int err;
		if (pu_handle)
			pu_api->simple_drain(pu_handle, &err);
	}
#endif

#ifdef WITH_PORTAUDIO
	else if (handle_type == SOUND_DEVICE_PORTAUDIO) {
		pa_api->stop(portaudio_stream);
		if (pawave) {
			free((void *)pawave);
			pawave = NULL;
		}
	}
#endif

#ifdef WITH_SDL_AUDIO
	else if (handle_type == SOUND_DEVICE_SDL) {
		while (xpbeep_sdl.GetAudioStatus() == SDL_AUDIO_PLAYING)
			SLEEP(1);
	}
#endif

#ifdef _WIN32
	if (handle_type == SOUND_DEVICE_WIN32) {
		if (wh[0].dwFlags & WHDR_PREPARED) {
			while (waveOutUnprepareHeader(waveOut, &wh[0], sizeof(wh)) == WAVERR_STILLPLAYING)
				SLEEP(1);
			FREE_AND_NULL(wh[0].lpData);
		}
		if (wh[1].dwFlags & WHDR_PREPARED) {
			while (waveOutUnprepareHeader(waveOut, &wh[1], sizeof(wh)) == WAVERR_STILLPLAYING)
				SLEEP(1);
			FREE_AND_NULL(wh[1].lpData);
		}
	}
#endif

#ifdef USE_ALSA_SOUND
	if (handle_type == SOUND_DEVICE_ALSA) {
		if (!alsa_device_open_failed)
			alsa_api->snd_pcm_drain(playback_handle);
	}
#endif

#ifdef AFMT_S16_NE
	else if (handle_type == SOUND_DEVICE_OSS) {
		ioctl(dsp, SNDCTL_DSP_SYNC, NULL);
	}
#endif

#endif /* XPDEV_THREAD_SAFE */
}

void
xptone_complete(void)
{
#ifdef XPDEV_THREAD_SAFE
	assert_pthread_mutex_lock(&handle_mutex);
#endif
	// coverity[sleep]
	xptone_complete_locked();
#ifdef XPDEV_THREAD_SAFE
	assert_pthread_mutex_unlock(&handle_mutex);
#endif
}

bool xptone_close_locked(void)
{
	xptone_complete_locked();

	if (handle_rc <= 0)
		return false;
	if (--handle_rc)
		return true;

#ifdef XPDEV_THREAD_SAFE
	/* Stop the device thread before tearing down the device so the thread
	 * cannot access closed handles. */
	stop_device_thread_locked();
#endif

#ifdef WITH_COREAUDIO
	if (handle_type == SOUND_DEVICE_COREAUDIO) {
		AudioQueueStop(ca_queue, true);
		AudioQueueDispose(ca_queue, true);
		ca_queue = NULL;
	}
#endif

#ifdef WITH_PORTAUDIO
	if (handle_type == SOUND_DEVICE_PORTAUDIO) {
		pa_api->close(portaudio_stream);
	}
#endif

#ifdef WITH_PULSEAUDIO
	if (handle_type == SOUND_DEVICE_PULSEAUDIO) {
#ifdef XPDEV_THREAD_SAFE
		/* Stop the mainloop thread before touching any pa_* state so no
		 * callbacks can be running during teardown. */
		if (pu_mainloop)
			pu_api->threaded_mainloop_stop(pu_mainloop);
		if (pu_stream) {
			pu_api->stream_disconnect(pu_stream);
			pu_api->stream_unref(pu_stream);
			pu_stream = NULL;
		}
		if (pu_context) {
			pu_api->context_disconnect(pu_context);
			pu_api->context_unref(pu_context);
			pu_context = NULL;
		}
		if (pu_mainloop) {
			pu_api->threaded_mainloop_free(pu_mainloop);
			pu_mainloop = NULL;
		}
		pulseaudio_initialized = false;
#else
		if (pu_handle)
			pu_api->simple_free(pu_handle);
		pu_handle = NULL;
#endif
	}
#endif

#ifdef WITH_SDL_AUDIO
	if (handle_type == SOUND_DEVICE_SDL) {
		xpbeep_sdl.CloseAudio();
		xpbeep_sdl.SDL_DestroySemaphore(sdlToneDone);
	}
#endif

#ifdef _WIN32
	if (handle_type == SOUND_DEVICE_WIN32) {
		int i;
		/* Device thread has stopped; drain, unprepare and free the ring. */
		for (i = 0; i < WIN32_NBUFS; i++) {
			if (wh[i].lpData == NULL)
				continue;
			while (waveOutUnprepareHeader(waveOut, &wh[i], sizeof(wh[i])) == WAVERR_STILLPLAYING)
				WaitForSingleObject(waveOut_done, 100);
			free(wh[i].lpData);
			wh[i].lpData = NULL;
		}
		waveOutClose(waveOut);
		if (waveOut_done) {
			CloseHandle(waveOut_done);
			waveOut_done = NULL;
		}
	}
#endif

#ifdef USE_ALSA_SOUND
	if (handle_type == SOUND_DEVICE_ALSA) {
		alsa_api->snd_pcm_close (playback_handle);
		playback_handle = NULL;
	}
#endif

#ifdef AFMT_S16_NE
	if (handle_type == SOUND_DEVICE_OSS) {
		close(dsp);
	}
#endif
	handle_type = SOUND_DEVICE_CLOSED;
	sound_device_open_failed = false;
#ifdef USE_ALSA_SOUND
	alsa_device_open_failed = false;
#endif
#ifdef WITH_SDL_AUDIO
	sdl_device_open_failed = false;
#endif
#ifdef WITH_PORTAUDIO
	portaudio_device_open_failed = false;
#endif
#ifdef WITH_PULSEAUDIO
	pulseaudio_device_open_failed = false;
#endif
#ifdef WITH_COREAUDIO
	coreaudio_device_open_failed = false;
#endif

	return true;
}

bool
xptone_close(void)
{
	bool ret;

#ifdef XPDEV_THREAD_SAFE
	assert_pthread_mutex_lock(&handle_mutex);
#endif
	// coverity[sleep]
	ret = xptone_close_locked();
#ifdef XPDEV_THREAD_SAFE
	assert_pthread_mutex_unlock(&handle_mutex);
#endif
	return ret;
}

#ifndef XPDEV_THREAD_SAFE
// This can't be const because the Win32 API is not const.
static bool
do_xp_play_sample(unsigned char *sampo, size_t sz, int *freed)
{
	unsigned char *samp;
#if defined(WITH_PORTAUDIO) || defined(_WIN32) || defined(WITH_SDL_AUDIO)
	int            need_copy = 0;
#endif

#ifdef WITH_PORTAUDIO
	if (handle_type == SOUND_DEVICE_PORTAUDIO)
		need_copy = 1;
#endif
#ifdef _WIN32
	if (handle_type == SOUND_DEVICE_WIN32)
		need_copy = 1;
#endif
#ifdef WITH_SDL_AUDIO
	if (handle_type == SOUND_DEVICE_SDL)
		need_copy = 1;
#endif

#if defined(WITH_PORTAUDIO) || defined(_WIN32) || defined(WITH_SDL_AUDIO)
	if (freed)
		*freed = need_copy;
	if (need_copy) {
		if (freed) {
			samp = sampo;
		}
		else {
			samp = malloc(sz);
			if (!samp)
				return false;
			memcpy((void *)samp, sampo, sz);
		}
	}
	else {
		samp = sampo;
	}
#else
	if (freed)
		*freed = 0;
	samp = sampo;
#endif

#ifdef WITH_COREAUDIO
	if (handle_type == SOUND_DEVICE_COREAUDIO) {
		size_t offset = 0;
		while (offset < sz) {
			sem_wait(&ca_buf_sem);
			AudioQueueBufferRef buf = NULL;
			for (int i = 0; i < CA_NUM_BUFS; i++) {
				if (ca_bufs[i]->mAudioDataByteSize == 0) {
					buf = ca_bufs[i];
					break;
				}
			}
			if (!buf) {
				sem_post(&ca_buf_sem);
				continue;
			}
			size_t chunk = sz - offset;
			if (chunk > CA_BUF_SIZE)
				chunk = CA_BUF_SIZE;
			memcpy(buf->mAudioData, samp + offset, chunk);
			buf->mAudioDataByteSize = chunk;
			AudioQueueEnqueueBuffer(ca_queue, buf, 0, NULL);
			offset += chunk;
		}
		return true;
	}
#endif

#ifdef WITH_PULSEAUDIO
	if (handle_type == SOUND_DEVICE_PULSEAUDIO) {
		int            err;
		pa_sample_spec ss;
		ss.format = PA_SAMPLE_S16NE;
		ss.rate = S_RATE;
		ss.channels = S_CHANNELS;
		if (pu_handle == NULL) {
			if ((pu_handle = pu_api->simple_new(NULL, "XPBeep", PA_STREAM_PLAYBACK, NULL, "Beeps and Boops", &ss, NULL, NULL, NULL)) == NULL) {
				pulseaudio_device_open_failed = true;
				pulseaudio_initialized = false;
				xptone_close_locked();
				xptone_open_locked();
			}
			else
				pulseaudio_initialized = true;
		}
		if (pulseaudio_initialized)
			pu_api->simple_write(pu_handle, sampo, sz, &err);
	}
#endif

#ifdef WITH_PORTAUDIO
	if (handle_type == SOUND_DEVICE_PORTAUDIO) {
		xptone_complete_locked();
		pawave = samp;
		portaudio_buf_pos = 0;
		portaudio_buf_len = sz;
		pa_api->start(portaudio_stream);
		return true;
	}
#endif

#ifdef WITH_SDL_AUDIO
	if (handle_type == SOUND_DEVICE_SDL) {
		xpbeep_sdl.LockAudio();
		swave = samp;
		sdl_audio_buf_pos = 0;
		sdl_audio_buf_len = sz;
		xpbeep_sdl.UnlockAudio();
		xpbeep_sdl.PauseAudio(false);
		xpbeep_sdl.SemWait(sdlToneDone);
		xpbeep_sdl.PauseAudio(true);
		return true;
	}
#endif

#ifdef _WIN32
	if (handle_type == SOUND_DEVICE_WIN32) {
		if (wh[curr_wh].dwFlags & WHDR_PREPARED) {
			while (waveOutUnprepareHeader(waveOut, &wh[curr_wh], sizeof(wh[curr_wh])) == WAVERR_STILLPLAYING)
				SLEEP(1);
		}
		free(wh[curr_wh].lpData);
		wh[curr_wh].lpData = (LPSTR)samp;
		wh[curr_wh].dwBufferLength = sz;
		if (waveOutPrepareHeader(waveOut, &wh[curr_wh], sizeof(wh[curr_wh])) == MMSYSERR_NOERROR) {
			if (waveOutWrite(waveOut, &wh[curr_wh], sizeof(wh[curr_wh])) == MMSYSERR_NOERROR) {
				curr_wh ^= 1;
			}
		}
		return true;
	}
#endif

#ifdef USE_ALSA_SOUND
	if (handle_type == SOUND_DEVICE_ALSA) {
		int ret;
		int written = 0;

		while (written < sz) {
			ret = alsa_api->snd_pcm_writei(playback_handle, samp + written, sz - written);
			if (ret < 0) {
				if (alsa_api->snd_pcm_prepare(playback_handle) == 0) {
					ret = 0;
				}
				else {
					if (written == 0) {
						/* Go back and try OSS */
						xptone_close_locked();
						alsa_device_open_failed = true;
						xptone_open_locked();
					}
					break;
				}
			}
			written += ret;
		}
		if (written == sz)
			return true;
	}
#endif

#ifdef AFMT_S16_NE
	if (handle_type == SOUND_DEVICE_OSS) {
		size_t wr = 0;
		while (wr < sz) {
			// coverity[overflow:SUPPRESS]
			ssize_t i = write(dsp, samp + wr, sz - wr);
			if (i >= 0)
				wr += i;
			else
				return false;
		}
		return true;
	}
#endif

	return false;
}
#endif /* !XPDEV_THREAD_SAFE */

/* ================================================================
 * Streaming mixer (Phase 2+3)
 *
 * Streams are appendable ring buffers of S16 stereo frames. The mixer
 * pulls from all active streams and sums them with per-channel volume
 * into the output buffer. The pull function is format-identical to what
 * any backend wants: fill N frames of S16 stereo at 44100 Hz.
 *
 * Backends feed the mixer in one of two shapes:
 *   - Pull-native backends (SDL, CoreAudio, PortAudio, PulseAudio async):
 *     the backend's own callback calls xp_mixer_pull directly.
 *   - Push-native backends (Win32 waveOut, OSS, ALSA): a dedicated device
 *     thread (started at open time) loops xp_mixer_pull → device_write.
 *
 * The mixer code never branches on backend — the split is entirely
 * confined to xptone_open_locked / xptone_close_locked.
 * ================================================================ */

#ifdef XPDEV_THREAD_SAFE

struct xp_audio_stream {
	int16_t         *ring;          /* ring_frames * 2 int16_t */
	size_t           ring_frames;
	size_t           write_pos;     /* monotonic frame counter */
	size_t           read_pos;      /* monotonic frame counter */
	float            volume_l;
	float            volume_r;
	bool             loop;
	bool             finished;      /* producer: no more data coming */
	bool             done;          /* mixer: fully drained */
	bool             auto_close;    /* reap on next xp_audio_open */
	sem_t            drain_sem;     /* posted when done transitions true */
	xpevent_t        progress;      /* auto-reset event; mixer pulses after
	                                 * advancing read_pos, unblocking the
	                                 * producer (xp_audio_append) or the
	                                 * foreground waiter (xp_audio_wait) */
	pthread_mutex_t  mutex;
};

/* Waiters use this timeout as a safety net so they always re-check the
 * predicate even if a progress event is missed. pthread condition variables
 * aren't in the Windows threadwrap, so we use xpevent_t instead. */
#define XP_AUDIO_POLL_MS 100

/* mixer_streams and mixer_lock declared earlier. */
static pthread_once_t mixer_once = PTHREAD_ONCE_INIT;
static int16_t        fade_gain_table[XP_AUDIO_FADE_TABLE_SIZE];  /* Q1.15 */

static void
mixer_init(void)
{
	int i;
	assert_pthread_mutex_init(&mixer_lock, NULL);
	/* Precompute dB-linear fade gain: position 0 → -60 dB,
	 * position TABLE_SIZE-1 → 0 dB. */
	for (i = 0; i < XP_AUDIO_FADE_TABLE_SIZE; i++) {
		double t    = (double)i / (double)(XP_AUDIO_FADE_TABLE_SIZE - 1);
		double db   = XP_AUDIO_FADE_FLOOR_DB * (1.0 - t);
		double gain = pow(10.0, db / 20.0);
		int    q15  = (int)(gain * 32767.0 + 0.5);
		if (q15 > 32767) q15 = 32767;
		if (q15 < 0)     q15 = 0;
		fade_gain_table[i] = (int16_t)q15;
	}
}

/* Returns Q1.15 gain at `pos / total` along a fade-in curve (0 → unity).
 * For a fade-out, callers pass `total - 1 - pos` as the position. */
static int16_t
fade_gain_q15(size_t pos, size_t total)
{
	double norm;
	double idx_f;
	int    idx;
	double frac;

	if (total == 0 || pos >= total)
		return 32767;
	norm  = (double)pos / (double)total;
	idx_f = norm * (double)(XP_AUDIO_FADE_TABLE_SIZE - 1);
	idx   = (int)idx_f;
	if (idx >= XP_AUDIO_FADE_TABLE_SIZE - 1)
		return fade_gain_table[XP_AUDIO_FADE_TABLE_SIZE - 1];
	frac = idx_f - (double)idx;
	return (int16_t)(fade_gain_table[idx] * (1.0 - frac) + fade_gain_table[idx + 1] * frac);
}

static struct xp_audio_stream *
stream_from_handle(xp_audio_handle_t h)
{
	struct xp_audio_stream *s;

	if (h < 0 || h >= XP_AUDIO_MAX_STREAMS)
		return NULL;
	assert_pthread_mutex_lock(&mixer_lock);
	s = mixer_streams[h];
	assert_pthread_mutex_unlock(&mixer_lock);
	return s;
}

/* Called by backends (callbacks directly, or push-backend device threads).
 * Fills `out` with `frames` frames of mixed S16 stereo at 44100 Hz. Silence
 * when no streams are active. Holds `mixer_lock` for the duration to serialize
 * with xp_audio_open/close/reap — this is the simplest correct way to make
 * stream freeing safe during concurrent pulls. Contention is minimal because
 * the only lock holders are open/close/reap (infrequent, short) and the pull
 * itself (short; ~1 ms per call).
 *
 * Returns the number of frames at the start of `out` that were populated from
 * at least one stream (the rest are silence padding from the initial memset).
 * Push-backend device threads use this to write only the "real data" prefix,
 * so trailing silence padding at the end of a note doesn't get queued into
 * the device between one note and the next. Callback backends fill the full
 * buffer regardless — the return value is not useful there. */
size_t
xp_mixer_pull(int16_t *out, size_t frames)
{
	int    i;
	size_t j;
	size_t max_n = 0;

	memset(out, 0, frames * S_FRAMESIZE);

	assert_pthread_mutex_lock(&mixer_lock);
	for (i = 0; i < XP_AUDIO_MAX_STREAMS; i++) {
		struct xp_audio_stream *s = mixer_streams[i];
		size_t  wp;
		size_t  rp;
		size_t  avail;
		size_t  n;
		float   vl;
		float   vr;
		bool    loop;
		bool    finished;

		if (!s)
			continue;
		assert_pthread_mutex_lock(&s->mutex);
		if (s->done) {
			assert_pthread_mutex_unlock(&s->mutex);
			continue;
		}
		wp       = s->write_pos;
		rp       = s->read_pos;
		vl       = s->volume_l;
		vr       = s->volume_r;
		loop     = s->loop;
		finished = s->finished;
		avail    = wp - rp;

		if (loop) {
			if (wp == 0) {
				assert_pthread_mutex_unlock(&s->mutex);
				continue;
			}
			n = frames;
		}
		else {
			n = (frames < avail) ? frames : avail;
		}

		if (n > max_n)
			max_n = n;

		for (j = 0; j < n; j++) {
			size_t  absolute = rp + j;
			size_t  idx;
			int32_t l;
			int32_t r;
			int32_t cur_l;
			int32_t cur_r;

			if (loop)
				absolute %= wp;
			idx = absolute % s->ring_frames;
			l = (int32_t)(s->ring[idx * 2 + 0] * vl);
			r = (int32_t)(s->ring[idx * 2 + 1] * vr);
			cur_l = (int32_t)out[j * 2 + 0] + l;
			cur_r = (int32_t)out[j * 2 + 1] + r;
			if (cur_l > 32767)       cur_l = 32767;
			else if (cur_l < -32768) cur_l = -32768;
			if (cur_r > 32767)       cur_r = 32767;
			else if (cur_r < -32768) cur_r = -32768;
			out[j * 2 + 0] = (int16_t)cur_l;
			out[j * 2 + 1] = (int16_t)cur_r;
		}

		if (!loop) {
			s->read_pos = rp + n;
			if (finished && s->read_pos >= s->write_pos) {
				s->done = true;
				sem_post(&s->drain_sem);
			}
		}
		SetEvent(s->progress);
		assert_pthread_mutex_unlock(&s->mutex);
	}
	assert_pthread_mutex_unlock(&mixer_lock);
	return max_n;
}

/* Release a stream's resources. Must be called with mixer_lock held and the
 * stream already removed from mixer_streams. */
static void
free_stream_locked(struct xp_audio_stream *s)
{
	sem_destroy(&s->drain_sem);
	CloseEvent(s->progress);
	pthread_mutex_destroy(&s->mutex);
	free(s->ring);
	free(s);
}

xp_audio_handle_t
xp_audio_open(float volume_l, float volume_r)
{
	struct xp_audio_stream *s;
	xp_audio_handle_t       handle = -1;
	int                     i;

	pthread_once(&mixer_once, mixer_init);

	s = calloc(1, sizeof(*s));
	if (!s)
		return -1;
	s->ring_frames = XP_AUDIO_DEFAULT_RING_FRAMES;
	s->ring = calloc(s->ring_frames * S_CHANNELS, sizeof(int16_t));
	if (!s->ring) {
		free(s);
		return -1;
	}
	s->volume_l = volume_l;
	s->volume_r = volume_r;
	sem_init(&s->drain_sem, 0, 0);
	/* Auto-reset, initially non-signaled. The mixer SetEvents after each
	 * read_pos advance; one pending waiter wakes and re-checks predicate. */
	s->progress = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (s->progress == NULL) {
		sem_destroy(&s->drain_sem);
		free(s->ring);
		free(s);
		return -1;
	}
	assert_pthread_mutex_init(&s->mutex, NULL);

	assert_pthread_mutex_lock(&mixer_lock);
	/* Reap any done+auto_close streams first to free slots. Safe under
	 * mixer_lock — no mixer pull is in progress. */
	for (i = 0; i < XP_AUDIO_MAX_STREAMS; i++) {
		struct xp_audio_stream *r = mixer_streams[i];
		if (r && r->auto_close && r->done) {
			mixer_streams[i] = NULL;
			free_stream_locked(r);
		}
	}
	for (i = 0; i < XP_AUDIO_MAX_STREAMS; i++) {
		if (!mixer_streams[i]) {
			mixer_streams[i] = s;
			handle = i;
			break;
		}
	}
	assert_pthread_mutex_unlock(&mixer_lock);

	if (handle < 0)
		free_stream_locked(s);
	return handle;
}

void
xp_audio_close(xp_audio_handle_t h)
{
	struct xp_audio_stream *s;

	if (h < 0 || h >= XP_AUDIO_MAX_STREAMS)
		return;
	assert_pthread_mutex_lock(&mixer_lock);
	s = mixer_streams[h];
	mixer_streams[h] = NULL;
	if (s) {
		/* Wake any producer blocked on progress, then free under
		 * mixer_lock — we know no mixer pull is in progress because it
		 * holds mixer_lock for its duration. */
		assert_pthread_mutex_lock(&s->mutex);
		s->done = true;
		SetEvent(s->progress);
		assert_pthread_mutex_unlock(&s->mutex);
		free_stream_locked(s);
	}
	assert_pthread_mutex_unlock(&mixer_lock);
}

bool
xp_audio_append(xp_audio_handle_t h, const int16_t *frames, size_t nframes)
{
	struct xp_audio_stream *s = stream_from_handle(h);
	size_t written = 0;

	if (!s || !frames)
		return false;

	assert_pthread_mutex_lock(&s->mutex);
	if (s->finished || s->done) {
		assert_pthread_mutex_unlock(&s->mutex);
		return false;
	}

	while (written < nframes) {
		size_t in_ring = s->write_pos - s->read_pos;
		size_t free_sp;
		size_t n;
		size_t j;

		while (in_ring >= s->ring_frames) {
			if (s->done) {
				assert_pthread_mutex_unlock(&s->mutex);
				return false;
			}
			assert_pthread_mutex_unlock(&s->mutex);
			WaitForEvent(s->progress, XP_AUDIO_POLL_MS);
			assert_pthread_mutex_lock(&s->mutex);
			in_ring = s->write_pos - s->read_pos;
		}
		free_sp = s->ring_frames - in_ring;
		n = nframes - written;
		if (n > free_sp)
			n = free_sp;
		for (j = 0; j < n; j++) {
			size_t idx = (s->write_pos + j) % s->ring_frames;
			s->ring[idx * 2 + 0] = frames[(written + j) * 2 + 0];
			s->ring[idx * 2 + 1] = frames[(written + j) * 2 + 1];
		}
		s->write_pos += n;
		written += n;
	}
	assert_pthread_mutex_unlock(&s->mutex);
	return true;
}

bool
xp_audio_append_faded(xp_audio_handle_t h, const int16_t *frames, size_t nframes,
                      const xp_audio_fade_t *fade)
{
	struct xp_audio_stream *s;
	int16_t                *shaped;
	size_t                  fi;
	size_t                  fo;
	size_t                  i;
	size_t                  written;
	bool                    do_crossfade;

	if (!fade || (fade->fade_in_frames == 0 && fade->fade_out_frames == 0 && !fade->crossfade))
		return xp_audio_append(h, frames, nframes);

	s = stream_from_handle(h);
	if (!s || !frames)
		return false;

	fi = fade->fade_in_frames;
	fo = fade->fade_out_frames;
	/* Clamp so fi+fo ≤ nframes */
	if (fi + fo > nframes) {
		if (fi + fo == 0) {
			fi = fo = 0;
		}
		else {
			size_t new_fi = (size_t)((uint64_t)nframes * fi / (fi + fo));
			fo = nframes - new_fi;
			fi = new_fi;
		}
	}

	shaped = malloc(nframes * S_FRAMESIZE);
	if (!shaped)
		return false;
	for (i = 0; i < nframes; i++) {
		int16_t gain = 32767;
		int32_t l;
		int32_t r;

		if (i < fi) {
			gain = fade_gain_q15(i, fi);
		}
		if (fo > 0 && i + fo >= nframes) {
			int16_t og = fade_gain_q15(nframes - 1 - i, fo);
			if (og < gain)
				gain = og;
		}
		l = ((int32_t)frames[i * 2 + 0] * gain) >> 15;
		r = ((int32_t)frames[i * 2 + 1] * gain) >> 15;
		shaped[i * 2 + 0] = (int16_t)l;
		shaped[i * 2 + 1] = (int16_t)r;
	}

	assert_pthread_mutex_lock(&s->mutex);
	if (s->finished || s->done) {
		assert_pthread_mutex_unlock(&s->mutex);
		free(shaped);
		return false;
	}

	/* Crossfade path: rewind write_pos by fi and sum-write the first fi
	 * frames over the existing (faded-out) tail. Only possible if the
	 * mixer hasn't already consumed that region. */
	do_crossfade = (fade->crossfade && fi > 0 && s->write_pos >= fi
	                && s->write_pos - fi >= s->read_pos);
	if (do_crossfade) {
		s->write_pos -= fi;
		for (i = 0; i < fi; i++) {
			size_t  idx = (s->write_pos + i) % s->ring_frames;
			int32_t l = (int32_t)s->ring[idx * 2 + 0] + shaped[i * 2 + 0];
			int32_t r = (int32_t)s->ring[idx * 2 + 1] + shaped[i * 2 + 1];
			if (l > 32767)       l = 32767;
			else if (l < -32768) l = -32768;
			if (r > 32767)       r = 32767;
			else if (r < -32768) r = -32768;
			s->ring[idx * 2 + 0] = (int16_t)l;
			s->ring[idx * 2 + 1] = (int16_t)r;
		}
		s->write_pos += fi;
	}

	/* Remaining (post-crossfade) frames: overwrite write. Reuse append
	 * semantics for buffer-full handling. */
	written = do_crossfade ? fi : 0;
	while (written < nframes) {
		size_t in_ring = s->write_pos - s->read_pos;
		size_t free_sp;
		size_t n;
		size_t j;

		while (in_ring >= s->ring_frames) {
			if (s->done) {
				assert_pthread_mutex_unlock(&s->mutex);
				free(shaped);
				return false;
			}
			assert_pthread_mutex_unlock(&s->mutex);
			WaitForEvent(s->progress, XP_AUDIO_POLL_MS);
			assert_pthread_mutex_lock(&s->mutex);
			in_ring = s->write_pos - s->read_pos;
		}
		free_sp = s->ring_frames - in_ring;
		n = nframes - written;
		if (n > free_sp)
			n = free_sp;
		for (j = 0; j < n; j++) {
			size_t idx = (s->write_pos + j) % s->ring_frames;
			s->ring[idx * 2 + 0] = shaped[(written + j) * 2 + 0];
			s->ring[idx * 2 + 1] = shaped[(written + j) * 2 + 1];
		}
		s->write_pos += n;
		written += n;
	}
	assert_pthread_mutex_unlock(&s->mutex);
	free(shaped);
	return true;
}

xp_audio_handle_t
xp_audio_play(const int16_t *frames, size_t nframes, float volume_l, float volume_r, bool loop)
{
	xp_audio_handle_t h = xp_audio_open(volume_l, volume_r);
	struct xp_audio_stream *s;

	if (h < 0)
		return -1;
	s = stream_from_handle(h);
	/* Resize ring to fit exactly if larger than default. Loop mode requires
	 * the ring to hold the full sample (read wraps to write_pos). */
	if (nframes > s->ring_frames) {
		int16_t *newring;
		assert_pthread_mutex_lock(&s->mutex);
		newring = realloc(s->ring, nframes * S_CHANNELS * sizeof(int16_t));
		if (!newring) {
			assert_pthread_mutex_unlock(&s->mutex);
			xp_audio_close(h);
			return -1;
		}
		s->ring        = newring;
		s->ring_frames = nframes;
		assert_pthread_mutex_unlock(&s->mutex);
	}
	if (loop) {
		assert_pthread_mutex_lock(&s->mutex);
		s->loop = true;
		assert_pthread_mutex_unlock(&s->mutex);
	}
	if (!xp_audio_append(h, frames, nframes)) {
		xp_audio_close(h);
		return -1;
	}
	if (!loop)
		xp_audio_finish(h);
	return h;
}

void
xp_audio_set_volume(xp_audio_handle_t h, float volume_l, float volume_r)
{
	struct xp_audio_stream *s = stream_from_handle(h);

	if (!s)
		return;
	assert_pthread_mutex_lock(&s->mutex);
	s->volume_l = volume_l;
	s->volume_r = volume_r;
	assert_pthread_mutex_unlock(&s->mutex);
}

void
xp_audio_finish(xp_audio_handle_t h)
{
	struct xp_audio_stream *s = stream_from_handle(h);

	if (!s)
		return;
	assert_pthread_mutex_lock(&s->mutex);
	s->finished = true;
	if (!s->loop && s->read_pos >= s->write_pos && !s->done) {
		s->done = true;
		sem_post(&s->drain_sem);
	}
	assert_pthread_mutex_unlock(&s->mutex);
}

void
xp_audio_stop(xp_audio_handle_t h)
{
	struct xp_audio_stream *s = stream_from_handle(h);

	if (!s)
		return;
	assert_pthread_mutex_lock(&s->mutex);
	s->read_pos  = s->write_pos;
	s->loop      = false;
	s->finished  = true;
	if (!s->done) {
		s->done = true;
		sem_post(&s->drain_sem);
	}
	SetEvent(s->progress);
	assert_pthread_mutex_unlock(&s->mutex);
}

/* Returns the current device-buffer latency in milliseconds. When the mixer
 * has drained our data (read_pos caught write_pos), the device kernel buffer
 * may still hold that data, so callers must wait this long for the hardware
 * to actually play it.
 *
 * Reads handle_type and the backend fd/handle without acquiring handle_mutex.
 * Races with xptone_close are benign: the query will fail and we fall through
 * to the fallback estimate. Taking handle_mutex here would deadlock callers
 * like xptone_complete_locked that hold it already. */
static uint32_t
xp_device_latency_ms(void)
{
	switch (handle_type) {
#ifdef AFMT_S16_NE
		case SOUND_DEVICE_OSS: {
			int bytes = 0;
			if (ioctl(dsp, SNDCTL_DSP_GETODELAY, &bytes) == 0 && bytes > 0)
				return (uint32_t)((uint64_t)bytes * 1000 /
				                  ((uint64_t)S_RATE * S_FRAMESIZE));
			return 50;
		}
#endif
#ifdef USE_ALSA_SOUND
		case SOUND_DEVICE_ALSA: {
			snd_pcm_sframes_t delay = 0;
			if (alsa_api && alsa_api->snd_pcm_delay && playback_handle
			    && alsa_api->snd_pcm_delay(playback_handle, &delay) == 0
			    && delay > 0)
				return (uint32_t)((uint64_t)delay * 1000 / S_RATE);
			return 50;
		}
#endif
#ifdef WITH_PULSEAUDIO
		case SOUND_DEVICE_PULSEAUDIO: {
			pa_usec_t us = 0;
			int       neg = 0;
			if (pu_api && pu_api->stream_get_latency && pu_stream && pu_mainloop) {
				pu_api->threaded_mainloop_lock(pu_mainloop);
				if (pu_api->stream_get_latency(pu_stream, &us, &neg) == 0 && !neg) {
					pu_api->threaded_mainloop_unlock(pu_mainloop);
					return (uint32_t)(us / 1000);
				}
				pu_api->threaded_mainloop_unlock(pu_mainloop);
			}
			return 50;
		}
#endif
#ifdef WITH_PORTAUDIO
		case SOUND_DEVICE_PORTAUDIO: {
			const PaStreamInfo *info;
			if (pa_api && pa_api->get_stream_info && portaudio_stream
			    && (info = pa_api->get_stream_info(portaudio_stream)) != NULL
			    && info->outputLatency > 0.0)
				return (uint32_t)(info->outputLatency * 1000.0);
			return 50;
		}
#endif
#ifdef _WIN32
		case SOUND_DEVICE_WIN32: {
			/* waveOutGetPosition returns the driver's running play position
			 * in the stream timeline. Queued-but-unplayed frames = what we
			 * have submitted minus what has played. */
			MMTIME mmt;
			mmt.wType = TIME_SAMPLES;
			if (waveOutGetPosition(waveOut, &mmt, sizeof(mmt)) == MMSYSERR_NOERROR
			    && mmt.wType == TIME_SAMPLES) {
				uint64_t played  = mmt.u.sample;
				uint64_t written = waveOut_frames_written;
				if (written > played)
					return (uint32_t)((written - played) * 1000 / S_RATE);
				return 0;
			}
			return 50;
		}
#endif
#ifdef WITH_COREAUDIO
		case SOUND_DEVICE_COREAUDIO: {
			/* AudioQueueGetCurrentTime returns a timestamp in the queue's
			 * own sample timeline (starts at 0). Before the queue has
			 * actually started playing, mSampleTime may be negative or
			 * unset — treat those as "nothing played yet". */
			AudioTimeStamp ts;
			if (ca_queue
			    && AudioQueueGetCurrentTime(ca_queue, NULL, &ts, NULL) == noErr
			    && (ts.mFlags & kAudioTimeStampSampleTimeValid)) {
				int64_t played = (int64_t)ts.mSampleTime;
				if (played < 0)
					played = 0;
				if ((int64_t)ca_frames_enqueued > played)
					return (uint32_t)((ca_frames_enqueued - played) * 1000 / S_RATE);
				return 0;
			}
			/* Fallback: all buffers are always in flight under the
			 * pull-native callback, so total queue depth is a safe
			 * upper bound. */
			return (uint32_t)(CA_NUM_BUFS * (CA_BUF_SIZE / S_FRAMESIZE) * 1000 / S_RATE);
		}
#endif
		default:
			/* Conservative upper-bound for backends without a latency
			 * query wired up yet (SDL). */
			if (handle_type != SOUND_DEVICE_CLOSED)
				return 50;
			return 0;
	}
}

void
xp_audio_wait(xp_audio_handle_t h)
{
	struct xp_audio_stream *s = stream_from_handle(h);
	size_t target;

	if (!s)
		return;
	/* Mixer-level sync: wait for all frames appended up to this call to be
	 * consumed by the mixer, or for the stream to be stopped / finished.
	 * The device kernel buffer may still contain a tail of those frames
	 * that has yet to physically play — callers who need to wait for that
	 * to reach the speaker should additionally call xp_audio_drain (which
	 * sleeps for the current device buffer latency). Between back-to-back
	 * appends in the same logical sequence (e.g. MF notes in MML), mixer
	 * sync is sufficient and device drain would insert audible gaps. */
	assert_pthread_mutex_lock(&s->mutex);
	target = s->write_pos;
	while (s->read_pos < target && !s->done) {
		assert_pthread_mutex_unlock(&s->mutex);
		WaitForEvent(s->progress, XP_AUDIO_POLL_MS);
		assert_pthread_mutex_lock(&s->mutex);
	}
	assert_pthread_mutex_unlock(&s->mutex);
}

/* Wait for the stream's data to reach the speaker: mixer sync first, then
 * sleep for the device kernel buffer's current latency. Intended for the
 * final "I'm done, don't continue until it's been heard" signal — not
 * between successive appends (that would insert gaps). */
void
xp_audio_drain(xp_audio_handle_t h)
{
	uint32_t extra_ms;

	xp_audio_wait(h);
	extra_ms = xp_device_latency_ms();
	if (extra_ms > 0)
		SLEEP(extra_ms);
}

#endif /* XPDEV_THREAD_SAFE */

/*
 * Convert U8 mono 22050 Hz data into a newly-allocated S16 stereo 44100 Hz
 * frame buffer. 2:1 linear-interpolation upsample, L==R. Returns the new
 * buffer (caller takes ownership) and writes the frame count to `*nframes_out`.
 */
static int16_t *
convert_u8mono22k_to_s16stereo44k(const unsigned char *in, size_t in_bytes, size_t *nframes_out)
{
	size_t   out_frames = in_bytes * 2;
	int16_t *out = malloc(out_frames * S_FRAMESIZE);
	size_t   i;

	if (!out)
		return NULL;
	for (i = 0; i < in_bytes; i++) {
		int16_t s      = (int16_t)(((int)in[i] - 128) << 8);
		int16_t s_next = (i + 1 < in_bytes)
		                   ? (int16_t)(((int)in[i + 1] - 128) << 8)
		                   : s;
		int16_t mid = (int16_t)(((int32_t)s + (int32_t)s_next) / 2);
		out[i * 4 + 0] = s;    /* frame 2i   L */
		out[i * 4 + 1] = s;    /* frame 2i   R */
		out[i * 4 + 2] = mid;  /* frame 2i+1 L */
		out[i * 4 + 3] = mid;  /* frame 2i+1 R */
	}
	*nframes_out = out_frames;
	return out;
}

#ifdef XPDEV_THREAD_SAFE
static void
init_sample(void)
{
	assert_pthread_mutex_init(&handle_mutex, NULL);
}

/*
 * Returns true for backends that need a dedicated device thread to pump
 * mixed audio out (the device is blocking/push). Pull-native backends
 * (SDL, CoreAudio, PortAudio) drive playback from their own callback.
 */
static bool
backend_is_push(int h)
{
	switch (h) {
#ifdef _WIN32
		case SOUND_DEVICE_WIN32:      return true;
#endif
#ifdef USE_ALSA_SOUND
		case SOUND_DEVICE_ALSA:       return true;
#endif
#ifdef AFMT_S16_NE
		case SOUND_DEVICE_OSS:        return true;
#endif
		default:                      return false;
	}
}

/*
 * Device thread: pulls S16 stereo frames from the mixer and writes them
 * to the currently-open push-backend device. Exits cleanly when asked.
 * The device fd/handle is guaranteed to remain valid while the thread
 * runs — xptone_close_locked joins this thread before tearing the device
 * down.
 */
static void
device_thread_fn(void *arg)
{
	int16_t buf[XP_AUDIO_DEVICE_CHUNK * S_CHANNELS];

	(void)arg;
	SetThreadName("xp_audio_dev");

	while (!device_thread_should_exit) {
		int    h = handle_type;
		size_t real_frames;

		real_frames = xp_mixer_pull(buf, XP_AUDIO_DEVICE_CHUNK);

		/* Two cases where we don't write:
		 *   1. Startup: no data yet; queueing silence would push the
		 *      first note behind it in the kernel buffer.
		 *   2. Mid-stream idle: stream ran out momentarily and nothing
		 *      else to mix. Writing the full silence chunk would land
		 *      between one note and the next, creating an audible gap.
		 *
		 * Instead we sleep briefly and poll. The kernel buffer drains
		 * naturally; as soon as the producer appends more data, the
		 * next pull returns real_frames > 0 and we write it into an
		 * empty / low kernel buffer — no silence padding between notes. */
		if (real_frames == 0) {
			SLEEP(1);
			continue;
		}

		switch (h) {
#ifdef _WIN32
			case SOUND_DEVICE_WIN32: {
				/* All WIN32_NBUFS WAVEHDRs were malloc+prepared at open
				 * time. We cycle through them round-robin: refill lpData,
				 * submit via waveOutWrite, advance. After wrapping the
				 * ring, the "next" buffer is likely still in the driver's
				 * queue (WHDR_INQUEUE set) — wait on waveOut_done for
				 * it to complete before overwriting.
				 *
				 * Trailing silence padding (buf[real_frames..] is zero
				 * from the mixer's initial memset) plays as negligible
				 * inter-note silence but keeps the driver happy with
				 * fixed-size chunks that match the prepared buffer
				 * length. */
				size_t bytes = XP_AUDIO_DEVICE_CHUNK * S_FRAMESIZE;
				while (wh[curr_wh].dwFlags & WHDR_INQUEUE) {
					if (device_thread_should_exit)
						break;
					WaitForSingleObject(waveOut_done, 100);
				}
				if (device_thread_should_exit)
					break;
				memcpy(wh[curr_wh].lpData, buf, bytes);
				wh[curr_wh].dwBufferLength = bytes;
				if (waveOutWrite(waveOut, &wh[curr_wh], sizeof(wh[curr_wh])) == MMSYSERR_NOERROR) {
					waveOut_frames_written += XP_AUDIO_DEVICE_CHUNK;
					curr_wh = (curr_wh + 1) % WIN32_NBUFS;
				}
				break;
			}
#endif
#ifdef USE_ALSA_SOUND
			case SOUND_DEVICE_ALSA: {
				snd_pcm_sframes_t rc = alsa_api->snd_pcm_writei(playback_handle, buf, real_frames);
				if (rc < 0)
					alsa_api->snd_pcm_prepare(playback_handle);
				break;
			}
#endif
#ifdef AFMT_S16_NE
			case SOUND_DEVICE_OSS: {
				ssize_t rc = write(dsp, buf, (size_t)real_frames * S_FRAMESIZE);
				(void)rc;
				break;
			}
#endif
			default:
				SLEEP(10);
				break;
		}
	}

	/* Buffer teardown (unprepare + free) happens in xptone_close_locked
	 * where it's sequenced with waveOutClose. The device thread just
	 * stops feeding. */
	device_thread_running = false;
	sem_post(&device_thread_stopped);
}

/* Start the device thread if the current backend is push-based. Idempotent. */
static bool
start_device_thread_locked(void)
{
	if (!backend_is_push(handle_type))
		return true;
	if (device_thread_running)
		return true;
	device_thread_should_exit = false;
	sem_init(&device_thread_stopped, 0, 0);
	device_thread_running = true;
	/* _beginthread is the cross-platform thread-start primitive (threadwrap
	 * supplies it on Windows via native threads and on POSIX via pthread).
	 * We synchronize shutdown via device_thread_stopped — no explicit join
	 * is needed or portably available. */
	_beginthread(device_thread_fn, 0, NULL);
	return true;
}

/* Stop the device thread (if running). Idempotent. */
static void
stop_device_thread_locked(void)
{
	if (!device_thread_running)
		return;
	device_thread_should_exit = true;
	/* handle_mutex is held by the caller (xptone_close_locked). The device
	 * thread doesn't take handle_mutex, so no deadlock. The thread posts
	 * device_thread_stopped once it has drained and cleared its running
	 * flag, so sem_wait is sufficient without an explicit pthread_join. */
	sem_wait(&device_thread_stopped);
	sem_destroy(&device_thread_stopped);
}

/* Post-open hook: for PortAudio, start the stream so the callback starts
 * being invoked; for push backends, spawn the device thread. Idempotent. */
static bool
post_open_setup_locked(void)
{
#ifdef WITH_PORTAUDIO
	if (handle_type == SOUND_DEVICE_PORTAUDIO) {
		if (pa_api->active(portaudio_stream) != 1) {
			if (pa_api->start(portaudio_stream) != paNoError)
				return false;
		}
		return true;
	}
#endif
	return start_device_thread_locked();
}

bool xp_play_sample16s(const int16_t *frames, size_t nframes, bool background)
{
	xp_audio_handle_t       h;
	struct xp_audio_stream *s;
	bool                    must_close = false;

	pthread_once(&sample_initialized_pto, init_sample);

	assert_pthread_mutex_lock(&handle_mutex);
	if (handle_type == SOUND_DEVICE_CLOSED) {
		must_close = true;
		if (!xptone_open_locked()) {
			assert_pthread_mutex_unlock(&handle_mutex);
			return false;
		}
	}
	(void)post_open_setup_locked();
	assert_pthread_mutex_unlock(&handle_mutex);

	h = xp_audio_play(frames, nframes, 1.0f, 1.0f, false);
	if (h < 0) {
		if (must_close)
			xptone_close();
		return false;
	}

	if (!background) {
		xp_audio_wait(h);
		xp_audio_close(h);
		if (must_close)
			xptone_close();
	}
	else {
		/* Let the stream auto-reap when a later xp_audio_open needs a slot. */
		s = stream_from_handle(h);
		if (s) {
			assert_pthread_mutex_lock(&s->mutex);
			s->auto_close = true;
			assert_pthread_mutex_unlock(&s->mutex);
		}
		/* If we opened the device, keep it open for the background stream
		 * to drain; reference will be released on the next xptone_close. */
	}
	return true;
}

bool xp_play_sample(unsigned char *sample, size_t size, bool background)
{
	int16_t *converted;
	size_t   nframes;
	bool     ret;

	converted = convert_u8mono22k_to_s16stereo44k(sample, size, &nframes);
	if (!converted)
		return false;
	ret = xp_play_sample16s(converted, nframes, background);
	free(converted);
	return ret;
}
#else
bool xp_play_sample16s(const int16_t *frames, size_t nframes, bool background)
{
	bool must_close = false;
	bool ret;

	(void)background;
	if (handle_type == SOUND_DEVICE_CLOSED) {
		must_close = true;
		if (!xptone_open())
			return false;
	}

	ret = do_xp_play_sample((unsigned char *)frames, nframes * S_FRAMESIZE, NULL);
	if (must_close)
		xptone_close();
	return ret;
}

bool xp_play_sample(unsigned char *sample, size_t sample_size, bool background)
{
	int16_t *converted;
	size_t   nframes;
	bool     ret;

	converted = convert_u8mono22k_to_s16stereo44k(sample, sample_size, &nframes);
	if (!converted)
		return false;

	ret = xp_play_sample16s(converted, nframes, background);
	free(converted);
	return ret;
}
#endif

/********************************************************************************/
/* Play a tone through the wave/DSP output device (sound card) - Deuce			*/
/********************************************************************************/

bool xptone(double freq, DWORD duration, DWORD shape)
{
	int16_t *wave;
	int      samples;
	bool     ret;

	/* Max 7.5 seconds of S16 stereo. `samples` is in frames. */
	wave = (int16_t *)malloc((size_t)(S_RATE * 15 / 2) * S_FRAMESIZE + S_FRAMESIZE);
	if (!wave)
		return false;
	if (freq < 17 && freq != 0)
		freq = 17;
	samples = S_RATE * duration / 1000;
	if ((shape & WAVE_SHAPE_NO_CLEAN) == 0) {
		if (freq) {
			if (samples <= S_RATE / freq * 2)
				samples = S_RATE / freq * 2;
		}
	}
	if (freq == 0 || samples > S_RATE / freq * 2) {
		int sample_len;

		xptone_makewave(freq, wave, S_RATE * 15 / 2, shape);
		/* Trailing-silence scan — silence = 0 in S16. Check the L channel of each
		 * frame (R == L by construction). */
		for (sample_len = S_RATE * 15 / 2 - 1; sample_len && wave[sample_len * S_CHANNELS] == 0; sample_len--)
			;
		sample_len++;
		while (samples > S_RATE * 15 / 2) {
			if (!xp_play_sample16s(wave, sample_len, true)) {
				free(wave);
				return false;
			}
			samples -= sample_len;
		}
	}
	xptone_makewave(freq, wave, samples, shape);
	ret = xp_play_sample16s(wave, samples, false);
	free(wave);
	return ret;
}

#ifdef __unix__
/****************************************************************************/
/* Generate a tone at specified frequency for specified milliseconds		*/
/* Thanks to Casey Martin (and Deuce) for this code							*/
/****************************************************************************/
void unix_beep(int freq, int dur)
{
#if (defined(__FreeBSD__) && defined(HAS_MACHINE_SPEAKER_H)) || ((defined(__OpenBSD__) || defined(__NetBSD__)) && defined(HAS_MACHINE_SPKR_H))
	int    speaker_fd = -1;
	tone_t tone;

	speaker_fd = open("/dev/speaker", O_WRONLY | O_APPEND);
	if (speaker_fd != -1)  {
		tone.frequency = freq;
		tone.duration = dur / 10;
		ioctl(speaker_fd, SPKRTONE, &tone);
		close(speaker_fd);
		return;
	}
#endif

#if defined(__FreeBSD__) || defined(__solaris__)
	static int console_fd = -1;

	if (console_fd == -1)
		console_fd = open("/dev/console", O_NOCTTY);

	if (console_fd != -1) {
#if defined(__solaris__)
		ioctl(console_fd, KIOCCMD, KBD_CMD_BELL);
#else
		if (freq != 0)   /* Don't divide by zero */
			ioctl(console_fd, KIOCSOUND, (int) (1193180 / freq));
#endif /* solaris */
		SLEEP(dur);
#if defined(__solaris__)
		ioctl(console_fd, KIOCCMD, KBD_CMD_NOBELL); /* turn off tone */
#else
		ioctl(console_fd, KIOCSOUND, 0);    /* turn off tone */
#endif /* solaris */
	}
#endif
}

#endif

/********************************************************************************/
/* Play sound through DSP/wave device, if unsuccessful, play through PC speaker	*/
/********************************************************************************/
void xpbeep(double freq, DWORD duration)
{
	if (xptone(freq, duration, WAVE_SHAPE_SINE_SAW_HARM))
		return;

#if defined(_WIN32)
	Beep((DWORD)(freq + .5), duration);
#elif defined(__unix__)
	unix_beep((int)(freq + .5), duration);
#endif
}
