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
#endif

#include <stdio.h>		/* sprintf */
#include <stdlib.h>		/* rand */

#include "gen_defs.h"	/* BOOL */ 
#include "sbbswrap.h"	/* verify prototypes */

/****************************************************************************/
/* Checks the disk drive for the existence of a file. Returns 1 if it       */
/* exists, 0 if it doesn't.                                                 */
/****************************************************************************/
BOOL fexist(char *filespec)
{
	long	handle;
    struct _finddata_t f;

	if((handle=_findfirst(filespec,&f))==-1)
		return(FALSE);
	_findclose(handle);
	if(f.attrib&_A_SUBDIR)
		return(FALSE);
	return(TRUE);
}

/****************************************************************************/
/* Returns the length of the file in 'filespec'                             */
/****************************************************************************/
long flength(char *filespec)
{
	long	handle;
    struct _finddata_t f;

	if((handle=_findfirst(filespec,&f))==-1)
		return(-1L);
	_findclose(handle);
	return(f.size);
}

/****************************************************************************/
/* Returns the time/date of the file in 'filespec' in time_t (unix) format  */
/****************************************************************************/
long fdate(char *filespec)
{
    long	handle;
	struct _finddata_t f;

	if((handle=_findfirst(filespec,&f))==-1)
		return(-1L);
	_findclose(handle);
	return(f.time_write);
}

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
/* There may be a native GNU C Library function to this...					*/
/****************************************************************************/
#ifdef __GNUC__
char* ultoa(ulong val, char* str, int radix)
{
	switch(radix) {
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

		if(avail.HighPart)
			return(0xffffffff);
		return(avail.LowPart);
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
