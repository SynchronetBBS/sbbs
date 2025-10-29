#ifndef THE_CLANS__WIN_WRAPPERS___H
#define THE_CLANS__WIN_WRAPPERS___H

#include <string.h>

#include <stddef.h>

size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);

#ifdef _MSC_VER
#include <sys/utime.h>

#define S_ISDIR(x) ((x) & _S_IFDIR)
#else
#include <utime.h>
#endif

#endif
