#include <stdio.h>	/* NULL */

#include "sdlfuncs.h"

#ifdef STATIC_SDL
int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	sdlf->gotfuncs=0;
	sdlf->Init=SDL_Init;
	sdlf->Quit=SDL_Quit;
	sdlf->SetModuleHandle=SDL_SetModuleHandle;
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
	sdlf->gotfuncs=1;
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
	sdlf->gotfuncs=1;
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
	sdlf->gotfuncs=1;
	return(0);
}
#endif

#endif
