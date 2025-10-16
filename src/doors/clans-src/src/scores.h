#ifndef THE_CLANS__SCORES___H
#define THE_CLANS__SCORES___H

#include "structs.h"

void DisplayScores(bool MakeFile);

void CreateScoreData(bool LocalOnly);
void SendScoreList(void);

void LeagueScores(void);

void ProcessScoreData(struct UserScore **UserScores);

void RemoveFromIPScores(const int16_t ClanID[2]);

#endif
