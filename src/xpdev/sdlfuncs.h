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
	int	(*SemWaitTimeout)	(SDL_sem *sem, Uint32 timeout);
	int	(*SemPost)	(SDL_sem *sem);
	Uint8	(*EventState)	(Uint8 type, int state);
	SDL_Surface	*(*CreateRGBSurface)	(Uint32 flags, int width, int height, int depth,
							Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
	SDL_Surface *(*CreateRGBSurfaceFrom)(void *pixels, int width, int height, int depth, int pitch,
							Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
	int	(*FillRect)	(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
	int	(*SetColors)	(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors);
	int	(*BlitSurface)	(SDL_Surface *src, SDL_Rect *srcrect,
								SDL_Surface *dst, SDL_Rect *dstrect);
	void	(*UpdateRects)	(SDL_Surface *screen, int numrects, SDL_Rect *rects);
	void	(*UpdateRect)	(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h);
	SDL_sem *(*SDL_CreateSemaphore)	(Uint32 initial_value);
	void (*SDL_DestroySemaphore)	(SDL_sem *semaphore);
	SDL_mutex	*(*SDL_CreateMutex)	(void);
	struct SDL_Thread	*(*CreateThread)	(int (*fn)(void *), void *data);
	void	(*KillThread)	(SDL_Thread *thread);
	void	(*WaitThread)	(SDL_Thread *thread, int *status);
	int	(*WaitEvent)	(SDL_Event *event);
	SDL_Surface	*(*SetVideoMode)	(int width, int height, int bpp, Uint32 flags);
	void	(*FreeSurface)	(SDL_Surface *surface);
	void	(*WM_SetCaption)	(const char *title, const char *icon);
	void	(*WM_SetIcon)	(SDL_Surface *icon, Uint8 *mask);
	int	(*ShowCursor)	(int toggle);
	Uint32	(*WasInit)	(Uint32 flags);
	int	(*EnableUNICODE)	(int enable);
	int	(*EnableKeyRepeat)	(int delay, int interval);
	int	(*GetWMInfo)	(SDL_SysWMinfo *info);
	char	*(*GetError)	(void);
	int (*InitSubSystem)(Uint32 flags);
	void (*QuitSubSystem)(Uint32 flags);
	int (*OpenAudio)(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
	void (*CloseAudio)(void);
	void (*LockAudio)(void);
	void (*UnlockAudio)(void);
	void (*PauseAudio)(int pause_on);
	SDL_audiostatus (*GetAudioStatus)(void);
	Uint32	(*MapRGB)	(SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);
	int	(*LockSurface)	(SDL_Surface *surface);
	void (*UnlockSurface)	(SDL_Surface *surface);
	SDL_Surface	*(*DisplayFormat)(SDL_Surface *surf);
	int	(*Flip)	(SDL_Surface *surface);
	SDL_Overlay *(*CreateYUVOverlay)(int width, int height, Uint32 format, SDL_Surface *display);
	int (*DisplayYUVOverlay)(SDL_Overlay *overlay, SDL_Rect *dstrect);
	void (*FreeYUVOverlay)	(SDL_Overlay *overlay);
	int	(*LockYUVOverlay)	(SDL_Overlay *overlay);
	void (*UnlockYUVOverlay)	(SDL_Overlay *overlay);
	const SDL_VideoInfo *(*GetVideoInfo)(void);
	const SDL_version *(*Linked_Version)(void);
	SDL_VideoInfo initial_videoinfo;
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
int init_sdl_video(void);
int SDL_main_env(int argc, char *argv[], char **env);
void run_sdl_drawing_thread(int (*drawing_thread)(void *data), void (*exit_drawing_thread)(void));
#ifdef __cplusplus
}
#endif

#endif
