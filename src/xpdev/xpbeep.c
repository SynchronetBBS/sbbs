/* $Id$ */

/* standard headers */
#include <math.h>

#if defined(_WIN32)
	#include <windows.h>
	#include <mmsystem.h>
#elif defined(__unix__)
	#include <fcntl.h>
	#if SOUNDCARD_H_IN==SYS
		#include <sys/soundcard.h>
	#elif SOUNDCARD_H_IN==INCLUDE
		#include <soundcard.h>
	#elif SOUNDCARD_H_IN==LINUX
		#include <linux/soundcard.h>
	#else
		#warning Cannot find soundcard.h
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
	#if defined(__OpenBSD__) || defined(__NetBSD__)
		#include <machine/spkr.h>
	#elif defined(__FreeBSD__)
		#include <machine/speaker.h>
	#endif
#endif

/* xpdev headers */
#include "genwrap.h"
#include "xpbeep.h"

#define S_RATE	22050

static BOOL sound_device_open_failed=FALSE;

#define WAVE_PI	3.14159265358979323846
#define WAVE_TPI 6.28318530717958647692

/********************************************************************************/
/* Calculate and generate a sound wave pattern (thanks to Deuce!)				*/
/********************************************************************************/
void makewave(double freq, unsigned char *wave, int samples, enum WAVE_SHAPE shape)
{
	int	i;
	int j;
	int midpoint;
	double inc;
	double pos;
	BOOL endhigh;
	BOOL starthigh;

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

#if 0
	if(wave[0]>128)
		starthigh=TRUE;
	else
		starthigh=FALSE;
	/* Completely remove the first wave fragment */
	i=0;
	if(wave[i]!=128) {
		for(;i<midpoint;i++) {
			if(starthigh && wave[i]<128)
				break;
			if(!starthigh && wave[i]>128)
				break;
			wave[i]=128;
		}
	}
#endif
}

/********************************************************************************/
/* Play a tone through the wave/DSP output device (sound card) - Deuce			*/
/********************************************************************************/

#if defined(_WIN32)

BOOL xptone(double freq, DWORD duration, enum WAVE_SHAPE shape)
{
	WAVEFORMATEX	w;
	WAVEHDR			wh;
	HWAVEOUT		waveOut;
	unsigned char	wave[S_RATE*15/2+1];
	WORD* 			p;
	DWORD			endTime;
	BOOL			success=FALSE;

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
	wh.lpData=wave;
	wh.dwBufferLength=S_RATE*duration/1000;
	if(wh.dwBufferLength<=S_RATE/freq*2)
		wh.dwBufferLength=S_RATE/freq*2;

	makewave(freq,wave,wh.dwBufferLength,shape);
	if(waveOutPrepareHeader(waveOut, &wh, sizeof(wh))!=MMSYSERR_NOERROR)
		goto abrt;
	if(waveOutWrite(waveOut, &wh, sizeof(wh))==MMSYSERR_NOERROR)
		success=TRUE;
abrt:
	while(waveOutUnprepareHeader(waveOut, &wh, sizeof(wh))==WAVERR_STILLPLAYING)
		SLEEP(1);
	waveOutClose(waveOut);

	return(success);
}

#elif defined(__unix__)

BOOL DLLCALL xptone(double freq, DWORD duration, enum WAVE_SHAPE shape)
{
#ifdef AFMT_U8
	int dsp;
	int format=AFMT_U8;
	int channels=1;
	int	rate=S_RATE;
	int samples;
	int	fragsize=0x7fff0004;
	int wr;
	int	i;
	unsigned char	wave[S_RATE*15/2+1];

	samples=S_RATE*duration/1000;
	if(samples<=S_RATE/freq*2)
		samples=S_RATE/freq*2;
	makewave(freq,wave,samples,shape);
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
	wr=0;
	while(wr<samples) {
		i=write(dsp, wave+wr, samples-wr);
		if(i>=0)
			wr+=i;
	}
	close(dsp);

	return(TRUE);
#else
	return(FALSE);
#endif
}

/****************************************************************************/
/* Generate a tone at specified frequency for specified milliseconds		*/
/* Thanks to Casey Martin (and Deuce) for this code							*/
/****************************************************************************/
void DLLCALL unix_beep(int freq, int dur)
{
	static int console_fd=-1;

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__)
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

#if !defined(__GNU__) && !defined(__QNX__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__APPLE__)
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
void xpbeep(double freq, DWORD duration)
{
	if(xptone(freq,duration,WAVE_SHAPE_SINE_SAW_HARM))
		return;

#if defined(_WIN32)
	Beep((DWORD)(freq+.5), duration);
#elif defined(__unix__)
	unix_beep((int)(freq+.5),duration);
#endif
}
