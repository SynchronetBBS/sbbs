/*
 * Mail ADT
 *
 *
 */

#ifndef THE_CLANS__MAIL___H
#define THE_CLANS__MAIL___H

#include "structs.h"

int16_t Mail_Read(void);

void Mail_Write(int16_t MessageType);

void Mail_RequestAlliance(struct Alliance *Alliance);

void Mail_WriteToAllies(struct Alliance *Alliance);

void GenericReply(struct Message *Reply, char *szReply, bool AllowReply);

void GenericMessage(char *szString, int16_t ToClanID[2], int16_t FromClanID[2], char *szFrom, bool AllowReply);

void MyWriteMessage2(int16_t ClanID[2], bool ToAll,
					 bool AllyReq, int16_t AllianceID, char *szAllyName,
					 bool GlobalPost, int16_t WhichVillage);

void PostMsj(struct Message *Message);

void Mail_Maint(void);

void GlobalMsgPost(void);

#endif
