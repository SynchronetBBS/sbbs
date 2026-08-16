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
	TITH_Origin = TITH_FIRST_TYPE,
	TITH_Signature = 2,
	TITH_SignedData = 3,
	TITH_SignedTLV = 4,
	TITH_Timestamp = 5,
	TITH_Destination = 6,
	TITH_Address = 7,
	TITH_PublicKey = 8,

	TITH_FIRST_EXPERIMENTAL = 32,
	TITH_LAST_EXPERIMENTAL = 63,

	TITH_FIRST_BUNDLE_TYPE = 64,
	TITH_Message = TITH_FIRST_BUNDLE_TYPE,
	TITH_File = 65,
	TITH_FileRequest = 66,
	TITH_Rejected = 67,
	TITH_Accepted = 68,
	TITH_PollMessages = 69,
	TITH_PollFiles = 70,
	TITH_PollFileRequests = 71,
	TITH_LAST_BUNDLE_TYPE = TITH_PollFileRequests,

	TITH_FIRST_ANCILLARY_TYPE = 96,
	TITH_Filename = TITH_FIRST_ANCILLARY_TYPE,
	TITH_Contents = 97,
	TITH_RequestIdentifier = 98,
	TITH_TLVHash = 99,
	TITH_LegacyAttributes = 101,
	TITH_TimestampOffset = 102,
	TITH_ToUserName = 103,
	TITH_FromUserName = 104,
	TITH_Subject = 105,
	TITH_MessageText = 106,
	TITH_Area = 107,
	TITH_AreaName = 108,
	TITH_AreaDescription = 109,
	TITH_TearLine = 110,
	TITH_OriginLine = 111,
	TITH_SeenBy = 112,
	TITH_Via = 113,
	TITH_MessageID = 114,
	TITH_ReplyTo = 115,
	TITH_OriginalCharacterSet = 116,
	TITH_AdditionalKludgeLine = 117,
	TITH_ShortDescription = 118,
	TITH_LongDescriptionLine = 119,
	TITH_MagicWord = 120,
	TITH_Replaces = 121,
	TITH_LAST_ANCILLARY_TYPE = TITH_Replaces,

	TITH_LAST_TYPE = TITH_LAST_ANCILLARY_TYPE
};

#endif
