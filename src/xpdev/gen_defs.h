/* gen_defs.h */

/* General(ly useful) constant, macro, and type definitions */

/* $Id$ */

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

#ifndef _GEN_DEFS_H
#define _GEN_DEFS_H

#include <errno.h>

/* Resolve multi-named errno constants */
#if defined(EDEADLK) && !defined(EDEADLOCK)
	#define EDEADLOCK EDEADLK
#endif

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN	/* Don't bring in excess baggage */
	#include <windows.h>
#elif defined(__OS2__)
	#define INCL_BASE	/* need this for DosSleep prototype */
	#include <os2.h>
#else
	#if (defined(__APPLE__) && defined(__MACH__) && defined(__POWERPC__)) || defined (__NetBSD__)
		#ifndef __unix__
			#define __unix__
		#endif
	#endif
#endif


#include <sys/types.h>

									/* Control characters */
#ifndef STX
#define STX 	0x02				/* Start of text			^B	*/
#endif
#ifndef ETX
#define ETX 	0x03				/* End of text				^C	*/
#endif
#ifndef BEL
#define BEL		0x07				/* Bell/beep				^G	*/
#endif
#ifndef FF
#define FF		0x0c				/* Form feed				^L	*/
#endif
#ifndef ESC
#define ESC 	0x1b				/* Escape					^[	*/
#endif
#ifndef DEL
#define DEL		0x7f				/* Delete					^BS	*/
#endif
#ifndef BS
#define BS		'\b'				/* Back space				^H	*/
#endif
#ifndef TAB
#define TAB 	'\t'				/* Horizontal tabulation	^I	*/
#endif
#ifndef LF
#define LF		'\n'				/* Line feed				^J	*/
#endif
#ifndef CR
#define CR		'\r'				/* Carriage return			^M	*/
#endif


enum {
	 CTRL_A=1
	,CTRL_B
	,CTRL_C
	,CTRL_D	
	,CTRL_E
	,CTRL_F
	,CTRL_G
	,CTRL_H
	,CTRL_I
	,CTRL_J
	,CTRL_K
	,CTRL_L
	,CTRL_M
	,CTRL_N
	,CTRL_O
	,CTRL_P
	,CTRL_Q
	,CTRL_R
	,CTRL_S
	,CTRL_T
	,CTRL_U
	,CTRL_V
	,CTRL_W
	,CTRL_X
	,CTRL_Y
	,CTRL_Z
};

/* Unsigned type short-hands	*/
#ifndef uchar
	#define uchar	unsigned char
#endif
#ifndef __GLIBC__
	#ifndef ushort
	#define ushort  unsigned short
	#define uint    unsigned int
	#define ulong   unsigned long
	#endif
#endif

/* Windows Types */
#ifndef BYTE
#define BYTE	uchar
#endif
#ifndef WORD
#define WORD	ushort
#endif
#ifndef DWORD
#define DWORD	ulong
#endif
#ifndef BOOL
#define BOOL	int
#endif
#ifndef TRUE
#define TRUE	1
#define FALSE	0
#endif
#ifndef INT_TO_BOOL
#define INT_TO_BOOL(x)	((x)?TRUE:FALSE)
#endif
#ifndef HANDLE
#define HANDLE	void*
#endif

/* Custom Types */
typedef struct {
	char*	name;
	char*	value;
} named_string_t;

typedef struct {
	char*	name;
	int		value;
} named_int_t;

typedef struct {
	char*	name;
	uint	value;
} named_uint_t;

typedef struct {
	char*	name;
	long	value;
} named_long_t;

typedef struct {
	char*	name;
	ulong	value;
} named_ulong_t;

typedef struct {
	char*	name;
	short	value;
} named_short_t;

typedef struct {
	char*	name;
	ushort	value;
} named_ushort_t;

typedef struct {
	char*	name;
	float	value;
} named_float_t;

typedef struct {
	char*	name;
	double	value;
} named_double_t;

typedef struct {
	char*	name;
	BOOL	value;
} named_bool_t;

typedef struct {
	int		key;
	char*	value;
} keyed_string_t;

typedef struct {
	int		key;
	int		value;
} keyed_int_t;


/************************/
/* Handy Integer Macros */
/************************/

/* Data Block Length Alignment Macro (returns required padding length for proper alignment) */
#define PAD_LENGTH_FOR_ALIGNMENT(len,blk)	(((len)%(blk))==0 ? 0 : (blk)-((len)%(blk)))

/***********************/
/* Handy String Macros */
/***********************/

/* This is a bound-safe version of strcpy basically - only works with fixed-length arrays */
#ifdef SAFECOPY_USES_SPRINTF
#define SAFECOPY(dst,src)				sprintf(dst,"%.*s",(int)sizeof(dst)-1,src)
#else
#define SAFECOPY(dst,src)				(strncpy(dst,src,sizeof(dst)), dst[(int)sizeof(dst)-1]=0)
#endif
#define TERMINATE(str)					str[sizeof(str)-1]=0
#if (defined __FreeBSD__) || (defined __NetBSD__) || (defined __OpenBSD__) || (defined(__APPLE__) && defined(__MACH__) && defined(__POWERPC__))
/* *BSD *nprintf() is already safe */
#define SAFEPRINTF(dst,fmt,arg)			snprintf(dst,sizeof(dst),fmt,arg)
#define SAFEPRINTF2(dst,fmt,a1,a2)		snprintf(dst,sizeof(dst),fmt,a1,a2)
#define SAFEPRINTF3(dst,fmt,a1,a2,a3)	snprintf(dst,sizeof(dst),fmt,a1,a2,a3)
#define SAFEPRINTF4(dst,fmt,a1,a2,a3,a4) snprintf(dst,sizeof(dst),fmt,a1,a2,a3,a4)
#else
#define SAFEPRINTF(dst,fmt,arg)			snprintf(dst,sizeof(dst),fmt,arg), TERMINATE(dst)
#define SAFEPRINTF2(dst,fmt,a1,a2)		snprintf(dst,sizeof(dst),fmt,a1,a2), TERMINATE(dst)
#define SAFEPRINTF3(dst,fmt,a1,a2,a3)	snprintf(dst,sizeof(dst),fmt,a1,a2,a3), TERMINATE(dst)
#define SAFEPRINTF4(dst,fmt,a1,a2,a3,a4) snprintf(dst,sizeof(dst),fmt,a1,a2,a3,a4), TERMINATE(dst)
#endif

/* Replace every occurance of c1 in str with c2, using p as a temporary char pointer */
#define REPLACE_CHARS(str,c1,c2,p)	for((p)=(str);*(p);(p)++) if(*(p)==(c1)) *(p)=(c2);

/* ASCIIZ char* parsing helper macros */
#define SKIP_WHITESPACE(p)			while(*(p) && isspace(*(p)))			(p)++;
#define FIND_WHITESPACE(p)			while(*(p) && !isspace(*(p)))			(p)++;
#define SKIP_CHAR(p,c)				while(*(p)==c)							(p)++;
#define FIND_CHAR(p,c)				while(*(p) && *(p)!=c)					(p)++;
#define SKIP_CHARSET(p,s)			while(*(p) && strchr(s,*(p))!=NULL)		(p)++;
#define FIND_CHARSET(p,s)			while(*(p) && strchr(s,*(p))==NULL)		(p)++;
#define SKIP_ALPHA(p)				while(*(p) && isalpha(*(p)))			(p)++;
#define FIND_ALPHA(p)				while(*(p) && !isalpha(*(p)))			(p)++;
#define SKIP_ALPHANUMERIC(p)		while(*(p) && isalnum(*(p)))			(p)++;
#define FIND_ALPHANUMERIC(p)		while(*(p) && !isalnum(*(p)))			(p)++;
#define SKIP_DIGIT(p)				while(*(p) && isdigit(*(p)))			(p)++;
#define FIND_DIGIT(p)				while(*(p) && !isdigit(*(p)))			(p)++;
#define SKIP_HEXDIGIT(p)			while(*(p) && isxdigit(*(p)))			(p)++;
#define FIND_HEXDIGIT(p)			while(*(p) && !isxdigit(*(p)))			(p)++;

/* Variable/buffer initialization (with zeros) */
#define ZERO_VAR(var)				memset(&(var),0,sizeof(var))
#define ZERO_ARRAY(array)			memset(array,0,sizeof(array))

/****************************************************************************/
/* MALLOC/FREE Macros for various compilers and environments				*/
/* MALLOC is used for allocations of 64k or less							*/
/* FREE is used to free buffers allocated with MALLOC						*/
/* LMALLOC is used for allocations of possibly larger than 64k				*/
/* LFREE is used to free buffers allocated with LMALLOC 					*/
/* REALLOC is used to re-size a previously MALLOCed or LMALLOCed buffer 	*/
/* FAR16 is used to create a far (32-bit) pointer in 16-bit compilers		*/
/* HUGE16 is used to create a huge (32-bit) pointer in 16-bit compilers 	*/
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
		#define LMALLOC(x) halloc(x,1)	/* far heap, but slow */
		#define MALLOC malloc			/* far heap, but 64k max */
		#define LFREE hfree
		#define FREE free
	#else	/* Other 16-bit Compiler */
		#define REALLOC realloc
		#define LMALLOC malloc
		#define MALLOC malloc
		#define LFREE free
		#define FREE free
	#endif
#else		/* 32-bit Compiler or Small Memory Model */
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
#define FREE_AND_NULL(x)			if(x!=NULL) { FREE(x); x=NULL; }
#define FREE_LIST_ITEMS(list,i)		if(list!=NULL) {				\
										for(i=0;list[i]!=NULL;i++)	\
											FREE_AND_NULL(list[i]);	\
									}
#define FREE_LIST(list,i)			FREE_LIST_ITEMS(list,i) FREE_AND_NULL(list)

/********************************/
/* Other Pointer-List Macros	*/
/********************************/
#define COUNT_LIST_ITEMS(list,i)	{ i=0; if(list!=NULL) while(list[i]!=NULL) i++; }

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


#endif /* Don't add anything after this #endif statement */
