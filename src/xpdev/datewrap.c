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

#if !defined(__BORLANDC__)

#include <time.h>	/* time(), time_t, struct tm, localtime() */

#if defined(_WIN32)
	#include <windows.h>	/* SYSTEMTIME and GetLocalTime() */
#endif

#include "datewrap.h"	/* struct defs, verify prototypes */

void getdate(struct date* nyd)
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
	nyt->ti_hour=systime.wHour;
	nyt->ti_min=systime.wMinute;
	nyt->ti_sec=systime.wSecond;
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
