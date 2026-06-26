#ifndef SYNCDUKE_SDL_SHIM_H_
#define SYNCDUKE_SDL_SHIM_H_

/*
 * Minimal no-op SDL.h shim for SyncDuke's headless build.
 *
 * The door builds with USE_SDL undefined, so duke3d.h/dukewin.h drop their real
 * SDL.h / SDL_mixer.h includes. A few spots in the vendored sources reference SDL
 * OUTSIDE those guards -- global.c's Error() calls SDL_Quit(), and menues.c has a
 * mouse-cursor-grab menu toggle -- all irrelevant to a terminal door. This shim
 * satisfies those references with no-ops, so we neither pull in real SDL nor edit
 * the vendored engine. It is found via the compat/ include dir for the two `#include
 * "SDL.h"` lines that aren't USE_SDL-gated upstream.
 */

typedef struct SDL_Surface { int w, h; void *pixels; } SDL_Surface;

#define SDL_GRAB_QUERY        0
#define SDL_GRAB_OFF          0
#define SDL_GRAB_ON           1
#define SDL_INIT_VIDEO        0
#define SDL_Quit()            ((void)0)
#define SDL_QuitSubSystem(x)  ((void)(x))
#define SDL_ShowCursor(x)     ((void)(x))
#define SDL_WM_GrabInput(m)   (SDL_GRAB_OFF)   /* always reports "free'd" */

#endif /* SYNCDUKE_SDL_SHIM_H_ */
