#include <math.h>

#ifdef _WIN32
	#include <windows.h>
	#include <mmsystem.h>
#endif
#ifdef __unix__
	#include <fcntl.h>
	#include <machine/soundcard.h>
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
#include "genwrap.h"

#define S_RATE	11025

static BOOL sound_device_open_failed=FALSE;

void makesine(double freq, unsigned char *wave, int samples)
{
	int	i;
	int j;
	int k;
	double inc;
	BOOL endhigh;
	BOOL starthigh;

	inc=8.0*atan(1.0);
	inc *= ((double)freq / (double)S_RATE);

	for(i=0;i<samples;i++) {
		wave[i]=(sin (inc * (double)i))*127+128;
	}
	
	/* Now we have a "perfect" sine wave... 
	 * we must clean it up now to avoid click/pop
	 */
	if(wave[samples-1]>127)
		endhigh=TRUE;
	else
		endhigh=FALSE;
	/* Completely remove the last wave fragment */
	for(i=samples-1;i>0;i--) {
		if(endhigh && wave[i]<=127)
			break;
		if(!endhigh && wave[i]>=127)
			break;
		wave[i]=127;
	}
	/* Fade out */
	for(k=10;k>1;k--) {
		for(;i>0;i--) {
			if(!endhigh && wave[i]<=127)
				break;
			if(endhigh && wave[i]>=127)
				break;
			j=wave[i];
			j-=128;
			j/=k;
			wave[i]=j+128;
		}
		for(;i>0;i--) {
			if(endhigh && wave[i]<=127)
				break;
			if(!endhigh && wave[i]>=127)
				break;
			j=wave[i];
			j-=128;
			j/=k;
			wave[i]=j+128;
		}
	}

	if(wave[0]>127)
		starthigh=TRUE;
	else
		starthigh=FALSE;
	/* Completely remove the first wave fragment */
	for(i=0;i<samples;i++) {
		if(starthigh && wave[i]<=127)
			break;
		if(!starthigh && wave[i]>=127)
			break;
		wave[i]=127;
	}
	/* Fade in */
	for(k=10;k>1;k--) {
		for(;i<samples;i--) {
			if(!starthigh && wave[i]<=127)
				break;
			if(starthigh && wave[i]>=127)
				break;
			j=wave[i];
			j-=128;
			j/=k;
			wave[i]=j+128;
		}
		for(;i<samples;i--) {
			if(starthigh && wave[i]<=127)
				break;
			if(!starthigh && wave[i]>=127)
				break;
			j=wave[i];
			j-=128;
			j/=k;
			wave[i]=j+128;
		}
	}
}

#ifdef _WIN32

void SineBeep(double freq, DWORD duration)
{
	WAVEFORMATEX	w;
	WAVEHDR			wh;
	HWAVEOUT		waveOut;
	unsigned char	wave[S_RATE*15/2+1];
	WORD* 			p;
	DWORD			endTime;

	w.wFormatTag = WAVE_FORMAT_PCM;
	w.nChannels = 1;
	w.nSamplesPerSec = S_RATE;
	w.wBitsPerSample = 8;
	w.nBlockAlign = (w.wBitsPerSample * w.nChannels) / 8;
	w.nAvgBytesPerSec = w.nSamplesPerSec * w.nBlockAlign;

	if(!sound_device_open_failed && waveOutOpen(&waveOut, WAVE_MAPPER, &w, 0, 0, 0)!=MMSYSERR_NOERROR)
		sound_device_open_failed=TRUE;
	if(sound_device_open_failed) {
		Beep((DWORD)(freq+.5), duration);
		return;
	}
	memset(&wh, 0, sizeof(wh));
	wh.lpData=wave;
	wh.dwBufferLength=S_RATE*duration/1000;
	makesine(freq,wave,wh.dwBufferLength);
	if(waveOutPrepareHeader(waveOut, &wh, sizeof(wh))!=MMSYSERR_NOERROR)
		goto abrt;
	if(waveOutWrite(waveOut, &wh, sizeof(wh))!=MMSYSERR_NOERROR)
		goto abrt;
abrt:
	while(waveOutUnprepareHeader(waveOut, &wh, sizeof(wh))==WAVERR_STILLPLAYING)
		SLEEP(1);
	waveOutClose(waveOut);
}

#endif /* _WIN32 */

#ifdef __unix__
/****************************************************************************/
/* Generate a tone at specified frequency for specified milliseconds		*/
/* Thanks to Casey Martin for this code										*/
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

void DLLCALL SineBeep(double freq, DWORD duration)
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
	makesine(freq,wave,samples);
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
	if(sound_device_open_failed) {
		sound_device_open_failed=TRUE;
		unix_beep((int)(freq+.5),duration);
		return;
	}
	wr=0;
	while(wr<samples) {
		i=write(dsp, wave+wr, samples-wr);
		if(i>=0)
			wr+=i;
	}
	close(dsp);
#else
		unix_beep((int)(freq+.5),duration);
#endif
}
#endif
