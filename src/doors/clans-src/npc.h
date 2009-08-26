
void NPC_AddNPCMember(char *szIndex);
void NPC_UpdateNPCNdx(char *szIndex, struct NPCNdx *NPCNdx);
void NPC_GetNPCNdx(struct NPCInfo *NPCInfo, struct NPCNdx *NPCNdx);
void NPC_GetNPC(struct pc *NPC, char *szFileName, _INT16 WhichNPC);
void NPC_ChatNPC(char *szIndex);
void NPC_ResetNPCClan(struct clan *NPCClan);

void NPC_Maint(void);
void ChatVillagers(_INT16 WhichMenu);
