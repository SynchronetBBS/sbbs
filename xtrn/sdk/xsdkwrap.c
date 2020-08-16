/* xsdkwrap.c */

/* Synchronet XSDK system-call wrappers (compiler & platform portability) */

/* $Id: xsdkwrap.c,v 1.21 2007/06/29 21:01:35 rswindell Exp $ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2006 Rob Swindell - http://www.synchro.net/copyright.html		*
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
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout xtrn	*
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
#if defined(_WIN32)

	#include <windows.h>	/* DWORD */

#elif defined(__unix__)

	#include <glob.h>       /* glob() wildcard matching */
	#include <string.h>     /* strlen() */
	#include <unistd.h>     /* getpid() */
	#include <fcntl.h>      /* fcntl() file/record locking */
	#include <stdlib.h>
	#include <unistd.h>
	#include <termios.h>
	#if defined(__FreeBSD__)
	#include <sys/kbio.h>
	#elif defined(__Linux__)
	#include <sys/kd.h>
	#endif
	#include <sys/time.h>
	#include <sys/types.h>
	#include <signal.h>
	#include <errno.h>
	#include <sys/file.h>	/* L_SET in SunOS (!) */

#endif

/* ANSI */
#include <stdarg.h>		/* va_list */
#include <sys/types.h>	/* _dev_t */
#include <sys/stat.h>	/* struct stat */

/* XSDK-specific */
#include "xsdkdefs.h"	/* MAX_PATH */
#include "xsdkwrap.h"	/* Verify prototypes */

#ifdef _WIN32
#define stat(f,s)	_stat(f,s)
#define STAT		struct _stat
#else
#define STAT		struct stat
#endif

#if defined(__unix__)
/****************************************************************************/
/* Wrapper for Win32 create/begin thread function							*/
/* Uses POSIX threads														*/
/****************************************************************************/
#if defined(_POSIX_THREADS)
#if defined(__BORLANDC__)
        #pragma argsused
#endif
ulong _beginthread(void( *start_address )( void * )
		,unsigned stack_size, void *arglist)
{
	pthread_t	thread;
	pthread_attr_t attr;

	pthread_attr_init(&attr);     /* initialize attribute structure */

	/* set thread attributes to PTHREAD_CREATE_DETACHED which will ensure
	   that thread resources are freed on exit() */
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* Default stack size in BSD is too small for JS stuff */
#ifdef BSD
	if(stack_size==0)
		stack_size=1<<17;
#endif
	if(stack_size!=0)
		pthread_attr_setstacksize(&attr, stack_size);

	if(pthread_create(&thread
#if defined(__BORLANDC__) /* a (hopefully temporary) work-around */
		,NULL
#else
		,&attr	/* default attributes */
#endif
		/* POSIX defines this arg as "void *(*start_address)" */
		,(void * (*)(void *)) start_address
		,arglist)==0)
		return((int) thread /* thread handle */);

	return(-1);	/* error */
}
#else

#error "Need _beginthread implementation for non-POSIX thread library."

#endif

/****************************************************************************/
/* Convert ASCIIZ string to upper case										*/
/****************************************************************************/
char* strupr(char* str)
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
char* strlwr(char* str)
{
	char*	p=str;

	while(*p) {
		*p=tolower(*p);
		p++;
	}
	return(str);
}

/****************************************************************************/
/* Reverse chars in string													*/
/****************************************************************************/
char* strrev(char* str)
{
    char t, *i=str, *j=str+strlen(str);

    while (i<j) {
        t=*i; *(i++)=*(--j); *j=t;
    }
    return str;
}

char * _fullpath(char *target, const char *path, size_t size)  {
	char	*out;
	char	*p;
	
	if(target==NULL)  {
		if((target=malloc(MAX_PATH+1))==NULL) {
			return(NULL);
		}
	}
	out=target;
	*out=0;

	if(*path != '/')  {
		p=getcwd(target,size);
		if(p==NULL || strlen(p)+strlen(path)>=size)
			return(NULL);
		out=strrchr(target,'\0');
		*(out++)='/';
		*out=0;
		out--;
	}
	strncat(target,path,size-1);
	
/*	if(stat(target,&sb))
		return(NULL);
	if(sb.st_mode&S_IFDIR)
		strcat(target,"/"); */

	for(;*out;out++)  {
		while(*out=='/')  {
			if(*(out+1)=='/')
				memmove(out,out+1,strlen(out));
			else if(*(out+1)=='.' && (*(out+2)=='/' || *(out+2)==0))
				memmove(out,out+2,strlen(out)-1);
			else if(*(out+1)=='.' && *(out+2)=='.' && (*(out+3)=='/' || *(out+3)==0))  {
				*out=0;
				p=strrchr(target,'/');
				memmove(p,out+3,strlen(out+3)+1);
				out=p;
			}
			else  {
				out++;
			}
		}
	}
	return(target);
}

/***************************************/
/* Console I/O Stuff (by Casey Martin) */
/***************************************/

static struct termios current;				// our current term settings
static struct termios original;				// old termios settings
static int beensetup = 0;					// has _termios_setup() been called?


/* Resets the termios to its previous state */
void _termios_reset(void)
{
	if(isatty(0))
		tcsetattr(0, TCSANOW, &original);
}

/************************************************
  This pair of functions handles Ctrl-Z presses
************************************************/

void _sighandler_stop(int sig)
{
    // clean up the terminal
    _termios_reset();

    // ... and stop
	kill(getpid(), SIGSTOP);
}

void _sighandler_cont(int sig)
{
    // restore terminal
	if(isatty(0))
		tcsetattr(0, TCSANOW, &current);
}


/* Prepares termios for non-blocking action */
void _termios_setup(void)
{
	beensetup = 1;
    
	if(isatty(0)) {
		tcgetattr(0, &original);

		memcpy(&current, &original, sizeof(struct termios));
		current.c_cc[VMIN] = 1;           // read() will return with one char
		current.c_cc[VTIME] = 0;          // read() blocks forever
		current.c_lflag &= ~ICANON;       // character mode
    	current.c_lflag &= ~ECHO;         // turn off echoing
		tcsetattr(0, TCSANOW, &current);

    	// Let's install an exit function, also.  This way, we can reset
    	// the termios silently
    	atexit(_termios_reset);

    	// install the Ctrl-Z handler
    	signal(SIGSTOP, _sighandler_stop);
    	signal(SIGCONT, _sighandler_cont);
	}
}


int kbhit(void)
{
	struct timeval timeout;		// passed in select() call
	fd_set inp;					// ditto

	/* 
	 * What the heck?  The ONE thing that would work properly if stdin isn't a
	 * terminal... and IT is avoided!
	 * 
	 * if(!isatty(0))
	 *	return(0);
	 */

	/* set up select() args */
	timeout.tv_sec=0;
	timeout.tv_usec=0;
	FD_ZERO(&inp);
	FD_SET(0, &inp);

	return select(1, &inp, NULL, NULL, &timeout);
}

int getch(void)
{
	char c;

    if (!beensetup)
    	// I hate to test for this every time, but this shouldn't be
        // called that often anyway...
    	_termios_setup();

    // get a char out of stdin
    read(0, &c, 1);

    return c;
}

#endif // __unix__

/**************/
/* File Stuff */
/**************/

#if !defined(USE_XPDEV)

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
/* Checks the file system for the existence of one or more files.			*/
/* Returns TRUE if it exists, FALSE if it doesn't.                          */
/* 'filespec' may contain wildcards!										*/
/****************************************************************************/
BOOL fexist(char *filespec)
{
#if defined(_WIN32)

	long	handle;
	struct _finddata_t f;

	if(access(filespec,0)==-1 && !strchr(filespec,'*') && !strchr(filespec,'?'))
		return(FALSE);

	if((handle=_findfirst((char*)filespec,&f))==-1)
		return(FALSE);

 	_findclose(handle);

 	if(f.attrib&_A_SUBDIR)
		return(FALSE);

	return(TRUE);

#elif defined(__unix__)

	/* portion by cmartin */

	glob_t g;
    int c;
	char *p;

	if(access(filespec,0)==-1 && !strchr(filespec,'*') && !strchr(filespec,'?'))
		return(FALSE);

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
		p=strchr(g.gl_pathv[c],0);
		if(p!=NULL) {
			p--;
	    	if (*p != '/') {
    	    	globfree(&g);
        	    return TRUE;
	        }
		}
    }

    globfree(&g);
    return FALSE;

#else

	#error "fexist() port needs to support wildcards!"

	return(FALSE);

#endif
}

#endif

#if defined(__unix__)

/****************************************************************************/
/* Returns the length of the file in 'fd'									*/
/****************************************************************************/
long filelength(int fd)
{
	STAT st;

	if(fstat(fd, &st)!=0)
		return(-1L);

	return(st.st_size);
}

#include <stdio.h>
#include <errno.h>
/* Sets a lock on a portion of a file */
int lock(int fd, long pos, long len)
{
	#if defined(F_SANERDLCKNO) || !(defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__))
 		struct flock alock;

	#ifndef F_SANEWRLCKNO
		int	flags;
		if((flags=fcntl(fd,F_GETFL))==-1)
			return -1;

		if(flags==O_RDONLY)
			alock.l_type = F_RDLCK; /* set read lock to prevent writes */
		else
			alock.l_type = F_WRLCK; /* set write lock to prevent all access */
	#else
		alock.l_type = F_SANEWRLCKNO;
	#endif
		alock.l_whence = L_SET;		/* SEEK_SET */
		alock.l_start = pos;
		alock.l_len = (int)len;

		if(fcntl(fd, F_SETLK, &alock)==-1 && errno != EINVAL)
			return(-1);
	#endif

	#if !defined(F_SANEWRLCKNO) && !defined(__QNX__) && !defined(__solaris__)
		/* use flock (doesn't work over NFS) */
		if(flock(fd,LOCK_EX|LOCK_NB)!=0 && errno == EWOULDBLOCK)
			return(-1);
	#endif

		return(0);
}

/* Removes a lock from a file record */
int unlock(int fd, long pos, long len)
{

#if defined(F_SANEUNLCK) || !(defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__))
	struct flock alock;
#ifdef F_SANEUNLCK
	alock.l_type = F_SANEUNLCK;   /* remove the lock */
#else
	alock.l_type = F_UNLCK;   /* remove the lock */
#endif
	alock.l_whence = L_SET;
	alock.l_start = pos;
	alock.l_len = (int)len;
	if(fcntl(fd, F_SETLK, &alock)==-1 && errno != EINVAL)
		return(-1);
#endif

#if !defined(F_SANEUNLCK) && !defined(__QNX__) && !defined(__solaris__)
	/* use flock (doesn't work over NFS) */
	if(flock(fd,LOCK_UN|LOCK_NB)!=0 && errno != EOPNOTSUPP)
		return(-1);
#endif

	return(0);
}

/* Opens a file in specified sharing (file-locking) mode */
#if !defined(__QNX__)
int sopen(const char *fn, int access, int share, ...)
{
	int fd;
	int pmode=S_IREAD;
#ifndef F_SANEWRLCKNO
	int	flock_op=LOCK_NB;	/* non-blocking */
#endif
#if defined(F_SANEWRLCKNO) || !(defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__))
	struct flock alock;
#endif
    va_list ap;

    if(access&O_CREAT) {
        va_start(ap,share);
        pmode = va_arg(ap,unsigned int);
        va_end(ap);
    }

	if ((fd = open(fn, access, pmode)) < 0)
		return -1;

	if (share == SH_DENYNO) /* no lock needed */
		return fd;
#if defined(F_SANEWRLCKNO) || !(defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__))
	/* use fcntl (doesn't work correctly with threads) */
	alock.l_type = share;
	alock.l_whence = L_SET;
	alock.l_start = 0;
	alock.l_len = 0;       /* lock to EOF */

	if(fcntl(fd, F_SETLK, &alock)==-1 && errno != EINVAL) {	/* EINVAL means the file does not support locking */
		close(fd);
		return -1;
	}
#endif

#if !defined(F_SANEWRLCKNO) && !defined(__QNX__) && !defined(__solaris__)
	/* use flock (doesn't work over NFS) */
	if(share==SH_DENYRW)
		flock_op|=LOCK_EX;
	else   /* SH_DENYWR */
		flock_op|=LOCK_SH;
	if(flock(fd,flock_op)!=0 && errno != EOPNOTSUPP) { /* That object doesn't do locks */
		if(errno==EWOULDBLOCK) 
			errno=EAGAIN;
		close(fd);
		return(-1);
	}
#endif

	return fd;
}
#endif /* !QNX */

#elif defined _MSC_VER || defined __MINGW32__

#include <io.h>				/* tell */
#include <stdio.h>			/* SEEK_SET */
#include <sys/locking.h>	/* _locking */

/* Fix MinGW locking.h typo */
#if defined LK_UNLOCK && !defined LK_UNLCK
	#define LK_UNLCK LK_UNLOCK
#endif

int lock(int file, long offset, long size) 
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

int unlock(int file, long offset, long size)
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

/****************************************************************************/
/* Return ASCII string representation of ulong								*/
/* There may be a native GNU C Library function to this...					*/
/****************************************************************************/
#if !defined _MSC_VER && !defined __BORLANDC__
char* ultoa(unsigned long val, char* str, int radix)
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
