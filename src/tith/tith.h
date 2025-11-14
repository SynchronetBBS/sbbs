#ifndef TITH_HEADER
#define TITH_HEADER

#include <stddef.h>
#include <stdint.h>
#include <threads.h>

/*
 * FSC-0067 - Solidly good ideas here
 * FSC-0093 - Describes Tiny Seen-By
 * FSP-1040 - SRIF file request interface
 */

enum TITH_Type {
	TITH_FIRST_TYPE = 1,

	TITH_FIRST_CMD = TITH_FIRST_TYPE,
	TITH_IAm = TITH_FIRST_CMD,
	TITH_LHP1,
	TITH_LHP2,
	TITH_Sending,
	TITH_ACK,
	TITH_NACK,
	TITH_Info,
	TITH_Encrypted,
	TITH_LAST_CMD = TITH_Encrypted,

	TITH_FIRST_DATA,
	TITH_ID = TITH_FIRST_DATA,
	TITH_Filename,
	TITH_UniqueWithExtension,
	TITH_MD5Sum,
	TITH_FileContents,
	TITH_Wildcard,
	TITH_Password,
	TITH_IfNewerThan,
	TITH_IfOlderOrSameAs,
	TITH_Message,
	TITH_ConnectingAddress,
	TITH_AcceptingAddress,
	TITH_KK_Packet1,
	TITH_KK_Packet2,
	TITH_LAST_DATA = TITH_KK_Packet2,

	TITH_LAST_TYPE = TITH_LAST_DATA
};

/*
 * Commands:
 * TITH_IAm, TITH_ConnectingAddress, TITH_AcceptingAddress
 * TITH_LHP1, TITH_KK_Packet1
 * TITH_LHP2, TITH_KK_Packet2
 * TITH_Sending, TITH_Filename/TITH_UniqueWithExtension, TITH_MD5Sum, [Zero or more TITH_RequestID], TITH_FileContents
 * TITH_Requesting, (All optional) TITH_Filename, TITH_Wildcard, TITH_IfNewerThan, TITH_IfOlderOrSameAs, TITH_Password
 * TITH_ACK, TITH_ID
 * TITH_NACK, TITH_ID
 * TITH_Info, TITH_Message
 */

struct TITH_TLV {
	struct TITH_TLV *next;
	struct TITH_TLV *tail;
	size_t remain;
	enum TITH_Type type;
	uint64_t length;
	uint8_t *value;
};

extern thread_local void *handle;

#endif
