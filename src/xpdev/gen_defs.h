/* gen_defs.h */

/* General(ly useful) constant, macro, and type definitions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN	/* Don't bring in excess baggage */
	#include <windows.h>
#elif defined(__OS2__)
	#define INCL_BASE	/* need this for DosSleep prototype */
	#include <os2.h>
#endif

#include <sys/types.h>

									/* Control characters */
#define STX 	0x02				/* Start of text			^B	*/
#define ETX 	0x03				/* End of text				^C	*/
#define BEL		0x07				/* Bell/beep				^G	*/
#define FF		0x0c				/* Form feed				^L	*/
#define ESC 	0x1b				/* Escape					^[	*/
#define DEL		0x7f				/* Delete					^BS	*/
#define BS		'\b'				/* Back space				^H	*/
#define TAB 	'\t'				/* Horizontal tabulation	^I	*/
#define LF		'\n'				/* Line feed				^J	*/
#define CR		'\r'				/* Carriage return			^M	*/
#define SP		' '					/* Space						*/

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
#ifndef HANDLE
#define HANDLE	void*
#endif

#define SAFECOPY(dst,src)	sprintf(dst,"%.*s",(int)sizeof(dst)-1,src)

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


#endif /* Don't add anything after this #endif statement */
