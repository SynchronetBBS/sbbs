/*
    SDL_main.c, placed in the public domain by Sam Lantinga  4/13/98

    The WinMain function -- calls your program's main() function
*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include <windows.h>
#include <malloc.h>			/* For _alloca() */

#define DIR_SEPERATOR TEXT("/")
#include <direct.h>

#include "ciolib.h"			/* ciolib_appname */

/* Parse a command line buffer into arguments */
static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	int argc;

	argc = 0;
	for ( bufp = cmdline; *bufp; ) {
		/* Skip leading whitespace */
		while ( isspace(*bufp) ) {
			++bufp;
		}
		/* Skip over argument */
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			/* Skip over word */
			while ( *bufp && (*bufp != '"') ) {
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			/* Skip over word */
			while ( *bufp && ! isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}

extern int CIOLIB_main(int argc, char **argv);

/* This is where execution begins [console apps] */
static int console_main(int argc, char *argv[], char **env)
{
	int n;
	char *bufp, *appname;

	/* Get the class name from argv[0] */
	appname = argv[0];
	if ( (bufp=strrchr(argv[0], '\\')) != NULL ) {
		appname = bufp+1;
	} else
	if ( (bufp=strrchr(argv[0], '/')) != NULL ) {
		appname = bufp+1;
	}

	if ( (bufp=strrchr(appname, '.')) == NULL )
		n = strlen(appname);
	else
		n = (bufp-appname);

	bufp = (char *)alloca(n+1);
	if ( bufp == NULL ) {
		return 1;
	}
	strncpy(bufp, appname, n);
	bufp[n] = '\0';
	ciolib_appname = bufp;

	/* Run the application main() code */
	n=CIOLIB_main(argc, argv);

	/* Exit cleanly, calling atexit() functions */
	exit(n);

	/* Hush little compiler, don't you cry... */
	return(n);
}

/* This is where execution begins [windowed apps] */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
	HINSTANCE handle;
	char **argv;
	int argc;
	char *cmdline;
	char *bufp;

	/* Start up DDHELP.EXE before opening any files, so DDHELP doesn't
	   keep them open.  This is a hack.. hopefully it will be fixed 
	   someday.  DDHELP.EXE starts up the first time DDRAW.DLL is loaded.
	 */
	handle = LoadLibrary(TEXT("DDRAW.DLL"));
	if ( handle != NULL ) {
		FreeLibrary(handle);
	}

	/* Grab the command line (use alloca() on Windows) */
	bufp = GetCommandLine();
	cmdline = (char *)alloca(strlen(bufp)+1);
	if ( cmdline == NULL ) {
		return 1;
	}
	strcpy(cmdline, bufp);

	/* Parse it into argv and argc */
	argc = ParseCommandLine(cmdline, NULL);
	argv = (char **)alloca((argc+1)*(sizeof *argv));
	if ( argv == NULL ) {
		return 1;
	}
	ParseCommandLine(cmdline, argv);

	/* Run the main program (after a little SDL initialization) */
	return(console_main(argc, argv, _environ));
}
