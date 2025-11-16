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
	TITH_Origin,
	TITH_Signature,
	TITH_SignedData,
	TITH_SignedTLV,
	TITH_Timestamp,
	TITH_Destination,

	TITH_FIRST_CMD = 32,
	TITH_Sending,
	TITH_ACK,
	TITH_NACK,
	TITH_Info,
	TITH_LAST_CMD = TITH_Info,

	TITH_FIRST_DATA = 64,
	TITH_ID = TITH_FIRST_DATA,
	TITH_Message,
	TITH_LAST_DATA = TITH_Message,

	TITH_LAST_TYPE = TITH_LAST_DATA
};

#endif
