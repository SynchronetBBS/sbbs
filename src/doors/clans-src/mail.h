/*
 * Mail ADT
 *
 *
 */


_INT16 Mail_Read(void);

void Mail_Write(_INT16 MessageType);

void Mail_RequestAlliance(struct Alliance *Alliance);

void Mail_WriteToAllies(struct Alliance *Alliance);

void GenericReply(struct Message *Reply, char *szReply, BOOL AllowReply);

void GenericMessage(char *szString, _INT16 ToClanID[2], _INT16 FromClanID[2], char *szFrom, BOOL AllowReply);

void MyWriteMessage2(_INT16 ClanID[2], BOOL ToAll,
					 BOOL AllyReq, _INT16 AllianceID, char *szAllyName,
					 BOOL GlobalPost, _INT16 WhichVillage);

void PostMsj(struct Message *Message);

void Mail_Maint(void);

void GlobalMsgPost(void);
