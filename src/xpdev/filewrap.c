/* filewrap.c */

/* File-related system-call wrappers */

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

/* OS-specific */
#if defined(__unix__)

#include <string.h>     /* strlen() */
#include <unistd.h>     /* getpid() */
#include <fcntl.h>      /* fcntl() file/record locking */
#include <sys/file.h>	/* L_SET for Solaris */

#endif

/* ANSI */
#include <sys/types.h>	/* _dev_t */
#include <sys/stat.h>	/* struct stat */

#include "filewrap.h"	/* Verify prototypes */

/****************************************************************************/
/* Returns the modification time of the file in 'fd'						*/
/****************************************************************************/
time_t DLLCALL filetime(int fd)
{
	struct stat st;

	if(fstat(fd, &st)!=0)
		return(-1);

	return(st.st_mtime);
}

#if defined(__unix__) && !defined(__BORLANDC__)

/****************************************************************************/
/* Returns the length of the file in 'fd'									*/
/****************************************************************************/
long DLLCALL filelength(int fd)
{
	struct stat st;

	if(fstat(fd, &st)!=0)
		return(-1L);

	return(st.st_size);
}

/* Sets a lock on a portion of a file */
int DLLCALL lock(int fd, long pos, int len)
{
	int	flags;
 	struct flock alock;

	if((flags=fcntl(fd,F_GETFL))==-1)
		return -1;

	if(flags==O_RDONLY)
		alock.l_type = F_RDLCK; // set read lock to prevent writes
	else
		alock.l_type = F_WRLCK; // set write lock to prevent all access
	alock.l_whence = L_SET;	  // SEEK_SET
	alock.l_start = pos;
	alock.l_len = len;

	if(fcntl(fd, F_SETLK, &alock)==-1)
		return(-1);
	return(0);
}

/* Removes a lock from a file record */
int DLLCALL unlock(int fd, long pos, int len)
{
	struct flock alock;

	alock.l_type = F_UNLCK;   // remove the lock
	alock.l_whence = L_SET;
	alock.l_start = pos;
	alock.l_len = len;
	if(fcntl(fd, F_SETLK, &alock)==-1)
		return(-1);
	return(0);
}

/* Opens a file in specified sharing (file-locking) mode */
int DLLCALL sopen(char *fn, int access, int share)
{
	int fd;
	struct flock alock;

	if ((fd = open(fn, access, S_IREAD|S_IWRITE)) < 0)
		return -1;

	if (share == SH_DENYNO)
		// no lock needed
		return fd;

	alock.l_type = share;
	alock.l_whence = L_SET;
	alock.l_start = 0;
	alock.l_len = 0;       // lock to EOF

	if (fcntl(fd, F_SETLK, &alock) == -1) {
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

int DLLCALL lock(int file, long offset, int size) 
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

int DLLCALL unlock(int file, long offset, int size)
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
