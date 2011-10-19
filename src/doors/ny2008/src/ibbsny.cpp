//
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <OpenDoor.h>	/* Must be before the various wrappers */

#include <dirwrap.h>
#include <genwrap.h>

#include "ny2008.h"
//
#include "ibbsny.h"
extern unsigned _stklen;

INT16             ibbsi = FALSE;

INT16             ibbsi_operator = FALSE;

INT16             ibbsi_game_num = 0;

char            bbs_namei[37];


char            aszShortMonthName[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};


/*
   tBool DirExists(const char *pszDirName) { char szDirFileName[PATH_CHARS +
   1]; struct ffblk DirEntry; //   if(pszDirName==NULL) return(FALSE); //
   if(strlen(pszDirName)>PATH_CHARS) return(FALSE); assert(pszDirName !=
   NULL); assert(strlen(pszDirName) <= PATH_CHARS); strcpy(szDirFileName,
   pszDirName); /* Remove any trailing backslash from directory name -/
   en(szDirFileName) - 1] == '\\') { szDirFileName[strlen(szDirFileName) - 1]
   = '\0'; } /* Return true iff file exists and it is a directory -/
   rFileName, &DirEntry, FA_ARCH|FA_DIREC) == 0 && (DirEntry.ff_attrib &
   FA_DIREC)); }
*/

void            MakeFilename(const char *pszPath, const char *pszFilename, char *pszOut) {
	/* Validate parameters in debug mode */
	//assert(pszPath != NULL);
	//assert(pszFilename != NULL);
	//assert(pszOut != NULL);
	//assert(pszPath != pszOut);
	//assert(pszFilename != pszOut);

	/* Copy path to output filename */
	strcpy(pszOut, pszPath);

	/* Ensure there is a trailing backslash */
	if (pszOut[strlen(pszOut) - 1] != '\\') {
		strcat(pszOut, "\\");
	}
	/* Append base filename */
	strcat(pszOut, pszFilename);
}

tIBResult       IBSendAll(tIBInfo * pInfo, char *pBuffer, INT16 nBufferSize) {
	tIBResult       ToReturn;
	INT16             iCurrentSystem;

	if (pBuffer == NULL)
		return (eBadParameter);

	/* Validate information structure */
	ToReturn = ValidateInfoStruct(pInfo);
	if (ToReturn != eSuccess)
		return (ToReturn);

	if (pInfo->paOtherSystem == NULL && pInfo->nTotalSystems != 0) {
		return (eBadParameter);
	}
	/* Loop for each system in other systems array */
	for (iCurrentSystem = 0; iCurrentSystem < pInfo->nTotalSystems;
	        ++iCurrentSystem) {
		/* Send information to that system */
		if (strcmp(pInfo->paOtherSystem[iCurrentSystem].szAddress, pInfo->szThisNodeAddress) != 0) {
			ToReturn = IBSend(pInfo, pInfo->paOtherSystem[iCurrentSystem].szAddress,
			                  pBuffer, nBufferSize);
			if (ToReturn != eSuccess)
				return (ToReturn);
		}
	}
	return (eSuccess);
}

tIBResult       IBSend(tIBInfo * pInfo, char *pszDestNode, char *pBuffer,
                       INT16 nBufferSize) {
	tIBResult       ToReturn;
	tMessageHeader  MessageHeader;
	time_t          lnSecondsSince1970;
	struct tm      *pTimeInfo;

	char            szTOPT[13];

	char            szFMPT[13];

	char            szINTL[43];

	char            szMSGID[42];

	INT16             nKludgeSize;

	INT16             nTextSize;

	char           *pszMessageText;

	tFidoNode       DestNode;
	tFidoNode       OrigNode;

	if (pszDestNode == NULL)
		return (eBadParameter);
	if (pBuffer == NULL)
		return (eBadParameter);

	/* Validate information structure */
	ToReturn = ValidateInfoStruct(pInfo);
	if (ToReturn != eSuccess)
		return (ToReturn);

	/* Get destination node address from string */
	ConvertStringToAddress(&DestNode, pszDestNode);

	/* Get origin address from string */
	ConvertStringToAddress(&OrigNode, pInfo->szThisNodeAddress);

	/* Construct message header */
	/* Construct to, from and subject information */
	strcpy(MessageHeader.szFromUserName, pInfo->szProgName);
	strcpy(MessageHeader.szToUserName, "@NY");
	strcat(MessageHeader.szToUserName, pszDestNode);
	strcpy(MessageHeader.szSubject, MESSAGE_SUBJECT);

	/* Construct date and time information */
	lnSecondsSince1970 = time(NULL);
	pTimeInfo = localtime(&lnSecondsSince1970);
	sprintf(MessageHeader.szDateTime, "%02.2d %s %02.2d  %02.2d:%02.2d:%02.2d",
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
	if (pInfo->bCrash)
		MessageHeader.wAttribute |= ATTRIB_CRASH;
	if (pInfo->bHold)
		MessageHeader.wAttribute |= ATTRIB_HOLD;
	if (pInfo->bEraseOnSend)
		MessageHeader.wAttribute |= ATTRIB_KILL_SENT;

	/* Create message control (kludge) lines */
	/* Create TOPT kludge line if destination point is non-zero */
	if (DestNode.wPoint != 0) {
		sprintf(szTOPT, "\1TOPT %u\r", DestNode.wPoint);
	} else {
		strcpy(szTOPT, "");
	}
	/* Create FMPT kludge line if origin point is non-zero */
	if (OrigNode.wPoint != 0) {
		sprintf(szFMPT, "\1FMPT %u\r", OrigNode.wPoint);
	} else {
		strcpy(szFMPT, "");
	}

	/*
	   Create INTL kludge line if origin and destination zone addresses
	   differ
	*/
	if (DestNode.wZone != OrigNode.wZone) {
		sprintf(szINTL, "\1INTL %u:%u/%u %u:%u/%u\r",
		        DestNode.wZone,
		        DestNode.wNet,
		        DestNode.wNode,
		        OrigNode.wZone,
		        OrigNode.wNet,
		        OrigNode.wNode);
	} else {
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
	} else {
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
	nTextSize = GetMaximumEncodedLength(nBufferSize)
	            + strlen(MESSAGE_HEADER)
	            + nKludgeSize
	            + strlen(MESSAGE_FOOTER)
	            + 1;

	/* Attempt to allocate space for message text */
	if ((pszMessageText = (char *)malloc(nTextSize)) == NULL) {
		return (eNoMemory);
	}
	/* Construct message text */
	strcpy(pszMessageText, szTOPT);
	strcat(pszMessageText, szFMPT);
	strcat(pszMessageText, szINTL);
	strcat(pszMessageText, szMSGID);
	strcat(pszMessageText, MESSAGE_PID);
	strcat(pszMessageText, MESSAGE_HEADER);
	EncodeBuffer(pszMessageText + strlen(pszMessageText), pBuffer, nBufferSize);
	strcat(pszMessageText, MESSAGE_FOOTER);

	/* Attempt to send the message */
	if (CreateMessage(pInfo->szNetmailDir, &MessageHeader, pszMessageText)) {
		ToReturn = eSuccess;
	} else {
		ToReturn = eGeneralFailure;
	}
	/* Deallocate message text buffer */
	free(pszMessageText);

	/* Return appropriate value */
	return (ToReturn);
}

tIBResult       IBSendMail(tIBInfo * pInfo, ibbs_mail_type * ibmail) {
	tIBResult       ToReturn;
	tMessageHeader  MessageHeader;
	time_t          lnSecondsSince1970;
	struct tm      *pTimeInfo;

	char            szTOPT[13];

	char            szFMPT[13];

	char            szINTL[43];

	char            szMSGID[42];

	INT16             nKludgeSize;

	INT16             nTextSize;

	char           *pszMessageText;

	tFidoNode       DestNode;
	tFidoNode       OrigNode;

	if (ibmail->node_r == NULL)
		return (eBadParameter);
	if (ibmail == NULL)
		return (eBadParameter);

	/* Validate information structure */
	ToReturn = ValidateInfoStruct(pInfo);
	if (ToReturn != eSuccess)
		return (ToReturn);

	/* Get destination node address from string */
	ConvertStringToAddress(&DestNode, ibmail->node_r);

	/* Get origin address from string */
	ConvertStringToAddress(&OrigNode, pInfo->szThisNodeAddress);

	/* Construct message header */
	/* Construct to, from and subject information */
	strcpy(MessageHeader.szFromUserName, pInfo->szProgName);
	strcpy(MessageHeader.szToUserName, "@NY");
	strcat(MessageHeader.szToUserName, ibmail->node_r);
	strcpy(MessageHeader.szSubject, MESSAGE_SUBJECT);

	/* Construct date and time information */
	lnSecondsSince1970 = time(NULL);
	pTimeInfo = localtime(&lnSecondsSince1970);
	sprintf(MessageHeader.szDateTime, "%02.2d %s %02.2d  %02.2d:%02.2d:%02.2d",
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
	if (pInfo->bCrash)
		MessageHeader.wAttribute |= ATTRIB_CRASH;
	if (pInfo->bHold)
		MessageHeader.wAttribute |= ATTRIB_HOLD;
	if (pInfo->bEraseOnSend)
		MessageHeader.wAttribute |= ATTRIB_KILL_SENT;

	/* Create message control (kludge) lines */
	/* Create TOPT kludge line if destination point is non-zero */
	if (DestNode.wPoint != 0) {
		sprintf(szTOPT, "\1TOPT %u\r", DestNode.wPoint);
	} else {
		strcpy(szTOPT, "");
	}
	/* Create FMPT kludge line if origin point is non-zero */
	if (OrigNode.wPoint != 0) {
		sprintf(szFMPT, "\1FMPT %u\r", OrigNode.wPoint);
	} else {
		strcpy(szFMPT, "");
	}

	/*
	   Create INTL kludge line if origin and destination zone addresses
	   differ
	*/
	if (DestNode.wZone != OrigNode.wZone) {
		sprintf(szINTL, "\1INTL %u:%u/%u %u:%u/%u\r",
		        DestNode.wZone,
		        DestNode.wNet,
		        DestNode.wNode,
		        OrigNode.wZone,
		        OrigNode.wNet,
		        OrigNode.wNode);
	} else {
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
	} else {
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
	nTextSize = sizeof(ibbs_mail_type) * 2 + 4 * 20 + 1
	            + strlen(MESSAGE_HEADER)
	            + nKludgeSize
	            + strlen(MESSAGE_FOOTER)
	            + 1;

	/* Attempt to allocate space for message text */
	if ((pszMessageText = (char *)malloc(nTextSize)) == NULL) {
		return (eNoMemory);
	}
	/* Construct message text */
	strcpy(pszMessageText, szTOPT);
	strcat(pszMessageText, szFMPT);
	strcat(pszMessageText, szINTL);
	strcat(pszMessageText, szMSGID);
	strcat(pszMessageText, MESSAGE_PID);
	strcat(pszMessageText, MESSAGE_HEADER);

	EncodeMail(pszMessageText + strlen(pszMessageText), ibmail);
	//EncodeBuffer(pszMessageText + strlen(pszMessageText), pBuffer, nBufferSize);
	strcat(pszMessageText, MESSAGE_FOOTER);

	/* Attempt to send the message */
	if (CreateMessage(pInfo->szNetmailDir, &MessageHeader, pszMessageText)) {
		ToReturn = eSuccess;
	} else {
		ToReturn = eGeneralFailure;
	}

	/* Deallocate message text buffer */
	free(pszMessageText);

	/* Return appropriate value */
	return (ToReturn);
}


INT16             GetMaximumEncodedLength(INT16 nUnEncodedLength) {
	INT16             nEncodedLength;

	/* The current encoding algorithm uses two characters to represent   */
	/* each byte of data, plus 1 byte per MAX_LINE_LENGTH characters for */
	/* the carriage return character.                                    */
	nEncodedLength = nUnEncodedLength * 2;

	return (nEncodedLength + (nEncodedLength / MAX_LINE_LENGTH - 1) + 1);
}

void            EncodeBuffer(char *pszDest, const char *pBuffer, INT16 nBufferSize) {
	INT16             iSourceLocation;

	INT16             nOutputChars = 0;

	char           *pcDest = pszDest;

	const char     *pcSource = (char *)pBuffer;

	/* Loop for each byte of the source buffer */
	for (iSourceLocation = 0; iSourceLocation < nBufferSize; ++iSourceLocation) {
		/* First character contains bits 0 - 5, with 01 in upper two bits */
		*pcDest++ = (*pcSource & 0x3f) | 0x40;
		/* Second character contains bits 6 & 7 in positions 4 & 5. Upper */
		/* two bits are 01, and all remaining bits are 0. */
		*pcDest++ = ((*pcSource & 0xc0) >> 2) | 0x40;

		/* Output carriage return when needed */
		if ((nOutputChars += 2) >= MAX_LINE_LENGTH - 1) {
			nOutputChars = 0;
			*pcDest++ = '\r';
		}
		/* Increment source pointer */
		++pcSource;
	}
	/* Add one last carriage return, regardless of what has come before */
	*pcDest++ = '\r';

	/* Terminate output string */
	*pcDest++ = '\0';
}

void            EncodeMail(char *pszDest, ibbs_mail_type * ibmail) {
	char           *pcDest = pszDest;

	INT16             x, l, ln;

	char            temp[25];

	l = strlen(ibmail->sender);
	for (x = 0; x < l; x++) {
		sprintf(pcDest, "%02X", (INT16)ibmail->sender[x]);
		pcDest += 2;
	}
	*pcDest = '\n';
	pcDest++;

	l = strlen(ibmail->senderI);
	for (x = 0; x < l; x++) {
		sprintf(pcDest, "%02X", (INT16)ibmail->senderI[x]);
		pcDest += 2;
	}
	*pcDest = '\n';
	pcDest++;

	sprintf(pcDest, "%X", (INT16)ibmail->sender_sex);
	pcDest += strlen(pcDest);

	*pcDest = '\n';
	pcDest++;

	l = strlen(ibmail->node_s);
	for (x = 0; x < l; x++) {
		sprintf(pcDest, "%02X", (INT16)ibmail->node_s[x]);
		pcDest += 2;
	}
	*pcDest = '\n';
	pcDest++;

	l = strlen(ibmail->node_r);
	for (x = 0; x < l; x++) {
		sprintf(pcDest, "%02X", (INT16)ibmail->node_r[x]);
		pcDest += 2;
	}
	*pcDest = '\n';
	pcDest++;

	l = strlen(ibmail->recver);
	for (x = 0; x < l; x++) {
		sprintf(pcDest, "%02X", (INT16)ibmail->recver[x]);
		pcDest += 2;
	}
	*pcDest = '\n';
	pcDest++;

	l = strlen(ibmail->recverI);
	for (x = 0; x < l; x++) {
		sprintf(pcDest, "%02X", (INT16)ibmail->recverI[x]);
		pcDest += 2;
	}
	*pcDest = '\n';
	pcDest++;

	sprintf(pcDest, "%X", (INT16)ibmail->quote_length);
	pcDest += strlen(pcDest);

	*pcDest = '\n';
	pcDest++;

	sprintf(pcDest, "%X", (INT16)ibmail->length);
	pcDest += strlen(pcDest);

	*pcDest = '\n';
	pcDest++;

	sprintf(pcDest, "%X", (INT16)ibmail->flirt);
	pcDest += strlen(pcDest);

	*pcDest = '\n';
	pcDest++;

	sprintf(pcDest, "%X", (INT16)ibmail->ill);
	pcDest += strlen(pcDest);

	*pcDest = '\n';
	pcDest++;

	sprintf(pcDest, "%X", (INT16)ibmail->inf);
	pcDest += strlen(pcDest);

	*pcDest = '\n';
	pcDest++;

	for (ln = 0; ln < (ibmail->length + ibmail->quote_length) && ln < 20; ln++) {
		l = strlen(ibmail->lines[ln]);
		if (l == 0) {
			*pcDest = '0';
			pcDest++;
			*pcDest = '0';
			pcDest++;
		}
		for (x = 0; x < l && x < 20; x++) {
			sprintf(pcDest, "%02X", (INT16)ibmail->lines[ln][x]);
			pcDest += 2;
		}
		*pcDest = '\n';
		pcDest++;

		if (l <= 20) {
			*pcDest = '0';
			pcDest++;
			*pcDest = '0';
			pcDest++;
		}
		for (x = 20; x < l && x < 40; x++) {
			sprintf(pcDest, "%02X", (INT16)ibmail->lines[ln][x]);
			pcDest += 2;
		}
		*pcDest = '\n';
		pcDest++;

		if (l <= 40) {
			*pcDest = '0';
			pcDest++;
			*pcDest = '0';
			pcDest++;
		}
		for (x = 40; x < l && x < 60; x++) {
			sprintf(pcDest, "%02X", (INT16)ibmail->lines[ln][x]);
			pcDest += 2;
		}
		*pcDest = '\n';
		pcDest++;

		if (l <= 60) {
			*pcDest = '0';
			pcDest++;
			*pcDest = '0';
			pcDest++;
		}
		for (x = 60; x < l; x++) {
			sprintf(pcDest, "%02X", (INT16)ibmail->lines[ln][x]);
			pcDest += 2;
		}
		*pcDest = '\n';
		pcDest++;
	}
	*pcDest = '\0';
}

/*
   void DecodeBuffer(const char *pszSource, void *pDestBuffer, int
   nBufferSize) { const char *pcSource = pszSource; char *pcDest = (char
   )pDestBuffer; int iDestLocation; tBool bFirstOfByte = TRUE; /* Search for
   beginning of buffer delimiter char, returning if not found -/
   hile(*pcSource && *pcSource != DELIMITER_CHAR) ++pcSource; if(!*pcSource)
   return; /* Move pointer to first char after delimiter char -/ ++pcSource;
   /* Loop until destination buffer is full, delimiter char is encountered,
   -/ or end of source buffer is encountered -/ iDestLocation = 0; tLocation
   < nBufferSize && *pcSource && *pcSource != DELIMITER_CHAR) { If this is a
   valid data character -/ if(*pcSource >= 0x40 && *pcSource <= 0x7e) { /* If
   this is first character of byte -/ if(bFirstOfByte) { cDest = *pcSource &
   0x3f; /* Toggle bFirstOfByte -/ bFirstOfByte = FALSE; } else /*
   if(!bFirstOfByte) -/ { pcDest |= (*pcSource & 0x30) << 2; /* Increment
   destination -/ ++iDestLocation; ++pcDest; /* Toggle bFirstOfByte -/
   bFirstOfByte = TRUE; } } /* Increment source byte pointer -/ ++pcSource; }
   }
*/
void            DecodeMail(const char *pszSource, ibbs_mail_type * ibmail) {
	const char     *pcSource = pszSource;

	INT16             x, ln, t;

	char            tmp[80];

	//, key;

	//tmp[2] = 0;
	x = 0;

	//printf("1\n");
	//scanf("%c", &key);
	while (*pcSource && *pcSource != DELIMITER_CHAR)
		++pcSource;
	if (!(*pcSource))
		return;
	//printf("2\n");
	//scanf("%c", &key);
	pcSource++;

	//printf("3\n");
	//scanf("%c", &key);
	while (*pcSource <= ' ')
		pcSource++;

	//printf("4\n");
	//scanf("%c", &key);
	while (*pcSource > ' ') {
		tmp[0] = *pcSource;
		tmp[1] = *(pcSource + 1);
		tmp[2] = 0;
		sscanf(tmp, "%X", &t);
		ibmail->sender[x] = t;
		pcSource += 2;
		x++;
	}
	ibmail->sender[x] = 0;

	//printf("5\n");
	//scanf("%c", &key);
	while (*pcSource <= ' ')
		pcSource++;

	//tmp[2] = 0;
	x = 0;
	while (*pcSource > ' ') {
		tmp[0] = *pcSource;
		tmp[1] = *(pcSource + 1);
		tmp[2] = 0;
		sscanf(tmp, "%X", &t);
		ibmail->senderI[x] = t;
		pcSource += 2;
		x++;
	}
	//printf("6\n");
	//scanf("%c", &key);
	ibmail->senderI[x] = 0;

	while (*pcSource <= ' ')
		pcSource++;

	sscanf(pcSource, "%s", &tmp);
	pcSource += strlen(tmp);
	sscanf(tmp, "%X", &t);
	ibmail->sender_sex = (sex_type) t;

	while (*pcSource <= ' ')
		pcSource++;

	tmp[2] = 0;
	x = 0;
	while (*pcSource > ' ') {
		tmp[0] = *pcSource;
		tmp[1] = *(pcSource + 1);
		sscanf(tmp, "%X", &t);
		ibmail->node_s[x] = t;
		pcSource += 2;
		x++;
	}
	ibmail->node_s[x] = 0;
	//printf("[%s]", ibmail->node_s);
	//debug
	// printf("7\n");
	//scanf("%c", &key);

	while (*pcSource <= ' ')
		pcSource++;

	tmp[2] = 0;
	x = 0;
	while (*pcSource > ' ') {
		tmp[0] = *pcSource;
		tmp[1] = *(pcSource + 1);
		sscanf(tmp, "%X", &t);
		ibmail->node_r[x] = t;
		pcSource += 2;
		x++;
	}
	ibmail->node_r[x] = 0;
	//printf("[%s]", ibmail->node_r);
	//debug
	while (*pcSource <= ' ')
		pcSource++;

	tmp[2] = 0;
	x = 0;
	while (*pcSource > ' ') {
		tmp[0] = *pcSource;
		tmp[1] = *(pcSource + 1);
		sscanf(tmp, "%X", &t);
		ibmail->recver[x] = t;
		pcSource += 2;
		x++;
	}
	ibmail->recver[x] = 0;
	while (*pcSource <= ' ')
		pcSource++;

	tmp[2] = 0;
	x = 0;
	while (*pcSource > ' ') {
		tmp[0] = *pcSource;
		tmp[1] = *(pcSource + 1);
		sscanf(tmp, "%X", &t);
		ibmail->recverI[x] = t;
		pcSource += 2;
		x++;
	}
	ibmail->recverI[x] = 0;
	while (*pcSource <= ' ')
		pcSource++;

	sscanf(pcSource, "%s", &tmp);
	pcSource += strlen(tmp);
	sscanf(tmp, "%X", &t);
	ibmail->quote_length = t;
	//printf("[QL%d]", (INT16)ibmail->quote_length);
	//debug
	while (*pcSource <= ' ')
		pcSource++;

	sscanf(pcSource, "%s", &tmp);
	pcSource += strlen(tmp);
	sscanf(tmp, "%X", &t);
	ibmail->length = t;
	//printf("[L%d]", (INT16)ibmail->length);
	//debug
	while (*pcSource <= ' ')
		pcSource++;

	sscanf(pcSource, "%s", &tmp);
	pcSource += strlen(tmp);
	sscanf(tmp, "%X", &t);
	ibmail->flirt = t;
	//printf("[F%d][%s]", (INT16)ibmail->flirt, tmp);
	//debug
	while (*pcSource <= ' ')
		pcSource++;

	sscanf(pcSource, "%s", &tmp);
	pcSource += strlen(tmp);
	sscanf(tmp, "%X", &t);
	ibmail->ill = (desease) t;
	//printf("[D%d]", (INT16)ibmail->ill);
	//debug
	while (*pcSource <= ' ')
		pcSource++;

	sscanf(pcSource, "%s", &tmp);
	pcSource += strlen(tmp);
	sscanf(tmp, "%X", &t);
	ibmail->inf = t;
	//printf("[INF%d]\n", (INT16)ibmail->inf);
	//debug
	while (*pcSource <= ' ')
		pcSource++;

	//printf("8\n");
	//scanf("%c", &key);

	for (ln = 0; ln < (ibmail->length + ibmail->quote_length) && ln < 20; ln++) {
		tmp[2] = 0;
		x = 0;
		while (*pcSource > ' ') {
			tmp[0] = *pcSource;
			tmp[1] = *(pcSource + 1);
			sscanf(tmp, "%X", &t);
			(ibmail->lines)[ln][x] = t;
			pcSource += 2;
			x++;
			if (x > 80)
				break;
		}
		while (*pcSource <= ' ')
			pcSource++;

		while (*pcSource > ' ') {
			tmp[0] = *pcSource;
			tmp[1] = *(pcSource + 1);
			sscanf(tmp, "%X", &t);
			(ibmail->lines)[ln][x] = t;
			pcSource += 2;
			x++;
			if (x > 80)
				break;
		}
		while (*pcSource <= ' ')
			pcSource++;

		while (*pcSource > ' ') {
			tmp[0] = *pcSource;
			tmp[1] = *(pcSource + 1);
			sscanf(tmp, "%X", &t);
			(ibmail->lines)[ln][x] = t;
			pcSource += 2;
			x++;
			if (x > 80)
				break;
		}
		while (*pcSource <= ' ')
			pcSource++;

		while (*pcSource > ' ') {
			tmp[0] = *pcSource;
			tmp[1] = *(pcSource + 1);
			sscanf(tmp, "%X", &t);
			(ibmail->lines)[ln][x] = t;
			pcSource += 2;
			x++;
			if (x > 80)
				break;
		}
		(ibmail->lines)[ln][x] = 0;
		while (*pcSource <= ' ')
			pcSource++;
	}
	//printf("9\n");
	//scanf("%c", &key);
	//return;
}


/*
   INT16 DecodeBufferR(const char *pszSource, void *pDestBuffer) { const char
   pcSource = pszSource; char *pcDest = (char *)pDestBuffer; char size[2];
   INT16 iDestLocation; int nBufferSize=0; tBool bFirstOfByte = TRUE; /* Search
   for beginning of buffer delimiter char, returning if not found -/
   hile(*pcSource && *pcSource != DELIMITER_CHAR) ++pcSource; if(!*pcSource)
   return(0); /* Move pointer to first char after delimiter char -/
   ++pcSource; /* Loop until destination buffer is full, delimiter char is
   encountered, -/ or end of source buffer is encountered -/ iDestLocation =
   0; tLocation < (nBufferSize+2) && *pcSource && *pcSource !=
   DELIMITER_CHAR) /* If this is a valid data character -/ if(*pcSource >=
   0x40 && pcSource <= 0x7e) { /* If this is first character of byte -/
   fByte) { if(iDestLocation<2) size[iDestLocation] = *pcSource & 0x3f; se
   pcDest = *pcSource & 0x3f; /* Toggle bFirstOfByte -/ bFirstOfByte = FALSE;
   } else /* if(!bFirstOfByte) -/ { if(iDestLocation<2) size[iDestLocation]
   |= (*pcSource & 0x30) << 2; else { pcDest |= (*pcSource & 0x30) << 2;
   Dest; } /* Increment destination -/ ++iDestLocation; /* Toggle
   bFirstOfByte -/ bFirstOfByte = TRUE; } if(iDestLocation==2) {
   BufferSize=*(INT16 *)size; pDestBuffer=malloc(nBufferSize); } } /* Increment
   source byte pointer -/ ++pcSource; } return(nBufferSize); }
*/

DWORD           GetNextMSGID(void) {
	/* MSGID should be unique for every message, for as long as possible.   */
	/* This technique adds the current time, in seconds since midnight on   */
	/* January 1st, 1970 to a psuedo-random number. The random generator    */
	/* is not seeded, as the application may have already seeded it for its */
	/* own purposes. Even if not seeded, the inclusion of the current time  */
	/* will cause the MSGID to almost always be different.                  */
	return ((DWORD)time(NULL) + (DWORD)rand());
}

tBool           CreateMessage(char *pszMessageDir, tMessageHeader * pHeader,
                              char *pszText) {
	DWORD           lwNewMsgNum;

	/* Get new message number */
	lwNewMsgNum = GetFirstUnusedMsgNum(pszMessageDir);

	/* Use WriteMessage() to create new message */
	return (WriteMessage(pszMessageDir, lwNewMsgNum, pHeader, pszText));
}

void            GetMessageFilename(char *pszMessageDir, DWORD lwMessageNum,
                                   char *pszOut) {
	char            szFileName[FILENAME_CHARS + 1];

	sprintf(szFileName, "%ld.msg", lwMessageNum);
	MakeFilename(pszMessageDir, szFileName, pszOut);
}

tBool           WriteMessage(char *pszMessageDir, DWORD lwMessageNum,
                             tMessageHeader * pHeader, char *pszText) {
	char            szFileName[PATH_CHARS + FILENAME_CHARS + 2];

	INT16             hFile;

	size_t          nTextSize;

	/* Get fully qualified filename of message to write */
	GetMessageFilename(pszMessageDir, lwMessageNum, szFileName);

	/* Open message file */
	hFile = sopen(szFileName, O_WRONLY | O_BINARY | O_CREAT, SH_DENYRW,
	              S_IREAD | S_IWRITE);

	/* If open failed, return FALSE */
	if (hFile == -1)
		return (FALSE);

	/* Attempt to write header */
	if (write(hFile, pHeader, sizeof(tMessageHeader)) != sizeof(tMessageHeader)) {
		/* On failure, close file, erase file, and return FALSE */
		close(hFile);
		unlink(szFileName);
		return (FALSE);
	}
	/* Determine size of message text, including string terminator */
	nTextSize = strlen(pszText) + 1;

	/* Attempt to write message text */
	if (write(hFile, pszText, nTextSize) != nTextSize) {
		/* On failure, close file, erase file, and return FALSE */
		close(hFile);
		unlink(szFileName);
		return (FALSE);
	}
	/* Close message file */
	close(hFile);

	/* Return with success */
	return (TRUE);
}

tBool           ReadMessage(char *pszMessageDir, DWORD lwMessageNum,
                            tMessageHeader * pHeader, char **ppszText) {
	char            szFileName[PATH_CHARS + FILENAME_CHARS + 2];

	INT16             hFile;

	size_t          nTextSize;

	/* Get fully qualified filename of message to read */
	GetMessageFilename(pszMessageDir, lwMessageNum, szFileName);

	/* Open message file */
	hFile = sopen(szFileName, O_RDONLY | O_BINARY, SH_DENYWR);

	/* If open failed, return FALSE */
	if (hFile == -1)
		return (FALSE);

	/* Determine size of message body */
	nTextSize = (size_t) filelength(hFile) - sizeof(tMessageHeader);

	/* Attempt to allocate space for message body, plus character for added */
	/* string terminator.                                                   */
	if ((*ppszText = (char *)malloc(nTextSize + 1)) == NULL) {
		/* On failure, close file and return FALSE */
		close(hFile);
		return (FALSE);
	}
	/* Attempt to read header */
	if (read(hFile, pHeader, sizeof(tMessageHeader)) != sizeof(tMessageHeader)) {
		/* On failure, close file, deallocate message buffer and return FALSE */
		close(hFile);
		free(*ppszText);
		return (FALSE);
	}
	/* Attempt to read message text */
	if (read(hFile, *ppszText, nTextSize) != nTextSize) {
		/* On failure, close file, deallocate message buffer and return FALSE */
		close(hFile);
		free(*ppszText);
		return (FALSE);
	}
	/* Ensure that message buffer is NULL-terminated */
	(*ppszText)[nTextSize + 1] = '\0';

	/* Close message file */
	close(hFile);

	/* Return with success */
	return (TRUE);
}

DWORD           GetFirstUnusedMsgNum(char *pszMessageDir) {
	DWORD           lwHighestMsgNum = 0;

	DWORD           lwCurrentMsgNum;

	glob_t          DirEntry;
	char            szFileName[PATH_CHARS + FILENAME_CHARS + 2];

	char          **fname;

	MakeFilename(pszMessageDir, "*.msg", szFileName);

	if (glob(szFileName, GLOB_NOSORT, NULL, &DirEntry) == 0) {
		for (fname = DirEntry.gl_pathv; *fname != NULL; fname++) {
			lwCurrentMsgNum = atol(*fname);
			if (lwCurrentMsgNum > lwHighestMsgNum) {
				lwHighestMsgNum = lwCurrentMsgNum;
			}
		}
		globfree(&DirEntry);
	}
	return (lwHighestMsgNum + 1);
}

tIBResult       ValidateInfoStruct(tIBInfo * pInfo) {
	if (pInfo == NULL)
		return (eBadParameter);

	//if (!DirExists(pInfo->szNetmailDir))
	return (eMissingDir);

	if (strlen(pInfo->szProgName) == 0)
		return (eBadParameter);

	return (eSuccess);
}


/*
   tIBResult IBGet(tIBInfo *pInfo, void *pBuffer, INT16 nMaxBufferSize) { esult
   ToReturn; struct ffblk DirEntry; DWORD lwCurrentMsgNum; ader
   MessageHeader; char szFileName[PATH_CHARS + FILENAME_CHARS + 2]; r
   pszText; tFidoNode ThisNode,OtherNode; /* Validate information structure
   -/ ToReturn = ValidateInfoStruct(pInfo); f(ToReturn != eSuccess)
   return(ToReturn); /* Get this node's address from string -/
   ConvertStringToAddress(&ThisNode, pInfo->szThisNodeAddress);
   MakeFilename(pInfo->szNetmailDir, "*.msg", szFileName); /* Seach through
   each message file in the netmail directory, in no -/ /* particular order.
   -/ first(szFileName, &DirEntry, FA_ARCH) == 0) { do { lwCurrentMsgNum =
   atol(DirEntry.ff_name); /* If able to read message -/
   if(ReadMessage(pInfo->szNetmailDir, lwCurrentMsgNum, &MessageHeader,
   &pszText)) { /* od_printf("\n\rREAD THROUGH MESSAGE\n\r");
   der.szToUserName); od_printf("|%s|\n\r",pInfo->szProgName);
   :",MessageHeader.wDestZone); od_printf("%d/",MessageHeader.wDestNet);
   printf("%d.",MessageHeader.wDestNode); estPoint); od_get_answer("1");
*/

/*
   If message is for us, and hasn't be read yet -/
   herNode,&MessageHeader.szToUserName[3]);
   if(strcmp(MessageHeader.szFromUserName, pInfo->szProgName) == 0 &&
   (ThisNode.wZone == OtherNode.wZone || OtherNode.wZone==0 ||
   ThisNode.wZone==0) && ThisNode.wNet == OtherNode.wNet && ThisNode.wNode ==
   OtherNode.wNode && ThisNode.wPoint == OtherNode.wPoint &&
   !(MessageHeader.wAttribute & ATTRIB_RECEIVED)) { /*
   od_printf("\n\rUNREAD AND FOR US\n\r"); od_get_answer("1");
*/

/*
   Decode message text, placing information in buffer -/ , pBuffer,
   nMaxBufferSize); /* If received messages should be deleted -/
   if(pInfo->bEraseOnReceive) { Determine filename of message to erase -/
   tmailDir, lwCurrentMsgNum, szFileName); /* Attempt to erase file -/
   if(unlink(szFileName) == -1) { ToReturn = eGeneralFailure; } else {
   ToReturn = eSuccess; } } /* If received messages should not be deleted -/
   else /* if(!pInfo->bEraseOnReceive) -/ { /* Mark message as read -/
   r.wAttribute |= ATTRIB_RECEIVED; ++MessageHeader.wTimesRead; /* Attempt to
   rewrite message -/ if(!WriteMessage(pInfo->szNetmailDir, lwCurrentMsgNum,
   &MessageHeader, pszText)) { ToReturn = eGeneralFailure; } else { ToReturn
   = eSuccess; } } /* Deallocate message text buffer -/ free(pszText); /*
   Return appropriate value -/ return(ToReturn); } free(pszText); } }
   while(findnext(&DirEntry) == 0); } /* If no new messages were found -/
   return(eNoMoreMessages); }
*/
tIBResult       IBGetMail(tIBInfo * pInfo, ibbs_mail_type * ibmail) {
	tIBResult       ToReturn;
	glob_t          DirEntry;
	DWORD           lwCurrentMsgNum;

	tMessageHeader  MessageHeader;
	char            szFileName[PATH_CHARS + FILENAME_CHARS + 2];

	char           *pszText;

	//, key;
	tFidoNode       ThisNode, OtherNode;
	char          **fname;

	//printf("1\n");
	/* Validate information structure */
	ToReturn = ValidateInfoStruct(pInfo);
	//printf("2\n");
	if (ToReturn != eSuccess)
		return (ToReturn);
	//printf("3\n");

	/* Get this node's address from string */
	ConvertStringToAddress(&ThisNode, pInfo->szThisNodeAddress);
	//printf("4\n");

	MakeFilename(pInfo->szNetmailDir, "*.msg", szFileName);
	//printf("5\n");

	/* Search through each message file in the netmail directory, in no */
	/* particular order.                                                */
	//if (findfirst(szFileName, &DirEntry, FA_ARCH) == 0)
	if (glob(szFileName, 0, NULL, &DirEntry) == 0) {
		//printf("6\n");
		for (fname = DirEntry.gl_pathv; *fname != NULL; fname++) {
			lwCurrentMsgNum = atol(*fname);

			/* If able to read message */
			if (ReadMessage(pInfo->szNetmailDir, lwCurrentMsgNum, &MessageHeader,
			                &pszText)) {
				/* If message is for us, and hasn't be read yet */
				ConvertStringToAddress(&OtherNode, &MessageHeader.szToUserName[3]);
				//printf("7\n");

				if (stricmp(MessageHeader.szFromUserName, pInfo->szProgName) == 0
				        && (ThisNode.wZone == OtherNode.wZone || OtherNode.wZone == 0 || ThisNode.wZone == 0)
				        && ThisNode.wNet == OtherNode.wNet
				        && ThisNode.wNode == OtherNode.wNode
				        && ThisNode.wPoint == OtherNode.wPoint
				        && !(MessageHeader.wAttribute & ATTRIB_RECEIVED)) {
					/* Decode message text, placing information in buffer */
					//printf("8\n");
					DecodeMail(pszText, ibmail);
					//printf("*\n");
					//scanf("%c", &key);

					/* If received messages should be deleted */
					if (pInfo->bEraseOnReceive) {
						/* Determine filename of message to erase */
						GetMessageFilename(pInfo->szNetmailDir, lwCurrentMsgNum,
						                   szFileName);

						/* Attempt to erase file */
						if (unlink(szFileName) == -1) {
							ToReturn = eGeneralFailure;
						} else {
							ToReturn = eSuccess;
						}
					}
					/* If received messages should not be deleted */
					else {	/* if(!pInfo->bEraseOnReceive) */
						/* Mark message as read */
						MessageHeader.wAttribute |= ATTRIB_RECEIVED;
						++MessageHeader.wTimesRead;

						/* Attempt to rewrite message */
						if (!WriteMessage(pInfo->szNetmailDir, lwCurrentMsgNum,
						                  &MessageHeader, pszText)) {
							ToReturn = eGeneralFailure;
						} else {
							ToReturn = eSuccess;
						}
					}
					/* Deallocate message text buffer */
					free(pszText);

					/* Return appropriate value */
					globfree(&DirEntry);
					return (ToReturn);
				}
				free(pszText);
			}
		};
		globfree(&DirEntry);
	}
	//printf("10\n");
	/* If no new messages were found */
	return (eNoMoreMessages);

}


/*
   tIBResult IBGetR(tIBInfo *pInfo, void *pBuffer, INT16 *nBufferLen) { ult
   ToReturn; struct ffblk DirEntry; DWORD lwCurrentMsgNum; er MessageHeader;
   char szFileName[PATH_CHARS + FILENAME_CHARS + 2]; char pszText; tFidoNode
   ThisNode,OtherNode; /* Validate information structure -/ ToReturn =
   ValidateInfoStruct(pInfo); f(ToReturn != eSuccess) return(ToReturn); /*
   Get this node's address from string -/ ConvertStringToAddress(&ThisNode,
   pInfo->szThisNodeAddress); MakeFilename(pInfo->szNetmailDir, "*.msg",
   szFileName); /* Seach through each message file in the netmail directory,
   in no -/ /* particular order.
   -/ first(szFileName, &DirEntry, FA_ARCH) == 0) { do { lwCurrentMsgNum =
   atol(DirEntry.ff_name); /* If able to read message -/
   if(ReadMessage(pInfo->szNetmailDir, lwCurrentMsgNum, &MessageHeader,
   &pszText)) { herNode,&MessageHeader.szToUserName[3]);
   if(strcmp(MessageHeader.szFromUserName, pInfo->szProgName) == 0 &&
   (ThisNode.wZone == OtherNode.wZone || OtherNode.wZone==0 ||
   ThisNode.wZone==0) && ThisNode.wNet == OtherNode.wNet && ThisNode.wNode ==
   OtherNode.wNode && ThisNode.wPoint == OtherNode.wPoint &&
   !(MessageHeader.wAttribute & ATTRIB_RECEIVED)) { /* Decode message text,
   placing information in buffer -/ nBufferLen=DecodeBufferR(pszText,
   pBuffer); /* If received messages should be deleted -/
   if(pInfo->bEraseOnReceive) { Determine filename of message to erase -/
   tmailDir, lwCurrentMsgNum, szFileName); /* Attempt to erase file -/
   if(unlink(szFileName) == -1) { ToReturn = eGeneralFailure; } else {
   ToReturn = eSuccess; } } /* If received messages should not be deleted -/
   else /* if(!pInfo->bEraseOnReceive) -/ { /* Mark message as read -/
   r.wAttribute |= ATTRIB_RECEIVED; ++MessageHeader.wTimesRead; /* Attempt to
   rewrite message -/ if(!WriteMessage(pInfo->szNetmailDir, lwCurrentMsgNum,
   &MessageHeader, pszText)) { ToReturn = eGeneralFailure; } else { ToReturn
   = eSuccess; } } /* Deallocate message text buffer -/ free(pszText); /*
   Return appropriate value -/ return(ToReturn); } free(pszText); } }
   while(findnext(&DirEntry) == 0); } /* If no new messages were found -/
   return(eNoMoreMessages); }
*/

void            ConvertAddressToString(char *pszDest, const tFidoNode * pNode) {
	if (pNode->wZone == 0) {
		if (pNode->wPoint == 0) {
			sprintf(pszDest, "%u/%u", pNode->wNet, pNode->wNode);
		} else {
			sprintf(pszDest, "%u/%u.%u", pNode->wNet, pNode->wNode, pNode->wPoint);
		}
	} else {
		if (pNode->wPoint == 0) {
			sprintf(pszDest, "%u:%u/%u", pNode->wZone, pNode->wNet, pNode->wNode);
		} else {
			sprintf(pszDest, "%u:%u/%u.%u", pNode->wZone, pNode->wNet, pNode->wNode, pNode->wPoint);
		}
	}
}

void            ConvertStringToAddress(tFidoNode * pNode, const char *pszSource) {
	pNode->wZone = 0;
	pNode->wNet = 0;
	pNode->wNode = 0;
	pNode->wPoint = 0;
	if (strchr(pszSource, ':') == NULL) {
		sscanf(pszSource, "%u/%u.%u", &(pNode->wNet), &(pNode->wNode), &(pNode->wPoint));
	} else {
		sscanf(pszSource, "%u:%u/%u.%u", &(pNode->wZone), &(pNode->wNet), &(pNode->wNode), &(pNode->wPoint));
	}
}

#define NUM_KEYWORDS       14
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
#define KEYWORD_IBBS       10
#define KEYWORD_IBBS_OP    11
#define KEYWORD_IBBS_GN    12
#define KEYWORD_BBS_NAME   13

const char           *apszKeyWord[NUM_KEYWORDS] = {"SystemAddress",
        "UserName",
        "NetmailDir",
        "Crash",
        "Hold",
        "EraseOnSend",
        "EraseOnReceive",
        "LinkWith",
        "LinkName",
        "LinkLocation",
        "InterBBS",
        "InterBBSOperator",
        "InterBBSGameNumber",
        "SystemName"};

tIBResult       IBReadConfig(tIBInfo * pInfo, const char *pszConfigFile) {
	/* Set default values for pInfo settings */

	/*
	   pInfo->nTotalSystems = 0; pInfo->paOtherSystem = NULL;
	*/
	/* Process configuration file */
	if (!ProcessConfigFile(pszConfigFile, NUM_KEYWORDS, apszKeyWord,
	                       ProcessConfigLine, (void *)pInfo)) {
		return (eFileOpenError);
	}
	/* else */
	return (eSuccess);
}

void            ProcessConfigLine(INT16 nKeyword, char *pszParameter, void *pCallbackData) {
	tIBInfo        *pInfo = (tIBInfo *) pCallbackData;
	tOtherNode     *paNewNodeArray;

	switch (nKeyword) {
		case KEYWORD_IBBS:
			ibbsi = TRUE;
			break;

		case KEYWORD_IBBS_OP:
			ibbsi_operator = TRUE;
			break;

		case KEYWORD_IBBS_GN:
			sscanf(pszParameter, "%d", &ibbsi_game_num);
			break;

		case KEYWORD_BBS_NAME:
			strncpy(bbs_namei, pszParameter, 36);
			//printf("*|%s|", bbs_namei);
			break;

		case KEYWORD_ADDRESS:
			strncpy(pInfo->szThisNodeAddress, pszParameter, NODE_ADDRESS_CHARS);
			pInfo->szThisNodeAddress[NODE_ADDRESS_CHARS] = '\0';
			break;

		case KEYWORD_USER_NAME:
			strncpy(pInfo->szProgName, pszParameter, PROG_NAME_CHARS);
			pInfo->szProgName[PROG_NAME_CHARS] = '\0';
			break;

		case KEYWORD_MAIL_DIR:
			strncpy(pInfo->szNetmailDir, pszParameter, PATH_CHARS);
			pInfo->szNetmailDir[PATH_CHARS] = '\0';
			break;

		case KEYWORD_CRASH:
			if (stricmp(pszParameter, "Yes") == 0) {
				pInfo->bCrash = TRUE;
			} else if (stricmp(pszParameter, "No") == 0) {
				pInfo->bCrash = FALSE;
			}
			break;

		case KEYWORD_HOLD:
			if (stricmp(pszParameter, "Yes") == 0) {
				pInfo->bHold = TRUE;
			} else if (stricmp(pszParameter, "No") == 0) {
				pInfo->bHold = FALSE;
			}
			break;

		case KEYWORD_KILL_SENT:
			if (stricmp(pszParameter, "Yes") == 0) {
				pInfo->bEraseOnSend = TRUE;
			} else if (stricmp(pszParameter, "No") == 0) {
				pInfo->bEraseOnSend = FALSE;
			}
			break;

		case KEYWORD_KILL_RCVD:
			if (stricmp(pszParameter, "Yes") == 0) {
				pInfo->bEraseOnReceive = TRUE;
			} else if (stricmp(pszParameter, "No") == 0) {
				pInfo->bEraseOnReceive = FALSE;
			}
			break;

		case KEYWORD_LINK_WITH:
			if (pInfo->nTotalSystems == 0) {
				pInfo->paOtherSystem = (tOtherNode *) malloc(sizeof(tOtherNode));
				if (pInfo->paOtherSystem == NULL) {
					break;
				}
			} else {
				if ((paNewNodeArray = (tOtherNode *) malloc(sizeof(tOtherNode) *
				                      (pInfo->nTotalSystems + 1))) == NULL) {
					break;
				}
				memcpy(paNewNodeArray, pInfo->paOtherSystem, sizeof(tOtherNode) *
				       pInfo->nTotalSystems);

				free(pInfo->paOtherSystem);

				pInfo->paOtherSystem = paNewNodeArray;
			}
			strncpy(pInfo->paOtherSystem[pInfo->nTotalSystems].szAddress,
			        pszParameter, NODE_ADDRESS_CHARS);
			pInfo->paOtherSystem[pInfo->nTotalSystems].
			szAddress[NODE_ADDRESS_CHARS] = '\0';
			++pInfo->nTotalSystems;
			break;

		case KEYWORD_LINK_NAME:
			if (pInfo->nTotalSystems != 0) {
				strncpy(pInfo->paOtherSystem[pInfo->nTotalSystems - 1].szSystemName,
				        pszParameter, SYSTEM_NAME_CHARS);
				pInfo->paOtherSystem[pInfo->nTotalSystems - 1].
				szSystemName[SYSTEM_NAME_CHARS] = '\0';
			}
			break;

		case KEYWORD_LINK_LOC:
			if (pInfo->nTotalSystems != 0) {
				strncpy(pInfo->paOtherSystem[pInfo->nTotalSystems - 1].szLocation,
				        pszParameter, LOCATION_CHARS);
				pInfo->paOtherSystem[pInfo->nTotalSystems - 1].
				szLocation[LOCATION_CHARS] = '\0';
			}
			break;
	}
}

/* Configuration file reader settings */
#define CONFIG_LINE_SIZE 128
#define MAX_TOKEN_CHARS 32
tBool           ProcessConfigFile(const char *pszFileName, INT16 nKeyWords, const char **papszKeyWord,
                                  void (*pfCallBack) (INT16, char *, void *), void *pCallBackData) {
	FILE           *pfConfigFile;
	char            szConfigLine[CONFIG_LINE_SIZE + 1];

	char           *pcCurrentPos;

	WORD    uCount;

	char            szToken[MAX_TOKEN_CHARS + 1];

	INT16             iKeyWord;

	/* Attempt to open configuration file */
	if ((pfConfigFile = fopen(pszFileName, "rt")) == NULL) {
		return (FALSE);
	}
	/* While not at end of file */
	while (!feof(pfConfigFile)) {
		/* Get the next line */
		if (fgets(szConfigLine, CONFIG_LINE_SIZE + 1, pfConfigFile) == NULL)
			break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos = (char *)szConfigLine;
		while (*pcCurrentPos) {
			if (*pcCurrentPos == '\n' || *pcCurrentPos == '\r' || *pcCurrentPos == ';') {
				*pcCurrentPos = '\0';
				break;
			}
			++pcCurrentPos;
		}
		/* Search for beginning of first token on line */
		pcCurrentPos = (char *)szConfigLine;
		while (*pcCurrentPos && isspace(*pcCurrentPos))
			++pcCurrentPos;

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos)
			continue;

		/* Get first token from line */
		uCount = 0;
		while (*pcCurrentPos && !isspace(*pcCurrentPos)) {
			if (uCount < MAX_TOKEN_CHARS)
				szToken[uCount++] = *pcCurrentPos;
			++pcCurrentPos;
		}
		if (uCount <= MAX_TOKEN_CHARS)
			szToken[uCount] = '\0';
		else
			szToken[MAX_TOKEN_CHARS] = '\0';

		/* Find beginning of configuration option parameters */
		while (*pcCurrentPos && isspace(*pcCurrentPos))
			++pcCurrentPos;

		/* Trim trailing spaces from setting string */
		for (uCount = strlen(pcCurrentPos) - 1; uCount > 0; --uCount) {
			if (isspace(pcCurrentPos[uCount])) {
				pcCurrentPos[uCount] = '\0';
			} else {
				break;
			}
		}
		/* Loop through list of keywords */
		for (iKeyWord = 0; iKeyWord < nKeyWords; ++iKeyWord) {
			//printf("|%s|", papszKeyWord[iKeyWord]);
			//printf("|>%d=%d<|\n", iKeyWord, KEYWORD_BBS_NAME);

			/* If keyword matches */
			if (stricmp(szToken, papszKeyWord[iKeyWord]) == 0) {
				//printf("|%s|", szToken);
				//printf("|>%d=%d<|", iKeyWord, KEYWORD_BBS_NAME);
				/* Call keyword processing callback function */
				(*pfCallBack) (iKeyWord, pcCurrentPos, pCallBackData);
			}
		}
	}
	/* Close the configuration file */
	fclose(pfConfigFile);

	/* Return with success */
	return (TRUE);
}
