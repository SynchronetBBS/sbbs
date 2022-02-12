/* General cross-platform development wrappers */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _GENWRAP_H
#define _GENWRAP_H

#include <stdio.h>		/* sprintf */
#include <string.h>		/* strerror() */
#include <time.h>		/* clock_t */
#include "gen_defs.h"	/* ulong */
#include "wrapdll.h"	/* DLLEXPORT and */

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
	#ifndef __MINGW32__
        typedef DWORD pid_t;
	#endif
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

/*
 * The alloca() function can't be implemented in C, and on some
 * platforms it can't be implemented at all as a callable function.
 * The GNU C compiler provides a built-in alloca() which we can use;
 * On platforms where alloca() is not in libc, programs which use
 * it will fail to link when compiled with non-GNU compilers.
 */
#if __GNUC__ >= 2 || defined(__INTEL_COMPILER)
#undef  alloca  /* some GNU bits try to get cute and define this on their own */
#define alloca(sz) __builtin_alloca(sz)
#elif defined(_WIN32)
#include <malloc.h>
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

#elif defined(__clang__) && defined(__clang_patchlevel__)

	#define DESCRIBE_COMPILER(str) SAFEPRINTF3(str,"Clang %u.%u.%u" \
		,__clang_major__,__clang_minor__,__clang_patchlevel__);

#elif defined(__clang__) && defined(__clang_minor__)

	#define DESCRIBE_COMPILER(str) SAFEPRINTF2(str,"Clang %u.%u" \
		,__clang_major__,__clang_minor__);

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
#elif defined(__HAIKU__)
	#define PLATFORM_DESC	"Haiku"
#else
	#error "Need to describe target platform"
#endif

#if defined(_M_X64) || defined(_M_AMD64) || defined(__x86_64__) || defined(__amd64__)
	#define ARCHITECTURE_DESC "x64"
#elif defined(__i386__) || _M_IX86 == 300
	#define ARCHITECTURE_DESC "i386"
#elif defined(__i486__) || _M_IX86 == 400
	#define ARCHITECTURE_DESC "i486"
#elif defined(__i586__) || _M_IX86 == 500
	#define ARCHITECTURE_DESC "i586"
#elif defined(__i686__) || _M_IX86 == 600
	#define ARCHITECTURE_DESC "i686"
#elif defined(__i786__) || _M_IX86 == 700
	#define ARCHITECTURE_DESC "i786"
#elif defined(_X86_) || defined(__x86__) || defined(_M_IX86)
	#define ARCHITECTURE_DESC "x86"
#elif defined(__mips__)
	#define ARCHITECTURE_DESC "mips"
#elif __ARM_ARCH == 5 || _M_ARM == 5
	#define ARCHITECTURE_DESC "armv5"
#elif __ARM_ARCH == 6 || _M_ARM == 6
	#define ARCHITECTURE_DESC "armv6"
#elif __ARM_ARCH == 7 || _M_ARM == 7
	#define ARCHITECTURE_DESC "armv7"
#elif __ARM_ARCH == 8 || _M_ARM == 8 
	#define ARCHITECTURE_DESC "armv8"
#elif defined(__aarch64__)
	#define ARCHITECTURE_DESC "arm64"
#elif defined(__arm__)
	#define ARCHITECTURE_DESC "arm"
#elif defined(_M_PPC) || defined(__ppc__)
	#define ARCHITECTURE_DESC "ppc"
#elif defined(_M_IA64) || defined(__ia64__)
	#define ARCHITECTURE_DESC "ia64"
#else
	#error "Need to describe target architecture"
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
	#define NEEDS_STRLCPY
#endif

#if defined(__WATCOMC__)
	#define vsnprintf(s,l,f,a)	vsprintf(s,f,a)
#endif

#if !defined(_MSC_VER) && !defined(__BORLANDC__) && !defined(__WATCOMC__)
	DLLEXPORT char* ultoa(ulong, char*, int radix);
#endif

#if defined(__unix__)
	DLLEXPORT char*	strupr(char* str);
	DLLEXPORT char*	strlwr(char* str);
	DLLEXPORT char* strrev(char* str);
	#if !defined(stricmp)
		#define stricmp			strcasecmp
		#define strnicmp		strncasecmp
	#endif
#endif

#if defined(NEEDS_STRLCPY)
	size_t strlcpy(char *dst, const char *src, size_t size);
#endif

#if defined(_WIN32)
	DLLEXPORT char* strcasestr(const char* haystack, const char* needle);
#endif

/* Skip white-space chars at beginning of string */
DLLEXPORT char*		skipsp(char* str);
/* Truncate white-space chars off end of string */
DLLEXPORT char*		truncsp(char* str);
/* Truncate white-space chars off end of every \n-terminated line in string */
DLLEXPORT char*		truncsp_lines(char* str);
/* Truncate new-line chars off end of string */
DLLEXPORT char*		truncnl(char* str);

#define STRERROR(x)		strerror(x)

/* Re-entrant version of strerror() */
DLLEXPORT char*		safe_strerror(int errnum, char* buf, size_t buflen);

/*********************/
/* Utility Functions */
/*********************/
/* Thunking for multi-threaded specific implementations of "errno" */
DLLEXPORT int get_errno(void);

/**********************************/
/* Common Utility Macro Functions */
/**********************************/

#if defined(_WIN32)

	#define YIELD()			Sleep(1) /* Must sleep at least 1ms to avoid 100% CPU utilization */
	#define	MAYBE_YIELD()	Sleep(0)
	#define SLEEP(x)		Sleep(x)
	#define	popen			_popen
	#define pclose			_pclose
	#define tzname			_tzname

#elif defined(__OS2__)

	#define YIELD()			DosSleep(1)	/* Must sleep at least 1ms to avoid 100% CPU utilization */
	#define	MAYBE_YIELD()	DosSleep(0)
	#define SLEEP(x)		DosSleep(x)

#elif defined(__unix__) || defined(__APPLE__) || defined(__HAIKU__)

	#if defined(_PTH_PTHREAD_H_)
		#define SLEEP(x)		({ int sleep_msecs=x; struct timeval tv; \
								tv.tv_sec=(sleep_msecs/1000); tv.tv_usec=((sleep_msecs%1000)*1000); \
								pth_nap(tv); })
	#else
		#define SLEEP(x)		({	int sleep_msecs=x; struct timespec ts={0}; \
								ts.tv_sec=(sleep_msecs/1000); ts.tv_nsec=((sleep_msecs%1000)*1000000); \
								while(nanosleep(&ts, &ts) != 0 && errno==EINTR && x > 1); })
	#endif

	#define YIELD()			SLEEP(1)

	#if defined(XPDEV_THREAD_SAFE)
		#if defined(__FreeBSD__)
			#define MAYBE_YIELD()			pthread_yield()
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

	DLLEXPORT char*	strtok_r(char *str, const char *delim, char **last);
#endif

/* Mimic the Borland randomize() and random() CRTL functions */
DLLEXPORT void		xp_randomize(void);
DLLEXPORT long		xp_random(int);

DLLEXPORT long double xp_timer(void);
DLLEXPORT char*		os_version(char *str);
DLLEXPORT char*		os_cmdshell(void);
DLLEXPORT char*		lastchar(const char* str);
DLLEXPORT int		safe_snprintf(char *dst, size_t size, const char *fmt, ...)
#if defined(__GNUC__)   // Catch printf-format errors
    __attribute__ ((format (printf, 3 , 4)));            // 1 is 'this'
#endif
;

/* C string/char escape-sequence processing */
DLLEXPORT char*		c_escape_str(const char* src, char* dst, size_t maxlen, BOOL ctrl_only);
DLLEXPORT char*		c_escape_char(char ch);
DLLEXPORT char*		c_unescape_str(char* str);
DLLEXPORT char		c_unescape_char_ptr(const char* str, char** endptr);
DLLEXPORT char		c_unescape_char(char ch);

/* Power-of-2 byte count string parser (e.g. "100K" returns 102400 if unit is 1) */
DLLEXPORT int64_t	parse_byte_count(const char*, ulong unit);
DLLEXPORT double	parse_duration(const char*);
DLLEXPORT char*		duration_to_str(double value, char* str, size_t size);
DLLEXPORT char*		duration_to_vstr(double value, char* str, size_t size);
DLLEXPORT char*		byte_count_to_str(int64_t bytes, char* str, size_t size);
DLLEXPORT char*		byte_estimate_to_str(int64_t bytes, char* str, size_t size, ulong unit, int precision);

/* Microsoft (e.g. DOS/Win32) real-time system clock API (ticks since process started) */
typedef		clock_t				msclock_t;
#define		MSCLOCKS_PER_SEC	1000
msclock_t	msclock(void);

DLLEXPORT BOOL		check_pid(pid_t);
DLLEXPORT BOOL		terminate_pid(pid_t);

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
