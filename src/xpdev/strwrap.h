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

#define strset(x,y)	memset(x, y, strlen(x))

#endif

#if defined(_MSC_VER) || defined(__MSVCRT__)
#if defined(__cplusplus)
extern "C" {
#endif

char *strndup(const char *str, size_t maxlen);
size_t strnlen(const char *s, size_t maxlen);

#if defined(__cplusplus)
}
#endif
#endif

#endif
