/*
 * mocks_mail.h
 *
 * Shared mocks for mail.h functionality.
 */

#ifndef MOCKS_MAIL_H
#define MOCKS_MAIL_H

bool Mail_Read(void)
{
	return false;
}

void Mail_Write(int16_t MessageType)
{
	(void)MessageType;
}

void Mail_RequestAlliance(struct Alliance *Alliance)
{
	(void)Alliance;
}

void Mail_WriteToAllies(struct Alliance *Alliance)
{
	(void)Alliance;
}

void GenericMessage(char *szString, int16_t ToClanID[2],
	int16_t FromClanID[2], char *szFrom, bool AllowReply)
{
	(void)szString; (void)ToClanID; (void)FromClanID;
	(void)szFrom; (void)AllowReply;
}

void MyWriteMessage2(int16_t ClanID[2], bool ToAll, bool AllyReq,
	int16_t AllianceID, char *szAllyName, bool GlobalPost,
	int16_t WhichVillage)
{
	(void)ClanID; (void)ToAll; (void)AllyReq; (void)AllianceID;
	(void)szAllyName; (void)GlobalPost; (void)WhichVillage;
}

void PostMsj(struct Message *Message)
{
	(void)Message;
}

void Mail_Maint(void)
{
}

void GlobalMsgPost(void)
{
}

#endif /* MOCKS_MAIL_H */
