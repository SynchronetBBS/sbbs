#ifndef THE_CLANS__WIN_WRAPPERS___H
#define THE_CLANS__WIN_WRAPPERS___H

#include <string.h>

#include <stddef.h>

size_t strlcpy(char *dst, const char *src, size_t dsize);
size_t strlcat(char *dst, const char *src, size_t dsize);

#ifdef _MSC_VER
#include <sys/utime.h>

#define S_ISDIR(x) ((x) & _S_IFDIR)

// These are extensions that Microsoft (correctly) added a leading underscore to...
#define unlink(x) _unlink(x)
#define strcasecmp(x, y) _stricmp(x, y)
#define fileno(x) _fileno(x)
#define cio_getch() _getch()
#define strdup(x) _strdup(x)
#define mkdir(x) _mkdir(x)
#define write(x, z, y) _write(x, (void*)y, (unsigned int)z)
#define close(x) _close(x)
#define open(x, y, ...) _open(x, y, __VA_ARGS__)
#define read(x, y, z) _read(x, y, z)

#define S_IREAD _S_IREAD
#define S_IWRITE _S_IWRITE
#else
#include <utime.h>
#endif

#endif
