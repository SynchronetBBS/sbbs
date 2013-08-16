#ifndef _XP_DL_H_
#define _XP_DL_H_

#include "wrapdll.h"

#ifndef STATIC_LINK
#if defined(__unix__)
	#include <dlfcn.h>

	typedef void * dll_handle;
	DLLEXPORT dll_handle DLLCALL xp_dlopen(const char **name, int mode, int major);
	#define xp_dlsym(handle, name)				dlsym(handle, #name)
	#define xp_dlclose(handle)					dlclose(handle)
#elif defined(_WIN32)
	#include <windows.h>

	typedef HMODULE dll_handle;
	DLLEXPORT dll_handle DLLCALL xp_dlopen(const char **name, int mode, int major);
	#define xp_dlsym(handle, name)				((void *)GetProcAddress(handle, #name))
	#define xp_dlclose(handle)					(FreeLibrary(handle)?0:-1)

	/* Unused values for *nix compatability */
	enum {
		 RTLD_LAZY
		,RTLD_NOW
		,RTLD_GLOBAL
		,RTLD_LOCAL
		,RTLD_TRACE
	};
#endif
#else
	typedef void* dll_handle;

	#define xp_dlopen(name, mode, major)	(name)
	#define xp_dlsym(handle, name)			(name)
	#define xp_dlclose(handle)				(0)
#endif

#endif
