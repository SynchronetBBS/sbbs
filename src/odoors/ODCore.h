/* OpenDoors Online Software Programming Toolkit
 * (C) Copyright 1991 - 1999 by Brian Pirie.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *        File: ODCore.h
 *
 * Description: Global functions and variables provide by the odcore.c
 *              module. These core facilities are used throughout OpenDoors,
 *              and are required regardless of what OpenDoors features that
 *              a given program uses.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Nov 16, 1995  6.00  BP   Created.
 *              Nov 17, 1995  6.00  BP   Use new input queue mechanism.
 *              Dec 24, 1995  6.00  BP   Added abtGreyBlock.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 01, 1996  6.00  BP   Changed TEXT_SIZE to 49.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Sep 01, 1996  6.10  BP   Update output area on od_set_per...().
 */

#ifndef _INC_ODCORE
#define _INC_ODCORE


/* Include other header files that have definitions neede by this one. */
#include "ODInQue.h"
#include "ODCom.h"
#include "ODPlat.h"
#include "ODScrn.h"


/* OpenDoors global initialized flag. */
extern BOOL bODInitialized;

/* Global serial port object handle. */
extern tPortHandle hSerialPort;

/* Global input queue object handle. */
extern tODInQueueHandle hODInputQueue;

/* Reentrancy control. */
extern BOOL bIsCallbackActive;
extern BOOL bShellChatActive;

/* Global working space. */
#define OD_GLOBAL_WORK_STRING_SIZE  1025
extern char szODWorkString[OD_GLOBAL_WORK_STRING_SIZE];

/* Global instance of the text information structure for general use. */
extern tODScrnTextInfo ODTextInfo;

/* Logfile function hooks. */
extern BOOL (*pfLogWrite)(INT);
extern void (*pfLogClose)(INT);

/* od_colour_config() support for od_printf(). */
extern char chColorCheck;
extern char *pchColorEndPos;

/* Status line information. */
extern BYTE btCurrentStatusLine;
extern OD_PERSONALITY_CALLBACK *pfCurrentPersonality;
extern char szDesiredPersonality[33];
typedef BOOL ODCALL SET_PERSONALITY_FUNC(char *pszName);
extern SET_PERSONALITY_FUNC *pfSetPersonality;

/* Commonly used character sequences. */
extern char abtBlackBlock[2];
extern char abtGreyBlock[2];
extern char szBackspaceWithDelete[4];

/* Current output area on screen. */
extern BYTE btOutputTop;
extern BYTE btOutputBottom;


/* Core functions used throughout OpenDoors. */
void ODWaitDrain(tODMilliSec MaxWait);
void ODStoreTextInfo(void);
void ODRestoreTextInfo(void);
void ODStringToName(char *pszToConvert);
BOOL ODPagePrompt(BOOL *pbPausing);


/* Number of built-in configuration file options. */
#define TEXT_SIZE 49

/* Number of user-defined info file options. */
#define LINES_SIZE 25


#endif /* _INC_ODCORE */
