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

#include <stdio.h>
#include <stdlib.h>

#include "defines.h"

#define TOTAL_MONTHS    12

long DaysSinceJan1(char szTheDate[])
{
	long CurMonth, Days = 0, ThisMonth, ThisYear, ThisDay;
	BOOL LeapYear;
	_INT16 NumDaysPerMonth[2][TOTAL_MONTHS] = {
		{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};

	ThisDay = atoi(&szTheDate[3]);
	ThisMonth = atoi(szTheDate);
	ThisYear = atoi(&szTheDate[6]);

	for (CurMonth = 1; CurMonth < ThisMonth; CurMonth++) {
		LeapYear = (ThisYear % 4) == 0;

		Days += (long) NumDaysPerMonth[LeapYear + 0][CurMonth - 1];
	}

	Days += ThisDay;

	return (Days);

}

long DaysSince1970(char szTheDate[])
{
	long Days = 0, ThisYear, CurYear;
	BOOL LeapYear;

	ThisYear = atoi(&szTheDate[6]);

	for (CurYear = 1970; CurYear < ThisYear; CurYear++) {
		LeapYear = (CurYear % 4) == 0;

		if (LeapYear)
			Days += 366;
		else
			Days += 365;
	}

	Days += DaysSinceJan1(szTheDate);

	return (Days);
}

long DaysBetween(char szFirstDate[], char szLastDate[])
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
	long Days;

	Days = DaysSince1970(szLastDate) - DaysSince1970(szFirstDate);

	return (Days);
}

