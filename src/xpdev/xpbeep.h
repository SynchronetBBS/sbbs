#ifndef _XPBEEP_H_
#define _XPBEEP_H_

#if defined(_WIN32)
	#define BEEP(freq,dur)	SineBeep((double)(freq),(DWORD)(dur))
#elif defined(__OS2__)
	#define BEEP(freq,dur)	DosBeep(freq,dur)
#elif defined(__unix__) || defined(__APPLE__)
	#define BEEP(freq,dur)	SineBeep((double)(freq),(DWORD)(dur))
#else
	#error "Unsupported Target: Need some macros and/or function prototypes here."
#endif

#ifdef __cplusplus
extern "C" {
#endif
void SineBeep(double freq, DWORD duration);
#ifdef __unix__
void unix_beep(int freq, int dur);
#endif
#ifdef __cplusplus
}
#endif

#endif
