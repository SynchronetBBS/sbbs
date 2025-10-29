#ifndef THE_CLANS__SYSTEM___H
#define THE_CLANS__SYSTEM___H

#include <stdnoreturn.h>

#include "defines.h"

extern struct system System;
extern bool Verbose;

/*
 * System functions
 */

void System_Init(void);
/*
 * Initializes whole system.
 */

noreturn void System_Close(void);
/*
 * purpose  Closes down the system, no matter WHERE it is called.
 *          Should be foolproof.
 */

void System_Error(char *szErrorMsg);
/*
 * purpose  To output an error message and close down the system.
 *          This SHOULD be run from anywhere and NOT fail.  It should
 *          be FOOLPROOF.
 */

void System_Maint(void);

int my_random(int limit);

#endif
