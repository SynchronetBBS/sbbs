#ifndef THE_CLANS__MISC___H
#define THE_CLANS__MISC___H

int32_t DaysBetween(char szFirstDate[], char szLastDate[]);
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

int32_t DaysSince1970(char szTheDate[]);

#endif
