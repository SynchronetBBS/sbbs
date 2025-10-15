/*
 * MyOpen ADT
 */

#include <stdio.h>
#include "defines.h"
#include "serialize.h"
#include "deserialize.h"

#ifndef MYOPEN_H
#define MYOPEN_H

extern uint8_t serBuf[4096];
extern int16_t erRet;

struct FileHeader {
	FILE *fp;     // used only when reading the file in
	char szFileName[30];
	int32_t lStart, lEnd, lFileSize;
};

void MyOpen(char *szFileName, char *szMode, struct FileHeader *FileHeader);
/*
 * This function opens the file specified and sees if it is a .PAKfile
 * file or a regular DOS file.
 */

void EncryptWrite(void *Data, int32_t DataSize, FILE *fp, char XorValue);
int16_t EncryptRead(void *Data, int32_t DataSize, FILE *fp, char XorValue);

#define EXTRABYTES      10L

#define EncryptWrite_s(s, d, fp, xv) do {                                 \
	STATIC_ASSERT(sizeof(*d) == STRUCT_SIZE_ ## s, "mismatched size"); \
	size_t bs = s_ ## s ## _s(d, serBuf, BUF_SIZE_ ## s);               \
	STATIC_ASSERT(bs = BUF_SIZE_ ## s, "mismatched size");               \
	EncryptWrite(serBuf, bs, fp, xv);                                     \
} while(0)

#define EncryptWrite16(d, fp, xv) do {    \
	((int16_t*)serBuf)[0] = SWAP16(*d); \
	EncryptWrite(serBuf, 2, fp, xv);    \
} while(0)

#define EncryptRead_s(s, d, fp, xv) do {                                 \
	size_t ret;                                                       \
	STATIC_ASSERT(sizeof(*d) == STRUCT_SIZE_ ## s, "mismatched size"); \
	ret = EncryptRead(serBuf, BUF_SIZE_ ## s, fp, xv);                  \
	assert(ret);                                                         \
	s_ ## s ## _d(serBuf, BUF_SIZE_ ## s, d);                             \
} while(0)

#define EncryptRead16(d, fp, xv) do {               \
	size_t ret = EncryptRead(serBuf, 2, fp, xv); \
	*(d) = SWAP16(((int16_t*)serBuf)[0]);         \
	assert(ret);                                   \
} while(0)

#define notEncryptRead_s(s, d, fp, xv)                                    \
	STATIC_ASSERT(sizeof(*d) == STRUCT_SIZE_ ## s, "mismatched size"); \
	erRet = EncryptRead(serBuf, BUF_SIZE_ ## s, fp, xv);                     \
	if (erRet) {                                                         \
		if (s_ ## s ## _d(serBuf, BUF_SIZE_ ## s, d) == SIZE_MAX)     \
			erRet = 0;                                             \
	}                                                                       \
	if (!erRet)

#define BUF_SIZE_clan 2310U
#define STRUCT_SIZE_clan 2608U

#define BUF_SIZE_empire 122U
#define STRUCT_SIZE_empire 152U

#define BUF_SIZE_item_data 62U
#define STRUCT_SIZE_item_data 68U

#define BUF_SIZE_Alliance 2102U
#define STRUCT_SIZE_Alliance 2312U

#define BUF_SIZE_Army 27U
#define STRUCT_SIZE_Army 28U

#define BUF_SIZE_Strategy 5U
#define STRUCT_SIZE_Strategy 5U

#define BUF_SIZE_AttackPacket 173U
#define STRUCT_SIZE_AttackPacket 204U

#define BUF_SIZE_AttackResult 263U
#define STRUCT_SIZE_AttackResult 272U

#define BUF_SIZE_EventHeader 35U
#define STRUCT_SIZE_EventHeader 40U

#define BUF_SIZE_FileHeader 46U
#define STRUCT_SIZE_FileHeader 56U

#define BUF_SIZE_game_data 105U
#define STRUCT_SIZE_game_data 108U

#define BUF_SIZE_ibbs_node_attack 4U
#define STRUCT_SIZE_ibbs_node_attack 4U

#define BUF_SIZE_ibbs_node_reset 6U
#define STRUCT_SIZE_ibbs_node_reset 8U

#define BUF_SIZE_ibbs_node_recon 9U
#define STRUCT_SIZE_ibbs_node_recon 12U

#define BUF_SIZE_Language 4036U
#define STRUCT_SIZE_Language 4040U
STATIC_ASSERT_GLOBAL(BUF_SIZE_Language <= sizeof(serBuf), "Shared buffer too small")

#define BUF_SIZE_Topic 98U
#define STRUCT_SIZE_Topic 98U

#define BUF_SIZE_NPCInfo 1235U
#define STRUCT_SIZE_NPCInfo 1238U

#define BUF_SIZE_NPCNdx 29U
#define STRUCT_SIZE_NPCNdx 30U

#define BUF_SIZE_Packet 37U
#define STRUCT_SIZE_Packet 40U

#define BUF_SIZE_SpellsInEffect 4U
#define STRUCT_SIZE_SpellsInEffect 4U

#define BUF_SIZE_pc 144U
#define STRUCT_SIZE_pc 152U

#define BUF_SIZE_PClass 69U
#define STRUCT_SIZE_PClass 70U

#define BUF_SIZE_Spell 67U
#define STRUCT_SIZE_Spell 104U

#define BUF_SIZE_SpyAttemptPacket 56U
#define STRUCT_SIZE_SpyAttemptPacket 56U

#define BUF_SIZE_SpyResultPacket 177U
#define STRUCT_SIZE_SpyResultPacket 208U

#define BUF_SIZE_TradeData 86U
#define STRUCT_SIZE_TradeData 88U

#define BUF_SIZE_TradeList 24U
#define STRUCT_SIZE_TradeList 24U

#define BUF_SIZE_UserInfo 65U
#define STRUCT_SIZE_UserInfo 66U

#define BUF_SIZE_UserScore 61U
#define STRUCT_SIZE_UserScore 64U

#define BUF_SIZE_village_data 285U
#define STRUCT_SIZE_village_data 316U

#define BUF_SIZE_LeavingData 27U
#define STRUCT_SIZE_LeavingData 28U

#define BUF_SIZE_Msg_Txt 88U
#define STRUCT_SIZE_Msg_Txt 96U

#define BUF_SIZE_Message 214U
#define STRUCT_SIZE_Message 224U

#define BUF_SIZE_MessageHeader 190U
#define STRUCT_SIZE_MessageHeader 190U

#endif
