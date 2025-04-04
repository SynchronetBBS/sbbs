#ifndef _STRWRAP_H_
#define _STRWRAP_H_

#include <string.h>

#if !defined _MSC_VER && !defined __BORLANDC__

#if defined(__cplusplus)
extern "C" {
#endif
char* itoa(int val, char* str, int radix);
char* ltoa(long val, char* str, int radix);
#if defined(__cplusplus)
}
#endif

#if (!defined(__MINGW32__)) || (__GNUC__ < 5)
#define strset(x, y) memset(x, y, strlen(x))
#endif

#endif

#if defined(_MSC_VER) || defined(__MSVCRT__)
#if defined(__cplusplus)
extern "C" {
#endif

char *strndup(const char *str, size_t maxlen);
#if defined(_WIN32) && !defined(_MSC_VER)
size_t strnlen(const char *s, size_t maxlen);
#endif

#if defined(__cplusplus)
}
#endif
#endif

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__EMSCRIPTEN__)
char * strdup(const char *str);
char * strtok_r(char *str, const char *delim, char **saveptr);
#endif
#if defined(__cplusplus)
}
#endif

#endif
