#ifndef _SDLFUNCS_H_
#define _SDLFUNCS_H_

#include "SDL.h"
#include "SDL_thread.h"
#include "SDL_syswm.h"

#ifdef _WIN32
        #define HACK_HACK_HACK __cdecl
#else
        #define HACK_HACK_HACK
#endif

struct sdlfuncs {
	int	(HACK_HACK_HACK *Init)	(Uint32 flags);
	void	(HACK_HACK_HACK *Quit)	(void);
	void	(HACK_HACK_HACK *SetModuleHandle)	(void *hInst);
	int	(HACK_HACK_HACK *mutexP)	(SDL_mutex *mutex);
	int	(HACK_HACK_HACK *mutexV)	(SDL_mutex *mutex);
	int	(HACK_HACK_HACK *PeepEvents)	(SDL_Event *events, int numevents,
					SDL_eventaction action, Uint32 mask);
	char	*(HACK_HACK_HACK *VideoDriverName)	(char *namebuf, int maxlen);
	int	(HACK_HACK_HACK *SemWait)	(SDL_sem *sem);
	int	(HACK_HACK_HACK *SemWaitTimeout)(SDL_sem *sem, Uint32 timeout);
	int	(HACK_HACK_HACK *SemPost)	(SDL_sem *sem);
	Uint8	(HACK_HACK_HACK *EventState)	(Uint8 type, int state);
	SDL_Surface	*(HACK_HACK_HACK *CreateRGBSurface)	(Uint32 flags, int width, int height, int depth,
							Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
	SDL_Surface *(HACK_HACK_HACK *CreateRGBSurfaceFrom)(void *pixels, int width, int height, int depth, int pitch,
							Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
	int	(HACK_HACK_HACK *FillRect)	(SDL_Surface *dst, SDL_Rect *dstrect, Uint32 color);
	int	(HACK_HACK_HACK *SetColors)	(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors);
	int	(HACK_HACK_HACK *BlitSurface)	(SDL_Surface *src, SDL_Rect *srcrect,
								SDL_Surface *dst, SDL_Rect *dstrect);
	void	(HACK_HACK_HACK *UpdateRects)	(SDL_Surface *screen, int numrects, SDL_Rect *rects);
	void	(HACK_HACK_HACK *UpdateRect)	(SDL_Surface *screen, Sint32 x, Sint32 y, Uint32 w, Uint32 h);
	SDL_sem *(HACK_HACK_HACK *SDL_CreateSemaphore)	(Uint32 initial_value);
	void (HACK_HACK_HACK *SDL_DestroySemaphore)	(SDL_sem *semaphore);
	SDL_mutex	*(HACK_HACK_HACK *SDL_CreateMutex)	(void);
	struct SDL_Thread	*(HACK_HACK_HACK *CreateThread)	(int (*fn)(void *), void *data);
	void	(HACK_HACK_HACK *KillThread)	(SDL_Thread *thread);
	void	(HACK_HACK_HACK *WaitThread)	(SDL_Thread *thread, int *status);
	int	(HACK_HACK_HACK *WaitEvent)	(SDL_Event *event);
	int	(HACK_HACK_HACK *PollEvent)	(SDL_Event *event);
	SDL_Surface	*(HACK_HACK_HACK *SetVideoMode)	(int width, int height, int bpp, Uint32 flags);
	void	(HACK_HACK_HACK *FreeSurface)	(SDL_Surface *surface);
	void	(HACK_HACK_HACK *WM_SetCaption)	(const char *title, const char *icon);
	void	(HACK_HACK_HACK *WM_SetIcon)	(SDL_Surface *icon, Uint8 *mask);
	int	(HACK_HACK_HACK *ShowCursor)	(int toggle);
	Uint32	(HACK_HACK_HACK *WasInit)	(Uint32 flags);
	int	(HACK_HACK_HACK *EnableUNICODE)	(int enable);
	int	(HACK_HACK_HACK *EnableKeyRepeat)	(int delay, int interval);
	int	(HACK_HACK_HACK *GetWMInfo)	(SDL_SysWMinfo *info);
	char	*(HACK_HACK_HACK *GetError)	(void);
	int (HACK_HACK_HACK *InitSubSystem)(Uint32 flags);
	void (HACK_HACK_HACK *QuitSubSystem)(Uint32 flags);
	int (HACK_HACK_HACK *OpenAudio)(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
	void (HACK_HACK_HACK *CloseAudio)(void);
	void (HACK_HACK_HACK *LockAudio)(void);
	void (HACK_HACK_HACK *UnlockAudio)(void);
	void (HACK_HACK_HACK *PauseAudio)(int pause_on);
	SDL_audiostatus (HACK_HACK_HACK *GetAudioStatus)(void);
	Uint32	(HACK_HACK_HACK *MapRGB)	(SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);
	int	(HACK_HACK_HACK *LockSurface)	(SDL_Surface *surface);
	void (HACK_HACK_HACK *UnlockSurface)	(SDL_Surface *surface);
	SDL_Surface	*(HACK_HACK_HACK *DisplayFormat)(SDL_Surface *surf);
	int	(HACK_HACK_HACK *Flip)	(SDL_Surface *surface);
	SDL_Overlay *(HACK_HACK_HACK *CreateYUVOverlay)(int width, int height, Uint32 format, SDL_Surface *display);
	int (HACK_HACK_HACK *DisplayYUVOverlay)(SDL_Overlay *overlay, SDL_Rect *dstrect);
	void (HACK_HACK_HACK *FreeYUVOverlay)	(SDL_Overlay *overlay);
	int	(HACK_HACK_HACK *LockYUVOverlay)	(SDL_Overlay *overlay);
	void (HACK_HACK_HACK *UnlockYUVOverlay)	(SDL_Overlay *overlay);
	const SDL_VideoInfo *(HACK_HACK_HACK *GetVideoInfo)(void);
	const SDL_version *(HACK_HACK_HACK *Linked_Version)(void);
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
