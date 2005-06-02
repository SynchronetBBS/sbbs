#ifndef _STRWRAP_H_
#define _STRWRAP_H_

#if defined(__cplusplus)
extern "C" {
#endif
char* itoa(int val, char* str, int radix);
char* ltoa(long val, char* str, int radix);
char* strset (char *p, char *set);
#if defined(__cplusplus)
}
#endif

#endif
