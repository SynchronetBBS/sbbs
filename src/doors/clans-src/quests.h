struct PACKED Quest {
	BOOL Active;
	char *pszQuestName;
	char *pszQuestIndex;
	char *pszQuestFile;
	BOOL Known;
} PACKED;

void ClearFlags(char *Flags);

BOOL RunEvent(BOOL QuoteToggle, char *szEventFile, char *szEventName,
			  struct NPCInfo *NPCInfo, char *szNPCIndex);

void Quests_GoQuest(void);


void Quests_Init(void);
void Quests_Close(void);
