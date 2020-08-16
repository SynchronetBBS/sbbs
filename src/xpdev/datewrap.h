/* datewrap.h */

/* Wrappers for non-standard date and time functions */

/* $Id: datewrap.h,v 1.28 2019/07/24 04:08:25 rswindell Exp $ */

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

#ifndef _DATEWRAP_H_
#define _DATEWRAP_H_

#include "genwrap.h"	/* time_t */

#if defined(__cplusplus)
extern "C" {
#endif

/* Return difference (in seconds) in time() result from standard (0 on success) */
DLLEXPORT time_t		DLLCALL		checktime(void);

/* Implementation of mktime()/timegm() that handles common tm element conversions for you */
DLLEXPORT time_t		DLLCALL		sane_mktime(struct tm*);
DLLEXPORT time_t		DLLCALL		sane_timegm(struct tm*);

/* Legacy (32-bit time_t) versions of time() and mktime() */
DLLEXPORT time32_t		DLLCALL		time32(time32_t* tp);
DLLEXPORT time32_t		DLLCALL		mktime32(struct tm*);
DLLEXPORT struct tm*	DLLCALL		localtime32(const time32_t* t, struct tm* tm);

/*********************************************************************************/
/* Win32 implementations of recursive (thread-safe) std C time functions on Unix */
/*********************************************************************************/

#if !defined(__unix__)	

	#include <time.h>		/* time_t, etc. */

	DLLEXPORT struct tm*    DLLCALL		gmtime_r(const time_t* t, struct tm* tm);
	DLLEXPORT struct tm*    DLLCALL		localtime_r(const time_t* t, struct tm* tm);
	DLLEXPORT char*	        DLLCALL		ctime_r(const time_t *t, char *buf);
	DLLEXPORT char*	        DLLCALL		asctime_r(const struct tm *tm, char *buf);

#endif

/* Microsoft's equivalent of GLIBC/BSD timegm() */
#if defined(_MSC_VER) || defined(__MINGW32__)
	#define timegm _mkgmtime
#endif

/***********************************/
/* Borland DOS date/time functions */
/***********************************/

#if defined(__BORLANDC__)

/* Borland C++ doesn't come with a timegm() or mkgmtime() equivalent */
#define timegm	mktime

#include <dos.h>

#else 

struct date {
	short da_year;
	char  da_day;
	char  da_mon;
};

struct time {
	unsigned char ti_min;
	unsigned char ti_hour;
	unsigned char ti_hund;
	unsigned char ti_sec;
};

#define getdate(x)	xp_getdate(x)
DLLEXPORT void	DLLCALL		xp_getdate(struct date*);
DLLEXPORT void	DLLCALL		gettime(struct time*);

#endif	/* !Borland */

#if defined(__cplusplus)
}
#endif

#endif	/* Don't add anything after this line */
