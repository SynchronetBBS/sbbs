/* xpdatetime.c */

/* Cross-platform (and eXtra Precision) date/time functions */

/* $Id: xpdatetime.c,v 1.13 2015/11/25 07:27:07 sbbs Exp $ */

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

#include <string.h>		/* memset */
#include "datewrap.h"	/* sane_mktime */
#include "xpdatetime.h"	/* xpDateTime_t */

/**************************************/
/* Cross-platform date/time functions */
/**************************************/

xpDateTime_t DLLCALL xpDateTime_create(unsigned year, unsigned month, unsigned day
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

xpDateTime_t DLLCALL xpDateTime_now(void)
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

/* Return local timezone offset (in minutes) */
xpTimeZone_t DLLCALL xpTimeZone_local(void)
{
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__) || defined(__DARWIN__) || defined(__linux__)
	struct tm tm;
	time_t t=time(NULL);

	localtime_r(&t, &tm);
	return(tm.tm_gmtoff/60);
#elif defined(_WIN32)
	TIME_ZONE_INFORMATION	tz;
	DWORD					tzRet;

	/*****************************/
	/* Get Time-zone information */
	/*****************************/
    memset(&tz,0,sizeof(tz));
	tzRet=GetTimeZoneInformation(&tz);
	switch(tzRet) {
		case TIME_ZONE_ID_DAYLIGHT:
			tz.Bias += tz.DaylightBias;
			break;
		case TIME_ZONE_ID_STANDARD:
			tz.Bias += tz.StandardBias;
			break;
	}

	return -tz.Bias;
#else

#if defined(__BORLANDC__) || defined(__CYGWIN__)
	#define timezone _timezone
#endif

	/* Converts (_)timezone from seconds west of UTC to minutes east of UTC */
	/* Adjust for DST, assuming adjustment is always 60 minutes <sigh> */
	return -((timezone/60) - (daylight*60));
#endif
}

/* TODO: Supports local timezone and UTC only, currently */
time_t DLLCALL xpDateTime_to_time(xpDateTime_t xpDateTime)
{
	struct tm tm;

	ZERO_VAR(tm);

	if(xpDateTime.date.year==0)
		return(INVALID_TIME);

	tm.tm_year	= xpDateTime.date.year;
	tm.tm_mon	= xpDateTime.date.month;
	tm.tm_mday	= xpDateTime.date.day;

	tm.tm_hour	= xpDateTime.time.hour;
	tm.tm_min	= xpDateTime.time.minute;
	tm.tm_sec	= (int)xpDateTime.time.second;

	if(xpDateTime.zone == xpTimeZone_UTC)
		return sane_timegm(&tm);
	if(xpDateTime.zone == xpTimeZone_LOCAL || xpDateTime.zone == xpTimeZone_local())
		return sane_mktime(&tm);
	return INVALID_TIME;
}

/* This version ignores the timezone in xpDateTime and always uses mktime() */
time_t DLLCALL xpDateTime_to_localtime(xpDateTime_t xpDateTime)
{
	xpDateTime.zone = xpTimeZone_LOCAL;
	return xpDateTime_to_time(xpDateTime);
}

xpDateTime_t DLLCALL time_to_xpDateTime(time_t ti, xpTimeZone_t zone)
{
	xpDateTime_t	never;
	struct tm tm;

	ZERO_VAR(never);
	ZERO_VAR(tm);
	if(localtime_r(&ti,&tm)==NULL)
		return(never);

	return xpDateTime_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday
		,tm.tm_hour,tm.tm_min,(float)tm.tm_sec
		,zone==xpTimeZone_LOCAL ? xpTimeZone_local() : zone);
}

xpDateTime_t DLLCALL gmtime_to_xpDateTime(time_t ti)
{
	xpDateTime_t	never;
	struct tm tm;

	ZERO_VAR(never);
	ZERO_VAR(tm);
	if(gmtime_r(&ti,&tm)==NULL)
		return(never);

	return xpDateTime_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday
		,tm.tm_hour,tm.tm_min,(float)tm.tm_sec
		,xpTimeZone_UTC);
}

/**********************************************/
/* Decimal-coded ISO-8601 date/time functions */
/**********************************************/

isoDate_t DLLCALL xpDateTime_to_isoDateTime(xpDateTime_t xpDateTime, isoTime_t* isoTime)
{
	if(isoTime!=NULL)
		*isoTime=0;

	if(xpDateTime.date.year==0)
		return(0);

	if(isoTime!=NULL)
		*isoTime=isoTime_create(xpDateTime.time.hour,xpDateTime.time.minute,xpDateTime.time.second);

	return isoDate_create(xpDateTime.date.year,xpDateTime.date.month,xpDateTime.date.day);
}

xpDateTime_t DLLCALL isoDateTime_to_xpDateTime(isoDate_t date, isoTime_t ti)
{
	return xpDateTime_create(isoDate_year(date),isoDate_month(date),isoDate_day(date)
		,isoTime_hour(ti),isoTime_minute(ti),(float)isoTime_second(ti),xpTimeZone_local());
}

isoDate_t DLLCALL time_to_isoDateTime(time_t ti, isoTime_t* isoTime)
{
	struct tm tm;

	if(isoTime!=NULL)
		*isoTime=0;

	ZERO_VAR(tm);
	if(localtime_r(&ti,&tm)==NULL)
		return(0);

	if(isoTime!=NULL)
		*isoTime=isoTime_create(tm.tm_hour,tm.tm_min,tm.tm_sec);

	return isoDate_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday);
}

isoTime_t DLLCALL time_to_isoTime(time_t ti)
{
	isoTime_t isoTime;
	
	time_to_isoDateTime(ti,&isoTime);

	return isoTime;
}

isoDate_t DLLCALL gmtime_to_isoDateTime(time_t ti, isoTime_t* isoTime)
{
	struct tm tm;

	if(isoTime!=NULL)
		*isoTime=0;

	ZERO_VAR(tm);
	if(gmtime_r(&ti,&tm)==NULL)
		return(0);

	if(isoTime!=NULL)
		*isoTime=isoTime_create(tm.tm_hour,tm.tm_min,tm.tm_sec);

	return isoDate_create(1900+tm.tm_year,1+tm.tm_mon,tm.tm_mday);
}

isoTime_t DLLCALL gmtime_to_isoTime(time_t ti)
{
	isoTime_t isoTime;
	
	gmtime_to_isoDateTime(ti,&isoTime);

	return isoTime;
}

time_t DLLCALL isoDateTime_to_time(isoDate_t date, isoTime_t ti)
{
	struct tm tm;

	ZERO_VAR(tm);

	if(date==0)
		return(INVALID_TIME);

	tm.tm_year	= isoDate_year(date);
	tm.tm_mon	= isoDate_month(date);
	tm.tm_mday	= isoDate_day(date);

	tm.tm_hour	= isoTime_hour(ti);
	tm.tm_min	= isoTime_minute(ti);
	tm.tm_sec	= isoTime_second(ti);

	return sane_mktime(&tm);
}

/****************************************************************************/
/* Conversion from xpDate/Time/Zone to isoDate/Time/Zone Strings			*/
/****************************************************************************/

char* DLLCALL xpDate_to_isoDateStr(xpDate_t date, const char* sep, char* str, size_t maxlen)
{
	if(sep==NULL)
		sep="-";

	snprintf(str,maxlen,"%04u%s%02u%s%02u"
		,date.year	,sep
		,date.month	,sep
		,date.day);

	return str;
}

/* precision	example output
 * -2			"14"
 * -1			"14:02"
 * 0            "14:02:39"
 * 1            "14:02:39.8"
 * 2            "14:02:39.82"
 * 3            "14:02:39.829"
 */
char* DLLCALL xpTime_to_isoTimeStr(xpTime_t ti, const char* sep, int precision
								   ,char* str, size_t maxlen)
{
	if(sep==NULL)
		sep=":";

	if(precision < -1)			/* HH */
		snprintf(str, maxlen, "%02u", ti.hour);
	else if(precision < 0)		/* HH:MM */
		snprintf(str, maxlen, "%02u%s%02u"
			,ti.hour		,sep
			,ti.minute
			);
	else						/* HH:MM:SS[.fract] */
		snprintf(str, maxlen, "%02u%s%02u%s%0*.*f"
			,ti.hour		,sep
			,ti.minute		,sep
			,precision ? (precision+3) : 2
			,precision
			,ti.second
			);

	return str;
}

char* DLLCALL xpTimeZone_to_isoTimeZoneStr(xpTimeZone_t zone, const char* sep
								   ,char *str, size_t maxlen)
{
	xpTimeZone_t	tz=zone;

	if(tz==xpTimeZone_UTC)
		return "Z";

	if(sep==NULL)
		sep=":";

	if(tz<0)
		tz=-tz;

	snprintf(str,maxlen,"%c%02u%s%02u"
		,zone < 0 ? '-':'+'
		,tz/60
		,sep
		,tz%60);

	return str;
}

char* DLLCALL xpDateTime_to_isoDateTimeStr(xpDateTime_t dt
								   ,const char* date_sep, const char* datetime_sep, const char* time_sep
								   ,int precision
								   ,char* str, size_t maxlen)
{
	char			tz_str[16];
	char			date_str[16];
	char			time_str[16];
	
	if(datetime_sep==NULL)	datetime_sep="T";

	snprintf(str,maxlen,"%s%s%s%s"
		,xpDate_to_isoDateStr(dt.date, date_sep, date_str, sizeof(date_str))
		,datetime_sep
		,xpTime_to_isoTimeStr(dt.time, time_sep, precision, time_str, sizeof(time_str))
		,xpTimeZone_to_isoTimeZoneStr(dt.zone,time_sep,tz_str,sizeof(tz_str)));

	return str;
}

/****************************************************************************/
/* isoDate/Time/Zone String parsing functions								*/
/****************************************************************************/

BOOL DLLCALL isoTimeZoneStr_parse(const char* str, xpTimeZone_t* zone)
{
	unsigned hour=0,minute=0;

	switch(*str) {
		case 0:		/* local time-zone */
			*zone = xpTimeZone_local();	
			return TRUE;	
		case 'Z':	/* UTC */
			*zone = xpTimeZone_UTC;		
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

/* TODO: adjust times in 24:xx:xx format */
xpDateTime_t DLLCALL isoDateTimeStr_parse(const char* str)
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
		) && isoTimeZoneStr_parse(zone,&xpDateTime.zone))
		return xpDateTime;

	return xpDateTime;
}
