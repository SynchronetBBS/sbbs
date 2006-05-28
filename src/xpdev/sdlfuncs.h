#ifndef _SDLFUNCS_H_
#define _SDLFUNCS_H_

#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_syswm.h"

struct sdlfuncs {
	int	(*Init)	(Uint32 flags);
	void	(*Quit)	(void);
	void	(*SetModuleHandle)	(void *hInst);
	int	(*mutexP)	(SDL_mutex *mutex);
	int	(*mutexV)	(SDL_mutex *mutex);
	int	(*PeepEvents)	(SDL_Event *events, int numevents,
					SDL_eventaction action, Uint32 mask);
	char	*(*VideoDriverName)	(char *namebuf, int maxlen);
	int	(*SemWait)	(SDL_sem *sem);
	int	(*SemPost)	(SDL_sem *sem);
	Uint8	(*EventState)	(Uint8 type, int state);
	SDL_Surface	*(*CreateRGBSurface)	(Uint32 flags, int width, int height, int depth,
							Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
	int	(*FillRect)	(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
	int	(*SetColors)	(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors);
	int	(*BlitSurface)	(SDL_Surface *src, SDL_Rect *srcrect,
								SDL_Surface *dst, SDL_Rect *dstrect);
	void	(*UpdateRects)	(SDL_Surface *screen, int numrects, SDL_Rect *rects);
	SDL_sem *(*SDL_CreateSemaphore)	(Uint32 initial_value);
	void (*SDL_DestroySemaphore)	(SDL_sem *semaphore);
	SDL_mutex	*(*SDL_CreateMutex)	(void);
	struct SDL_Thread	*(*CreateThread)	(int (*fn)(void *), void *data);
	int	(*WaitEvent)	(SDL_Event *event);
	SDL_Surface	*(*SetVideoMode)	(int width, int height, int bpp, Uint32 flags);
	void	(*FreeSurface)	(SDL_Surface *surface);
	void	(*WM_SetCaption)	(const char *title, const char *icon);
	int	(*ShowCursor)	(int toggle);
	Uint32	(*WasInit)	(Uint32 flags);
	int	(*EnableUNICODE)	(int enable);
	int	(*EnableKeyRepeat)	(int delay, int interval);
	int	(*GetWMInfo)	(struct SDL_SysWMinfo *info);
	char	*(*GetError)	(void);
	int (*InitSubSystem)(Uint32 flags);
	void (*QuitSubSystem)(Uint32 flags);
	int (*OpenAudio)(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
	void (*CloseAudio)(void);
	void (*PauseAudio)(int pause_on);
	int	gotfuncs;
};

/* Defined in SDL_win32_main.c for Win32 */
extern struct sdlfuncs	sdl;

#ifdef __cplusplus
extern "C" {
#endif
int load_sdl_funcs(struct sdlfuncs *sdlf);
int init_sdl_audio(void);
int init_sdl_video(void);
#ifdef __cplusplus
}
#endif

#endif
