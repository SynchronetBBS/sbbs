#include "defines.h"
#include "structs.h"

#ifndef QUESTS_H
#define QUESTS_H

struct Quest {
	bool Active;
	char *pszQuestName;
	char *pszQuestIndex;
	char *pszQuestFile;
	bool Known;
};

struct EventHeader {
	char szName[30];
	char Pad1[2];
	int32_t EventSize;
	bool Event;           // 1 = Event, 0 = Result
	char Pad2[3];
};

void ClearFlags(char *Flags);

bool RunEvent(bool QuoteToggle, char *szEventFile, char *szEventName,
			  struct NPCInfo *NPCInfo, char *szNPCIndex);

void Quests_GoQuest(void);


void Quests_Init(void);
void Quests_Close(void);

#endif
