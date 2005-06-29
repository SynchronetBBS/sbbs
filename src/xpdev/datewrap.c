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

#include "genwrap.h"
#include "datewrap.h"	/* isoDateTime_t */

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
							  ,int zone)
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
		,systime.wHour,systime.wMinute,(float)systime.wSecond+(systime.wMilliseconds*0.001F),0));
#else	/* !Win32 (e.g. Unix) */
	struct tm tm;
	struct timeval tv;

	gettimeofday(&tv,NULL);
	localtime_r(&tv.tv_sec,&tm);

	return xpDateTime_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday
		,tm.tm_hour,tm.tm_min,(float)tm.tm_sec+(tv.tv_usec*0.00001),0);
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

xpDateTime_t time_to_xpDateTime(time_t time)
{
	xpDateTime_t	never;
	struct tm tm;

	memset(&never,0,sizeof(never));
	if(time==0)
		return(never);

	ZERO_VAR(tm);
	if(gmtime_r(&time,&tm)==NULL)
		return(never);

	return xpDateTime_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday
		,tm.tm_hour,tm.tm_min,(float)tm.tm_sec,0);
}


/**********************************************/
/* Decimal-coded ISO-8601 date/time functions */
/**********************************************/

isoDateTime_t isoDateTime_create(unsigned year, unsigned month, unsigned day
								,unsigned hour, unsigned minute, unsigned second)
{
	isoDateTime_t	isoDateTime;

	isoDateTime.date = isoDate_create(year,month,day);
	isoDateTime.time = isoTime_create(hour,minute,second);

	return isoDateTime;
}

isoDateTime_t isoDateTime_now(void)
{
	return time_to_isoDateTime(time(NULL));
}

isoDateTime_t time_to_isoDateTime(time_t time)
{
	isoDateTime_t	never;
	struct tm tm;

	memset(&never,0,sizeof(never));
	if(time==0)
		return(never);

	ZERO_VAR(tm);
	if(gmtime_r(&time,&tm)==NULL)
		return(never);

	return isoDateTime_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday
		,tm.tm_hour,tm.tm_min,tm.tm_sec);
}

isoDate_t time_to_isoDate(time_t time)
{
	isoDateTime_t	isoDateTime = time_to_isoDateTime(time);

	return isoDateTime.date;
}

isoTime_t time_to_isoTime(time_t time)
{
	isoDateTime_t	isoDateTime = time_to_isoDateTime(time);

	return isoDateTime.time;
}

time_t isoDate_to_time(isoDate_t date, isoTime_t time)
{
	struct tm tm;

	ZERO_VAR(tm);

	if(date==0)
		return(0);

	tm.tm_year	= isoDate_year(date);
	tm.tm_mon	= isoDate_month(date);
	tm.tm_mday	= isoDate_day(date);

	tm.tm_hour	= isoTime_hour(time);
	tm.tm_min	= isoTime_minute(time);
	tm.tm_sec	= isoTime_second(time);

	return sane_mktime(&tm);
}

time_t isoDateTime_to_time(isoDateTime_t iso)
{
	return isoDate_to_time(iso.date,iso.time);
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
	struct timeval tim;

	gettimeofday(&tim,NULL);
	localtime_r(&tim.tv_sec,&dte);
	nyt->ti_min=dte.tm_min;
	nyt->ti_hour=dte.tm_hour;
	nyt->ti_sec=dte.tm_sec;
	nyt->ti_hund=tim.tv_usec/10000;
#endif
}

#endif	/* !Borland */
