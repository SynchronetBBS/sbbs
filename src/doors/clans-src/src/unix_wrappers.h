#ifdef __unix__

#ifndef THE_CLANS__UNIX_WRAPPERS___H
#define THE_CLANS__UNIX_WRAPPERS___H

#include <signal.h>
#include <stdlib.h>

#include <sys/stat.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <stdio.h>
#include <time.h>

#include "defines.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#define MAXDIR  PATH_MAX
#define MAXPATH PATH_MAX

#define _SH_DENYWR   1
#define _SH_DENYRW   2

FILE * _fsopen(const char *pathname, const char *mode, int flags);

void delay(unsigned msec);

void FreeFileList(char **fl);
char **FilesOrderedByDate(const char *path, const char *match, bool *error);
const char *FileName(const char *path);

#endif

#endif
