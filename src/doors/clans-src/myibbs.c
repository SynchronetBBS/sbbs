
/*
 * This code is taken from the OpenDoors sample IBBS code by Brian Pirie.
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef __unix__
#include "unix_wrappers.h"
#else
#ifndef _MSC_VER
# include <dir.h>
#else
# include <share.h>
#endif
#include <io.h>
#include <dos.h>
#endif
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "structs.h"
#include <OpenDoor.h>
#include "interbbs.h"
#include "video.h"

#define MAIL_OTHER      0
#define MAIL_BINKLEY    1


extern struct config *Config;



char aszShortMonthName[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
								 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
								};

void RidPath(char *pszFileName, char  *pszFileNameNoPath);

tBool DirExists(const char *pszDirName)
{
	char szDirFileName[PATH_CHARS + 1];
#if !defined(_WIN32) & !defined(__unix__)
	struct ffblk DirEntry;
#else
	struct stat file_stats;
#endif

	assert(pszDirName != NULL);
	assert(strlen(pszDirName) <= PATH_CHARS);

	strcpy(szDirFileName, pszDirName);

	/* Remove any trailing backslash from directory name */
	if (szDirFileName[strlen(szDirFileName) - 1] == '/' || szDirFileName[strlen(szDirFileName) - 1] == '\\') {
		szDirFileName[strlen(szDirFileName) - 1] = '\0';
	}

	/* Return true iff file exists and it is a directory */
#if !defined(_WIN32) & !defined(__unix__)
	return(findfirst(szDirFileName, &DirEntry, FA_ARCH|FA_DIREC) == 0 &&
		   (DirEntry.ff_attrib & FA_DIREC));
#else
	if (stat(szDirFileName, &file_stats) == 0)
		if (file_stats.st_mode & S_IFDIR)
			return TRUE;

	return FALSE;
#endif
}


void MakeFilename(const char *pszPath, const char *pszFilename, char *pszOut)
{
	/* Validate parameters in debug mode */
	assert(pszPath != NULL);
	assert(pszFilename != NULL);
	assert(pszOut != NULL);
	assert(pszPath != pszOut);
	assert(pszFilename != pszOut);

	/* Copy path to output filename */
	strcpy(pszOut, pszPath);

	/* Ensure there is a trailing backslash */
	if (pszOut[strlen(pszOut) - 1] != '\\' && pszOut[strlen(pszOut) - 1] != '/') {
		strcat(pszOut, "/");
	}

	/* Append base filename */
	strcat(pszOut, pszFilename);
}

_INT16 GetMaximumEncodedLength(_INT16 nUnEncodedLength)
{
	_INT16 nEncodedLength;

	/* The current encoding algorithm uses two characters to represent   */
	/* each byte of data, plus 1 byte per MAX_LINE_LENGTH characters for */
	/* the carriage return character.                                    */

	nEncodedLength = nUnEncodedLength * 2;

	return(nEncodedLength + (nEncodedLength / MAX_LINE_LENGTH - 1) + 1);
}


DWORD GetNextMSGID(void)
{
	/* MSGID should be unique for every message, for as long as possible.   */
	/* This technique adds the current time, in seconds since midnight on   */
	/* January 1st, 1970 to a psuedo-random number. The random generator    */
	/* is not seeded, as the application may have already seeded it for its */
	/* own purposes. Even if not seeded, the inclusion of the current time  */
	/* will cause the MSGID to almost always be different.                  */
	return((DWORD)time(NULL) + (DWORD)rand());
}


tBool CreateMessage(char *pszMessageDir, tMessageHeader *pHeader,
					char *pszText)
{
	DWORD lwNewMsgNum;

	/* Get new message number */
	lwNewMsgNum = GetFirstUnusedMsgNum(pszMessageDir);

	/* Use WriteMessage() to create new message */
	return(WriteMessage(pszMessageDir, lwNewMsgNum, pHeader, pszText));
}


void GetMessageFilename(char *pszMessageDir, DWORD lwMessageNum,
						char *pszOut)
{
	char szFileName[FILENAME_CHARS + 1];

	sprintf(szFileName, "%ld.msg", lwMessageNum);
	MakeFilename(pszMessageDir, szFileName, pszOut);
}


tBool WriteMessage(char *pszMessageDir, DWORD lwMessageNum,
				   tMessageHeader *pHeader, char *pszText)
{
	char szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	_INT16 hFile;
	size_t nTextSize;

	/* Get fully qualified filename of message to write */
	GetMessageFilename(pszMessageDir, lwMessageNum, szFileName);

	/* Open message file */
#ifdef __unix__
	hFile = open(szFileName, O_WRONLY|O_BINARY|O_CREAT,
				 S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
#elif defined(_WIN32)
	hFile = sopen(szFileName, O_WRONLY|O_BINARY|O_CREAT,SH_DENYRW,
				  S_IREAD|S_IWRITE);
#else
	hFile = open(szFileName, O_WRONLY|O_BINARY|O_CREAT|O_DENYALL,
				 S_IREAD|S_IWRITE);
#endif

	/* If open failed, return FALSE */
	if (hFile == -1) return(FALSE);

	/* Attempt to write header */
	if (write(hFile, pHeader, sizeof(tMessageHeader)) != sizeof(tMessageHeader)) {
		/* On failure, close file, erase file, and return FALSE */
		close(hFile);
		unlink(szFileName);
		return(FALSE);
	}

	/* Determine size of message text, including string terminator */
	nTextSize = strlen(pszText) + 1;
	// nTextSize = strlen(pszText);

	/* Attempt to write message text */
	if ((unsigned)write(hFile, pszText, nTextSize) != nTextSize) {
		/* On failure, close file, erase file, and return FALSE */
		close(hFile);
		unlink(szFileName);
		return(FALSE);
	}

	/* Close message file */
	close(hFile);

	/* Return with success */
	return(TRUE);
}


tBool ReadMessage(char *pszMessageDir, DWORD lwMessageNum,
				  tMessageHeader *pHeader, char **ppszText)
{
	char szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	_INT16 hFile;
	size_t nTextSize;

	/* Get fully qualified filename of message to read */
	GetMessageFilename(pszMessageDir, lwMessageNum, szFileName);

	DisplayStr("> ");
	DisplayStr(szFileName);
	DisplayStr("     \r");

	/* Open message file */
#ifdef __unix__
	hFile = open(szFileName, O_RDONLY|O_BINARY);
#elif defined(_WIN32)
	hFile = sopen(szFileName, O_RDONLY|O_BINARY,SH_DENYWR);
#else
	hFile = open(szFileName, O_RDONLY|O_BINARY|O_DENYWRITE);
#endif

	/* If open failed, return FALSE */
	if (hFile == -1) return(FALSE);

	/* Determine size of message body */
	nTextSize = (size_t)filelength(hFile) - sizeof(tMessageHeader);

	/* Attempt to allocate space for message body, plus character for added */
	/* string terminator.                                                   */
	if ((*ppszText = malloc(nTextSize + 1)) == NULL) {
		/* On failure, close file and return FALSE */
		close(hFile);
		return(FALSE);
	}

	/* Attempt to read header */
	if (read(hFile, pHeader, sizeof(tMessageHeader)) != sizeof(tMessageHeader)) {
		/* On failure, close file, deallocate message buffer and return FALSE */
		close(hFile);
		free(*ppszText);
		return(FALSE);
	}

	/* Attempt to read message text */
	if ((unsigned)read(hFile, *ppszText, nTextSize) != nTextSize) {
		/* On failure, close file, deallocate message buffer and return FALSE */
		close(hFile);
		free(*ppszText);
		return(FALSE);
	}

	/* Ensure that message buffer is NULL-terminated */
	(*ppszText)[nTextSize + 1] = '\0';

	/* Close message file */
	close(hFile);

	/* Return with success */
	return(TRUE);
}


DWORD GetFirstUnusedMsgNum(char *pszMessageDir)
{
	DWORD lwHighestMsgNum = 0;
	DWORD lwCurrentMsgNum;
#ifndef _WIN32
	struct ffblk DirEntry;
#else
	struct _finddata_t find_data;
	long find_handle;
#endif
	char szFileName[PATH_CHARS + FILENAME_CHARS + 2];

	MakeFilename(pszMessageDir, "*.msg", szFileName);

#ifndef _WIN32
	if (findfirst(szFileName, &DirEntry, FA_ARCH) == 0) {
		do {
			lwCurrentMsgNum = atol(DirEntry.ff_name);
			if (lwCurrentMsgNum > lwHighestMsgNum) {
				lwHighestMsgNum = lwCurrentMsgNum;
			}
		}
		while (findnext(&DirEntry) == 0);
	}
#else
	find_handle = _findfirst(szFileName, &find_data);
	if (find_handle) {
		do {
			lwCurrentMsgNum = atol(find_data.name);
			if (lwCurrentMsgNum > lwHighestMsgNum) {
				lwHighestMsgNum = lwCurrentMsgNum;
			}
		}
		while (_findnext(find_handle, &find_data) == 0);
		_findclose(find_handle);
	}
#endif

	return(lwHighestMsgNum + 1);
}


tIBResult ValidateInfoStruct(tIBInfo *pInfo)
{
	if (pInfo == NULL) return(eBadParameter);

	if (!DirExists(pInfo->szNetmailDir)) return(eMissingDir);

	if (strlen(pInfo->szProgName) == 0) return(eBadParameter);

	return(eSuccess);
}

void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource)
{
	pNode->wZone = 0;
	pNode->wNet = 0;
	pNode->wNode = 0;
	pNode->wPoint = 0;
	sscanf(pszSource, "%hu:%hu/%hu.%hu", &(pNode->wZone), &(pNode->wNet)
		,&(pNode->wNode), &(pNode->wPoint));
}


#define NUM_KEYWORDS       10

#define KEYWORD_ADDRESS    0
#define KEYWORD_USER_NAME  1
#define KEYWORD_MAIL_DIR   2
#define KEYWORD_CRASH      3
#define KEYWORD_HOLD       4
#define KEYWORD_KILL_SENT  5
#define KEYWORD_KILL_RCVD  6
#define KEYWORD_LINK_WITH  7
#define KEYWORD_LINK_NAME  8
#define KEYWORD_LINK_LOC   9

tIBResult IBSendFileAttach(tIBInfo *pInfo, char *pszDestNode, char *pszFileName)
{
	tIBResult ToReturn;
	tMessageHeader MessageHeader;
	time_t lnSecondsSince1970;
	struct tm *pTimeInfo;
	char szTOPT[13];
	char szFMPT[13];
	char szINTL[43];
	char szMSGID[42];
	_INT16 nKludgeSize;
	_INT16 nTextSize, iTemp;
	char *pszMessageText;
	tFidoNode DestNode;
	tFidoNode OrigNode;
	char szFileNameNoPath[50];

	if (pszDestNode == NULL) return(eBadParameter);
	if (pszFileName == NULL) return(eBadParameter);

	/* Validate information structure */
	ToReturn = ValidateInfoStruct(pInfo);
	if (ToReturn != eSuccess) return(ToReturn);

	/* Get destination node address from string */
	ConvertStringToAddress(&DestNode, pszDestNode);

	/* Get origin address from string */
	ConvertStringToAddress(&OrigNode, pInfo->szThisNodeAddress);

	/* Construct message header */
	/* Construct to, from and subject information */
	strncpy(MessageHeader.szFromUserName, pInfo->szProgName, 36);
	strncpy(MessageHeader.szToUserName, pInfo->szProgName, 36);
	// strcpy(MessageHeader.szSubject, MESSAGE_SUBJECT);

	/* file attach addition */

	/* figure out filename if no path in it */
	// FIXME: In future remove this code
//   RidPath(pszFileName, szFileNameNoPath);
	strcpy(szFileNameNoPath, pszFileName);

	if (Config->MailerType == MAIL_BINKLEY) {
		MessageHeader.szSubject[0] = '^';    /* delete file when sent */
		strncpy(&MessageHeader.szSubject[1], szFileNameNoPath, 72);
	}
	else
		strncpy(MessageHeader.szSubject, szFileNameNoPath, 72);

	/* Construct date and time information */
	lnSecondsSince1970 = time(NULL);
	pTimeInfo = localtime(&lnSecondsSince1970);
	sprintf(MessageHeader.szDateTime, "%2.2d %s %2.2d  %2.2d:%2.2d:%2.2d",
			pTimeInfo->tm_mday,
			aszShortMonthName[pTimeInfo->tm_mon],
			pTimeInfo->tm_year,
			pTimeInfo->tm_hour,
			pTimeInfo->tm_min,
			pTimeInfo->tm_sec);

	/* Construct misc. information */
	MessageHeader.wTimesRead = 0;
	MessageHeader.wCost = 0;
	MessageHeader.wReplyTo = 0;
	MessageHeader.wNextReply = 0;

	/* Construct destination address */
	MessageHeader.wDestZone = DestNode.wZone;
	MessageHeader.wDestNet = DestNode.wNet;
	MessageHeader.wDestNode = DestNode.wNode;
	MessageHeader.wDestPoint = DestNode.wPoint;

	/* Construct origin address */
	MessageHeader.wOrigZone = OrigNode.wZone;
	MessageHeader.wOrigNet = OrigNode.wNet;
	MessageHeader.wOrigNode = OrigNode.wNode;
	MessageHeader.wOrigPoint = OrigNode.wPoint;

	/* Construct message attributes */
	MessageHeader.wAttribute = ATTRIB_PRIVATE | ATTRIB_LOCAL;
	if (pInfo->bCrash) MessageHeader.wAttribute |= ATTRIB_CRASH;
	if (pInfo->bHold) MessageHeader.wAttribute |= ATTRIB_HOLD;
	if (pInfo->bEraseOnSend) MessageHeader.wAttribute |= ATTRIB_KILL_SENT;

	/* file attach additions */
	MessageHeader.wAttribute |= ATTRIB_FILE_ATTACH;

	/* Create message control (kludge) lines */
	/* Create TOPT kludge line if destination point is non-zero */
	if (DestNode.wPoint != 0) {
		sprintf(szTOPT, "\1TOPT %u\r", DestNode.wPoint);
	}
	else {
		strcpy(szTOPT, "");
	}

	/* Create FMPT kludge line if origin point is non-zero */
	if (OrigNode.wPoint != 0) {
		sprintf(szFMPT, "\1FMPT %u\r", OrigNode.wPoint);
	}
	else {
		strcpy(szFMPT, "");
	}

	/* Create INTL kludge line if origin and destination zone addresses differ */
	// FIXME: always use intl?
	if (DestNode.wZone != OrigNode.wZone || TRUE) {
		sprintf(szINTL, "\1INTL %u:%u/%u %u:%u/%u\r",
				DestNode.wZone,
				DestNode.wNet,
				DestNode.wNode,
				OrigNode.wZone,
				OrigNode.wNet,
				OrigNode.wNode);
	}
	else {
		strcpy(szINTL, "");
	}

	/* Create MSGID kludge line, including point if non-zero */
	if (OrigNode.wPoint != 0) {
		sprintf(szMSGID, "\1MSGID: %u:%u/%u.%u %lx\r",
				OrigNode.wZone,
				OrigNode.wNet,
				OrigNode.wNode,
				OrigNode.wPoint,
				GetNextMSGID());
	}
	else {
		sprintf(szMSGID, "\1MSGID: %u:%u/%u %lx\r",
				OrigNode.wZone,
				OrigNode.wNet,
				OrigNode.wNode,
				GetNextMSGID());
	}

	/* Determine total size of kludge lines */
	nKludgeSize = strlen(szTOPT)
				  + strlen(szFMPT)
				  + strlen(szINTL)
				  + strlen(szMSGID)
				  + strlen(MESSAGE_PID);

	/* Determine total size of message text */
	nTextSize = nKludgeSize + 1;

	if (Config->MailerType != MAIL_BINKLEY)
		nTextSize += (strlen("FLAGS KFS")+1);

	/* Attempt to allocate space for message text */
	if ((pszMessageText = malloc(nTextSize)) == NULL) {
		return(eNoMemory);
	}

	/* Construct message text */
	strcpy(pszMessageText, szTOPT);
	strcat(pszMessageText, szFMPT);
	strcat(pszMessageText, szINTL);
	strcat(pszMessageText, szMSGID);
	strcat(pszMessageText, MESSAGE_PID);
	if (Config->MailerType != MAIL_BINKLEY) {
		iTemp = strlen(pszMessageText);
		strcpy(pszMessageText+iTemp, "FLAGS KFS");
		/*      pszMessageText[ iTemp ] = '';
		      pszMessageText[ iTemp+1 ] = 'F';
		      pszMessageText[ iTemp+2 ] = 'L';
		      pszMessageText[ iTemp+3 ] = 'A';
		      pszMessageText[ iTemp+4 ] = 'G';
		      pszMessageText[ iTemp+5 ] = 'S';
		      pszMessageText[ iTemp+6 ] = ' ';
		      pszMessageText[ iTemp+7 ] = 'K';
		      pszMessageText[ iTemp+8 ] = 'F';
		      pszMessageText[ iTemp+9 ] = 'S';
		      pszMessageText[ iTemp+10 ] = '\0';    */
	}

	/* Attempt to send the message */
	if (CreateMessage(pInfo->szNetmailDir, &MessageHeader, pszMessageText)) {
		ToReturn = eSuccess;
	}
	else {
		ToReturn = eGeneralFailure;
	}

	/* Deallocate message text buffer */
	free(pszMessageText);

	/* Return appropriate value */
	return(ToReturn);
}

tIBResult IBGetFile(tIBInfo *pInfo, char *szAttachFile)
{
	tIBResult ToReturn;
#ifndef _WIN32
	struct ffblk DirEntry;
#else
	struct _finddata_t find_data;
	long find_handle;
#endif
	DWORD lwCurrentMsgNum;
	tMessageHeader MessageHeader;
	char szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	char *pszText;
	tFidoNode ThisNode;
	char szFileNameNoPath[50], szTemp[50];

	/* Validate information structure */
	ToReturn = ValidateInfoStruct(pInfo);
	if (ToReturn != eSuccess) return(ToReturn);

	/* Get this node's address from string */
	ConvertStringToAddress(&ThisNode, pInfo->szThisNodeAddress);

	MakeFilename(pInfo->szNetmailDir, "*.msg", szFileName);

	/* Seach through each message file in the netmail directory, in no */
	/* particular order.                                               */
#ifndef _WIN32
	if (findfirst(szFileName, &DirEntry, FA_ARCH) == 0)
#else
	find_handle = _findfirst(szFileName, &find_data);
	if (find_handle)
#endif
	{
		do {
#ifndef _WIN32
			lwCurrentMsgNum = atol(DirEntry.ff_name);
#else
			lwCurrentMsgNum = atol(find_data.name);
#endif

			/* If able to read message */
			if (ReadMessage(pInfo->szNetmailDir, lwCurrentMsgNum, &MessageHeader,
							&pszText)) {
				/* If message is for us, and hasn't be read yet */
				if (strcmp(MessageHeader.szToUserName, pInfo->szProgName) == 0
						&& ThisNode.wZone == MessageHeader.wDestZone
						&& ThisNode.wNet == MessageHeader.wDestNet
						&& ThisNode.wNode == MessageHeader.wDestNode
						&& ThisNode.wPoint == MessageHeader.wDestPoint
						&& !(MessageHeader.wAttribute & ATTRIB_RECEIVED)) {

					/* strcpy inbounddir to filename */
					/* strcat the subject of message -- actual filename */
					// FIXME: problems later on?!
					strcpy(szAttachFile, Config->szInboundDir);

					// remove the ^ and path if it is first char

					// problem here since in local league, we don't need the ^
					// char and since we "send" each other packets and receive them
					// with the same filename as we got them, in real life, a packet
					// would be sent and received with no path to open it, so we
					// assume we use the inbound path and open the file from that

					// remove ^ char
					if (MessageHeader.szSubject[0] == '^')
						strcat(szTemp, &MessageHeader.szSubject[1]);
					else
						strcpy(szTemp, MessageHeader.szSubject);

					// dude: remove path if it exists -- local leagues only
					RidPath(MessageHeader.szSubject, szFileNameNoPath);

					//printf("headersubject: %s\n", szTemp);
					//printf("filename nopath: %s\n", szFileNameNoPath);
					strcat(szAttachFile, szFileNameNoPath);

					//printf("inbound File found:  %s\n", szAttachFile);

					/* If received messages should be deleted */
					if (pInfo->bEraseOnReceive) {
						/* Determine filename of message to erase */
						GetMessageFilename(pInfo->szNetmailDir, lwCurrentMsgNum,
										   szFileName);

						/* Attempt to erase file */
						if (unlink(szFileName) == -1) {
							ToReturn = eGeneralFailure;
						}
						else {
							ToReturn = eSuccess;
						}
					}

					/* If received messages should not be deleted */
					else { /* if(!pInfo->bEraseOnReceive) */
						/* Mark message as read */
						MessageHeader.wAttribute |= ATTRIB_RECEIVED;
						++MessageHeader.wTimesRead;

						/* Attempt to rewrite message */
						if (!WriteMessage(pInfo->szNetmailDir, lwCurrentMsgNum,
										  &MessageHeader, pszText)) {
							ToReturn = eGeneralFailure;
						}
						else {
							ToReturn = eSuccess;
						}
					}

					/* Deallocate message text buffer */
					free(pszText);

					/* Return appropriate value */
					return(ToReturn);
				}
				free(pszText);
			}
#ifndef _WIN32
		}
		while (findnext(&DirEntry) == 0);
#else
		} while (_findnext(find_handle, &find_data) == 0);
		_findclose(find_handle);
#endif
	}

	/* If no new messages were found */
	return(eNoMoreMessages);
}

void RidPath(char *pszFileName, char  *pszFileNameNoPath)
{
	_INT16 CurChar;

	for (CurChar = strlen(pszFileName); CurChar > 0; CurChar--) {
		/* is this a '\' char?  If so, break; */
		if (pszFileName[CurChar] == '\\' || pszFileName[CurChar] == '/') {
			CurChar++;  // skip the \ char
			break;
		}
	}
	strcpy(pszFileNameNoPath, &pszFileName[CurChar]);
}
