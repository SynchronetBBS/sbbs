#ifndef _XPBEEP_H_
#define _XPBEEP_H_

#include "gen_defs.h"

#if defined(_WIN32)
	#if 1
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

#ifdef __cplusplus
extern "C" {
#endif
void xpbeep(double freq, DWORD duration);
#ifdef __unix__
void unix_beep(int freq, int dur);
#endif
#ifdef __cplusplus
}
#endif

#endif
