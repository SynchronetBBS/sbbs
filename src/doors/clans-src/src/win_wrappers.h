#ifndef THE_CLANS__WIN_WRAPPERS___H
#define THE_CLANS__WIN_WRAPPERS___H

#include <string.h>

#include <stddef.h>

size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);

#ifdef _WIN32
#include <sys/utime.h>
#include <share.h>

#ifndef S_ISDIR
#define S_ISDIR(x) ((x) & _S_IFDIR)
#endif
#ifndef S_IREAD
#define S_IREAD _S_IREAD
#endif
#ifndef S_IWRITE
#define S_IWRITE _S_IWRITE
#endif

// These are extensions that Microsoft (correctly) added a leading underscore to...
#define unlink(x) _unlink(x)
#define strcasecmp(x, y) _stricmp(x, y)
#define fileno(x) _fileno(x)
#define cio_getch() _getch()
#define strdup(x) _strdup(x)
#define mkdir(x) _mkdir(x)

#else
#include <utime.h>
#endif

#endif
