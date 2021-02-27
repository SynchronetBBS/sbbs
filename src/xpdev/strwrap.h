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

#endif
