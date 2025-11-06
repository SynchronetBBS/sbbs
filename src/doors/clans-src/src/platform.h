#ifndef THE_CLANS__PLATFORM___H
#define THE_CLANS__PLATFORM___H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef __unix__
# include <dos.h>
# include <share.h>
#endif

#ifdef __MSDOS__
# include <alloc.h>
# include <malloc.h>
#endif

#ifdef _WIN32
# include "win_wrappers.h"
#endif

#ifdef __unix__
# include "unix_wrappers.h"
#endif

size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);
int32_t CRCValue(const void *Data, ptrdiff_t DataSize);
bool isRelative(const char *fname);
bool SameFile(const char *f1, const char *f2);
bool DirExists(const char *pszDirName);
void Strip(char *szString);
void RemovePipes(char *pszSrc, char *pszDest);
bool iscodechar(char c);
int32_t DaysBetween(char szFirstDate[], char szLastDate[]);
int32_t DaysSince1970(char szTheDate[]);

#endif
