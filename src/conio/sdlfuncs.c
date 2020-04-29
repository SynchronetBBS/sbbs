#include <stdlib.h>	/* getenv()/exit()/atexit() */
#include <stdio.h>	/* NULL */

#include "gen_defs.h"
#include "threadwrap.h"
#include <SDL.h>
#include "sdlfuncs.h"
#include "sdl_con.h"
extern int sdl_video_initialized;

struct sdlfuncs sdl;

/* Make xp_dl do static linking */
#ifdef STATIC_SDL
#define STATIC_LINK
#endif
#include <xp_dl.h>

#include "ciolib.h"

static int sdl_funcs_loaded=0;
static int sdl_initialized=0;
static int sdl_audio_initialized=0;

static void QuitWrap(void);

int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	dll_handle	sdl_dll;
	const char *libnames[]={"SDL2", "SDL", NULL};

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
	if((sdlf->PeepEvents=xp_dlsym(sdl_dll, SDL_PeepEvents))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetCurrentVideoDriver=xp_dlsym(sdl_dll, SDL_GetCurrentVideoDriver))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->EventState=xp_dlsym(sdl_dll, SDL_EventState))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateRGBSurfaceFrom=xp_dlsym(sdl_dll, SDL_CreateRGBSurfaceFrom))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->RenderPresent=xp_dlsym(sdl_dll, SDL_RenderPresent))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WaitEventTimeout=xp_dlsym(sdl_dll, SDL_WaitEventTimeout))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateWindowAndRenderer=xp_dlsym(sdl_dll, SDL_CreateWindowAndRenderer))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->FreeSurface=xp_dlsym(sdl_dll, SDL_FreeSurface))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetWindowTitle=xp_dlsym(sdl_dll, SDL_SetWindowTitle))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetWindowSize=xp_dlsym(sdl_dll, SDL_GetWindowSize))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetWindowIcon=xp_dlsym(sdl_dll, SDL_SetWindowIcon))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->ShowCursor=xp_dlsym(sdl_dll, SDL_ShowCursor))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WasInit=xp_dlsym(sdl_dll, SDL_WasInit))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetWindowWMInfo=xp_dlsym(sdl_dll, SDL_GetWindowWMInfo))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetError=xp_dlsym(sdl_dll, SDL_GetError))==NULL) {
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
	if((sdlf->CreateTexture=xp_dlsym(sdl_dll, SDL_CreateTexture))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UpdateTexture=xp_dlsym(sdl_dll, SDL_UpdateTexture))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->RenderClear=xp_dlsym(sdl_dll, SDL_RenderClear))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->RenderCopy=xp_dlsym(sdl_dll, SDL_RenderCopy))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetHint=xp_dlsym(sdl_dll, SDL_SetHint))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetHint=xp_dlsym(sdl_dll, SDL_GetHint))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetModState=xp_dlsym(sdl_dll, SDL_GetModState))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetWindowSize=xp_dlsym(sdl_dll, SDL_SetWindowSize))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->DestroyTexture=xp_dlsym(sdl_dll, SDL_DestroyTexture))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetWindowFullscreen=xp_dlsym(sdl_dll, SDL_SetWindowFullscreen))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->LockTexture=xp_dlsym(sdl_dll, SDL_LockTexture))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockTexture=xp_dlsym(sdl_dll, SDL_UnlockTexture))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->QueryTexture=xp_dlsym(sdl_dll, SDL_QueryTexture))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetWindowPosition=xp_dlsym(sdl_dll, SDL_GetWindowPosition))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetWindowPosition=xp_dlsym(sdl_dll, SDL_SetWindowPosition))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetWindowMinimumSize=xp_dlsym(sdl_dll, SDL_SetWindowMinimumSize))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetClipboardText=xp_dlsym(sdl_dll, SDL_SetClipboardText))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetClipboardText=xp_dlsym(sdl_dll, SDL_GetClipboardText))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->free=xp_dlsym(sdl_dll, SDL_free))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
#ifndef STATIC_SDL
	{
		int (HACK_HACK_HACK *ra)(char *name, Uint32 style, void *hInst);
		if ((ra = xp_dlsym(sdl_dll, SDL_RegisterApp)) != NULL) {
			ra(ciolib_appname, 0, NULL);
		}
	}
#endif

	sdlf->gotfuncs=1;
	sdl_funcs_loaded=1;
	return(0);
}

int init_sdl_video(void)
{
	char	*drivername;
	int		use_sdl_video=FALSE;
#ifdef _WIN32
	char		*driver_env=NULL;
#endif

	if(sdl_video_initialized)
		return(0);

	load_sdl_funcs(&sdl);

	if (!sdl.gotfuncs)
		return -1;

	use_sdl_video=TRUE;

	sdl.SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2" );
	sdl.SetHint(SDL_HINT_VIDEO_ALLOW_SCREENSAVER, "1" );
#ifdef _WIN32
	/* Fail to windib (ie: No mouse attached) */
	if(sdl.Init(SDL_INIT_VIDEO)) {
		if(sdl.Init(0)==0)
			sdl_initialized=TRUE;
	}
	else {
		sdl_video_initialized=TRUE;
		sdl_initialized=TRUE;
	}
#else
	/*
	 * SDL2: Is the below comment still true for SDL2?
	 * On Linux, SDL doesn't properly detect availability of the
	 * framebuffer apparently.  This results in remote connections
	 * displaying on the local framebuffer... a definate no-no.
	 * This ugly hack attempts to prevent this... of course, remote X11
	 * connections must still be allowed.
	 */
	if((!use_sdl_video) || ((getenv("REMOTEHOST")!=NULL || getenv("SSH_CLIENT")!=NULL) && getenv("DISPLAY")==NULL)) {
		/* Sure ,we can't use video, but audio is still valid! */
		if(sdl.Init(0)==0)
			sdl_initialized=TRUE;
	}
	else {
		if(sdl.Init(SDL_INIT_VIDEO)==0) {
			sdl_initialized=TRUE;
			sdl_video_initialized=TRUE;
		}
		else {
			/* Sure ,we can't use video, but audio is still valid! */
			if(sdl.Init(0)==0)
				sdl_initialized=TRUE;
		}
	}
#endif
	if(sdl_video_initialized && (drivername = sdl.GetCurrentVideoDriver())!=NULL) {
		/* Unacceptable drivers */
		if((!strcmp(drivername, "caca")) || (!strcmp(drivername,"aalib")) || (!strcmp(drivername,"dummy"))) {
			sdl.QuitSubSystem(SDL_INIT_VIDEO);
			sdl_video_initialized=FALSE;
		}
		else {
			sdl_video_initialized=TRUE;
		}
	}

	if(sdl_video_initialized) {
		atexit(QuitWrap);
		return 0;
	}

	return(-1);
}

int init_sdl_audio(void)
{
	if(!sdl_initialized)
		return(-1);
	if(sdl_audio_initialized)
		return(0);
	if(sdl.InitSubSystem(SDL_INIT_AUDIO)==0) {
		sdl_audio_initialized=TRUE;
		return(0);
	}
	return(-1);
}

static void QuitWrap(void)
{
	if (sdl_initialized) {
#if !defined(__DARWIN__)
		exit_sdl_con();
#endif
		if(sdl.Quit)
			sdl.Quit();
	}
}
