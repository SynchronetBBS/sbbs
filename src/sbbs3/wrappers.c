/* wrappers.c */

/* Synchronet system-call wrappers */

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

#ifdef _WIN32

#include <windows.h>	/* WINAPI, etc */
#include <io.h>			/* _findfirst */

#elif defined __unix__

#include <unistd.h>		/* usleep */
#include <fcntl.h>		/* O_NOCCTY */
#include <ctype.h>		/* toupper */
#include <sys/kd.h>		/* KIOCSOUND */
#include <sys/ioctl.h>	/* ioctl */

#endif

#include <sys/types.h>	/* _dev_t */
#include <sys/stat.h>	/* struct stat */

#include <stdio.h>		/* sprintf */
#include <stdlib.h>		/* rand */
#include <errno.h>		/* ENOENT definitions */

#include "gen_defs.h"	/* BOOL */ 
#include "sbbswrap.h"	/* verify prototypes */

#ifdef _WIN32
#define stat(f,s)	_stat(f,s)
#define STAT		struct _stat
#else
#define STAT		struct stat
#endif

/****************************************************************************/
/* Checks the file system for the existence of one or more files.			*/
/* Returns TRUE if it exists, FALSE if it doesn't.                          */
/* 'filespec' may contain wildcards!										*/
/****************************************************************************/
BOOL fexist(char *filespec)
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

#else

#warning "fexist() port needs to support wildcards!"

	STAT st;

	if(stat(filespec, &st)!=0)
		return(FALSE);

	if(st.st_mode&S_IFDIR)	/* Directory, not a file */
		return(FALSE);		

	return(TRUE);

#endif
}

/****************************************************************************/
/* Returns the length of the file in 'filename'                             */
/****************************************************************************/
long flength(char *filename)
{
	STAT st;

	if(stat(filename, &st)!=0)
		return(-1L);

	return(st.st_size);
}

/****************************************************************************/
/* Returns the time/date of the file in 'filename' in time_t (unix) format  */
/****************************************************************************/
long fdate(char *filename)
{
	STAT st;

	if(stat(filename, &st)!=0)
		return(-1L);

	return(st.st_mtime);
}

/****************************************************************************/
/* Returns the attributes (mode) for specified 'filename'					*/
/****************************************************************************/
int getfattr(char* filename)
{
#ifdef _WIN32
	long handle;
	struct _finddata_t	finddata;

	if((handle=_findfirst(filename,&finddata))==-1) {
		errno=ENOENT;
		return(-1);
	}
	_findclose(handle);
	return(finddata.attrib);
#else
	STAT st;

	if(stat(filename, &st)!=0) {
		errno=ENOENT;
		return(-1L);
	}

	return(st.st_mode);
#endif
}

/****************************************************************************/
/* Returns the length of the file in 'fd'									*/
/****************************************************************************/
#ifdef __unix__
long filelength(int fd)
{
	STAT st;

	if(fstat(fd, &st)!=0)
		return(-1L);

	return(st.st_size);
}
#endif

/****************************************************************************/
/* Generate a tone at specified frequency for specified milliseconds		*/
/* Thanks to Casey Martin for this code										*/
/****************************************************************************/
#ifdef __unix__
void sbbs_beep(int freq, int dur)
{
	static int console_fd=-1;

	if(console_fd == -1) 
  		console_fd = open("/dev/console", O_NOCTTY);
	
	if(console_fd != -1) {
		ioctl(console_fd, KIOCSOUND, (int) (1193180 / freq));
		mswait(dur);
		ioctl(console_fd, KIOCSOUND, 0);	/* turn off tone */
	}
}
#endif

/****************************************************************************/
/* Return random number between 0 and n-1									*/
/****************************************************************************/
#ifndef __BORLANDC__
int sbbs_random(int n)
{
	float f;

	if(n<2)
		return(0);
	f=(float)rand()/(float)RAND_MAX;

	return((int)(n*f));
}
#endif

/****************************************************************************/
/* Return ASCII string representation of ulong								*/
/* There may be a native GNU C Library function to this...					*/
/****************************************************************************/
#if !defined _MSC_VER && !defined __BORLANDC__
char* ultoa(ulong val, char* str, int radix)
{
	switch(radix) {
		case 8:
			sprintf(str,"%lo",val);
			break;
		case 10:
			sprintf(str,"%lu",val);
			break;
		case 16:
			sprintf(str,"%lx",val);
			break;
		default:
			sprintf(str,"bad radix: %d",radix);
			break;
	}
	return(str);
}
#endif

/****************************************************************************/
/* Convert ASCIIZ string to upper case										*/
/****************************************************************************/
#ifdef __unix__
char* strupr(char *str)
{
	char* p=str;

	while(*p) {
		*p=toupper(*p);
		p++;
	}
	return(str);
}
/****************************************************************************/
/* Convert ASCIIZ string to lower case										*/
/****************************************************************************/
char* strlwr(char *str)
{
	char* p=str;

	while(*p) {
		*p=tolower(*p);
		p++;
	}
	return(str);
}
#endif

/****************************************************************************/
/* Return free disk space in bytes (up to a maximum of 4GB)					*/
/****************************************************************************/
#ifdef _WIN32
	typedef BOOL(WINAPI * GetDiskFreeSpaceEx_t)
		(LPCTSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER); 
#endif

ulong getfreediskspace(char* path)
{
#ifdef _WIN32
	HINSTANCE		hK32;
	char			root[16];
	DWORD			TotalNumberOfClusters;
	DWORD			NumberOfFreeClusters;
	DWORD			BytesPerSector;
	DWORD			SectorsPerCluster;
	ULARGE_INTEGER	avail;
	ULARGE_INTEGER	size;
	GetDiskFreeSpaceEx_t GetDiskFreeSpaceEx;

	if ((hK32 = LoadLibrary("KERNEL32")) == NULL)
		return(0);

	GetDiskFreeSpaceEx = (GetDiskFreeSpaceEx_t)GetProcAddress(hK32,"GetDiskFreeSpaceExA");
 
	if (GetDiskFreeSpaceEx!=NULL) {	/* Windows 95-OSR2 or later */
		if(!GetDiskFreeSpaceEx(
			path,		// pointer to the directory name
			&avail,		// receives the number of bytes on disk available to the caller
			&size,		// receives the number of bytes on disk
			NULL))		// receives the free bytes on disk
			return(0);

		if(avail.u.HighPart)
			return(~0);	/* 4GB max */

		return(avail.u.LowPart);
	}

	/* Windows 95 (old way), limited to 2GB */
	sprintf(root,"%.3s",path);
	if(!GetDiskFreeSpace(
		root,					// pointer to root path
		&SectorsPerCluster,		// pointer to sectors per cluster
		&BytesPerSector,		// pointer to bytes per sector
		&NumberOfFreeClusters,	// pointer to number of free clusters
		&TotalNumberOfClusters  // pointer to total number of clusters
		))
		return(0);

	return(NumberOfFreeClusters*SectorsPerCluster*BytesPerSector);
#else
	#warning OS-specific code needed here
	return(0);
#endif
}
