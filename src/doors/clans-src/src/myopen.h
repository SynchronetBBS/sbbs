/*
 * MyOpen ADT
 */

#ifndef THE_CLANS__MYOPEN___H
#define THE_CLANS__MYOPEN___H

#include <stdio.h>
#include "defines.h"

struct FileHeader;
#include "serialize.h"
#include "deserialize.h"

extern uint8_t serBuf[4096];
extern size_t erRet;

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

void EncryptWrite(void *Data, size_t DataSize, FILE *fp, char XorValue);
size_t EncryptRead(void *Data, size_t DataSize, FILE *fp, char XorValue);

#define EXTRABYTES      10L

#define EncryptWrite_s(s, d, fp, xv) do {                    \
	size_t bs = s_ ## s ## _s(d, serBuf, BUF_SIZE_ ## s); \
	assert(bs == BUF_SIZE_ ## s);                          \
	if (bs != SIZE_MAX)                                     \
		EncryptWrite(serBuf, bs, fp, xv);                \
} while(0)

#define EncryptWrite16(d, fp, xv) do {     \
	((int16_t*)serBuf)[0] = SWAP16S(*d); \
	EncryptWrite(serBuf, 2, fp, xv);     \
} while(0)

#define EncryptRead_s(s, d, fp, xv) do {                 \
	size_t ret;                                       \
	ret = EncryptRead(serBuf, BUF_SIZE_ ## s, fp, xv); \
	assert(ret);                                        \
	if (ret)                                             \
		s_ ## s ## _d(serBuf, BUF_SIZE_ ## s, d);     \
} while(0)

#define EncryptRead16(d, fp, xv) do {               \
	size_t ret = EncryptRead(serBuf, 2, fp, xv); \
	assert(ret);                                  \
	if (ret)                                       \
		*(d) = SWAP16S(((int16_t*)serBuf)[0]);   \
} while(0)

#define notEncryptRead_s(s, d, fp, xv)                                 \
	erRet = EncryptRead(serBuf, BUF_SIZE_ ## s, fp, xv);            \
	if (erRet) {                                                     \
		if (s_ ## s ## _d(serBuf, BUF_SIZE_ ## s, d) == SIZE_MAX) \
			erRet = 0;                                         \
	}                                                                   \
	if (!erRet)

#define BUF_SIZE_Alliance 2121U
#define BUF_SIZE_Army 27U
#define BUF_SIZE_AttackPacket 192U
#define BUF_SIZE_AttackResult 263U
#define BUF_SIZE_clan 2249U
#define BUF_SIZE_empire 141U
#define BUF_SIZE_EventHeader 35U
#define BUF_SIZE_FileHeader 42U
#define BUF_SIZE_game_data 105U
#define BUF_SIZE_ibbs_node_attack 4U
#define BUF_SIZE_ibbs_node_recon 9U
#define BUF_SIZE_ibbs_node_reset 6U
#define BUF_SIZE_item_data 62U
#define BUF_SIZE_Language 4032U
#define BUF_SIZE_LeavingData 27U
#define BUF_SIZE_Message 210U
#define BUF_SIZE_MessageHeader 190U
#define BUF_SIZE_Msg_Txt 84U
#define BUF_SIZE_NPCInfo 1228U
#define BUF_SIZE_NPCNdx 29U
#define BUF_SIZE_Packet 38U
#define BUF_SIZE_pc 140U
#define BUF_SIZE_PClass 69U
#define BUF_SIZE_Spell 39U
#define BUF_SIZE_SpellsInEffect 4U
#define BUF_SIZE_SpyAttemptPacket 88U
#define BUF_SIZE_SpyResultPacket 186U
#define BUF_SIZE_Strategy 5U
#define BUF_SIZE_Topic 98U
#define BUF_SIZE_TradeData 86U
#define BUF_SIZE_TradeList 24U
#define BUF_SIZE_UserInfo 65U
#define BUF_SIZE_UserScore 61U
#define BUF_SIZE_village_data 304U
STATIC_ASSERT_GLOBAL(BUF_SIZE_Language <= sizeof(serBuf), "Shared buffer too small")

#endif
