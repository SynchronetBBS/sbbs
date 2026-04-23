/* TODO: USE PORTAUDIO! */

/* MinGW's <math.h> hides M_PI et al. unless _USE_MATH_DEFINES is set
 * before it's first included.  The macro lives in the implementation-
 * reserved namespace, so only opt in on the platform that needs it. */
#if defined(__MINGW32__) || defined(__MINGW64__)
#define _USE_MATH_DEFINES
#endif

/* standard headers */
#include <math.h>
#include <stdlib.h>
#include "xp_dl.h"

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	/* MT builds with a modern compiler (MSVC, MinGW-w64) use WASAPI
	 * (Vista+) for audio output. Non-MT builds and Borland (whose 2002
	 * Platform SDK predates Vista) stay on waveOut. Set the target
	 * version BEFORE <windows.h> so IAudioClient et al. are exposed
	 * later in this file. */
	#if defined(XPDEV_THREAD_SAFE) && !defined(__BORLANDC__)
		#undef _WIN32_WINNT
		#define _WIN32_WINNT 0x0600
		#undef WINVER
		#define WINVER 0x0600
	#endif
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

/* Device thread: for push-based backends (ALSA, OSS) a dedicated thread
 * loops xp_mixer_pull → device_write. Pull-native backends (SDL,
 * CoreAudio, PortAudio, PulseAudio async, Win32 WASAPI) don't use this
 * thread — their own callbacks (or the WASAPI render thread owned by
 * the xp_wasapi_* block below) call xp_mixer_pull directly. */
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
#if defined(XPDEV_THREAD_SAFE) && !defined(__BORLANDC__)
/* MT builds with Vista+ SDK headers use WASAPI (pull-native shared-mode,
 * event-driven) instead of waveOut. The wrapper owns its own render
 * thread that pulls from xp_mixer_pull and writes into the WASAPI
 * buffer. Borland's 2002 Platform SDK predates Vista (no mmdeviceapi.h),
 * so Borland MT falls through to the 'neither' case at the bottom — it
 * has no Win32 audio backend. Non-MT builds keep the waveOut state for
 * the synchronous do_xp_play_sample code path. */
#define COBJMACROS                 /* IFoo_Method(this, args) macro form */
#define INITGUID                   /* emit storage for referenced GUIDs */
/* <initguid.h> must come AFTER #define INITGUID to redefine DEFINE_GUID
 * into the storage-emitting form. <windows.h> at the top of this file
 * already pulled in <objbase.h> without INITGUID set, so initguid.h
 * wasn't included then — do it explicitly now so the CLSID_ / IID_
 * GUIDs we reference get locally-defined storage. */
#include <initguid.h>
#include <mmreg.h>                 /* DEFINE_WAVEFORMATEX_GUID template */
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>                  /* MMCSS (runtime-loaded) */

/* KSDATAFORMAT_SUBTYPE_PCM is declared in the SDK via DEFINE_GUIDSTRUCT /
 * DEFINE_GUIDNAMED. In C mode that expands to a bare symbol reference
 * (not a struct-with-uuid attribute like C++). MinGW-w64's libuuid.a
 * doesn't ship the storage for this particular KS GUID, so we emit our
 * own using the standard SDK STATIC_ brace-init template — this works
 * identically on MinGW-w64 and MSVC. */
static const GUID xp_wasapi_ks_subtype_pcm = { STATIC_KSDATAFORMAT_SUBTYPE_PCM };

/* COM function-import libs (ole32 for CoInitializeEx / CoCreateInstance /
 * CoUninitialize) are declared alongside the other xpdev-MT Win32 libs
 * in xpdev_mt.props (MSVC) and xpdev/Common.gmake (MinGW).
 *
 * GUID storage varies by toolchain:
 *   - MinGW-w64's <mmdeviceapi.h> and <audioclient.h> already call
 *     DEFINE_GUID(...) for these four GUIDs in the header itself, so
 *     INITGUID (set above before <initguid.h>) causes storage to be
 *     emitted in THIS translation unit automatically — no local
 *     DEFINE_GUID needed.
 *   - MSVC's Windows SDK headers only declare `EXTERN_C const IID X;`
 *     (storage expected from uuid.lib). In C mode, modern MSVC's
 *     uuid.lib doesn't actually ship these WASAPI GUIDs — the MS
 *     samples compile as C++ where __uuidof handles it at compile
 *     time, so the C-mode gap goes untested. Define them ourselves
 *     under the INITGUID-active DEFINE_GUID, which emits
 *     DECLSPEC_SELECTANY COMDAT storage.
 * See the handmade.network WASAPI-in-C thread for the definitive
 * discussion of this MSVC-specific gap. */
#ifdef _MSC_VER
DEFINE_GUID(CLSID_MMDeviceEnumerator,
    0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator,
    0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioClient,
    0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
DEFINE_GUID(IID_IAudioRenderClient,
    0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2);
#endif

#include "threadwrap.h"
#include "semwrap.h"

static IMMDeviceEnumerator *wasapi_enumerator;
static IMMDevice           *wasapi_device;
static IAudioClient        *wasapi_client;
static IAudioRenderClient  *wasapi_render;
static HANDLE               wasapi_event;
static sem_t                wasapi_thread_stopped;
static UINT32               wasapi_buffer_frames;
static bool                 wasapi_com_owned;
static bool                 wasapi_thread_started;
/* This whole block is gated out for Borland (no Vista SDK headers), so
 * the _Atomic keyword is fine unconditionally here. */
static _Atomic bool         wasapi_stop_requested;

/* MMCSS function pointers — runtime-loaded via LoadLibrary so we don't
 * need libavrt at link time (MinGW may not ship it). Failure is non-
 * fatal: the render thread just runs at normal priority. */
typedef HANDLE (WINAPI *pfn_AvSetMmThreadCharacteristicsW)(LPCWSTR, LPDWORD);
typedef BOOL   (WINAPI *pfn_AvRevertMmThreadCharacteristics)(HANDLE);
static HMODULE                              avrt_dll;
static pfn_AvSetMmThreadCharacteristicsW    pAvSetMmThreadCharacteristicsW;
static pfn_AvRevertMmThreadCharacteristics  pAvRevertMmThreadCharacteristics;

static void xp_wasapi_teardown(void);
#else
static HWAVEOUT              waveOut;
/* waveOut playback is a push model — we submit WAVEHDRs and the driver
 * plays them in order. Only used in non-MT builds. */
#define WIN32_NBUFS 4
static WAVEHDR               wh[WIN32_NBUFS];
static int                   curr_wh;
static HANDLE                waveOut_done;
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
static void     xp_audio_mixer_sync(xp_audio_handle_t h);
size_t          xp_mixer_pull(int16_t *out, size_t frames);

/* Mixer state (defined in full later in the file). Hoisted here so
 * xptone_complete_locked can drain streams on completion. */
#define XP_AUDIO_MAX_STREAMS        32
#define XP_AUDIO_DEFAULT_RING_FRAMES (S_RATE)
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
			/* Full-scale U8→S16: the -12 dB headroom reduction that was
			 * historically baked in here is now applied as stream-level
			 * attenuation at mix time (cterm music_stream / fx_stream open
			 * at -12 dB; xptone opens its ephemeral stream at -12 dB). */
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

#if defined(_WIN32) && defined(XPDEV_THREAD_SAFE) && !defined(__BORLANDC__)
/* ============================================================
 * WASAPI backend (shared-mode event-driven, pull-native).
 * Owns its own render thread that pulls from xp_mixer_pull and
 * writes into the WASAPI buffer. Replaces waveOut for MT builds.
 * ============================================================ */

static void
wasapi_thread_fn(void *arg)
{
	HANDLE  mmcss_handle = NULL;
	DWORD   mmcss_task   = 0;
	HRESULT hr;
	UINT32  padding;
	UINT32  avail;
	BYTE   *data;

	(void)arg;

	if (pAvSetMmThreadCharacteristicsW)
		mmcss_handle = pAvSetMmThreadCharacteristicsW(L"Audio", &mmcss_task);

	while (!wasapi_stop_requested) {
		DWORD rc = WaitForSingleObject(wasapi_event, 200);
		if (rc != WAIT_OBJECT_0)
			continue;  /* timeout → re-check stop flag */
		if (wasapi_stop_requested)
			break;

		hr = IAudioClient_GetCurrentPadding(wasapi_client, &padding);
		if (FAILED(hr))
			break;
		if (padding >= wasapi_buffer_frames)
			continue;
		avail = wasapi_buffer_frames - padding;
		if (avail == 0)
			continue;

		data = NULL;
		hr = IAudioRenderClient_GetBuffer(wasapi_render, avail, &data);
		if (FAILED(hr)) {
			/* AUDCLNT_E_DEVICE_INVALIDATED (unplugged headphones,
			 * driver swap, session ended) → backend dies. Auto-
			 * recovery (reopen on endpoint change) is a future
			 * enhancement; for now the user gets no audio until
			 * xptone_close + reopen. */
			break;
		}
		xp_mixer_pull((int16_t *)data, (size_t)avail);
		IAudioRenderClient_ReleaseBuffer(wasapi_render, avail, 0);
	}

	if (mmcss_handle && pAvRevertMmThreadCharacteristics)
		pAvRevertMmThreadCharacteristics(mmcss_handle);
	sem_post(&wasapi_thread_stopped);
}

/* Idempotent: safe on partial-open state. Stop render thread first (no
 * more callbacks), then release COM objects in reverse acquisition order. */
static void
xp_wasapi_teardown(void)
{
	if (wasapi_thread_started) {
		wasapi_stop_requested = true;
		if (wasapi_event)
			SetEvent(wasapi_event);
		sem_wait(&wasapi_thread_stopped);
		sem_destroy(&wasapi_thread_stopped);
		wasapi_thread_started = false;
	}
	if (wasapi_client)
		IAudioClient_Stop(wasapi_client);
	if (wasapi_render) {
		IAudioRenderClient_Release(wasapi_render);
		wasapi_render = NULL;
	}
	if (wasapi_client) {
		IAudioClient_Release(wasapi_client);
		wasapi_client = NULL;
	}
	if (wasapi_device) {
		IMMDevice_Release(wasapi_device);
		wasapi_device = NULL;
	}
	if (wasapi_enumerator) {
		IMMDeviceEnumerator_Release(wasapi_enumerator);
		wasapi_enumerator = NULL;
	}
	if (wasapi_event) {
		CloseHandle(wasapi_event);
		wasapi_event = NULL;
	}
	if (avrt_dll) {
		FreeLibrary(avrt_dll);
		avrt_dll = NULL;
		pAvSetMmThreadCharacteristicsW   = NULL;
		pAvRevertMmThreadCharacteristics = NULL;
	}
	if (wasapi_com_owned) {
		CoUninitialize();
		wasapi_com_owned = false;
	}
	wasapi_buffer_frames  = 0;
	wasapi_stop_requested = false;
}

static bool
xp_wasapi_open(void)
{
	HRESULT              hr;
	WAVEFORMATEXTENSIBLE fmt;
	BYTE                *prime;
	uintptr_t            th;

	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (hr == S_OK || hr == S_FALSE)
		wasapi_com_owned = true;
	else if (hr == RPC_E_CHANGED_MODE)
		wasapi_com_owned = false;   /* already in STA; coexist */
	else
		return false;

	/* Optional MMCSS. Missing avrt.dll is fine — no priority boost. */
	avrt_dll = LoadLibraryW(L"avrt.dll");
	if (avrt_dll) {
		pAvSetMmThreadCharacteristicsW = (pfn_AvSetMmThreadCharacteristicsW)
		    GetProcAddress(avrt_dll, "AvSetMmThreadCharacteristicsW");
		pAvRevertMmThreadCharacteristics = (pfn_AvRevertMmThreadCharacteristics)
		    GetProcAddress(avrt_dll, "AvRevertMmThreadCharacteristics");
	}

	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
	                      &IID_IMMDeviceEnumerator, (void **)&wasapi_enumerator);
	if (FAILED(hr))
		goto fail;

	hr = IMMDeviceEnumerator_GetDefaultAudioEndpoint(wasapi_enumerator,
	        eRender, eConsole, &wasapi_device);
	if (FAILED(hr))
		goto fail;

	hr = IMMDevice_Activate(wasapi_device, &IID_IAudioClient, CLSCTX_ALL,
	                        NULL, (void **)&wasapi_client);
	if (FAILED(hr))
		goto fail;

	/* Shared-mode S16 stereo 44100. AUTOCONVERTPCM lets the engine
	 * resample/remap to the device's native format; we keep producing
	 * 44100 regardless of what the hardware runs at. */
	ZeroMemory(&fmt, sizeof fmt);
	fmt.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
	fmt.Format.nChannels            = S_CHANNELS;
	fmt.Format.nSamplesPerSec       = S_RATE;
	fmt.Format.wBitsPerSample       = S_BYTEDEPTH * 8;
	fmt.Format.nBlockAlign          = S_FRAMESIZE;
	fmt.Format.nAvgBytesPerSec      = S_RATE * S_FRAMESIZE;
	fmt.Format.cbSize               = sizeof(fmt) - sizeof(WAVEFORMATEX);
	fmt.Samples.wValidBitsPerSample = 16;
	fmt.dwChannelMask               = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
	fmt.SubFormat                   = xp_wasapi_ks_subtype_pcm;

	hr = IAudioClient_Initialize(wasapi_client, AUDCLNT_SHAREMODE_SHARED,
	        AUDCLNT_STREAMFLAGS_EVENTCALLBACK
	      | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
	      | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
	        100000 /* 10 ms buffer target, 100ns units */,
	        0, (WAVEFORMATEX *)&fmt, NULL);
	if (FAILED(hr))
		goto fail;

	wasapi_event = CreateEventW(NULL, FALSE, FALSE, NULL);
	if (!wasapi_event)
		goto fail;

	hr = IAudioClient_SetEventHandle(wasapi_client, wasapi_event);
	if (FAILED(hr))
		goto fail;

	hr = IAudioClient_GetService(wasapi_client, &IID_IAudioRenderClient,
	                             (void **)&wasapi_render);
	if (FAILED(hr))
		goto fail;

	hr = IAudioClient_GetBufferSize(wasapi_client, &wasapi_buffer_frames);
	if (FAILED(hr) || wasapi_buffer_frames == 0)
		goto fail;

	/* Prime with silence before Start — otherwise the engine has nothing
	 * to play, never drains, never signals the event, and the render
	 * thread blocks forever on the wait. Same issue as CoreAudio. */
	prime = NULL;
	hr = IAudioRenderClient_GetBuffer(wasapi_render, wasapi_buffer_frames, &prime);
	if (FAILED(hr))
		goto fail;
	ZeroMemory(prime, (size_t)wasapi_buffer_frames * S_FRAMESIZE);
	IAudioRenderClient_ReleaseBuffer(wasapi_render, wasapi_buffer_frames,
	                                 AUDCLNT_BUFFERFLAGS_SILENT);

	hr = IAudioClient_Start(wasapi_client);
	if (FAILED(hr))
		goto fail;

	wasapi_stop_requested = false;
	sem_init(&wasapi_thread_stopped, 0, 0);
	th = _beginthread(wasapi_thread_fn, 0, NULL);
	if (th == (uintptr_t)-1) {
		sem_destroy(&wasapi_thread_stopped);
		IAudioClient_Stop(wasapi_client);
		goto fail;
	}
	wasapi_thread_started = true;
	return true;

fail:
	xp_wasapi_teardown();
	return false;
}

static void
xp_wasapi_close(void)
{
	xp_wasapi_teardown();
}

static uint32_t
xp_wasapi_latency_ms(void)
{
	UINT32  padding = 0;
	HRESULT hr;

	if (!wasapi_client)
		return 0;
	hr = IAudioClient_GetCurrentPadding(wasapi_client, &padding);
	if (FAILED(hr))
		return 0;
	return (uint32_t)((uint64_t)padding * 1000 / S_RATE);
}
#endif /* _WIN32 && XPDEV_THREAD_SAFE && !__BORLANDC__ */

#ifdef XPDEV_THREAD_SAFE
pthread_once_t sample_initialized_pto = PTHREAD_ONCE_INIT;
#endif
static bool
xptone_open_locked(void)
{
#if defined(_WIN32) && (!defined(XPDEV_THREAD_SAFE) || defined(__BORLANDC__))
	/* waveOut format descriptor: only the non-WASAPI Win32 paths (non-MT
	 * and Borland MT) fill this in. MSVC/MinGW MT go through
	 * xp_wasapi_open() which builds its own WAVEFORMATEXTENSIBLE. */
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
#if defined(XPDEV_THREAD_SAFE) && !defined(__BORLANDC__)
			/* MT builds with Vista SDK headers go through WASAPI
			 * (shared-mode, event-driven, pull-native). The wrapper
			 * owns its own render thread and calls xp_mixer_pull
			 * directly — no device_thread_fn branch needed. */
			if (xp_wasapi_open()) {
				handle_type = SOUND_DEVICE_WIN32;
				handle_rc++;
				return true;
			}
			sound_device_open_failed = true;
			return false;
#elif !defined(XPDEV_THREAD_SAFE)
			/* Non-MT builds keep the legacy waveOut push backend used by
			 * do_xp_play_sample. */
			memset(&w, 0, sizeof(w));
			w.wFormatTag      = WAVE_FORMAT_PCM;
			w.nChannels       = S_CHANNELS;
			w.nSamplesPerSec  = S_RATE;
			w.wBitsPerSample  = S_BYTEDEPTH * 8;
			w.nBlockAlign     = (w.wBitsPerSample * w.nChannels) / 8;
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
			curr_wh = 0;
			handle_type = SOUND_DEVICE_WIN32;
			handle_rc++;
			return true;
#else
			/* Borland MT on Win32: no audio backend available (no Vista
			 * SDK headers for WASAPI; waveOut lives in the non-MT else
			 * branch). Fail gracefully; selection falls through to any
			 * other enabled backend. */
			sound_device_open_failed = true;
			return false;
#endif
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
				xp_audio_mixer_sync(i);
		}
		/* One device-latency sleep covers all streams — by the loop above
		 * every one has been drained through the mixer, and the device's
		 * kernel buffer is shared. */
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
#if defined(XPDEV_THREAD_SAFE) && !defined(__BORLANDC__)
		xp_wasapi_close();
#elif !defined(XPDEV_THREAD_SAFE)
		int i;
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
#endif
		/* Borland MT: xptone_open never succeeded for Win32, so
		 * handle_type == SOUND_DEVICE_WIN32 is unreachable here. */
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
 *   - Pull-native backends (SDL, CoreAudio, PortAudio, PulseAudio async,
 *     Win32 WASAPI): the backend's own callback (or, for WASAPI, the
 *     dedicated render thread in the xp_wasapi_* block above) calls
 *     xp_mixer_pull directly.
 *   - Push-native backends (OSS, ALSA): a dedicated device thread
 *     (started at open time) loops xp_mixer_pull → device_write.
 *
 * The mixer code never branches on backend — the split is entirely
 * confined to xptone_open_locked / xptone_close_locked.
 * ================================================================ */

#ifdef XPDEV_THREAD_SAFE

/* One queued buffer inside a stream.  `frames` is owned by the mixer
 * (caller malloc()s, channel free()s when fully consumed).  `consumed`
 * is the mixer's read cursor; it wraps at nframes for looping bufs.
 *
 * Fade metadata: the mixer applies envelopes at pull time rather than
 * baking them into the sample data on append.  Intrinsic fade-in
 * applies only to iteration 0 of a loop; intrinsic fade-out applies
 * only when the buf reaches its natural end (never during looping).
 * Overlay fade-out is set by a subsequent crossfade append: a
 * unity→zero envelope layered on top, with overlay_elapsed counting
 * frames since overlay began; when elapsed reaches total the buf is
 * dropped regardless of its own cursor state (breaks the loop).
 */
struct xp_audio_buf {
	int16_t             *frames;         /* owned — free()d on drop */
	size_t               nframes;
	size_t               consumed;

	/* Per-entry volume: multiplies the stream's base volume during
	 * playback of this buf only.  Default 1.0 (unity) means "inherit
	 * the stream volume unchanged". */
	float                volume_l;
	float                volume_r;

	size_t               fade_in;
	size_t               fade_out;

	bool                 loop;
	size_t               loop_iter;

	size_t               overlay_total;
	size_t               overlay_elapsed;

	struct xp_audio_buf *next;
};

struct xp_audio_stream {
	struct xp_audio_buf *head;          /* mixer consumes from here */
	struct xp_audio_buf *tail;          /* producer appends here->next */
	float                volume_l;      /* live — mixer reads this */
	float                volume_r;
	/* Optional equal-power volume ramp.  When ramp_total > 0 the mixer
	 * advances volume_l/r each pull by interpolating start→target in
	 * linear-amplitude space via a sin/cos curve (rising side uses sin,
	 * falling uses cos, same as the patch-fade curve), then converts
	 * back to dB for the live volume_l/r the mixer reads.
	 *
	 * Equal-power — not dB-linear — because a dB-linear 0→-60 dB ramp
	 * sits at -30 dB (~3%% amplitude) at the midpoint, so a 0↔-60 pan
	 * would fade to near-silence in the middle.  Equal-power holds
	 * each side at 0.707 (-3 dB) at the midpoint, combined power
	 * constant through the sweep.
	 *
	 * ramp_target_{l,r} (dB) are stored for the exact snap at the end
	 * of the ramp to cancel float drift. */
	float                ramp_start_amp_l;  /* linear amplitude */
	float                ramp_start_amp_r;
	float                ramp_target_amp_l;
	float                ramp_target_amp_r;
	float                ramp_target_l;     /* dB, snap target */
	float                ramp_target_r;
	size_t               ramp_elapsed;
	size_t               ramp_total;
	bool                 finished;      /* producer: no more appends */
	bool                 done;          /* mixer: empty queue, finished set */
	bool                 auto_close;    /* reap on next xp_audio_open */
	sem_t                drain_sem;     /* posted on done transition */
	xpevent_t            progress;      /* auto-reset; mixer pulses after
	                                     * consuming / dropping a buf, so
	                                     * a producer blocked on node
	                                     * malloc or a drainer on queue
	                                     * empty can re-check. */
	pthread_mutex_t      mutex;
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
	/* Precompute an equal-power fade curve: gain = sin(t * π/2).
	 * Paired with its cos(t*π/2) complement (supplied by callers that
	 * pass (total-1-pos) for the fade-out side), the two curves satisfy
	 * sin²+cos² = 1 — so a crossfade where A fades out while B fades in
	 * holds constant combined power through the overlap.
	 * A dB-linear curve here would put both A and B at −30 dB at the
	 * midpoint (combined ≈ −24 dB), which is audibly silent and makes
	 * the crossfade sound like a sequential fade-out-then-fade-in. */
	for (i = 0; i < XP_AUDIO_FADE_TABLE_SIZE; i++) {
		double t    = (double)i / (double)(XP_AUDIO_FADE_TABLE_SIZE - 1);
		double gain = sin(t * M_PI * 0.5);
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

/* Free a buf's owned frame buffer and the node itself. */
static void
free_buf(struct xp_audio_buf *b)
{
	free(b->frames);
	free(b);
}

/* Unlink and free all queued bufs on a stream.  Must be called with
 * s->mutex held. */
static void
drop_all_bufs_locked(struct xp_audio_stream *s)
{
	struct xp_audio_buf *b = s->head;
	while (b) {
		struct xp_audio_buf *next = b->next;
		free_buf(b);
		b = next;
	}
	s->head = s->tail = NULL;
}

/* Mix one buf into int32_t `accum` for up to `frames` output frames.
 * Envelopes (intrinsic fade-in/out + overlay) are evaluated sample-
 * by-sample.  `vl`/`vr` are the stream-base dB values; per-buf dB sums
 * with them and the composite converts to a linear gain once per call.
 *
 * Accumulation is lossless: per-sample sums can exceed ±32767 because
 * xp_mixer_pull applies a single soft-clip + narrow pass after every
 * buf has contributed.  Writes `*written` with the number of frames
 * actually added to `accum`.  Returns true if the buf should be
 * dropped after this pull (natural end or overlay expiry). */
static bool
mix_one_buf(struct xp_audio_buf *b, int32_t *accum, size_t frames, float vl, float vr,
            size_t *written)
{
	size_t j;
	bool   drop = false;
	/* dB summation → one powf per channel per buf per pull, then a
	 * hot-loop multiply. */
	float  gain_l = powf(10.0f, (vl + b->volume_l) / 20.0f);
	float  gain_r = powf(10.0f, (vr + b->volume_r) / 20.0f);

	for (j = 0; j < frames; j++) {
		int16_t s_l;
		int16_t s_r;
		int32_t gain_q15 = 32767;
		int32_t l_val;
		int32_t r_val;

		/* Overlay expiry breaks the loop and terminates the buf. */
		if (b->overlay_total > 0 && b->overlay_elapsed >= b->overlay_total) {
			drop = true;
			break;
		}
		if (b->consumed >= b->nframes) {
			if (b->loop) {
				b->consumed = 0;
				b->loop_iter++;
			}
			else {
				drop = true;
				break;
			}
		}

		s_l = b->frames[b->consumed * 2 + 0];
		s_r = b->frames[b->consumed * 2 + 1];

		/* Intrinsic fade-in — iteration 0 only. */
		if (b->loop_iter == 0 && b->fade_in > 0 && b->consumed < b->fade_in) {
			int16_t g = fade_gain_q15(b->consumed, b->fade_in);
			gain_q15 = (gain_q15 * g) >> 15;
		}
		/* Intrinsic fade-out — non-looping bufs only, over the tail. */
		if (!b->loop && b->fade_out > 0 && b->consumed + b->fade_out >= b->nframes) {
			int16_t g = fade_gain_q15(b->nframes - 1 - b->consumed, b->fade_out);
			gain_q15 = (gain_q15 * g) >> 15;
		}
		/* Overlay fade-out — imposed by a later crossfade append. */
		if (b->overlay_total > 0) {
			int16_t g = fade_gain_q15(
			    b->overlay_total - 1 - b->overlay_elapsed,
			    b->overlay_total);
			gain_q15 = (gain_q15 * g) >> 15;
		}

		l_val = ((int32_t)s_l * gain_q15) >> 15;
		r_val = ((int32_t)s_r * gain_q15) >> 15;
		l_val = (int32_t)((float)l_val * gain_l);
		r_val = (int32_t)((float)r_val * gain_r);

		accum[j * 2 + 0] += l_val;
		accum[j * 2 + 1] += r_val;

		b->consumed++;
		if (b->overlay_total > 0)
			b->overlay_elapsed++;
	}
	*written = j;
	/* Check for overlay / natural end at the exact end-of-frame boundary
	 * so the buf is dropped the moment it's fully consumed. */
	if (!drop && b->overlay_total > 0 && b->overlay_elapsed >= b->overlay_total)
		drop = true;
	if (!drop && b->consumed >= b->nframes && !b->loop)
		drop = true;
	return drop;
}

/* Soft-clip + narrow one int32_t accumulator sample to int16.  Uses
 * tanh as the saturator: below roughly -6 dB it's nearly linear
 * (transparent for normal mixes), above full-scale it compresses
 * smoothly into ±1.0 — the tube-style warm character instead of the
 * harsh harmonics that hard clipping produces when multi-stream sums
 * exceed range. */
static int16_t
soft_clip_narrow(int32_t sum)
{
	float   x = (float)sum / 32767.0f;
	float   y = tanhf(x);
	int32_t out = (int32_t)(y * 32767.0f);
	/* Defensive clamp: tanh is bounded in theory, but float→int rounding
	 * near the asymptote can land on the edge. */
	if (out > 32767)       out = 32767;
	else if (out < -32768) out = -32768;
	return (int16_t)out;
}

/* Advance `s`'s volume ramp by `frames` frames and update s->volume_l/r
 * to the interpolated position.  Interpolation is in linear-amplitude
 * space via the fade table (sin curve for channels going up, cos for
 * channels going down) so opposing ramps satisfy sin²+cos² = 1 and a
 * pan stays at constant combined power.  Snaps to the exact dB target
 * on the last step.  Must be called with s->mutex held. */
static void
advance_volume_ramp_locked(struct xp_audio_stream *s, size_t frames)
{
	size_t remaining;
	size_t step;
	size_t pos;
	float  rising_curve;
	float  falling_curve;
	float  amp_l;
	float  amp_r;

	if (s->ramp_total == 0)
		return;
	remaining = s->ramp_total - s->ramp_elapsed;
	step      = frames < remaining ? frames : remaining;
	s->ramp_elapsed += step;
	if (s->ramp_elapsed >= s->ramp_total) {
		s->volume_l   = s->ramp_target_l;
		s->volume_r   = s->ramp_target_r;
		s->ramp_total = 0;
		return;
	}
	pos           = s->ramp_elapsed;
	rising_curve  = (float)fade_gain_q15(pos, s->ramp_total) / 32767.0f;
	falling_curve = (float)fade_gain_q15(s->ramp_total - 1 - pos, s->ramp_total)
	                 / 32767.0f;
	/* Per channel: if target > start (rising) use sin(t·π/2); if
	 * target < start (falling) use cos(t·π/2).  Paired opposite
	 * directions give equal-power sum. */
	if (s->ramp_target_amp_l >= s->ramp_start_amp_l)
		amp_l = s->ramp_start_amp_l
		        + (s->ramp_target_amp_l - s->ramp_start_amp_l) * rising_curve;
	else
		amp_l = s->ramp_target_amp_l
		        + (s->ramp_start_amp_l - s->ramp_target_amp_l) * falling_curve;
	if (s->ramp_target_amp_r >= s->ramp_start_amp_r)
		amp_r = s->ramp_start_amp_r
		        + (s->ramp_target_amp_r - s->ramp_start_amp_r) * rising_curve;
	else
		amp_r = s->ramp_target_amp_r
		        + (s->ramp_start_amp_r - s->ramp_target_amp_r) * falling_curve;
	s->volume_l = (amp_l > 1e-6f) ? 20.0f * log10f(amp_l) : -60.0f;
	s->volume_r = (amp_r > 1e-6f) ? 20.0f * log10f(amp_r) : -60.0f;
}

/* Called by backends (callbacks directly, or push-backend device threads).
 * Fills `out` with `frames` frames of mixed S16 stereo at 44100 Hz. Silence
 * when no streams are active. Holds `mixer_lock` for the duration to serialize
 * with xp_audio_open/close/reap — this is the simplest correct way to make
 * stream freeing safe during concurrent pulls.
 *
 * Returns the number of frames at the start of `out` that were populated from
 * at least one stream — push-backend device threads use this to skip queuing
 * trailing silence between notes. */
size_t
xp_mixer_pull(int16_t *out, size_t frames)
{
	/* Persistent int32_t accumulator sized to the largest pull seen so
	 * far.  Grows monotonically under mixer_lock (which serialises
	 * pulls) and freed on process exit only.  All bufs in all streams
	 * sum into accum losslessly; a single soft-clip + narrow pass at
	 * the end writes saturated int16 to `out`. */
	static int32_t *accum     = NULL;
	static size_t   accum_cap = 0;

	int    i;
	size_t j;
	size_t max_n = 0;

	assert_pthread_mutex_lock(&mixer_lock);

	if (accum_cap < frames) {
		int32_t *p = (int32_t *)realloc(accum, frames * S_CHANNELS * sizeof(int32_t));
		if (!p) {
			assert_pthread_mutex_unlock(&mixer_lock);
			memset(out, 0, frames * S_FRAMESIZE);
			return 0;
		}
		accum     = p;
		accum_cap = frames;
	}
	memset(accum, 0, frames * S_CHANNELS * sizeof(int32_t));

	for (i = 0; i < XP_AUDIO_MAX_STREAMS; i++) {
		struct xp_audio_stream *s = mixer_streams[i];
		struct xp_audio_buf    *head;
		struct xp_audio_buf    *next;
		float                   vl;
		float                   vr;
		bool                    dropped_any = false;
		bool                    play_next;
		size_t                  written;
		bool                    drop;

		if (!s)
			continue;
		assert_pthread_mutex_lock(&s->mutex);
		/* Advance the volume ramp regardless of head state, so a
		 * listener that ramps on an empty channel and later queues a
		 * buf starts at the post-ramp value rather than the pre-ramp
		 * one. */
		advance_volume_ramp_locked(s, frames);
		if (s->done || !s->head) {
			assert_pthread_mutex_unlock(&s->mutex);
			continue;
		}
		vl = s->volume_l;
		vr = s->volume_r;

		/* FIFO: only head is active.  Exception: when head has a
		 * crossfade overlay into its successor, BOTH head (decaying)
		 * and head->next (rising) mix concurrently during the overlap.
		 * Subsequent bufs stay queued and wait their turn.  This bounds
		 * concurrent playback to two bufs per stream even when the
		 * caller stacks crossfade appends. */
		head      = s->head;
		next      = head->next;
		play_next = (head->overlay_total > 0) && next;

		written = 0;
		drop = mix_one_buf(head, accum, frames, vl, vr, &written);
		if (written > max_n)
			max_n = written;
		if (drop) {
			s->head = next;
			if (s->tail == head)
				s->tail = NULL;
			free_buf(head);
			head = NULL;
			dropped_any = true;
		}

		if (play_next) {
			struct xp_audio_buf *after_next = next->next;

			written = 0;
			drop = mix_one_buf(next, accum, frames, vl, vr, &written);
			if (written > max_n)
				max_n = written;
			if (drop) {
				if (head)
					head->next = after_next;
				else
					s->head = after_next;
				if (s->tail == next)
					s->tail = head;
				free_buf(next);
				dropped_any = true;
			}
		}

		if (s->finished && !s->head && !s->done) {
			s->done = true;
			sem_post(&s->drain_sem);
		}
		if (dropped_any || s->head == NULL)
			SetEvent(s->progress);
		assert_pthread_mutex_unlock(&s->mutex);
	}

	/* Single soft-clip + narrow pass.  When max_n < frames, the tail
	 * beyond max_n has zero accumulator so soft_clip_narrow(0) → 0; a
	 * memset would be faster for that segment but tanh(0) is trivial
	 * so the uniform pass keeps the code simple. */
	for (j = 0; j < frames * S_CHANNELS; j++)
		out[j] = soft_clip_narrow(accum[j]);

	assert_pthread_mutex_unlock(&mixer_lock);
	return max_n;
}

/* Release a stream's resources. Must be called with mixer_lock held and the
 * stream already removed from mixer_streams. */
static void
free_stream_locked(struct xp_audio_stream *s)
{
	drop_all_bufs_locked(s);
	sem_destroy(&s->drain_sem);
	CloseEvent(s->progress);
	pthread_mutex_destroy(&s->mutex);
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
	s->volume_l = volume_l;
	s->volume_r = volume_r;
	sem_init(&s->drain_sem, 0, 0);
	/* Auto-reset, initially non-signaled. */
	s->progress = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (s->progress == NULL) {
		sem_destroy(&s->drain_sem);
		free(s);
		return -1;
	}
	assert_pthread_mutex_init(&s->mutex, NULL);

	assert_pthread_mutex_lock(&mixer_lock);
	/* Reap any done+auto_close streams first to free slots. */
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
		/* Wake any producer blocked on progress, then free. */
		assert_pthread_mutex_lock(&s->mutex);
		s->done = true;
		SetEvent(s->progress);
		assert_pthread_mutex_unlock(&s->mutex);
		free_stream_locked(s);
	}
	assert_pthread_mutex_unlock(&mixer_lock);
}

bool
xp_audio_append(xp_audio_handle_t h, int16_t *frames, size_t nframes,
                const xp_audio_opts_t *opts)
{
	struct xp_audio_stream *s = stream_from_handle(h);
	struct xp_audio_buf    *node = NULL;

	if (!s || !frames || nframes == 0) {
		free(frames);
		return false;
	}

	/* Node allocation may race the mixer for memory; on failure wait
	 * for the mixer to drop at least one buf (freeing space) and retry.
	 * The stream's progress event fires on every buf drop. */
	for (;;) {
		node = calloc(1, sizeof(*node));
		if (node)
			break;
		assert_pthread_mutex_lock(&s->mutex);
		if (s->done) {
			assert_pthread_mutex_unlock(&s->mutex);
			free(frames);
			return false;
		}
		assert_pthread_mutex_unlock(&s->mutex);
		WaitForEvent(s->progress, XP_AUDIO_POLL_MS);
	}
	node->frames   = frames;
	node->nframes  = nframes;
	/* 0 dB = unity (no per-entry adjustment); opts override below. */
	node->volume_l = 0.0f;
	node->volume_r = 0.0f;
	if (opts) {
		node->fade_in  = (opts->fade_in_frames  < nframes) ? opts->fade_in_frames  : nframes;
		node->fade_out = (opts->fade_out_frames < nframes) ? opts->fade_out_frames : nframes;
		if (node->fade_in + node->fade_out > nframes) {
			size_t sum = node->fade_in + node->fade_out;
			size_t new_fi = (size_t)((uint64_t)nframes * node->fade_in / sum);
			node->fade_out = nframes - new_fi;
			node->fade_in  = new_fi;
		}
		node->loop = opts->loop;
		/* Per-entry volume rides on top of the stream base — it only
		 * attenuates / pans this one buf, never the stream.  Callers
		 * who want unity must pass 1.0f (or use XP_AUDIO_OPTS_INIT). */
		node->volume_l = opts->volume_l;
		node->volume_r = opts->volume_r;
	}

	assert_pthread_mutex_lock(&s->mutex);
	if (s->finished || s->done) {
		assert_pthread_mutex_unlock(&s->mutex);
		free_buf(node);
		return false;
	}

	/* Crossfade: start an overlay fade-out on the current tail buf
	 * immediately — it will decay over the next fade_in frames while
	 * the new buf ramps up.  No-op if queue is empty or no fade_in. */
	if (opts && opts->crossfade && opts->fade_in_frames > 0 && s->tail) {
		s->tail->overlay_total   = opts->fade_in_frames;
		s->tail->overlay_elapsed = 0;
	}

	/* Link to tail. */
	if (s->tail)
		s->tail->next = node;
	else
		s->head = node;
	s->tail = node;

	SetEvent(s->progress);
	assert_pthread_mutex_unlock(&s->mutex);
	return true;
}

xp_audio_handle_t
xp_audio_play(const int16_t *frames, size_t nframes, const xp_audio_opts_t *opts)
{
	xp_audio_handle_t  h;
	int16_t           *copy;

	if (!frames || nframes == 0)
		return -1;

	/* Stream base volume stays at unity; any opts->volume_{l,r} rides
	 * as the per-buf entry volume inside xp_audio_append. */
	h = xp_audio_open(0.0f, 0.0f);
	if (h < 0)
		return -1;

	copy = (int16_t *)malloc(nframes * S_FRAMESIZE);
	if (!copy) {
		xp_audio_close(h);
		return -1;
	}
	memcpy(copy, frames, nframes * S_FRAMESIZE);

	if (!xp_audio_append(h, copy, nframes, opts)) {
		/* append() took ownership of copy and already freed it on failure. */
		xp_audio_close(h);
		return -1;
	}

	/* If the caller didn't request looping, mark finished so the stream
	 * transitions to done once the buf drains. */
	if (!opts || !opts->loop)
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
	s->volume_l   = volume_l;
	s->volume_r   = volume_r;
	s->ramp_total = 0;
	assert_pthread_mutex_unlock(&s->mutex);
}

void
xp_audio_ramp_volume(xp_audio_handle_t h, float target_l, float target_r, size_t nframes)
{
	struct xp_audio_stream *s = stream_from_handle(h);

	if (!s)
		return;
	assert_pthread_mutex_lock(&s->mutex);
	if (nframes == 0) {
		s->volume_l   = target_l;
		s->volume_r   = target_r;
		s->ramp_total = 0;
	}
	else {
		s->ramp_start_amp_l  = powf(10.0f, s->volume_l / 20.0f);
		s->ramp_start_amp_r  = powf(10.0f, s->volume_r / 20.0f);
		s->ramp_target_amp_l = powf(10.0f, target_l    / 20.0f);
		s->ramp_target_amp_r = powf(10.0f, target_r    / 20.0f);
		s->ramp_target_l     = target_l;
		s->ramp_target_r     = target_r;
		s->ramp_elapsed      = 0;
		s->ramp_total        = nframes;
	}
	assert_pthread_mutex_unlock(&s->mutex);
}

void
xp_audio_get_volume(xp_audio_handle_t h, float *volume_l, float *volume_r)
{
	struct xp_audio_stream *s = stream_from_handle(h);

	if (!s)
		return;
	assert_pthread_mutex_lock(&s->mutex);
	if (volume_l)
		*volume_l = s->volume_l;
	if (volume_r)
		*volume_r = s->volume_r;
	assert_pthread_mutex_unlock(&s->mutex);
}

bool
xp_audio_is_idle(xp_audio_handle_t h)
{
	struct xp_audio_stream *s = stream_from_handle(h);
	bool                    idle;

	if (!s)
		return true;
	assert_pthread_mutex_lock(&s->mutex);
	idle = (s->head == NULL) || s->done;
	assert_pthread_mutex_unlock(&s->mutex);
	return idle;
}

void
xp_audio_finish(xp_audio_handle_t h)
{
	struct xp_audio_stream *s = stream_from_handle(h);
	struct xp_audio_buf    *b;

	if (!s)
		return;
	assert_pthread_mutex_lock(&s->mutex);
	s->finished = true;
	/* End any in-flight loops: once the stream is finished no new buf
	 * will arrive to terminate the loop via crossfade, so the looping
	 * tail needs to play out its fade_out (if any) and stop. */
	for (b = s->head; b; b = b->next) {
		if (b->loop) {
			b->loop = false;
			if (b->fade_out > 0) {
				size_t remaining = b->nframes - b->consumed;
				if (remaining < b->fade_out) {
					/* Not enough tail content for the
					 * fade — overlay the decay on top
					 * of the remaining frames. */
					b->overlay_total   = remaining;
					b->overlay_elapsed = 0;
				}
			}
		}
	}
	if (!s->head && !s->done) {
		s->done = true;
		sem_post(&s->drain_sem);
	}
	SetEvent(s->progress);
	assert_pthread_mutex_unlock(&s->mutex);
}

void
xp_audio_stop(xp_audio_handle_t h)
{
	struct xp_audio_stream *s = stream_from_handle(h);

	if (!s)
		return;
	assert_pthread_mutex_lock(&s->mutex);
	drop_all_bufs_locked(s);
	s->finished = true;
	if (!s->done) {
		s->done = true;
		sem_post(&s->drain_sem);
	}
	SetEvent(s->progress);
	assert_pthread_mutex_unlock(&s->mutex);
}

void
xp_audio_clear(xp_audio_handle_t h)
{
	struct xp_audio_stream *s = stream_from_handle(h);

	if (!s)
		return;
	assert_pthread_mutex_lock(&s->mutex);
	drop_all_bufs_locked(s);
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
#if defined(_WIN32) && !defined(__BORLANDC__)
		case SOUND_DEVICE_WIN32:
			/* WASAPI: padding = queued-but-not-yet-played frames. */
			return xp_wasapi_latency_ms();
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

/* Internal: block until the mixer has consumed every buf currently
 * queued on the stream (or the stream is done/stopped).  Does NOT
 * sleep for device latency — callers add that when they want the full
 * "heard by the speaker" barrier.  Used by xp_audio_drain and by the
 * per-stream walk in xptone_complete_locked.
 *
 * A looping tail would cause infinite wait; drain semantics assume
 * the caller has already ended the loop (via xp_audio_finish or a
 * final crossfade-free append).  On a live looping stream this returns
 * immediately after the non-looping prefix has drained. */
static void
xp_audio_mixer_sync(xp_audio_handle_t h)
{
	struct xp_audio_stream *s = stream_from_handle(h);

	if (!s)
		return;
	assert_pthread_mutex_lock(&s->mutex);
	while (s->head && !s->done) {
		/* If the head is a looping buf, there's nothing finite to
		 * wait for — return so the caller doesn't deadlock. */
		if (s->head->loop && s->head->overlay_total == 0)
			break;
		assert_pthread_mutex_unlock(&s->mutex);
		WaitForEvent(s->progress, XP_AUDIO_POLL_MS);
		assert_pthread_mutex_lock(&s->mutex);
	}
	assert_pthread_mutex_unlock(&s->mutex);
}

/* Block until every frame appended up to this call has been consumed by
 * the mixer AND the device's kernel buffer has had time to play them.
 * Intended for the final "I'm done, don't continue until it's been heard"
 * barrier — e.g. at the end of an ANSI-music block, so subsequent terminal
 * output doesn't race past the audio. */
void
xp_audio_drain(xp_audio_handle_t h)
{
	uint32_t extra_ms;

	xp_audio_mixer_sync(h);
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
int16_t *
xp_u8mono22k_to_s16stereo44k(const unsigned char *in, size_t in_bytes, size_t *nframes_out)
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

	h = xp_audio_play(frames, nframes, NULL);
	if (h < 0) {
		if (must_close)
			xptone_close();
		return false;
	}

	if (!background) {
		xp_audio_drain(h);
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

	converted = xp_u8mono22k_to_s16stereo44k(sample, size, &nframes);
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

	converted = xp_u8mono22k_to_s16stereo44k(sample, sample_size, &nframes);
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

#ifdef XPDEV_THREAD_SAFE
bool xptone(double freq, DWORD duration, DWORD shape)
{
	int16_t           *scratch   = NULL;
	int16_t           *chunk;
	int                samples;
	int                sample_len = 0;
	bool               ret        = false;
	bool               must_close = false;
	xp_audio_handle_t  h          = -1;

	if (freq < 17 && freq != 0)
		freq = 17;
	samples = S_RATE * duration / 1000;
	if ((shape & WAVE_SHAPE_NO_CLEAN) == 0) {
		if (freq) {
			if (samples <= S_RATE / freq * 2)
				samples = S_RATE / freq * 2;
		}
	}

	pthread_once(&sample_initialized_pto, init_sample);

	/* Ensure the audio backend is open for the life of this call. */
	assert_pthread_mutex_lock(&handle_mutex);
	if (handle_type == SOUND_DEVICE_CLOSED) {
		must_close = true;
		if (!xptone_open_locked()) {
			assert_pthread_mutex_unlock(&handle_mutex);
			goto cleanup;
		}
	}
	(void)post_open_setup_locked();
	assert_pthread_mutex_unlock(&handle_mutex);

	/* One stream for the whole tone.  Long tones (> 7.5 s) generate the
	 * first 7.5 s into a scratch buffer, trim trailing silence, then
	 * enqueue copies of that chunk back-to-back on the same stream so
	 * segments play seamlessly.  Stream opens at -12 dB — the synth
	 * renders at full amplitude and this is the canonical headroom
	 * reduction for tones (see xptone_makewave comment). */
	h = xp_audio_open(-12.0f, -12.0f);
	if (h < 0)
		goto cleanup;

	if (freq == 0 || samples > S_RATE / freq * 2) {
		scratch = (int16_t *)malloc((size_t)(S_RATE * 15 / 2) * S_FRAMESIZE);
		if (!scratch)
			goto cleanup;
		xptone_makewave(freq, scratch, S_RATE * 15 / 2, shape);
		/* Trailing-silence scan — silence = 0 in S16.  Check the L
		 * channel of each frame (R == L by construction). */
		for (sample_len = S_RATE * 15 / 2 - 1; sample_len && scratch[sample_len * S_CHANNELS] == 0; sample_len--)
			;
		sample_len++;
		while (samples > S_RATE * 15 / 2) {
			chunk = (int16_t *)malloc((size_t)sample_len * S_FRAMESIZE);
			if (!chunk)
				goto cleanup;
			memcpy(chunk, scratch, (size_t)sample_len * S_FRAMESIZE);
			if (!xp_audio_append(h, chunk, sample_len, NULL))
				goto cleanup;  /* append freed chunk on failure */
			samples -= sample_len;
		}
	}
	/* Final remainder. */
	chunk = (int16_t *)malloc((size_t)samples * S_FRAMESIZE + S_FRAMESIZE);
	if (!chunk)
		goto cleanup;
	xptone_makewave(freq, chunk, samples, shape);
	if (!xp_audio_append(h, chunk, samples, NULL))
		goto cleanup;

	xp_audio_finish(h);
	xp_audio_drain(h);
	ret = true;

cleanup:
	if (h >= 0)
		xp_audio_close(h);
	if (must_close)
		xptone_close();
	free(scratch);
	return ret;
}
#else /* !XPDEV_THREAD_SAFE */
bool xptone(double freq, DWORD duration, DWORD shape)
{
	int16_t *wave;
	int      samples;
	int      i;
	bool     ret;

	/* Max 7.5 seconds of S16 stereo.  `samples` is in frames. */
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
		for (sample_len = S_RATE * 15 / 2 - 1; sample_len && wave[sample_len * S_CHANNELS] == 0; sample_len--)
			;
		sample_len++;
		/* Apply -12 dB headroom (matches the xptone_makewave
		 * pre-rewrite behaviour; in MT builds this attenuation is
		 * done at stream-open, here we bake it in since the non-MT
		 * playback path has no mixer / stream volume). */
		for (i = 0; i < sample_len * S_CHANNELS; i++)
			wave[i] = (int16_t)((float)wave[i] * 0.251f);
		while (samples > S_RATE * 15 / 2) {
			if (!xp_play_sample16s(wave, sample_len, true)) {
				free(wave);
				return false;
			}
			samples -= sample_len;
		}
	}
	xptone_makewave(freq, wave, samples, shape);
	for (i = 0; i < samples * S_CHANNELS; i++)
		wave[i] = (int16_t)((float)wave[i] * 0.251f);
	ret = xp_play_sample16s(wave, samples, false);
	free(wave);
	return ret;
}
#endif

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
