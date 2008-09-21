/* $Id$ */

#ifndef _XPBEEP_H_
#define _XPBEEP_H_

#include "gen_defs.h"

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


#ifdef __cplusplus
extern "C" {
#endif
BOOL xptone_open(void);
BOOL xptone_close(void);
void xpbeep(double freq, DWORD duration);
BOOL xp_play_sample(unsigned char *sample, size_t sample_size, BOOL background);
BOOL xptone(double freq, DWORD duration, enum WAVE_SHAPE);
#ifdef __unix__
void unix_beep(int freq, int dur);
#endif
#ifdef __cplusplus
}
#endif

#endif
