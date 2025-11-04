/*

The Clans BBS Door Game
Copyright (C) 1997-2002 Allen Ussher

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/*
 * Miscellaneous functions we need
 */

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "defines.h"
#include "misc.h"
#include "system.h"

#define TOTAL_MONTHS    12

static bool IsLeapYear(int year)
{
	if ((year % 4) == 0) {
		if ((year % 100) == 0)
			return (year % 400) == 0;
		return true;
	}
	return false;
}

static int32_t DaysSinceJan1(char szTheDate[])
{
	int CurMonth, ThisMonth, ThisYear, ThisDay;
	int NumDaysPerMonth[2][TOTAL_MONTHS] = {
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};
	int32_t Days = 0;

	ThisMonth = atoi(szTheDate);
	ThisDay = atoi(&szTheDate[3]);
	ThisYear = atoi(&szTheDate[6]);

	for (CurMonth = 1; CurMonth < ThisMonth; CurMonth++) {
		bool LeapYear = IsLeapYear(ThisYear);

		Days += (int32_t) NumDaysPerMonth[LeapYear + 0][CurMonth - 1];
	}

	Days += ThisDay;

	return (Days);

}

int32_t DaysSince1970(char szTheDate[])
{
	int ThisYear, CurYear;
	int32_t Days = 0;

	ThisYear = atoi(&szTheDate[6]);

	for (CurYear = 1970; CurYear < ThisYear; CurYear++) {
		bool LeapYear = IsLeapYear(CurYear);

		if (LeapYear)
			Days += 366;
		else
			Days += 365;
	}

	Days += DaysSinceJan1(szTheDate);

	return (Days);
}

int32_t DaysBetween(char szFirstDate[], char szLastDate[])
/*
 * This function returns the number of days between the first date and
 * last date.
 *
 *
 * DaysBetween = LastDate - FirstDate
 *
 *
 * LastDate is MOST recent date.
 */
{
	int32_t Days;

	Days = DaysSince1970(szLastDate) - DaysSince1970(szFirstDate);

	return (Days);
}

char atoc(const char *str, const char *desc, const char *func)
{
	int ret = atoi(str);

	if (ret < CHAR_MIN || ret > CHAR_MAX) {
		char szErrorString[1024];
		snprintf(szErrorString, sizeof(szErrorString), "%s value (%d) out of range (%d to %d) in %s", desc, ret, CHAR_MIN, CHAR_MAX, func);
		System_Error(szErrorString);
	}
	return (char)ret;
}

uint8_t atou8(const char *str, const char *desc, const char *func)
{
	int ret = atoi(str);

	if (ret < 0 || ret > 0xFF) {
		char szErrorString[1024];
		snprintf(szErrorString, sizeof(szErrorString), "%s value (%hhu) out of range (%d to %d) in %s", desc, ret, 0, 0xFF, func);
		System_Error(szErrorString);
	}
	return (uint8_t)ret;
}

int16_t ato16(const char *str, const char *desc, const char *func)
{
	int ret = atoi(str);

	if (ret < INT16_MIN || ret > INT16_MAX) {
		char szErrorString[1024];
		snprintf(szErrorString, sizeof(szErrorString), "%s value (%d) out of range (%d to %d) in %s", desc, ret, INT16_MIN, INT16_MAX, func);
		System_Error(szErrorString);
	}
	return (int16_t)ret;
}

uint16_t atou16(const char *str, const char *desc, const char *func)
{
	int ret = atoi(str);

	if (ret < 0 || ret > 0xFFFF) {
		char szErrorString[1024];
		snprintf(szErrorString, sizeof(szErrorString), "%s value (%d) out of range (%d to %d) in %s", desc, ret, 0, 0xFFFF, func);
		System_Error(szErrorString);
	}
	return (uint16_t)ret;
}

int32_t ato32(const char *str, const char *desc, const char *func)
{
	long ret = atol(str);

	if (ret < INT32_MIN || ret > INT32_MAX) {
		char szErrorString[1024];
		snprintf(szErrorString, sizeof(szErrorString), "%s value (%ld) out of range (%d to %d) in %s", desc, ret, INT32_MIN, INT32_MAX, func);
		System_Error(szErrorString);
	}
	return (int32_t)ret;
}

unsigned long atoul(const char *str, const char *desc, const char *func)
{
	errno = 0;
	unsigned long long ret = strtoul(str, NULL, 0);
	if ((ret == 0 || ret == ULONG_MAX) && errno) {
		char szErrorString[1024];
		snprintf(szErrorString, sizeof(szErrorString), "%s value (%s) unparsable in %s", desc, str, func);
		System_Error(szErrorString);
	}
	return (unsigned long)ret;
}

unsigned long long atoull(const char *str, const char *desc, const char *func)
{
	errno = 0;
	unsigned long long ret = strtoull(str, NULL, 0);
	if ((ret == 0 || ret == ULLONG_MAX) && errno) {
		char szErrorString[1024];
		snprintf(szErrorString, sizeof(szErrorString), "%s value (%s) unparsable in %s", desc, str, func);
		System_Error(szErrorString);
	}
	return (unsigned long long)ret;
}
