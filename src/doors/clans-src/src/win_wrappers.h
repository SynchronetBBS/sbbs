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

// These are extensions that Microsoft (correctly) added a leading underscore to.
// Map the POSIX spelling onto the underscored one rather than declaring it: the
// UCRT already declares strdup itself, so a second declaration draws C4273
// (inconsistent dll linkage) and every call site draws C4996 for the deprecated
// POSIX name -- which /sdl, on in the MSVC projects, turns into an error.
// <string.h> is included above, so this rewrites our calls, not that declaration.
#define strdup _strdup

void display_win32_error();

#else
#include <utime.h>
#endif

#endif
