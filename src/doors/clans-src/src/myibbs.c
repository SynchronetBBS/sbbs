
/*
 * This code is taken from the OpenDoors sample IBBS code by Brian Pirie.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "platform.h"

#include "defines.h"
#include "ibbs.h"
#include "interbbs.h"
#include "myibbs.h"
#include "myopen.h"
#include "readcfg.h"
#include "structs.h"
#include "system.h"
#include "video.h"

static void MakeFilename(const char *pszPath, const char *pszFilename, char *pszOut, size_t sz);
static tIBResult ValidateInfoStruct(tIBInfo *pInfo);
static tBool CreateMessage(char *pszMessageDir, tMessageHeader *pHeader,
					char *pszText);
static uint32_t GetFirstUnusedMsgNum(char *pszMessageDir);
static void GetMessageFilename(char *pszMessageDir, uint32_t lwMessageNum,
						char *pszOut, size_t sz);
static tBool WriteMessage(char *pszMessageDir, uint32_t lwMessageNum,
				   tMessageHeader *pHeader, char *pszText);
static uint32_t GetNextMSGID(void);
void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource);

static void MakeFilename(const char *pszPath, const char *pszFilename, char *pszOut, size_t sz)
{
	/* Validate parameters in debug mode */
	assert(pszPath != NULL);
	assert(pszFilename != NULL);
	assert(pszOut != NULL);
	assert(pszPath != pszOut);
	assert(pszFilename != pszOut);

	/* Copy path to output filename */
	strlcpy(pszOut, pszPath, sz);

	/* Ensure there is a trailing backslash */
	if (pszOut[strlen(pszOut) - 1] != '\\' && pszOut[strlen(pszOut) - 1] != '/') {
		strlcat(pszOut, "/", sz);
	}

	/* Append base filename */
	strlcat(pszOut, pszFilename, sz);
}

static uint32_t GetNextMSGID(void)
{
	/* MSGID should be unique for every message, for as long as possible.   */
	/* This technique adds the current time, in seconds since midnight on   */
	/* January 1st, 1970 to a psuedo-random number. The random generator    */
	/* is not seeded, as the application may have already seeded it for its */
	/* own purposes. Even if not seeded, the inclusion of the current time  */
	/* will cause the MSGID to almost always be different.                  */
	return((uint32_t)time(NULL) + (uint32_t)rand());
}


static tBool CreateMessage(char *pszMessageDir, tMessageHeader *pHeader,
					char *pszText)
{
	uint32_t lwNewMsgNum;

	/* Get new message number */
	lwNewMsgNum = GetFirstUnusedMsgNum(pszMessageDir);

	/* Use WriteMessage() to create new message */
	return(WriteMessage(pszMessageDir, lwNewMsgNum, pHeader, pszText));
}


static void GetMessageFilename(char *pszMessageDir, uint32_t lwMessageNum,
						char *pszOut, size_t sz)
{
	char szFileName[FILENAME_CHARS + 1];

	snprintf(szFileName, sizeof(szFileName), "%" PRId32 ".msg", (uint32_t)lwMessageNum);
	MakeFilename(pszMessageDir, szFileName, pszOut, sz);
}


static tBool WriteMessage(char *pszMessageDir, uint32_t lwMessageNum,
				   tMessageHeader *pHeader, char *pszText)
{
	char szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	FILE *hFile;
	size_t nTextSize;
	char hbuf[BUF_SIZE_MessageHeader];

	do {
		/* Get fully qualified filename of message to write */
		GetMessageFilename(pszMessageDir, lwMessageNum, szFileName, sizeof(szFileName));

		/* Open message file */
		hFile = _fsopen(szFileName, "wbx", _SH_DENYRW);

		/* If open failed, return false */
		if (!hFile) {
			if (errno == EEXIST)
				lwMessageNum++;
			else
				return(false);
		}
	} while (!hFile);

	/* Attempt to write header */
	s_MessageHeader_s(pHeader, hbuf, sizeof(hbuf));
	if (fwrite(hbuf, sizeof(hbuf), 1, hFile) != 1) {
		/* On failure, close file, erase file, and return false */
		fclose(hFile);
		unlink(szFileName);
		return(false);
	}

	/* Determine size of message text, including string terminator */
	nTextSize = strlen(pszText) + 1;

	/* Attempt to write message text */
	if (fwrite(pszText, nTextSize, 1, hFile) != 1) {
		/* On failure, close file, erase file, and return false */
		fclose(hFile);
		unlink(szFileName);
		return(false);
	}

	/* Close message file */
	fclose(hFile);

	/* Return with success */
	return(true);
}

int
ptrcmp(const void *a, const void *b)
{
	const uintptr_t *p1 = a;
	const uintptr_t *p2 = b;

	if (*p1 < *p2)
		return -1;
	if (*p2 < *p1)
		return 0;
	return 0;
}

static uint32_t GetFirstUnusedMsgNum(char *pszMessageDir)
{
	bool err;
	char **fnames = FilesOrderedByDate(pszMessageDir, "*.msg", &err);
	if (err)
		System_Error("Failed to get list of MSG files");
	if (fnames == NULL)
		return 1;
	// Convert to an array of uintptr_ts...
	size_t cnt = 0;
	for (char **fp = fnames; *fp; fp++) {
		const char *fn = FileName(*fp);
		cnt++;
		if ((fn)[0] < '0' || (fn)[0] > '9') {
			free(*fp);
			*fp = 0;
		}
		else {
			char *endptr;
			errno = 0;
			unsigned long val = strtoul(fn, &endptr, 10);
			if (errno || strcasecmp(endptr, ".msg") != 0) {
				free(*fp);
				*fp = 0;
			}
			else {
				free(*fp);
				*fp = (char *)((intptr_t)val);
			}
		}
	}
	qsort(fnames, cnt, sizeof(char *), ptrcmp);
	if (fnames[0] > (char *)1) {
		free(fnames);
		return 1;
	}
	for (size_t i = 0; i < cnt; i++) {
		if (fnames[i + 1] > fnames[i] + 1) {
			uint32_t ret = (uint32_t)((uintptr_t)fnames[i] + 1);
			free(fnames);
			return ret;
		}
	}
	uint32_t ret = (uint32_t)((uintptr_t)fnames[cnt - 1] + 1);
	free(fnames);
	return ret;
}


static tIBResult ValidateInfoStruct(tIBInfo *pInfo)
{
	if (pInfo == NULL) return(eBadParameter);

	if (!DirExists(pInfo->szNetmailDir)) return(eMissingDir);

	if (strlen(pInfo->szProgName) == 0) return(eBadParameter);

	return(eSuccess);
}

void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource)
{
	uint16_t zone = 0, net = 0, node = 0, pt = 0;

	sscanf(pszSource, "%hu:%hu/%hu.%hu", &zone, &net
		,&node, &pt);
	pNode->wZone = zone;
	pNode->wNet = net;
	pNode->wNode = node;
	pNode->wPoint = pt;
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

tIBResult IBSendFileAttach(tIBInfo *pInfo, char *pszDestNode, const char *pszFileName)
{
	tIBResult ToReturn;
	tMessageHeader MessageHeader = {0};
	time_t lnSecondsSince1970;
	struct tm *pTimeInfo;
	char szTOPT[13];
	char szFMPT[13];
	char szINTL[43];
	char szMSGID[42];
	size_t nKludgeSize;
	size_t nTextSize;
	size_t iTemp;
	char *pszMessageText;
	tFidoNode DestNode;
	tFidoNode OrigNode;
	char szFileNameNoPath[72];

	if (Config.MailerType == MAIL_NONE)
		return eSuccess;
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
	strlcpy(MessageHeader.szFromUserName, pInfo->szProgName, sizeof(MessageHeader.szFromUserName));
	strlcpy(MessageHeader.szToUserName, pInfo->szProgName, sizeof(MessageHeader.szToUserName));
	// strlcpy(MessageHeader.szSubject, MESSAGE_SUBJECT, sizeof(MessageHeader.szSubject));

	/* file attach addition */

	/* figure out filename if no path in it */
	strlcpy(szFileNameNoPath, pszFileName, sizeof(szFileNameNoPath));

	if (Config.MailerType == MAIL_BINKLEY) {
		MessageHeader.szSubject[0] = '^';    /* delete file when sent */
		strlcpy(&MessageHeader.szSubject[1], szFileNameNoPath, sizeof(MessageHeader.szSubject) - 1);
	}
	else
		strlcpy(MessageHeader.szSubject, szFileNameNoPath, sizeof(MessageHeader.szSubject));

	/* Construct date and time information */
	lnSecondsSince1970 = time(NULL);
	pTimeInfo = localtime(&lnSecondsSince1970);
	snprintf(MessageHeader.szDateTime, sizeof(MessageHeader.szDateTime), "%2.2d %s %2.2d  %2.2d:%2.2d:%2.2d",
			pTimeInfo->tm_mday,
			aszShortMonthName[pTimeInfo->tm_mon],
			pTimeInfo->tm_year % 100,
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
		snprintf(szTOPT, sizeof(szTOPT), "\1TOPT %u\r", DestNode.wPoint);
	}
	else {
		strlcpy(szTOPT, "", sizeof(szTOPT));
	}

	/* Create FMPT kludge line if origin point is non-zero */
	if (OrigNode.wPoint != 0) {
		snprintf(szFMPT, sizeof(szFMPT), "\1FMPT %u\r", OrigNode.wPoint);
	}
	else {
		strlcpy(szFMPT, "", sizeof(szFMPT));
	}

	/* Create INTL kludge line if origin and destination zone addresses differ */
	// FIXME: always use intl?
	if (DestNode.wZone != OrigNode.wZone || true) {
		snprintf(szINTL, sizeof(szINTL), "\1INTL %u:%u/%u %u:%u/%u\r",
				DestNode.wZone,
				DestNode.wNet,
				DestNode.wNode,
				OrigNode.wZone,
				OrigNode.wNet,
				OrigNode.wNode);
	}
	else {
		strlcpy(szINTL, "", sizeof(szINTL));
	}

	/* Create MSGID kludge line, including point if non-zero */
	if (OrigNode.wPoint != 0) {
		snprintf(szMSGID, sizeof(szMSGID), "\1MSGID: %u:%u/%u.%u %" PRIx32 "\r",
				OrigNode.wZone,
				OrigNode.wNet,
				OrigNode.wNode,
				OrigNode.wPoint,
				(uint32_t)GetNextMSGID());
	}
	else {
		snprintf(szMSGID, sizeof(szMSGID), "\1MSGID: %u:%u/%u %" PRIx32 "\r",
				OrigNode.wZone,
				OrigNode.wNet,
				OrigNode.wNode,
				(uint32_t)GetNextMSGID());
	}

	/* Determine total size of kludge lines */
	nKludgeSize = strlen(szTOPT)
				  + strlen(szFMPT)
				  + strlen(szINTL)
				  + strlen(szMSGID)
				  + strlen(MESSAGE_PID);

	/* Determine total size of message text */
	nTextSize = nKludgeSize + 1;

	if (Config.MailerType != MAIL_BINKLEY)
		nTextSize += (strlen("FLAGS KFS")+1);

	/* Attempt to allocate space for message text */
	if ((pszMessageText = malloc(nTextSize)) == NULL) {
		return(eNoMemory);
	}

	/* Construct message text */
	strlcpy(pszMessageText, szTOPT, nTextSize);
	strlcat(pszMessageText, szFMPT, nTextSize);
	strlcat(pszMessageText, szINTL, nTextSize);
	strlcat(pszMessageText, szMSGID, nTextSize);
	strlcat(pszMessageText, MESSAGE_PID, nTextSize);
	if (Config.MailerType != MAIL_BINKLEY) {
		iTemp = strlen(pszMessageText);
		strlcpy(pszMessageText+iTemp, "FLAGS KFS", nTextSize - iTemp);
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
