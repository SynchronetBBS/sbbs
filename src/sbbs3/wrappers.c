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

/* FreeBSD uses kbio.h instead of kd.h */
/* BSD also needs mount.h instead of vfs.h */
/* param.h has needed definitions for the FreeBSD system */
#ifndef __FreeBSD__
#include <sys/kd.h>		/* KIOCSOUND */
#endif

#ifdef __FreeBSD__
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/kbio.h>
#endif

#include <sys/ioctl.h>	/* ioctl */

#ifdef __GLIBC__		/* actually, BSD, but will work for now */
#include <sys/vfs.h>    /* statfs() */
#endif

#endif

#include <sys/types.h>	/* _dev_t */
#include <sys/stat.h>	/* struct stat */

#include <stdio.h>		/* sprintf */
#include <stdlib.h>		/* rand */
#include <errno.h>		/* ENOENT definitions */

#include "sbbs.h"		/* getfname */

/* Win32 (minimal) implementation of POSIX.2 glob() function */
/* This code _may_ work on other DOS-based platforms (e.g. OS/2) */

#if !defined(__unix__)

#ifdef __BORLANDC__
	#pragma argsused
#endif

static int glob_compare( const void *arg1, const void *arg2 )
{
   /* Compare all of both strings: */
   return stricmp( * ( char** ) arg1, * ( char** ) arg2 );
}

int	DLLCALL	glob(const char *pattern, int flags, void* unused, glob_t* glob)
{
    struct	_finddata_t ff;
	long	ff_handle;
	size_t	found=0;
	char	path[MAX_PATH+1];
	char*	p;
	char**	new_pathv;

	if(!(flags&GLOB_APPEND)) {
		glob->gl_pathc=0;
		glob->gl_pathv=NULL;
	}

	ff_handle=_findfirst((char*)pattern,&ff);
	while(ff_handle!=-1) {
		if(!(flags&GLOB_ONLYDIR) || ff.attrib&_A_SUBDIR) {
			if((new_pathv=realloc(glob->gl_pathv
				,(glob->gl_pathc+1)*sizeof(char*)))==NULL) {
				globfree(glob);
				return(GLOB_NOSPACE);
			}
			glob->gl_pathv=new_pathv;

			/* build the full pathname */
			strcpy(path,pattern);
			p=getfname(path);
			*p=0;
			strcat(path,ff.name);

			if((glob->gl_pathv[glob->gl_pathc]=malloc(strlen(path)+2))==NULL) {
				globfree(glob);
				return(GLOB_NOSPACE);
			}
			strcpy(glob->gl_pathv[glob->gl_pathc],path);
			if(flags&GLOB_MARK && ff.attrib&_A_SUBDIR)
				strcat(glob->gl_pathv[glob->gl_pathc],"/");

			glob->gl_pathc++;
			found++;
		}
		if(_findnext(ff_handle, &ff)!=0) {
			_findclose(ff_handle);
			ff_handle=-1; 
		} 
	}

	if(found==0)
		return(GLOB_NOMATCH);

	if(!(flags&GLOB_NOSORT)) {
		qsort(glob->gl_pathv,found,sizeof(char*),glob_compare);
	}

	return(0);	/* success */
}

void DLLCALL globfree(glob_t* glob)
{
	size_t i;

	if(glob==NULL)
		return;

	if(glob->gl_pathv!=NULL) {
		for(i=0;i<glob->gl_pathc;i++)
			if(glob->gl_pathv[i]!=NULL)
				free(glob->gl_pathv[i]);

		free(glob->gl_pathv);
		glob->gl_pathv=NULL;
	}
	glob->gl_pathc=0;
}

#endif /* !defined(__unix__) */

/****************************************************************************/
/* Returns the time/date of the file in 'filename' in time_t (unix) format  */
/****************************************************************************/
time_t DLLCALL fdate(char *filename)
{
	struct stat st;

	if(access(filename,0)==-1)
		return(-1);

	if(stat(filename, &st)!=0)
		return(-1);

	return(st.st_mtime);
}

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

/****************************************************************************/
/* Returns TRUE if the filename specified is a directory					*/
/****************************************************************************/
BOOL DLLCALL isdir(char *filename)
{
	struct stat st;

	if(stat(filename, &st)!=0)
		return(FALSE);

	return((st.st_mode&S_IFDIR) ? TRUE : FALSE);
}

/****************************************************************************/
/* Returns the attributes (mode) for specified 'filename'					*/
/****************************************************************************/
int DLLCALL getfattr(char* filename)
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
	struct stat st;

	if(stat(filename, &st)!=0) {
		errno=ENOENT;
		return(-1L);
	}

	return(st.st_mode);
#endif
}

/****************************************************************************/
/* Generate a tone at specified frequency for specified milliseconds		*/
/* Thanks to Casey Martin for this code										*/
/****************************************************************************/
#ifdef __unix__
void DLLCALL sbbs_beep(int freq, int dur)
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
int DLLCALL sbbs_random(int n)
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
char* DLLCALL ultoa(ulong val, char* str, int radix)
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
/* Reverse characters of a string (provided by amcleod)						*/
/****************************************************************************/
#ifdef __unix__
char* strrev(char* str)
{
    char t, *i=str, *j=str+strlen(str);

    while (i<j) {
        t=*i; *(i++)=*(--j); *j=t;
    }
    return str;
}
#endif

/****************************************************************************/
/* Wrapper for Win32 create/begin thread function							*/
/* Uses POSIX threads														*/
/****************************************************************************/
#if defined(__unix__) && defined(SBBS)
#if defined(_POSIX_THREADS)
ulong _beginthread(void( *start_address )( void * )
		,unsigned stack_size, void *arglist)
{
	pthread_t	thread;
	pthread_attr_t attr;

	pthread_attr_init(&attr);     /* initialize attribute structure */

	/* set thread attributes to PTHREAD_CREATE_DETACHED which will ensure
	   that thread resources are freed on exit() */
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	if(pthread_create(&thread
		,&attr	/* default attributes */
		/* POSIX defines this arg as "void *(*start_address)" */
		,(void *) start_address
		,arglist)==0)
		return((int) thread /* thread handle */);

	return(-1);	/* error */
}
#else

#error "Need _beginthread implementation for non-POSIX thread library."

#endif

#endif	/* __unix__ */

/****************************************************************************/
/* Win32 implementation of POSIX sem_getvalue() function					*/
/****************************************************************************/
#ifdef _WIN32
int sem_getvalue(sem_t* psem, int* val)
{
	if(psem==NULL || val==NULL)
		return(-1);

	if(WaitForSingleObject(*(psem),0)==WAIT_OBJECT_0)
		*val=1;
	else
		*val=0;

	return(0);
}
#endif

/****************************************************************************/
/* Return free disk space in bytes (up to a maximum of 4GB)					*/
/****************************************************************************/
#ifdef _WIN32
	typedef BOOL(WINAPI * GetDiskFreeSpaceEx_t)
		(LPCTSTR,PULARGE_INTEGER,PULARGE_INTEGER,PULARGE_INTEGER); 
#endif

ulong DLLCALL getfreediskspace(char* path)
{
#ifdef _WIN32
	char			root[16];
	DWORD			TotalNumberOfClusters;
	DWORD			NumberOfFreeClusters;
	DWORD			BytesPerSector;
	DWORD			SectorsPerCluster;
	ULARGE_INTEGER	avail;
	ULARGE_INTEGER	size;
	GetDiskFreeSpaceEx_t GetDiskFreeSpaceEx;

	GetDiskFreeSpaceEx 
		= (GetDiskFreeSpaceEx_t)GetProcAddress(hK32,"GetDiskFreeSpaceExA");
 
	if (GetDiskFreeSpaceEx!=NULL) {	/* Windows 95-OSR2 or later */
		if(!GetDiskFreeSpaceEx(
			path,		// pointer to the directory name
			&avail,		// receives the number of bytes on disk avail to the caller
			&size,		// receives the number of bytes on disk
			NULL))		// receives the free bytes on disk
			return(0);
#ifdef _ANONYMOUS_STRUCT
		if(avail.HighPart)
#else
		if(avail.u.HighPart)
#endif
			return(0xffffffff);	/* 4GB max */

#ifdef _ANONYMOUS_STRUCT
		return(avail.LowPart);
#else
		return(avail.u.LowPart);
#endif
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


/* statfs is also used under FreeBSD */
#elif defined(__GLIBC__) || defined(__FreeBSD__)

	struct statfs fs;

    if (statfs(path, &fs) < 0)
    	return 0;

    return fs.f_bsize * fs.f_bavail;
    
#else

	#warning OS-specific code needed here
	return(0);

#endif
}
