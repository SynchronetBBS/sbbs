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
extern bool erRet;

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

bool EncryptWrite(void *Data, size_t DataSize, FILE *fp, char XorValue);
bool EncryptRead(void *Data, size_t DataSize, FILE *fp, char XorValue);

#define EXTRABYTES      10L

#define CheckedEncryptWrite(b, s, fp, xv) do {                                                   \
	if (!EncryptWrite(b, s, fp, xv)) {                                                        \
		char szES[1024];                                                                   \
		snprintf(szES, sizeof(szES), "EncryptWrite() failed at %s:%d", __func__, __LINE__); \
		System_Error(szES);                                                                  \
	}                                                                                             \
} while(0)

#define EncryptWrite_s(s, d, fp, xv) do {                                                               \
	size_t bs = s_ ## s ## _s(d, serBuf, BUF_SIZE_ ## s);                                            \
	assert(bs == BUF_SIZE_ ## s);                                                                     \
	if (bs != SIZE_MAX) {                                                                              \
		if (!EncryptWrite(serBuf, bs, fp, xv)) {                                                    \
			char szES[1024];                                                                     \
			snprintf(szES, sizeof(szES), "EncryptWrite_s() failed at %s:%d", __func__, __LINE__); \
			System_Error(szES);                                                                    \
		}                                                                                               \
	}                                                                                                        \
	else {                                                                                                    \
		char szErrorStr[1024];                                                                             \
		snprintf(szErrorStr, sizeof(szErrorStr), "EncryptWrite_s() failed at %s:%d", __func__, __LINE__);   \
		System_Error(szErrorStr);                                                                            \
	}                                                                                                             \
} while(0)

#define EncryptWrite16(d, fp, xv) do {                                                                        \
	((int16_t*)serBuf)[0] = SWAP16S(*d);                                                                   \
	if (!EncryptWrite(serBuf, 2, fp, xv)) {                                                                 \
		char szErrorStr[1024];                                                                           \
		snprintf(szErrorStr, sizeof(szErrorStr), "EncryptWrite_s() failed at %s:%d", __func__, __LINE__); \
		System_Error(szErrorStr);                                                                          \
	}                                                                                                           \
} while(0)

#define EncryptRead_s(s, d, fp, xv) do {                                                               \
	bool ret;                                                                                       \
	ret = EncryptRead(serBuf, BUF_SIZE_ ## s, fp, xv);                                               \
	assert(ret);                                                                                      \
	if (ret)                                                                                           \
		s_ ## s ## _d(serBuf, BUF_SIZE_ ## s, d);                                                   \
	else {                                                                                               \
		char szErrorStr[1024];                                                                        \
		snprintf(szErrorStr, sizeof(szErrorStr), "EncryptRead_s failed at %s:%d", __func__, __LINE__); \
		System_Error(szErrorStr);                                                                       \
	}                                                                                                        \
} while(0)

#define EncryptRead16(d, fp, xv) do {                                                                     \
	bool ret = EncryptRead(serBuf, 2, fp, xv);                                                         \
	assert(ret);                                                                                        \
	if (ret)                                                                                             \
		*(d) = SWAP16S(((int16_t*)serBuf)[0]);                                                        \
	else {                                                                                                 \
		char szErrorStr[1024];                                                                          \
		snprintf(szErrorStr, sizeof(szErrorStr), "EncryptRead16() failed at %s:%d", __func__, __LINE__); \
		System_Error(szErrorStr);                                                                         \
	}                                                                                                          \
} while(0)

#define notEncryptRead_s(s, d, fp, xv)                                 \
	erRet = EncryptRead(serBuf, BUF_SIZE_ ## s, fp, xv);            \
	if (erRet) {                                                     \
		if (s_ ## s ## _d(serBuf, BUF_SIZE_ ## s, d) == SIZE_MAX) \
			erRet = false;                                     \
	}                                                                   \
	if (!erRet)

#define BUF_SIZE_Alliance 2121
#define BUF_SIZE_Army 27
#define BUF_SIZE_AttackPacket 192
#define BUF_SIZE_AttackResult 263
#define BUF_SIZE_clan 2249
#define BUF_SIZE_empire 141
#define BUF_SIZE_EventHeader 35
#define BUF_SIZE_FileHeader 42
#define BUF_SIZE_game_data 105
#define BUF_SIZE_ibbs_node_attack 4
#define BUF_SIZE_ibbs_node_recon 9
#define BUF_SIZE_ibbs_node_reset 6
#define BUF_SIZE_item_data 62
#define BUF_SIZE_Language 4032
#define BUF_SIZE_LeavingData 27
#define BUF_SIZE_Message 210
#define BUF_SIZE_MessageHeader 190
#define BUF_SIZE_Msg_Txt 84
#define BUF_SIZE_NPCInfo 1228
#define BUF_SIZE_NPCNdx 29
#define BUF_SIZE_Packet 38
#define BUF_SIZE_pc 140
#define BUF_SIZE_PClass 69
#define BUF_SIZE_Spell 39
#define BUF_SIZE_SpellsInEffect 4
#define BUF_SIZE_SpyAttemptPacket 88
#define BUF_SIZE_SpyResultPacket 186
#define BUF_SIZE_Strategy 5
#define BUF_SIZE_Topic 98
#define BUF_SIZE_TradeData 86
#define BUF_SIZE_TradeList 24
#define BUF_SIZE_UserInfo 65
#define BUF_SIZE_UserScore 61
#define BUF_SIZE_village_data 304
STATIC_ASSERT_GLOBAL(BUF_SIZE_Language <= sizeof(serBuf), "Shared buffer too small")

#endif
