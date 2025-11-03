#ifndef THE_CLANS__CONSOLE___H
#define THE_CLANS__CONSOLE___H

#include <stdnoreturn.h>
#include "defines.h"

bool Door_Initialized(void);
void rputs(const char *str);
void CheckMem(const void *Test);
noreturn void System_Error(char *szErrorMsg);
void LogDisplayStr(const char *szString);

#endif
