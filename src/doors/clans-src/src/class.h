#ifndef THE_CLANS__CLASS___H
#define THE_CLANS__CLASS___H

#include "defines.h"
#include "structs.h"

extern struct PClass *PClasses[MAX_PCLASSES], *Races[MAX_PCLASSES];

void PClass_Init(void);
/*
 * DeInitializes classes and races.
 *
 */

void PClass_Close(void);
/*
 * DeInitializes classes and races.
 *
 */

#endif
