#include <stdlib.h>	/* getenv()/exit()/atexit() */
#include <stdio.h>	/* NULL */

#include <SDL.h>
#ifndef main
 #define USE_REAL_MAIN
#endif
#include "gen_defs.h"
#include "threadwrap.h"
#ifdef USE_REAL_MAIN
 #undef main
#endif
#include "sdlfuncs.h"

#ifndef _WIN32
struct sdlfuncs sdl;
#endif

/* Make xp_dl do static linking */
#ifdef STATIC_SDL
#define STATIC_LINK
#endif
#include "xp_dl.h"

static int sdl_funcs_loaded=0;
static int sdl_initialized=0;
static int sdl_audio_initialized=0;

int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	dll_handle	sdl_dll;
	const char *libnames[]={"SDL", "SDL-1.2", "SDL-1.1", NULL};

	putenv("SDL_VIDEO_ALLOW_SCREENSAVER=1");
	sdlf->gotfuncs=0;
	if((sdl_dll=xp_dlopen(libnames,RTLD_LAZY|RTLD_GLOBAL,SDL_PATCHLEVEL))==NULL)
		return(-1);

	if((sdlf->Init=xp_dlsym(sdl_dll, SDL_Init))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->Quit=xp_dlsym(sdl_dll, SDL_Quit))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SemWait=xp_dlsym(sdl_dll, SDL_SemWait))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SemPost=xp_dlsym(sdl_dll, SDL_SemPost))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_CreateSemaphore=xp_dlsym(sdl_dll, SDL_CreateSemaphore))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_DestroySemaphore=xp_dlsym(sdl_dll, SDL_DestroySemaphore))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->InitSubSystem=xp_dlsym(sdl_dll, SDL_InitSubSystem))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->QuitSubSystem=xp_dlsym(sdl_dll, SDL_QuitSubSystem))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->OpenAudio=xp_dlsym(sdl_dll, SDL_OpenAudio))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CloseAudio=xp_dlsym(sdl_dll, SDL_CloseAudio))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->PauseAudio=xp_dlsym(sdl_dll, SDL_PauseAudio))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->LockAudio=xp_dlsym(sdl_dll, SDL_LockAudio))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockAudio=xp_dlsym(sdl_dll, SDL_UnlockAudio))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetAudioStatus=xp_dlsym(sdl_dll, SDL_GetAudioStatus))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	sdlf->gotfuncs=1;
	sdl_funcs_loaded=1;
	return(0);
}

int init_sdl_audio(void)
{
	if (!sdl_funcs_loaded) {
		if (load_sdl_funcs(&sdl) != 0)
			return -1;
	}
	if(!sdl_initialized) {
		if(sdl.Init(0)==0)
			sdl_initialized=TRUE;
		else
			return(-1);
	}
	if(sdl_audio_initialized)
		return(0);
	if(sdl.InitSubSystem(SDL_INIT_AUDIO)==0) {
		sdl_audio_initialized=TRUE;
		return(0);
	}
	return(-1);
}
