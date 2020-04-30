#ifndef _SDLFUNCS_H_
#define _SDLFUNCS_H_

#include "SDL.h"
#include "SDL_syswm.h"

#ifdef _WIN32
        #define HACK_HACK_HACK __cdecl
#else
        #define HACK_HACK_HACK
#endif

struct sdlfuncs {
	int	(HACK_HACK_HACK *Init)	(Uint32 flags);
	void	(HACK_HACK_HACK *Quit)	(void);
	int	(HACK_HACK_HACK *PeepEvents)	(SDL_Event *events, int numevents,
					SDL_eventaction action, Uint32 minType, Uint32 maxType);
	char	*(HACK_HACK_HACK *GetCurrentVideoDriver)	(void);
	Uint8	(HACK_HACK_HACK *EventState)	(Uint32 type, int state);
	SDL_Surface *(HACK_HACK_HACK *CreateRGBSurfaceFrom)(void *pixels, int width, int height, int depth, int pitch,
							Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
	void	(HACK_HACK_HACK *RenderPresent)	(SDL_Renderer *renderer);
	int	(HACK_HACK_HACK *WaitEventTimeout)	(SDL_Event *event, int timeout);
	int (HACK_HACK_HACK *CreateWindowAndRenderer)	(int w, int h, Uint32 flags, SDL_Window **win, SDL_Renderer **ren);
	void	(HACK_HACK_HACK *FreeSurface)	(SDL_Surface *surface);
	void	(HACK_HACK_HACK *SetWindowTitle)	(SDL_Window *window, const char *title);
	void	(HACK_HACK_HACK *GetWindowSize)	(SDL_Window *window, int *w, int *h);
	void	(HACK_HACK_HACK *SetWindowIcon)	(SDL_Window *win, SDL_Surface *icon);
	int	(HACK_HACK_HACK *ShowCursor)	(int toggle);
	Uint32	(HACK_HACK_HACK *WasInit)	(Uint32 flags);
	SDL_bool (HACK_HACK_HACK *GetWindowWMInfo) (SDL_Window *window, SDL_SysWMinfo *info);
	const char	*(HACK_HACK_HACK *GetError)	(void);
	int (HACK_HACK_HACK *InitSubSystem)(Uint32 flags);
	void (HACK_HACK_HACK *QuitSubSystem)(Uint32 flags);
	SDL_Texture* (HACK_HACK_HACK *CreateTexture)	(SDL_Renderer *renderer, Uint32 format, int access, int w, int h);
	int (HACK_HACK_HACK *UpdateTexture)	(SDL_Texture *texture, const SDL_Rect * rect, const void *pixels, int pitch);
	int (HACK_HACK_HACK *RenderClear)	(SDL_Renderer *renderer);
	int (HACK_HACK_HACK *RenderCopy)	(SDL_Renderer *renderer, SDL_Texture *texture, const SDL_Rect *srcrect, const SDL_Rect *dstrect);
	SDL_bool (HACK_HACK_HACK *SetHint)	(const char *name, const char *value);
	const char * (HACK_HACK_HACK *GetHint)	(const char *name);
	SDL_Keymod (HACK_HACK_HACK *GetModState)	(void);
	void (HACK_HACK_HACK *SetWindowSize)	(SDL_Window *window, int w, int h);
	void (HACK_HACK_HACK *DestroyTexture)	(SDL_Texture *texture);
	void (HACK_HACK_HACK *SetWindowFullscreen)	(SDL_Window *window, Uint32 flags);
	void (HACK_HACK_HACK *LockTexture)	(SDL_Texture *texture, const SDL_Rect *rect, void **pixels, int *pitch);
	void (HACK_HACK_HACK *UnlockTexture)	(SDL_Texture *texture);
	void (HACK_HACK_HACK *QueryTexture)	(SDL_Texture *texture, Uint32 *format, int *access, int *w, int *h);
	void (HACK_HACK_HACK *GetWindowPosition)	(SDL_Window *window, int *x, int *y);
	void (HACK_HACK_HACK *SetWindowPosition)	(SDL_Window *window, int x, int y);
	void (HACK_HACK_HACK *SetWindowMinimumSize)	(SDL_Window *window, int w, int y);
	void (HACK_HACK_HACK *SetClipboardText)	(const char *);
	char *(HACK_HACK_HACK *GetClipboardText)	(void);
	void(HACK_HACK_HACK *free)	(void *);
	int	gotfuncs;
};

/* Defined in SDL_win32_main.c for Win32 */
extern struct sdlfuncs	sdl;
extern sem_t *sdl_exit_sem;

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
