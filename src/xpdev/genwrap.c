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

#include <string.h>     /* strlen() */
#include <stdarg.h>		/* vsnprintf() */
#include <stdlib.h>		/* RAND_MAX */
#include <fcntl.h>		/* O_NOCTTY */
#include <time.h>		/* clock() */
#include <errno.h>		/* errno */
#include <ctype.h>		/* toupper/tolower */
#include <limits.h>		/* CHAR_BIT */
#include <math.h>		/* fmod */

#include "strwrap.h"		/* strdup */
#include "ini_file.h"

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
	if ((size_t)numchars >= size && numchars > 0) {
		if (size == 0)
			numchars = 0;
		else
			numchars = size - 1;
	}
	return(numchars);
}

#ifdef NEEDS_STRLCPY
size_t strlcpy(char *dst, const char *src, size_t dsize)
{
	const char *osrc = src;
	size_t nleft = dsize;

	/* Copy as many bytes as will fit. */
	if (nleft != 0) {
		while (--nleft != 0) {
			if ((*dst++ = *src++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src. */
	if (nleft == 0) {
		if (dsize != 0)
			*dst = '\0'; /* NUL-terminate dst */
		while (*src++)
			;
	}

	return(src - osrc - 1); /* count does not include NUL */
}

size_t
strlcat(char *dst, const char *src, size_t dsize)
{
	const char *odst = dst;
	const char *osrc = src;
	size_t n = dsize;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end. */
	while (n-- != 0 && *dst != '\0')
		dst++;
	dlen = dst - odst;
	n = dsize - dlen;

	if (n-- == 0)
		return(dlen + strlen(src));
	while (*src != '\0') {
		if (n != 0) {
			*dst++ = *src;
			n--;
		}
		src++;
	}
	*dst = '\0';

	return(dlen + (src - osrc));    /* count does not include NUL */
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

char* c_escape_str(const char* src, char* dst, size_t maxlen, bool ctrl_only)
{
	const char*	s;
	char*		d;
	const char*	e;

	for(s=src,d=dst;*s && (size_t)(d-dst)<maxlen;s++) {
		if((!ctrl_only || (uchar)*s < ' ') && (e=c_escape_char(*s))!=NULL) {
			strncpy(d,e,maxlen-(d-dst));
			d+=strlen(d);
		} else if((uchar)*s < ' ' || (uchar)*s >= '\x7f') {
			d += safe_snprintf(d, maxlen-(d-dst), "\\x%02X", (uchar)*s);
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
int64_t parse_byte_count(const char* str, uint64_t unit)
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
	if(unit > 1)
		bytes /= unit;
	if(bytes < 0 || bytes > (double)INT64_MAX)
		return INT64_MAX;
	return (int64_t)bytes;
}

static const double one_pebibyte = 1024.0*1024.0*1024.0*1024.0*1024.0;
static const double one_tebibyte = 1024.0*1024.0*1024.0*1024.0;
static const double one_gibibyte = 1024.0*1024.0*1024.0;
static const double one_mebibyte = 1024.0*1024.0;
static const double one_kibibyte = 1024.0;

/* Convert an exact byte count to a string with a floating point value
   and a single letter multiplier/suffix.
   For values evenly divisible by 1024, no suffix otherwise.
*/
char* byte_count_to_str(uint64_t bytes, char* str, size_t size)
{
	if(bytes && fmod((double)bytes,one_pebibyte)==0)
		safe_snprintf(str, size, "%gP",bytes/one_pebibyte);
	else if(bytes && fmod((double)bytes,one_tebibyte)==0)
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
   'unit' is the smallest divisor used.
*/
char* byte_estimate_to_str(uint64_t bytes, char* str, size_t size, uint64_t unit, int precision)
{
	if(bytes >= one_pebibyte)
		safe_snprintf(str, size, "%1.*fP", precision, bytes/one_pebibyte);
	else if(bytes >= one_tebibyte || unit == one_tebibyte)
		safe_snprintf(str, size, "%1.*fT", precision, bytes/one_tebibyte);
	else if(bytes >= one_gibibyte || unit == one_gibibyte)
		safe_snprintf(str, size, "%1.*fG", precision, bytes/one_gibibyte);
	else if(bytes >= one_mebibyte || unit == one_mebibyte)
		safe_snprintf(str, size, "%1.*fM", precision, bytes/one_mebibyte);
	else if(bytes >= one_kibibyte || unit == one_kibibyte)
		safe_snprintf(str, size, "%1.*fK", precision, bytes/one_kibibyte);
	else
		safe_snprintf(str, size, "%"PRIi64, bytes);

	return str;
}

static const double one_year   = 365.0*24.0*60.0*60.0; // Actually, 365.2425
static const double one_week   =   7.0*24.0*60.0*60.0;
static const double one_day    =       24.0*60.0*60.0;
static const double one_hour   =            60.0*60.0;
static const double one_minute =                 60.0;

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
			case 'Y':	t*= one_year; break;
			case 'W':	t*= one_week; break;
			case 'D':	t*= one_day; break;
			case 'H':	t*= one_hour; break;
			case 'M':	t*= one_minute; break;
		}
	}
	return t;
}

/* Convert a duration (in seconds) to a string
 * with a single letter multiplier/suffix: 
 * (y)ears, (w)eeks, (d)ays, (h)ours, (m)inutes, or (s)econds
 */
char* duration_to_str(double value, char* str, size_t size)
{
	if(value && fmod(value, one_year)==0)
		safe_snprintf(str, size, "%gy",value/one_year);
	else if(value && fmod(value, one_week)==0)
		safe_snprintf(str, size, "%gw",value/one_week);
	else if(value && fmod(value, one_day)==0)
		safe_snprintf(str, size, "%gd",value/one_day);
	else if(value && fmod(value, one_hour)==0)
		safe_snprintf(str, size, "%gh",value/one_hour);
	else if(value && fmod(value, one_minute)==0)
		safe_snprintf(str, size, "%gm",value/one_minute);
	else
		safe_snprintf(str, size, "%gs",value);

	return str;
}

/* Convert a duration (in seconds) to a verbose string
 * with a word clarifier / modifier: 
 * year[s], week[s], day[s], hour[s], minute[s] or second[s]
 */
char* duration_to_vstr(double value, char* str, size_t size)
{
	if(value && fmod(value, one_year)==0) {
		value /= one_year;
		safe_snprintf(str, size, "%g year%s", value, value == 1 ? "":"s");
	}
	else if(value && fmod(value, one_week)==0) {
		value /= one_week;
		safe_snprintf(str, size, "%g week%s", value, value == 1 ? "":"s");
	}
	else if(value && fmod(value, one_day)==0) {
		value /= one_day;
		safe_snprintf(str, size, "%g day%s", value, value == 1 ? "":"s");
	}
	else if(value && fmod(value, one_hour)==0) {
		value /= one_hour;
		safe_snprintf(str, size, "%g hour%s", value, value == 1 ? "":"s");
	}
	else if(value && fmod(value, one_minute)==0) {
		value /= one_minute;
		safe_snprintf(str, size, "%g minute%s", value, value == 1 ? "":"s");
	}
	else
		safe_snprintf(str, size, "%g second%s", value, value == 1 ? "":"s");

	return str;
}

/* Convert a duration estimate (in seconds) to a string
 * with a single letter multiplier/suffix:
 * (y)ears, (w)eeks, (d)ays, (h)ours, (m)inutes, or (s)econds
 */
char* duration_estimate_to_str(double value, char* str, size_t size, double unit, int precision)
{
	if(value && fmod(value, one_year) == 0)
		safe_snprintf(str, size, "%gy", value/one_year);
	else if(value >= one_year || unit == one_year)
		safe_snprintf(str, size, "%1.*fy", precision, value/one_year);
	else if(value && fmod(value, one_week) == 0)
		safe_snprintf(str, size, "%gw", value/one_week);
	else if(unit == one_week) // prefer "90 days" over "12.9 weeks"
		safe_snprintf(str, size, "%1.*fw", precision, value/one_week);
	else if(value && fmod(value, one_day) == 0)
		safe_snprintf(str, size, "%gd", value/one_day);
	else if(value >= one_day || unit == one_day)
		safe_snprintf(str, size, "%1.*fd", precision, value/one_day);
	else if(value && fmod(value, one_hour) == 0)
		safe_snprintf(str, size, "%gh", value/one_hour);
	else if(value >= one_hour || unit == one_hour)
		safe_snprintf(str, size, "%1.*fh", precision, value/one_hour);
	else if(value && fmod(value, one_minute) == 0)
		safe_snprintf(str, size, "%gm", value/one_minute);
	else if(value >= one_minute || unit == one_minute)
		safe_snprintf(str, size, "%1.*fm", precision, value/one_minute);
	else
		safe_snprintf(str, size, "%gs",value);

	return str;
}

/* Convert a duration estimate (in seconds) to a verbose string
 * with a word clarifier / modifier:
 * year[s], week[s], day[s], hour[s], minute[s] or second[s]
 */
char* duration_estimate_to_vstr(double value, char* str, size_t size, double unit, int precision)
{
	if(value && fmod(value, one_year) == 0) {
		value /= one_year;
		safe_snprintf(str, size, "%g year%s", value, value == 1 ? "":"s");
	}
	else if(value >= one_year || unit == one_year) {
		value /= one_year;
		safe_snprintf(str, size, "%1.*f year%s", precision, value, value == 1 ? "":"s");
	}
	else if(value && fmod(value, one_week) == 0) {
		value /= one_week;
		safe_snprintf(str, size, "%g week%s", value, value == 1 ? "":"s");
	}
	else if(unit == one_week) { // prefer "90 days" over "12.9 weeks"
		value /= one_week;
		safe_snprintf(str, size, "%1.*f week%s", precision, value, value == 1 ? "":"s");
	}
	else if(value && fmod(value, one_day) == 0) {
		value /= one_day;
		safe_snprintf(str, size, "%g day%s", value, value == 1 ? "":"s");
	}
	else if(value >= one_day || unit == one_day) {
		value /= one_day;
		safe_snprintf(str, size, "%1.*f day%s", precision, value, value == 1 ? "":"s");
	}
	else if(value && fmod(value, one_hour) == 0) {
		value /= one_hour;
		safe_snprintf(str, size, "%g hour%s", value, value == 1 ? "":"s");
	}
	else if(value >= one_hour || unit == one_hour) {
		value /= one_hour;
		safe_snprintf(str, size, "%1.*f hour%s", precision, value, value == 1 ? "":"s");
	}
	else if(value && fmod(value, one_minute) == 0) {
		value /= one_minute;
		safe_snprintf(str, size, "%g minute%s", value, value == 1 ? "":"s");
	}
	else if(value >= one_minute || unit == one_minute) {
		value /= one_minute;
		safe_snprintf(str, size, "%1.*f minute%s", precision, value, value == 1 ? "":"s");
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
		if(read(rf, &seed, sizeof(seed)) != sizeof seed)
			seed = UINT_MAX;
		close(rf);
	}
	else {
#endif
		unsigned curtime	= (unsigned)time(NULL);
		unsigned process_id = (unsigned)GetCurrentProcessId();

		seed = curtime ^ BYTE_SWAP_INT(process_id);

		#if defined(_WIN32) || defined(GetCurrentThreadId)
			seed ^= (unsigned)(uintptr_t)GetCurrentThreadId();
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

#if (defined(__GNUC__) && (__GNUC__ < 5)) || !defined(__MINGW32__)
char* _i64toa(int64_t val, char* str, int radix)
{
	switch(radix) {
		case 8:
			sprintf(str,"%"PRIo64,val);
			break;
		case 10:
			sprintf(str,"%"PRId64,val);
			break;
		case 16:
			sprintf(str,"%"PRIx64,val);
			break;
		default:
			sprintf(str,"bad radix: %d",radix);
			break;
	}
	return str;
}

char* _ui64toa(uint64_t val, char* str, int radix)
{
	switch(radix) {
		case 8:
			sprintf(str,"%"PRIo64,val);
			break;
		case 10:
			sprintf(str,"%"PRIu64,val);
			break;
		case 16:
			sprintf(str,"%"PRIx64,val);
			break;
		default:
			sprintf(str,"bad radix: %d",radix);
			break;
	}
	return str;
}
#endif
#endif

/****************************************************************************/
/* Write the version details of the current operating system into str		*/
/****************************************************************************/
char* os_version(char *str, size_t size)
{
#if defined(__OS2__) && defined(__BORLANDC__)

	safe_snprintf(str, size, "OS/2 %u.%u (%u.%u)",_osmajor/10,_osminor/10,_osmajor,_osminor);

#elif defined(_WIN32)

	/* Windows Version */
	char*			winflavor="";
	OSVERSIONINFO	winver;

	winver.dwOSVersionInfoSize=sizeof(winver);

	#pragma warning(suppress : 4996) // error C4996: 'GetVersionExA': was declared deprecated
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

	if(winver.dwMajorVersion == 6 && winver.dwMinorVersion == 1) {
		winver.dwMajorVersion = 7;
		winver.dwMinorVersion = 0;
	}
	else {
		static NTSTATUS (WINAPI *pRtlGetVersion)(PRTL_OSVERSIONINFOW lpVersionInformation) = NULL;
		if(pRtlGetVersion == NULL) {
			HINSTANCE ntdll = LoadLibrary("ntdll.dll");
			if(ntdll != NULL)
				pRtlGetVersion = (NTSTATUS (WINAPI *)(PRTL_OSVERSIONINFOW))GetProcAddress(ntdll, "RtlGetVersion");
		}
		if(pRtlGetVersion != NULL) {
			pRtlGetVersion((PRTL_OSVERSIONINFOW)&winver);
			if(winver.dwMajorVersion == 10 && winver.dwMinorVersion == 0 &&  winver.dwBuildNumber >= 22000)
				winver.dwMajorVersion = 11;
		}
	}

	safe_snprintf(str, size, "Windows %sVersion %lu.%lu"
			,winflavor
			,winver.dwMajorVersion, winver.dwMinorVersion);
	if(winver.dwBuildNumber)
		sprintf(str+strlen(str), " (Build %lu)", winver.dwBuildNumber);
	if(winver.szCSDVersion[0])
		sprintf(str+strlen(str), " %s", winver.szCSDVersion);

#elif defined(__unix__)
	FILE* fp = fopen("/etc/os-release", "r");
	if(fp == NULL)
		fp = fopen("/usr/lib/os-release", "r");
	if(fp != NULL) {
		char value[INI_MAX_VALUE_LEN];
		char* p = iniReadString(fp, NULL, "PRETTY_NAME", "Unix", value);
		fclose(fp);
		SKIP_CHAR(p, '"');
		strncpy(str, p, size);
		p = lastchar(str);
		if(*p == '"')
			*p = '\0';
	} else {
		struct utsname unixver;

		if(uname(&unixver) != 0)
			safe_snprintf(str, size, "Unix (uname errno: %d)",errno);
		else
			safe_snprintf(str, size, "%s %s"
				,unixver.sysname	/* e.g. "Linux" */
				,unixver.release	/* e.g. "2.2.14-5.0" */
				);
	}
#else	/* DOS */

	safe_snprintf(str, size, "DOS %u.%02u",_osmajor,_osminor);

#endif

	return(str);
}

/****************************************************************************/
/* Write the CPU architecture according to the Operating System into str	*/
/****************************************************************************/
char* os_cpuarch(char *str, size_t size)
{
#if defined(_WIN32)
	SYSTEM_INFO sysinfo;

#if _WIN32_WINNT < 0x0501
	GetSystemInfo(&sysinfo);
#else
	GetNativeSystemInfo(&sysinfo);
#endif
	switch(sysinfo.wProcessorArchitecture) {
		case PROCESSOR_ARCHITECTURE_AMD64:
			safe_snprintf(str, size, "x64");
			break;
		case PROCESSOR_ARCHITECTURE_ARM:
			safe_snprintf(str, size, "ARM");
			break;
#if defined PROCESSOR_ARCHITECTURE_ARM64
		case PROCESSOR_ARCHITECTURE_ARM64:
			safe_snprintf(str, size, "ARM64");
			break;
#endif
		case PROCESSOR_ARCHITECTURE_IA64:
			safe_snprintf(str, size, "IA-64");
			break;
		case PROCESSOR_ARCHITECTURE_INTEL:
			safe_snprintf(str, size, "x86");
			break;
		default:
			safe_snprintf(str, size, "unknown");
			break;
	}

#elif defined(__unix__)

	struct utsname unixver;

	if(uname(&unixver) == 0)
		safe_snprintf(str, size, "%s", unixver.machine);
	else
		safe_snprintf(str, size, "unknown");

#endif

	return str;
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

/********************************************************/
/* Stupid real-time system clock implementation.	*/
/********************************************************/
clock_t msclock(void)
{
	uint64_t t = (uint64_t)(xp_timer() * 1000);

	return (clock_t)(t&0xffffffff);
}

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

/****************************************************************************/
/* Returns the current value of the systems best timer (in MILLISECONDS)	*/
/* Any value < 0 indicates an error											*/
/****************************************************************************/
uint64_t xp_timer64(void)
{
	uint64_t ret;
#if defined(__unix__)
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
		ret = ts.tv_sec * 1000;
		ret += ts.tv_nsec / 1000000;
	}
	else
		ret = -1;
#elif defined(_WIN32)
	static bool can_use_QPF = true;
	static bool intable = false;
	static bool initialized = false;
	static uint32_t msfreq;
	static double msdfreq;
	LARGE_INTEGER	tick;

	if (!initialized) {
		LARGE_INTEGER	freq;
		if (!QueryPerformanceFrequency(&freq))
			can_use_QPF = false;
		else
        		intable = (freq.QuadPart % 1000) == 0;

		if (intable)
			msfreq = (uint32_t)(freq.QuadPart / 1000);
		else
			msdfreq = ((double)freq.QuadPart) / 1000;

		initialized = true;
	}
	if (can_use_QPF) {
		if (!QueryPerformanceCounter(&tick)) {
			can_use_QPF = false;
			return GetTickCount();
		}
		if (intable)
			ret = tick.QuadPart / msfreq;
		else
			ret = (uint32_t)(((double)tick.QuadPart) / msdfreq);
	}
	else {
		ret=GetTickCount();
	}
#else
#error no high-resolution time for this platform
#endif
	return(ret);
}

/* Returns true if specified process is running */
bool check_pid(pid_t pid)
{
#ifdef __EMSCRIPTEN__
	fprintf(stderr, "%s not implemented", __func__);
	return false;
#else
#if defined(__unix__)
	return(kill(pid,0)==0);
#elif defined(_WIN32)
	HANDLE	h;
	bool	result=false;

	if((h=OpenProcess(PROCESS_QUERY_INFORMATION,/* inheritable: */false, pid)) != NULL) {
		DWORD	code;
		if(GetExitCodeProcess(h,(PDWORD)&code)==true && code==STILL_ACTIVE)
			result=true;
		CloseHandle(h);
	}
	return result;
#else
	return false;	/* Need check_pid() definition! */
#endif
#endif
}

/* Terminate (unconditionally) the specified process */
bool terminate_pid(pid_t pid)
{
#ifdef __EMSCRIPTEN__
	fprintf(stderr, "%s not implemented", __func__);
	return false;
#else
#if defined(__unix__)
	return(kill(pid,SIGKILL)==0);
#elif defined(_WIN32)
	HANDLE	h;
	bool	result=false;

	if((h=OpenProcess(PROCESS_TERMINATE,/* inheritable: */false, pid)) != NULL) {
		if(TerminateProcess(h,255))
			result=true;
		CloseHandle(h);
	}
	return result;
#else
	return false;	/* Need check_pid() definition! */
#endif
#endif
}

/****************************************************************************/
/* Re-entrant (thread-safe) version of strerror()							*/
/* GNU (not POSIX) inspired API												*/
/****************************************************************************/
char* safe_strerror(int errnum, char *buf, size_t buflen)
{
	snprintf(buf, buflen, "Unknown error: %d", errnum);

#if defined(_MSC_VER)
	strerror_s(buf, buflen, errnum);
#elif defined(_WIN32) || defined(__EMSCRIPTEN__)
	strlcpy(buf, strerror(errnum), buflen);
#elif defined(_GNU_SOURCE) && defined(__USE_GNU)
	char* ret = strerror_r(errnum, buf, buflen);
	if (ret != buf)
		strlcpy(buf, ret, buflen);
#else
	strerror_r(errnum, buf, buflen);
#endif
	return buf;
}

/****************************************************************************/
/* Common realloc mistake: 'p' nulled but not freed upon failure			*/
/* [memleakOnRealloc]														*/
/****************************************************************************/
void* realloc_or_free(void* p, size_t size)
{
	void* n = realloc(p, size);
	if(n == NULL)
		free(p);
	return n;
}
