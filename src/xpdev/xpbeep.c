/* $Id$ */

/* TODO: USE PORTAUDIO! */

/* standard headers */
#include <math.h>
#include <stdlib.h>
#include "xp_dl.h"

#if defined(_WIN32)
	#include <windows.h>
	#include <mmsystem.h>
#elif defined(__unix__)
	#include <fcntl.h>
	#include <sys/ioctl.h>
	#if SOUNDCARD_H_IN==1
		#include <sys/soundcard.h>
	#elif SOUNDCARD_H_IN==2
		#include <soundcard.h>
	#elif SOUNDCARD_H_IN==3
		#include <linux/soundcard.h>
	#else
		#ifndef USE_ALSA_SOUND
			#warning Cannot find soundcard.h
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

/* xpdev headers */
#ifdef WITH_PORTAUDIO
#include <portaudio.h>
#endif

#ifdef WITH_SDL_AUDIO
#include "sdlfuncs.h"
#endif

#include "genwrap.h"
#include "xpbeep.h"

#define S_RATE	22050

#ifdef XPDEV_THREAD_SAFE
#include "threadwrap.h"

static BOOL sample_thread_running=FALSE;
static sem_t sample_pending_sem;
static sem_t sample_complete_sem;
static BOOL sample_initialized=FALSE;
static pthread_mutex_t sample_mutex;
static const unsigned char *sample_buffer;
static size_t sample_size;
#endif

static BOOL sound_device_open_failed=FALSE;
#ifdef USE_ALSA_SOUND
static BOOL alsa_device_open_failed=FALSE;
#endif
#ifdef WITH_SDL_AUDIO
static BOOL sdl_device_open_failed=FALSE;
#endif
#ifdef WITH_PORTAUDIO
static BOOL portaudio_device_open_failed=FALSE;
#endif

enum {
	 SOUND_DEVICE_CLOSED
	,SOUND_DEVICE_WIN32
	,SOUND_DEVICE_ALSA
	,SOUND_DEVICE_OSS
	,SOUND_DEVICE_SDL
	,SOUND_DEVICE_PORTAUDIO
};

static int handle_type=SOUND_DEVICE_CLOSED;

#ifdef WITH_PORTAUDIO
static PaStream			*portaudio_stream;
static int				portaudio_buf_len=0;
static int				portaudio_buf_pos=0;
static const unsigned char	*pawave=NULL;
static int				portaudio_initialized=FALSE;
#ifndef PaStream	// Detect version... defined for 1.8 and not for 1.9
#define PortAudioCallback	void
#define PaTimestamp		PaTime
#endif
struct portaudio_api_struct {
	PaError (*init)( void );
	PaError (*open)( PaStream** stream,
                              int numInputChannels,
                              int numOutputChannels,
                              PaSampleFormat sampleFormat,
                              double sampleRate,
                              unsigned long framesPerBuffer,
                              unsigned long numberOfBuffers,
                              PortAudioCallback *callback,
                              void *userData );
	PaError (*close)( PaStream* );
	PaError (*start)( PaStream *stream );
	PaError (*stop)( PaStream *stream );
	PaError (*active)( PaStream *stream );
	PaError (*write)( PaStream *stream, const void *buf, unsigned long frames );
	int	(*version)( void );
	int	ver;
};
struct portaudio_api_struct *pa_api=NULL;
#endif

#ifdef WITH_SDL_AUDIO
static SDL_AudioSpec	spec;
static int				sdl_audio_buf_len=0;
static int				sdl_audio_buf_pos=0;
static unsigned char	*swave;
static SDL_sem			*sdlToneDone;
#endif

#ifdef _WIN32
static	HWAVEOUT		waveOut;
static	WAVEHDR			wh[2];
static	int				curr_wh;
#endif

#ifdef USE_ALSA_SOUND
static snd_pcm_t *playback_handle;
static snd_pcm_hw_params_t *hw_params=NULL;
#endif

#ifdef AFMT_U8
static	int dsp;
#endif

#define WAVE_PI	3.14159265358979323846
#define WAVE_TPI 6.28318530717958647692

#ifdef USE_ALSA_SOUND
struct alsa_api_struct {
	int		(*snd_pcm_open)
				(snd_pcm_t **pcm, const char *name, snd_pcm_stream_t stream, int mode);
	int		(*snd_pcm_hw_params_malloc)
				(snd_pcm_hw_params_t **ptr);
	int		(*snd_pcm_hw_params_any)
				(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
	int		(*snd_pcm_hw_params_set_access)
				(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_access_t _access);
	int		(*snd_pcm_hw_params_set_format)
				(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, snd_pcm_format_t val);
	int		(*snd_pcm_hw_params_set_rate_near)
				(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int *val, int *dir);
	int		(*snd_pcm_hw_params_set_channels)
				(snd_pcm_t *pcm, snd_pcm_hw_params_t *params, unsigned int val);
	int		(*snd_pcm_hw_params)
				(snd_pcm_t *pcm, snd_pcm_hw_params_t *params);
	int		(*snd_pcm_prepare)
				(snd_pcm_t *pcm);
	void	(*snd_pcm_hw_params_free)
				(snd_pcm_hw_params_t *obj);
	int 	(*snd_pcm_close)
				(snd_pcm_t *pcm);
	snd_pcm_sframes_t (*snd_pcm_writei)
				(snd_pcm_t *pcm, const void *buffer, snd_pcm_uframes_t size);
	int		(*snd_pcm_drain)
				(snd_pcm_t *pcm);
};

struct alsa_api_struct *alsa_api=NULL;
#endif

/********************************************************************************/
/* Calculate and generate a sound wave pattern (thanks to Deuce!)				*/
/********************************************************************************/
void DLLCALL makewave(double freq, unsigned char *wave, int samples, enum WAVE_SHAPE shape)
{
	int	i;
	int midpoint;
	double inc;
	double pos;
	BOOL endhigh;
	int crossings;
	int numcross=0;

	if(freq==0) {
		memset(wave, 128, samples);
	}
	else {
		midpoint=samples/2;
		inc=8.0*atan(1.0);
		inc *= ((double)freq / (double)S_RATE);

		for(i=0;i<samples;i++) {
			pos=(inc*(double)i);
			pos -= (int)(pos/WAVE_TPI)*WAVE_TPI;
			switch(shape) {
				case WAVE_SHAPE_SINE:
					wave[i]=(sin (pos))*127+128;
					break;
				case WAVE_SHAPE_SINE_HARM:
					wave[i]=(sin (pos))*64+128;
					wave[i]=(sin ((inc*2)*(double)i))*24;
					wave[i]=(sin ((inc*3)*(double)i))*16;
					break;
				case WAVE_SHAPE_SAWTOOTH:
					wave[i]=(WAVE_TPI-pos)*40.5;
					break;
				case WAVE_SHAPE_SQUARE:
					wave[i]=(pos<WAVE_PI)?255:0;
					break;
				case WAVE_SHAPE_SINE_SAW:
					wave[i]=(((sin (pos))*127+128)+((WAVE_TPI-pos)*40.5))/2;
					break;
				case WAVE_SHAPE_SINE_SAW_CHORD:
					wave[i]=(((sin (pos))*64+128)+((WAVE_TPI-pos)*6.2))/2;
					wave[i]+=(sin ((inc/2)*(double)i))*24;
					wave[i]+=(sin ((inc/3)*(double)i))*16;
					break;
				case WAVE_SHAPE_SINE_SAW_HARM:
					wave[i]=(((sin (pos))*64+128)+((WAVE_TPI-pos)*6.2))/2;
					wave[i]+=(sin ((inc*2)*(double)i))*24;
					wave[i]+=(sin ((inc*3)*(double)i))*16;
					break;
			}
		}

		/* Now we have a "perfect" wave... 
		 * we must clean it up now to avoid click/pop
		 */
		if(wave[samples-1]>128)
			endhigh=TRUE;
		else
			endhigh=FALSE;
		/* Completely remove the last wave fragment */
		i=samples-1;
		if(wave[i]!=128) {
			for(;i>midpoint;i--) {
				if(endhigh && wave[i]<128)
					break;
				if(!endhigh && wave[i]>128)
					break;
				wave[i]=128;
			}
		}

		/* Number of crossings should be on the order of 5ms worth... according to the ARRL */
		/* We're ASSuming that a full wave crosses twice */
		numcross=freq/100;
		if(numcross) {
			/* Ramp up and down by one third for three corssings of 128 */
			crossings=0;
			for(i=0; i<(samples-1); i++) {
				if(((wave[i]<128 && wave[i+1]>=128) || (wave[i]>128 && wave[i+1]<=128)) && i>2) {
					crossings++;
					if(crossings>=numcross)
						break;
				}
				wave[i]=128+((wave[i]-128)/((numcross-crossings)*(numcross-crossings)));
			}
			crossings=0;
			for(i=samples-1; i>0; i--) {
				if(((wave[i]<128 && wave[i-1]>=128) || (wave[i]>128 && wave[i-1]<=128)) && i>2) {
					crossings++;
					if(crossings>=numcross)
						break;
				}
				wave[i]=128+((wave[i]-128)/((numcross-crossings)*(numcross-crossings)));
			}
		}
	}
}

#ifdef WITH_PORTAUDIO
/*
 * Used by v18 library, not v19!
 */
static int portaudio_callback(void *inputBuffer
				, void *outputBuffer
				, unsigned long framesPerBuffer
				, const PaTimestamp outTime
				, void *userData )
{
	int copylen=framesPerBuffer;
	int maxlen=portaudio_buf_len-portaudio_buf_pos;

	if(copylen>maxlen) {
		copylen=maxlen;
		memset(((char *)outputBuffer)+copylen, 128, framesPerBuffer-copylen);
	}
	if(copylen) {
		memcpy(outputBuffer, (*((unsigned char **)userData))+portaudio_buf_pos, copylen);
		portaudio_buf_pos+=copylen;
	}
	if(portaudio_buf_pos >= portaudio_buf_len)
		return(1);
	return(0);
}
#endif

#ifdef WITH_SDL_AUDIO
void DLLCALL sdl_fillbuf(void *userdata, Uint8 *stream, int len)
{
	int	copylen=len;
	int maxlen=sdl_audio_buf_len-sdl_audio_buf_pos;

	/* Copy in the current buffer */
	if(copylen>maxlen)
		copylen=maxlen;
	/* Fill with silence */
	if(len>copylen)
		memset(stream+copylen, spec.silence, len-copylen);
	if(copylen) {
		memcpy(stream, swave+sdl_audio_buf_pos, copylen);
		sdl_audio_buf_pos+=copylen;
		/* If we're done, post the semaphore */
		if(sdl_audio_buf_pos>=sdl_audio_buf_len) {
			sdl.SemPost(sdlToneDone);
			sdl_audio_buf_len=0;
			sdl_audio_buf_pos=0;
			free(swave);
		}
	}
}
#endif

BOOL DLLCALL xptone_open(void)
{
#ifdef _WIN32
	WAVEFORMATEX	w;
#endif

#ifdef AFMT_U8
	int format=AFMT_U8;
	int channels=1;
	int	rate=S_RATE;
	int	fragsize=0x7fff0004;
#endif

	/* Already open */
	if(handle_type!=SOUND_DEVICE_CLOSED)
		return(TRUE);

#ifdef WITH_PORTAUDIO
	if(!portaudio_device_open_failed) {
		if(pa_api==NULL) {
			dll_handle dl=NULL;
			const char *libnames[]={"portaudio",NULL};
			if(((pa_api=(struct portaudio_api_struct *)malloc(sizeof(struct portaudio_api_struct)))==NULL)
					|| ((dl=xp_dlopen(libnames,RTLD_LAZY,0))==NULL)
					|| ((pa_api->init=xp_dlsym(dl,Pa_Initialize))==NULL)
					|| ((pa_api->open=xp_dlsym(dl,Pa_OpenDefaultStream))==NULL)
					|| ((pa_api->close=xp_dlsym(dl,Pa_CloseStream))==NULL)
					|| ((pa_api->start=xp_dlsym(dl,Pa_StartStream))==NULL)
					||
						(
							((pa_api->active=xp_dlsym(dl,Pa_StreamActive))==NULL)
							&& ((pa_api->active=xp_dlsym(dl,Pa_IsStreamActive))==NULL)
						)
					|| ((pa_api->stop=xp_dlsym(dl,Pa_StopStream))==NULL)
					) {
				if(dl)
					xp_dlclose(dl);
				free(pa_api);
				pa_api=NULL;
            }
			else {
				/* Get version and other optional pointers */
				pa_api->ver=1800;
				if((pa_api->version=xp_dlsym(dl, Pa_GetVersion))!=NULL) {
					pa_api->ver=pa_api->version();
					if(pa_api->ver >= 1899) {
						if((pa_api->write=xp_dlsym(dl, Pa_WriteStream))==NULL) {
							xp_dlclose(dl);
							free(pa_api);
							pa_api=NULL;
						}
					}
				}
			}
            if(pa_api==NULL) {
                portaudio_device_open_failed=TRUE;
			}
		}
		if(pa_api != NULL) {
			if(!portaudio_initialized) {
				if(pa_api->init() != paNoError)
					portaudio_device_open_failed=TRUE;
				else
					portaudio_initialized=TRUE;
			}
			if(portaudio_initialized) {
				if(pa_api->open(&portaudio_stream
						, 0	/* No input */
						, 1	/* Mono output */
						, paUInt8
						, S_RATE
						, 256
						, 0
						, pa_api->ver >= 1899 ? NULL : portaudio_callback
						, &pawave) != paNoError)
					portaudio_device_open_failed=TRUE;
				else {
					handle_type=SOUND_DEVICE_PORTAUDIO;
					return(TRUE);
				}
			}
		}
	}
#endif

#ifdef WITH_SDL_AUDIO
	if(!sdl_device_open_failed) {
		if(init_sdl_audio()==-1)
			sdl_device_open_failed=TRUE;
		else {
			spec.freq=22050;
			spec.format=AUDIO_U8;
			spec.channels=1;
			spec.samples=256;		/* Size of audio buffer */
			spec.size=256;
			spec.callback=sdl_fillbuf;
			spec.userdata=NULL;
			if(sdl.OpenAudio(&spec, NULL)==-1) {
				sdl_device_open_failed=TRUE;
			}
			else {
				sdlToneDone=sdl.SDL_CreateSemaphore(0);
				sdl_audio_buf_len=0;
				sdl_audio_buf_pos=0;
				sdl.PauseAudio(FALSE);
				handle_type=SOUND_DEVICE_SDL;
				return(TRUE);
			}
		}
	}
#endif

#ifdef _WIN32
	if(!sound_device_open_failed) {
		w.wFormatTag = WAVE_FORMAT_PCM;
		w.nChannels = 1;
		w.nSamplesPerSec = S_RATE;
		w.wBitsPerSample = 8;
		w.nBlockAlign = (w.wBitsPerSample * w.nChannels) / 8;
		w.nAvgBytesPerSec = w.nSamplesPerSec * w.nBlockAlign;

		if(!sound_device_open_failed && waveOutOpen(&waveOut, WAVE_MAPPER, &w, 0, 0, 0)!=MMSYSERR_NOERROR)
			sound_device_open_failed=TRUE;
		if(sound_device_open_failed)
			return(FALSE);
		memset(&wh, 0, sizeof(wh));
		wh[0].dwBufferLength=S_RATE*15/2+1;
		wh[1].dwBufferLength=S_RATE*15/2+1;
		handle_type=SOUND_DEVICE_WIN32;
		if(!sound_device_open_failed)
			return(TRUE);
	}
#endif

#ifdef USE_ALSA_SOUND
	if(!alsa_device_open_failed) {
		if(alsa_api==NULL) {
			dll_handle dl=NULL;
			const char *libnames[]={"asound", NULL};
			if(((alsa_api=(struct alsa_api_struct *)malloc(sizeof(struct alsa_api_struct)))==NULL)
					|| ((dl=xp_dlopen(libnames,RTLD_LAZY,2))==NULL)
					|| ((alsa_api->snd_pcm_open=xp_dlsym(dl,snd_pcm_open))==NULL)
					|| ((alsa_api->snd_pcm_hw_params_malloc=xp_dlsym(dl,snd_pcm_hw_params_malloc))==NULL)
					|| ((alsa_api->snd_pcm_hw_params_any=xp_dlsym(dl,snd_pcm_hw_params_any))==NULL)
					|| ((alsa_api->snd_pcm_hw_params_set_access=xp_dlsym(dl,snd_pcm_hw_params_set_access))==NULL)
					|| ((alsa_api->snd_pcm_hw_params_set_format=xp_dlsym(dl,snd_pcm_hw_params_set_format))==NULL)
					|| ((alsa_api->snd_pcm_hw_params_set_rate_near=xp_dlsym(dl,snd_pcm_hw_params_set_rate_near))==NULL)
					|| ((alsa_api->snd_pcm_hw_params_set_channels=xp_dlsym(dl,snd_pcm_hw_params_set_channels))==NULL)
					|| ((alsa_api->snd_pcm_hw_params=xp_dlsym(dl,snd_pcm_hw_params))==NULL)
					|| ((alsa_api->snd_pcm_prepare=xp_dlsym(dl,snd_pcm_prepare))==NULL)
					|| ((alsa_api->snd_pcm_hw_params_free=xp_dlsym(dl,snd_pcm_hw_params_free))==NULL)
					|| ((alsa_api->snd_pcm_close=xp_dlsym(dl,snd_pcm_close))==NULL)
					|| ((alsa_api->snd_pcm_writei=xp_dlsym(dl,snd_pcm_writei))==NULL)
					|| ((alsa_api->snd_pcm_drain=xp_dlsym(dl,snd_pcm_drain))==NULL)
					) {
				if(dl)
					xp_dlclose(dl);
				free(alsa_api);
				alsa_api=NULL;
				alsa_device_open_failed=TRUE;
			}
			if(alsa_api==NULL)
				alsa_device_open_failed=TRUE;
		}
		if(alsa_api!=NULL) {
			unsigned int rate=S_RATE;
			if((alsa_api->snd_pcm_open(&playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)<0)
					|| (alsa_api->snd_pcm_hw_params_malloc(&hw_params)<0)
					|| (alsa_api->snd_pcm_hw_params_any(playback_handle, hw_params)<0)
					|| (alsa_api->snd_pcm_hw_params_set_access(playback_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0)
					|| (alsa_api->snd_pcm_hw_params_set_format(playback_handle, hw_params, SND_PCM_FORMAT_U8) < 0)
					|| (alsa_api->snd_pcm_hw_params_set_rate_near(playback_handle, hw_params, &rate, 0) < 0) 
					|| (alsa_api->snd_pcm_hw_params_set_channels(playback_handle, hw_params, 1) < 0)
					|| (alsa_api->snd_pcm_hw_params(playback_handle, hw_params) < 0)
					|| (alsa_api->snd_pcm_prepare(playback_handle) < 0)) {
				alsa_device_open_failed=TRUE;
				if(hw_params!=NULL)
					alsa_api->snd_pcm_hw_params_free(hw_params);
				if(playback_handle!=NULL) {
					alsa_api->snd_pcm_close(playback_handle);
					playback_handle=NULL;
				}
			}
			else {
				alsa_api->snd_pcm_hw_params_free(hw_params);
				handle_type=SOUND_DEVICE_ALSA;
				return(TRUE);
			}
		}
	}
#endif

#ifdef AFMT_U8
	if(!sound_device_open_failed) {
		if((dsp=open("/dev/dsp",O_WRONLY,0))<0) {
			sound_device_open_failed=TRUE;
		}
		else  {
			ioctl(dsp, SNDCTL_DSP_SETFRAGMENT, &fragsize);
			if((ioctl(dsp, SNDCTL_DSP_SETFMT, &format)==-1) || format!=AFMT_U8) {
				sound_device_open_failed=TRUE;
				close(dsp);
			}
			else if((ioctl(dsp, SNDCTL_DSP_CHANNELS, &channels)==-1) || channels!=1) {
				sound_device_open_failed=TRUE;
				close(dsp);
			}
			else if((ioctl(dsp, SNDCTL_DSP_SPEED, &rate)==-1) || rate!=S_RATE) {
				sound_device_open_failed=TRUE;
				close(dsp);
			}
		}
	}
	if(sound_device_open_failed)
		return(FALSE);
	handle_type=SOUND_DEVICE_OSS;
	if(!sound_device_open_failed)
		return(TRUE);
#endif
	return(FALSE);
}

void DLLCALL xptone_complete(void)
{
	if(handle_type==SOUND_DEVICE_CLOSED)
		return;
#ifdef WITH_PORTAUDIO
	else if(handle_type==SOUND_DEVICE_PORTAUDIO) {
		while(pa_api->active(portaudio_stream))
			SLEEP(1);
		pa_api->stop(portaudio_stream);
		if (pawave) {
			free((void *)pawave);
			pawave = NULL;
		}
	}
#endif

#ifdef WITH_SDL_AUDIO
	else if(handle_type==SOUND_DEVICE_SDL) {
		while(sdl.GetAudioStatus()==SDL_AUDIO_PLAYING)
			SLEEP(1);
	}
#endif

#ifdef _WIN32
	if(handle_type==SOUND_DEVICE_WIN32) {
		if(wh[0].dwFlags & WHDR_PREPARED) {
			while(waveOutUnprepareHeader(waveOut, &wh[0], sizeof(wh))==WAVERR_STILLPLAYING)
				SLEEP(1);
			FREE_AND_NULL(wh[0].lpData);
		}
		if(wh[1].dwFlags & WHDR_PREPARED) {
			while(waveOutUnprepareHeader(waveOut, &wh[1], sizeof(wh))==WAVERR_STILLPLAYING)
				SLEEP(1);
			FREE_AND_NULL(wh[1].lpData);
		}
	}
#endif

#ifdef USE_ALSA_SOUND
	if(handle_type==SOUND_DEVICE_ALSA) {
		if(!alsa_device_open_failed) {
			while(alsa_api->snd_pcm_drain(playback_handle))
				SLEEP(1);
		}
	}
#endif

#ifdef AFMT_U8
	else if(handle_type==SOUND_DEVICE_OSS) {
		ioctl(dsp, SNDCTL_DSP_SYNC, NULL);
	}
#endif

}

BOOL DLLCALL xptone_close(void)
{
	xptone_complete();
#ifdef WITH_PORTAUDIO
	if(handle_type==SOUND_DEVICE_PORTAUDIO) {
		pa_api->close(portaudio_stream);
	}
#endif

#ifdef WITH_SDL_AUDIO
	if(handle_type==SOUND_DEVICE_SDL) {
		sdl.CloseAudio();
		sdl.SDL_DestroySemaphore(sdlToneDone);
	}
#endif

#ifdef _WIN32
	if(handle_type==SOUND_DEVICE_WIN32) {
		waveOutClose(waveOut);
	}
#endif

#ifdef USE_ALSA_SOUND
	if(handle_type==SOUND_DEVICE_ALSA) {
		alsa_api->snd_pcm_close (playback_handle);
		playback_handle=NULL;
	}
#endif

#ifdef AFMT_U8
	if(handle_type==SOUND_DEVICE_OSS) {
		close(dsp);
	}
#endif
	handle_type=SOUND_DEVICE_CLOSED;
	sound_device_open_failed=FALSE;
#ifdef USE_ALSA_SOUND
	alsa_device_open_failed=FALSE;
#endif
#ifdef WITH_SDL_AUDIO
	sdl_device_open_failed=FALSE;
#endif
#ifdef WITH_PORTAUDIO
	portaudio_device_open_failed=FALSE;
#endif

	return(TRUE);
}

#ifdef XPDEV_THREAD_SAFE
void DLLCALL xp_play_sample_thread(void *data)
{
	BOOL			must_close;
	BOOL			posted_last=TRUE;
	BOOL			waited=FALSE;
	unsigned char	*sample=NULL;
	size_t			this_sample_size;

#ifdef AFMT_U8
	int wr;
	int	i;
#endif

	SetThreadName("Sample Play");
	sample_thread_running=TRUE;
	while(1) {
		must_close = FALSE;
		if(!waited) {
			if(sem_wait(&sample_pending_sem)!=0)
				goto error_return;
		}
		else
			waited=FALSE;
		posted_last=FALSE;
		if(pthread_mutex_lock(&sample_mutex)!=0)
			goto error_return;

		if(handle_type==SOUND_DEVICE_CLOSED) {
			must_close=TRUE;
			if(!xptone_open()) {
				sem_post(&sample_complete_sem);
				pthread_mutex_unlock(&sample_mutex);
				continue;
			}
		}
		this_sample_size=sample_size;
		FREE_AND_NULL(sample);
		sample=(unsigned char *)malloc(sample_size);
		if(sample==NULL) {
				sem_post(&sample_complete_sem);
				pthread_mutex_unlock(&sample_mutex);
				continue;
		}
		memcpy(sample, sample_buffer, this_sample_size);
		pthread_mutex_unlock(&sample_mutex);

	#ifdef WITH_PORTAUDIO
		if(handle_type==SOUND_DEVICE_PORTAUDIO) {
			if(pa_api->ver >= 1899) {
				pa_api->write(portaudio_stream, sample, this_sample_size);
				FREE_AND_NULL(sample);
			}
			else {
				xptone_complete();
				pawave=sample;
				sample=NULL;
				portaudio_buf_pos=0;
				portaudio_buf_len=this_sample_size;
				pa_api->start(portaudio_stream);
			}
		}
	#endif

	#ifdef WITH_SDL_AUDIO
		if(handle_type==SOUND_DEVICE_SDL) {
			sdl.LockAudio();
			swave=sample;
			sample=NULL;
			sdl_audio_buf_pos=0;
			sdl_audio_buf_len=this_sample_size;
			sdl.UnlockAudio();
			sdl.PauseAudio(FALSE);
			sdl.SemWait(sdlToneDone);
			sdl.PauseAudio(TRUE);
		}
	#endif

	#ifdef _WIN32
		if(handle_type==SOUND_DEVICE_WIN32) {
			if(wh[curr_wh].dwFlags & WHDR_PREPARED) {
				while(waveOutUnprepareHeader(waveOut, &wh[curr_wh], sizeof(wh[curr_wh]))==WAVERR_STILLPLAYING)
					SLEEP(1);
			}
			FREE_AND_NULL(wh[curr_wh].lpData);
			wh[curr_wh].lpData=sample;
			sample=NULL;
			wh[curr_wh].dwBufferLength=this_sample_size;
			if(waveOutPrepareHeader(waveOut, &wh[curr_wh], sizeof(wh[curr_wh]))==MMSYSERR_NOERROR) {
				if(waveOutWrite(waveOut, &wh[curr_wh], sizeof(wh[curr_wh]))==MMSYSERR_NOERROR) {
					curr_wh ^= 1;
				}
			}
		}
	#endif

	#ifdef USE_ALSA_SOUND
		if(handle_type==SOUND_DEVICE_ALSA) {
			int ret;
			int written=0;

			while(written < this_sample_size) {
				ret=alsa_api->snd_pcm_writei(playback_handle, sample+written, this_sample_size-written);
				if(ret < 0) {
					if(written==0) {
						/* Go back and try OSS */
						xptone_close();
						alsa_device_open_failed=TRUE;
						xptone_open();
					}
					break;
				}
				written += ret;
			}
#ifndef AFMT_U8
			FREE_AND_NULL(sample);
#endif
		}
	#endif

	#ifdef AFMT_U8
		if(handle_type==SOUND_DEVICE_OSS) {
			wr=0;
			while(wr<sample_size) {
				i=write(dsp, sample_buffer+wr, sample_size-wr);
				if(i>=0)
					wr+=i;
			}
			FREE_AND_NULL(sample);
		}
	#endif
		sem_post(&sample_complete_sem);
		posted_last=TRUE;
		if(must_close) {
			if(sem_trywait(&sample_pending_sem)==0)
				waited=TRUE;
			else
				xptone_close();
		}
	}

error_return:
#ifdef _WIN32
	if(handle_type==SOUND_DEVICE_WIN32) {
		if(wh[curr_wh].dwFlags & WHDR_PREPARED) {
			while(waveOutUnprepareHeader(waveOut, &wh[curr_wh], sizeof(wh[curr_wh]))==WAVERR_STILLPLAYING)
				SLEEP(1);
		}
		FREE_AND_NULL(wh[curr_wh].lpData);
	}
#endif

	FREE_AND_NULL(sample);
	xptone_close();
	if(!posted_last)
		sem_post(&sample_complete_sem);
	sample_thread_running=FALSE;
}

BOOL DLLCALL xp_play_sample(const unsigned char *sample, size_t size, BOOL background)
{
	if(!sample_initialized) {
		if(pthread_mutex_init(&sample_mutex, NULL)!=0)
			return(FALSE);
		pthread_mutex_lock(&sample_mutex);
		if(sem_init(&sample_pending_sem, 0, 0)!=0) {
			pthread_mutex_unlock(&sample_mutex);
			pthread_mutex_destroy(&sample_mutex);
			return(FALSE);
		}
		if(sem_init(&sample_complete_sem, 0, 1)!=0) {
			pthread_mutex_unlock(&sample_mutex);
			pthread_mutex_destroy(&sample_mutex);
			sem_destroy(&sample_pending_sem);
			return(FALSE);
		}
		sample_initialized=TRUE;
		pthread_mutex_unlock(&sample_mutex);
	}

	sem_wait(&sample_complete_sem);
	pthread_mutex_lock(&sample_mutex);
	if(!sample_thread_running) {
		_beginthread(xp_play_sample_thread, 0,NULL);
	}

	sample_buffer=sample;
	sample_size=size;
	pthread_mutex_unlock(&sample_mutex);
	sem_post(&sample_pending_sem);
	if(!background) {
		sem_wait(&sample_complete_sem);
		sem_post(&sample_complete_sem);
	}
	return(TRUE);
}
#else
BOOL DLLCALL xp_play_sample(const unsigned char *sample, size_t sample_size, BOOL background)
{
	BOOL			must_close=FALSE;

#ifdef AFMT_U8
	int wr;
	int	i;
#endif

	if(handle_type==SOUND_DEVICE_CLOSED) {
		must_close=TRUE;
		if(!xptone_open())
			return(FALSE);
	}

#ifdef WITH_PORTAUDIO
	if(handle_type==SOUND_DEVICE_PORTAUDIO) {
		if(pa_api->ver >= 1899) {
			pa_api->write(portaudio_stream, sample, sample_size);
			free(sample);
		}
		else {
			xptone_complete();
			pawave=sample;
			portaudio_buf_pos=0;
			portaudio_buf_len=sample_size;
			pa_api->start(portaudio_stream);
		}
		if(must_close)
			xptone_close();
		return TRUE;
	}
#endif

#ifdef WITH_SDL_AUDIO
	if(handle_type==SOUND_DEVICE_SDL) {
		sdl.LockAudio();
		swave=sample;
		sdl_audio_buf_pos=0;
		sdl_audio_buf_len=sample_size;
		sdl.UnlockAudio();
		sdl.PauseAudio(FALSE);
		sdl.SemWait(sdlToneDone);
		sdl.PauseAudio(TRUE);
		if(must_close)
			xptone_close();
		return TRUE;
	}
#endif

#ifdef _WIN32
	if(handle_type==SOUND_DEVICE_WIN32) {
		while(waveOutUnprepareHeader(waveOut, &wh[curr_wh], sizeof(wh[curr_wh]))==WAVERR_STILLPLAYING)
			SLEEP(1);
		FREE_AND_NULL(wh[curr_wh].lpData);
		wh[curr_wh].lpData=sample;
		wh[curr_wh].dwBufferLength=sample_size;
		if(waveOutPrepareHeader(waveOut, &wh[curr_wh], sizeof(wh[curr_wh]))==MMSYSERR_NOERROR) {
			if(waveOutWrite(waveOut, &wh[curr_wh], sizeof(wh[curr_wh]))==MMSYSERR_NOERROR) {
				curr_wh ^= 1;
				if(must_close)
					xptone_close();
				return TRUE;
			}
		}
	}
#endif

#ifdef USE_ALSA_SOUND
	if(handle_type==SOUND_DEVICE_ALSA) {
		int ret;
		int written=0;

		while(written < sample_size) {
			ret=alsa_api->snd_pcm_writei(playback_handle, sample+written, sample_size-written);
			if(ret < 0) {
				if(written==0) {
					/* Go back and try OSS */
					xptone_close();
					alsa_device_open_failed=TRUE;
					xptone_open();
				}
				break;
			}
			written += ret;
		}
		if(!alsa_device_open_failed) {
			if(must_close)
				xptone_close();
			return(TRUE);
		}
	}
#endif

#ifdef AFMT_U8
	if(handle_type==SOUND_DEVICE_OSS) {
		wr=0;
		while(wr<sample_size) {
			i=write(dsp, sample+wr, sample_size-wr);
			if(i>=0)
				wr+=i;
		}
		if(must_close)
			xptone_close();
		return(TRUE);
	}
#endif
	if(must_close)
		xptone_close();
	return(FALSE);
}
#endif

/********************************************************************************/
/* Play a tone through the wave/DSP output device (sound card) - Deuce			*/
/********************************************************************************/

BOOL DLLCALL xptone(double freq, DWORD duration, enum WAVE_SHAPE shape)
{
	unsigned char	*wave;
	int samples;

	wave=(unsigned char *)malloc(S_RATE*15/2+1);
	if(!wave)
		return FALSE;
	if(freq<17 && freq != 0)
		freq=17;
	samples=S_RATE*duration/1000;
	if(freq) {
		if(samples<=S_RATE/freq*2)
			samples=S_RATE/freq*2;
	}
	if(freq==0 || samples > S_RATE/freq*2) {
		int sample_len;

		makewave(freq,wave,S_RATE*15/2,shape);
		for(sample_len=S_RATE*15/2-1; sample_len && wave[sample_len]==128; sample_len--)
			;
		sample_len++;
		while(samples > S_RATE*15/2) {
			if(!xp_play_sample(wave, sample_len, TRUE))
				return FALSE;
			samples -= sample_len;
		}
	}
	makewave(freq,wave,samples,shape);
	return(xp_play_sample(wave, samples, FALSE));
}

#ifdef __unix__
/****************************************************************************/
/* Generate a tone at specified frequency for specified milliseconds		*/
/* Thanks to Casey Martin (and Deuce) for this code							*/
/****************************************************************************/
void DLLCALL unix_beep(int freq, int dur)
{
	static int console_fd=-1;

#if (defined(__FreeBSD__) && defined(HAS_MACHINE_SPEAKER_H)) || ((defined(__OpenBSD__) || defined(__NetBSD__)) && defined(HAS_MACHINE_SPKR_H))
	int speaker_fd=-1;
	tone_t tone;

	speaker_fd = open("/dev/speaker", O_WRONLY|O_APPEND);
	if(speaker_fd != -1)  {
		tone.frequency=freq;
		tone.duration=dur/10;
		ioctl(speaker_fd,SPKRTONE,&tone);
		close(speaker_fd);
		return;
	}
#endif

#if !defined(__GNU__) && !defined(__QNX__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__APPLE__) && !defined(__CYGWIN__) && !defined(__HAIKU__)
	if(console_fd == -1) 
  		console_fd = open("/dev/console", O_NOCTTY);
	
	if(console_fd != -1) {
#if defined(__solaris__)
		ioctl(console_fd, KIOCCMD, KBD_CMD_BELL);
#else
		if(freq != 0)	/* Don't divide by zero */
			ioctl(console_fd, KIOCSOUND, (int) (1193180 / freq));
#endif /* solaris */
		SLEEP(dur);
#if defined(__solaris__)
		ioctl(console_fd, KIOCCMD, KBD_CMD_NOBELL);	/* turn off tone */
#else
		ioctl(console_fd, KIOCSOUND, 0);	/* turn off tone */
#endif /* solaris */
	}
#endif
}

#endif

/********************************************************************************/
/* Play sound through DSP/wave device, if unsuccessful, play through PC speaker	*/
/********************************************************************************/
void DLLCALL xpbeep(double freq, DWORD duration)
{
	if(xptone(freq,duration,WAVE_SHAPE_SINE_SAW_HARM))
		return;

#if defined(_WIN32)
	Beep((DWORD)(freq+.5), duration);
#elif defined(__unix__)
	unix_beep((int)(freq+.5),duration);
#endif
}
