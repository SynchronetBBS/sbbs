#include <stdlib.h>	/* getenv()/exit()/atexit() */
#include <stdio.h>	/* NULL */

#include "gen_defs.h"
#undef main
#include "sdlfuncs.h"

#ifndef _WIN32
struct sdlfuncs sdl;
#endif

static int sdl_funcs_loaded=0;
static int sdl_initialized=0;
static int sdl_audio_initialized=0;
static int sdl_video_initialized=0;

int XPDEV_main(int argc, char **argv, char **enviro);

#ifdef STATIC_SDL
int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	sdlf->gotfuncs=0;
	sdlf->Init=SDL_Init;
	sdlf->Quit=SDL_Quit;
#ifdef _WIN32
	sdlf->SetModuleHandle=SDL_SetModuleHandle;
#endif
	sdlf->mutexP=SDL_mutexP;
	sdlf->mutexV=SDL_mutexV;
	sdlf->PeepEvents=SDL_PeepEvents;
	sdlf->VideoDriverName=SDL_VideoDriverName;
	sdlf->SemWait=SDL_SemWait;
	sdlf->SemPost=SDL_SemPost;
	sdlf->EventState=SDL_EventState;
	sdlf->CreateRGBSurface=SDL_CreateRGBSurface;
	sdlf->FillRect=SDL_FillRect;
	sdlf->SetColors=SDL_SetColors;
	sdlf->BlitSurface=SDL_UpperBlit;
	sdlf->UpdateRects=SDL_UpdateRects;
	sdlf->SDL_CreateSemaphore=SDL_CreateSemaphore;
	sdlf->SDL_DestroySemaphore=SDL_DestroySemaphore;
	sdlf->SDL_CreateMutex=SDL_CreateMutex;
	sdlf->CreateThread=SDL_CreateThread;
	sdlf->WaitEvent=SDL_WaitEvent;
	sdlf->SetVideoMode=SDL_SetVideoMode;
	sdlf->FreeSurface=SDL_FreeSurface;
	sdlf->WM_SetCaption=SDL_WM_SetCaption;
	sdlf->ShowCursor=SDL_ShowCursor;
	sdlf->WasInit=SDL_WasInit;
	sdlf->EnableUNICODE=SDL_EnableUNICODE;
	sdlf->EnableKeyRepeat=SDL_EnableKeyRepeat;
	sdlf->GetWMInfo=SDL_GetWMInfo;
	sdlf->GetError=SDL_GetError;
	sdlf->InitSubSystem=SDL_InitSubSystem;
	sdlf->QuitSubSystem=SDL_QuitSubSystem;
	sdlf->OpenAudio=SDL_OpenAudio;
	sdlf->CloseAudio=SDL_CloseAudio;
	sdlf->PauseAudio=SDL_PauseAudio;
	sdlf->LockAudio=SDL_LockAudio;
	sdlf->UnlockAudio=SDL_UnlockAudio;
	sdlf->gotfuncs=1;
	sdl_funcs_loaded=1;
	return(0);
}
#else

#if defined(_WIN32)
#include <Windows.h>

int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	HMODULE	sdl_dll;

	sdlf->gotfuncs=0;
	if((sdl_dll=LoadLibrary("SDL.dll"))==NULL)
		if((sdl_dll=LoadLibrary("SDL-1.2.dll"))==NULL)
			if((sdl_dll=LoadLibrary("SDL-1.1.dll"))==NULL)
				return(-1);

	if((sdlf->Init=GetProcAddress(sdl_dll, "SDL_Init"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->Quit=GetProcAddress(sdl_dll, "SDL_Quit"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SetModuleHandle=GetProcAddress(sdl_dll, "SDL_SetModuleHandle"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->mutexP=GetProcAddress(sdl_dll, "SDL_mutexP"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->mutexV=GetProcAddress(sdl_dll, "SDL_mutexV"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->PeepEvents=GetProcAddress(sdl_dll, "SDL_PeepEvents"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->VideoDriverName=GetProcAddress(sdl_dll, "SDL_VideoDriverName"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SemWait=GetProcAddress(sdl_dll, "SDL_SemWait"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SemPost=GetProcAddress(sdl_dll, "SDL_SemPost"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->EventState=GetProcAddress(sdl_dll, "SDL_EventState"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateRGBSurface=GetProcAddress(sdl_dll, "SDL_CreateRGBSurface"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->FillRect=GetProcAddress(sdl_dll, "SDL_FillRect"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SetColors=GetProcAddress(sdl_dll, "SDL_SetColors"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->BlitSurface=GetProcAddress(sdl_dll, "SDL_UpperBlit"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->UpdateRects=GetProcAddress(sdl_dll, "SDL_UpdateRects"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_CreateSemaphore=GetProcAddress(sdl_dll, "SDL_CreateSemaphore"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_DestroySemaphore=GetProcAddress(sdl_dll, "SDL_DestroySemaphore"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_CreateMutex=GetProcAddress(sdl_dll, "SDL_CreateMutex"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateThread=GetProcAddress(sdl_dll, "SDL_CreateThread"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->WaitEvent=GetProcAddress(sdl_dll, "SDL_WaitEvent"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SetVideoMode=GetProcAddress(sdl_dll, "SDL_SetVideoMode"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->FreeSurface=GetProcAddress(sdl_dll, "SDL_FreeSurface"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->WM_SetCaption=GetProcAddress(sdl_dll, "SDL_WM_SetCaption"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->ShowCursor=GetProcAddress(sdl_dll, "SDL_ShowCursor"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->WasInit=GetProcAddress(sdl_dll, "SDL_WasInit"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->EnableUNICODE=GetProcAddress(sdl_dll, "SDL_EnableUNICODE"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->EnableKeyRepeat=GetProcAddress(sdl_dll, "SDL_EnableKeyRepeat"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->GetWMInfo=GetProcAddress(sdl_dll, "SDL_GetWMInfo"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->GetError=GetProcAddress(sdl_dll, "SDL_GetError"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->InitSubSystem=GetProcAddress(sdl_dll, "SDL_InitSubSystem"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->QuitSubSystem=GetProcAddress(sdl_dll, "SDL_QuitSubSystem"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->OpenAudio=GetProcAddress(sdl_dll, "SDL_OpenAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->CloseAudio=GetProcAddress(sdl_dll, "SDL_CloseAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->PauseAudio=GetProcAddress(sdl_dll, "SDL_PauseAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->LockAudio=GetProcAddress(sdl_dll, "SDL_LockAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockAudio=GetProcAddress(sdl_dll, "SDL_UnlockAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	sdlf->gotfuncs=1;
	sdl_funcs_loaded=1;
	return(0);
}
#elif defined(__unix__)
#include <dlfcn.h>

int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	void	*sdl_dll;

	sdlf->gotfuncs=0;
	if((sdl_dll=dlopen("libSDL.so",RTLD_LAZY|RTLD_GLOBAL))==NULL)
		if((sdl_dll=dlopen("libSDL-1.2.so",RTLD_LAZY|RTLD_GLOBAL))==NULL)
			if((sdl_dll=dlopen("libSDL-1.1.so",RTLD_LAZY|RTLD_GLOBAL))==NULL)
				return(-1);

	if((sdlf->Init=dlsym(sdl_dll, "SDL_Init"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->Quit=dlsym(sdl_dll, "SDL_Quit"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->mutexP=dlsym(sdl_dll, "SDL_mutexP"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->mutexV=dlsym(sdl_dll, "SDL_mutexV"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->PeepEvents=dlsym(sdl_dll, "SDL_PeepEvents"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->VideoDriverName=dlsym(sdl_dll, "SDL_VideoDriverName"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SemWait=dlsym(sdl_dll, "SDL_SemWait"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SemPost=dlsym(sdl_dll, "SDL_SemPost"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->EventState=dlsym(sdl_dll, "SDL_EventState"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateRGBSurface=dlsym(sdl_dll, "SDL_CreateRGBSurface"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->FillRect=dlsym(sdl_dll, "SDL_FillRect"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetColors=dlsym(sdl_dll, "SDL_SetColors"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->BlitSurface=dlsym(sdl_dll, "SDL_UpperBlit"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UpdateRects=dlsym(sdl_dll, "SDL_UpdateRects"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_CreateSemaphore=dlsym(sdl_dll, "SDL_CreateSemaphore"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_DestroySemaphore=dlsym(sdl_dll, "SDL_DestroySemaphore"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_CreateMutex=dlsym(sdl_dll, "SDL_CreateMutex"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateThread=dlsym(sdl_dll, "SDL_CreateThread"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WaitEvent=dlsym(sdl_dll, "SDL_WaitEvent"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetVideoMode=dlsym(sdl_dll, "SDL_SetVideoMode"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->FreeSurface=dlsym(sdl_dll, "SDL_FreeSurface"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WM_SetCaption=dlsym(sdl_dll, "SDL_WM_SetCaption"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->ShowCursor=dlsym(sdl_dll, "SDL_ShowCursor"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WasInit=dlsym(sdl_dll, "SDL_WasInit"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->EnableUNICODE=dlsym(sdl_dll, "SDL_EnableUNICODE"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->EnableKeyRepeat=dlsym(sdl_dll, "SDL_EnableKeyRepeat"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetWMInfo=dlsym(sdl_dll, "SDL_GetWMInfo"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetError=dlsym(sdl_dll, "SDL_GetError"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->InitSubSystem=dlsym(sdl_dll, "SDL_InitSubSystem"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->QuitSubSystem=dlsym(sdl_dll, "SDL_QuitSubSystem"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->OpenAudio=dlsym(sdl_dll, "SDL_OpenAudio"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CloseAudio=dlsym(sdl_dll, "SDL_CloseAudio"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->PauseAudio=dlsym(sdl_dll, "SDL_PauseAudio"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->LockAudio=dlsym(sdl_dll, "SDL_LockAudio"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockAudio=dlsym(sdl_dll, "SDL_UnlockAudio"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	sdlf->gotfuncs=1;
	sdl_funcs_loaded=1;
	return(0);
}
#endif

#endif

int init_sdl_video(void)
{
	/* This is all handled in SDL_main_env() */
	if(sdl_video_initialized)
		return(0);
	else
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

/* atexit() function */
static void sdl_exit(void)
{
	sdl.Quit();
}

#ifndef main
int main(int argc, char **argv, char **env)
#else
int SDL_main_env(int argc, char **argv, char **env)
#endif
{
	unsigned int i;
	SDL_Event	ev;
	char	drivername[64];

#ifndef _WIN32
	load_sdl_funcs(&sdl);
#endif

	if(sdl.gotfuncs) {
#ifdef _WIN32
		/* Fail to windib (ie: No mouse attached) */
		if(sdl.Init(SDL_INIT_VIDEO)) {
			if(getenv("SDL_VIDEODRIVER")==NULL) {
				putenv("SDL_VIDEODRIVER=windib");
				WinExec(GetCommandLine(), SW_SHOWDEFAULT);
				exit(0);
			}
			sdl.gotfuncs=FALSE;
		}
		else {
			sdl_video_initialized=TRUE;
		}
#else

		/*
		 * On Linux, SDL doesn't properly detect availability of the
		 * framebuffer apparently.  This results in remote connections
		 * displaying on the local framebuffer... a definate no-no.
		 * This ugly hack attempts to prevent this... of course, remote X11
		 * connections must still be allowed.
		 */
		if(getenv("REMOTEHOST")!=NULL && getenv("DISPLAY")==NULL) {
			/* Sure ,we can't use video, but audio is still valid! */
			if(sdl.Init(0)==0)
				sdl_initialized=TRUE;
			sdl.gotfuncs=FALSE;
		}
		else {
			if(sdl.Init(SDL_INIT_VIDEO))
				sdl.gotfuncs=FALSE;
			else {
				sdl_initialized=TRUE;
				sdl_video_initialized=TRUE;
			}
		}
#endif
		if(sdl_video_initialized && sdl.VideoDriverName(drivername, sizeof(drivername))!=NULL) {
			/* Unacceptable drivers */
			if((!strcmp(drivername, "caca")) || (!strcmp(drivername,"aalib")) || (!strcmp(drivername,"dummy"))) {
				sdl.gotfuncs=FALSE;
				sdl.QuitSubSystem(SDL_INIT_VIDEO);
				sdl_video_initialized=FALSE;
			}
			else {
				sdl_video_initialized=TRUE;
			}
		}
	}
	if(sdl_initialized)
		atexit(sdl_exit);
	return(XPDEV_main(argc, argv, env));
}
