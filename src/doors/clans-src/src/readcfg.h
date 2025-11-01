#ifndef THE_CLANS__READCFG___H
#define THE_CLANS__READCFG___H

#include "structs.h"

extern struct config Config;

void AddInboundDir(const char *dir);
bool Config_Init(uint16_t Node, struct NodeData *(*getNodeData)(int));
void Config_Close(void);

#endif
