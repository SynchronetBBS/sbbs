#ifndef THE_CLANS__WIN_WRAPPERS___H
#define THE_CLANS__WIN_WRAPPERS___H

#ifdef _WIN32
#include <sys/utime.h>
#include <conio.h>
#include <direct.h>
#include <share.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
char *strdup(const char *s);
#endif

void display_win32_error();

#else
#include <utime.h>
#endif

#endif
