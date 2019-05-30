#include <stdio.h>
#include "dirwrap.h"
#include "xp_dl.h"

#ifndef STATIC_LINK
#if defined(__unix__)

DLLEXPORT dll_handle DLLCALL xp_dlopen(const char **names, int mode, int major)
{
	const char	*name;
	char		fname[MAX_PATH+1];
	dll_handle	ret=NULL;
	int			i;

	for(; *names && (*names)[0]; names++) {
		name=*names;
		/* Try for version match */
		sprintf(fname, "lib%s.so.%d", name, major);
		if((ret=dlopen(fname, mode))!=NULL)
			return(ret);

		/* Try for name match */
		sprintf(fname, "lib%s.so", name);
		if((ret=dlopen(fname, mode))!=NULL)
			return(ret);

		/* Try for name match without the prefix */
		sprintf(fname, "%s.so", name);
		if((ret=dlopen(fname, mode))!=NULL)
			return(ret);

		/* Try all previous major versions */
		for(i=major-1; i>=0; i--) {
			sprintf(fname, "lib%s.so.%d", name, i);
			if((ret=dlopen(fname, mode))!=NULL)
				return(ret);
		}

#if defined(__APPLE__) && defined(__MACH__)
		/* Try for version match */
		sprintf(fname, "lib%s.%d.dylib", name, major);
		if((ret=dlopen(fname, mode))!=NULL)
			return(ret);

		/* Try for name match */
		sprintf(fname, "lib%s.dylib", name);
		if((ret=dlopen(fname, mode))!=NULL)
			return(ret);

		/* Try all previous major versions */
		for(i=major-1; i>=0; i--) {
			sprintf(fname, "lib%s.%d.dylib", name, i);
			if((ret=dlopen(fname, mode))!=NULL)
				return(ret);
		}
#endif	/* OS X */
	}

	return(NULL);
}
#elif defined(_WIN32)
DLLEXPORT dll_handle DLLCALL xp_dlopen(const char **names, int mode, int major)
{
	char		fname[MAX_PATH+1];
	dll_handle	ret=NULL;

	for(; *names && (*names)[0]; names++) {
		sprintf(fname, "%s.dll", *names);
		if((ret=LoadLibrary(fname))!=NULL)
			return(ret);
	}
	return(NULL);
}
#endif	/* __unix__,_WIN32 */

#endif	/* STATIC_LINK */
