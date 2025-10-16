/*
 * Code taken from the OpenDoors sample IBBS code.
 */

#ifndef THE_CLANS__INTERBBS___H
#define THE_CLANS__INTERBBS___H

#include "defines.h"

/******************************************************************************/
/*                         Configuration Constants                            */
/******************************************************************************/
#define PROG_NAME_CHARS    35
#define PATH_CHARS         80
#define FILENAME_CHARS     12
#define MESSAGE_SUBJECT    "OpenDoors Inter-BBS Door Message"
#define MESSAGE_PID        "\1PID: ODIBMS 1\r"
#define MAX_LINE_LENGTH    70
#define MESSAGE_HEADER     "$"
#define MESSAGE_FOOTER     "$"
#define DELIMITER_CHAR     '$'
#define NODE_ADDRESS_CHARS 23
#define SYSTEM_NAME_CHARS  40
#define LOCATION_CHARS     40

/******************************************************************************/
/*                                Data Types                                  */
/******************************************************************************/
#ifndef tBool
typedef int16_t tBool;
#endif

typedef enum {
	eSuccess,
	eNoMoreMessages,
	eGeneralFailure,
	eBadParameter,
	eNoMemory,
	eMissingDir,
	eFileOpenError
} tIBResult;

typedef struct {
	char szAddress[NODE_ADDRESS_CHARS + 1];
	char szSystemName[SYSTEM_NAME_CHARS + 1];
	char szLocation[LOCATION_CHARS + 1];
} tOtherNode;

typedef struct {
	char szThisNodeAddress[NODE_ADDRESS_CHARS + 1];
	char szProgName[PROG_NAME_CHARS + 1];
	char szNetmailDir[PATH_CHARS + 1];
	tBool bCrash;
	tBool bHold;
	tBool bEraseOnSend;
	tBool bEraseOnReceive;
	int16_t nTotalSystems;
	tOtherNode *paOtherSystem;
}  tIBInfo;


/******************************************************************************/
/*                         InterBBS API Functions                             */
/******************************************************************************/
tIBResult IBSend(tIBInfo *pInfo, char *pszDestNode, void *pBuffer,
				 int16_t nBufferSize);
tIBResult IBSendAll(tIBInfo *pInfo, void *pBuffer, int16_t nBufferSize);
tIBResult IBGet(tIBInfo *pInfo, void *pBuffer, int16_t nMaxBufferSize);
tIBResult IBReadConfig(tIBInfo *pInfo, char *pszConfigFile);


/******************************************************************************/
/*                           Private Declarations                             */
/******************************************************************************/
typedef struct MessageHeader {
	char szFromUserName[36];
	char szToUserName[36];
	char szSubject[72];
	char szDateTime[20];         /* "DD Mon YY  HH:MM:SS" */
	uint16_t wTimesRead;
	uint16_t wDestNode;
	uint16_t wOrigNode;
	uint16_t wCost;                  /* Lowest unit of originator's currency */
	uint16_t wOrigNet;
	uint16_t wDestNet;
	uint16_t wDestZone;
	uint16_t wOrigZone;
	uint16_t wDestPoint;
	uint16_t wOrigPoint;
	uint16_t wReplyTo;
	uint16_t wAttribute;
	uint16_t wNextReply;
}  tMessageHeader;

#define ATTRIB_PRIVATE      0x0001
#define ATTRIB_CRASH        0x0002
#define ATTRIB_RECEIVED     0x0004
#define ATTRIB_SENT         0x0008
#define ATTRIB_FILE_ATTACH  0x0010
#define ATTRIB_IN_TRANSIT   0x0020
#define ATTRIB_ORPHAN       0x0040
#define ATTRIB_KILL_SENT    0x0080
#define ATTRIB_LOCAL        0x0100
#define ATTRIB_HOLD         0x0200
#define ATTRIB_FILE_REQUEST 0x0800
#define ATTRIB_RECEIPT_REQ  0x1000
#define ATTRIB_IS_RECEIPT   0x2000
#define ATTRIB_AUDIT_REQ    0x4000
#define ATTRIB_FILE_UPDATE  0x8000

typedef struct {
	uint16_t wZone;
	uint16_t wNet;
	uint16_t wNode;
	uint16_t wPoint;
} tFidoNode;


tBool DirExists(const char *pszDirName);
void MakeFilename(const char *pszPath, const char *pszFilename, char *pszOut);
tIBResult ValidateInfoStruct(tIBInfo *pInfo);
tBool CreateMessage(char *pszMessageDir, tMessageHeader *pHeader,
					char *pszText);
uint32_t GetFirstUnusedMsgNum(char *pszMessageDir);
void GetMessageFilename(char *pszMessageDir, uint32_t lwMessageNum,
						char *pszOut);
tBool WriteMessage(char *pszMessageDir, uint32_t lwMessageNum,
				   tMessageHeader *pHeader, char *pszText);
tBool ReadMessage(char *pszMessageDir, uint32_t lwMessageNum,
				  tMessageHeader *pHeader, char **ppszText);
uint32_t GetNextMSGID(void);
int16_t GetMaximumEncodedLength(int16_t nUnEncodedLength);
void EncodeBuffer(char *pszDest, const void *pBuffer, int16_t nBufferSize);
void DecodeBuffer(const char *pszSource, void *pDestBuffer, int16_t nBufferSize);
void ConvertAddressToString(char *pszDest, const tFidoNode *pNode);
void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource);
tBool ProcessConfigFile(char *pszFileName, int16_t nKeyWords, char **papszKeyWord,
						void (*pfCallBack)(int, char *, void *), void *pCallBackData);
void ProcessConfigLine(int16_t nKeyword, char *pszParameter, void *pCallbackData);
tIBResult IBSendFileAttach(tIBInfo *pInfo, char *pszDestNode, char *pszFileName);
#endif
