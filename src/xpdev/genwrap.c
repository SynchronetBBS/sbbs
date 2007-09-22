/* genwrap.c */

/* General cross-platform development wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2007 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#endif	/* __unix__ */

#include "genwrap.h"	/* Verify prototypes */
#include "xpendian.h"	/* BYTE_SWAP */

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
char DLLCALL c_unescape_char(char ch)
{
	switch(ch) {
		case 'e':	return(ESC);	/* non-standard */
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
char DLLCALL c_unescape_char_ptr(const char* str, char** endptr)
{
	char	ch;

	if(toupper(*str)=='X')
		ch=(char)strtol(++str,endptr,16);
	else if(isdigit(*str))
		ch=(char)strtol(++str,endptr,8);
	else {
		ch=c_unescape_char(*(str++));
		if(endptr!=NULL)
			*endptr=(char*)str;
	}

	return(ch);
}

/****************************************************************************/
/* Unescape a C string, in place											*/
/****************************************************************************/
char* DLLCALL c_unescape_str(char* str)
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
			ch=c_unescape_char_ptr(src,&src);
		*(dst++)=ch;
	}
	*dst=0;
	free(buf);
	return(str);
}

char* DLLCALL c_escape_char(char ch)
{
	switch(ch) {
		case 0:		return("\\x00");
		case 1:		return("\\x01");
		case ESC:	return("\\e");		/* non-standard */
		case '\a':	return("\\a");
		case '\b':	return("\\b");
		case '\f':	return("\\f");
		case '\n':	return("\\n");
		case '\r':	return("\\r");
		case '\t':	return("\\t");
		case '\v':	return("\\v");
		case '\\':	return("\\\\");
		case '\"':	return("\\\"");
		case '\'':	return("\\'");
	}
	return(NULL);
}

char* DLLCALL c_escape_str(const char* src, char* dst, size_t maxlen, BOOL ctrl_only)
{
	const char*	s;
	char*	d;
	char*	e;

	for(s=src,d=dst;*s && (size_t)(d-dst)<maxlen;s++,d++) {
		if((!ctrl_only || (uchar)*s < ' ') && (e=c_escape_char(*s))!=NULL) {
			*d=0;
			strncat(dst,e,maxlen-(d-dst));
			d++;
		} else *d=*s;
	}
	*d=0;

	return(dst);
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
#endif

#if !defined(__unix__)

/****************************************************************************/
/* Implementations of the recursive (thread-safe) version of strtok			*/
/* Thanks to Apache Portable Runtime (APR)									*/
/****************************************************************************/
char* DLLCALL strtok_r(char *str, const char *delim, char **last)
{
    char* token;

    if (str==NULL)      /* subsequent call */
        str = *last;    /* start where we left off */

    /* skip characters in delimiter (will terminate at '\0') */
    while(*str && strchr(delim, *str))
        ++str;

    if(!*str) {         /* no more tokens */
		*last = str;
        return NULL;
	}

    token = str;

    /* skip valid token characters to terminate token and
     * prepare for the next call (will terminate at '\0)
     */
    *last = token + 1;
    while(**last && !strchr(delim, **last))
        ++*last;

    if (**last) {
        **last = '\0';
        ++*last;
    }

    return token;
}

#endif

/****************************************************************************/
/* Initialize (seed) the random number generator							*/
/****************************************************************************/
unsigned DLLCALL xp_randomize(void)
{
	unsigned seed=~0;

#if defined(HAS_DEV_URANDOM) && defined(URANDOM_DEV)
	int     rf;

	if((rf=open(URANDOM_DEV, O_RDONLY))!=-1) {
		read(rf, &seed, sizeof(seed));
		close(rf);
	}
	else {
#endif
		unsigned curtime	= (unsigned)time(NULL);
		unsigned process_id = (unsigned)GetCurrentProcessId();

		seed = curtime ^ BYTE_SWAP_INT(process_id);

		#if defined(_WIN32) || defined(GetCurrentThreadId)
			seed ^= (unsigned)GetCurrentThreadId();
		#endif

#if defined(HAS_DEV_URANDOM) && defined(URANDOM_DEV)
	}
#endif

#ifdef HAS_RANDOM_FUNC
 	srandom(seed);
	return(seed);
#else
 	srand(seed);
	return(seed);
#endif
}

/****************************************************************************/
/* Return random number between 0 and n-1									*/
/****************************************************************************/
int DLLCALL xp_random(int n)
{
#ifdef HAS_RANDOM_FUNC
	if(n<2)
		return(0);
	return(random()%n);
#else
	float f=0;

	if(n<2)
		return(0);
	f=(float)rand()/(float)RAND_MAX;

	return((int)(n*f));
#endif
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

char* DLLCALL os_cmdshell(void)
{
	char*	shell=getenv(OS_CMD_SHELL_ENV_VAR);

#if defined(__unix__)
	if(shell==NULL)
#ifdef _PATH_BSHELL
		shell=_PATH_BSHELL;
#else
		shell="/bin/sh";
#endif
#endif

	return(shell);
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

/****************************************************************/
/* Microsoft (DOS/Win32) real-time system clock implementation.	*/
/****************************************************************/
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
/* Skips all white-space chars at beginning of 'str'						*/
/****************************************************************************/
char* DLLCALL skipsp(char* str)
{
	SKIP_WHITESPACE(str);
	return(str);
}

/****************************************************************************/
/* Truncates all white-space chars off end of 'str'	(needed by STRERRROR)	*/
/****************************************************************************/
char* DLLCALL truncsp(char* str)
{
	size_t i,len;

	i=len=strlen(str);
	while(i && isspace(str[i-1])) 
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

/****************************************************************************/
/* Returns the current value of the systems best timer (in SECONDS)			*/
/* Any value < 0 indicates an error											*/
/****************************************************************************/
long double	DLLCALL	xp_timer(void)
{
	long double ret;
#if defined(__unix__)
	struct timeval tv;
	if(gettimeofday(&tv,NULL)==1)
		return(-1);
	ret=tv.tv_usec;
	ret /= 1000000;
	ret += tv.tv_sec;
#elif defined(_WIN32)
	LARGE_INTEGER	freq;
	LARGE_INTEGER	tick;
	if(QueryPerformanceFrequency(&freq) && QueryPerformanceCounter(&tick)) {
#if 0
		ret=((long double)tick.HighPart*4294967296)+((long double)tick.LowPart);
		ret /= ((long double)freq.HighPart*4294967296)+((long double)freq.LowPart);
#else
		/* In MSVC, a long double does NOT have 19 decimals of precision */
		ret=(((long double)(tick.QuadPart%freq.QuadPart))/freq.QuadPart);
		ret+=tick.QuadPart/freq.QuadPart;
#endif
	}
	else {
		ret=GetTickCount();
		ret /= 1000;
	}
#else
	ret=time(NULL);	/* Weak implementation */
#endif
	return(ret);
}
