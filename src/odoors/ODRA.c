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
 *        File: ODRA.c
 *
 * Description: Implements the RemoteAccess personality.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Oct 19, 1994  6.00  BP   Use new od_page constants.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Jul 18, 1995  6.00  BP   Fix ODStatGetUserAge() bug.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 14, 1995  6.00  BP   32-bit portability.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 22, 1995  6.00  BP   Added od_connect_speed.
 *              Dec 24, 1995  6.00  BP   Fixed black square at pos 25x80.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 03, 1996  6.00  BP   Display connect speed with %lu.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODScrn.h"
#include "ODStat.h"
#include "ODInEx.h"


/* Private variables, local to this module. */
static BOOL bRAPersHasBeenOn = FALSE;


/* Private function prototypes. */
static void ODRADisplayPageInfo(void);
static void ODRADisplayDate(char *pszDateString);
static void ODRADisplayFlags(BYTE btFlags);
static void ODRADisplayTime(void);


/* ----------------------------------------------------------------------------
 * pdef_ra()
 *
 * Personality function for the RemoteAccess-like status line / function key
 * personality.
 *
 * Parameters: btOperation - Indicates personality operation to be performed.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL pdef_ra(BYTE btOperation)
{
   BYTE btInfoType = od_control.od_info_type;

   switch(btOperation)
   {
      case PEROP_DISPLAY1:
         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(1, 24);
         ODScrnDisplayString("                                                                     (Node      ");
         ODScrnSetCursorPos(1, 24);
         ODScrnPrintf("%s of %s at %lu BPS", od_control.user_name,
            od_control.user_location, od_control.od_connect_speed);

         if(!od_control.od_user_keyboard_on)
         {
            ODScrnSetCursorPos(49, 24);
            ODScrnSetAttribute(0x99);
            ODScrnDisplayString("(Keyboard)");
            ODScrnSetAttribute(0x70);
            bRAPersHasBeenOn = TRUE;
         }

         ODRADisplayPageInfo();

         ODScrnSetCursorPos(76, 24);
         if(od_control.od_node < 1000)
         {
            ODScrnPrintf("%u)", od_control.od_node);
         }
         else
         {
            ODScrnDisplayString("?)");
         }
         ODScrnSetCursorPos(1, 25);
         ODScrnDisplayString("Security:        Time:                                               (F9)=Help ");

         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);

         ODScrnSetCursorPos(11, 25);
         ODScrnPrintf("%u", od_control.user_security);
         ODScrnSetCursorPos(24, 25);
         ODScrnPrintf("%d mins   ", od_control.user_timelimit);
         if(od_control.user_ansi)
         {
            ODScrnSetCursorPos(42, 25);
            ODScrnDisplayString("(ANSI)");
         }

         if(od_control.user_avatar)
         {
            ODScrnSetCursorPos(48, 25);
            ODScrnDisplayString("(AVT)");
         }

         if(od_control.sysop_next)
         {
            ODScrnSetCursorPos(53, 25);
            ODScrnDisplayString("(SN) ");
         }

         if(od_control.user_wantchat)
         {
            ODScrnSetCursorPos(57, 25);
            ODScrnSetAttribute(0x99);
            ODScrnDisplayString("(Wants Chat)");
            ODScrnSetAttribute(0x70);
         }
         break;


      case PEROP_DISPLAY2:
         ODScrnSetAttribute(0x70);
         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);
         ODScrnSetCursorPos(1, 24);
         ODScrnDisplayString("Voice#:               Last Call   :                       First Call:           ");
         ODScrnSetCursorPos(1, 25);
         ODScrnDisplayString("Data #:               Times Called:            Age:        Birthdate:          ");
         if(od_control.od_extended_info || btInfoType == SFDOORSDAT
            || btInfoType == DOORSYS_GAP || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnSetCursorPos(8, 24);
            ODScrnPrintf("%13.13s", od_control.user_homephone);
         }
         if(od_control.od_extended_info || btInfoType == DOORSYS_GAP
            || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnSetCursorPos(8, 25);
            ODScrnPrintf("%13.13s", od_control.user_dataphone);
         }
         if(od_control.od_extended_info || btInfoType == DOORSYS_GAP
            || btInfoType == CHAINTXT || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnSetCursorPos(37, 24);
            ODRADisplayDate(od_control.user_lastdate);
         }
         if(od_control.od_extended_info || btInfoType == DOORSYS_GAP
            || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnSetCursorPos(37, 25);
            ODScrnPrintf("%lu", od_control.user_numcalls);
         }
         if(btInfoType == RA1EXITINFO || btInfoType == RA2EXITINFO
            || btInfoType == DOORSYS_WILDCAT)
         {
            char szUserAge[7];
            ODScrnSetCursorPos(53, 25);
            ODStatGetUserAge(szUserAge);
            ODScrnDisplayString(szUserAge);
            ODScrnSetCursorPos(71, 24);
            ODRADisplayDate(od_control.user_firstcall);
            ODScrnSetCursorPos(71, 25);
            ODRADisplayDate(od_control.user_birthday);
         }
         break;


      case PEROP_DISPLAY3:
         ODScrnSetAttribute(0x70);
         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);
         ODScrnSetCursorPos(1, 24);
         if(od_control.od_extended_info || btInfoType == SFDOORSDAT
            || btInfoType == DOORSYS_GAP || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnDisplayString("Uploads:                Downloads:               Tagged: 0k (0)                 ");
            if(btInfoType == DOORSYS_GAP)
            {
               ODScrnSetCursorPos(10, 24);
               ODScrnPrintf("%lu", od_control.user_uploads);
               ODScrnSetCursorPos(36, 24);
               ODScrnPrintf("%lu", od_control.user_downloads);
            }
            else
            {
               ODScrnSetCursorPos(10, 24);
               ODScrnPrintf("%luk (%lu)", od_control.user_upk,
                  od_control.user_uploads);
               ODScrnSetCursorPos(36,24);
               ODScrnPrintf("%luk (%lu)", od_control.user_downk,
                  od_control.user_downloads);
            }
         }
         else
         {
            ODScrnDisplayString("                                                                                ");
         }
         ODScrnSetCursorPos(1, 25);
         if(od_control.od_extended_info)
         {
            ODScrnDisplayString("Flags: (A):--------  (B):--------  (C):--------  (D):--------                  ");
            ODScrnSetCursorPos(12, 25);
            ODRADisplayFlags(od_control.user_flags[0]);
            ODScrnSetCursorPos(26, 25);
            ODRADisplayFlags(od_control.user_flags[1]);
            ODScrnSetCursorPos(40, 25);
            ODRADisplayFlags(od_control.user_flags[2]);
            ODScrnSetCursorPos(54, 25);
            ODRADisplayFlags(od_control.user_flags[3]);
         }
         else
         {
            ODScrnDisplayString("                                                                               ");
         }
         break;


      case PEROP_DISPLAY4:
         ODScrnSetAttribute(0x70);
         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);
         ODScrnSetCursorPos(1, 24);
         ODScrnDisplayString("                                                                   (Time      ) ");
         if(od_control.od_extended_info)
         {
            ODScrnSetCursorPos(1, 24);
            ODScrnPrintf("Last Caller: %s    Total System Calls: %lu",
               od_control.system_last_caller, od_control.system_calls);
         }
         ODRADisplayTime();

         ODScrnSetCursorPos(1, 25);
         if(od_control.od_extended_info || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnDisplayString("(Printer OFF)      (Local Screen ON )        Next Event                        ");
            ODScrnSetCursorPos(57, 25);
            if(od_control.event_status == ES_ENABLED
               || btInfoType == DOORSYS_WILDCAT)
            {
               ODScrnPrintf("(%s, errorlevel %u)",od_control.event_starttime,od_control.event_errorlevel);
            }
            else
            {
               ODScrnDisplayString("none, errorlevel 0");
            }
         }
         else
         {
            ODScrnDisplayString("                                                                               ");
         }
         break;


      case PEROP_DISPLAY5:
         ODScrnSetAttribute(0x70);
         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);
         ODScrnSetCursorPos(1, 24);
         if(od_control.od_extended_info || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnDisplayString("Msgs posted    :           Highread :                                Group 1    ");
            ODScrnSetCursorPos(18,24);
            ODScrnPrintf("%u",od_control.user_messages);
            ODScrnSetCursorPos(39,24);
            ODScrnPrintf("%u",od_control.user_lastread);
            if(btInfoType == RA1EXITINFO || btInfoType == RA2EXITINFO)
            {
               ODScrnSetCursorPos(76, 24);
               ODScrnPrintf("%u", od_control.user_group);
            }
         }
         else
         {
            ODScrnDisplayString("                                                                                ");
         }

         ODScrnSetCursorPos(1, 25);
         if(od_control.od_extended_info || btInfoType == CHAINTXT
            || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnDisplayString("Credit         :           Handle   :                                          ");
            if(btInfoType == EXITINFO || btInfoType == RA1EXITINFO
               || btInfoType == RA2EXITINFO)
            {
               ODScrnSetCursorPos(18, 25);
               ODScrnPrintf("%u.00", (unsigned int)od_control.user_net_credit);
            }
            if(btInfoType == CHAINTXT || btInfoType == RA1EXITINFO
               || btInfoType == RA2EXITINFO || btInfoType == DOORSYS_WILDCAT)
            {
               ODScrnSetCursorPos(39, 25);
               ODScrnDisplayString(od_control.user_handle);
            }
         }
         else
         {
            ODScrnDisplayString("                                                                               ");
         }
         break;


      case PEROP_DISPLAY6:
         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(1, 24);
         ODScrnDisplayString("                                                                                ");
         ODScrnSetCursorPos(1, 25);
         ODScrnDisplayString("                                                                               ");
         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);
         if(btInfoType == RA1EXITINFO || btInfoType == RA2EXITINFO
            || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnSetCursorPos(1, 24);
            ODScrnDisplayString(od_control.user_comment);
         }
         if(od_control.user_wantchat
            && strlen(od_control.user_reasonforchat) !=0 )
         {
            ODScrnSetCursorPos(1, 25);
            strcpy(szStatusText, od_control.user_reasonforchat);
            szStatusText[69 - strlen(od_control.user_name)] = '\0';
            ODScrnPrintf("Chat (%s): %s", od_control.user_name, szStatusText);
         }
         break;

      case PEROP_DISPLAY7:
         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(1, 24);
         ODScrnDisplayString("                                                                                ");
         ODScrnSetCursorPos(1, 25);
         ODScrnDisplayString("                                                                               ");
         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);
         break;

      case PEROP_DISPLAY8:
         ODScrnSetAttribute(0x70);
         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);
         ODScrnSetCursorPos(1, 24);
         ODScrnDisplayString("ALT: (C)hat (H)angup (J)Shell (L)ockOut (K)eyboard (N)extOn (D)rop To BBS       ");
         ODScrnDisplayString("                                   -Inc Time -Dec Time  (F1)-(F7)=Extra Stats");
         break;

      case PEROP_UPDATE1:
         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(24, 25);

         ODScrnPrintf("%d mins   ", od_control.user_timelimit);

         ODScrnSetCursorPos(42, 25);
         if(od_control.user_ansi)
         {
            ODScrnDisplayString("(ANSI)");
         }
         else
         {
            ODScrnDisplayString("      ");
         }

         if(od_control.user_avatar)
         {
            ODScrnDisplayString("(AVT)");
         }
         else
         {
            ODScrnDisplayString("     ");
         }

         if(od_control.sysop_next)
         {
            ODScrnDisplayString("(SN)");
         }
         else
         {
            ODScrnDisplayString("    ");
         }

         if(od_control.user_wantchat)
         {
            ODScrnSetAttribute(0x99);
            ODScrnDisplayString("(Wants Chat)");
            ODScrnSetAttribute(0x70);
         }
         else
         {
            ODScrnDisplayString("            ");
         }

         ODRADisplayPageInfo();

         if(od_control.od_user_keyboard_on && bRAPersHasBeenOn)
         {
            ODScrnSetCursorPos(49, 24);
            ODScrnDisplayString("          ");
         }
         if(!od_control.od_user_keyboard_on)
         {
            bRAPersHasBeenOn = TRUE;
            ODScrnSetCursorPos(49, 24);
            ODScrnSetAttribute(0x99);
            ODScrnDisplayString("(Keyboard)");
            ODScrnSetAttribute(0x70);
         }

         break;

      case PEROP_UPDATE4:
         ODScrnSetAttribute(0x70);
         ODRADisplayTime();
         break;

      case PEROP_INITIALIZE:
         bRAStatus = TRUE;
         od_control.key_hangup = 0x2300;
         od_control.key_drop2bbs = 0x2000;
         od_control.key_dosshell = 0x2400;
         od_control.key_chat = 0x2e00;
         od_control.key_sysopnext = 0x3100;
         od_control.key_lockout = 0x2600;
         od_control.key_status[0] = 0x3b00;
         od_control.key_status[1] = 0x3c00;
         od_control.key_status[2] = 0x3d00;
         od_control.key_status[3] = 0x3e00;
         od_control.key_status[4] = 0x3f00;
         od_control.key_status[5] = 0x4000;
         od_control.key_status[6] = 0x4100;
         od_control.key_status[7] = 0x4300;
         od_control.key_status[8] = 0x4400;
         od_control.key_keyboardoff = 0x2500;
         od_control.key_moretime = 0x4800;
         od_control.key_lesstime = 0x5000;
         od_control.od_page_statusline = 5;
   }
}


/* ----------------------------------------------------------------------------
 * ODRADisplayPageInfo()                               *** PRIVATE FUNCTION ***
 *
 * Displays the current sysop page information on the RemoteAccess status line,
 * at the appropriate position, and in the appropriate color.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODRADisplayPageInfo(void)
{
   time_t nUnixTime;
   struct tm *TimeBlock;
   BOOL bFailed = FALSE;
   INT nMinute;

   ODScrnSetCursorPos(60, 24);

   switch(od_control.od_okaytopage)
   {
      case PAGE_ENABLE:
         ODScrnSetAttribute(0x19);
         ODScrnDisplayString("(PAGE ON) ");
         ODScrnSetAttribute(0x70);
         break;

      case PAGE_DISABLE:
         ODScrnSetAttribute(0x19);
         ODScrnDisplayString("(PAGE OFF)");
         ODScrnSetAttribute(0x70);
         break;

      case PAGE_USE_HOURS:
         nUnixTime = time(NULL);
         TimeBlock = localtime(&nUnixTime);
         nMinute = (60 * TimeBlock->tm_hour) + TimeBlock->tm_min;
         if(od_control.od_pagestartmin < od_control.od_pageendmin)
         {
            if(nMinute < od_control.od_pagestartmin
               || nMinute >= od_control.od_pageendmin)
            {
               bFailed = TRUE;
            }
         }
         else
         {
            if(nMinute < od_control.od_pagestartmin
               && nMinute >= od_control.od_pageendmin)
            {
               bFailed=TRUE;
            }
         }

         if(bFailed)
         {
            ODScrnDisplayString("(PAGE OFF)");
         }
         else
         {
            ODScrnDisplayString("(PAGE ON) ");
         }
   }
}


/* ----------------------------------------------------------------------------
 * ODRADisplayTime()                                   *** PRIVATE FUNCTION ***
 *
 * Displays the current time on the RemoteAccess status line, at the
 * appropriate position. The time is displayed in the current color.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODRADisplayTime(void)
{
   time_t nUnixTime;
   struct tm *TimeBlock;

   nUnixTime = time(NULL);
   TimeBlock = localtime(&nUnixTime);
   ODScrnSetCursorPos(74, 24);
   ODScrnPrintf("%02.2d:%02.2d", TimeBlock->tm_hour, TimeBlock->tm_min);
}


/* ----------------------------------------------------------------------------
 * ODRADisplayDate()                                   *** PRIVATE FUNCTION ***
 *
 * Displays a date stored in a string on the RemoteAccess status line, if
 * the string represents a valid date. The date is displayed at the current
 * cursor location, in the current color.
 *
 * Parameters: pszDateString - Pointer to a string containing the date to
 *                             display.
 *
 *     Return: void
 */
static void ODRADisplayDate(char *pszDateString)
{
   INT nMonth;
   INT nTemp;

   ASSERT(pszDateString != NULL);

   if(strlen(pszDateString) != 8) return;

   nMonth = atoi(pszDateString) - 1;
   if(nMonth < 0 || nMonth > 11) return;

   nTemp=atoi((char *)pszDateString + 3);
   if(nTemp < 1 || nTemp > 31) return;

   if(pszDateString[6] < '0' || pszDateString[6] > '9'
      || pszDateString[7] < '0' || pszDateString[7] > '9')
   {
      return;
   }

   ODScrnDisplayChar(pszDateString[3]);
   ODScrnDisplayChar(pszDateString[4]);
   ODScrnDisplayChar('-');
   ODScrnDisplayString(od_control.od_month[nMonth]);
   ODScrnDisplayChar('-');
   ODScrnDisplayChar(pszDateString[6]);
   ODScrnDisplayChar(pszDateString[7]);
}


/* ----------------------------------------------------------------------------
 * ODRADisplayFlags()                                  *** PRIVATE FUNCTION ***
 *
 * Displays sysop-defined user flags on the status line. The flags are
 * displayed at the current cursor location, in the current color.
 *
 * Parameters: btFlags - Byte of eight flags to display.
 *
 *     Return: void
 */
static void ODRADisplayFlags(BYTE btFlags)
{
   BYTE btMask = 0x01;
   INT nFlag;

   for(nFlag = 0; nFlag < 8; ++nFlag)
   {
      if(btFlags & btMask)
      {
         ODScrnDisplayChar('X');
      }
      else
      {
         ODScrnDisplayChar('-');
      }
      btMask <<= 1;
   }
}
