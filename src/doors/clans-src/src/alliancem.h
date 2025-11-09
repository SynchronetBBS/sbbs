#ifndef THE_CLANS__ALLIANCEM___H
#define THE_CLANS__ALLIANCEM___H

#include "defines.h"
#include "structs.h"

void CreateAlliance(struct Alliance *Alliance);
void ShowAlliances(struct clan *Clan);
bool EnterAlliance(struct Alliance *Alliance);
void FormAlliance(int16_t AllyID);
void KillAlliance(int16_t WhichAlliance);
void Alliance_Maint(void);

#endif
