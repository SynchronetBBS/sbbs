/* datewrap.h */

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

#ifndef _DATEWRAP_H_
#define _DATEWRAP_H_

#include "genwrap.h"	/* time_t */

/**********************************************/
/* Decimal-coded ISO-8601 date/time functions */
/**********************************************/

typedef long	isoDate_t;	/* CCYYMMDD (decimal) */
typedef int		isoTime_t;	/* HHMMSS   (decimal) */

typedef struct {
	isoDate_t	date;	
	isoTime_t	time;	
} isoDateTime_t;

#define isoDate_year(date)		((date)/10000)
#define isoDate_month(date)		(((date)/100)%100)
#define isoDate_day(date)		((date)%100)

#define isoTime_hour(time)		((time)/10000)
#define isoTime_minute(time)	(((time)/100)%100)
#define isoTime_second(time)	((time)%100)

isoDateTime_t	isoDateTime_create(unsigned year, unsigned month, unsigned day
								   ,unsigned hour, unsigned minute, unsigned second);
isoDate_t		time_to_isoDate(time_t);
isoTime_t		time_to_isoTime(time_t);
isoDateTime_t	time_to_isoDateTime(time_t);
time_t			isoDate_to_time(isoDate_t date, isoTime_t time);
time_t			isoDateTime_to_time(isoDateTime_t);


/***********************************/
/* Borland DOS date/time functions */
/***********************************/

#if defined(__BORLANDC__)

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

#if defined(__cplusplus)
extern "C" {
#endif

#define getdate(x)	xp_getdate(x)
void xp_getdate(struct date*);
void gettime(struct time*);

#if defined(__cplusplus)
}
#endif

#endif	/* !Borland */

#endif	/* Don't add anything after this line */

