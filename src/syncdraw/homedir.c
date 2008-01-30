#include <string.h>
#include "dirwrap.h"

#if defined(_WIN32)
#include "shlobj.h"
#endif

char	HomeDir[MAX_PATH+1];

char *homedir(void)
{
#if defined(_WIN32)
	if(SUCCEEDED(SHGetFolderPath(NULL, 
			CSIDL_APPDATA|CSIDL_FLAG_CREATE, 
			NULL, 
			0, 
                        HomeDir))) 
	{
		backslash(HomeDir);
		strcat(HomeDir,"SyncTools/");
		if(!isdir(HomeDir))
			MKDIR(HomeDir);
		strcat(HomeDir,"SyncDraw/");
		if(!isdir(HomeDir))
			MKDIR(HomeDir);
		/* Really, why would I want a version in here? */
		if(isdir(HomeDir))
			return(HomeDir);
	}
	strcpy(HomeDir,"./");
	return(HomeDir);

#elif defined(__unix__)
	char *p;

	p=getenv("HOME");
	if(p!=NULL) {
		strcpy(HomeDir,p);
		backslash(HomeDir);
		if(!isdir(HomeDir))
			MKDIR(HomeDir);
		strcat(HomeDir,".syncdraw/");
		if(!isdir(HomeDir))
			MKDIR(HomeDir);
		if(isdir(HomeDir))
			return(HomeDir);
	}
	strcpy(HomeDir,"./");
	return(HomeDir);

#else

	#error Cannot locate users application data path

#endif
}
