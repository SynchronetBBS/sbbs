/* Cross-platform (and eXtra Precision) date/time functions */

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

#ifndef _XPDATETIME_H_
#define _XPDATETIME_H_

#include "gen_defs.h"   /* uint32_t and time_t */
#include "wrapdll.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**************************************/
/* Cross-platform date/time functions */
/**************************************/

#define INVALID_TIME    (time_t)-1  /* time_t representation of an invalid date/time */

typedef struct {
	unsigned year;      /* 0-9999 */
	unsigned month;     /* 1-12 */
	unsigned day;       /* 1-31 */
} xpDate_t;

typedef struct {
	unsigned hour;      /* 0-23 */
	unsigned minute;    /* 0-59 */
	float second;       /* 0.0-59.999, supports fractional seconds */
} xpTime_t;

typedef int xpTimeZone_t;
#define xpTimeZone_UTC      0
#define xpTimeZone_LOCAL    1

typedef struct {
	xpDate_t date;
	xpTime_t time;
	xpTimeZone_t zone;      /* minutes +/- UTC */
} xpDateTime_t;

DLLEXPORT xpDateTime_t xpDateTime_create(unsigned year, unsigned month, unsigned day
                                         , unsigned hour, unsigned minute, float second
                                         , xpTimeZone_t);
DLLEXPORT xpDateTime_t  xpDateTime_now(void);
DLLEXPORT time_t       xpDateTime_to_time(xpDateTime_t);
DLLEXPORT time_t       xpDateTime_to_localtime(xpDateTime_t);
DLLEXPORT xpDate_t     time_to_xpDate(time_t);
DLLEXPORT xpDateTime_t time_to_xpDateTime(time_t, xpTimeZone_t);
DLLEXPORT xpDateTime_t gmtime_to_xpDateTime(time_t);
DLLEXPORT xpTimeZone_t  xpTimeZone_local(void);

/**********************************************/
/* Decimal-coded ISO-8601 date/time functions */
/**********************************************/

typedef uint32_t isoDate_t;     /* CCYYMMDD (decimal) */
typedef uint32_t isoTime_t;     /* HHMMSS   (decimal) */

#define         isoDate_create(year, mon, day)    (((year) * 10000) + ((mon) * 100) + (day))
#define         isoTime_create(hour, min, sec)    (((hour) * 10000) + ((min) * 100) + ((unsigned)sec))

#define         isoDate_year(date)              ((date) / 10000)
#define         isoDate_month(date)             (((date) / 100) % 100)
#define         isoDate_day(date)               ((date) % 100)

#define         isoTime_hour(time)              ((time) / 10000)
#define         isoTime_minute(time)            (((time) / 100) % 100)
#define         isoTime_second(time)            ((time) % 100)

DLLEXPORT bool          isoTimeZoneStr_parse(const char* str, xpTimeZone_t*);
DLLEXPORT xpDateTime_t  isoDateTimeStr_parse(const char* str);

/**************************************************************/
/* Conversion between time_t (local and GMT) and isoDate/Time */
/**************************************************************/
DLLEXPORT isoTime_t time_to_isoTime(time_t);
DLLEXPORT isoTime_t gmtime_to_isoTime(time_t);
DLLEXPORT isoDate_t time_to_isoDateTime(time_t, isoTime_t*);
DLLEXPORT isoDate_t gmtime_to_isoDateTime(time_t, isoTime_t*);
DLLEXPORT time_t    isoDateTime_to_time(isoDate_t, isoTime_t);
#define         time_to_isoDate(t)      time_to_isoDateTime(t, NULL)
#define         gmtime_to_isoDate(t)    gmtime_to_isoDateTime(t, NULL)

/***************************************************/
/* Conversion between xpDate/Time and isoDate/Time */
/***************************************************/

#define         xpDate_to_isoDate(date) isoDate_create((date).year, (date).month, (date).day)
#define         xpTime_to_isoTime(time) isoTime_create((time).hour, (time).minute, (unsigned)((time).second))

DLLEXPORT xpDateTime_t isoDateTime_to_xpDateTime(isoDate_t, isoTime_t);
DLLEXPORT isoDate_t    xpDateTime_to_isoDateTime(xpDateTime_t, isoTime_t*);

/*****************************************************************/
/* Conversion from xpDate/Time/Zone to isoDate/Time/Zone Strings */
/*****************************************************************/

/* NULL sep (separator) values are automatically replaced with ISO-standard separators */

/* precision	example output
 * -2			"14"
 * -1			"14:02"
 * 0            "14:02:39"
 * 1            "14.02:39.8"
 * 2            "14.02:39.82"
 * 3            "14.02:39.829"
 */
DLLEXPORT char* xpDate_to_isoDateStr(xpDate_t
                                     , const char* sep
                                     , char* str, size_t maxlen);
DLLEXPORT char* xpTime_to_isoTimeStr(xpTime_t
                                     , const char* sep
                                     , int precision
                                     , char* str, size_t maxlen);
DLLEXPORT char* xpTimeZone_to_isoTimeZoneStr(xpTimeZone_t
                                             , const char* sep
                                             , char *str, size_t maxlen);
DLLEXPORT char* xpDateTime_to_isoDateTimeStr(xpDateTime_t
                                             , const char* date_sep, const char* datetime_sep, const char* time_sep
                                             , int precision
                                             , char* str, size_t maxlen);
DLLEXPORT char* time_to_isoDateTimeStr(time_t, xpTimeZone_t
                                       , char* str, size_t maxlen);
DLLEXPORT char* gmtime_to_isoDateTimeStr(time_t
                                         , char* str, size_t maxlen);

#if defined(__cplusplus)
}
#endif

#endif  /* Don't add anything after this line */
