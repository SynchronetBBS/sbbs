#ifdef __unix__

#ifndef THE_CLANS__UNIX_WRAPPERS___H
#define THE_CLANS__UNIX_WRAPPERS___H

#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include "defines.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif
#define MAXDIR  PATH_MAX
#define MAXPATH PATH_MAX

char * fullpath(char *target, const char *path, size_t size);

#endif

#endif
