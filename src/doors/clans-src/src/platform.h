#ifndef THE_CLANS__PLATFORM___H
#define THE_CLANS__PLATFORM___H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#if defined(_WIN32) || defined(__MSDOS__)
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
#define PLAT_SH_DENYWR   1
#define PLAT_SH_DENYRW   2

bool plat_CreateSemfile(const char *filename, const char *content);
bool plat_getmode(const char *filename, unsigned *mode);
bool plat_chmod(const char *filename, unsigned mode);
bool plat_mkdir(const char *dir);
void plat_GetExePath(const char *argv0, char *buf, size_t bufsz);
FILE *plat_fsopen(const char *pathname, const char *mode, int shflag);
int plat_stricmp(const char *s1, const char *s2);
bool plat_DeleteFile(const char *fname);
void plat_Delay(unsigned msec);
void FreeFileList(char **fl);
char **FilesOrderedByDate(const char *path, const char *match, bool *error);
const char *FileName(const char *path);
bool plat_getftime(FILE *fp, uint16_t *date, uint16_t *time);
bool plat_setftime(FILE *fp, uint16_t date, uint16_t time);

#endif
