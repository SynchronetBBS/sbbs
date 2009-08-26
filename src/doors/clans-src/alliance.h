
void GetAlliances(struct Alliance *Alliances[MAX_ALLIANCES]);
void UpdateAlliances(struct Alliance *Alliances[MAX_ALLIANCES]);
void CreateAlliance(struct Alliance *Alliance, struct Alliance *Alliances[MAX_ALLIANCES]);
void ShowAlliances(struct clan *Clan);
BOOL EnterAlliance(struct Alliance *Alliance);
void KillAlliance(int AllianceID);
void Alliance_Maint(void);

void FormAlliance(int AllyID);
