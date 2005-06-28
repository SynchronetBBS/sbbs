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

/**********************************************/
/* Decimal-coded ISO-8601 date/time functions */
/**********************************************/
isoDateTime_t time_to_isoDateTime(time_t time)
{
	struct tm tm;
	isoDateTime_t	isoDateTime = {0,0};

	if(time==0)
		return(isoDateTime);

	ZERO_VAR(tm);
	if(gmtime_r(&time,&tm)==NULL)
		return(isoDateTime);

	isoDateTime.date=((tm.tm_year+1900)*10000)+((tm.tm_mon+1)*100)+tm.tm_mday;
	isoDateTime.time=(tm.tm_hour*10000)+(tm.tm_min*100)+tm.tm_sec;

	return(isoDateTime);
}

long time_to_isoDate(time_t time)
{
	isoDateTime_t	isoDateTime = time_to_isoDateTime(time);

	return(isoDateTime.date);
}

long time_to_isoTime(time_t time)
{
	isoDateTime_t	isoDateTime = time_to_isoDateTime(time);

	return(isoDateTime.time);
}

time_t isoDate_to_time(long date, long time)
{
	struct tm tm;

	ZERO_VAR(tm);

	if(date==0)
		return(0);

	tm.tm_year=date/10000;
	tm.tm_mon=(date/100)%100;
	tm.tm_mday=date%100;

	tm.tm_hour=time/10000;
	tm.tm_min=(time/100)%100;
	tm.tm_sec=time%100;

	/* correct for tm-weirdness */
	if(tm.tm_year>=1900)
		tm.tm_year-=1900;
	if(tm.tm_mon)
		tm.tm_mon--;
	tm.tm_isdst=-1;	/* Auto-adjust for DST */

	return(mktime(&tm));
}

time_t isoDateTime_to_time(isoDateTime_t iso)
{
	return(isoDate_to_time(iso.date,iso.time));
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
	struct tm *dte;

	tim=time(NULL);
	dte=localtime(&tim);
	nyd->da_year=dte->tm_year+1900;
	nyd->da_day=dte->tm_mday;
	nyd->da_mon=dte->tm_mon+1;
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
	struct tm *dte;
	struct timeval tim;

	gettimeofday(&tim,NULL);
	dte=localtime(&tim.tv_sec);
	nyt->ti_min=dte->tm_min;
	nyt->ti_hour=dte->tm_hour;
	nyt->ti_sec=dte->tm_sec;
	nyt->ti_hund=tim.tv_usec/10000;
#endif
}

#endif	/* !Borland */
