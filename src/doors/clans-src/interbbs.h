/*
 * Code taken from the OpenDoors sample IBBS code.
 */

#ifndef INTERBBS_H
#define INTERBBS_H


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
typedef _INT16 tBool;
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#if !defined(DWORD_DEFINED) && !defined(DWORD)
typedef unsigned long DWORD;
#endif

#if !defined(WORD_DEFINED) && !defined(_MSC_VER)
#ifndef WORD
typedef unsigned _INT16 WORD;
#endif
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

typedef struct PACKED {
	char szAddress[NODE_ADDRESS_CHARS + 1];
	char szSystemName[SYSTEM_NAME_CHARS + 1];
	char szLocation[LOCATION_CHARS + 1];
} PACKED tOtherNode;

typedef struct PACKED {
	char szThisNodeAddress[NODE_ADDRESS_CHARS + 1];
	char szProgName[PROG_NAME_CHARS + 1];
	char szNetmailDir[PATH_CHARS + 1];
	tBool bCrash;
	tBool bHold;
	tBool bEraseOnSend;
	tBool bEraseOnReceive;
	_INT16 nTotalSystems;
	tOtherNode *paOtherSystem;
}  PACKED tIBInfo;


/******************************************************************************/
/*                         InterBBS API Functions                             */
/******************************************************************************/
tIBResult IBSend(tIBInfo *pInfo, char *pszDestNode, void *pBuffer,
				 _INT16 nBufferSize);
tIBResult IBSendAll(tIBInfo *pInfo, void *pBuffer, _INT16 nBufferSize);
tIBResult IBGet(tIBInfo *pInfo, void *pBuffer, _INT16 nMaxBufferSize);
tIBResult IBReadConfig(tIBInfo *pInfo, char *pszConfigFile);


/******************************************************************************/
/*                           Private Declarations                             */
/******************************************************************************/
typedef struct PACKED {
	char szFromUserName[36];
	char szToUserName[36];
	char szSubject[72];
	char szDateTime[20];         /* "DD Mon YY  HH:MM:SS" */
	WORD wTimesRead;
	WORD wDestNode;
	WORD wOrigNode;
	WORD wCost;                  /* Lowest unit of originator's currency */
	WORD wOrigNet;
	WORD wDestNet;
	WORD wDestZone;
	WORD wOrigZone;
	WORD wDestPoint;
	WORD wOrigPoint;
	WORD wReplyTo;
	WORD wAttribute;
	WORD wNextReply;
}  PACKED tMessageHeader;

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

typedef struct PACKED {
	WORD wZone;
	WORD wNet;
	WORD wNode;
	WORD wPoint;
} PACKED tFidoNode;


tBool DirExists(const char *pszDirName);
void MakeFilename(const char *pszPath, const char *pszFilename, char *pszOut);
tIBResult ValidateInfoStruct(tIBInfo *pInfo);
tBool CreateMessage(char *pszMessageDir, tMessageHeader *pHeader,
					char *pszText);
DWORD GetFirstUnusedMsgNum(char *pszMessageDir);
void GetMessageFilename(char *pszMessageDir, DWORD lwMessageNum,
						char *pszOut);
tBool WriteMessage(char *pszMessageDir, DWORD lwMessageNum,
				   tMessageHeader *pHeader, char *pszText);
tBool ReadMessage(char *pszMessageDir, DWORD lwMessageNum,
				  tMessageHeader *pHeader, char **ppszText);
DWORD GetNextMSGID(void);
_INT16 GetMaximumEncodedLength(_INT16 nUnEncodedLength);
void EncodeBuffer(char *pszDest, const void *pBuffer, _INT16 nBufferSize);
void DecodeBuffer(const char *pszSource, void *pDestBuffer, _INT16 nBufferSize);
void ConvertAddressToString(char *pszDest, const tFidoNode *pNode);
void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource);
tBool ProcessConfigFile(char *pszFileName, _INT16 nKeyWords, char **papszKeyWord,
						void (*pfCallBack)(int, char *, void *), void *pCallBackData);
void ProcessConfigLine(_INT16 nKeyword, char *pszParameter, void *pCallbackData);
tIBResult IBSendFileAttach(tIBInfo *pInfo, char *pszDestNode, char *pszFileName);
#endif
