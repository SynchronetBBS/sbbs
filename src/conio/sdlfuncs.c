#include <stdio.h>	/* NULL */

#include "sdlfuncs.h"

#ifdef STATIC_SDL
int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	sdlf->Init=SDL_Init;
	sdlf->Quit=SDL_Quit;
	sdlf->RegisterApp=SDL_RegisterApp;
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
	return(0);
}
#else

#if defined(_WIN32)
#include <Windows.h>

int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	HMODULE	dl;

	if((dl=LoadLibrary("SDL.dll"))==NULL)
		if((dl=LoadLibrary("SDL-1.2.dll"))==NULL)
			if((dl=LoadLibrary("SDL-1.1.dll"))==NULL)
				return(-1);

	if((sdlf->Init=GetProcAddress(dl, "SDL_Init"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->Quit=GetProcAddress(dl, "SDL_Quit"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->RegisterApp=GetProcAddress(dl, "SDL_RegisterApp"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->SetModuleHandle=GetProcAddress(dl, "SDL_SetModuleHandle"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->mutexP=GetProcAddress(dl, "SDL_mutexP"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->mutexV=GetProcAddress(dl, "SDL_mutexV"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->PeepEvents=GetProcAddress(dl, "SDL_PeepEvents"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->VideoDriverName=GetProcAddress(dl, "SDL_VideoDriverName"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->SemWait=GetProcAddress(dl, "SDL_SemWait"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->SemPost=GetProcAddress(dl, "SDL_SemPost"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->EventState=GetProcAddress(dl, "SDL_EventState"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->CreateRGBSurface=GetProcAddress(dl, "SDL_CreateRGBSurface"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->FillRect=GetProcAddress(dl, "SDL_FillRect"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->SetColors=GetProcAddress(dl, "SDL_SetColors"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->BlitSurface=GetProcAddress(dl, "SDL_UpperBlit"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->UpdateRects=GetProcAddress(dl, "SDL_UpdateRects"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->SDL_CreateSemaphore=GetProcAddress(dl, "SDL_CreateSemaphore"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->SDL_CreateMutex=GetProcAddress(dl, "SDL_CreateMutex"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->CreateThread=GetProcAddress(dl, "SDL_CreateThread"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->WaitEvent=GetProcAddress(dl, "SDL_WaitEvent"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->SetVideoMode=GetProcAddress(dl, "SDL_SetVideoMode"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->FreeSurface=GetProcAddress(dl, "SDL_FreeSurface"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->WM_SetCaption=GetProcAddress(dl, "SDL_WM_SetCaption"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->ShowCursor=GetProcAddress(dl, "SDL_ShowCursor"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->WasInit=GetProcAddress(dl, "SDL_WasInit"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->EnableUNICODE=GetProcAddress(dl, "SDL_EnableUNICODE"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->EnableKeyRepeat=GetProcAddress(dl, "SDL_EnableKeyRepeat"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	if((sdlf->GetWMInfo=GetProcAddress(dl, "SDL_GetWMInfo"))==NULL) {
		FreeLibrary(dl);
		return(-1);
	}
	return(0);
}
#elif defined(__unix__)
#include <dlfcn.h>

int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	void	*dl;

	if((dl=dlopen("libSDL.so",RTLD_LAZY|HTLD_GLOBAL))==NULL)
		if((dl=dlopen("libSDL-1.2.so",RTLD_LAZY|HTLD_GLOBAL))==NULL)
			if((dl=dlopen("libSDL-1.1.so",RTLD_LAZY|HTLD_GLOBAL))==NULL)
				return(-1);

	if((sdlf->Init=dlsym(dl, "SDL_Init"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->Quit=dlsym(dl, "SDL_Quit"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->RegisterApp=dlsym(dl, "SDL_RegisterApp"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->SetModuleHandle=dlsym(dl, "SDL_SetModuleHandle"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->mutexP=dlsym(dl, "SDL_mutexP"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->mutexV=dlsym(dl, "SDL_mutexV"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->PeepEvents=dlsym(dl, "SDL_PeepEvents"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->VideoDriverName=dlsym(dl, "SDL_VideoDriverName"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->SemWait=dlsym(dl, "SDL_SemWait"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->SemPost=dlsym(dl, "SDL_SemPost"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->EventState=dlsym(dl, "SDL_EventState"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->CreateRGBSurface=dlsym(dl, "SDL_CreateRGBSurface"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->FillRect=dlsym(dl, "SDL_FillRect"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->SetColors=dlsym(dl, "SDL_SetColors"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->BlitSurface=dlsym(dl, "SDL_UpperBlit"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->UpdateRects=dlsym(dl, "SDL_UpdateRects"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->CreateSemaphore=dlsym(dl, "SDL_CreateSemaphore"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->CreateMutex=dlsym(dl, "SDL_CreateMutex"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->CreateThread=dlsym(dl, "SDL_CreateThread"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->WaitEvent=dlsym(dl, "SDL_WaitEvent"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->SetVideoMode=dlsym(dl, "SDL_SetVideoMode"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->FreeSurface=dlsym(dl, "SDL_FreeSurface"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->WM_SetCaption=dlsym(dl, "SDL_WM_SetCaption"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->ShowCursor=dlsym(dl, "SDL_ShowCursor"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->WasInit=dlsym(dl, "SDL_WasInit"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->EnableUNICODE=dlsym(dl, "SDL_EnableUNICODE"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->EnableKeyRepeat=dlsym(dl, "SDL_EnableKeyRepeat"))==NULL) {
		dlclose(dl);
		return(-1);
	}
	if((sdlf->GetWMInfo=dlsym(dl, "SDL_GetWMInfo"))==NULL) {
		dlclose(dl);
		return(-1);
	}
}
#endif

#endif
