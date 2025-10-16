#ifndef THE_CLANS__CLANSINI___H
#define THE_CLANS__CLANSINI___H

#include "structs.h"

extern struct IniFile IniFile;

void ClansIni_Init(void);
/*
 * Reads in CLANS.INI and stores it in memory.
 */

void ClansIni_Close(void);
/*
 * Deinitializes anything initialized by ClansIni_Init
 */

#endif
