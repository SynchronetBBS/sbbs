#include "config.h"

#ifdef HAVE_SDL2MIXER
#define HAVE_SDLMIXER
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_rwops.h"
#define USE_SFX_INIT_THREAD
#define HWSDLX_CreateThread(_func_) SDL_CreateThread(_func_, "SFX init", 0)
#endif /* HAVE_SDL2MIXER */

#include "hwsdl_audio.c"
