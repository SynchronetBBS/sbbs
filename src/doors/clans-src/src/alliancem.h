#ifndef THE_CLANS__ALLIANCEM___H
#define THE_CLANS__ALLIANCEM___H

#include "defines.h"
#include "structs.h"

void CreateAlliance(struct Alliance *Alliance, struct Alliance *Alliances[MAX_ALLIANCES]);
void ShowAlliances(struct clan *Clan);
bool EnterAlliance(struct Alliance *Alliance);
void FormAlliance(int16_t AllyID);
void KillAlliance(int16_t WhichAlliance, struct Alliance *Alliances[MAX_ALLIANCES]);
void Alliance_Maint(void);

#endif
