#ifndef THE_CLANS__WIN_WRAPPERS___H
#define THE_CLANS__WIN_WRAPPERS___H

#include <string.h>

#ifdef _WIN32

#include <stddef.h>

size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);

#endif
#endif
