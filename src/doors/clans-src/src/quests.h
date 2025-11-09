#ifndef THE_CLANS__QUESTS___H
#define THE_CLANS__QUESTS___H

#include "defines.h"
#include "structs.h"

struct Quest {
	bool Active;
	char *pszQuestName;
	char *pszQuestIndex;
	char *pszQuestFile;
	bool Known;
};

struct EventHeader {
	char szName[30];
	int32_t EventSize;
	bool Event;           // 1 = Event, 0 = Result
};

extern struct Quest Quests[MAX_QUESTS];
extern uint8_t Quests_TFlags[8];

void ClearFlags(uint8_t *Flags);

bool RunEvent(bool QuoteToggle, char *szEventFile, char *szEventName,
			  struct NPCInfo *NPCInfo, char *szNPCIndex);

void Quests_GoQuest(void);

void Quests_Init(void);
void Quests_Close(void);

#endif
