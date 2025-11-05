#ifndef THE_CLANS__LANGUAGE___H
#define THE_CLANS__LANGUAGE___H

#include "defines.h"
#include "structs.h"

extern struct Language *Language;

void Language_Init(char *szLangFile);
/*
 * The language file is loaded.  If it isn't found, the system is shut
 * down.
 */

void Language_Close(void);
/*
 * The language file is freed from memory.
 */

void LoadStrings(int16_t StartIndex, int16_t NumStrings, char *szStrings[]);
/*
 * This function loads NumStrings of strings from the language file.
 * starting with string StartIndex.
 *
 * PRE: StartIndex is the first string's index.
 *      NumStrings is the number of strings to load.
 *
 */

void CheckMem(void *Test);
/*
 * Gives system error if the pointer is NULL.
 */

char *DupeStr(const char *Str);
/*
 * This returns a pointer to a malloc'd string of length length.
 */

#endif
