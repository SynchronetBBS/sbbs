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
#include <unistd.h>     /* getpid() */
#include <fcntl.h>      /* fcntl() file/record locking */

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
/* Convert ASCIIZ string to upper case										*/
/****************************************************************************/
#ifdef __unix__
char* SMBCALL strupr(char* str)
{
	char*	p=str;

	while(*p) {
		*p=toupper(*p);
		p++;
	}
	return(str);
}
/****************************************************************************/
/* Convert ASCIIZ string to lower case										*/
/****************************************************************************/
char* SMBCALL strlwr(char* str)
{
	char*	p=str;

	while(*p) {
		*p=tolower(*p);
		p++;
	}
	return(str);
}
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

	glob_t g;
    int c;
    int l;

    // start the search
    glob(filespec, GLOB_MARK | GLOB_NOSORT, NULL, &g);

    if (!g.gl_pathc) {
	    // no results
    	globfree(&g);
    	return FALSE;
    }

    // make sure it's not a directory
	c = g.gl_pathc;
    while (c--) {
    	l = strlen(g.gl_pathv[c]);
    	if (l && g.gl_pathv[c][l-1] != '/') {
        	globfree(&g);
            return TRUE;
        }
    }
        
    globfree(&g);
    return FALSE;

#else

#warning "fexist() port needs to support wildcards!"

	return(FALSE);

#endif
}

#if defined(__unix__)

/****************************************************************************/
/* Returns the length of the file in 'fd'									*/
/****************************************************************************/
long SMBCALL filelength(int fd)
{
	STAT st;

	if(fstat(fd, &st)!=0)
		return(-1L);

	return(st.st_size);
}

/* Sets a lock on a portion of a file */
int SMBCALL lock(int fd, long pos, int len)
{
	int	flags;
 	struct flock alock;

	if((flags=fcntl(fd,F_GETFL))<0)
		return -1;

	if(flags==O_RDONLY)
		alock.l_type = F_RDLCK; // set read lock to prevent writes
	else
		alock.l_type = F_WRLCK; // set write lock to prevent all access
	alock.l_whence = L_SET;	  // SEEK_SET
	alock.l_start = pos;
	alock.l_len = len;

	return fcntl(fd, F_SETLK, &alock);
}

/* Removes a lock from a file record */
int SMBCALL unlock(int fd, long pos, int len)
{
	struct flock alock;

	alock.l_type = F_UNLCK;   // remove the lock
	alock.l_whence = L_SET;
	alock.l_start = pos;
	alock.l_len = len;
	return fcntl(fd, F_SETLK, &alock);
}

/* Opens a file in specified sharing (file-locking) mode */
int SMBCALL sopen(char *fn, int access, int share)
{
	int fd;
	struct flock alock;

	if (share == SH_DENYNO	/* we're going to be using lock/unlock later */
		&& access == O_RDONLY)
		access = O_RDWR;	/* must have write access to set exclusive lock */

	if ((fd = open(fn, access, S_IREAD|S_IWRITE)) < 0)
		return -1;

	if (share == SH_DENYNO)
		// no lock needed
		return fd;

	alock.l_type = share;
	alock.l_whence = L_SET;
	alock.l_start = 0;
	alock.l_len = 0;       // lock to EOF

#if 0
	/* The l_pid field is only used with F_GETLK to return the process 
		ID of the process holding a blocking lock.  */
	alock.l_pid = getpid();
#endif

	if (fcntl(fd, F_SETLK, &alock) < 0) {
		close(fd);
		return -1;
	}

	return fd;
}

#elif defined _MSC_VER || defined __MINGW32__

#include <io.h>				/* tell */
#include <stdio.h>			/* SEEK_SET */
#include <sys/locking.h>	/* _locking */

/* Fix MinGW locking.h typo */
#if defined LK_UNLOCK && !defined LK_UNLCK
	#define LK_UNLCK LK_UNLOCK
#endif

int SMBCALL lock(int file, long offset, int size) 
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

int SMBCALL unlock(int file, long offset, int size)
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