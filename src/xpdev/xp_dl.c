#include "dirwrap.h"
#include "xp_dl.h"

#if defined(__unix__)
xp_dlopen(const char *name, int mode, int major, int minor)
{
	char		fname[MAX_PATH+1];
	dll_handle	ret=NULL;
	int			i;

	/* Try for exact version match */
	sprintf(fname, "%s.%d.%d", name, major, minor);
	if((dll_handle=dlopen(fname, mode))!=NULL)
		return(ret);

	/* Try for major version match */
	sprintf(fname, "%s.%d", name, major);
	if((dll_handle=dlopen(fname, mode))!=NULL)
		return(ret);

	/* Try for preferred match (symlink) */
	if((dll_handle=dlopen(name, mode))!=NULL)
		return(ret);

	/* Try all previous major versions */
	for(i=major-1; i>=0; i--) {
		sprintf(fname, "%s.%d", name, i);
		if((dll_handle=dlopen(fname, mode))!=NULL)
			return(ret);
	}

	return(NULL);
}
#endif


#endif
