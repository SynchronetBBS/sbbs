/* genwrap.c */

/* General cross-platform development wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <string.h>     /* strlen() */
#include <stdarg.h>	/* vsnprintf() */
#include <stdlib.h>		/* RAND_MAX */
#include <fcntl.h>		/* O_NOCTTY */
#include <time.h>		/* clock() */
#include <errno.h>		/* errno */
#include <ctype.h>		/* toupper/tolower */

#if defined(__unix__)
	#include <sys/ioctl.h>		/* ioctl() */
	#include <sys/utsname.h>	/* uname() */
	/* KIOCSOUND */
	#if defined(__FreeBSD__)
		#include <sys/kbio.h>
	#elif defined(__linux__)
		#include <sys/kd.h>	
	#elif defined(__solaris__)
		#include <sys/kbio.h>
		#include <sys/kbd.h>
	#endif
	#if defined(__OpenBSD__) || defined(__NetBSD__)
		#include <machine/spkr.h>
	#elif defined(__FreeBSD__)
		#include <machine/speaker.h>
	#endif
#endif	/* __unix__ */

#include "genwrap.h"	/* Verify prototypes */

/****************************************************************************/
/* Used to replace snprintf()  guarantees to terminate.			  			*/
/****************************************************************************/
int DLLCALL safe_snprintf(char *dst, size_t size, const char *fmt, ...)
{
	va_list argptr;
	int     numchars;

	va_start(argptr,fmt);
	numchars= vsnprintf(dst,size,fmt,argptr);
	va_end(argptr);
	dst[size-1]=0;
#ifdef _MSC_VER
	if(numchars==-1)
		numchars=strlen(dst);
#endif
	if(numchars>=(int)size && numchars>0)
		numchars=size-1;
	return(numchars);
}

/****************************************************************************/
/* Return last character of string											*/
/****************************************************************************/
char* DLLCALL lastchar(const char* str)
{
	size_t	len;

	len = strlen(str);

	if(len)
		return((char*)&str[len-1]);

	return((char*)str);
}

/****************************************************************************/
/* Return character value of C-escaped (\) character						*/
/****************************************************************************/
char DLLCALL unescape_char(char ch)
{
	switch(ch) {
		case '\\':	return('\\');
		case '\'':	return('\'');
		case '"':	return('"');
		case '?':	return('?');
		case 'a':	return('\a');
		case 'b':	return('\b');
		case 'f':	return('\f');
		case 'n':	return('\n');
		case 'r':	return('\r');
		case 't':	return('\t');
		case 'v':	return('\v');
	}
	return(ch);
}

/****************************************************************************/
/* Return character value of C-escaped (\) character sequence				*/
/* (supports \Xhh and \0ooo escape sequences)								*/
/* This code currently has problems with sequences like: "\x01blue"			*/
/****************************************************************************/
char DLLCALL unescape_char_ptr(const char* str, char** endptr)
{
	char	ch;

	if(toupper(*str)=='X')
		ch=(char)strtol(++str,endptr,16);
	else if(isdigit(*str))
		ch=(char)strtol(++str,endptr,8);
	else {
		ch=unescape_char(*(str++));
		if(endptr!=NULL)
			*endptr=(char*)str;
	}

	return(ch);
}

/****************************************************************************/
/* Unescape a C string, in place											*/
/****************************************************************************/
char* DLLCALL unescape_cstr(char* str)
{
	char	ch;
	char*	buf;
	char*	src;
	char*	dst;

	if(str==NULL || (buf=strdup(str))==NULL)
		return(NULL);

	src=buf;
	dst=str;
	while((ch=*(src++))!=0) {
		if(ch=='\\')	/* escape */
			ch=unescape_char_ptr(src,&src);
		*(dst++)=ch;
	}
	*dst=0;
	free(buf);
	return(str);
}

/****************************************************************************/
/* Convert ASCIIZ string to upper case										*/
/****************************************************************************/
#if defined(__unix__)
char* DLLCALL strupr(char* str)
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
char* DLLCALL strlwr(char* str)
{
	char*	p=str;

	while(*p) {
		*p=tolower(*p);
		p++;
	}
	return(str);
}
/****************************************************************************/
/* Reverse characters of a string (provided by amcleod)						*/
/****************************************************************************/
char* strrev(char* str)
{
    char t, *i=str, *j=str+strlen(str);

    while (i<j) {
        t=*i; *(i++)=*(--j); *j=t;
    }
    return str;
}

/****************************************************************************/
/* Generate a tone at specified frequency for specified milliseconds		*/
/* Thanks to Casey Martin for this code										*/
/****************************************************************************/
void DLLCALL unix_beep(int freq, int dur)
{
	static int console_fd=-1;

#if defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__NetBSD__)
	int speaker_fd=-1;
	tone_t tone;

	speaker_fd = open("/dev/speaker", O_WRONLY|O_APPEND);
	if(speaker_fd != -1)  {
		tone.frequency=freq;
		tone.duration=dur/10;
		ioctl(speaker_fd,SPKRTONE,&tone);
		close(speaker_fd);
		return;
	}
#endif

#if !defined(__GNU__) && !defined(__QNX__) && !defined(__OpenBSD__) && !defined(__NetBSD__) && !defined(__APPLE__)
	if(console_fd == -1) 
  		console_fd = open("/dev/console", O_NOCTTY);
	
	if(console_fd != -1) {
#if defined(__solaris__)
		ioctl(console_fd, KIOCCMD, KBD_CMD_BELL);
#else
		if(freq != 0)	/* Don't divide by zero */
			ioctl(console_fd, KIOCSOUND, (int) (1193180 / freq));
#endif /* solaris */
		SLEEP(dur);
#if defined(__solaris__)
		ioctl(console_fd, KIOCCMD, KBD_CMD_NOBELL);	/* turn off tone */
#else
		ioctl(console_fd, KIOCSOUND, 0);	/* turn off tone */
#endif /* solaris */
	}
#endif
}
#endif

/****************************************************************************/
/* Return random number between 0 and n-1									*/
/****************************************************************************/
int DLLCALL xp_random(int n)
{
	float f;
	static BOOL initialized;

	if(!initialized) {
		srand(time(NULL));	/* seed random number generator */
		rand();				/* throw away first result */
		initialized=TRUE;
	}
	if(n<2)
		return(0);
	f=(float)rand()/(float)RAND_MAX;

	return((int)(n*f));
}

/****************************************************************************/
/* Return ASCII string representation of ulong								*/
/* There may be a native GNU C Library function to this...					*/
/****************************************************************************/
#if !defined(_MSC_VER) && !defined(__BORLANDC__) && !defined(__WATCOMC__)
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
/* Write the version details of the current operating system into str		*/
/****************************************************************************/
char* DLLCALL os_version(char *str)
{
#if defined(__OS2__) && defined(__BORLANDC__)

	sprintf(str,"OS/2 %u.%u (%u.%u)",_osmajor/10,_osminor/10,_osmajor,_osminor);

#elif defined(_WIN32)

	/* Windows Version */
	char*			winflavor="";
	OSVERSIONINFO	winver;

	winver.dwOSVersionInfoSize=sizeof(winver);
	GetVersionEx(&winver);

	switch(winver.dwPlatformId) {
		case VER_PLATFORM_WIN32_NT:
			winflavor="NT ";
			break;
		case VER_PLATFORM_WIN32s:
			winflavor="Win32s ";
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			winver.dwBuildNumber&=0xffff;
			break;
	}

	sprintf(str,"Windows %sVersion %u.%u (Build %u) %s"
			,winflavor
			,winver.dwMajorVersion, winver.dwMinorVersion
			,winver.dwBuildNumber,winver.szCSDVersion);

#elif defined(__unix__)

	struct utsname unixver;

	if(uname(&unixver)<0)
		sprintf(str,"Unix (uname errno: %d)",errno);
	else
		sprintf(str,"%s %s %s"
			,unixver.sysname	/* e.g. "Linux" */
			,unixver.release	/* e.g. "2.2.14-5.0" */
			,unixver.machine	/* e.g. "i586" */
			);

#else	/* DOS */

	sprintf(str,"DOS %u.%02u",_osmajor,_osminor);

#endif

	return(str);
}

#if !defined(__unix__)

/****************************************************************************/
/* Win32 implementations of the recursive (thread-safe) versions of std C	*/
/* time functions (gmtime, localtime, ctime, and asctime) used in Unix.		*/
/* The native Win32 versions of these functions are already thread-safe.	*/
/****************************************************************************/

struct tm* DLLCALL gmtime_r(const time_t* t, struct tm* tm)
{
	struct tm* tmp = gmtime(t);

	if(tmp==NULL)
		return(NULL);

	*tm = *tmp;
	return(tm);
}

struct tm* DLLCALL localtime_r(const time_t* t, struct tm* tm)
{
	struct tm* tmp = localtime(t);

	if(tmp==NULL)
		return(NULL);

	*tm = *tmp;
	return(tm);
}

char* DLLCALL ctime_r(const time_t *t, char *buf)
{
	char* p = ctime(t);

	if(p==NULL)
		return(NULL);

	strcpy(buf,p);
	return(buf);
}

char* DLLCALL asctime_r(const struct tm *tm, char *buf)
{
	char* p = asctime(tm);

	if(p==NULL)
		return(NULL);

	strcpy(buf,p);
	return(buf);
}

#endif	/* !defined(__unix__) */

/********************************************/
/* Hi-res real-time clock implementation.	*/
/********************************************/
#ifdef __unix__
clock_t DLLCALL msclock(void)
{
	long long int usecs;
	struct timeval tv;
	if(gettimeofday(&tv,NULL)==1)
		return(-1);
	usecs=tv.tv_sec*1000000+tv.tv_usec;
	return((clock_t)(usecs/(1000000/MSCLOCKS_PER_SEC)));
}
#endif

/****************************************************************************/
/* Truncates all white-space chars off end of 'str'	(needed by STRERRROR)	*/
/****************************************************************************/
char* DLLCALL truncsp(char* str)
{
	size_t i,len;

	i=len=strlen(str);
	while(i && (str[i-1]==' ' || str[i-1]=='\t' || str[i-1]=='\r' || str[i-1]=='\n')) 
		i--;
	if(i!=len)
		str[i]=0;	/* truncate */

	return(str);
}

/****************************************************************************/
/* Truncates all white-space chars off end of \n-terminated lines in 'str'	*/
/****************************************************************************/
char* DLLCALL truncsp_lines(char* dst)
{
	char* sp;
	char* dp;
	char* src;

	if((src=strdup(dst))==NULL)
		return(dst);

	for(sp=src, dp=dst; *sp!=0; sp++) {
		if(*sp=='\n')
			while(dp!=dst 
				&& (*(dp-1)==' ' || *(dp-1)=='\t' || *(dp-1)=='\r') && *(dp-1)!='\n') 
					dp--;
		*(dp++)=*sp;
	}
	*dp=0;

	free(src);
	return(dst);
}

/****************************************************************************/
/* Truncates carriage-return and line-feed chars off end of 'str'			*/
/****************************************************************************/
char* DLLCALL truncnl(char* str)
{
	size_t i,len;

	i=len=strlen(str);
	while(i && (str[i-1]=='\r' || str[i-1]=='\n')) 
		i--;
	if(i!=len)
		str[i]=0;	/* truncate */

	return(str);
}

/****************************************************************************/
/* Return errno from the proper C Library implementation					*/
/* (single/multi-threaded)													*/
/****************************************************************************/
int DLLCALL	get_errno(void)
{
	return(errno);
}
