/* datewrap.c */

/* Wrappers for non-standard date and time functions */

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

#include "string.h"	/* memset */
#include "genwrap.h"
#include "datewrap.h"

/* Compensates for struct tm "weirdness" */
time_t sane_mktime(struct tm* tm)
{
	if(tm->tm_year>=1900)
		tm->tm_year-=1900;
	if(tm->tm_mon)		/* Month is zero-based */
		tm->tm_mon--;
	tm->tm_isdst=-1;	/* Auto-adjust for DST */

	return mktime(tm);
}

#if !defined(__BORLANDC__)

/***********************************/
/* Borland DOS date/time functions */
/***********************************/

#if defined(_WIN32)
	#include <windows.h>	/* SYSTEMTIME and GetLocalTime() */
#else
	#include <sys/time.h>	/* stuct timeval, gettimeofday() */
#endif

void xp_getdate(struct date* nyd)
{
	time_t tim;
	struct tm dte;

	tim=time(NULL);
	localtime_r(&tim,&dte);
	nyd->da_year=dte.tm_year+1900;
	nyd->da_day=dte.tm_mday;
	nyd->da_mon=dte.tm_mon+1;
}

void gettime(struct time* nyt)
{
#if defined(_WIN32)
	SYSTEMTIME systime;

	GetLocalTime(&systime);
	nyt->ti_hour=(unsigned char)systime.wHour;
	nyt->ti_min=(unsigned char)systime.wMinute;
	nyt->ti_sec=(unsigned char)systime.wSecond;
	nyt->ti_hund=systime.wMilliseconds/10;
#else	/* !Win32 (e.g. Unix) */
	struct tm dte;
	time_t t;
	struct timeval tim;

	gettimeofday(&tim, NULL);
	t=tim.tv_sec;
	localtime_r(&t,&dte);
	nyt->ti_min=dte.tm_min;
	nyt->ti_hour=dte.tm_hour;
	nyt->ti_sec=dte.tm_sec;
	nyt->ti_hund=tim.tv_usec/10000;
#endif
}

#endif	/* !Borland */

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
