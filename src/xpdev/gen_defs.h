/* General(ly useful) constant, macro, and type definitions */

/* $Id: gen_defs.h,v 1.85 2020/08/15 19:57:51 rswindell Exp $ */
// vi: tabstop=4
																			
/****************************************************************************
 * @format.tab-size 4           (Plain Text/Source Code File Header)        *
 * @format.use-tabs true        (see http://www.synchro.net/ptsc_hdr.html)  *
 *                                                                          *
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *                                                                          *
 * This library is free software; you can redistribute it and/or            *
 * modify it under the terms of the GNU Lesser General Public License       *
 * as published by the Free Software Foundation; either version 2           *
 * of the License, or (at your option) any later version.                   *
 * See the GNU Lesser General Public License for more details: lgpl.txt or  *
 * http://www.fsf.org/copyleft/lesser.html                                  *
 *                                                                          *
 * Anonymous FTP access to the most recent released source is available at  *
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net  *
 *                                                                          *
 * Anonymous CVS access to the development source and modification history  *
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:                  *
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login            *
 *     (just hit return, no password is necessary)                          *
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src     *
 *                                                                          *
 * For Synchronet coding style and modification guidelines, see             *
 * http://www.synchro.net/source.html                                       *
 *                                                                          *
 * You are encouraged to submit any modifications (preferably in Unix diff  *
 * format) via e-mail to mods@synchro.net                                   *
 *                                                                          *
 * Note: If this box doesn't appear square, then you need to fix your tabs. *
 ****************************************************************************/

#ifndef _GEN_DEFS_H
#define _GEN_DEFS_H

#include "cp437defs.h"
#include <errno.h>

/* Resolve multi-named errno constants */
#if defined(EDEADLK) && !defined(EDEADLOCK)
        #define EDEADLOCK EDEADLK
#endif

#if defined(_WIN32)
        #define WIN32_LEAN_AND_MEAN     /* Don't bring in excess baggage */
        #include <windows.h>
#elif defined(__OS2__)
        #define INCL_BASE       /* need this for DosSleep prototype */
        #include <os2.h>
#else
        #if (defined(__APPLE__) && defined(__MACH__) && defined(__POWERPC__)) || defined (__NetBSD__)
                #ifndef __unix__
                        #define __unix__
                #endif
        #endif
#endif

#include <ctype.h>
#include <sys/types.h>
#ifdef HAS_INTTYPES_H
#if defined __cplusplus
#define __STDC_FORMAT_MACROS
#endif
#include <inttypes.h>
#else
#ifdef HAS_STDINT_H
#include <stdint.h>
#endif
#endif

                                               /* Control characters */
#ifndef STX
#define STX     0x02                           /* Start of text                 ^B      */
#endif
#ifndef ETX
#define ETX     0x03                           /* End of text                   ^C      */
#endif
#ifndef BEL
#define BEL     0x07                            /* Bell/beep                    ^G      */
#endif
#ifndef FF
#define FF      0x0c                            /* Form feed                    ^L      */
#endif
#ifndef ESC
#define ESC     0x1b                            /* Escape                       ^[      */
#endif
#ifndef DEL
#define DEL     0x7f                            /* Delete                       ^BS     */
#endif
#ifndef BS
#define BS      '\b'                            /* Back space                   ^H      */
#endif
#ifndef TAB
#define TAB     '\t'                            /* Horizontal tabulation        ^I      */
#endif
#ifndef LF
#define LF      '\n'                            /* Line feed                    ^J      */
#endif
#ifndef CR
#define CR      '\r'                            /* Carriage return              ^M      */
#endif

#ifndef CTRL_A
enum {
	 CTRL_AT						// NUL
	,CTRL_A							// SOH
	,CTRL_B							// STX
	,CTRL_C							// ETX
	,CTRL_D							// EOT
	,CTRL_E							// ENQ
	,CTRL_F							// ACK
	,CTRL_G							// BEL
	,CTRL_H							// BS
	,CTRL_I							// HT
	,CTRL_J							// LF
	,CTRL_K							// VT
	,CTRL_L							// FF
	,CTRL_M							// CR
	,CTRL_N							// SO
	,CTRL_O							// SI
	,CTRL_P							// DLE
	,CTRL_Q							// DC1
	,CTRL_R							// DC2
	,CTRL_S							// DC3
	,CTRL_T							// DC4
	,CTRL_U							// NAK
	,CTRL_V							// SYN
	,CTRL_W							// ETB
	,CTRL_X							// CAN
	,CTRL_Y							// EM
	,CTRL_Z							// SUB
	,CTRL_OPEN_BRACKET				// ESC
	,CTRL_BACKSLASH					// FS
	,CTRL_CLOSE_BRACKET				// GS
	,CTRL_CARET						// RS
	,CTRL_UNDERSCORE				// US
	,CTRL_QUESTION_MARK	= 0x7f		// DEL
};
#endif

/* Unsigned type short-hands    */
#ifndef uchar
    #define uchar   unsigned char
#endif
#ifndef ushort
	#define ushort  unsigned short
	typedef unsigned int uint;			/* Incompatible with Spidermonkey header files when #define'd */
	#define ulong   unsigned long
#endif

/* Printf format specifiers... */
#if defined(_MSC_VER) || defined(__WATCOMC__) || defined(__BORLANDC__)
#define XP_PRIsize_t					"I"
#else
#define XP_PRIsize_t					"z"
#endif

#if !defined(HAS_INTTYPES_H) && !defined(XPDEV_DONT_DEFINE_INTTYPES)

#if !defined(HAS_STDINT_H)

typedef char    int8_t;
#ifndef INT8_MAX
#define INT8_MAX 0x7f
#endif
#ifndef INT16_MAX
#define INT16_MAX (-0x7f-1)
#endif
typedef short   int16_t;
#ifndef INT16_MAX
#define INT16_MAX 0x7fff
#endif
#ifndef INT16_MIN
#define INT16_MIN (-0x7fff-1)
#endif
typedef long    int32_t;
#ifndef INT32_MAX
#define INT32_MAX 0x7fffffff
#endif
#ifndef INT32_MIN
#define INT32_MIN (-0x7fffffff-1)
#endif
typedef uchar   uint8_t;
#ifndef UINT8_MAX
#define UINT8_MAX 0xff
#endif
typedef ushort  uint16_t;
#ifndef UINT16_MAX
#define UINT16_MAX 0xffff
#endif
typedef ulong   uint32_t;
#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffff
#endif

#endif

#if !defined(_MSDOS)
#if defined(_MSC_VER) || defined(__WATCOMC__) || defined(__BORLANDC__)
typedef SSIZE_T ssize_t;
typedef signed __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define INTTYPES_H_64BIT_PREFIX         "I64"
#else
typedef signed long long int int64_t;
typedef unsigned long long int uint64_t;
#define INTTYPES_H_64BIT_PREFIX         "ll"
#endif

typedef uint64_t	uintmax_t;
#define _UINTMAX_T_DECLARED
typedef int64_t		intmax_t;
#define _INTMAX_T_DECLARED

#if !defined(HAS_STDINT_H) && !defined(_UINTPTR_T_DEFINED)
typedef uintmax_t	uintptr_t;
typedef intmax_t	intptr_t;
#endif
#endif // !MSDOS

/* printf integer formatters: */

#define PRId32	"d"
#define PRIu32	"u"
#define PRIi32	"i"
#define PRIx32	"x"
#define PRIX32	"X"
#define PRIo32	"o"

#define PRIi64  INTTYPES_H_64BIT_PREFIX"i"
#define PRIu64  INTTYPES_H_64BIT_PREFIX"u"
#define PRId64  INTTYPES_H_64BIT_PREFIX"d"
#define PRIx64  INTTYPES_H_64BIT_PREFIX"x"
#define PRIX64  INTTYPES_H_64BIT_PREFIX"X"
#define PRIo64  INTTYPES_H_64BIT_PREFIX"o"

#define PRIdMAX	PRId64
#define PRIiMAX	PRIi64
#define PRIuMAX	PRIu64
#define PRIxMAX	PRIx64
#define PRIXMAX	PRIX64
#define PRIoMAX	PRIo64

/* scanf integer formatters: */

#define SCNd32 	PRId32
#define SCNi32 	PRIi32
#define SCNu32 	PRIu32
#define SCNx32 	PRIx32
#define SCNX32 	PRIX32
#define SCNo32 	PRIo32

#define SCNd64 	PRId64
#define SCNi64 	PRIi64
#define SCNu64 	PRIu64
#define SCNx64 	PRIx64
#define SCNX64 	PRIX64
#define SCNo64 	PRIo64

#define SCNdMAX	PRId64
#define SCNiMAX	PRIi64
#define SCNuMAX	PRIu64
#define SCNxMAX	PRIx64
#define SCNXMAX	PRIX64
#define SCNoMAX	PRIo64

#endif

/* Legacy 32-bit time_t */
typedef int32_t         time32_t;

#if defined(_WIN32)
#  if defined _MSC_VER && !defined _FILE_OFFSET_BITS
#    define _FILE_OFFSET_BITS 64
#  endif
#  if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS==64)
#    define off_t       int64_t
#    define PRIdOFF     PRId64
#    define PRIuOFF     PRIu64
#  else
#    define PRIdOFF     "ld"
#    define PRIuOFF     "lu"
#  endif
#elif defined(__linux__) || defined(__sun__)
#  if (defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS==64)) || defined(__LP64__)
#    define PRIdOFF     PRId64
#    define PRIuOFF     PRIu64
#  else
#    define PRIdOFF     "ld"
#    define PRIuOFF     "lu"
#  endif
#else
#  define PRIdOFF       PRId64
#  define PRIuOFF       PRIu64
#endif

/* Windows Types */

#ifndef _WIN32
#ifndef BYTE
#define BYTE    uint8_t
#endif
#ifndef WORD
#define WORD    uint16_t
#endif
#ifndef DWORD
#define DWORD   uint32_t
#endif
#ifndef BOOL
#define BOOL    int
#endif

#ifndef TRUE
#define TRUE    1
#define FALSE   0
#endif
#ifndef HANDLE
#define HANDLE  void*
#endif
#endif

#ifndef INT_TO_BOOL
#define INT_TO_BOOL(x)  ((x)?TRUE:FALSE)
#endif

/* Custom Types */
typedef struct {
        char*   name;
        char*   value;
} named_string_t;

typedef struct {
        char*   name;
        int     value;
} named_int_t;

typedef struct {
        char*   name;
        uint    value;
} named_uint_t;

typedef struct {
        char*   name;
        long    value;
} named_long_t;

typedef struct {
        char*   name;
        ulong   value;
} named_ulong_t;

typedef struct {
        char*   name;
        short   value;
} named_short_t;

typedef struct {
        char*   name;
        ushort  value;
} named_ushort_t;

typedef struct {
        char*   name;
        float   value;
} named_float_t;

typedef struct {
        char*   name;
        double  value;
} named_double_t;

typedef struct {
        char*   name;
        BOOL    value;
} named_bool_t;

typedef struct {
        int     key;
        char*   value;
} keyed_string_t;

typedef struct {
        int     key;
        int     value;
} keyed_int_t;


/************************/
/* Handy Integer Macros */
/************************/

/* Data Block Length Alignment Macro (returns required padding length for proper alignment) */
#define PAD_LENGTH_FOR_ALIGNMENT(len,blk)       (((len)%(blk))==0 ? 0 : (blk)-((len)%(blk)))

#define HEX_DIGITS(n)   ( n > 0xfffffff  ? 8 \
                        : n > 0x0ffffff  ? 7 \
                        : n > 0x00fffff  ? 6 \
                        : n > 0x000ffff  ? 5 \
                        : n > 0x0000fff  ? 4 \
                        : n > 0x00000ff  ? 3 \
                        : n > 0x000000f  ? 2 : 1 )
#define DEC_DIGITS(n)   ( n < 10         ? 1 \
                        : n < 100        ? 2 \
                        : n < 1000       ? 3 \
                        : n < 10000      ? 4 \
                        : n < 100000     ? 5 \
                        : n < 1000000    ? 6 \
                        : n < 10000000   ? 7 \
                        : n < 100000000  ? 8 \
                        : n < 1000000000 ? 9 : 10 )

/***********************/
/* Handy String Macros */
/***********************/

/* Null-Terminate an ASCIIZ char array */
#define TERMINATE(str)                      str[sizeof(str)-1]=0

/* This is a bound-safe version of strcpy basically - only works with fixed-length arrays */
#ifdef SAFECOPY_USES_SPRINTF
#define SAFECOPY(dst,src)                   sprintf(dst,"%.*s",(int)sizeof(dst)-1,src)
#else   /* strncpy is faster */
#define SAFECOPY(dst,src)                   (strncpy(dst,src,sizeof(dst)), TERMINATE(dst))
#endif

#define SAFECAT(dst, src) do { \
	if(strlen((char*)(dst)) + strlen((char*)(src)) < sizeof(dst)) { \
		strcat((char*)(dst), (char*)(src)); \
	} \
} while(0)

/* Bound-safe version of sprintf() - only works with fixed-length arrays */
#if (defined __FreeBSD__) || (defined __NetBSD__) || (defined __OpenBSD__) || (defined(__APPLE__) && defined(__MACH__) && defined(__POWERPC__))
/* *BSD *nprintf() is already safe */
#define SAFEPRINTF(dst,fmt,arg)             snprintf(dst,sizeof(dst),fmt,arg)
#define SAFEPRINTF2(dst,fmt,a1,a2)          snprintf(dst,sizeof(dst),fmt,a1,a2)
#define SAFEPRINTF3(dst,fmt,a1,a2,a3)		snprintf(dst,sizeof(dst),fmt,a1,a2,a3)
#define SAFEPRINTF4(dst,fmt,a1,a2,a3,a4)	snprintf(dst,sizeof(dst),fmt,a1,a2,a3,a4)
#else										
#define SAFEPRINTF(dst,fmt,arg)				snprintf(dst,sizeof(dst),fmt,arg), TERMINATE(dst)
#define SAFEPRINTF2(dst,fmt,a1,a2)			snprintf(dst,sizeof(dst),fmt,a1,a2), TERMINATE(dst)
#define SAFEPRINTF3(dst,fmt,a1,a2,a3)		snprintf(dst,sizeof(dst),fmt,a1,a2,a3), TERMINATE(dst)
#define SAFEPRINTF4(dst,fmt,a1,a2,a3,a4)	snprintf(dst,sizeof(dst),fmt,a1,a2,a3,a4), TERMINATE(dst)
#endif

/* Replace every occurrence of c1 in str with c2, using p as a temporary char pointer */
#define REPLACE_CHARS(str,c1,c2,p)      for((p)=(str);*(p);(p)++) if(*(p)==(c1)) *(p)=(c2);

/* ASCIIZ char* parsing helper macros */
/* These (unsigned char) typecasts defeat MSVC debug assertion when passed a negative value */
#define IS_WHITESPACE(c)				(isspace((unsigned char)(c)) || c == CP437_NO_BREAK_SPACE)
#define IS_CONTROL(c)					iscntrl((unsigned char)(c))
#define IS_ALPHA(c)						isalpha((unsigned char)(c))
#define IS_ALPHANUMERIC(c)				isalnum((unsigned char)(c))
#define IS_UPPERCASE(c)					isupper((unsigned char)(c))
#define IS_LOWERCASE(c)					islower((unsigned char)(c))
#define IS_PUNCTUATION(c)				ispunct((unsigned char)(c))
#define IS_PRINTABLE(c)					isprint((unsigned char)(c))
#define IS_DIGIT(c)						isdigit((unsigned char)(c))
#define IS_HEXDIGIT(c)					isxdigit((unsigned char)(c))
#define IS_OCTDIGIT(c)					((c) >= '0' && (c) <= '7')
#define SKIP_WHITESPACE(p)              while((p) && *(p) && IS_WHITESPACE(*(p)))        (p)++;
#define FIND_WHITESPACE(p)              while((p) && *(p) && !IS_WHITESPACE(*(p)))       (p)++;
#define SKIP_CHAR(p,c)                  while((p) && *(p)==c)                            (p)++;
#define FIND_CHAR(p,c)                  while((p) && *(p) && *(p)!=c)                    (p)++;
#define SKIP_CHARSET(p,s)               while((p) && *(p) && strchr(s,*(p))!=NULL)       (p)++;
#define FIND_CHARSET(p,s)               while((p) && *(p) && strchr(s,*(p))==NULL)       (p)++;
#define SKIP_CRLF(p)					SKIP_CHARSET(p, "\r\n")
#define FIND_CRLF(p)					FIND_CHARSET(p, "\r\n")
#define SKIP_ALPHA(p)                   while((p) && *(p) && IS_ALPHA(*(p)))             (p)++;
#define FIND_ALPHA(p)                   while((p) && *(p) && !IS_ALPHA(*(p)))            (p)++;
#define SKIP_ALPHANUMERIC(p)            while((p) && *(p) && IS_ALPHANUMERIC(*(p)))      (p)++;
#define FIND_ALPHANUMERIC(p)            while((p) && *(p) && !IS_ALPHANUMERIC(*(p)))     (p)++;
#define SKIP_DIGIT(p)                   while((p) && *(p) && IS_DIGIT(*(p)))             (p)++;
#define FIND_DIGIT(p)                   while((p) && *(p) && !IS_DIGIT(*(p)))            (p)++;
#define SKIP_HEXDIGIT(p)                while((p) && *(p) && IS_HEXDIGIT(*(p)))          (p)++;
#define FIND_HEXDIGIT(p)                while((p) && *(p) && !IS_HEXDIGIT(*(p)))         (p)++;

#define HEX_CHAR_TO_INT(ch) 			(((ch)&0xf)+(((ch)>>6)&1)*9)
#define DEC_CHAR_TO_INT(ch)				((ch)&0xf)
#define OCT_CHAR_TO_INT(ch)				((ch)&0x7)

/* Variable/buffer initialization (with zeros) */
#define ZERO_VAR(var)                           memset(&(var),0,sizeof(var))
#define ZERO_ARRAY(array)                       memset(array,0,sizeof(array))

/****************************************************************************/
/* MALLOC/FREE Macros for various compilers and environments                */
/* MALLOC is used for allocations of 64k or less                            */
/* FREE is used to free buffers allocated with MALLOC                       */
/* LMALLOC is used for allocations of possibly larger than 64k              */
/* LFREE is used to free buffers allocated with LMALLOC                     */
/* REALLOC is used to re-size a previously MALLOCed or LMALLOCed buffer     */
/* FAR16 is used to create a far (32-bit) pointer in 16-bit compilers       */
/* HUGE16 is used to create a huge (32-bit) pointer in 16-bit compilers     */
/****************************************************************************/
#if defined(__COMPACT__) || defined(__LARGE__) || defined(__HUGE__)
        #define HUGE16 huge
        #define FAR16 far
        #if defined(__TURBOC__)
                #define REALLOC(x,y) farrealloc(x,y)
                #define LMALLOC(x) farmalloc(x)
                #define MALLOC(x) farmalloc(x)
                #define LFREE(x) farfree(x)
                #define FREE(x) farfree(x)
        #elif defined(__WATCOMC__)
                #define REALLOC realloc
                #define LMALLOC(x) halloc(x,1)  /* far heap, but slow */
                #define MALLOC malloc           /* far heap, but 64k max */
                #define LFREE hfree
                #define FREE free
        #else   /* Other 16-bit Compiler */
                #define REALLOC realloc
                #define LMALLOC malloc
                #define MALLOC malloc
                #define LFREE free
                #define FREE free
        #endif
#else           /* 32-bit Compiler or Small Memory Model */
        #define HUGE16
        #define FAR16
        #define REALLOC realloc
        #define LMALLOC malloc
        #define MALLOC malloc
        #define LFREE free
        #define FREE free
#endif

/********************************/
/* Handy Pointer-freeing Macros */
/********************************/
#define FREE_AND_NULL(x)	do {                  \
								if((x)!=NULL) {   \
									FREE(x);      \
									(x)=NULL;     \
								}		          \
							} while(0)
#define FREE_LIST_ITEMS(list,i)         if(list!=NULL) {                                \
											for(i=0;list[i]!=NULL;i++)      \
												FREE_AND_NULL(list[i]); \
                                        }
#define FREE_LIST(list,i)               FREE_LIST_ITEMS(list,i) FREE_AND_NULL(list)

/********************************/
/* Other Pointer-List Macros    */
/********************************/
#define COUNT_LIST_ITEMS(list,i)        { i=0; if(list!=NULL) while(list[i]!=NULL) i++; }

#if defined(__unix__)
        #include <syslog.h>
#else
        /*
         * log priorities (copied from BSD syslog.h)
         */
        #define LOG_EMERG       0       /* system is unusable */
        #define LOG_ALERT       1       /* action must be taken immediately */
        #define LOG_CRIT        2       /* critical conditions */
        #define LOG_ERR         3       /* error conditions */
        #define LOG_WARNING     4       /* warning conditions */
        #define LOG_NOTICE      5       /* normal but significant condition */
        #define LOG_INFO        6       /* informational */
        #define LOG_DEBUG       7       /* debug-level messages */
#endif

#ifdef WITH_SDL_AUDIO
        #include <SDL.h>
#endif

#endif /* Don't add anything after this #endif statement */
