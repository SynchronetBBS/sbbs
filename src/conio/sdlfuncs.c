#include <stdlib.h>	/* getenv()/exit()/atexit() */
#include <stdio.h>	/* NULL */
#ifdef __unix__
#include <dlfcn.h>
#endif

#include <SDL.h>
#ifndef main
 #define USE_REAL_MAIN
#endif
#include "gen_defs.h"
#ifdef USE_REAL_MAIN
 #undef main
#endif
#include "sdlfuncs.h"

#ifndef _WIN32
struct sdlfuncs sdl;
#endif

static int sdl_funcs_loaded=0;
static int sdl_initialized=0;
static int sdl_audio_initialized=0;
static int sdl_video_initialized=0;
static int (*sdl_drawing_thread)(void *data)=NULL;
static void (*sdl_exit_drawing_thread)(void)=NULL;
static int main_returned=0;
static SDL_sem *sdl_main_sem;
SDL_sem *sdl_exit_sem;

int CIOLIB_main(int argc, char **argv, char **enviro);

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
	sdlf->CreateRGBSurfaceFrom=SDL_CreateRGBSurfaceFrom;
	sdlf->FillRect=SDL_FillRect;
	sdlf->SetColors=SDL_SetColors;
	sdlf->BlitSurface=SDL_UpperBlit;
	sdlf->UpdateRects=SDL_UpdateRects;
	sdlf->UpdateRect=SDL_UpdateRect;
	sdlf->SDL_CreateSemaphore=SDL_CreateSemaphore;
	sdlf->SDL_DestroySemaphore=SDL_DestroySemaphore;
	sdlf->SDL_CreateMutex=SDL_CreateMutex;
	sdlf->CreateThread=SDL_CreateThread;
	sdlf->KillThread=SDL_KillThread;
	sdlf->WaitThread=SDL_WaitThread;
	sdlf->WaitEvent=SDL_WaitEvent;
	sdlf->SetVideoMode=SDL_SetVideoMode;
	sdlf->FreeSurface=SDL_FreeSurface;
	sdlf->WM_SetCaption=SDL_WM_SetCaption;
	sdlf->WM_SetIcon=SDL_WM_SetIcon;
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
	sdlf->GetAudioStatus=SDL_GetAudioStatus;
	sdlf->MapRGB=SDL_MapRGB;
	sdlf->LockSurface=SDL_LockSurface;
	sdlf->UnlockSurface=SDL_UnlockSurface;
	sdlf->DisplayFormat=SDL_DisplayFormat;
	sdlf->Flip=SDL_Flip;
	sdlf->CreateYUVOverlay=SDL_CreateYUVOverlay;
	sdlf->DisplayYUVOverlay=SDL_DisplayYUVOverlay;
	sdlf->FreeYUVOverlay=SDL_FreeYUVOverlay;
	sdlf->LockYUVOverlay=SDL_LockYUVOverlay;
	sdlf->UnlockYUVOverlay=SDL_UnlockYUVOverlay;
	sdlf->GetVideoInfo=SDL_GetVideoInfo;
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

	if((sdlf->Init=(void *)GetProcAddress(sdl_dll, "SDL_Init"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->Quit=(void *)GetProcAddress(sdl_dll, "SDL_Quit"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SetModuleHandle=(void *)GetProcAddress(sdl_dll, "SDL_SetModuleHandle"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->mutexP=(void *)GetProcAddress(sdl_dll, "SDL_mutexP"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->mutexV=(void *)GetProcAddress(sdl_dll, "SDL_mutexV"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->PeepEvents=(void *)GetProcAddress(sdl_dll, "SDL_PeepEvents"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->VideoDriverName=(void *)GetProcAddress(sdl_dll, "SDL_VideoDriverName"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SemWait=(void *)GetProcAddress(sdl_dll, "SDL_SemWait"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SemPost=(void *)GetProcAddress(sdl_dll, "SDL_SemPost"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->EventState=(void *)GetProcAddress(sdl_dll, "SDL_EventState"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateRGBSurface=(void *)GetProcAddress(sdl_dll, "SDL_CreateRGBSurface"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateRGBSurfaceFrom=(void *)GetProcAddress(sdl_dll, "SDL_CreateRGBSurfaceFrom"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->FillRect=(void *)GetProcAddress(sdl_dll, "SDL_FillRect"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SetColors=(void *)GetProcAddress(sdl_dll, "SDL_SetColors"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->BlitSurface=(void *)GetProcAddress(sdl_dll, "SDL_UpperBlit"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->UpdateRects=(void *)GetProcAddress(sdl_dll, "SDL_UpdateRects"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->UpdateRect=(void *)GetProcAddress(sdl_dll, "SDL_UpdateRect"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_CreateSemaphore=(void *)GetProcAddress(sdl_dll, "SDL_CreateSemaphore"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_DestroySemaphore=(void *)GetProcAddress(sdl_dll, "SDL_DestroySemaphore"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SDL_CreateMutex=(void *)GetProcAddress(sdl_dll, "SDL_CreateMutex"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateThread=(void *)GetProcAddress(sdl_dll, "SDL_CreateThread"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->KillThread=(void *)GetProcAddress(sdl_dll, "SDL_KillThread"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->WaitThread=(void *)GetProcAddress(sdl_dll, "SDL_WaitThread"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->WaitEvent=(void *)GetProcAddress(sdl_dll, "SDL_WaitEvent"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->SetVideoMode=(void *)GetProcAddress(sdl_dll, "SDL_SetVideoMode"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->FreeSurface=(void *)GetProcAddress(sdl_dll, "SDL_FreeSurface"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->WM_SetCaption=(void *)GetProcAddress(sdl_dll, "SDL_WM_SetCaption"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->WM_SetIcon=(void *)GetProcAddress(sdl_dll, "SDL_WM_SetIcon"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->ShowCursor=(void *)GetProcAddress(sdl_dll, "SDL_ShowCursor"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->WasInit=(void *)GetProcAddress(sdl_dll, "SDL_WasInit"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->EnableUNICODE=(void *)GetProcAddress(sdl_dll, "SDL_EnableUNICODE"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->EnableKeyRepeat=(void *)GetProcAddress(sdl_dll, "SDL_EnableKeyRepeat"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->GetWMInfo=(void *)GetProcAddress(sdl_dll, "SDL_GetWMInfo"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->GetError=(void *)GetProcAddress(sdl_dll, "SDL_GetError"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->InitSubSystem=(void *)GetProcAddress(sdl_dll, "SDL_InitSubSystem"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->QuitSubSystem=(void *)GetProcAddress(sdl_dll, "SDL_QuitSubSystem"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->OpenAudio=(void *)GetProcAddress(sdl_dll, "SDL_OpenAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->CloseAudio=(void *)GetProcAddress(sdl_dll, "SDL_CloseAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->PauseAudio=(void *)GetProcAddress(sdl_dll, "SDL_PauseAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->LockAudio=(void *)GetProcAddress(sdl_dll, "SDL_LockAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockAudio=(void *)GetProcAddress(sdl_dll, "SDL_UnlockAudio"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->GetAudioStatus=(void *)GetProcAddress(sdl_dll, "SDL_GetAudioStatus"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->MapRGB=(void *)GetProcAddress(sdl_dll, "SDL_MapRGB"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->LockSurface=(void *)GetProcAddress(sdl_dll, "SDL_LockSurface"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockSurface=(void *)GetProcAddress(sdl_dll, "SDL_UnlockSurface"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->DisplayFormat=(void *)GetProcAddress(sdl_dll, "SDL_DisplayFormat"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->Flip=(void *)GetProcAddress(sdl_dll, "SDL_Flip"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateYUVOverlay=(void *)GetProcAddress(sdl_dll, "SDL_CreateYUVOverlay"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->DisplayYUVOverlay=(void *)GetProcAddress(sdl_dll, "SDL_DisplayYUVOverlay"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->FreeYUVOverlay=(void *)GetProcAddress(sdl_dll, "SDL_FreeYUVOverlay"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->LockYUVOverlay=(void *)GetProcAddress(sdl_dll, "SDL_LockYUVOverlay"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockYUVOverlay=(void *)GetProcAddress(sdl_dll, "SDL_UnlockYUVOverlay"))==NULL) {
		FreeLibrary(sdl_dll);
		return(-1);
	}
	if((sdlf->GetVideoInfo=(void *)GetProcAddress(sdl_dll, "SDL_GetVideoInfo"))==NULL) {
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
	if((sdlf->CreateRGBSurfaceFrom=dlsym(sdl_dll, "SDL_CreateRGBSurfaceFrom"))==NULL) {
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
	if((sdlf->UpdateRect=dlsym(sdl_dll, "SDL_UpdateRect"))==NULL) {
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
	if((sdlf->KillThread=dlsym(sdl_dll, "SDL_KillThread"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WaitThread=dlsym(sdl_dll, "SDL_WaitThread"))==NULL) {
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
	if((sdlf->WM_SetIcon=dlsym(sdl_dll, "SDL_WM_SetIcon"))==NULL) {
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
	if((sdlf->GetAudioStatus=dlsym(sdl_dll, "SDL_GetAudioStatus"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->MapRGB=dlsym(sdl_dll, "SDL_MapRGB"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->LockSurface=dlsym(sdl_dll, "SDL_LockSurface"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockSurface=dlsym(sdl_dll, "SDL_UnlockSurface"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->DisplayFormat=dlsym(sdl_dll, "SDL_DisplayFormat"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->Flip=dlsym(sdl_dll, "SDL_Flip"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateYUVOverlay=dlsym(sdl_dll, "SDL_CreateYUVOverlay"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->DisplayYUVOverlay=dlsym(sdl_dll, "SDL_DisplayYUVOverlay"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->FreeYUVOverlay=dlsym(sdl_dll, "SDL_FreeYUVOverlay"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->LockYUVOverlay=dlsym(sdl_dll, "SDL_LockYUVOverlay"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockYUVOverlay=dlsym(sdl_dll, "SDL_UnlockYUVOverlay"))==NULL) {
		dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetVideoInfo=dlsym(sdl_dll, "SDL_GetVideoInfo"))==NULL) {
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

struct main_args {
	int		argc;
	char	**argv;
	char	**enviro;
};

static int sdl_run_main(void *data)
{
	struct main_args	*args;
	int	ret;

	args=data;
	ret=CIOLIB_main(args->argc, args->argv, args->enviro);
	main_returned=1;
	sdl.SemPost(sdl_main_sem);
	if(sdl_exit_drawing_thread!=NULL)
		sdl_exit_drawing_thread();
	sdl.SemPost(sdl_exit_sem);
	return(ret);
}

void run_sdl_drawing_thread(int (*drawing_thread)(void *data), void (*exit_drawing_thread)(void))
{
	sdl_drawing_thread=drawing_thread;
	sdl_exit_drawing_thread=exit_drawing_thread;
	sdl.SemPost(sdl_main_sem);
}

#ifndef main
int main(int argc, char **argv, char **env)
#else
int SDL_main_env(int argc, char **argv, char **env)
#endif
{
	char	drivername[64];
	struct main_args ma;
	SDL_Thread	*main_thread;
	int		main_ret;
	int		use_sdl_video=FALSE;

	ma.argc=argc;
	ma.argv=argv;
	ma.enviro=env;
#ifndef _WIN32
	load_sdl_funcs(&sdl);
#endif

	if(sdl.gotfuncs) {
		use_sdl_video=TRUE;

#ifdef _WIN32
		/* Fail to windib (ie: No mouse attached) */
		if(sdl.Init(SDL_INIT_VIDEO)) {
			if(getenv("SDL_VIDEODRIVER")==NULL) {
				putenv("SDL_VIDEODRIVER=windib");
				WinExec(GetCommandLine(), SW_SHOWDEFAULT);
				return(0);
			}
			/* Sure ,we can't use video, but audio is still valid! */
			if(sdl.Init(0)==0)
				sdl_initialized=TRUE;
		}
		else {
			sdl_video_initialized=TRUE;
			sdl_initialized=TRUE;
		}
#else
		/*
		 * On Linux, SDL doesn't properly detect availability of the
		 * framebuffer apparently.  This results in remote connections
		 * displaying on the local framebuffer... a definate no-no.
		 * This ugly hack attempts to prevent this... of course, remote X11
		 * connections must still be allowed.
		 */
		if((!use_sdl_video) || (getenv("REMOTEHOST")!=NULL && getenv("DISPLAY")==NULL)) {
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
		if(sdl_video_initialized && sdl.VideoDriverName(drivername, sizeof(drivername))!=NULL) {
			/* Unacceptable drivers */
			if((!strcmp(drivername, "caca")) || (!strcmp(drivername,"aalib")) || (!strcmp(drivername,"dummy"))) {
				sdl.QuitSubSystem(SDL_INIT_VIDEO);
				sdl_video_initialized=FALSE;
			}
			else {
				const SDL_VideoInfo *initial=sdl.GetVideoInfo();

				/* Save initial video mode */
				if(initial)
					sdl.initial_videoinfo=*initial;
				else
					memset(&sdl.initial_videoinfo, 0, sizeof(sdl.initial_videoinfo));
				sdl_video_initialized=TRUE;
			}
		}
	}
	if(sdl_video_initialized) {
		atexit(sdl.Quit);
		sdl_main_sem=sdl.SDL_CreateSemaphore(0);
		sdl_exit_sem=sdl.SDL_CreateSemaphore(0);
		main_thread=sdl.CreateThread(sdl_run_main,&ma);
		sdl.SemWait(sdl_main_sem);
		if(sdl_drawing_thread!=NULL) {
			sdl_drawing_thread(NULL);
			sdl_exit_drawing_thread=NULL;
			if(!main_returned) {
				main_ret=0;
			}
		}
		sdl.SemWait(sdl_exit_sem);
		if(main_returned)
			sdl.WaitThread(main_thread, &main_ret);
	}
	else
		main_ret=CIOLIB_main(argc, argv, env);
	return(main_ret);
}
