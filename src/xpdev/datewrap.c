/* Wrappers for non-standard date and time functions */

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

#include "string.h"	/* memset */
#include "genwrap.h"
#include "datewrap.h"

/* Return difference (in seconds) in time() result from standard */
time_t checktime(void)
{
	time_t		t=0x2D24BD00L;	/* Correct time_t value on Jan-1-1994 */
	struct tm	gmt;
	struct tm	tm;
	struct tm*	tmp;

	memset(&tm,0,sizeof(tm));
	tm.tm_year=94;
	tm.tm_mday=1;
	tmp = gmtime_r(&t,&gmt);
	if(tmp == NULL)
		return -1;
	return (time_t)difftime(mktime(&tm), mktime(tmp));
}

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

/* Compensates for struct tm "weirdness" */
time_t sane_timegm(struct tm* tm)
{
	if(tm->tm_year>=1900)
		tm->tm_year-=1900;
	if(tm->tm_mon)		/* Month is zero-based */
		tm->tm_mon--;
	tm->tm_isdst=0;		/* Don't adjust for DST */

	return timegm(tm);
}

time32_t time32(time32_t* tp)
{
	time_t t;
	uint32_t t32;

	t=time(NULL);
	/* coverity[store_truncates_time_t] */
	t32 = (uint32_t)t;

	if(tp!=NULL)
		*tp=(time32_t)t32;

	return (time32_t)t32;
}

time32_t mktime32(struct tm* tm)
{
	time_t t = mktime(tm); /* don't use sane_mktime since tm->tm_mon is assumed to be already zero-based */
	/* coverity[store_truncates_time_t] */
	uint32_t t32 = (uint32_t)t;
	return (time32_t)t32;
}

struct tm* localtime32(const time32_t* t32, struct tm* tm)
{
	time_t	t=*t32;
	struct tm* tmp;

	if((tmp=localtime(&t))==NULL)
		return(NULL);

	*tm = *tmp;
	return(tm);
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

#if (!defined(__unix__)) || defined(__EMSCRIPTEN__)

/****************************************************************************/
/* Win32 implementations of the recursive (thread-safe) versions of std C	*/
/* time functions (gmtime, localtime, ctime, and asctime) used in Unix.		*/
/* The native Win32 versions of these functions are already thread-safe.	*/
/****************************************************************************/

struct tm* gmtime_r(const time_t* t, struct tm* tm)
{
	struct tm* tmp = gmtime(t);

	if(tmp==NULL)
		return(NULL);

	*tm = *tmp;
	return(tm);
}

struct tm* localtime_r(const time_t* t, struct tm* tm)
{
	struct tm* tmp = localtime(t);

	if(tmp==NULL)
		return(NULL);

	*tm = *tmp;
	return(tm);
}

char* ctime_r(const time_t *t, char *buf)
{
	char* p = ctime(t);

	if(p==NULL)
		return(NULL);

	strcpy(buf,p);
	return(buf);
}

char* asctime_r(const struct tm *tm, char *buf)
{
	char* p = asctime(tm);

	if(p==NULL)
		return(NULL);

	strcpy(buf,p);
	return(buf);
}

#endif	/* !defined(__unix__) */
