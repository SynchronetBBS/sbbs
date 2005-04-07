#ifndef INTERBBS_H
#define INTERBBS_H

// I wanted to be able to use this information after receiving a message
// so it is now external...and MUST be defined at the beginning of a program
#include "OpenDoor.h"

#if defined(_WIN32) || defined(__BORLANDC__)
	#define PRAGMA_PACK
#endif

#if defined(PRAGMA_PACK) || defined(__WATCOMC__)
	#define _PACK
#else
	#define _PACK __attribute__ ((packed))
#endif

/******************************************************************************/
/*Configuration Constants                                                     */
/******************************************************************************/
#define PROG_NAME_CHARS    35
#define PATH_CHARS         80
#define FILENAME_CHARS     12
#define MESSAGE_SUBJECT    "Inter-BBS Game Message"
#define MESSAGE_PID        "\1PID: ODIBMS 1\r"
#define MAX_LINE_LENGTH    70
#define MESSAGE_HEADER     "$"
#define MESSAGE_FOOTER     "$"
#define DELIMITER_CHAR     '$'
#define NODE_ADDRESS_CHARS 23
//#define SYSTEM_NAME_CHARS  40
//#define LOCATION_CHARS     40
#define SYSTEM_NAME_CHARS  20
#define LOCATION_CHARS     20

/******************************************************************************/
/* Data Types                                                              */
/******************************************************************************/
//#ifndef tBool
//typedef INT16 tBool;
//#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

// 1/97 added check to see if we are compiling in windows
// 3/05 This comes in from OpenDoor.h now
//#if !defined(WIN32) && !defined(__WIN32__)
//#ifndef WORD
// was an unsigned int, changed to unsigned short to match others... 1/97
//typedef unsigned short WORD;
//#endif
//#endif

//#ifndef DWORD
//typedef unsigned long DWORD;
//#endif

typedef enum
	 {
	 eSuccess,
	 eNoMoreMessages,
	 eGeneralFailure,
	 eBadParameter,
	 eNoMemory,
	 eMissingDir,
	 eFileOpenError
	 } tIBResult;

typedef struct
	 {
	 char szAddress[NODE_ADDRESS_CHARS + 1];
	 INT16 szNumber;
	 char szSystemName[SYSTEM_NAME_CHARS + 1];
	 char szSendTo;
//   char szLocation[LOCATION_CHARS + 1];
	 } tOtherNode;

typedef struct
	 {
	 char szThisNodeAddress[NODE_ADDRESS_CHARS + 1];
	 char szProgName[PROG_NAME_CHARS + 1];
	 char szNetmailDir[PATH_CHARS + 1];
	 INT16 bCrash;
	 INT16 bHold;
	 INT16 bEraseOnSend;
	 INT16 bEraseOnReceive;
	 // I added the next line
	 INT16 bDirect;
	 INT16 bImmediate;
	 INT16 bFileAttach;
	 INT16 bDeleteOnSend;
	 INT16 bIgnoreZonePoint;
	 char szToUserName[36];
	 char szFromUserName[36];
	 char szSubject[72];

	 INT16 nTotalSystems;
	 tOtherNode *paOtherSystem;
//   tOtherNode far *paOtherSystem;
	 } tIBInfo;


/******************************************************************************/
/*                         InterBBS API Functions                             */
/******************************************************************************/
//tIBResult IBSend(tIBInfo *pInfo, char *pszDestNode, void *pBuffer,
//       INT16 nBufferSize);
// 1/97 removed far keyword
tIBResult IBSend(tIBInfo *pInfo, char *pszDestNode, void *pBuffer,
		 INT16 nBufferSize, INT16 bEncoded, INT16 bUseGigo);

//tIBResult IBSendAll(tIBInfo *pInfo, void *pBuffer, INT16 nBufferSize);
//tIBResult IBSendAll(tIBInfo *pInfo, void *pBuffer, INT16 nBufferSize, INT16 thisbbsnum);
tIBResult IBSendAll(tIBInfo *pInfo, void *pBuffer, INT16 nBufferSize,
			INT16 thisbbsnum, INT16 bEncoded, INT16 bUseGigo);
tIBResult IBGet(tIBInfo *pInfo, void *pBuffer, INT16 nMaxBufferSize);
tIBResult IBReadConfig(tIBInfo *pInfo, char *pszConfigFile);

/******************************************************************************/
/*                                                       Private Declarations                                                     */
/******************************************************************************/
#if defined(PRAGMA_PACK)
	#pragma pack(push,1)            /* Disk image structures must be packed */
#endif

typedef struct _PACK
	 {
	 char szFromUserName[36];
	 char szToUserName[36];
	 char szSubject[72];
	 char szDateTime[20];                 /* "DD Mon YY  HH:MM:SS" */
	 WORD wTimesRead;
	 WORD wDestNode;
	 WORD wOrigNode;
	 WORD wCost;                                  /* Lowest unit of originator's currency */
	 WORD wOrigNet;
	 WORD wDestNet;
	 WORD wDestZone;
	 WORD wOrigZone;
	 WORD wDestPoint;
	 WORD wOrigPoint;
	 WORD wReplyTo;
	 WORD wAttribute;
	 WORD wNextReply;
	 } tMessageHeader;

#if defined(PRAGMA_PACK)
	#pragma pack(pop)       /* original packing */
#endif

#define ATTRIB_PRIVATE          0x0001
#define ATTRIB_CRASH            0x0002
#define ATTRIB_RECEIVED         0x0004
#define ATTRIB_SENT             0x0008
#define ATTRIB_FILE_ATTACH      0x0010
#define ATTRIB_IN_TRANSIT       0x0020
#define ATTRIB_ORPHAN           0x0040
#define ATTRIB_KILL_SENT        0x0080
#define ATTRIB_LOCAL            0x0100
#define ATTRIB_HOLD             0x0200
#define ATTRIB_DIRECT           0x0400  // I figured this one out...
#define ATTRIB_FILE_REQUEST     0x0800
#define ATTRIB_RECEIPT_REQ      0x1000
#define ATTRIB_IS_RECEIPT       0x2000
#define ATTRIB_AUDIT_REQ        0x4000
#define ATTRIB_FILE_UPDATE      0x8000

#define DIRECT -3
// mine ??

typedef struct
	 {
	 WORD wZone;
	 WORD wNet;
	 WORD wNode;
	 WORD wPoint;
	 } tFidoNode;


INT16 DirExists(const char *pszDirName);
void MakeFilename(const char *pszPath, const char *pszFilename, char *pszOut);
tIBResult ValidateInfoStruct(tIBInfo *pInfo);
INT16 CreateMessage(char *pszMessageDir, tMessageHeader *pHeader,
					char *pszText);
//INT16 CreateMessage(char *pszMessageDir, tMessageHeader *pHeader,
//                  char far *pszText);
DWORD GetFirstUnusedMsgNum(char *pszMessageDir);
void GetMessageFilename(char *pszMessageDir, DWORD lwMessageNum,
						char *pszOut);
INT16 WriteMessage(char *pszMessageDir, DWORD lwMessageNum,
					 tMessageHeader *pHeader, char *pszText);
//INT16 WriteMessage(char *pszMessageDir, DWORD lwMessageNum,
//                   tMessageHeader *pHeader, char far *pszText);
INT16 ReadMessage(char *pszMessageDir, DWORD lwMessageNum,
					tMessageHeader *pHeader, char **ppszText);
//INT16 ReadMessage(char *pszMessageDir, DWORD lwMessageNum,
//                  tMessageHeader *pHeader, char far **ppszText);
DWORD GetNextMSGID(void);
INT16 GetMaximumEncodedLength(INT16 nUnEncodedLength);
void EncodeBuffer(char *pszDest, const void *pBuffer, INT16 nBufferSize);
void DecodeBuffer(const char *pszSource, void *pDestBuffer, INT16 nBufferSize);
void ConvertAddressToString(char *pszDest, const tFidoNode *pNode);
void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource);
INT16 ProcessConfigFile(char *pszFileName, INT16 nKeyWords, char **papszKeyWord,
			void (*pfCallBack)(INT16, char *, void *), void *pCallBackData);
void ProcessConfigLine(INT16 nKeyword, char *pszParameter, void *pCallbackData);



INT16 IsMSGFromMember( tMessageHeader *MessageHeader, tIBInfo *pInfo);

extern WORD IBBSRead;
extern char temphost[150], *nextbbs;

extern tMessageHeader MessageHeader;

#ifdef __BID_ENDIAN__
void MessageHeader_le(tMessageHeader *);
#else
#define MessageHeader_le(x)
#endif

#endif

