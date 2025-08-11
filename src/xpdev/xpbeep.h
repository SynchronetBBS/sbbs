/* $Id: xpbeep.h,v 1.16 2020/04/15 15:18:00 deuce Exp $ */

#ifndef _XPBEEP_H_
#define _XPBEEP_H_

#include "gen_defs.h"
#include "wrapdll.h"

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
#define XPBEEP_DEVICE_DEFAULT (XPBEEP_DEVICE_WIN32 | XPBEEP_DEVICE_ALSA | XPBEEP_DEVICE_OSS | XPBEEP_DEVICE_PORTAUDIO | XPBEEP_DEVICE_PULSEAUDIO)
extern uint32_t xpbeep_sound_devices_enabled;

#ifdef __cplusplus
extern "C" {
#endif
DLLEXPORT void xptone_makewave(double freq, unsigned char *wave, int samples, DWORD shape);
DLLEXPORT bool xptone_open(void);
DLLEXPORT bool xptone_close(void);
DLLEXPORT void xpbeep(double freq, DWORD duration);
DLLEXPORT bool xp_play_sample(unsigned char *sample, size_t sample_size, bool background);
DLLEXPORT void xptone_complete(void);
DLLEXPORT bool xptone(double freq, DWORD duration, DWORD shape);
#ifdef __unix__
DLLEXPORT void unix_beep(int freq, int dur);
#endif
#ifdef __cplusplus
}
#endif

#endif
