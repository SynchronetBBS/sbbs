#ifndef THE_CLANS__ALLIANCE___H
#define THE_CLANS__ALLIANCE___H

#include "defines.h"
#include "structs.h"

void GetAlliances(struct Alliance *Alliances[MAX_ALLIANCES]);
void FreeAlliances(struct Alliance *Alliances[MAX_ALLIANCES]);
void UpdateAlliances(struct Alliance *Alliances[MAX_ALLIANCES]);
void KillAlliances(void);
void DeleteAlliance(int Index, struct Alliance *Alliances[MAX_ALLIANCES]);

#endif
