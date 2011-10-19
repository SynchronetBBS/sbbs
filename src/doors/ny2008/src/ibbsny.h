#ifndef INTERBBS_H
#define INTERBBS_H

/******************************************************************************/
/*						   Configuration Constants							  */
/******************************************************************************/
/*#define PROG_NAME_CHARS    35
#define PATH_CHARS         80
#define FILENAME_CHARS     12
#define MESSAGE_SUBJECT    "##NY2008 v0.10 IBBS Door Message"
#define MESSAGE_PID        "\1PID: ODIBMS 1\r"
#define MAX_LINE_LENGTH    70
#define MESSAGE_HEADER     "$"
#define MESSAGE_FOOTER     "$"
#define DELIMITER_CHAR     '$'
#define NODE_ADDRESS_CHARS 23
#define SYSTEM_NAME_CHARS  40
#define LOCATION_CHARS     40*/

/******************************************************************************/
/*								  Data Types								  */
/******************************************************************************/

#ifndef tBool
typedef INT16 tBool;
#endif
#include "const.h"

#include "structs.h"

/******************************************************************************/
/*                         InterBBS API Functions                             */
/******************************************************************************/
tIBResult IBSend(tIBInfo *pInfo, char *pszDestNode, char *pBuffer,
                 INT16 nBufferSize);

tIBResult IBSendAll(tIBInfo *pInfo, char *pBuffer, INT16 nBufferSize);
//tIBResult IBGet(tIBInfo *pInfo, void *pBuffer, INT16 nMaxBufferSize);
tIBResult IBSendMail(tIBInfo *pInfo, ibbs_mail_type *ibmail);
tIBResult IBGetMail(tIBInfo *pInfo, ibbs_mail_type *ibmail);
//tIBResult IBGetR(tIBInfo *pInfo, void *pBuffer,INT16 *nBufferLen);
tIBResult IBReadConfig(tIBInfo *pInfo, const char *pszConfigFile);


/******************************************************************************/
/*							 Private Declarations							  */
/******************************************************************************/
#define ATTRIB_PRIVATE		0x0001
#define ATTRIB_CRASH		0x0002
#define ATTRIB_RECEIVED 	0x0004
#define ATTRIB_SENT 		0x0008
#define ATTRIB_FILE_ATTACH	0x0010
#define ATTRIB_IN_TRANSIT	0x0020
#define ATTRIB_ORPHAN		0x0040
#define ATTRIB_KILL_SENT	0x0080
#define ATTRIB_LOCAL		0x0100
#define ATTRIB_HOLD 		0x0200
#define ATTRIB_FILE_REQUEST 0x0800
#define ATTRIB_RECEIPT_REQ	0x1000
#define ATTRIB_IS_RECEIPT	0x2000
#define ATTRIB_AUDIT_REQ	0x4000
#define ATTRIB_FILE_UPDATE	0x8000

//tBool DirExists(const char *pszDirName);
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
INT16 GetMaximumEncodedLength(INT16 nUnEncodedLength);
void EncodeBuffer(char *pszDest, const char *pBuffer, INT16 nBufferSize);
//void DecodeBuffer(const char *pszSource, void *pDestBuffer, INT16 nBufferSize);
void EncodeMail(char *pszDest, ibbs_mail_type *ibmail);
void DecodeMail(const char *pcSource, ibbs_mail_type *ibmail);
//int DecodeBufferR(const char *pszSource, void *pDestBuffer);
void ConvertAddressToString(char *pszDest, const tFidoNode *pNode);
void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource);
tBool ProcessConfigFile(const char *pszFileName, INT16 nKeyWords, const char **papszKeyWord,
                        void (*pfCallBack)(INT16, char *, void *), void *pCallBackData);
void ProcessConfigLine(INT16 nKeyword, char *pszParameter, void *pCallbackData);

#endif
