#ifndef _SDLFUNCS_H_
#define _SDLFUNCS_H_

#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_syswm.h"

struct sdlfuncs {
	int	(*Init)	(Uint32 flags);
	void	(*Quit)	(void);
	int	(*SemWait)	(SDL_sem *sem);
	int	(*SemPost)	(SDL_sem *sem);
	SDL_sem *(*SDL_CreateSemaphore)	(Uint32 initial_value);
	void (*SDL_DestroySemaphore)	(SDL_sem *semaphore);
	int (*InitSubSystem)(Uint32 flags);
	void (*QuitSubSystem)(Uint32 flags);
	int (*OpenAudio)(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
	void (*CloseAudio)(void);
	void (*LockAudio)(void);
	void (*UnlockAudio)(void);
	void (*PauseAudio)(int pause_on);
	SDL_AudioStatus (*GetAudioStatus)(void);
	int	gotfuncs;
};

/* Defined in SDL_win32_main.c for Win32 */
extern struct sdlfuncs	sdl;
extern SDL_sem *sdl_exit_sem;

#ifdef __cplusplus
extern "C" {
#endif
int load_sdl_funcs(struct sdlfuncs *sdlf);
int init_sdl_audio(void);
int SDL_main_env(int argc, char *argv[], char **env);
#ifdef __cplusplus
}
#endif

#endif
