/* datewrap.c */

/* Wrappers for Borland getdate() and gettime() functions */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
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
#include "datewrap.h"	/* xpDateTime_t */

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

/**************************************/
/* Cross-platform date/time functions */
/**************************************/

xpDateTime_t xpDateTime_create(unsigned year, unsigned month, unsigned day
							  ,unsigned hour, unsigned minute, float second
							  ,xpTimeZone_t zone)
{
	xpDateTime_t	xpDateTime;

	xpDateTime.date.year	= year;
	xpDateTime.date.month	= month;
	xpDateTime.date.day		= day;
	xpDateTime.time.hour	= hour;
	xpDateTime.time.minute	= minute;
	xpDateTime.time.second	= second;
	xpDateTime.zone			= zone;

	return xpDateTime;
}

xpDateTime_t xpDateTime_now(void)
{
#if defined(_WIN32)
	SYSTEMTIME systime;

	GetLocalTime(&systime);
	return(xpDateTime_create(systime.wYear,systime.wMonth,systime.wDay
		,systime.wHour,systime.wMinute,(float)systime.wSecond+(systime.wMilliseconds*0.001F)
		,xpTimeZone_local()));
#else	/* !Win32 (e.g. Unix) */
	struct tm tm;
	struct timeval tv;
	time_t	t;

	gettimeofday(&tv, NULL);
	t=tv.tv_sec;
	localtime_r(&t,&tm);

	return xpDateTime_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday
		,tm.tm_hour,tm.tm_min,(float)tm.tm_sec+(tv.tv_usec*0.00001)
		,xpTimeZone_local());
#endif
}

xpTimeZone_t xpTimeZone_local(void)
{
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__DARWIN__)
	struct tm tm;
	time_t t;

	localtime_r(&t, &tm);
	return(tm.tm_gmtoff/60);
#else
#if defined(__BORLANDC__) || defined(__CYGWIN__)
	#define timezone _timezone
#endif

	/* Converts (_)timezone from seconds west of UTC to minutes east of UTC */
	return -timezone/60;
#endif
}

time_t xpDateTime_to_time(xpDateTime_t xpDateTime)
{
	struct tm tm;

	ZERO_VAR(tm);

	if(xpDateTime.date.year==0)
		return(0);

	tm.tm_year	= xpDateTime.date.year;
	tm.tm_mon	= xpDateTime.date.month;
	tm.tm_mday	= xpDateTime.date.day;

	tm.tm_hour	= xpDateTime.time.hour;
	tm.tm_min	= xpDateTime.time.minute;
	tm.tm_sec	= (int)xpDateTime.time.second;

	return sane_mktime(&tm);

}

xpDateTime_t time_to_xpDateTime(time_t ti)
{
	xpDateTime_t	never;
	struct tm tm;

	memset(&never,0,sizeof(never));
	if(ti==0)
		return(never);

	ZERO_VAR(tm);
	if(gmtime_r(&ti,&tm)==NULL)
		return(never);

	return xpDateTime_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday
		,tm.tm_hour,tm.tm_min,(float)tm.tm_sec,xpTimeZone_local());
}

/**********************************************/
/* Decimal-coded ISO-8601 date/time functions */
/**********************************************/

isoDate_t xpDateTime_to_isoDateTime(xpDateTime_t xpDateTime, isoTime_t* isoTime)
{
	if(isoTime!=NULL)
		*isoTime=0;

	if(xpDateTime.date.year==0)
		return(0);

	if(isoTime!=NULL)
		*isoTime=isoTime_create(xpDateTime.time.hour,xpDateTime.time.minute,xpDateTime.time.second);

	return isoDate_create(xpDateTime.date.year,xpDateTime.date.month,xpDateTime.date.day);
}

xpDateTime_t isoDateTime_to_xpDateTime(isoDate_t date, isoTime_t ti)
{
	return xpDateTime_create(isoDate_year(date),isoDate_month(date),isoDate_day(date)
		,isoTime_hour(ti),isoTime_minute(ti),(float)isoTime_second(ti),xpTimeZone_local());
}

isoDate_t time_to_isoDateTime(time_t ti, isoTime_t* isoTime)
{
	struct tm tm;

	if(isoTime!=NULL)
		*isoTime=0;

	if(ti==0)
		return(0);

	ZERO_VAR(tm);
	if(gmtime_r(&ti,&tm)==NULL)
		return(0);

	if(isoTime!=NULL)
		*isoTime=isoTime_create(tm.tm_hour,tm.tm_min,tm.tm_sec);

	return isoDate_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday);
}

isoTime_t time_to_isoTime(time_t ti)
{
	isoTime_t isoTime;
	
	time_to_isoDateTime(ti,&isoTime);

	return isoTime;
}

time_t isoDateTime_to_time(isoDate_t date, isoTime_t ti)
{
	struct tm tm;

	ZERO_VAR(tm);

	if(date==0)
		return(0);

	tm.tm_year	= isoDate_year(date);
	tm.tm_mon	= isoDate_month(date);
	tm.tm_mday	= isoDate_day(date);

	tm.tm_hour	= isoTime_hour(ti);
	tm.tm_min	= isoTime_minute(ti);
	tm.tm_sec	= isoTime_second(ti);

	return sane_mktime(&tm);
}

BOOL isoTimeZone_parse(const char* str, xpTimeZone_t* zone)
{
	unsigned hour=0,minute=0;

	switch(*str) {
		case 0:		/* local time-zone */
			*zone = xpTimeZone_local();	
			return TRUE;	
		case 'Z':	/* UTC */
			*zone = 0;		
			return TRUE;
		case '+':
		case '-':	/* "+/- HH[:]MM" */
			if(sscanf(str+1,"%2u%*s%2u",&hour,&minute)>=1) {
				*zone = (hour*60) + minute;
				if(*str=='-')
					*zone = -(*zone);
				return TRUE;
			}
			break;
	}
	return FALSE;
}

xpDateTime_t isoDateTime_parse(const char* str)
{
	char zone[16];
	xpDateTime_t	xpDateTime;

	zone[0]=0;
	ZERO_VAR(xpDateTime);

	if((sscanf(str,"%4u-%2u-%2uT%2u:%2u:%f%6s"		/* CCYY-MM-DDThh:MM:ss±hhmm */
		,&xpDateTime.date.year
		,&xpDateTime.date.month
		,&xpDateTime.date.day
		,&xpDateTime.time.hour
		,&xpDateTime.time.minute
		,&xpDateTime.time.second
		,zone)>=2
	||	sscanf(str,"%4u%2u%2uT%2u%2u%f%6s"			/* CCYYMMDDThhmmss±hhmm */
		,&xpDateTime.date.year
		,&xpDateTime.date.month
		,&xpDateTime.date.day
		,&xpDateTime.time.hour
		,&xpDateTime.time.minute
		,&xpDateTime.time.second
		,zone)>=4
	||	sscanf(str,"%4u%2u%2u%2u%2u%f%6s"			/* CCYYMMDDhhmmss±hhmm */
		,&xpDateTime.date.year
		,&xpDateTime.date.month
		,&xpDateTime.date.day
		,&xpDateTime.time.hour
		,&xpDateTime.time.minute
		,&xpDateTime.time.second
		,zone)>=1
		) && isoTimeZone_parse(zone,&xpDateTime.zone))
		return xpDateTime;

	return xpDateTime;
}

/***********************************/
/* Borland DOS date/time functions */
/***********************************/

#if !defined(__BORLANDC__)

#if defined(_WIN32)
	#include <windows.h>	/* SYSTEMTIME and GetLocalTime() */
#else
	#include <sys/time.h>	/* stuct timeval, gettimeofday() */
#endif

#include "datewrap.h"	/* struct defs, verify prototypes */

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
