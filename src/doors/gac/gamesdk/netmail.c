#include "genwrap.h"
#include "netmail.h"
//unsigned INT16 TestingBit;

#include <assert.h>
#include <ctype.h>
#include <string.h>
//#include <dir.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
//#include <io.h>
//#include <dos.h>
#include <fcntl.h>
//#include <alloc.h>
#include <sys/stat.h>

#include "datewrap.h"
#include "dirwrap.h"
#include "filewrap.h"


char aszShortMonthName[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
				 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// My addition
WORD IBBSRead;
char temphost[150], *nextbbs;
// 1/97 removed far keywords
INT16 DirExists(const char *pszDirName)
	 {
	 /* Return true iff file exists and it is a directory */
	 return(isdir(pszDirName));
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
	 backslash(pszOut);

	 /* Append base filename */
	 strcat(pszOut, pszFilename);
	 }


tIBResult IBSendAll(tIBInfo *pInfo, void *pBuffer, INT16 nBufferSize,
	INT16 thisbbsnum, INT16 bEncoded, INT16 bUseGigo)
	 {
	 tIBResult ToReturn;
	 INT16 iCurrentSystem;

	 if(pBuffer == NULL && bEncoded == TRUE) return(eBadParameter);

	 /* Validate information structure */
	 ToReturn = ValidateInfoStruct(pInfo);
	 if(ToReturn != eSuccess) return(ToReturn);

	 if(pInfo->paOtherSystem == NULL && pInfo->nTotalSystems != 0)
			{
			return(eBadParameter);
			}

	 /* Loop for each system in other systems array */
	 for(iCurrentSystem = 0; iCurrentSystem < pInfo->nTotalSystems;
		++iCurrentSystem)
			{

				// I added these lines (3) to skip the current system
//              if (pInfo->paOtherSystem[iCurrentSystem].szNumber != thisbbsnum &&
//                  pInfo->paOtherSystem[iCurrentSystem].szNumber != frombbs
//                  && pInfo->paOtherSystem[iCurrentSystem].szSendTo == TRUE)
				if (pInfo->paOtherSystem[iCurrentSystem].szNumber != thisbbsnum)
				{
					/* Send information to that system */
//          ToReturn = IBSend(pInfo, pInfo->paOtherSystem[iCurrentSystem].szAddress,
//          pBuffer, nBufferSize);
			ToReturn = IBSend(pInfo, pInfo->paOtherSystem[iCurrentSystem].szAddress,
				pBuffer, nBufferSize, bEncoded, bUseGigo);
		if(ToReturn != eSuccess)        return(ToReturn);
	}



			/* Send information to that system */
//      ToReturn = IBSend(pInfo, pInfo->paOtherSystem[iCurrentSystem].szAddress,
//                        pBuffer, nBufferSize);
//      if(ToReturn != eSuccess) return(ToReturn);



			}

	 return(eSuccess);
	 }


//tIBResult IBSend(tIBInfo *pInfo, char *pszDestNode, void *pBuffer,
//       INT16 nBufferSize)
tIBResult IBSend(tIBInfo *pInfo, char *pszDestNode, void *pBuffer,
		 INT16 nBufferSize, INT16 bEncoded, INT16 bUseGigo)
	 {
	 tIBResult ToReturn;
	 tMessageHeader MessageHeader;
	 time_t lnSecondsSince1970;
	 struct tm *pTimeInfo;
	 char szTOPT[13];
	 char szFMPT[13];
	 char szINTL[43];
	 char szMSGID[42];
	 INT16 nKludgeSize;
	 INT16 nTextSize;
	 char *pszMessageText;
//   char far *pszMessageText;
	 tFidoNode DestNode;
	 tFidoNode OrigNode;

	 char gigoline[80];

	 if(pszDestNode == NULL) return(eBadParameter);
	 if(pBuffer == NULL) return(eBadParameter);

	 /* Validate information structure */
	 ToReturn = ValidateInfoStruct(pInfo);
	 if(ToReturn != eSuccess) return(ToReturn);

	 /* Get destination node address from string */
	 ConvertStringToAddress(&DestNode, pszDestNode);

	 /* Get origin address from string */
	 ConvertStringToAddress(&OrigNode, pInfo->szThisNodeAddress);

	 /* Construct message header */
	 /* Construct to, from and subject information */
	if (bEncoded == TRUE)
	{
	 // encoded
	 // 11/4/96 Init fields to null spaces...
	 memset(MessageHeader.szFromUserName, '\0', 36);
	 memset(MessageHeader.szToUserName, '\0', 36);
	 memset(MessageHeader.szSubject, '\0', 72);
	 strcpy(MessageHeader.szFromUserName, pInfo->szProgName);
	 strcpy(MessageHeader.szToUserName, pInfo->szProgName);
	 strcpy(MessageHeader.szSubject, MESSAGE_SUBJECT);
	}
	else
	{
	 
	 // not encoded
	 // init fields to null spaces
	 memset(MessageHeader.szFromUserName, '\0', 36);
	 strcpy(MessageHeader.szFromUserName, pInfo->szFromUserName);
	 
	 // test for GIGO style addressing
	 memset(MessageHeader.szToUserName, '\0', 36);
	 if (bUseGigo) 
		strcpy(MessageHeader.szToUserName, "UUCP");
	 else 
		strcpy(MessageHeader.szToUserName, pInfo->szToUserName);

	 memset(MessageHeader.szSubject, '\0', 72);
	 strcpy(MessageHeader.szSubject, pInfo->szSubject);
	
	}

	 /* Construct date and time information */
	 lnSecondsSince1970 = time(NULL);
	 pTimeInfo = localtime(&lnSecondsSince1970);
	 sprintf(MessageHeader.szDateTime, "%02d %s %02d  %02d:%02d:%02d",
		 pTimeInfo->tm_mday,
		 aszShortMonthName[pTimeInfo->tm_mon],
		 pTimeInfo->tm_year-100,
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
	 if(pInfo->bCrash) MessageHeader.wAttribute |= ATTRIB_CRASH;
		// added for Easy Mail
	 if(pInfo->bDirect) MessageHeader.wAttribute |= ATTRIB_DIRECT;

	 if(pInfo->bHold) MessageHeader.wAttribute |= ATTRIB_HOLD;
	 if(pInfo->bEraseOnSend) MessageHeader.wAttribute |= ATTRIB_KILL_SENT;
// my addition
	 if(pInfo->bFileAttach) MessageHeader.wAttribute |= ATTRIB_FILE_ATTACH;
//   if(pInfo->bDeleteOnSend) MessageHeader.wAttribute |= ATTRIB_DEL_FILE;

	 /* Create message control (kludge) lines */
	 /* Create TOPT kludge line if destination point is non-zero */
	 if(DestNode.wPoint != 0)
			{
		 sprintf(szTOPT, "\1TOPT %u\r", DestNode.wPoint);
			}
	 else
			{
			strcpy(szTOPT, "");
			}

	 /* Create FMPT kludge line if origin point is non-zero */
	 if(OrigNode.wPoint != 0)
			{
			sprintf(szFMPT, "\1FMPT %u\r", OrigNode.wPoint);
			}
	 else
			{
			strcpy(szFMPT, "");
			}

	 /* Create INTL kludge line if origin and destination zone addresses differ */
	 /* 10/96 GAC Changed to always create International line for proper
		game usage in networks... */
	 /*
	 if(DestNode.wZone != OrigNode.wZone)
			{
	 */
			sprintf(szINTL, "\1INTL %u:%u/%u %u:%u/%u\r",
				DestNode.wZone,
				DestNode.wNet,
				DestNode.wNode,
				OrigNode.wZone,
				OrigNode.wNet,
				OrigNode.wNode);
	 /*
			}
	 else
			{
			strcpy(szINTL, "");
			}
	 */

	 /* Create MSGID kludge line, including point if non-zero */
/*
	 if(OrigNode.wPoint != 0)
			{
			sprintf(szMSGID, "\1MSGID: %u:%u/%u.%u %lx\r",
				OrigNode.wZone,
				OrigNode.wNet,
				OrigNode.wNode,
				OrigNode.wPoint,
				GetNextMSGID());
			}
	 else
			{
			sprintf(szMSGID, "\1MSGID: %u:%u/%u %lx\r",
				OrigNode.wZone,
				OrigNode.wNet,
				OrigNode.wNode,
				GetNextMSGID());
			}
*/
	szMSGID[0]=0;
	 /* Determine total size of kludge lines */
	 nKludgeSize = strlen(szTOPT)
		+ strlen(szFMPT)
		+ strlen(szINTL)
		+ strlen(szMSGID);

//      + strlen(MESSAGE_PID);

	 /* Determine total size of message text */
	if (bEncoded == TRUE)
	{
	 nTextSize = GetMaximumEncodedLength(nBufferSize)
				+ strlen(MESSAGE_HEADER)
				+ nKludgeSize
				+ strlen(MESSAGE_FOOTER)
				+ 1;
	}
	else
	{
	 nTextSize = nBufferSize
//              + strlen(MESSAGE_HEADER)
				+ nKludgeSize
//              + strlen(MESSAGE_FOOTER)
				+ 1;
	 if((pInfo->bDeleteOnSend || pInfo->bImmediate) && bEncoded == FALSE) nTextSize += 8;
	 if(pInfo->bDeleteOnSend && bEncoded == FALSE) nTextSize += 4;
	 if(pInfo->bImmediate && bEncoded == FALSE) nTextSize += 4;
		// check for Gigo style of NetMail addressing
	 if (bUseGigo)
	 {
			sprintf(gigoline, "To: %s\n\r\n\r", pInfo->szToUserName);
			nTextSize += strlen(gigoline);
	 }
	}
	 /* Attempt to allocate space for message text */
		// I changed it so the memory comes from the FAR heap
//   if((pszMessageText = (char far *) farmalloc(nTextSize)) == NULL)
	 if((pszMessageText = malloc(nTextSize)) == NULL)
			{
			return(eNoMemory);
			}

	 /* Construct message text */
	 strcpy(pszMessageText, szTOPT);
	 strcat(pszMessageText, szFMPT);
	 strcat(pszMessageText, szINTL);
//   strcat(pszMessageText, szMSGID);
//   strcat(pszMessageText, MESSAGE_PID);
	if (bEncoded == TRUE)    strcat(pszMessageText, MESSAGE_HEADER);
	if (bEncoded == TRUE)    EncodeBuffer(pszMessageText + strlen(pszMessageText), pBuffer, nBufferSize);
	else
	{
		// if Gigo style, add the To: line first
		// Should be doing this in the easy mail files and not here...
		if (bUseGigo) strcat(pszMessageText, gigoline);
		strncat(pszMessageText, pBuffer, nBufferSize);
	}

	if (bEncoded == TRUE)    strcat(pszMessageText, MESSAGE_FOOTER);
																																 // removed /r/n                // 01H
	if((pInfo->bDeleteOnSend || pInfo->bImmediate ) && bEncoded == FALSE) strcat(pszMessageText, "FLAGS");
	if(pInfo->bDeleteOnSend && bEncoded == FALSE) strcat(pszMessageText, " KFS");
	if(pInfo->bImmediate && bEncoded == FALSE) strcat(pszMessageText, " IMM");

	 /* Attempt to send the message */
	 if(CreateMessage(pInfo->szNetmailDir, &MessageHeader, pszMessageText))
			{
			ToReturn = eSuccess;
			}
	 else
			{
			ToReturn = eGeneralFailure;
			}

	 /* Deallocate message text buffer */
	 free(pszMessageText);
//   farfree(pszMessageText);

	 /* Return appropriate value */
	 return(ToReturn);
	 }





INT16 GetMaximumEncodedLength(INT16 nUnEncodedLength)
	 {
	 INT16 nEncodedLength;

	 /* The current encoding algorithm uses two characters to represent   */
	 /* each byte of data, plus 1 byte per MAX_LINE_LENGTH characters for */
	 /* the carriage return character.                                    */

	 nEncodedLength = nUnEncodedLength * 2;

	 return(nEncodedLength + (nEncodedLength / MAX_LINE_LENGTH - 1) + 1);
	 }


void EncodeBuffer(char *pszDest, const void *pBuffer, INT16 nBufferSize)
	 {
	 INT16 iSourceLocation;
	 INT16 nOutputChars = 0;
	 char *pcDest = pszDest;
	 const char *pcSource = pBuffer;

	 /* Loop for each byte of the source buffer */
	 for(iSourceLocation = 0; iSourceLocation < nBufferSize; ++iSourceLocation)
	 {
			/* First character contains bits 0 - 5, with 01 in upper two bits */
			*pcDest++ = (*pcSource & 0x3f) | 0x40;
			/* Second character contains bits 6 & 7 in positions 4 & 5. Upper */
			/* two bits are 01, and all remaining bits are 0. */
			*pcDest++ = ((*pcSource & 0xc0) >> 2) | 0x40;

			/* Output carriage return when needed */
			if((nOutputChars += 2) >= MAX_LINE_LENGTH - 1)
			{
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


void DecodeBuffer(const char *pszSource, void *pDestBuffer, INT16 nBufferSize)
	 {
	 const char *pcSource = pszSource;
	 char *pcDest = pDestBuffer;
	 INT16 iDestLocation;
	 INT16 bFirstOfByte = TRUE;

// Set the global variable number of bytes to zero!
IBBSRead = 0;

	 /* Search for beginning of buffer delimiter char, returning if not found */
	 while(*pcSource && *pcSource != DELIMITER_CHAR) ++pcSource;
	 if(!*pcSource) return;

	 /* Move pointer to first char after delimiter char */
	 ++pcSource;

   /* Loop until destination buffer is full, delimiter char is encountered, */
   /* or end of source buffer is encountered */
   iDestLocation = 0;
   while(iDestLocation < nBufferSize && *pcSource
	&& *pcSource != DELIMITER_CHAR)
	  {
			/* If this is a valid data character */
			// if(*pcSource >= 0x40 && *pcSource <= 0x7f)
			/* A char is signed, so it CAN'T be > 0x7f */
			if(*pcSource >= 0x40)
	 {
	 /* If this is first character of byte */
	 if(bFirstOfByte)
			{
		*pcDest = *pcSource & 0x3f;

		/* Toggle bFirstOfByte */
		bFirstOfByte = FALSE;
		}
	 else /* if(!bFirstOfByte) */
		{
		*pcDest |= (*pcSource & 0x30) << 2;

		/* Increment destination */
		++iDestLocation;
		++pcDest;

// Increment bytes read
IBBSRead++;

		/* Toggle bFirstOfByte */
		bFirstOfByte = TRUE;
			}
	 }

			/* Increment source byte pointer */
			++pcSource;
			}
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


INT16 CreateMessage(char *pszMessageDir, tMessageHeader *pHeader,
				char *pszText)
//INT16 CreateMessage(char *pszMessageDir, tMessageHeader *pHeader,
//              char far *pszText)
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


INT16 WriteMessage(char *pszMessageDir, DWORD lwMessageNum,
			 tMessageHeader *pHeader, char *pszText)
//INT16 WriteMessage(char *pszMessageDir, DWORD lwMessageNum,
//           tMessageHeader *pHeader, char far *pszText)
	 {
	 char szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	 int hFile;
	 size_t nTextSize;

	 /* Get fully qualified filename of message to write */
	 GetMessageFilename(pszMessageDir, lwMessageNum, szFileName);

	 /* Open message file */
	 hFile = sopen(szFileName, O_WRONLY|O_BINARY|O_CREAT, SH_DENYRW, 
		S_IREAD|S_IWRITE);

	 /* If open failed, return FALSE */
	 if(hFile == -1) return(FALSE);

	 /* Attempt to write header */
	 MessageHeader_le(pHeader);
	 if(write(hFile, pHeader, sizeof(tMessageHeader)) != sizeof(tMessageHeader))
			{
			/* On failure, close file, erase file, and return FALSE */
			close(hFile);
			unlink(szFileName);
	 		MessageHeader_le(pHeader);
			return(FALSE);
			}
	 MessageHeader_le(pHeader);

   /* Determine size of message text, including string terminator */
   nTextSize = strlen(pszText) + 1;

   /* Attempt to write message text */
	 if(write(hFile, pszText, nTextSize) != nTextSize)
			{
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


INT16 ReadMessage(char *pszMessageDir, DWORD lwMessageNum,
			tMessageHeader *pHeader, char **ppszText)
//INT16 ReadMessage(char *pszMessageDir, DWORD lwMessageNum,
//          tMessageHeader *pHeader, char far **ppszText)
	 {
	 char szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	 int hFile;
	 size_t nTextSize;

	 /* Get fully qualified filename of message to read */
	 GetMessageFilename(pszMessageDir, lwMessageNum, szFileName);

	 /* Open message file */
	 hFile = sopen(szFileName, O_RDONLY|O_BINARY, SH_DENYWR);

	 /* If open failed, return FALSE */
	 if(hFile == -1) return(FALSE);

	 /* Determine size of message body */
	 nTextSize = (size_t)filelength(hFile) - sizeof(tMessageHeader);

	 /* Attempt to allocate space for message body, plus character for added */
	 /* string terminator.                                                   */
		// I changed this to allocate from the FAR data heap
//      if((*ppszText =  (char far *) farmalloc(nTextSize + 1)) == NULL)
	 if((*ppszText = malloc(nTextSize + 1)) == NULL)
			{
			/* On failure, close file and return FALSE */
			close(hFile);
			return(FALSE);
			}

	 /* Attempt to read header */
	 if(read(hFile, pHeader, sizeof(tMessageHeader)) != sizeof(tMessageHeader))
			{
			/* On failure, close file, deallocate message buffer and return FALSE */
			close(hFile);
			free(*ppszText);
//          farfree(*ppszText);
			return(FALSE);
			}
	 MessageHeader_le(pHeader);

	 /* Attempt to read message text */
	 if(read(hFile, *ppszText, nTextSize) != nTextSize)
			{
			/* On failure, close file, deallocate message buffer and return FALSE */
			close(hFile);
			free(*ppszText);
//          farfree(*ppszText);
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
	 glob_t DirEntry;
	 char szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	int i;

	 MakeFilename(pszMessageDir, "*.msg", szFileName);

	 if(glob(szFileName, GLOB_NOSORT, NULL, &DirEntry) == 0)
			{
		for(i=0; i<DirEntry.gl_pathc; i++)
	 {
	 lwCurrentMsgNum = atol(DirEntry.gl_pathv[i]);
	 if(lwCurrentMsgNum > lwHighestMsgNum)
		{
		lwHighestMsgNum = lwCurrentMsgNum;
		}
	 }
	  }

   return(lwHighestMsgNum + 1);
   }


tIBResult ValidateInfoStruct(tIBInfo *pInfo)
	 {
	 if(pInfo == NULL) return(eBadParameter);

	 if(!DirExists(pInfo->szNetmailDir)) return(eMissingDir);

	 if(strlen(pInfo->szProgName) == 0) return(eBadParameter);

	 return(eSuccess);
	 }

// check the list of league members and make sure this BBS is listed
// INT16 IsMSGFromMember( word wOrigZone, word wOrigNet, word wOrigNode, word wOrigPoint, INT16 bIgnoreZonePoint)
INT16 IsMSGFromMember( tMessageHeader *MessageHeader, tIBInfo *pInfo)
{

	INT16 iCurrentSystem;
	tFidoNode FromNode;

	// Search the InterBBS function for the information
	// Loop for each system in other systems array
	for(iCurrentSystem = 0; iCurrentSystem < pInfo->nTotalSystems; ++iCurrentSystem)
	{
		 /* Get this node's address from string */
		 ConvertStringToAddress(&FromNode, pInfo->paOtherSystem[iCurrentSystem].szAddress);

		// Check to see if this from address matches the message header
		if (
				 (
				 FromNode.wZone == MessageHeader->wOrigZone
				 && FromNode.wNet == MessageHeader->wOrigNet
				 && FromNode.wNode == MessageHeader->wOrigNode
				 && FromNode.wPoint == MessageHeader->wOrigPoint
				 && pInfo->bIgnoreZonePoint == FALSE
				 )
				 ||
				// Many systems do not recognize zone and points
				 ( FromNode.wNet == MessageHeader->wOrigNet
				 && FromNode.wNode == MessageHeader->wOrigNode
				 && pInfo->bIgnoreZonePoint == TRUE
				 )
		) return(TRUE);

	}

	// did not find the from nodes address in any of the league addresses
	// set the message to read so we don't process it again!

	if (pInfo->bIgnoreZonePoint == FALSE) od_printf("`red`Unknown Node: Ignoring Information from %d:%d/%d.%d\n\r", MessageHeader->wOrigZone, MessageHeader->wOrigNet, MessageHeader->wOrigNode, MessageHeader->wOrigPoint);
	else od_printf("`red`Unknown Node: Ignoring Information from %d/%d\n\r", MessageHeader->wOrigNet, MessageHeader->wOrigNode);

	od_log_write("Ignored Information from Unknown Node");

	return(FALSE);
}

tIBResult IBGet(tIBInfo *pInfo, void *pBuffer, INT16 nMaxBufferSize)
	 {
	 tIBResult ToReturn;
	 glob_t DirEntry;
	 DWORD lwCurrentMsgNum;
//   tMessageHeader MessageHeader;
	 char szFileName[PATH_CHARS + FILENAME_CHARS + 2];
	 char *pszText;
//   char far *pszText;
	 tFidoNode ThisNode;
	int	i;

	 /* Validate information structure */
	 ToReturn = ValidateInfoStruct(pInfo);
	 if(ToReturn != eSuccess) return(ToReturn);

// printf("Validated\n");

	 /* Get this node's address from string */
	 ConvertStringToAddress(&ThisNode, pInfo->szThisNodeAddress);

	 MakeFilename(pInfo->szNetmailDir, "*.msg", szFileName);

	 /* Seach through each message file in the netmail directory, in no */
	 /* particular order.                                               */
	 if(glob(szFileName, GLOB_NOSORT, NULL, &DirEntry) == 0)
			{
			for(i=0; i<DirEntry.gl_pathc; i++)
	 {
	 lwCurrentMsgNum = atol(DirEntry.gl_pathv[i]);

	 /* If able to read message */
	 if(ReadMessage(pInfo->szNetmailDir, lwCurrentMsgNum, &MessageHeader,
			&pszText))
			{
// printf("Message read\n");
// printf("To %ud:%ud/%ud.%ud\n", MessageHeader.wDestZone, MessageHeader.wDestNet,MessageHeader.wDestNode,MessageHeader.wDestPoint);
// printf("Us %ud:%ud/%ud.%ud\n", ThisNode.wZone, ThisNode.wNet,ThisNode.wNode,ThisNode.wPoint);
// gac_pause();
			/* If message is for us, and hasn't be read yet */
			if (
					 (  strnicmp(MessageHeader.szToUserName, pInfo->szProgName, strlen(pInfo->szProgName)) == 0
				 && ThisNode.wZone == MessageHeader.wDestZone
				 && ThisNode.wNet == MessageHeader.wDestNet
				 && ThisNode.wNode == MessageHeader.wDestNode
				 && ThisNode.wPoint == MessageHeader.wDestPoint
				 && !(MessageHeader.wAttribute & ATTRIB_RECEIVED)
				 && pInfo->bIgnoreZonePoint == FALSE
					 )
				 ||
				// Many systems do not recognize zone and points
					 ( strnicmp(MessageHeader.szToUserName, pInfo->szProgName, strlen(pInfo->szProgName)) == 0
				 && ThisNode.wNet == MessageHeader.wDestNet
				 && ThisNode.wNode == MessageHeader.wDestNode
				 && !(MessageHeader.wAttribute & ATTRIB_RECEIVED)
				 && pInfo->bIgnoreZonePoint == TRUE
						)
				 )
				 {

				 /* Decode message text, placing information in buffer */
				 DecodeBuffer(pszText, pBuffer, nMaxBufferSize);

		 /* If received messages should be deleted */
		 if(pInfo->bEraseOnReceive)
			{
			/* Determine filename of message to erase */
			GetMessageFilename(pInfo->szNetmailDir, lwCurrentMsgNum,
						 szFileName);

			/* Attempt to erase file */
			if(unlink(szFileName) == -1)
				 {
				 ToReturn = eGeneralFailure;
				 }
			else
				 {
				 ToReturn = eSuccess;
				 }
			}

				 /* If received messages should not be deleted */
				 else /* if(!pInfo->bEraseOnReceive) */
			{
			/* Mark message as read */
			MessageHeader.wAttribute |= ATTRIB_RECEIVED;
			++MessageHeader.wTimesRead;

			/* Attempt to rewrite message */
			if(!WriteMessage(pInfo->szNetmailDir, lwCurrentMsgNum,
			 &MessageHeader, pszText))
				 {
				 ToReturn = eGeneralFailure;
				 }
			else
				 {
				 ToReturn = eSuccess;
				 }
			}

				 /* Deallocate message text buffer */
				 free(pszText);
//               farfree(pszText);

				 /* Return appropriate value */
// check if message is from a legit league member, if not skip it
if ( IsMSGFromMember(&MessageHeader, pInfo) != TRUE && ToReturn != eGeneralFailure)
{
	continue;
}
				 return(ToReturn);
				 }
			free(pszText);
//          farfree(pszText);
			}
	 }
			}

	 /* If no new messages were found */
	 return(eNoMoreMessages);
	 }


void ConvertAddressToString(char *pszDest, const tFidoNode *pNode)
	 {
	 if(pNode->wPoint == 0)
			{
			sprintf(pszDest, "%u:%u/%u", pNode->wZone, pNode->wNet, pNode->wNode);
			}
	 else /* if(pNode->wPoint !=0) */
			{
			sprintf(pszDest, "%u:%u/%u.%u", pNode->wZone, pNode->wNet, pNode->wNode,
				pNode->wPoint);
	  }
   }


void ConvertStringToAddress(tFidoNode *pNode, const char *pszSource)
   {
   pNode->wZone = 0;
   pNode->wNet = 0;
   pNode->wNode = 0;
	 pNode->wPoint = 0;

	 // I added this check to hopefully clean up the problems with zones and points

	 if (sscanf(pszSource, "%hu:%hu/%hu.%hu", &(pNode->wZone), &(pNode->wNet),
		&(pNode->wNode), &(pNode->wPoint)) != 4)
		{
			pNode->wPoint = 0;
			if (sscanf(pszSource, "%hu:%hu/%hu", &(pNode->wZone), &(pNode->wNet),
				&(pNode->wNode)) != 3)
			{
				pNode->wZone = 0;
				sscanf(pszSource, "%hu/%hu", &(pNode->wNet), &(pNode->wNode));
			}
		}
	 }

/*
//#define NUM_KEYWORDS       10
#define NUM_KEYWORDS       12

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
// My addition
#define KEYWORD_LINK_NUMBER  10
#define KEYWORD_IGNORE 11

char *apszKeyWord[NUM_KEYWORDS] = {"SystemAddress",
				   "UserName",
				   "NetmailDir",
				   "Crash",
				   "Hold",
				   "EraseOnSend",
               "EraseOnReceive",
				   "LinkWith",
				   "LinkName",
				   "LinkLocation",
				   // my additions
				   "LinkNumber",
				   "IgnoreZonePoint",
				   };


tIBResult IBReadConfig(tIBInfo *pInfo, char *pszConfigFile)
   {
   // Set default values for pInfo settings 
   pInfo->nTotalSystems = 0;
   pInfo->paOtherSystem = NULL;

   // Process configuration file 
   if(!ProcessConfigFile(pszConfigFile, NUM_KEYWORDS, apszKeyWord,
			 ProcessConfigLine, (void *)pInfo))
			{
	  return(eFileOpenError);
	  }

	 // else
	 return(eSuccess);
	 }


void ProcessConfigLine(INT16 nKeyword, char *pszParameter, void *pCallbackData)
	 {
	 tIBInfo *pInfo = (tIBInfo *)pCallbackData;
	 tOtherNode *paNewNodeArray;

	 switch(nKeyword)
			{
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
	 if(stricmp(pszParameter, "Yes") == 0)
		{
		pInfo->bCrash = TRUE;
		}
	 else if(stricmp(pszParameter, "No") == 0)
		{
			pInfo->bCrash = FALSE;
		}
	 break;

	  case KEYWORD_HOLD:
	 if(stricmp(pszParameter, "Yes") == 0)
		{
		pInfo->bHold = TRUE;
		}
	 else if(stricmp(pszParameter, "No") == 0)
		{
		pInfo->bHold = FALSE;
		}
	 break;

	  case KEYWORD_KILL_SENT:
	 if(stricmp(pszParameter, "Yes") == 0)
		{
		pInfo->bEraseOnSend = TRUE;
		}
	 else if(stricmp(pszParameter, "No") == 0)
		{
		pInfo->bEraseOnSend = FALSE;
		}
	 break;

	  case KEYWORD_KILL_RCVD:
	 if(stricmp(pszParameter, "Yes") == 0)
		{
		pInfo->bEraseOnReceive = TRUE;
		}
	 else if(stricmp(pszParameter, "No") == 0)
		{
		pInfo->bEraseOnReceive = FALSE;
		}
	 break;

	  case KEYWORD_LINK_WITH:
	 if(pInfo->nTotalSystems == 0)
		{
			pInfo->paOtherSystem = malloc(sizeof(tOtherNode));
		if(pInfo->paOtherSystem == NULL)
		   {
		   break;
		   }
		}
	 else
		{
			if((paNewNodeArray = malloc(sizeof(tOtherNode) *
			 (pInfo->nTotalSystems + 1))) == NULL)
				 {
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
	 if(pInfo->nTotalSystems != 0)
		{
		strncpy(pInfo->paOtherSystem[pInfo->nTotalSystems - 1].szSystemName,
			pszParameter, SYSTEM_NAME_CHARS);
		pInfo->paOtherSystem[pInfo->nTotalSystems - 1].
		   szSystemName[SYSTEM_NAME_CHARS] = '\0';
		}
	 break;

	  case KEYWORD_LINK_LOC:
	 if(pInfo->nTotalSystems != 0)
		{
		strncpy(pInfo->paOtherSystem[pInfo->nTotalSystems - 1].szLocation,
			pszParameter, LOCATION_CHARS);
		pInfo->paOtherSystem[pInfo->nTotalSystems - 1].
		   szLocation[LOCATION_CHARS] = '\0';
		}
	 break;
	  // My additions
	  case KEYWORD_LINK_NUMBER:
	 if(pInfo->nTotalSystems != 0)
		{
		pInfo->paOtherSystem[pInfo->nTotalSystems - 1].
		   szNumber = atoi(pszParameter);
		}
	 break;
	  case KEYWORD_IGNORE:
	 if(stricmp(pszParameter, "Yes") == 0)
		{
		pInfo->bIgnoreZonePoint = TRUE;
		}
	 else if(stricmp(pszParameter, "No") == 0)
		{
		pInfo->bIgnoreZonePoint = FALSE;
		}
	 break;
	  }
   }
*/

/* Configuration file reader settings */
#define CONFIG_LINE_SIZE 128
#define MAX_TOKEN_CHARS 32

INT16 ProcessConfigFile(char *pszFileName, INT16 nKeyWords, char **papszKeyWord,
		  void (*pfCallBack)(INT16, char *, void *), void *pCallBackData)
   {
   FILE *pfConfigFile;
   char szConfigLine[CONFIG_LINE_SIZE + 1];
   char *pcCurrentPos;
   WORD uCount;
   char szToken[MAX_TOKEN_CHARS + 1];
   INT16 iKeyWord;

   /* Attempt to open configuration file */
   if((pfConfigFile = fopen(pszFileName, "rb")) == NULL)
	  {
	  return(FALSE);
	  }

   /* While not at end of file */
   while(!feof(pfConfigFile))
	  {
	  /* Get the next line */
	  if(fgets(szConfigLine, CONFIG_LINE_SIZE + 1 ,pfConfigFile) == NULL) break;

	  /* Ignore all of line after comments or CR/LF char */
	  pcCurrentPos=(char *)szConfigLine;
	  while(*pcCurrentPos)
	 {
	 if(*pcCurrentPos=='\n' || *pcCurrentPos=='\r' || *pcCurrentPos==';')
		{
		*pcCurrentPos='\0';
		break;
		}
	 ++pcCurrentPos;
	 }

	  /* Search for beginning of first token on line */
	  pcCurrentPos=(char *)szConfigLine;
	  while(*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

	  /* If no token was found, proceed to process the next line */
	  if(!*pcCurrentPos) continue;

	  /* Get first token from line */
	  uCount=0;
	  while(*pcCurrentPos && !isspace(*pcCurrentPos))
	 {
	 if(uCount<MAX_TOKEN_CHARS) szToken[uCount++]=*pcCurrentPos;
	 ++pcCurrentPos;
	 }
	  if(uCount<=MAX_TOKEN_CHARS)
	 szToken[uCount]='\0';
	  else
	 szToken[MAX_TOKEN_CHARS]='\0';

	  /* Find beginning of configuration option parameters */
	  while(*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

	   /* Trim trailing spaces from setting string */
	  for(uCount=strlen(pcCurrentPos)-1;uCount>0;--uCount)
	 {
	 if(isspace(pcCurrentPos[uCount]))
		{
		pcCurrentPos[uCount]='\0';
		}
	 else
		{
		break;
		}
	 }

	  /* Loop through list of keywords */
	  for(iKeyWord = 0; iKeyWord < nKeyWords; ++iKeyWord)
	 {
	 /* If keyword matches */
	 if(stricmp(szToken, papszKeyWord[iKeyWord]) == 0)
		{
		/* Call keyword processing callback function */
		(*pfCallBack)(iKeyWord, pcCurrentPos, pCallBackData);
		}
	 }
	  }

   /* Close the configuration file */
   fclose(pfConfigFile);

   /* Return with success */
   return(TRUE);
   }

#ifdef __BIG_ENDIAN__
void MessageHeader_le(tMessageHeader *mh)
{
	mh->wTimesRead=LE_SHORT(mh->wTimesRead);
	mh->wDestNode=LE_SHORT(mh->wDestNode);
	mh->wOrigNode=LE_SHORT(mh->wOrigNode);
	mh->wCost=LE_SHORT(mh->wCost);
	mh->wOrigNet=LE_SHORT(mh->wOrigNet);
	mh->wDestNet=LE_SHORT(mh->wDestNet);
	mh->wDestZone=LE_SHORT(mh->wDestZone);
	mh->wOrigZone=LE_SHORT(mh->wOrigZone);
	mh->wDestPoint=LE_SHORT(mh->wDestPoint);
	mh->wOrigPoint=LE_SHORT(mh->wOrigPoint);
	mh->wReplyTo=LE_SHORT(mh->wReplyTo);
	mh->wAttribute=LE_SHORT(mh->wAttribute);
	mh->wNextReply=LE_SHORT(mh->wNextReply);
}
#endif
