/* sbbswrap.h */

/* Synchronet system-call wrappers */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2000 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
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

#ifndef _SBBSWRAP_H
#define _SBBSWRAP_H

#include "gen_defs.h"	/* ulong */

#ifdef DLLEXPORT
#undef DLLEXPORT
#endif
#ifdef DLLCALL
#undef DLLCALL
#endif

#ifdef _WIN32
	#ifdef WRAPPER_DLL
		#define DLLEXPORT	__declspec(dllexport)
	#else
		#define DLLEXPORT	__declspec(dllimport)
	#endif
	#ifdef __BORLANDC__
		#define DLLCALL __stdcall
	#else
		#define DLLCALL
	#endif
#else	/* !_WIN32 */
	#define DLLEXPORT
	#define DLLCALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************/
/* OS-specific */
/***************/

#if defined(_WIN32)

	#define mswait(x)			Sleep(x)
	#define sbbs_beep(freq,dur)	Beep(freq,dur)

#elif defined(__OS2__)

	#define mswait(x)			DosSleep(x)
	#define sbbs_beep(freq,dur)	DosBeep(freq,dur)

#elif defined(__unix__)

	#define mswait(x)			usleep(x*1000)
	#define _mkdir(dir)			mkdir(dir,0777)
	#define _rmdir(dir)			rmdir(dir)

	DLLEXPORT void	DLLCALL sbbs_beep(int freq, int dur);
	DLLEXPORT char*	DLLCALL strupr(char* str);
	DLLEXPORT char*	DLLCALL strlwr(char* str);

#else	/* Unsupported OS */

	#warning "Unsupported Target: Need some macros of function prototypes here."

#endif

/*********************/
/* Compiler-specific */
/*********************/

/* Compiler Description */
#if defined(__BORLANDC__)

	#define COMPILER_DESC(str) sprintf(str,"BCC %X.%02X" \
		,__BORLANDC__>>8,__BORLANDC__&0xff);	

#elif defined(_MSC_VER)

	#define COMPILER_DESC(str) sprintf(str,"MSC %u", _MSC_VER);

#elif defined(__GNUC__) && defined(__GLIBC__)

	#define COMPILER_DESC(str) sprintf(str,"GCC %u.%02u (GLIBC %u.%u)" \
		,__GNUC__,__GNUC_MINOR__,__GLIBC__,__GLIBC_MINOR__);

#elif defined(__GNUC__)

	#define COMPILER_DESC(str) sprintf(str,"GCC %u.%02u" \
		,__GNUC__,__GNUC_MINOR__);

#else /* Unknown compiler */

	#define COMPILER_DESC(str) strcpy(str,"UNKNOWN COMPILER");

#endif

/* Macros */
#if defined(_MSC_VER) || defined(__MINGW32__)

#define CHMOD(s,m)		_chmod(s,m)
#define PUTENV  		_putenv
#define GETCWD  		_getcwd

#elif defined(__BORLANDC__)

#define CHMOD(s,m)		_chmod(s,1,m)
#define PUTENV  		putenv
#define GETCWD  		getcwd

#else	/* ??? */

#define CHMOD(s,m)		chmod(s,m)
#define PUTENV  		putenv
#define GETCWD  		getcwd

#endif

#if defined(__BORLANDC__)
	#define sbbs_random(x)		random(x)
#else 
	DLLEXPORT int	DLLCALL sbbs_random(int n);
#endif

#if __BORLANDC__ > 0x0410
	#define _chmod(p,f,a)		_rtl_chmod(p,f,a) 	/* _chmod obsolete in 4.x */
#endif

#if !defined(_MSC_VER) && !defined(__BORLANDC__)
	DLLEXPORT char* DLLCALL ultoa(ulong, char*, int radix);
#endif


/* General file system wrappers for all platforms and compilers */
DLLEXPORT long		DLLCALL fdate(char *filename);
DLLEXPORT int		DLLCALL getfattr(char* filename);
DLLEXPORT ulong		DLLCALL getfreediskspace(char* path);

#ifdef __cplusplus
}
#endif

#endif	/* Don't add anything after this line */
