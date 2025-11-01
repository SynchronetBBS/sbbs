#ifndef THE_CLANS__MISC___H
#define THE_CLANS__MISC___H

#include <inttypes.h>

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
char atoc(const char *str, const char *desc, const char *func);
uint8_t atou8(const char *str, const char *desc, const char *func);
int16_t ato16(const char *str, const char *desc, const char *func);
uint16_t atou16(const char *str, const char *desc, const char *func);
int32_t ato32(const char *str, const char *desc, const char *func);
unsigned long atoul(const char *str, const char *desc, const char *func);
unsigned long long atoull(const char *str, const char *desc, const char *func);

#endif
