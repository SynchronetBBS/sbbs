/* OpenDoors Online Software Programming Toolkit
 * (C) Copyright 1991 - 1999 by Brian Pirie.
 *
 * Oct-2001 door32.sys/socket modifications by Rob Swindell (www.synchro.net)
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
 *        File: ODInEx1.c
 *
 * Description: Performs OpenDoors initialization and shutdown operations
 *              (od_init() and od_exit()), including drop file I/O. This
 *              module is broken into two files, ODInEx1.c and ODInEx2.c.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Oct 19, 1994  6.00  BP   Default paging hours 8:00-22:00.
 *              Oct 21, 1994  6.00  BP   Further isolated com routines.
 *              Oct 29, 1994  6.00  BP   Properly read dorinfo?.def BBS name.
 *              Oct 31, 1994  6.00  BP   Only use dorinfo?.def /w exitinfo.bbs.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Dec 31, 1994  6.00  BP   Remove call to _mt_init().
 *              Dec 31, 1994  6.00  BP   Add call to ODPlatInit().
 *              Jul 30, 1995  6.00  BP   Split up od_init().
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 14, 1995  6.00  BP   New default for od_colour_char is 0.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Nov 17, 1995  6.00  BP   Use new input queue mechanism.
 *              Nov 23, 1995  6.00  BP   Moved Pascal conversion to odutil.c.
 *              Dec 21, 1995  6.00  BP   Add ability to use already open port.
 *              Dec 22, 1995  6.00  BP   Added od_connect_speed.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Dec 30, 1995  6.00  BP   Only use comm idle func under DOS.
 *              Jan 01, 1996  6.00  BP   Added od_disable_dtr.
 *              Jan 01, 1996  6.00  BP   Raise DTR after opening serial port.
 *              Jan 02, 1996  6.00  BP   Use printf() in ODInitError().
 *              Jan 03, 1996  6.00  BP   Display connect speed with %lu.
 *              Jan 19, 1996  6.00  BP   Don't use atexit() under Win32.
 *              Jan 19, 1996  6.00  BP   Make ODInitError() a shared function.
 *              Jan 20, 1996  6.00  BP   Prompt for user name if force_local.
 *              Jan 23, 1996  6.00  BP   Added od_exiting and OD_TEXTMODE.
 *              Jan 31, 1996  6.00  BP   Added DIS_NAME_PROMPT.
 *              Jan 31, 1996  6.00  BP   Support new SFDOORS.DAT format.
 *              Feb 02, 1996  6.00  BP   Added RA 2.50 EXITINFO.BBS support.
 *              Feb 06, 1996  6.00  BP   Added od_silent_mode.
 *              Feb 08, 1996  6.00  BP   Recognize SFSYSOP.DAT.
 *              Feb 09, 1996  6.00  BP   Correctly translate RA 2.x sex field.
 *              Feb 09, 1996  6.00  BP   Made default outbound buffer 3072.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 20, 1996  6.00  BP   Added bParsedCmdLine.
 *              Feb 21, 1996  6.00  BP   Don't override command line options.
 *              Feb 21, 1996  6.00  BP   Change od_always_clear default to on.
 *              Feb 23, 1996  6.00  BP   Changed default DTR disable string.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 06, 1996  6.10  BP   Added TRIBBS.SYS support.
 *              Mar 06, 1996  6.10  BP   Initial support for Door32 interface.
 *              Mar 13, 1996  6.10  BP   Added od_local_win_col.
 *              Mar 17, 1996  6.10  BP   Reset text color after local login.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Apr 08, 1996  6.10  BP   Store local login name in user_handle.
 *              Jan 13, 1997  6.10  BP   Fixes for Door32 support.
 *              Oct 19, 2001  6.20  RS   Added door32.sys and socket support.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "OpenDoor.h"
#ifdef ODPLAT_NIX
#include <termios.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#endif
#include "ODStr.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODScrn.h"
#include "ODInQue.h"
#include "ODKrnl.h"
#include "ODInEx.h"
#include "ODUtil.h"
#ifdef ODPLAT_WIN32
#include "ODFrame.h"
#include "ODRes.h"
#endif /* ODPLAT_WIN32 */

/* Default size of local/remote combined keyboard buffer. */
#define DEFAULT_EVENT_QUEUE_SIZE    128


/* Local private helper functions. */
static void ODInitReadExitInfo(void);
static void ODInitPartTwo(void);
static BOOL ODInitReadSFDoorsDAT(void);
static void ODInitPartTwo(void);


/* Private variables. */
static BYTE btRAStatusToSet = 0;
#ifndef ODPLAT_WIN32
static BOOL bPreset = TRUE;
#endif /* !ODPLAT_WIN32 */
static char szIFTemp[256];

/* Configuration file keywords. */
static char *apszConfigText[] =
{
   "Node",
   "BBSDir",
   "DoorDir",
   "LogFileName",
   "DisableLogging",
   "SundayPagingHours",
   "MondayPagingHours",
   "TuesdayPagingHours",
   "WednesdayPagingHours",
   "ThursdayPagingHours",
   "FridayPagingHours",
   "SaturdayPagingHours",
   "MaximumDoorTime",
   "SysopName",
   "SystemName",
   "SwappingDisable",
   "SwappingDir",
   "SwappingNoEMS",
   "LockedBPS",
   "SerialPort",
   "CustomFileName",
   "CustomFileLine",
   "InactivityTimeout",
   "PageDuration",
   "ChatUserColour",
   "ChatSysopColour",
   "FileListTitleColour",
   "FileListNameColour",
   "FileListSizeColour",
   "FileListDescriptionColour",
   "FileListOfflineColour",
   "Personality",
   "NoFossil",
   "PortAddress",
   "PortIRQ",
   "ReceiveBuffer",
   "TransmitBuffer",
   "PagePromptColour",
   "LocalMode",
   "PopupMenuTitleColour",
   "PopupMenuBorderColour",
   "PopupMenuTextColour",
   "PopupMenuKeyColour",
   "PopupMenuHighlightColour",
   "PopupMenuHighKeyColour",
   "NoFIFO",
   "FIFOTriggerSize",
   "DisableDTR",
   "NoDTRDisable",
};

/* Custom drop file keywords. */
static char *apszConfigLines[] =
{
   "Ignore",
   "ComPort",
   "FossilPort",
   "ModemBPS",
   "LocalMode",
   "UserName",
   "UserFirstName",
   "UserLastName",
   "Alias",
   "HoursLeft",
   "MinutesLeft",
   "SecondsLeft",
   "ANSI",
   "AVATAR",
   "PagePausing",
   "ScreenLength",
   "ScreenClearing",
   "Security",
   "City",
   "Node",
   "SysopName",
   "SysopFirstName",
   "SysopLastName",
   "SystemName",
   "RIP",
};

/* Logfile messages. */
static char *apszLogMessages[] = 
{
   "Carrier lost, exiting door",
   "System operator terminating call, exiting door",
   "User's time limit expired, exiting door",
   "User keyboard inactivity time limit exceeded, exiting door",
   "System operator returning user to BBS, exiting door",
   "Exiting door with errorlevel %d",
   "Invoking operating system shell",
   "Returning from operating system shell",
   "User paging system operator",
   "Entering sysop chat mode",
   "Terminating sysop chat mode",
   "%s entering door",
   "Reason for chat: %s",
   "Exiting door",
};

/* Color name strings. */
static char *apszColorNames[] =
{
   "BLACK",
   "BLUE",
   "GREEN",
   "CYAN",
   "RED",
   "MAGENTA",
   "YELLOW",
   "WHITE",
   "BROWN",
   "GREY",
   "BRIGHT",
   "FLASHING",
};

/* Array of door information (drop) file names to search for. */
static char *apszDropFileNames[] = 
{
   "exitinfo.bbs",
   "dorinfo1.def",
   "chain.txt",
   "sfdoors.dat",
   "door.sys",
   "callinfo.bbs",
   "sfmain.dat",
   "sffile.dat",
   "sfmess.dat",
   "sfsysop.dat",
   "tribbs.sys",
   "door32.sys",
};

/* Array of door information (drop) file numbers 
 * (corresponding to apszDropFileNames) 
 */
enum {
   FOUND_EXITINFO_BBS,
   FOUND_DORINFO1_DEF,
   FOUND_CHAIN_TXT,
   FOUND_SFDOORS_DAT,
   FOUND_DOOR_SYS,
   FOUND_CALLINFO_BBS,
   FOUND_SFMAIN_DAT,
   FOUND_SFFILE_DAT,
   FOUND_SFMESS_DAT,
   FOUND_SFSYSOP_DAT,
   FOUND_TRIBBS_SYS,
   FOUND_DOOR32_SYS,
};

#define FOUND_NONE -1


/* Global variables. */
WORD wODNodeNumber = 65535U;
BOOL bIsCoSysop;
BOOL bIsSysop;
char *apszDropFileInfo[25];
BYTE btExitReason = 0;
DWORD dwForcedBPS = 1;
INT nForcedPort = -1;
DWORD dwFileBPS;
char szDropFilePath[120];
char szExitinfoBBSPath[120];
INT16 nInitialElapsed;
char *szOriginalDir = NULL;
BYTE btDoorSYSLock = 0;
time_t nStartupUnixTime;
INT16 nInitialRemaining;
BOOL bSysopNameSet = FALSE;
char szForcedSysopName[40];
BOOL bSystemNameSet = FALSE;
char szForcedSystemName[40];
BOOL bUserFull = FALSE;
BOOL bCalledFromConfig = FALSE;
tRA2ExitInfoRecord *pRA2ExitInfoRecord = NULL;
tExitInfoRecord *pExitInfoRecord = NULL;
tExtendedExitInfo *pExtendedExitInfo = NULL;
struct _pcbsys *pPCBoardSysRecord = NULL;
struct _userssyshdr *pUserSysHeader = NULL;
struct _userssysrec *pUserSysRecord = NULL;
BOOL bPreOrExit = FALSE;
BOOL bRAStatus;
BOOL bPromptForUserName = FALSE;
BOOL bParsedCmdLine = FALSE;
WORD wPreSetInfo = 0;
#ifdef ODPLAT_WIN32
tODThreadHandle hFrameThread;
#endif /* ODPLAT_WIN32 */


/* ----------------------------------------------------------------------------
 * od_init()
 *
 * Starts up OpenDoors. Initializes various members of od_control, reads the
 * BBS door information (drop file), initializes the serial port and carries
 * out other operations that must be done at initialization time. May be
 * explicitly called by the user, or called as a result of the first call to
 * some other OpenDoors API function.
 *
 * Parameters: none
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_init(void)
{
   BYTE btCount;
   FILE *pfDropFile=NULL;
   char *pointer;
   INT nFound = FOUND_NONE;
#ifdef _WIN32
   float forcefloats;

   forcefloats=1.1;
#endif

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_init()");

   /* If a callback function is active, then don't do anything. */
   if(bIsCallbackActive) return;

   /* If we are not being called from within ODConfigInit(). */
   if(!bCalledFromConfig)
   {
      /* If OpenDoors has already been initialized, then return without */
      /* doing anything.                                                */
      if(bODInitialized) return;

      /* Otherwise, set the initialized flag so that od_init() won't be */
      /* run again.                                                     */
      bODInitialized = TRUE;

      /* Initialize program name string. */
      if(od_control.od_prog_name[0] == '\0')
      {
         strcpy(od_control.od_prog_name, OD_VER_SHORTNAME);
      }

      /* Initialize color name strings in od_control. */
      for(btCount = 0; btCount < DIM(apszColorNames); ++btCount)
      {
         if(!*od_control.od_color_names[btCount])
         {
            strcpy(od_control.od_color_names[btCount],
               apszColorNames[btCount]);
         }
      }

      /* Initialize custom drop file strings in od_control. */
      for(btCount = 0; btCount < LINES_SIZE; ++btCount)
      {
         if(!*od_control.od_cfg_lines[btCount])
         {
            strcpy(od_control.od_cfg_lines[btCount], apszConfigLines[btCount]);
         }
      }

      /* Initialize configuration keyword strings in od_control. */
      for(btCount = 0; btCount < TEXT_SIZE; ++btCount)
      {
         if(!*od_control.od_cfg_text[btCount])
         {
            strcpy(od_control.od_cfg_text[btCount], apszConfigText[btCount]);
         }
      }

      /* Enable multiple personality system if it has been installed. */
#ifdef OD_TEXTMODE
      if(od_control.od_mps != NULL)
      {
         (*(OD_COMPONENT_CALLBACK *)od_control.od_mps)();
      }
#endif /* !OD_TEXTMODE */

      /* If baud rate has been set in od_control, then remember the forced */
      /* rate for later use.                                               */
      if(od_control.baud != 0)
      {
         dwForcedBPS = od_control.baud;
      }

      /* If the serial port number has already been set in od_control, then */
      /* remember the forced port number for later use.                     */
      if(od_control.port != 0)
      {
         nForcedPort = od_control.port;
      }

      /* If the configuration file system has been installed, then allow   */
      /* the configuration file initialization routine take over           */
      /* initialization from here. Once it has read the configuration file */
      /* it will call od_init() again, at which time od_init() will carry  */
      /* on from this point in the code.                                  */
      if(od_control.config_file != NULL)
      {
         (*(OD_COMPONENT_CALLBACK *)od_control.config_file)();
         return;
      }
   }

   /* Initialize the OpenDoors platform-specific utility functions. */
   ODPlatInit();

   /* Store the time of door startup. */
   time(&nStartupUnixTime);

   /* Allocate the set of strings that are used to store certain parts */
   /* of drop files for rewriting at od_exit() time.                   */
   for(btCount = 0; btCount < DIM(apszDropFileInfo); ++btCount)
   {
      if((apszDropFileInfo[btCount] = (char *)malloc(81)) == NULL)
      {
malloc_error:
         ODInitError("Insufficient memory available to start up program.");
         exit(od_control.od_errorlevel[1]);
      }
   }

   /* Determine the current node number. */ 
   if((pointer=getenv("TASK")) != NULL)
   {
      od_control.od_node = atoi(pointer);
   }
   else if((pointer=getenv("SBBSNNUM")) != NULL)
   {
      od_control.od_node = atoi(pointer);
   }
   else if(wODNodeNumber != 65535U)
   {
      od_control.od_node = wODNodeNumber;
   }
   else if(od_control.od_node == 0)
   {
      od_control.od_node = 1;
   }

   /* If a custom drop file format is not being used, then set certain   */
   /* od_control members to their default values. In the case where      */
   /* a custom drop file format is being used, these values will already */
   /* have been set, and so we don't want to touch them.                 */
   if(od_control.od_info_type != CUSTOM)
   {
      od_control.user_avatar = FALSE;
      od_control.user_rip = FALSE;
      od_control.user_attribute = 0x06;
      od_control.user_screen_length = 23;
      od_control.user_screenwidth = 80;
      od_control.od_page_pausing = TRUE;
      od_control.od_page_len = 15;
   }
   else
   {
      /* When a custom drop file is being used, there are certain      */
      /* variables that we must initialize if they weren't already set */
      /* when the custom drop file was read.                           */
      if(od_control.user_timelimit == 0) od_control.user_timelimit = 60;
#ifdef ODPLAT_NIX
      if(od_control.port == -1) od_control.baud = 1L;
#else
      if(od_control.port == -1) od_control.baud = 0L;
#endif
   }

   /* Setup inbound local/remote buffer. */
   if(ODInQueueAlloc(&hODInputQueue, od_control.od_in_buf_size == 0 ?
      DEFAULT_EVENT_QUEUE_SIZE : od_control.od_in_buf_size) != kODRCSuccess)
   {
      goto malloc_error;
   }

   /* Enable user's keyboard by default. */
   od_control.od_user_keyboard_on = TRUE;

   /* If door information (drop) file reading has been disabled, then */
   /* don't attempt to read any drop file.                            */
   if(od_control.od_disable & DIS_INFOFILE)
   {
      od_control.od_info_type = NO_DOOR_FILE;
   }

   /* Otherwise, if the local mode override has been explicitly asked for, */
   /* setup od_control for default local mode operation.                   */
   else if(od_control.od_force_local)
   {
force_local:
      /* No door information file is being used. */
      od_control.od_info_type = NO_DOOR_FILE;

      /* Operate in local mode. */
#ifdef ODPLAT_NIX
      od_control.baud = 1L;
#else
      od_control.baud = 0L;
#endif

      if(!bParsedCmdLine)
      {
         /* Enable ANSI mode. */
         od_control.user_ansi = TRUE;

         /* Default to 60 minutes of time available. */
         od_control.user_timelimit = 60;
      }

      /* Choose the appropriate system name. */
      if(bSystemNameSet)
      {
         strcpy(od_control.user_location, szForcedSystemName);
      }
      else if(od_control.system_name[0] != '\0')
      {
         strcpy(od_control.user_location, od_control.system_name);
      }
      else
      {
         strcpy(od_control.user_location, "Unknown Location");
      }
   }

   /* If drop file reading isn't disable, if a custom format drop file */
   /* hasn't already been read and automatic local mode hasn't been    */
   /* specified, then attempt to find and read a standard drop file.   */
   else if(od_control.od_info_type != CUSTOM)
   {
      /* Generate the name of the dorinfox.def file for this node. */
      if(od_control.od_node > 35)
      {
         apszDropFileNames[1] = (char *)"dorinfo1.def";
      }
      else if(od_control.od_node > 9)
      {
         sprintf(szIFTemp, "dorinfo%c.def", od_control.od_node + 55);
         apszDropFileNames[1] = (char *)szIFTemp;
      }
      else
      {
         sprintf(szIFTemp, "dorinfo%d.def", od_control.od_node);
         apszDropFileNames[1] = (char *)szIFTemp;
      }

      nFound = FOUND_NONE;

      if(!ODFileAccessMode(od_control.info_path, 4))
      {
         /* Check for a DORINFOx.DEF filename. */
         if(ODStringHasTail(od_control.info_path, ".def") &&
            strlen(od_control.info_path) >= strlen(apszDropFileNames[2]) &&
            strnicmp((char *)&od_control.info_path +
            (strlen(od_control.info_path) - 12), "dorinfo", 7) == 0)
         {
            nFound = FOUND_DORINFO1_DEF;
            strcpy(szDropFilePath, od_control.info_path);
         }
         else
         {
            /* Check filenames other than DORINFOx.DEF */
            for(btCount = 0; btCount < DIM(apszDropFileNames); ++btCount)
            {
               if(ODStringHasTail(od_control.info_path,
                  apszDropFileNames[btCount]))
               {
                  strcpy(szDropFilePath, od_control.info_path);
                  nFound = btCount;
                  break;
               }
            }
         }
      }

      /* Search for a door information file. */
      if(nFound == FOUND_NONE)
      {
         nFound = ODSearchForDropFile((char **)&apszDropFileNames,
            DIM(apszDropFileNames), (char *)&szDropFilePath,
            (char *)&szExitinfoBBSPath);
      }

      if(nFound == FOUND_EXITINFO_BBS)
      {
         od_control.od_info_type = NO_DOOR_FILE;
         ODInitReadExitInfo();
         if(od_control.od_info_type == NO_DOOR_FILE)
         {
            goto DropFileFail;
         }

         ODMakeFilename(szODWorkString, szExitinfoBBSPath, "dorinfo1.def",
            sizeof(szExitinfoBBSPath));
         if((pfDropFile = fopen(szODWorkString, "r")) == NULL)
         {
            goto DropFileFail;
         }

         goto read_dorinfox;
      }

      else if(nFound==FOUND_DORINFO1_DEF)
      {
          /* Open DORINFO?.DEF. */
          if((pfDropFile = fopen(szDropFilePath, "r")) == NULL) goto DropFileFail;

          /* Set door type to DORINFO. */
          od_control.od_info_type = DORINFO1;

read_dorinfox:
          /* If not able to read first line. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL)
          {
             goto DropFileFail;
          }

          if(szIFTemp[strlen(szIFTemp) - 1] == '\n')
          {
            szIFTemp[strlen(szIFTemp) - 1] = '\0';
          }
          if(szIFTemp[strlen(szIFTemp) - 1] == '\r')
          {
            szIFTemp[strlen(szIFTemp) - 1] = '\0';
          }
          strncpy(od_control.system_name, szIFTemp, 39);
          od_control.system_name[39] = '\0';

          /* get sysop name from DORINFO1.DEF */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          ODStringToName(szIFTemp);
          strncpy(od_control.sysop_name, szIFTemp, 19);

                                          /* get sysop's last name */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          ODStringToName(szIFTemp);
          if(strlen(szIFTemp))
          {
             strcat(od_control.sysop_name," ");
             strncat(od_control.sysop_name,szIFTemp,19);
          }
                                   /* get com port that modem is connected to */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.port=szIFTemp[3]-'1';
                                          /* determine BPS rate of connection */
          if(fgets((char *)apszDropFileInfo[0],255,pfDropFile)==NULL) goto DropFileFail;
#ifdef ODPLAT_NIX
          od_control.baud= (od_control.port == -1) ? 1 : atol((char *)apszDropFileInfo[0]);
#else
          od_control.baud= (od_control.port == -1) ? 0 : atol((char *)apszDropFileInfo[0]);
#endif

          if(fgets((char *)apszDropFileInfo[1],80,pfDropFile)==NULL) goto DropFileFail;

                                          /* get user's first name */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          ODStringToName(szIFTemp);
          strncpy(od_control.user_name,szIFTemp,17);
                                          /* get user's last name */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          ODStringToName(szIFTemp);
          if(strlen(szIFTemp))
          {
             strcat(od_control.user_name," ");
             strncat(od_control.user_name,szIFTemp,17);
          }
                                          /* get user's location */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          ODStringToName(szIFTemp);
          strncpy(od_control.user_location,szIFTemp,25);
                                          /* get ANSI mode settings */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(szIFTemp[0]=='0') od_control.user_ansi=FALSE;
          else od_control.user_ansi=TRUE;
                                          /* get user security level */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_security=atoi(szIFTemp);
                                          /* get time left in door */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_timelimit=atoi(szIFTemp);
          fclose(pfDropFile);
       }
                                       /* Read CHAIN.TXT */
       else if(nFound==FOUND_CHAIN_TXT)
       {
          if((pfDropFile=fopen(szDropFilePath,"r"))==NULL) goto DropFileFail;

          od_control.od_info_type=CHAINTXT;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_num=atoi(szIFTemp);

          if(fgets((char *)&od_control.user_handle,35,pfDropFile)==NULL) goto DropFileFail;
          ODStringToName(od_control.user_handle);

          if(fgets((char *)&od_control.user_name,35,pfDropFile)==NULL) goto DropFileFail;
          ODStringToName(od_control.user_name);

          if(fgets((char *)&od_control.user_callsign,12,pfDropFile)==NULL) goto DropFileFail;
          ODStringToName(od_control.user_callsign);

          if(fgets((char *)apszDropFileInfo[0],80,pfDropFile)==NULL) goto DropFileFail;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_sex=szIFTemp[0];

          if(fgets((char *)apszDropFileInfo[1],80,pfDropFile)==NULL) goto DropFileFail;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          strncpy(od_control.user_lastdate,szIFTemp,8);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_screenwidth=atoi(szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_screen_length=atoi(szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_security=atoi(szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          bIsSysop=atoi(szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          bIsCoSysop=atoi(szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_ansi=atoi(szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;    /* non-zero if remote */

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_timelimit=atoi(szIFTemp);
          od_control.user_timelimit/=60;

          if(fgets((char *)apszDropFileInfo[3],80,pfDropFile)==NULL) goto DropFileFail;

          if(fgets((char *)apszDropFileInfo[4],80,pfDropFile)==NULL) goto DropFileFail;

          if(fgets((char *)apszDropFileInfo[5],80,pfDropFile)==NULL) goto DropFileFail;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(strcmp(szIFTemp,"KB")==0)
          {
#ifdef ODPLAT_NIX
             od_control.baud=1;
#else
             od_control.baud=0;
#endif
          }
          else
          {
             od_control.baud=atol(szIFTemp);
          }

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.port=atoi(szIFTemp)-1;

          if(fgets((char *)apszDropFileInfo[6],80,pfDropFile)==NULL) goto DropFileFail;

          if(fgets((char *)&od_control.user_password,15,pfDropFile)==NULL) goto DropFileFail;
          ODStringToName(od_control.user_password);

          if(fgets((char *)apszDropFileInfo[2],80,pfDropFile)==NULL) goto DropFileFail;
          if(fgets((char *)apszDropFileInfo[7],80,pfDropFile)==NULL) goto DropFileFail;
          if(fgets((char *)apszDropFileInfo[8],80,pfDropFile)==NULL) goto DropFileFail;
          if(fgets((char *)apszDropFileInfo[9],80,pfDropFile)==NULL) goto DropFileFail;
          if(fgets((char *)apszDropFileInfo[10],80,pfDropFile)==NULL) goto DropFileFail;
          if(fgets((char *)apszDropFileInfo[11],80,pfDropFile)==NULL) goto DropFileFail;
          if(fgets((char *)apszDropFileInfo[12],80,pfDropFile)==NULL) goto DropFileFail;

          fclose(pfDropFile);
       }

       else if(nFound == FOUND_SFDOORS_DAT 
			 || nFound == FOUND_SFMAIN_DAT 
			 || nFound == FOUND_SFFILE_DAT 
			 || nFound == FOUND_SFMESS_DAT
          || nFound == FOUND_SFSYSOP_DAT)
       {
          od_control.od_info_type = SFDOORSDAT;

          if(!ODInitReadSFDoorsDAT())
          {
             goto DropFileFail;
          }
       }

       else if(nFound==FOUND_DOOR_SYS)
       {
          if((pfDropFile=fopen(szDropFilePath,"r"))==NULL) goto DropFileFail;

          /* Read line 1. */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;

          if(szIFTemp[0]=='C' && szIFTemp[1]=='O' && szIFTemp[2]=='M' && szIFTemp[4]==':')
          {                            /* GAP style DOOR.SYS */
             od_control.od_info_type=DOORSYS_GAP;

             od_control.port=szIFTemp[3]-'1';

             /* Check for COM0:STDIO */
             if(!strncmp(szIFTemp,"COM0:STDIO",10))
                od_control.od_com_method=COM_STDIO;

             /* Check for COM0:SOCKET### */
             if(!strncmp(szIFTemp,"COM0:SOCKET",11)) {
                od_control.od_com_method=COM_SOCKET;
		od_control.od_use_socket = TRUE;
                od_control.od_open_handle=atoi(szIFTemp+11);
             }

             /* Check for COM0:HANDLE### */
             if(!strncmp(szIFTemp,"COM0:HANDLE",11)) {
                od_control.od_com_method=COM_WIN32;
                od_control.od_open_handle=atoi(szIFTemp+11);
             }

             /* Read line 2. */
             if(fgets((char *)apszDropFileInfo[0], 80, pfDropFile) == NULL)
             {
               goto DropFileFail;
             }
             od_control.od_connect_speed = atol(apszDropFileInfo[0]);

             /* Read line 3. */
             if(fgets((char *)apszDropFileInfo[1],80,pfDropFile)==NULL) goto DropFileFail;

             /* Read line 4. */
             if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
             od_control.od_node = atoi(szIFTemp);

             /* Read line 5. */
             if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
             strupr(szIFTemp);
             if(strchr(szIFTemp, 'N') != NULL)
             {
                btDoorSYSLock = 1;
                od_control.baud = atol(apszDropFileInfo[0]);
             }
             else if(strchr(szIFTemp, 'Y') != NULL)
             {
                btDoorSYSLock = 2;
                od_control.baud = 19200;
             }
             else
             {
                od_control.baud = atol(szIFTemp);
             }

#ifdef ODPLAT_NIX
             if(od_control.port == -1) od_control.baud = 1L;
#else
             if(od_control.port == -1) od_control.baud = 0L;
#endif

             /* Read line 6. */
             if(fgets((char *)apszDropFileInfo[3],80,pfDropFile)==NULL) goto DropFileFail;

             /* Read line 7. */
             if(fgets((char *)apszDropFileInfo[4],80,pfDropFile)==NULL) goto DropFileFail;

             /* Read line 8. */
             if(fgets((char *)apszDropFileInfo[5],80,pfDropFile)==NULL) goto DropFileFail;

             /* Read line 9. */
             if(fgets((char *)apszDropFileInfo[22],80,pfDropFile)==NULL) goto DropFileFail;

             /* Read line 10. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             szIFTemp[35]='\0';
             ODStringToName(szIFTemp);
             strcpy(od_control.user_name,szIFTemp);

             /* Read line 11. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             szIFTemp[25]='\0';
             if(szIFTemp[strlen(szIFTemp)-1]=='\n') szIFTemp[strlen(szIFTemp)-1]='\0';
             if(szIFTemp[strlen(szIFTemp)-1]=='\r') szIFTemp[strlen(szIFTemp)-1]='\0';
             strcpy(od_control.user_location,szIFTemp);

             /* Read line 12. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             szIFTemp[15]='\0';
             ODStringToName(szIFTemp);
             strcpy(od_control.user_homephone,szIFTemp);

             /* Read line 13. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             szIFTemp[15]='\0';
             ODStringToName(szIFTemp);
             strcpy(od_control.user_dataphone,szIFTemp);

             /* Read line 14. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             szIFTemp[15]='\0';
             if(szIFTemp[strlen(szIFTemp)-1]=='\n') szIFTemp[strlen(szIFTemp)-1]='\0';
             if(szIFTemp[strlen(szIFTemp)-1]=='\r') szIFTemp[strlen(szIFTemp)-1]='\0';
             strcpy(od_control.user_password,szIFTemp);

             /* Read line 15. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.user_security=atoi(szIFTemp);

             /* Read line 16. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.user_numcalls=atoi(szIFTemp);

             /* Read line 17. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             szIFTemp[15]='\0';
             ODStringToName(szIFTemp);
             strcpy(od_control.user_lastdate,szIFTemp);

             /* Read line 18. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;

             /* Read line 19. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.user_timelimit=atoi(szIFTemp);

             /* Read line 20. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             strupr(szIFTemp);
             if(!strcmp(szIFTemp,"RIP"))
             {
                od_control.user_rip=TRUE;
                od_control.user_ansi=TRUE;
             }
             else if(szIFTemp[0]=='G')
             {
                od_control.user_rip=FALSE;
                od_control.user_ansi=TRUE;
             }
             else
             {
                od_control.user_rip=FALSE;
                od_control.user_ansi=FALSE;
             }

             /* Read line 21. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.user_screen_length=atoi(szIFTemp);

             /* Read line 22. */
             if(fgets((char *)apszDropFileInfo[8],80,pfDropFile)==NULL) goto DropFileFail;

             /* Read line 23. */
             if(fgets((char *)apszDropFileInfo[9],80,pfDropFile)==NULL) goto DropFileFail;
             if(apszDropFileInfo[9][strlen(apszDropFileInfo[9])-1]!='\n')
             {
                int ch;
                apszDropFileInfo[9][strlen(apszDropFileInfo[9])-1]='\n';
                do
                {
                   ch = fgetc(pfDropFile);
                } while(ch != '\n' && ch != EOF);
             }

             /* Read line 24. */
again:
             if(fgets((char *)apszDropFileInfo[10],80,pfDropFile)==NULL) goto DropFileFail;
             if(strchr(apszDropFileInfo[10],',')!=NULL) goto again;

             /* Read line 25. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             szIFTemp[15]='\0';
             ODStringToName(szIFTemp);
             strcpy(od_control.user_subdate,szIFTemp);

             /* Read line 26. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.user_num=atoi(szIFTemp);

             /* Read line 27. */
             if(fgets((char *)apszDropFileInfo[6],80,pfDropFile)==NULL) goto DropFileFail;

             /* Read line 28. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.user_uploads=atoi(szIFTemp);

             /* Read line 29. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.user_downloads=atoi(szIFTemp);

             /* Read line 30. */
             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.user_todayk=atoi(szIFTemp);

             /* Read line 31. */
             if(fgets((char *)apszDropFileInfo[21],80,pfDropFile)==NULL) goto DropFileFail;


                                        /* Beginning of extending DOOR.SYS data */
             /* Read line 32. */
             fgets((char *)apszDropFileInfo[7],80,pfDropFile);
             /* Read line 33. */
             fgets((char *)apszDropFileInfo[11],80,pfDropFile);
             /* Read line 34. */
             fgets((char *)apszDropFileInfo[12],80,pfDropFile);
             /* Read line 35. */
             fgets((char *)apszDropFileInfo[13],80,pfDropFile);
             /* Read line 36. */
             if(fgets((char *)apszDropFileInfo[14],80,pfDropFile)!=NULL)
             {
                /* Read line 37. */
                strncpy(od_control.user_birthday,apszDropFileInfo[7],8);
                od_control.user_birthday[8]='\0';

                /* Read line 38. */
                strncpy(od_control.sysop_name,apszDropFileInfo[13],39);
                od_control.sysop_name[39]='\0';
                ODStringToName(od_control.sysop_name);

                /* Read line 39. */
                strncpy(od_control.user_handle,apszDropFileInfo[14],35);
                od_control.user_handle[35]='\0';
                ODStringToName(od_control.user_handle);

                /* Read line 40. */
                if(fgets(szIFTemp,255,pfDropFile)==NULL) goto finished;
                strncpy(od_control.event_starttime, szIFTemp, 5);
                od_control.event_starttime[5] = '\0';

                /* Read line 41. */
                if(fgets(szIFTemp,255,pfDropFile)==NULL) goto finished;
                if(szIFTemp[0]=='y' || szIFTemp[0]=='Y')
                   od_control.user_error_free=TRUE;
                else
                   od_control.user_error_free=FALSE;

                /* Read line 42. */
                if(fgets((char *)apszDropFileInfo[7],80,pfDropFile)==NULL) goto finished;
                /* Read line 43. */
                if(fgets((char *)apszDropFileInfo[13],80,pfDropFile)==NULL) goto finished;
                /* Read line 44. */
                if(fgets((char *)apszDropFileInfo[14],80,pfDropFile)==NULL) goto finished;
                /* Read line 45. */
                if(fgets((char *)apszDropFileInfo[15],80,pfDropFile)==NULL) goto finished;
                /* Read line 46. */
                if(fgets((char *)apszDropFileInfo[16],80,pfDropFile)==NULL) goto finished;

                /* Read line 47. */
                if(fgets(szIFTemp,255,pfDropFile)==NULL) goto finished;
                strncpy(od_control.user_logintime,szIFTemp,5);
                od_control.user_logintime[5]='\0';

                /* Read line 48. */
                if(fgets(szIFTemp,255,pfDropFile)==NULL) goto finished;
                strncpy(od_control.user_lasttime,szIFTemp,5);
                od_control.user_lasttime[5]='\0';

                /* Read line 49. */
                if(fgets((char *)apszDropFileInfo[18],80,pfDropFile)==NULL) goto finished;

                /* Read line 50. */
                if(fgets((char *)apszDropFileInfo[19],80,pfDropFile)==NULL) goto finished;

                /* Read line 51. */
                if(fgets(szIFTemp,255,pfDropFile)==NULL) goto finished;
                od_control.user_upk=atoi(szIFTemp);

                /* Read line 52. */
                if(fgets(szIFTemp,255,pfDropFile)==NULL) goto finished;
                od_control.user_downk=atoi(szIFTemp);

                /* Read line 53. */
                if(fgets(szIFTemp,255,pfDropFile)==NULL) goto finished;
                strncpy(od_control.user_comment,szIFTemp,79);
                od_control.user_comment[79]='\0';
                if(od_control.user_comment[strlen(od_control.user_comment)-1]=='\n')
                   od_control.user_comment[strlen(od_control.user_comment)-1]='\0';
                if(od_control.user_comment[strlen(od_control.user_comment)-1]=='\r')
                   od_control.user_comment[strlen(od_control.user_comment)-1]='\0';

                /* Read line 54. */
                if(fgets((char *)apszDropFileInfo[20],80,pfDropFile)==NULL) goto finished;

                /* Read line 55. */
                if(fgets(szIFTemp,255,pfDropFile)==NULL) goto finished;
                od_control.user_messages=atoi(szIFTemp);


                od_control.od_info_type=DOORSYS_WILDCAT;
             }
          }

          else                            /* DoorWay style DOOR.SYS */
          {
             od_control.od_info_type=DOORSYS_DRWY;

             szIFTemp[35]='\0';
             ODStringToName(szIFTemp);
             strcpy(od_control.user_name,szIFTemp);

             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.port=szIFTemp[0]-'1';

             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             if(od_control.port==-1)
             {
#ifdef ODPLAT_NIX
                od_control.baud=1L;
#else
                od_control.baud=0L;
#endif
             }
             else
             {
                od_control.baud=atol(szIFTemp);
             }

             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             od_control.user_timelimit=atoi(szIFTemp);

             if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
             if(szIFTemp[0]=='G')
             {
                od_control.user_ansi=TRUE;
             }
             else
             {
                od_control.user_ansi=FALSE;
             }
          }
finished:
          fclose(pfDropFile);
       }

       else if(nFound==FOUND_CALLINFO_BBS)
       {
          if((pfDropFile=fopen(szDropFilePath,"r"))==NULL) goto DropFileFail;

          od_control.od_info_type=CALLINFO;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          szIFTemp[35]='\0';
          ODStringToName(szIFTemp);
          strcpy(od_control.user_name,szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          szIFTemp[25]='\0';
          ODStringToName(szIFTemp);
          strcpy(od_control.user_location,szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_security=atoi(szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_timelimit=atoi(szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(szIFTemp[0]=='M')
          {
             od_control.user_ansi=FALSE;
          }
          else
          {
             od_control.user_ansi=TRUE;
          }

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          szIFTemp[15]='\0';
          ODStringToName(szIFTemp);
          strcpy(od_control.user_password,szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          szIFTemp[15]='\0';
          ODStringToName(szIFTemp);
          strcpy(od_control.user_homephone,szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_screen_length=atoi(szIFTemp);

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.port=szIFTemp[3]-'1';

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;

          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.baud=atol(szIFTemp);

          fclose(pfDropFile);
       }
       else if(nFound == FOUND_TRIBBS_SYS)
       {
          /* We have found reading a TRIBBS.SYS file. */

          /* Attempt to open the file. */
          if((pfDropFile = fopen(szDropFilePath, "r")) == NULL)
          {
             goto DropFileFail;
          }

          /* Record the drop file type being used. */
          od_control.od_info_type = TRIBBSSYS;

          /* Read line 1: User's number. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.user_num = atoi(szIFTemp);

          /* Read line 2: User's name. */
          if(fgets((char *)&od_control.user_name, 35, pfDropFile) == NULL)
          {
             goto DropFileFail;
          }
          ODStringToName(od_control.user_name);

          /* Read line 3: User's password. */
          if(fgets((char *)&od_control.user_password, 15, pfDropFile) == NULL)
          {
             goto DropFileFail;
          }

          /* Read line 4: User's security level. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.user_security = atoi(szIFTemp);

          /* Read line 5: "Y" for expert mode, "N" for novice mode. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.user_expert = (szIFTemp[0] == 'Y');

          /* Read line 6: "Y" for ANSI, "N" for TTY. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.user_ansi = (szIFTemp[0] == 'Y');

          /* Read line 7: Minutes remaining for this call. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.user_timelimit = atoi(szIFTemp);

          /* Read line 8: User's phone number. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          szIFTemp[15] = '\0';
          ODStringToName(szIFTemp);
          strcpy(od_control.user_homephone, szIFTemp);

          /* Read line 9: User's location. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          ODStringToName(szIFTemp);
          strncpy(od_control.user_location, szIFTemp, 25);

          /* Read line 10: User's birthday. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          strncpy(od_control.user_birthday, szIFTemp, 8);
          od_control.user_birthday[8] = '\0';
          od_control.user_birthday[2] = '-';
          od_control.user_birthday[5] = '-';

          /* Read line 11: Node number. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.od_node = atoi(szIFTemp);

          /* Read line 12: Serial port. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.port = atoi(szIFTemp) - 1;

          /* Read line 13: Baud rate. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.od_connect_speed =
             (od_control.port == -1) ? 0 : atol(szIFTemp);

          /* Read line 14: Locked baud rate or 0 for unlocked port. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.baud = atol(szIFTemp);
          if(od_control.baud == 0)
          {
             od_control.baud = od_control.od_connect_speed;
          }

          /* Read line 15: "Y" for RTS/CTS handshaking, "N" for none. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.od_com_flow_control = (szIFTemp[0] == 'Y')
             ? COM_RTSCTS_FLOW : COM_NO_FLOW;

          /* Read line 16: "Y" for error free connection, "N" otherwise. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.user_error_free = (szIFTemp[0] == 'Y');

          /* Read line 17: BBS name. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          ODStringCopy(od_control.system_name, szIFTemp, 40);
          ODStringToName(od_control.system_name);

          /* Read line 18: Sysop's name. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          ODStringCopy(od_control.sysop_name, szIFTemp, 40);
          ODStringToName(od_control.sysop_name);

          /* Read line 19: User's alias. */
          if(fgets((char *)&od_control.user_handle, 35, pfDropFile) == NULL)
          {
             goto DropFileFail;
          }
          ODStringToName(od_control.user_handle);

          /* Read line 20: "Y" for RIP mode, "N" for no RIP mode. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.user_rip = (szIFTemp[0] == 'Y');

          /* Now, close the TRIBBS.SYS file. */
          fclose(pfDropFile);
       }
       else if(nFound == FOUND_DOOR32_SYS)
       {
          /* We have found a DOOR32.SYS file. */

          /* Attempt to open the file. */
          if((pfDropFile = fopen(szDropFilePath, "r")) == NULL)
          {
             goto DropFileFail;
          }

          /* Record the drop file type being used. */
          od_control.od_info_type = DOOR32SYS;

          /* Read line 1: Comm type. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          switch(atoi(szIFTemp)) {
				case 0: /* local */
					od_control.od_force_local = TRUE;	/* Is this the right way? */
					break;
				case 1: /* serial */
					break;
				case 2: /* telnet */
					od_control.od_use_socket = TRUE;
					break;
			 }
			 /* Read line 2: Comm or Socket handle. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.od_open_handle	= atoi(szIFTemp);

			 /* Read line 3: Baud rate */
          if(fgets((char *)apszDropFileInfo[0],255,pfDropFile)==NULL) goto DropFileFail;
#ifdef ODPLAT_NIX
          od_control.baud= (od_control.port == -1) ? 1 : atol((char *)apszDropFileInfo[0]);
#else
          od_control.baud= (od_control.port == -1) ? 0 : atol((char *)apszDropFileInfo[0]);
#endif

			 /* Read line 4: BBS Software name and version - unused. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;

          /* Read line 5: User's number. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.user_num = atoi(szIFTemp);

          /* Read line 6: User's real name. */
          if(fgets((char *)&od_control.user_name, 35, pfDropFile) == NULL)
          {
             goto DropFileFail;
          }
          ODStringToName(od_control.user_name);

          /* Read line 7: User's alias. */
          if(fgets((char *)&od_control.user_handle, 35, pfDropFile) == NULL)
          {
             goto DropFileFail;
          }
          ODStringToName(od_control.user_handle);

          /* Read line 8: User's security level */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
          od_control.user_security=atoi(szIFTemp);

          /* Read line 9: Minutes remaining for this call. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.user_timelimit = atoi(szIFTemp);

          /* Read line 10: User's Terminal type  */
          if(fgets(szIFTemp,255,pfDropFile)==NULL) goto DropFileFail;
    		 od_control.user_ansi=TRUE;
			 switch(atoi(szIFTemp)) {
					case 0: /* ASCII */
						od_control.user_ansi=FALSE;
						break;
					case 1: /* ANSI */
						od_control.user_ansi=TRUE;
						break;
					case 2: /* AVATAR */
						od_control.user_avatar=TRUE;
						break;
					case 3: /* RIP */
						od_control.user_rip=TRUE;
						break;
			 }

          /* Read line 11: Current Node number. */
          if(fgets(szIFTemp, 255, pfDropFile) == NULL) goto DropFileFail;
          od_control.od_node = atoi(szIFTemp);

          /* Now, close the DOOR32.SYS file. */
          fclose(pfDropFile);
       }
       else
       {
DropFileFail:
			 if(pfDropFile!=NULL)	/* Let's not leave the file open */
				 fclose(pfDropFile);

          od_control.od_info_type = NO_DOOR_FILE;

          if(od_control.od_no_file_func != NULL)
          {
             (*od_control.od_no_file_func)();
          }

          /* Check whether force local has been turned on by the no */
          /* file function.                                         */
          if(od_control.od_force_local)
          {
             goto force_local;
          }

          /* Exit with failure if the no file function has not read a */
          /* door information file itself.                            */
          if(od_control.od_info_type == NO_DOOR_FILE
            && (wPreSetInfo & PRESET_REQUIRED) != PRESET_REQUIRED)
          {
             ODInitError("Unable to read door information (drop) file.");
             exit(od_control.od_errorlevel[1]);
          }
       }
   }

   ODInitPartTwo();
}


/* ----------------------------------------------------------------------------
 * ODInitReadSFDoorsDAT()                              *** PRIVATE FUNCTION ***
 *
 * Reads an SFDOORS.DAT format drop file.
 *
 * Parameters: none
 *
 *     Return: TRUE on success or FALSE on failure.
 */
static BOOL ODInitReadSFDoorsDAT(void)
{
   FILE *pfDropFile;

   if((pfDropFile=fopen(szDropFilePath,"r"))==NULL) return(FALSE);

   /* Line 1: User number. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_num=atoi(szIFTemp);

   /* Line 2: User name. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   szIFTemp[35]='\0';
   ODStringToName(szIFTemp);
   strcpy(od_control.user_name,szIFTemp);

   /* Line 3: User password. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   szIFTemp[15]='\0';
   ODStringToName(szIFTemp);
   strcpy(od_control.user_password,szIFTemp);

   /* Line 4: Unused. */
   if(fgets((char *)apszDropFileInfo[0],80,pfDropFile)==NULL) return(FALSE);

   /* Line 5: Modem <-> Serial port bps rate. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.baud=atol(szIFTemp);

   /* Line 6: Serial port number. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.port=atoi(szIFTemp)-1;

   /* Line 7: User's time remaining. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_timelimit=atoi(szIFTemp);

   /* Line 8: Unused. */
   if(fgets((char *)apszDropFileInfo[13],80,pfDropFile)==NULL) return(FALSE);

   /* Line 9: Unused. */
   if(fgets((char *)apszDropFileInfo[14],80,pfDropFile)==NULL) return(FALSE);

   /* Line 10: User's ANSI mode setting. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   strupr(szIFTemp);
   od_control.user_ansi=(szIFTemp[0]=='T');

   /* Line 11: User's security level. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_security=atoi(szIFTemp);

   /* Line 12: User's upload count. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_uploads=atoi(szIFTemp);

   /* Line 13: User's download count. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_downloads=atoi(szIFTemp);

   /* Line 14: Unused. */
   if(fgets((char *)apszDropFileInfo[1],80,pfDropFile)==NULL) return(FALSE);

   /* Line 15: User's login time. */
   if(fgets((char *)apszDropFileInfo[2],255,pfDropFile)==NULL) return(FALSE);
   sprintf(od_control.user_logintime, "%02d:%02d",
      atoi((char *)apszDropFileInfo[2]) % 60,
      atoi((char *)apszDropFileInfo[2]) / 60);

   /* Line 16: Unused. */
   if(fgets((char *)apszDropFileInfo[3],80,pfDropFile)==NULL) return(FALSE);

   /* Line 17: Sysop next flag. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   strupr(szIFTemp);
   od_control.sysop_next=(szIFTemp[0]=='T');

   /* Line 18: Unused. */
   if(fgets((char *)apszDropFileInfo[4],80,pfDropFile)==NULL) return(FALSE);

   /* Line 19: Unused. */
   if(fgets((char *)apszDropFileInfo[5],80,pfDropFile)==NULL) return(FALSE);

   /* Line 20: Unused. */
   if(fgets((char *)apszDropFileInfo[6],80,pfDropFile)==NULL) return(FALSE);

   /* Line 21: Error free connection flag. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   strupr(szIFTemp);
   od_control.user_error_free=(szIFTemp[0]=='T');

   /* Line 22: Current message area. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_msg_area=atoi(szIFTemp);

   /* Line 23: Current file area. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_file_area=atoi(szIFTemp);

   /* Line 24: Current node number. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.od_node=atoi(szIFTemp);

   /* Line 25: Unused. */
   if(fgets((char *)apszDropFileInfo[10],80,pfDropFile)==NULL) return(FALSE);

   /* Line 26: Unused. */
   if(fgets((char *)apszDropFileInfo[11],80,pfDropFile)==NULL) return(FALSE);

   /* Line 27: Unused. */
   if(fgets((char *)apszDropFileInfo[12],80,pfDropFile)==NULL) return(FALSE);

   /* Line 28: Kilobytes downloaded today. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_todayk=atoi(szIFTemp);

   /* Line 29: Kilobytes uploaded in total. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_upk=atoi(szIFTemp);

   /* Line 30: Kilobytes downloaded in total. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   od_control.user_downk=atoi(szIFTemp);

   /* Line 31: User's home phone number. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   szIFTemp[15]='\0';
   ODStringToName(szIFTemp);
   strcpy(od_control.user_homephone,szIFTemp);

   /* Line 32: User's home location. */
   if(fgets(szIFTemp,255,pfDropFile)==NULL) return(FALSE);
   szIFTemp[25]='\0';
   ODStringToName(szIFTemp);
   strcpy(od_control.user_location,szIFTemp);

   /* Line 33: Unused. */
   if(fgets((char *)apszDropFileInfo[15],80,pfDropFile)==NULL)
   {
      apszDropFileInfo[15][0]='\0';
   }

   /* Line 34: RIP mode. */
   if(fgets(szIFTemp,255,pfDropFile) != NULL)
   {
      strupr(szIFTemp);
      od_control.user_rip = (szIFTemp[0] == 'T');
   }

   /* line 35: User wants chat. */
   if(fgets(szIFTemp,255,pfDropFile) != NULL)
   {
      strupr(szIFTemp);
      od_control.user_wantchat = (szIFTemp[0] == 'T');
   }

   /* Line 36: Unused. */
   if(fgets((char *)apszDropFileInfo[17],80,pfDropFile)==NULL)
   {
      apszDropFileInfo[17][0]='\0';
   }

   /* Line 37: IRQ number. */
   if(fgets(szIFTemp,255,pfDropFile) != NULL)
   {
      od_control.od_com_irq = atoi(szIFTemp);
   }

   /* Line 38: Serial port address. */
   if(fgets(szIFTemp,255,pfDropFile) != NULL)
   {
      od_control.od_com_address = atoi(szIFTemp);
   }

   /* Line 39: Unused. */
   if(fgets((char *)apszDropFileInfo[18],80,pfDropFile)==NULL)
   {
      apszDropFileInfo[18][0]='\0';
   }

   fclose(pfDropFile);

   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODInitReadExitInfo()                                *** PRIVATE FUNCTION ***
 *
 * Reads an EXITINFO.BBS format drop file.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODInitReadExitInfo(void)
{
   DWORD dwFileSize;
   FILE *pfDropFile;
   BYTE btCount;

   od_control.od_extended_info=FALSE;
   od_control.od_ra_info=FALSE;

   /* Try to open EXITINFO.BBS. */
   ODMakeFilename(szODWorkString, szExitinfoBBSPath, "exitinfo.bbs",
      sizeof(szExitinfoBBSPath));
   if((pfDropFile = fopen(szODWorkString, "rb")) != NULL)
   {
      dwFileSize = ODFileSize(pfDropFile);

      if(dwFileSize >= 2363)
      {
         if((pRA2ExitInfoRecord=malloc(sizeof(tRA2ExitInfoRecord)))!=NULL)
         {
            if(fread(pRA2ExitInfoRecord,1,2363,pfDropFile)==2363)
            {
               od_control.od_ra_info=TRUE;
               od_control.od_extended_info=TRUE;
               od_control.od_info_type=RA2EXITINFO;

               od_control.baud=(long)pRA2ExitInfoRecord->baud;
               od_control.system_calls=pRA2ExitInfoRecord->num_calls;
               ODStringPascalToC(od_control.system_last_caller,pRA2ExitInfoRecord->last_caller,35);
               ODStringPascalToC(od_control.system_last_handle,pRA2ExitInfoRecord->sLastHandle,35);
               ODStringPascalToC(od_control.timelog_start_date,pRA2ExitInfoRecord->start_date,8);
               memcpy(&od_control.timelog_busyperhour,&pRA2ExitInfoRecord->busyperhour,62);
               ODStringPascalToC(od_control.user_name,pRA2ExitInfoRecord->name,35);
               ODStringPascalToC(od_control.user_location,pRA2ExitInfoRecord->location,25);
               ODStringPascalToC(od_control.user_org,pRA2ExitInfoRecord->organisation,50);
               for(btCount=0;btCount<3;++btCount)
                  ODStringPascalToC(od_control.user_address[btCount],pRA2ExitInfoRecord->address[btCount],50);
               ODStringPascalToC(od_control.user_handle,pRA2ExitInfoRecord->handle,35);
               ODStringPascalToC(od_control.user_comment,pRA2ExitInfoRecord->comment,80);
               od_control.user_pwd_crc=pRA2ExitInfoRecord->password_crc;
               ODStringPascalToC(od_control.user_dataphone,pRA2ExitInfoRecord->dataphone,15);
               ODStringPascalToC(od_control.user_homephone,pRA2ExitInfoRecord->homephone,15);
               ODStringPascalToC(od_control.user_lasttime,pRA2ExitInfoRecord->lasttime,5);
               ODStringPascalToC(od_control.user_lastdate,pRA2ExitInfoRecord->lastdate,8);
               od_control.user_attribute=pRA2ExitInfoRecord->attrib;
               od_control.user_attrib2=pRA2ExitInfoRecord->attrib2;
               memcpy(&od_control.user_flags,&pRA2ExitInfoRecord->flags,14);
               od_control.user_security=pRA2ExitInfoRecord->sec;
               od_control.user_lastread=pRA2ExitInfoRecord->lastread;
               memcpy(&od_control.user_numcalls,&pRA2ExitInfoRecord->nocalls,29);
               od_control.user_group=pRA2ExitInfoRecord->group;
               memcpy(&od_control.user_combinedrecord,&pRA2ExitInfoRecord->combinedrecord,200);
               ODStringPascalToC(od_control.user_firstcall,pRA2ExitInfoRecord->firstcall,8);
               ODStringPascalToC(od_control.user_birthday,pRA2ExitInfoRecord->birthday,8);
               ODStringPascalToC(od_control.user_subdate,pRA2ExitInfoRecord->subdate,8);
               od_control.user_screenwidth=pRA2ExitInfoRecord->screenwidth;
               od_control.user_language=pRA2ExitInfoRecord->language;
               od_control.user_date_format=pRA2ExitInfoRecord->dateformat;
               ODStringPascalToC(od_control.user_forward_to,pRA2ExitInfoRecord->forwardto,35);
               memcpy(&od_control.user_msg_area,&pRA2ExitInfoRecord->msgarea,15);
               od_control.user_sex = (pRA2ExitInfoRecord->sex == 1) ? 'M' : 'F';
               od_control.user_attrib3=pRA2ExitInfoRecord->btAttribute3;
               ODStringPascalToC(od_control.user_password,pRA2ExitInfoRecord->sPassword,15);
               od_control.event_status=pRA2ExitInfoRecord->status;
               ODStringPascalToC(od_control.event_starttime,pRA2ExitInfoRecord->starttime,5);
               memcpy(&od_control.event_errorlevel,&pRA2ExitInfoRecord->errorlevel,3);
               ODStringPascalToC(od_control.event_last_run,pRA2ExitInfoRecord->lasttimerun,8);
               memcpy(&od_control.user_netmailentered,&pRA2ExitInfoRecord->netmailentered,2);
               ODStringPascalToC(od_control.user_logintime,pRA2ExitInfoRecord->logintime,5);
               ODStringPascalToC(od_control.user_logindate,pRA2ExitInfoRecord->logindate,8);
               memcpy(&od_control.user_timelimit,&pRA2ExitInfoRecord->timelimit,6);
               memcpy(&od_control.user_num,&pRA2ExitInfoRecord->userrecord,8);
               ODStringPascalToC(od_control.user_timeofcreation,pRA2ExitInfoRecord->timeofcreation,5);
               od_control.user_logon_pwd_crc=pRA2ExitInfoRecord->logonpasswordcrc;
               od_control.user_wantchat=pRA2ExitInfoRecord->wantchat;
               od_control.user_deducted_time=pRA2ExitInfoRecord->deducted_time;
               for(btCount=0;btCount<50;++btCount)
                  ODStringPascalToC(od_control.user_menustack[btCount],pRA2ExitInfoRecord->menustack[btCount],8);
               od_control.user_menustackpointer=pRA2ExitInfoRecord->menustackpointer;
               memcpy(&od_control.user_error_free,&pRA2ExitInfoRecord->error_free,3);
               ODStringPascalToC(od_control.user_emsi_crtdef,pRA2ExitInfoRecord->emsi_crtdef,40);
               ODStringPascalToC(od_control.user_emsi_protocols,pRA2ExitInfoRecord->emsi_protocols,40);
               ODStringPascalToC(od_control.user_emsi_capabilities,pRA2ExitInfoRecord->emsi_capabilities,40);
               ODStringPascalToC(od_control.user_emsi_requests,pRA2ExitInfoRecord->emsi_requests,40);
               ODStringPascalToC(od_control.user_emsi_software,pRA2ExitInfoRecord->emsi_software,40);
               memcpy(&od_control.user_hold_attr1,&pRA2ExitInfoRecord->hold_attr1,3);
               ODStringPascalToC(od_control.user_reasonforchat,pRA2ExitInfoRecord->page_reason,77);
               btRAStatusToSet = pRA2ExitInfoRecord->status_line-1;
               ODStringPascalToC(od_control.user_last_cost_menu,pRA2ExitInfoRecord->last_cost_menu,8);
               od_control.user_menu_cost=pRA2ExitInfoRecord->menu_cost_per_min;
               od_control.user_rip=pRA2ExitInfoRecord->has_rip;
               od_control.user_rip_ver=pRA2ExitInfoRecord->btRIPVersion;

               od_control.user_ansi=od_control.user_attribute&8;
               od_control.user_avatar=od_control.user_attrib2&2;
            }

            else
            {
               free(pRA2ExitInfoRecord);
            }
         }
      }

      else if(dwFileSize>=1493)
      {
         if(ODReadExitInfoPrimitive(pfDropFile,476))
         {
            if((pExtendedExitInfo=malloc(sizeof(tExtendedExitInfo)))!=NULL)
            {
               if(fread(pExtendedExitInfo,1,sizeof(tExtendedExitInfo), pfDropFile)==sizeof(tExtendedExitInfo))
               {                 /* transfer info into od_control struct */
                  ODStringPascalToC(od_control.user_timeofcreation,pExitInfoRecord->bbs.ra.timeofcreation,5);
                  ODStringPascalToC(od_control.user_logonpassword,pExitInfoRecord->bbs.ra.logonpassword,15);
                  od_control.user_wantchat=pExitInfoRecord->bbs.ra.wantchat;

                  od_control.user_deducted_time=pExtendedExitInfo->deducted_time;
                  for(btCount=0;btCount<50;++btCount)
                  {
                     ODStringPascalToC(od_control.user_menustack[btCount],pExtendedExitInfo->menustack[btCount],8);
                  }
                  od_control.user_menustackpointer=pExtendedExitInfo->menustackpointer;
                  ODStringPascalToC(od_control.user_handle,pExtendedExitInfo->userhandle,35);
                  ODStringPascalToC(od_control.user_comment,pExtendedExitInfo->comment,80);
                  ODStringPascalToC(od_control.user_firstcall,pExtendedExitInfo->firstcall,8);
                  memcpy(od_control.user_combinedrecord,pExtendedExitInfo->combinedrecord,25);
                  ODStringPascalToC(od_control.user_birthday,pExtendedExitInfo->birthday,8);
                  ODStringPascalToC(od_control.user_subdate,pExtendedExitInfo->subdate,8);
                  od_control.user_screenwidth=pExtendedExitInfo->screenwidth;
                  od_control.user_msg_area=pExtendedExitInfo->msgarea;
                  od_control.user_file_area=pExtendedExitInfo->filearea;
                  od_control.user_language=pExtendedExitInfo->language;
                  od_control.user_date_format=pExtendedExitInfo->dateformat;
                  ODStringPascalToC(od_control.user_forward_to,pExtendedExitInfo->forwardto,35);
                  memcpy(&od_control.user_error_free,&pExtendedExitInfo->error_free,3);
                  ODStringPascalToC(od_control.user_emsi_crtdef,pExtendedExitInfo->emsi_crtdef,40);
                  ODStringPascalToC(od_control.user_emsi_protocols,pExtendedExitInfo->emsi_protocols,40);
                  ODStringPascalToC(od_control.user_emsi_capabilities,pExtendedExitInfo->emsi_capabilities,40);
                  ODStringPascalToC(od_control.user_emsi_requests,pExtendedExitInfo->emsi_requests,40);
                  ODStringPascalToC(od_control.user_emsi_software,pExtendedExitInfo->emsi_software,40);
                  memcpy(&od_control.user_hold_attr1,&pExtendedExitInfo->hold_attr1,3);

                  od_control.od_ra_info=TRUE;
                  od_control.od_extended_info=TRUE;
                  od_control.od_info_type=RA1EXITINFO;
               }
            }
         }
      }

      else if(dwFileSize>476)
      {
         if(dwFileSize > sizeof(tExitInfoRecord))
         {
            dwFileSize = sizeof(tExitInfoRecord);
         }

         if(ODReadExitInfoPrimitive(pfDropFile,(int)dwFileSize))
         {
            od_control.user_wantchat=pExitInfoRecord->bbs.qbbs.qwantchat;
            for(btCount=0;btCount<pExitInfoRecord->bbs.qbbs.gosublevel;++btCount)
            {
               ODStringPascalToC(od_control.user_menustack[btCount],pExitInfoRecord->bbs.qbbs.menustack[btCount],8);
            }
            od_control.user_menustackpointer=pExitInfoRecord->bbs.qbbs.gosublevel;
            ODStringPascalToC(od_control.user_menustack[od_control.user_menustackpointer],pExitInfoRecord->bbs.qbbs.menu,8);

            od_control.od_extended_info=TRUE;
            od_control.od_info_type=QBBS275EXITINFO;
            nInitialElapsed=pExitInfoRecord->elapsed;

            od_control.user_rip = pExitInfoRecord->bbs.qbbs.ripactive;
         }
      }

      else if(dwFileSize>=452)
      {
         if(ODReadExitInfoPrimitive(pfDropFile,(int)dwFileSize))
         {
            ODStringPascalToC(od_control.user_timeofcreation,pExitInfoRecord->bbs.ra.timeofcreation,5);
            ODStringPascalToC(od_control.user_logonpassword,pExitInfoRecord->bbs.ra.logonpassword,15);
            od_control.user_wantchat=pExitInfoRecord->bbs.ra.wantchat;

            od_control.od_extended_info=TRUE;
            od_control.od_info_type=EXITINFO;
         }
      }

      od_control.od_page_pausing=od_control.user_attribute&0x04;

      fclose(pfDropFile);
   }
}


/* ----------------------------------------------------------------------------
 * ODInitPartTwo()                                     *** PRIVATE FUNCTION ***
 *
 * Called by od_init() to carry on with second stage of OpenDoors
 * initialization. The sole reason that this function exists is because some
 * compilers were unable to compile this file with a huge od_init() function
 * that had this code in the main od_init() body.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODInitPartTwo(void)
{
   BYTE btCount;
#ifdef ODPLAT_NIX
   struct termios term;
   struct passwd  *uinfo;
#endif

   /* Initialize any colors that haven't already been set. */
   if(!od_control.od_list_title_col) od_control.od_list_title_col = 0x0f;
   if(!od_control.od_continue_col) od_control.od_continue_col = 0x0f;
   if(!od_control.od_list_name_col) od_control.od_list_name_col = 0x0e;
   if(!od_control.od_list_size_col) od_control.od_list_size_col = 0x0d;
   if(!od_control.od_list_comment_col) od_control.od_list_comment_col = 0x03;
   if(!od_control.od_list_offline_col) od_control.od_list_offline_col = 0x0c;
   if(!od_control.od_local_win_col) od_control.od_local_win_col = 0x19;
   if(!od_control.od_menu_title_col) od_control.od_menu_title_col = 0x74;
   if(!od_control.od_menu_border_col) od_control.od_menu_border_col = 0x70;
   if(!od_control.od_menu_text_col) od_control.od_menu_text_col = 0x70;
   if(!od_control.od_menu_key_col) od_control.od_menu_key_col = 0x7f;
   if(!od_control.od_menu_highkey_col) od_control.od_menu_highkey_col = 0x0f;
   if(!od_control.od_menu_highlight_col)
   {
      od_control.od_menu_highlight_col = 0x07;
   }

   /* Color sequence delimiter characters. */
   od_control.od_color_char = '\0';
   od_control.od_color_delimiter = '`';

   /* Enable sysop paging between the hours of 8:00am and 10:00pm. */
   od_control.od_okaytopage = PAGE_USE_HOURS;
   od_control.od_pagestartmin = 480;
   od_control.od_pageendmin = 1320;

   /* Default maximum user inactivity is 200 seconds. */
   od_control.od_inactivity = 200;

   /* Screen clearing is on by default. */
   od_control.od_always_clear = TRUE;
   
   od_control.od_inactive_warning = 10;
   od_control.od_cur_attrib = -1;

   /* Enable screen clearing on door exit. */
   od_control.od_clear_on_exit = TRUE;

   /* RA/QBBS control codes are now disabled by default. */
   od_control.od_no_ra_codes = TRUE;

   if(od_control.od_chat_color1 == 0) od_control.od_chat_color1 = 0x0c;
   if(od_control.od_chat_color2 == 0) od_control.od_chat_color2 = 0x0f;

   /* Set default messages and prompts. */
   od_control.od_before_shell = "\n\rPlease wait a moment...\n\r";
   od_control.od_after_shell = "\n\r...Thanks for waiting\n\r\n\r";
   od_control.od_help_text = "  Alt: [C]hat [H]angup [L]ockout [J]Dos [K]eyboard-Off [D]rop to BBS            ";
   od_control.od_before_chat = "\n\rThe system operator has placed you in chat mode to talk with you:\n\r\n\r";
   od_control.od_after_chat = "\n\rChat mode ended.\n\r\n\r";
   od_control.od_inactivity_timeout = "\n\rMaximum user inactivity time has elapsed, please call again.\n\r\n\r";
   od_control.od_inactivity_warning = "\n\rWARNING: Inactivity timeout in 10 seconds, press a key now to remain online.\n\r\n\r";
   od_control.od_time_warning = "\n\rWARNING: You only have %d minute(s) remaining for this session.\n\r\n\r";
   od_control.od_time_left = "%d mins   ";
   od_control.od_sysop_next = "[SN] ";
   od_control.od_no_keyboard = "[Keyboard]";
   od_control.od_want_chat = "[Want-Chat]";
   od_control.od_no_time = "\n\rSorry, you have used up of your time for this session.\n\r\n\r";
   od_control.od_no_sysop = "\n\rSorry, the system operator is not available at this time.\n\r";
   od_control.od_press_key = "Press [Enter] to continue";
   od_control.od_chat_reason = "               Why would you like to chat? (Blank line to cancel)\n\r";
   od_control.od_paging = "\n\rPaging system operator for chat";
   od_control.od_no_response = " No response.\n\r\n\r";
   od_control.od_status_line[0] = "                                                                     [Node:     ";
   od_control.od_status_line[1] = "%s of %s at %lu BPS";
   od_control.od_status_line[2] = "Security:        Time:                                               [F9]=Help ";
   od_control.od_month[0] = "Jan";
   od_control.od_month[1] = "Feb";
   od_control.od_month[2] = "Mar";
   od_control.od_month[3] = "Apr";
   od_control.od_month[4] = "May";
   od_control.od_month[5] = "Jun";
   od_control.od_month[6] = "Jul";
   od_control.od_month[7] = "Aug";
   od_control.od_month[8] = "Sep";
   od_control.od_month[9] = "Oct";
   od_control.od_month[10] = "Nov";
   od_control.od_month[11] = "Dec";
   od_control.od_day[0] = "Sun";
   od_control.od_day[1] = "Mon";
   od_control.od_day[2] = "Tue";
   od_control.od_day[3] = "Wed";
   od_control.od_day[4] = "Thu";
   od_control.od_day[5] = "Fri";
   od_control.od_day[6] = "Sat";
   od_control.od_offline = "[OFFLINE] ";
   od_control.od_continue = "Continue? [Y/n/=]";
   od_control.od_continue_yes = 'y';
   od_control.od_continue_no = 'n';
   od_control.od_continue_nonstop = '=';
   od_control.od_help_text2 = OD_VER_STATUSLINE;
   od_control.od_sending_rip = "Sending RIP file: ";
   od_control.od_hanging_up = "Ending call...";
   od_control.od_exiting = "Program is exiting...";
   if(strlen(od_control.od_disable_dtr) == 0)
   {
      strcpy(od_control.od_disable_dtr, "~+++~  AT&D0|  ATO|");
   }

   nInitialRemaining = od_control.user_timelimit;

   if(od_control.od_maxtime > 0 && od_control.od_maxtime <= 1440)
   {
      if(od_control.user_timelimit > (INT)od_control.od_maxtime)
      {
         od_control.od_maxtime_deduction
            = od_control.user_timelimit-od_control.od_maxtime;
         od_control.user_timelimit
            = od_control.od_maxtime;
      }
   }

   /* If sysop name is unkown, then use the word "Sysop". */
   if(strlen(od_control.sysop_name) == 0)
   {
      strcpy(od_control.sysop_name, "Sysop");
   }

   /* If in foced local mode and user  name has not yet been set. */
   if(od_control.od_force_local && od_control.user_name[0] == '\0')
   {
      /* If name prompting is not disabled, then turn on flag to prompt for */
      /* user's name.                                                       */
      if(!(od_control.od_disable & DIS_NAME_PROMPT))
      {
         bPromptForUserName = TRUE;
      }

      /* Use sysop's name as default user name. */
      if(bSysopNameSet)
      {
         strcpy(od_control.user_name, szForcedSysopName);
      }
      else
      {
         strcpy(od_control.user_name, od_control.sysop_name);
      }
   }

   dwFileBPS = od_control.baud;

   /* Determine desired BPS rate. */
   if(dwForcedBPS != 1)
   {
      if(od_control.od_disable & DIS_LOCAL_OVERRIDE || od_control.baud != 0)
      {
         od_control.baud = dwForcedBPS;
      }
   }

   /* If we are not operating in local mode, then setup for serial I/O. */
#ifndef ODPLAT_NIX
   if(od_control.baud != 0)
#endif
   {
      tComMethod ComMethod;

      /* Attempt to allocate an OpenDoors port object. */
      if(ODComAlloc(&hSerialPort) != kODRCSuccess)
      {
malloc_error:
         ODInitError("Insufficient memory available to start up program.");
         exit(od_control.od_errorlevel[1]);
      }

      /* Set socket I/O method, if specified by user. */
      if(od_control.od_use_socket)
      {
         ODComSetPreferredMethod(hSerialPort, kComMethodSocket);
      }

#if defined ODPLAT_WIN32 || defined ODPLAT_NIX
      /* Check whether a handle has been provided by the caller. */
      if(od_control.od_open_handle != 0)
      {
         if(ODComOpenFromExistingHandle(hSerialPort, od_control.od_open_handle)
            != kODRCSuccess)
         {
            ODInitError("Unable to use provided serial port handle.");
            exit(od_control.od_errorlevel[1]);
         }
      }
      else
#endif /* ODPLAT_WIN32 */
      {
         /* Set flow control type. */
         switch(od_control.od_com_flow_control)
         {
            case COM_NO_FLOW:
               ODComSetFlowControl(hSerialPort, FLOW_NONE);
               break;
            case COM_RTSCTS_FLOW:
               ODComSetFlowControl(hSerialPort, FLOW_RTSCTS);
               break;
            case COM_DEFAULT_FLOW:
               /* Don't say anything to the serial I/O module about flow */
               /* control. */
               break;
            default:
               /* We should never get here. */
               ASSERT(FALSE);
         }

         /* Determine desired port. */
         if(nForcedPort != -1) od_control.port=nForcedPort;

         /* Set port number. */
         ODComSetPort(hSerialPort, (BYTE)od_control.port);

         /* If serial port address has been explicitly set, then set user's */
         /* port address in serial port object.                             */
         if(od_control.od_com_address != 0)
         {
            ODComSetPortAddress(hSerialPort, od_control.od_com_address);
         }

         /* If serial port IRQ line number has been explicitly set, then */
         /* set user's setting in serial port object.                    */
         if(od_control.od_com_irq >= 1 && od_control.od_com_irq < 15)
         {
            ODComSetIRQ(hSerialPort, od_control.od_com_irq);
         }

         /* Set BPS rate, if this is not disabled. */
         if(!(od_control.od_disable&DIS_BPS_SETTING))
         {
            ODComSetSpeed(hSerialPort, od_control.baud);
         }
         else
         {
            ODComSetSpeed(hSerialPort, SPEED_UNSPECIFIED);
         }

         /* Set serial I/O method, if specified by user. */
         if(od_control.od_no_fossil)
         {
            ODComSetPreferredMethod(hSerialPort, kComMethodUART);
         }

         /* Establish default buffer sizes */
         if(od_control.od_com_rx_buf == 0)
            od_control.od_com_rx_buf = 256;
         if(od_control.od_com_tx_buf == 0)
            od_control.od_com_tx_buf = 3072;

         /* Set buffer sizes for serial port. */
         ODComSetRXBuf(hSerialPort, od_control.od_com_rx_buf);
         ODComSetTXBuf(hSerialPort, od_control.od_com_tx_buf);

         /* Set FIFO enabled/disabled and trigger size */
         if(od_control.od_com_no_fifo)
         {
            /* Disable UART FIFO buffers. */
            ODComSetFIFO(hSerialPort, FIFO_DISABLE);
         }
         else
         {
            /* Enable FIFO, setting size of FIFO trigger. */
            switch(od_control.od_com_fifo_trigger)
            {
               case 1:
                  ODComSetFIFO(hSerialPort, FIFO_ENABLE | FIFO_TRIGGER_1);
                  break;
               case 4:
                  ODComSetFIFO(hSerialPort, FIFO_ENABLE | FIFO_TRIGGER_4);
                  break;
               case 8:
                  ODComSetFIFO(hSerialPort, FIFO_ENABLE | FIFO_TRIGGER_8);
                  break;
               case 14:
                  ODComSetFIFO(hSerialPort, FIFO_ENABLE | FIFO_TRIGGER_14);
                  break;
               default:
                  od_control.od_com_fifo_trigger = 4;
                  ODComSetFIFO(hSerialPort, FIFO_ENABLE | FIFO_TRIGGER_4);
            }
         }

#ifdef ODPLAT_DOS
         /* Set od_kernel() to be idle function. */
         ODComSetIdleFunction(hSerialPort, od_kernel);
#endif /* ODPLAT_DOS */

         /* Open serial port. */
         switch(ODComOpen(hSerialPort))
         {
            case kODRCSuccess:
               /* Serial port open succeeded, so proceed with OpenDoors */
               /* initialization.                                       */
               break;
            case kODRCNoMemory:
               goto malloc_error;
            case kODRCNoPortAddress:
               ODInitError("Serial port address is unknown, cannot continue.");
               exit(od_control.od_errorlevel[1]);
               break;
            case kODRCNoUART:
               ODInitError("No UART at specified port address, cannot continue.\n");
               exit(od_control.od_errorlevel[1]);
               break;
            default:
               ODInitError("Unable to access serial port, cannot continue.\n");
               exit(od_control.od_errorlevel[1]);
               break;
         }
      }

      /* Raise DTR signal. */
      ODComSetDTR(hSerialPort, TRUE);

      /* Get serial I/O method actually being used. */
      ODComGetMethod(hSerialPort, (tComMethod *)&ComMethod);
      switch(ComMethod)
      {
         case kComMethodFOSSIL:
            od_control.od_com_method = COM_FOSSIL;
            break;
         case kComMethodUART:
            od_control.od_com_method = COM_INTERNAL;
            break;
         case kComMethodWin32:
            od_control.od_com_method = COM_WIN32;
            break;
         case kComMethodDoor32:
            od_control.od_com_method = COM_DOOR32;
            break;
			case kComMethodSocket:
				od_control.od_com_method = COM_SOCKET;	/* Why are using doubling up constants here? */
				break;
		 case kComMethodStdIO:
		    od_control.od_com_method = COM_STDIO;
			break;
         default:
            ODInitError("No method of accessing serial port, cannot continue.\n");
            exit(od_control.od_errorlevel[1]);
            break;
      }
   }

   /* If we are operating in local mode, then disable silent mode. */
   if(od_control.baud == 0)
   {
      od_control.od_silent_mode = FALSE;
   }

   /* Setup local screen. */
   ODScrnInitialize();
#ifdef OD_TEXTMODE
   ODScrnSetBoundary(1, 1, 80, 23);
#else /* !OD_TEXTMODE */
   ODScrnSetBoundary(1, 1, 80, 25);
#endif /* !OD_TEXTMODE */

#ifndef ODPLAT_WIN32
   if(bPreset)
   {
      atexit(ODAtExitCallback);
      bPreset = FALSE;
   }
#endif /* !ODPLAT_WIN32 */

   /* Setup remote terminal for ANSI graphics if operating in RIP mode. */
   if(od_control.user_rip)
   {
      od_clr_scr();
   }

#ifdef ODPLAT_WIN32
   /* Startup the OpenDoors frame window, if we are not operating in silent */
   /* mode.                                                                 */
   if(!od_control.od_silent_mode)
   {
#ifdef OD_DLL
      ODFrameStart(GetModuleHandle(OD_DLL_NAME), &hFrameThread);
#else /* !OD_DLL */
      ODFrameStart(GetModuleHandle(NULL), &hFrameThread);
#endif /* !OD_DLL */
   }
#endif /* ODPLAT_WIN32 */

   /* Initialize the OpenDoors kernel. */
   ODKrnlInitialize();

#ifndef ODPLAT_WIN32
#ifdef ODPLAT_NIX
   if(bPromptForUserName)
   {
      od_control.od_com_method=COM_STDIO;
      od_control.baud=19200;
      gethostname(od_control.system_name,sizeof(od_control.system_name));
      od_control.system_name[sizeof(od_control.system_name)-1]=0;
      if (isatty(STDIN_FILENO))  {
        tcgetattr(STDIN_FILENO,&term);
   	  od_control.baud=cfgetispeed(&term);
        if(!od_control.baud)
   	    od_control.baud=cfgetispeed(&term);
        if(!od_control.baud)
   		 od_control.baud=19200;
      }
      uinfo=getpwuid(getuid());
      ODStringCopy(od_control.user_handle, uinfo->pw_name,sizeof(od_control.user_handle));
      ODStringCopy(od_control.user_name, uinfo->pw_gecos,sizeof(od_control.user_name));
   }
#else
   if(bPromptForUserName)
   {
      void *pWindow = ODScrnCreateWindow(10, 8, 70, 15,
         od_control.od_local_win_col, od_control.od_prog_name,
         od_control.od_local_win_col);

      if(pWindow != NULL)
      {
         ODScrnSetCursorPos(12, 9);
         ODScrnDisplayString("This program has been started in local mode,");
         ODScrnSetCursorPos(12, 10);
         ODScrnDisplayString("independently of a BBS system. When operating in this");
         ODScrnSetCursorPos(12, 11);
         ODScrnDisplayString("mode, you may specify what name you should be known to");
         ODScrnSetCursorPos(12, 12);
         ODScrnDisplayString("the program by.");
         ODScrnSetCursorPos(12, 14);
         ODScrnDisplayString("Your name:");
         ODScrnLocalInput(23, 14, od_control.user_name, 35);
         ODStringCopy(od_control.user_handle, od_control.user_name,
            sizeof(od_control.user_name));
         ODScrnDestroyWindow(pWindow);
         ODScrnSetCursorPos(1, 1);
         ODScrnSetAttribute(0x07);
      }
   }
#endif /* !ODPLAT_NIX */
#endif /* !ODPLAT_WIN32 */

#ifdef OD_TEXTMODE
   /* Setup sysop status line/function key personality. */
   if(pfSetPersonality == NULL)
   {
no_default:
      if (od_control.od_default_personality == NULL)
      {
         pfCurrentPersonality = pdef_opendoors;
      }
      else
      {
         pfCurrentPersonality = od_control.od_default_personality;
      }
      (*pfCurrentPersonality)(20);
      if(bRAStatus)
      {
         od_set_statusline(btRAStatusToSet);
      }
      else
      {
         od_set_statusline(0);
      }
   }
   else
   {
      if(!((*pfSetPersonality)(szDesiredPersonality)))
      {
         goto no_default;
      }
   }
#endif /* OD_TEXTMODE */

   /* If connect speed has not been set yet, then set it to the */
   /* serial port speed.                                        */
   if(od_control.od_connect_speed == 0)
   {
      od_control.od_connect_speed = od_control.baud;
   }

   /* Initialize the array of characters to use for drawing boxes, */
   /* window and menu boarders, etc.                               */
   od_control.od_box_chars[0] = 218U;
   od_control.od_box_chars[1] = 196U;
   od_control.od_box_chars[2] = 191U;
   od_control.od_box_chars[3] = 179U;
   od_control.od_box_chars[4] = 192U;
   od_control.od_box_chars[5] = 217U;

   /* Enable pausing and stoping of listing. */
   od_control.od_list_stop = TRUE;
   od_control.od_list_pause = TRUE;

   /* Initialize array of logfile messages. */
   for(btCount = 0; btCount < DIM(apszLogMessages); ++btCount)
   {
      if(od_control.od_logfile_messages[btCount] == NULL)
      {
         od_control.od_logfile_messages[btCount]
            = apszLogMessages[btCount];
      }
   }

   /* Set log file name to default, if none has been specified already. */
   if(strlen(od_control.od_logfile_name) == 0)
   {
      strcpy(od_control.od_logfile_name, "DOOR.LOG");
   }

   /* If OpenDoors log file system is installed, then start it up. */
   if(od_control.od_logfile != NULL)
   {
       (*(OD_COMPONENT_CALLBACK *)od_control.od_logfile)();
   }
}


/* ----------------------------------------------------------------------------
 * ODInitError()
 *
 * Called to display an error message when OpenDoors initialization fails,
 * forcing the program to exit.
 *
 * Parameters: pszErrorText - Pointer to the error message explaining the
 *                            reason for failure.
 *
 *     Return: void
 */
void ODInitError(char *pszErrorText)
{
#ifdef ODPLAT_DOS
   printf("%s: %s\n", od_control.od_prog_name, pszErrorText);
   if(bParsedCmdLine)
   {
      printf("Use the -HELP command line option for help, or -LOCAL for local mode.\n");
   }
#endif
#ifdef ODPLAT_WIN32
   char *pszMessage;
   if(!bParsedCmdLine ||
      (pszMessage = malloc(strlen(pszErrorText) + 80)) == NULL)
   {
      MessageBox(NULL, pszErrorText, od_control.od_prog_name,
         MB_ICONSTOP | MB_OK | MB_TASKMODAL);
   }
   else
   {
      sprintf(pszMessage,
         "%s\nUse the -HELP command line option for help, or -LOCAL for local mode.",
         pszErrorText);
      MessageBox(NULL, pszMessage, od_control.od_prog_name,
         MB_ICONSTOP | MB_OK | MB_TASKMODAL);
      free(pszMessage);
   }
#endif
#ifdef ODPLAT_NIX
   fwrite(pszErrorText,strlen(pszErrorText),1,stderr);
#endif
}


/* ----------------------------------------------------------------------------
 * ODInitLoginDlgProc()
 *
 * Dialog box proceedure for the local mode login dialog box.
 *
 * Parameters: hwndDlg  - Window handle to the dialog box.
 *
 *             uMsg     - Message ID.
 *
 *             wParam   - First message parameter.
 *
 *             lParam   - Second message parameter.
 *
 *     Return: TRUE if message is processed, FALSE otherwise.
 */
#ifdef ODPLAT_WIN32
BOOL CALLBACK ODInitLoginDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
   LPARAM lParam)
{
   switch(uMsg)
   {
      case WM_INITDIALOG:
         ODFrameCenterWindowInParent(hwndDlg);

         /* Set the title of the dialog box to the name of this program. */
         SetWindowText(hwndDlg, od_control.od_prog_name);

         /* The initial text in the user name dialog box should be the */
         /* default user name.                                         */
         SetWindowText(GetDlgItem(hwndDlg, IDC_USER_NAME),
            od_control.user_name);

         /* Limit the number of characters that may be entered as the */
         /* user's name to the maximum size of the string.            */
         SendMessage(GetDlgItem(hwndDlg, IDC_USER_NAME), EM_LIMITTEXT,
            sizeof(od_control.user_name), 0L);

         return(TRUE);

      case WM_COMMAND:
         /* If a command has been chosen. */
         switch(LOWORD(wParam))
         {
            case IDOK:
               /* If the OK button has been pressed, obtain the entered */
               /* user name.                                            */
               GetWindowText(GetDlgItem(hwndDlg, IDC_USER_NAME),
                  od_control.user_name, sizeof(od_control.user_name));
               ODStringCopy(od_control.user_handle, od_control.user_name,
                  sizeof(od_control.user_name));

               /* Now close the dialog. */
               EndDialog(hwndDlg, IDOK);
               break;

            case IDCANCEL:
               /* If the Cancel button has benn pressed, close the dialog. */
               EndDialog(hwndDlg, IDCANCEL);
               break;
         }
         return(TRUE);

      default:
         /* Otherwise, indicate that this message has not been processed. */
         return(FALSE);
   }
}
#endif /* ODPLAT_WIN32 */
