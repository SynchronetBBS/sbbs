#ifndef THE_CLANS__ALLIANCE___H
#define THE_CLANS__ALLIANCE___H

#include "defines.h"
#include "structs.h"

extern struct Alliance *Alliances[MAX_ALLIANCES];
extern int16_t NumAlliances;

void DeleteAlliance(int Index);
void Alliances_Init(void);
void Alliances_Close(void);
void KillAlliances(void);

#endif
