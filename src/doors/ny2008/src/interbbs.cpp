#include <assert.h>
#include <ctype.h>
#include <string.h>
#if !defined(__WATCOMC__)
#include <dir.h>
#endif
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <dos.h>
#include <fcntl.h>
#include <sys\stat.h>

#include "ny2008.h"
//#include "\ibbs\interbbs.h"
extern unsigned _stklen;


char aszShortMonthName[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

tBool DirExists(const char *pszDirName) {
        char szDirFileName[PATH_CHARS + 1];
        struct ffblk DirEntry;

        //   if(pszDirName==NULL) return(FALSE);
        //   if(strlen(pszDirName)>PATH_CHARS) return(FALSE);
        assert(pszDirName != NULL);
        assert(strlen(pszDirName) <= PATH_CHARS);

        strcpy(szDirFileName, pszDirName);

        /* Remove any trailing backslash from directory name */
        if(szDirFileName[strlen(szDirFileName) - 1] == '\\') {
                szDirFileName[strlen(szDirFileName) - 1] = '\0';
        }

        /* Return true iff file exists and it is a directory */
        return(findfirst(szDirFileName, &DirEntry, FA_ARCH|FA_DIREC) == 0 &&
               (DirEntry.ff_attrib & FA_DIREC));
}


void MakeFilename(const char *pszPath, const char *pszFilename, char *pszOut) {
        /* Validate parameters in debug mode */
        assert(pszPath != NULL);
        assert(pszFilename != NULL);
        assert(pszOut != NULL);
        assert(pszPath != pszOut);
        assert(pszFilename != pszOut);

        /* Copy path to output filename */
        strcpy(pszOut, pszPath);

        /* Ensure there is a trailing backslash */
        if(pszOut[strlen(pszOut) - 1] != '\\') {
                strcat(pszOut, "\\");
        }

        /* Append base filename */
        strcat(pszOut, pszFilename);
}


tIBResult IBSendAll(tIBInfo *pInfo, void *pBuffer, INT16 nBufferSize) {
        tIBResult ToReturn;
        INT16 iCurrentSystem;

        if(pBuffer == NULL)
                return(eBadParameter);

        /* Validate information structure */
        ToReturn = ValidateInfoStruct(pInfo);
        if(ToReturn != eSuccess)
                return(ToReturn);

        if(pInfo->paOtherSystem == NULL && pInfo->nTotalSystems != 0) {
                return(eBadParameter);
        }

        /* Loop for each system in other systems array */
        for(iCurrentSystem = 0; iCurrentSystem < pInfo->nTotalSystems;
                ++iCurrentSystem) {
                /* Send information to that system */
                if(strcmp(pInfo->paOtherSystem[iCurrentSystem].szAddress,pInfo->szThisNodeAddress)!=0) {
                        ToReturn = IBSend(pInfo, pInfo->paOtherSystem[iCurrentSystem].szAddress,
                                          pBuffer, nBufferSize);
                        if(ToReturn != eSuccess)
                                return(ToReturn);
                }
        }

        return(eSuccess);
}


tIBResult IBSend(tIBInfo *pInfo, char *pszDestNode, void *pBuffer,
                 INT16 nBufferSize) {
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
        tFidoNode DestNode;
        tFidoNode OrigNode;

        if(pszDestNode == NULL)
                return(eBadParameter);
        if(pBuffer == NULL)
                return(eBadParameter);

        /* Validate information structure */
        ToReturn = ValidateInfoStruct(pInfo);
        if(ToReturn != eSuccess)
                return(ToReturn);

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
        if(pInfo->bCrash)
                MessageHeader.wAttribute |= ATTRIB_CRASH;
        if(pInfo->bHold)
                MessageHeader.wAttribute |= ATTRIB_HOLD;
        if(pInfo->bEraseOnSend)
                MessageHeader.wAttribute |= ATTRIB_KILL_SENT;

        /* Create message control (kludge) lines */
        /* Create TOPT kludge line if destination point is non-zero */
        if(DestNode.wPoint != 0) {
                sprintf(szTOPT, "\1TOPT %u\r", DestNode.wPoint);
        } else {
                strcpy(szTOPT, "");
        }

        /* Create FMPT kludge line if origin point is non-zero */
        if(OrigNode.wPoint != 0) {
                sprintf(szFMPT, "\1FMPT %u\r", OrigNode.wPoint);
        } else {
                strcpy(szFMPT, "");
        }

        /* Create INTL kludge line if origin and destination zone addresses differ */
        if(DestNode.wZone != OrigNode.wZone) {
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
        if(OrigNode.wPoint != 0) {
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
        if((pszMessageText = (char *)malloc(nTextSize)) == NULL) {
                return(eNoMemory);
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
        if(CreateMessage(pInfo->szNetmailDir, &MessageHeader, pszMessageText)) {
                ToReturn = eSuccess;
        } else {
                ToReturn = eGeneralFailure;
        }

        /* Deallocate message text buffer */
        free(pszMessageText);

        /* Return appropriate value */
        return(ToReturn);
}

tIBResult IBSendMail(tIBInfo *pInfo, ibbs_mail_type *ibmail) {
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
        tFidoNode DestNode;
        tFidoNode OrigNode;

        if(ibmail->node_r == NULL)
                return(eBadParameter);
        if(ibmail == NULL)
                return(eBadParameter);

        /* Validate information structure */
        ToReturn = ValidateInfoStruct(pInfo);
        if(ToReturn != eSuccess)
                return(ToReturn);

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
        if(pInfo->bCrash)
                MessageHeader.wAttribute |= ATTRIB_CRASH;
        if(pInfo->bHold)
                MessageHeader.wAttribute |= ATTRIB_HOLD;
        if(pInfo->bEraseOnSend)
                MessageHeader.wAttribute |= ATTRIB_KILL_SENT;


        /* Create message control (kludge) lines */
        /* Create TOPT kludge line if destination point is non-zero */
        if(DestNode.wPoint != 0) {
                sprintf(szTOPT, "\1TOPT %u\r", DestNode.wPoint);
        } else {
                strcpy(szTOPT, "");
        }

        /* Create FMPT kludge line if origin point is non-zero */
        if(OrigNode.wPoint != 0) {
                sprintf(szFMPT, "\1FMPT %u\r", OrigNode.wPoint);
        } else {
                strcpy(szFMPT, "");
        }

        /* Create INTL kludge line if origin and destination zone addresses differ */
        if(DestNode.wZone != OrigNode.wZone) {
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
        if(OrigNode.wPoint != 0) {
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
        if((pszMessageText = (char *)malloc(nTextSize)) == NULL) {
                return(eNoMemory);
        }

        /* Construct message text */
        strcpy(pszMessageText, szTOPT);
        strcat(pszMessageText, szFMPT);
        strcat(pszMessageText, szINTL);
        strcat(pszMessageText, szMSGID);
        strcat(pszMessageText, MESSAGE_PID);
        strcat(pszMessageText, MESSAGE_HEADER);


        EncodeMail(pszMessageText + strlen(pszMessageText),ibmail);
        //   EncodeBuffer(pszMessageText + strlen(pszMessageText), pBuffer, nBufferSize);
        strcat(pszMessageText, MESSAGE_FOOTER);


        /* Attempt to send the message */
        if(CreateMessage(pInfo->szNetmailDir, &MessageHeader, pszMessageText)) {
                ToReturn = eSuccess;
        } else {
                ToReturn = eGeneralFailure;
        }


        /* Deallocate message text buffer */
        free(pszMessageText);


        /* Return appropriate value */
        return(ToReturn);
}



INT16 GetMaximumEncodedLength(INT16 nUnEncodedLength) {
        INT16 nEncodedLength;

        /* The current encoding algorithm uses two characters to represent   */
        /* each byte of data, plus 1 byte per MAX_LINE_LENGTH characters for */
        /* the carriage return character.                                    */

        nEncodedLength = nUnEncodedLength * 2;

        return(nEncodedLength + (nEncodedLength / MAX_LINE_LENGTH - 1) + 1);
}


void EncodeBuffer(char *pszDest, const void *pBuffer, INT16 nBufferSize) {
        INT16 iSourceLocation;
        INT16 nOutputChars = 0;
        char *pcDest = pszDest;
        const char *pcSource = (char *)pBuffer;

        /* Loop for each byte of the source buffer */
        for(iSourceLocation = 0; iSourceLocation < nBufferSize; ++iSourceLocation) {
                /* First character contains bits 0 - 5, with 01 in upper two bits */
                *pcDest++ = (*pcSource & 0x3f) | 0x40;
                /* Second character contains bits 6 & 7 in positions 4 & 5. Upper */
                /* two bits are 01, and all remaining bits are 0. */
                *pcDest++ = ((*pcSource & 0xc0) >> 2) | 0x40;

                /* Output carriage return when needed */
                if((nOutputChars += 2) >= MAX_LINE_LENGTH - 1) {
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

void EncodeMail(char *pszDest,ibbs_mail_type *ibmail) {
        char *pcDest = pszDest;
        INT16 x,l,ln;

        l=strlen(ibmail->sender);
        for(x=0;x<l;x++) {
                sprintf(pcDest,"%X",(INT16)ibmail->sender[x]);
                pcDest+=2;
        }
        *pcDest='\n';
        pcDest++;

        l=strlen(ibmail->senderI);
        for(x=0;x<l;x++) {
                sprintf(pcDest,"%X",(INT16)ibmail->senderI[x]);
                pcDest+=2;
        }
        *pcDest='\n';
        pcDest++;

        sprintf(pcDest,"%X",(INT16)ibmail->sender_sex);
        pcDest++;

        *pcDest='\n';
        pcDest++;

        l=strlen(ibmail->node_s);
        for(x=0;x<l;x++) {
