/* genwrap.h */

/* General cross-platform development wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2008 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#ifndef _GENWRAP_H
#define _GENWRAP_H

#include <stdio.h>		/* sprintf */
#include <string.h>		/* strerror() */
#include "gen_defs.h"	/* ulong */
#include "wrapdll.h"	/* DLLEXPORT and DLLCALL */

#if defined(__unix__)
	#include <sched.h>		/* sched_yield */
	#include <time.h>	/* clock_t */
	#include <sys/time.h>	/* struct timeval */
	#include <strings.h>	/* strcasecmp() */
	#include <unistd.h>		/* usleep */

	#ifdef XPDEV_THREAD_SAFE
		#include <pthread.h>/* Check for GNU PTH libs */
		#ifdef _PTH_PTHREAD_H_
			#include <pth.h>
		#endif
		#define GetCurrentThreadId()	pthread_self()
	#endif
#elif defined(_WIN32)
	#include <process.h>	/* getpid() */
#endif

#if !defined(_WIN32)
	/* Simple Win32 function equivalents */
	#define GetCurrentProcessId()		getpid()
#endif

/* utime() support */
#if defined(_MSC_VER) || defined(__WATCOMC__)
	#include <sys/utime.h>
#else
	#include <utime.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

/*********************/
/* Compiler-specific */
/*********************/

/* Compiler Description */
#if defined(__BORLANDC__)

	#define DESCRIBE_COMPILER(str) SAFEPRINTF2(str,"BCC %X.%02X" \
		,__BORLANDC__>>8,__BORLANDC__&0xff);	

#elif defined(_MSC_VER)

	#define DESCRIBE_COMPILER(str) SAFEPRINTF(str,"MSC %u", _MSC_VER);

#elif defined(__GNUC__) && defined(__GNUC_PATCHLEVEL__)

	#define DESCRIBE_COMPILER(str) SAFEPRINTF3(str,"GCC %u.%u.%u" \
		,__GNUC__,__GNUC_MINOR__,__GNUC_PATCHLEVEL__);

#elif defined(__GNUC__) && defined(__GNUC_MINOR__)

	#define DESCRIBE_COMPILER(str) SAFEPRINTF2(str,"GCC %u.%u" \
		,__GNUC__,__GNUC_MINOR__);

#elif defined(__WATCOMC__)

	#define DESCRIBE_COMPILER(str) SAFEPRINTF(str,"WATC %d" \
		,__WATCOMC__);

#elif defined(__DMC__)	/* Digital Mars C/C++ */

	#define DESCRIBE_COMPILER(str) SAFEPRINTF(str,"DMC %X.%02X" \
		,__DMC__>>8,__DMC__&0xff);	

#else /* Unknown compiler */

	#define DESCRIBE_COMPILER(str) SAFECOPY(str,"UNKNOWN COMPILER");

#endif

/**********/
/* Macros */
/**********/

/* Target Platform Description */
#if defined(_WIN64)
	#define PLATFORM_DESC	"Win64"
#elif defined(_WIN32)
	#define PLATFORM_DESC	"Win32"
#elif defined(__OS2__)
	#define PLATFORM_DESC	"OS/2"
#elif defined(__MSDOS__)
	#define PLATFORM_DESC	"DOS"
#elif defined(__linux__)
	#define PLATFORM_DESC	"Linux"
#elif defined(__FreeBSD__)
	#define PLATFORM_DESC	"FreeBSD"
#elif defined(__OpenBSD__)
	#define PLATFORM_DESC	"OpenBSD"
#elif defined(__NetBSD__)
	#define PLATFORM_DESC	"NetBSD"
#elif defined(__APPLE__) && defined(__MACH__) && defined(__POWERPC__)
	#define PLATFORM_DESC	"MacOSX"
#elif defined(BSD)
	#define PLATFORM_DESC	"BSD"
#elif defined(__solaris__)
	#define PLATFORM_DESC	"Solaris"
#elif defined(__sun__)
	#define PLATFORM_DESC	"SunOS"
#elif defined(__gnu__)
	#define PLATFORM_DESC	"GNU/Hurd"
#elif defined(__QNX__)
	#define PLATFORM_DESC	"QNX"
#elif defined(__unix__)
	#define PLATFORM_DESC	"Unix"
#else
	#error "Need to describe target platform"
#endif

/*********************/
/* String Functionss */
/*********************/

#ifndef USE_SNPRINTF
	#define snprintf		safe_snprintf
#endif

#if defined(_MSC_VER) || defined(__MINGW32__) || defined(__DMC__)
#if !defined(snprintf)
	#define snprintf		_snprintf
#endif
	#define vsnprintf		_vsnprintf
#endif

#if defined(__WATCOMC__)
	#define vsnprintf(s,l,f,a)	vsprintf(s,f,a)
#endif

#if !defined(_MSC_VER) && !defined(__BORLANDC__) && !defined(__WATCOMC__)
	DLLEXPORT char* DLLCALL ultoa(ulong, char*, int radix);
#endif

#if defined(__unix__)
	DLLEXPORT char*	DLLCALL strupr(char* str);
	DLLEXPORT char*	DLLCALL strlwr(char* str);
	DLLEXPORT char* DLLCALL	strrev(char* str);
	#if !defined(stricmp)
		#define stricmp			strcasecmp
		#define strnicmp		strncasecmp
	#endif
#endif

/* Skip white-space chars at beginning of string */
DLLEXPORT char*		DLLCALL skipsp(char* str);
/* Truncate white-space chars off end of string */
DLLEXPORT char*		DLLCALL truncsp(char* str);
/* Truncate white-space chars off end of every \n-terminated line in string */
DLLEXPORT char*		DLLCALL truncsp_lines(char* str);
/* Truncate new-line chars off end of string */
DLLEXPORT char*		DLLCALL truncnl(char* str);

#if defined(__unix__)
	#define STRERROR(x)		strerror(x)
#else
	#define STRERROR(x)		truncsp(strerror(x))
#endif

/*********************/
/* Utility Functions */
/*********************/
/* Thunking for multi-threaded specific implementations of "errno" */
DLLEXPORT int DLLCALL	get_errno(void);

/**********************************/
/* Common Utility Macro Functions */
/**********************************/

#if defined(_WIN32)

	#define YIELD()			Sleep(1) /* Must sleep at least 1ms to avoid 100% CPU utilization */
	#define	MAYBE_YIELD()	Sleep(0)
	#define SLEEP(x)		Sleep(x)
	#define	popen			_popen
	#define pclose			_pclose
	#if !defined(_MSC_VER)	/* Conflicts with latest (Windows 2003 R2) PlatformSDK include/time.h */
		#define tzname			_tzname
	#endif

#elif defined(__OS2__)

	#define YIELD()			DosSleep(1)	/* Must sleep at least 1ms to avoid 100% CPU utilization */
	#define	MAYBE_YIELD()	DosSleep(0)
	#define SLEEP(x)		DosSleep(x)

#elif defined(__unix__) || defined(__APPLE__)

	#if defined(_PTH_PTHREAD_H_)
		#define SLEEP(x)		({ int sleep_msecs=x; struct timeval tv; \
								tv.tv_sec=(sleep_msecs/1000); tv.tv_usec=((sleep_msecs%1000)*1000); \
								pth_nap(tv); })
	#else
		#define SLEEP(x)		({	int sleep_msecs=x; struct timeval tv; \
								tv.tv_sec=(sleep_msecs/1000); tv.tv_usec=((sleep_msecs%1000)*1000); \
								select(0,NULL,NULL,NULL,&tv); })
	#endif

	#define YIELD()			SLEEP(1)

	#if defined(XPDEV_THREAD_SAFE)
		#if defined(__FreeBSD__)
			#define MAYBE_YIELD()			pthread_yield()
		#elif defined(_PTH_PTHREAD_H_)
			#define MAYBE_YIELD()			pth_yield(NULL)
		#elif defined(_POSIX_PRIORITY_SCHEDULING)
			#define MAYBE_YIELD()			sched_yield()
		#else
			#define MAYBE_YIELD()			YIELD()
		#endif
	#else
		#if defined(_POSIX_PRIORITY_SCHEDULING)
			#define	MAYBE_YIELD()			sched_yield()
		#else
			#define MAYBE_YIELD()			YIELD()
		#endif
	#endif

	/*
	 * QNX doesn't support fork() in threaded apps (yet) using vfork() instead
	 * works, but relies on undefined behaviours not being nasty.  On most OSs
	 * vfork() will share the stack between the parent and child...
	 */
	#if defined(__QNX__)
		#define FORK()	vfork()
	#else
		#define FORK()	fork()
	#endif


#else	/* Unsupported OS */

	#error "Unsupported Target: Need some macros and/or function prototypes here."

#endif

/* Command processor/shell environment variable name */
#ifdef __unix__
	#define OS_CMD_SHELL_ENV_VAR	"SHELL"
#else	/* DOS/Windows/OS2 */
	#define OS_CMD_SHELL_ENV_VAR	"COMSPEC"
#endif

/* Win32 implementations of recursive (thread-safe) std C time functions on Unix */

#if !defined(__unix__)	

	#include <time.h>		/* time_t, etc. */

	DLLEXPORT struct tm*    DLLCALL		gmtime_r(const time_t* t, struct tm* tm);
	DLLEXPORT struct tm*    DLLCALL		localtime_r(const time_t* t, struct tm* tm);
	DLLEXPORT char*	        DLLCALL		ctime_r(const time_t *t, char *buf);
	DLLEXPORT char*	        DLLCALL		asctime_r(const struct tm *tm, char *buf);
	DLLEXPORT char*			DLLCALL		strtok_r(char *str, const char *delim, char **last);

#endif

#if defined(__solaris__)
	#define CTIME_R(x,y)	ctime_r(x,y)
	/* #define CTIME_R(x,y)	ctime_r(x,y,sizeof y) */
#else
	#define CTIME_R(x,y)	ctime_r(x,y)
#endif

/* Mimic the Borland randomize() and random() CRTL functions */
DLLEXPORT unsigned	DLLCALL xp_randomize(void);
DLLEXPORT int		DLLCALL	xp_random(int);

DLLEXPORT long double  	DLLCALL	xp_timer(void);
DLLEXPORT char*		DLLCALL os_version(char *str);
DLLEXPORT char*		DLLCALL os_cmdshell(void);
DLLEXPORT char*		DLLCALL	lastchar(const char* str);
DLLEXPORT int		DLLCALL safe_snprintf(char *dst, size_t size, const char *fmt, ...);

/* C string/char escape-sequence processing */
DLLEXPORT char*		DLLCALL c_escape_str(const char* src, char* dst, size_t maxlen, BOOL ctrl_only);
DLLEXPORT char*		DLLCALL c_escape_char(char ch);
DLLEXPORT char*		DLLCALL c_unescape_str(char* str);
DLLEXPORT char		DLLCALL c_unescape_char_ptr(const char* str, char** endptr);
DLLEXPORT char		DLLCALL c_unescape_char(char ch);

/* Microsoft (e.g. DOS/Win32) real-time system clock API (ticks since process started) */
typedef		clock_t				msclock_t;
#if defined(_WIN32) || defined(__OS2__)
	#define		MSCLOCKS_PER_SEC	CLOCKS_PER_SEC	/* e.g. 18.2 on DOS, 1000.0 on Win32 */
	#define		msclock()			clock()
#else
	#define		MSCLOCKS_PER_SEC	1000
	msclock_t	msclock(void);
#endif

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
