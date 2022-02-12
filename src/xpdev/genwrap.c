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
#include <stdarg.h>		/* vsnprintf() */
#include <stdlib.h>		/* RAND_MAX */
#include <fcntl.h>		/* O_NOCTTY */
#include <time.h>		/* clock() */
#include <errno.h>		/* errno */
#include <ctype.h>		/* toupper/tolower */
#include <limits.h>		/* CHAR_BIT */
#include <math.h>		/* fmod */

#if defined(__unix__)
	#include <sys/ioctl.h>		/* ioctl() */
	#include <sys/utsname.h>	/* uname() */
	#include <signal.h>
#elif defined(_WIN32)
	#include <windows.h>
	#include <lm.h>		/* NetWkstaGetInfo() */
#endif

#include "genwrap.h"	/* Verify prototypes */
#include "xpendian.h"	/* BYTE_SWAP */

/****************************************************************************/
/* Used to replace snprintf()  guarantees to terminate.			  			*/
/****************************************************************************/
int safe_snprintf(char *dst, size_t size, const char *fmt, ...)
{
	va_list argptr;
	int     numchars;

	va_start(argptr,fmt);
	numchars= vsnprintf(dst,size,fmt,argptr);
	va_end(argptr);
	if (size > 0)
		dst[size-1]=0;
#ifdef _MSC_VER
	if(numchars==-1)
		numchars=strlen(dst);
#endif
	if(numchars>=(int)size && numchars>0)
		numchars=size-1;
	return(numchars);
}

#ifdef NEEDS_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t size)
{
	size_t i;

	if(size < 1)
		return 0;

	for(i = 0; src[i] != '\0'; i++) {
		if(i < (size - 1))
			*(dst++) = src[i];
	}
	*dst = 0;
	return i;
}
#endif

#ifdef _WIN32
/****************************************************************************/
/* Case insensitive version of strstr()	- currently heavy-handed			*/
/****************************************************************************/
char* strcasestr(const char* haystack, const char* needle)
{
	const char* p;
	size_t len = strlen(needle);

	for(p = haystack; *p != '\0'; p++) {
		if(strnicmp(p, needle, len) == 0)
			return (char*)p;
	}
	return NULL;
}
#endif

/****************************************************************************/
/* Return last character of string											*/
/****************************************************************************/
char* lastchar(const char* str)
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
char c_unescape_char(char ch)
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
/* Return character value of C-escaped (\) character literal sequence		*/
/* supports \x## (hex) and \### sequences (for octal or decimal)			*/
/****************************************************************************/
char c_unescape_char_ptr(const char* str, char** endptr)
{
	char	ch;

	if(*str == 'x') {
		int digits = 0;		// \x## for hexadecimal character literals (only 2 digits supported)
		++str;
		ch = 0;
		while(digits < 2 && IS_HEXDIGIT(*str)) {
			ch *= 0x10;	
			ch += HEX_CHAR_TO_INT(*str);
			str++;
			digits++;
		}
#ifdef C_UNESCAPE_OCTAL_SUPPORT
	} else if(IS_OCTDIGIT(*str)) {
		int digits = 0;		// \### for octal character literals (only 3 digits supported)
		ch = 0;
		while(digits < 3 && IS_OCTDIGIT(*str)) {
			ch *= 8;
			ch += OCT_CHAR_TO_INT(*str);
			str++;
			digits++;
		}
#else
	} else if(IS_DIGIT(*str)) {
		int digits = 0;		// \### for decimal character literals (only 3 digits supported)
		ch = 0;
		while(digits < 3 && IS_DIGIT(*str)) {
			ch *= 10;
			ch += DEC_CHAR_TO_INT(*str);
			str++;
			digits++;
		}
#endif
	 } else
		ch=c_unescape_char(*(str++));

	if(endptr!=NULL)
		*endptr=(char*)str;

	return ch;
}

/****************************************************************************/
/* Unescape a C string, in place											*/
/****************************************************************************/
char* c_unescape_str(char* str)
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

char* c_escape_char(char ch)
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

char* c_escape_str(const char* src, char* dst, size_t maxlen, BOOL ctrl_only)
{
	const char*	s;
	char*		d;
	const char*	e;

	for(s=src,d=dst;*s && (size_t)(d-dst)<maxlen;s++) {
		if((!ctrl_only || (uchar)*s < ' ') && (e=c_escape_char(*s))!=NULL) {
			strncpy(d,e,maxlen-(d-dst));
			d+=strlen(d);
		} else if((uchar)*s < ' ') {
			d += safe_snprintf(d, maxlen-(d-dst), "\\x%02X", *s);
		} else *d++=*s;
	}
	*d=0;

	return(dst);
}

/* Returns a byte count parsed from the 'str' argument, supporting power-of-2
 * short-hands (e.g. 'K' for kibibytes).
 * If 'unit' is specified (greater than 1), result is divided by this amount.
 * 
 * Moved from ini_file.c/parseBytes()
 */
int64_t parse_byte_count(const char* str, ulong unit)
{
	char*	p=NULL;
	double	bytes;

	bytes=strtod(str,&p);
	if(p!=NULL) {
		SKIP_WHITESPACE(p);
		switch(toupper(*p)) {
			case 'E':
				bytes*=1024;
				/* fall-through */
			case 'P':
				bytes*=1024;
				/* fall-through */
			case 'T':
				bytes*=1024;
				/* fall-through */
			case 'G':
				bytes*=1024;
				/* fall-through */
			case 'M':
				bytes*=1024;
				/* fall-through */
			case 'K':
				bytes*=1024;
				break;
		}
	}
	return((int64_t)(unit>1 ? (bytes/unit):bytes));
}

static const double one_tebibyte = 1024.0*1024.0*1024.0*1024.0;
static const double one_gibibyte = 1024.0*1024.0*1024.0;
static const double one_mebibyte = 1024.0*1024.0;
static const double one_kibibyte = 1024.0;

/* Convert an exact byte count to a string with a floating point value
   and a single letter multiplier/suffix.
   For values evenly divisible by 1024, no suffix otherwise.
*/
char* byte_count_to_str(int64_t bytes, char* str, size_t size)
{
	if(bytes && fmod((double)bytes,one_tebibyte)==0)
		safe_snprintf(str, size, "%gT",bytes/one_tebibyte);
	else if(bytes && fmod((double)bytes,one_gibibyte)==0)
		safe_snprintf(str, size, "%gG",bytes/one_gibibyte);
	else if(bytes && fmod((double)bytes,one_mebibyte)==0)
		safe_snprintf(str, size, "%gM",bytes/one_mebibyte);
	else if(bytes && fmod((double)bytes,one_kibibyte)==0)
		safe_snprintf(str, size, "%gK",bytes/one_kibibyte);
	else
		safe_snprintf(str, size, "%"PRIi64, bytes);

	return str;
}

/* Convert a rounded byte count to a string with a floating point value
   with a single decimal place and a single letter multiplier/suffix.
   This function also appends 'B' for exact byte counts (< 1024).
   'unit' is the smallest divisor used.
*/
char* byte_estimate_to_str(int64_t bytes, char* str, size_t size, ulong unit, int precision)
{
	if(bytes >= one_tebibyte)
		safe_snprintf(str, size, "%1.*fT", precision, bytes/one_tebibyte);
	else if(bytes >= one_gibibyte || unit == one_gibibyte)
		safe_snprintf(str, size, "%1.*fG", precision, bytes/one_gibibyte);
	else if(bytes >= one_mebibyte || unit == one_mebibyte)
		safe_snprintf(str, size, "%1.*fM", precision, bytes/one_mebibyte);
	else if(bytes >= one_kibibyte || unit == one_kibibyte)
		safe_snprintf(str, size, "%1.*fK", precision, bytes/one_kibibyte);
	else
		safe_snprintf(str, size, "%"PRIi64"B", bytes);

	return str;
}


/* Parse a duration string, default unit is in seconds */
/* (Y)ears, (W)eeks, (D)ays, (H)ours, and (M)inutes */
/* suffixes/multipliers are supported. */
/* Return value is in seconds */
double parse_duration(const char* str)
{
	char*	p=NULL;
	double	t;

	t=strtod(str,&p);
	if(p!=NULL) {
		SKIP_WHITESPACE(p);
		switch(toupper(*p)) {
			case 'Y':	t*=365.0*24.0*60.0*60.0; break;
			case 'W':	t*=  7.0*24.0*60.0*60.0; break;
			case 'D':	t*=		 24.0*60.0*60.0; break;
			case 'H':	t*=			  60.0*60.0; break;
			case 'M':	t*=				   60.0; break;
		}
	}
	return t;
}

/* Convert a duration (in seconds) to a string
 * with a single letter multiplier/suffix: 
 * (Y)ears, (W)eeks, (D)ays, (H)ours, (M)inutes, or (S)econds
 */
char* duration_to_str(double value, char* str, size_t size)
{
	if(value && fmod(value,365.0*24.0*60.0*60.0)==0)
		safe_snprintf(str, size, "%gY",value/(365.0*24.0*60.0*60.0));
	else if(value && fmod(value,7.0*24.0*60.0*60.0)==0)
		safe_snprintf(str, size, "%gW",value/(7.0*24.0*60.0*60.0));
	else if(value && fmod(value,24.0*60.0*60.0)==0)
		safe_snprintf(str, size, "%gD",value/(24.0*60.0*60.0));
	else if(value && fmod(value,60.0*60.0)==0)
		safe_snprintf(str, size, "%gH",value/(60.0*60.0));
	else if(value && fmod(value,60.0)==0)
		safe_snprintf(str, size, "%gM",value/60.0);
	else
		safe_snprintf(str, size, "%gS",value);

	return str;
}

/* Convert a duration (in seconds) to a verbose string
 * with a word clarifier / modifier: 
 * year[s], week[s], day[s], hour[s], minute[s] or second[s]
 */
char* duration_to_vstr(double value, char* str, size_t size)
{
	if(value && fmod(value,365.0*24.0*60.0*60.0)==0) {
		value /= (365.0*24.0*60.0*60.0);
		safe_snprintf(str, size, "%g year%s", value, value == 1 ? "":"s");
	}
	else if(value && fmod(value,7.0*24.0*60.0*60.0)==0) {
		value /= (7.0*24.0*60.0*60.0);
		safe_snprintf(str, size, "%g week%s", value, value == 1 ? "":"s");
	}
	else if(value && fmod(value,24.0*60.0*60.0)==0) {
		value /= (24.0*60.0*60.0);
		safe_snprintf(str, size, "%g day%s", value, value == 1 ? "":"s");
	}
	else if(value && fmod(value,60.0*60.0)==0) {
		value /= (60.0*60.0);
		safe_snprintf(str, size, "%g hour%s", value, value == 1 ? "":"s");
	}
	else if(value && fmod(value,60.0)==0) {
		value /= 60.0;
		safe_snprintf(str, size, "%g minute%s", value, value == 1 ? "":"s");
	}
	else
		safe_snprintf(str, size, "%g second%s", value, value == 1 ? "":"s");

	return str;
}


/****************************************************************************/
/* Convert ASCIIZ string to upper case										*/
/****************************************************************************/
#if defined(__unix__)
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
char* strtok_r(char *str, const char *delim, char **last)
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
void xp_randomize(void)
{
#if !(defined(HAS_SRANDOMDEV_FUNC) && defined(HAS_RANDOM_FUNC))
	unsigned seed=~0;
#if defined(HAS_DEV_URANDOM) && defined(URANDOM_DEV)
	int		rf;
#endif
#endif

#if defined(HAS_SRANDOMDEV_FUNC) && defined(HAS_RANDOM_FUNC)
	srandomdev();
	return;
#else

#if defined(HAS_DEV_URANDOM) && defined(URANDOM_DEV)
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
#else
 	srand(seed);
#endif
#endif
}

/****************************************************************************/
/* Return random number between 0 and n-1									*/
/****************************************************************************/
long xp_random(int n)
{
#ifdef HAS_RANDOM_FUNC
	long			curr;
	unsigned long	limit;

	if(n<2)
		return(0);

	limit = ((1UL<<((sizeof(long)*CHAR_BIT)-1)) / n) * n - 1;

	while(1) {
		curr=random();
		if(curr <= limit)
			return(curr % n);
	}
#else
	double f=0;
	int ret;

	if(n<2)
		return(0);
	do {
		f=(double)rand()/(double)(RAND_MAX+1);
		ret=(int)(n*f);
	} while(ret==n);

	return(ret);
#endif
}

/****************************************************************************/
/* Return ASCII string representation of ulong								*/
/* There may be a native GNU C Library function to this...					*/
/****************************************************************************/
#if !defined(_MSC_VER) && !defined(__BORLANDC__) && !defined(__WATCOMC__)
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
/* Write the version details of the current operating system into str		*/
/****************************************************************************/
char* os_version(char *str)
{
#if defined(__OS2__) && defined(__BORLANDC__)

	sprintf(str,"OS/2 %u.%u (%u.%u)",_osmajor/10,_osminor/10,_osmajor,_osminor);

#elif defined(_WIN32)

	/* Windows Version */
	char*			winflavor="";
	OSVERSIONINFO	winver;

	winver.dwOSVersionInfoSize=sizeof(winver);
	#pragma warning(suppress : 4996)
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

	/* Work-around Microsoft Windows 8.1 stupidity where GetVersionEx() lies about the current OS version */
	if(winver.dwMajorVersion == 6 && winver.dwMinorVersion == 2) {
		WKSTA_INFO_100* wksta_info;
		if(NetWkstaGetInfo(NULL, 100, (LPBYTE*)&wksta_info) == NERR_Success) {
			winver.dwMajorVersion = wksta_info->wki100_ver_major;
			winver.dwMinorVersion = wksta_info->wki100_ver_minor;
			winver.dwBuildNumber = 0;
		}
	}

	sprintf(str,"Windows %sVersion %lu.%lu"
			,winflavor
			,winver.dwMajorVersion, winver.dwMinorVersion);
	if(winver.dwBuildNumber)
		sprintf(str+strlen(str), " (Build %lu)", winver.dwBuildNumber);
	if(winver.szCSDVersion[0])
		sprintf(str+strlen(str), " %s", winver.szCSDVersion);

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

char* os_cmdshell(void)
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

/****************************************************************/
/* Microsoft (DOS/Win32) real-time system clock implementation.	*/
/****************************************************************/
#ifdef __unix__
clock_t msclock(void)
{
        long double t = roundl(xp_timer() * 1000);

        if (sizeof(clock_t) < 8) {
                while (t > INT32_MAX)
                        t -= UINT32_MAX;
        }

	return (clock_t)t;
}
#endif

/****************************************************************************/
/* Skips all white-space chars at beginning of 'str'						*/
/****************************************************************************/
char* skipsp(char* str)
{
	SKIP_WHITESPACE(str);
	return(str);
}

/****************************************************************************/
/* Truncates all white-space chars off end of 'str'	(needed by STRERRROR)	*/
/****************************************************************************/
char* truncsp(char* str)
{
	size_t i,len;

	if(str!=NULL) {
		i=len=strlen(str);
		while(i && IS_WHITESPACE(str[i-1]))
			i--;
		if(i!=len)
			str[i]=0;	/* truncate */
	}
	return(str);
}

/****************************************************************************/
/* Truncates common white-space chars off end of \n-terminated lines in		*/
/* 'dst' and retains original line break format	(e.g. \r\n or \n)			*/
/****************************************************************************/
char* truncsp_lines(char* dst)
{
	char* sp;
	char* dp;
	char* src;

	if((src=strdup(dst))==NULL)
		return(dst);

	for(sp=src, dp=dst; *sp!=0; sp++) {
		if(*sp=='\n') {
			while(dp!=dst
				&& (*(dp-1)==' ' || *(dp-1)=='\t' || *(dp-1)=='\r'))
					dp--;
			if(sp!=src && *(sp-1)=='\r')
				*(dp++)='\r';
		}
		*(dp++)=*sp;
	}
	*dp=0;

	free(src);
	return(dst);
}

/****************************************************************************/
/* Truncates carriage-return and line-feed chars off end of 'str'			*/
/****************************************************************************/
char* truncnl(char* str)
{
	size_t i,len;

	if(str!=NULL) {
		i=len=strlen(str);
		while(i && (str[i-1]=='\r' || str[i-1]=='\n'))
			i--;
		if(i!=len)
			str[i]=0;	/* truncate */
	}
	return(str);
}

/****************************************************************************/
/* Return errno from the proper C Library implementation					*/
/* (single/multi-threaded)													*/
/****************************************************************************/
int get_errno(void)
{
	return(errno);
}

/****************************************************************************/
/* Returns the current value of the systems best timer (in SECONDS)			*/
/* Any value < 0 indicates an error											*/
/****************************************************************************/
long double	xp_timer(void)
{
	long double ret;
#if defined(__unix__)
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		ret = ts.tv_sec;
		ret += ((long double)ts.tv_nsec) / 1000000000;
	}
	else
		ret = -1;
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
#error no high-resolution time for this platform
#endif
	return(ret);
}

/* Returns TRUE if specified process is running */
BOOL check_pid(pid_t pid)
{
#if defined(__unix__)
	return(kill(pid,0)==0);
#elif defined(_WIN32)
	HANDLE	h;
	BOOL	result=FALSE;

	if((h=OpenProcess(PROCESS_QUERY_INFORMATION,/* inheritable: */FALSE, pid)) != NULL) {
		DWORD	code;
		if(GetExitCodeProcess(h,(PDWORD)&code)==TRUE && code==STILL_ACTIVE)
			result=TRUE;
		CloseHandle(h);
	}
	return result;
#else
	return FALSE;	/* Need check_pid() definition! */
#endif
}

/* Terminate (unconditionally) the specified process */
BOOL terminate_pid(pid_t pid)
{
#if defined(__unix__)
	return(kill(pid,SIGKILL)==0);
#elif defined(_WIN32)
	HANDLE	h;
	BOOL	result=FALSE;

	if((h=OpenProcess(PROCESS_TERMINATE,/* inheritable: */FALSE, pid)) != NULL) {
		if(TerminateProcess(h,255))
			result=TRUE;
		CloseHandle(h);
	}
	return result;
#else
	return FALSE;	/* Need check_pid() definition! */
#endif
}

/****************************************************************************/
/* Re-entrant (thread-safe) version of strerror()							*/
/* GNU (not POSIX) inspired API											*/
/****************************************************************************/
char* safe_strerror(int errnum, char *buf, size_t buflen)
{
	strncpy(buf, "Unknown error", buflen);
	buf[buflen - 1] = 0;

#if defined(_MSC_VER)
	strerror_s(buf, buflen, errnum);
#elif defined(_WIN32)
	strncpy(buf, strerror(errnum), buflen);
	buf[buflen - 1] = 0;
#elif defined(_GNU_SOURCE)
	buf = strerror_r(errnum, buf, buflen);
#else
	strerror_r(errnum, buf, buflen);
#endif
	return buf;
}
