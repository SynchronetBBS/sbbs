/* smbwrap.c */

/* Synchronet SMBLIB system-call wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
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

/* OS-specific */
#if defined(__unix__)

#include <glob.h>       /* glob() wildcard matching */
#include <string.h>     /* strlen() */

#endif

/* ANSI */
#include <sys/types.h>	/* _dev_t */
#include <sys/stat.h>	/* struct stat */


/* SMB-specific */
#include "smblib.h"		/* SMBCALL */
#include "smbwrap.h"	/* Verify prototypes */

#ifdef _WIN32
#define stat(f,s)	_stat(f,s)
#define STAT		struct _stat
#else
#define STAT		struct stat
#endif

/****************************************************************************/
/* Returns the length of the file in 'filename'                             */
/****************************************************************************/
long SMBCALL flength(char *filename)
{
	STAT st;

	if(stat(filename, &st)!=0)
		return(-1L);

	return(st.st_size);
}

/****************************************************************************/
/* Checks the file system for the existence of one or more files.			*/
/* Returns TRUE if it exists, FALSE if it doesn't.                          */
/* 'filespec' may contain wildcards!										*/
/****************************************************************************/
BOOL SMBCALL fexist(char *filespec)
{
#ifdef _WIN32

	long	handle;
	struct _finddata_t f;

	if((handle=_findfirst(filespec,&f))==-1)
		return(FALSE);

 	_findclose(handle);

 	if(f.attrib&_A_SUBDIR)
		return(FALSE);

	return(TRUE);

#elif defined(__unix__)	/* portion by cmartin */

	glob_t *aglob;
    int c;
    int l;

    // start the search
    glob(filespec, GLOB_MARK | GLOB_NOSORT, NULL, aglob);

    if (!aglob->gl_pathc) {
	    // no results
    	globfree(aglob);
    	return FALSE;
    }

    // make sure it's not a directory
	c = aglob->gl_pathc;
    while (c--) {
    	l = strlen(aglob->gl_pathv[c]);
    	if (aglob->gl_pathv[c][l] != '/') {
        	globfree(aglob);
            return TRUE;
        }
    }
        
    globfree(aglob);
    return FALSE;

#else

#warning "fexist() port needs to support wildcards!"

	return(FALSE);

#endif
}

#if defined(__unix__)

#include <unistd.h>
#include <fcntl.h>

/****************************************************************************/
/* Returns the length of the file in 'fd'									*/
/****************************************************************************/
long filelength(int fd)
{
	stat st;

	if(fstat(fd, &st)!=0)
		return(-1L);

	return(st.st_size);
}

/* Sets a lock on a portion of a file */
int lock(int fd, int pos, int len)
{
	return 0;
}

/* Removes a lock from a file record */
int unlock(int fd, int pos, int len)
{
	return 0;
}

/* Opens a file in specified sharing (file-locking) mode */
int sopen(char *fn, int access, int share, int perm)
{
	return 0;
}

#elif defined _MSC_VER || defined __MINGW32__

#include <io.h>				/* tell */
#include <stdio.h>			/* SEEK_SET */
#include <sys/locking.h>	/* _locking */

/* Fix MinGW locking.h typo */
#if defined LK_UNLOCK && !defined LK_UNLCK
	#define LK_UNLCK LK_UNLOCK
#endif

int lock(int file, long offset, int size) 
{
	int	i;
	long	pos;
   
	pos=tell(file);
	if(offset!=pos)
		lseek(file, offset, SEEK_SET);
	i=_locking(file,LK_NBLCK,size);
	if(offset!=pos)
		lseek(file, pos, SEEK_SET);
	return(i);
}

int unlock(int file, long offset, int size)
{
	int	i;
	long	pos;
   
	pos=tell(file);
	if(offset!=pos)
		lseek(file, offset, SEEK_SET);
	i=_locking(file,LK_UNLCK,size);
	if(offset!=pos)
		lseek(file, pos, SEEK_SET);
	return(i);
}

#endif	/* !Unix && (MSVC || MinGW) */