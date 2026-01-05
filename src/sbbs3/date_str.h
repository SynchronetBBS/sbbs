/* Synchronet date/time string conversion routines */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#ifndef _DATE_STR_H_
#define _DATE_STR_H_

#include "scfgdefs.h"	// scfg_t
#include "dllexport.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const char* wday[];	/* abbreviated weekday names */
extern const char* mon[];	/* abbreviated month names */

DLLEXPORT char* date_format(scfg_t*, char* buf, size_t, bool verbal);
DLLEXPORT char* date_template(scfg_t*, char* buf, size_t);
DLLEXPORT char* zonestr(short zone);
DLLEXPORT time32_t dstrtounix(enum date_fmt, const char* str);
DLLEXPORT char* unixtodstr(scfg_t*, time32_t, char* str);
DLLEXPORT char* datestr(scfg_t*, time_t, char* str);
DLLEXPORT char* verbal_datestr(scfg_t*, time_t, char* str);
DLLEXPORT char* sectostr(uint seconds, char*);
DLLEXPORT char* seconds_to_str(uint, char*);
DLLEXPORT char* tm_as_hhmm(scfg_t* cfg, struct tm*, char* buf);
DLLEXPORT char* time_as_hhmm(scfg_t* cfg, time_t, char* buf);
DLLEXPORT char* timestr(scfg_t* cfg, time32_t, char* str);
DLLEXPORT char* minutes_as_hhmm(uint minutes, char* str, size_t, bool verbose);
DLLEXPORT char* minutes_to_str(uint minutes, char* str, size_t, bool estimate);

#ifdef __cplusplus
}
#endif
#endif /* Don't add anything after this line */
