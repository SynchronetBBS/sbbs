#include <stdlib.h>	/* getenv()/exit()/atexit() */
#include <stdio.h>	/* NULL */

#include "gen_defs.h"
#include "threadwrap.h"
#include <SDL.h>
#ifndef main
 #define USE_REAL_MAIN
#endif
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
#include <xp_dl.h>

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
int XPDEV_main(int argc, char **argv, char **enviro)
{
	return CIOLIB_main(argc, argv, enviro);
}


int load_sdl_funcs(struct sdlfuncs *sdlf)
{
	dll_handle	sdl_dll;
	const char *libnames[]={"SDL", "SDL-1.2", "SDL-1.1", NULL};

	putenv("SDL_VIDEO_ALLOW_SCREENSAVER=1");
	sdlf->gotfuncs=0;
	if((sdl_dll=xp_dlopen(libnames,RTLD_LAZY|RTLD_GLOBAL,SDL_PATCHLEVEL))==NULL)
		return(-1);

#ifdef _WIN32
	if((sdlf->SetModuleHandle=xp_dlsym(sdl_dll, SDL_SetModuleHandle))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
#endif
	if((sdlf->Init=xp_dlsym(sdl_dll, SDL_Init))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->Quit=xp_dlsym(sdl_dll, SDL_Quit))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->mutexP=xp_dlsym(sdl_dll, SDL_mutexP))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->mutexV=xp_dlsym(sdl_dll, SDL_mutexV))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->PeepEvents=xp_dlsym(sdl_dll, SDL_PeepEvents))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->VideoDriverName=xp_dlsym(sdl_dll, SDL_VideoDriverName))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SemWait=xp_dlsym(sdl_dll, SDL_SemWait))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SemWaitTimeout=xp_dlsym(sdl_dll, SDL_SemWaitTimeout))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SemPost=xp_dlsym(sdl_dll, SDL_SemPost))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->EventState=xp_dlsym(sdl_dll, SDL_EventState))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateRGBSurface=xp_dlsym(sdl_dll, SDL_CreateRGBSurface))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateRGBSurfaceFrom=xp_dlsym(sdl_dll, SDL_CreateRGBSurfaceFrom))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->FillRect=xp_dlsym(sdl_dll, SDL_FillRect))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetColors=xp_dlsym(sdl_dll, SDL_SetColors))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->BlitSurface=xp_dlsym(sdl_dll, SDL_UpperBlit))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UpdateRects=xp_dlsym(sdl_dll, SDL_UpdateRects))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UpdateRect=xp_dlsym(sdl_dll, SDL_UpdateRect))==NULL) {
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
	if((sdlf->SDL_CreateMutex=xp_dlsym(sdl_dll, SDL_CreateMutex))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateThread=xp_dlsym(sdl_dll, SDL_CreateThread))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->KillThread=xp_dlsym(sdl_dll, SDL_KillThread))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WaitThread=xp_dlsym(sdl_dll, SDL_WaitThread))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WaitEvent=xp_dlsym(sdl_dll, SDL_WaitEvent))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->PollEvent=xp_dlsym(sdl_dll, SDL_PollEvent))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->SetVideoMode=xp_dlsym(sdl_dll, SDL_SetVideoMode))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->FreeSurface=xp_dlsym(sdl_dll, SDL_FreeSurface))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WM_SetCaption=xp_dlsym(sdl_dll, SDL_WM_SetCaption))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->WM_SetIcon=xp_dlsym(sdl_dll, SDL_WM_SetIcon))==NULL) {
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
	if((sdlf->EnableUNICODE=xp_dlsym(sdl_dll, SDL_EnableUNICODE))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->EnableKeyRepeat=xp_dlsym(sdl_dll, SDL_EnableKeyRepeat))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetWMInfo=xp_dlsym(sdl_dll, SDL_GetWMInfo))==NULL) {
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
	if((sdlf->MapRGB=xp_dlsym(sdl_dll, SDL_MapRGB))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->LockSurface=xp_dlsym(sdl_dll, SDL_LockSurface))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockSurface=xp_dlsym(sdl_dll, SDL_UnlockSurface))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->DisplayFormat=xp_dlsym(sdl_dll, SDL_DisplayFormat))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->Flip=xp_dlsym(sdl_dll, SDL_Flip))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->CreateYUVOverlay=xp_dlsym(sdl_dll, SDL_CreateYUVOverlay))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->DisplayYUVOverlay=xp_dlsym(sdl_dll, SDL_DisplayYUVOverlay))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->FreeYUVOverlay=xp_dlsym(sdl_dll, SDL_FreeYUVOverlay))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->LockYUVOverlay=xp_dlsym(sdl_dll, SDL_LockYUVOverlay))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->UnlockYUVOverlay=xp_dlsym(sdl_dll, SDL_UnlockYUVOverlay))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->GetVideoInfo=xp_dlsym(sdl_dll, SDL_GetVideoInfo))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	if((sdlf->Linked_Version=xp_dlsym(sdl_dll, SDL_Linked_Version))==NULL) {
		xp_dlclose(sdl_dll);
		return(-1);
	}
	sdlf->gotfuncs=1;
	sdl_funcs_loaded=1;
	return(0);
}

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

static void QuitWrap(void)
{
	if(sdl.Quit && sdl_initialized)
		sdl.Quit();
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
	int		main_ret=0;
	int		use_sdl_video=FALSE;
#ifdef _WIN32
	char		*driver_env=NULL;
#endif

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
			driver_env=getenv("SDL_VIDEODRIVER");
			if(driver_env==NULL || strcmp(driver_env,"windib")) {
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
		SetThreadName("SDL Main");
		atexit(QuitWrap);
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
