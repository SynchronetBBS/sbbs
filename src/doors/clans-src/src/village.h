#ifndef THE_CLANS__VILLAGE___H
#define THE_CLANS__VILLAGE___H

#include "structs.h"

extern struct village Village;

void Village_Init(void);
void Village_Close(void);

void Village_Maint(void);

void Village_NewRuler(void);

int16_t OutsiderTownHallMenu(void);
int16_t TownHallMenu(void);

void Village_Reset(void);

#endif
