/* dirwrap.h */

/* Directory system-call wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _DIRWRAP_H
#define _DIRWRAP_H

#include <stdlib.h>		/* _fullpath() on Win32 */

#if defined(__unix__)
	#include <sys/param.h>	/* PATH_MAX */
#endif

#if defined(INCLUDE_PATHS_H)
	#include <paths.h>		/* _PATHS_* macros */
#endif

#include "gen_defs.h"	/* ulong */
#include "wrapdll.h"	/* DLLEXPORT and DLLCALL */

#if defined(__cplusplus)
extern "C" {
#endif

/****************/
/* RTL-specific */
/****************/

#if defined(__unix__)

	#define ALLFILES "*"	/* matches all files in a directory */
	#include <glob.h>		/* POSIX.2 directory pattern matching function */
	#define MKDIR(dir)		mkdir(dir,0777)

#else	

	#define ALLFILES "*.*"	/* matches all files in a directory */
	#ifdef __WATCOMC__
		#define MKDIR(dir)		mkdir(dir)
	#else
		#define MKDIR(dir)		_mkdir(dir)
	#endif

	/* glob-compatible findfirst/findnext wrapper */

	typedef struct 
	{
			size_t gl_pathc;			/* Count of paths matched so far */
			char **gl_pathv;			/* List of matched pathnames. */
			size_t gl_offs;				/* Slots to reserve in 'gl_pathv'. */
	} glob_t;

	/* Bits set in the FLAGS argument to `glob'.  */
	#define GLOB_ERR        (1 << 0)	/* Return on read errors.  */
	#define GLOB_MARK       (1 << 1)	/* Append a slash to each name.  */
	#define GLOB_NOSORT     (1 << 2)	/* Don't sort the names.  */
	#define GLOB_DOOFFS     (1 << 3)	/* Insert PGLOB->gl_offs NULLs.  */
	#define GLOB_NOCHECK    (1 << 4)	/* If nothing matches, return the pattern.  */
	#define GLOB_APPEND     (1 << 5)	/* Append to results of a previous call.  */
	#define GLOB_NOESCAPE   (1 << 6)	/* Backslashes don't quote metacharacters.  */
	#define GLOB_PERIOD     (1 << 7)	/* Leading `.' can be matched by metachars.  */
	#define GLOB_MAGCHAR    (1 << 8)	/* Set in gl_flags if any metachars seen.  */
	#define GLOB_ALTDIRFUNC (1 << 9)	/* Use gl_opendir et al functions.  */
	#define GLOB_BRACE      (1 << 10)	/* Expand "{a,b}" to "a" "b".  */
	#define GLOB_NOMAGIC    (1 << 11)	/* If no magic chars, return the pattern.  */
	#define GLOB_TILDE      (1 << 12)	/* Expand ~user and ~ to home directories. */
	#define GLOB_ONLYDIR    (1 << 13)	/* Match only directories.  */
	#define GLOB_TILDE_CHECK (1 << 14)	/* Like GLOB_TILDE but return an error
										  if the user name is not available.  */
	/* Error returns from `glob'.  */
	#define GLOB_NOSPACE    1       /* Ran out of memory.  */
	#define GLOB_ABORTED    2       /* Read error.  */
	#define GLOB_NOMATCH    3       /* No matches found.  */
	#define GLOB_NOSYS      4       /* Not implemented.  */

	DLLEXPORT int	DLLCALL	glob(const char *pattern, int flags, void* unused, glob_t*);
	DLLEXPORT void	DLLCALL globfree(glob_t*);

#endif

#define FULLPATH(a,r,l)		_fullpath(a,r,l)

/*****************************/
/* POSIX Directory Functions */
/*****************************/
#if defined(_MSC_VER) || defined(__DMC__)
	#include <io.h>		/* _finddata_t */

	/* dirent structure returned by readdir().
	 */
	struct dirent
	{	
		char        d_name[260];	/* filename */
	};

	/* DIR type returned by opendir().  The members of this structure
	 * must not be accessed by application programs.
	 */
	typedef struct
	{
		char				filespec[260];
		struct dirent		dirent;
		long				handle;
		struct _finddata_t	finddata;
		BOOL				end;		/* End of directory flag */
	} DIR;


	/* Prototypes.
	 */
	DIR            *	opendir  (const char *__dirname);
	struct dirent  *	readdir  (DIR *__dir);
	int                 closedir (DIR *__dir);
	void                rewinddir(DIR *__dir);
#elif defined(__WATCOMC__)
	#include <direct.h>	/* opendir, etc defined here in Watcom */
#else
	#include <dirent.h>	/* POSIX directory functions */
#endif


/**********/
/* Macros */
/**********/

/* POSIX readdir convenience macro */
#ifndef DIRENT
#define DIRENT struct dirent	
#endif

#if defined(__unix__)
	#define PATH_DELIM			'/'
	#define IS_PATH_DELIM(x)	(x=='/')

	/* These may be pre-defined in paths.h (BSD) */
	#ifndef _PATH_TMP
	#define _PATH_TMP			"/tmp/"
	#endif
	#ifndef _PATH_DEVNULL
	#define _PATH_DEVNULL		"/dev/null"
	#endif

#else /* MS-DOS based OS */

	#define PATH_DELIM			'\\'
	#define IS_PATH_DELIM(x)	((x)=='/' || (x)=='\\')
	#define _PATH_TMP			getenv("TEMP")
	#define _PATH_DEVNULL		"NUL"

#endif

#if !defined(MAX_PATH)	/* maximum path length */
	#if defined MAXPATHLEN
		#define MAX_PATH MAXPATHLEN	/* clib.h */
	#elif defined PATH_MAX
		#define MAX_PATH PATH_MAX
	#elif defined _MAX_PATH
		#define MAX_PATH _MAX_PATH
	#else
		#define MAX_PATH 260		
	#endif
#endif

#if defined(_MSC_VER) || defined(__MINGW32__)
	#define CHMOD(s,m)		_chmod(s,m)
#elif defined(__BORLANDC__) && !defined(__unix__)
	#define CHMOD(p,a)		_rtl_chmod(p,1,a) 	/* _chmod obsolete in 4.x */
#else	
	#define CHMOD(s,m)		chmod(s,m)
#endif

/* General file system wrappers for all platforms and compilers */
DLLEXPORT BOOL		DLLCALL fexist(const char *filespec);
DLLEXPORT BOOL		DLLCALL fexistcase(char *filespec);	/* fixes upr/lwr case fname */
DLLEXPORT long		DLLCALL flength(const char *filename);
DLLEXPORT time_t	DLLCALL fdate(const char *filename);
DLLEXPORT BOOL		DLLCALL	isdir(const char *filename);
DLLEXPORT char*		DLLCALL getfname(const char* path);
DLLEXPORT int		DLLCALL getfattr(const char* filename);
DLLEXPORT ulong		DLLCALL getfreediskspace(const char* path, ulong unit);
DLLEXPORT ulong		DLLCALL delfiles(char *inpath, char *spec);
DLLEXPORT char*		DLLCALL backslash(char* path);

#if defined(__unix__)
DLLEXPORT void DLLCALL _splitpath(const char *path, char *drive, char *dir, 
								  char *fname, char *ext);
DLLEXPORT char * DLLCALL _fullpath(char *target, const char *path, size_t size);
#endif

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
