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
 *        File: ODWCat.c
 *
 * Description: Implements the Wildcat personality.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Jul 18, 1995  6.00  BP   Fix ODStatGetUserAge() bug.
 *              Nov 13, 1995  6.00  BP   32-bit portability.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 24, 1995  6.00  BP   Fixed black square at pos 25x80.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 03, 1996  6.00  BP   Display connect speed with %lu.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>

#include "OpenDoor.h"
#include "ODGen.h"
#include "ODScrn.h"
#include "ODCore.h"
#include "ODStat.h"
#include "ODKrnl.h"


/* ----------------------------------------------------------------------------
 * pdef_wildcat()
 *
 * Personality function for the Wildcat BBS-like status line / function key
 * personality.
 *
 * Parameters: btOperation - Indicates personality operation to be performed.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL pdef_wildcat(BYTE btOperation)
{
   BYTE btInfoType = od_control.od_info_type;

   switch(btOperation)
   {
      case PEROP_DISPLAY1:
         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(1,24);
         ODScrnDisplayString("                                    Baud:                                       ");
         ODScrnSetCursorPos(1,25);
         ODScrnDisplayString("Phone:                               Sec:                      Time Left:      ");
         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);

         ODScrnSetAttribute(0x71);
         ODScrnSetCursorPos(1,24);
         sprintf(szStatusText, "(%s), ", od_control.user_name,
            od_control.user_location);
         ODScrnPrintf("%34.34s", szStatusText);
         ODScrnSetCursorPos(43, 24);
         ODScrnPrintf("%lu", od_control.od_connect_speed);

         ODScrnSetCursorPos(8,25);
         if(od_control.od_extended_info || btInfoType==SFDOORSDAT || btInfoType==DOORSYS_GAP || btInfoType==DOORSYS_WILDCAT)
         {
            ODScrnDisplayString(od_control.user_homephone);
         }

         if(btInfoType==RA1EXITINFO || btInfoType==RA2EXITINFO || btInfoType==DOORSYS_WILDCAT)
         {
            char szUserAge[7];
            ODScrnSetCursorPos(28,25);
            ODScrnSetAttribute(0x70);
            ODScrnDisplayString("Age: ");
            ODScrnSetAttribute(0x71);
            ODStatGetUserAge(szUserAge);
            ODScrnDisplayString(szUserAge);
         }

         ODScrnSetCursorPos(43,25);
         ODScrnPrintf("%u",od_control.user_security);

         if(btInfoType==RA1EXITINFO || btInfoType==RA2EXITINFO || btInfoType==DOORSYS_WILDCAT)
         {
            if(strlen(od_control.user_firstcall)==8)
            {
               ODScrnSetCursorPos(49,25);
               ODScrnSetAttribute(0x70);
               ODScrnDisplayString("Since: ");
               ODScrnSetAttribute(0x71);
               ODScrnDisplayChar(od_control.user_firstcall[0]);
               ODScrnDisplayChar(od_control.user_firstcall[1]);
               ODScrnDisplayChar('/');
               ODScrnDisplayChar(od_control.user_firstcall[6]);
               ODScrnDisplayChar(od_control.user_firstcall[7]);
            }
         }

      case PEROP_UPDATE1:
         ODScrnSetAttribute(0x71);
         ODScrnSetCursorPos(74,25);
         if(od_control.user_timelimit<=9)
         {
            ODScrnPrintf("   %d",od_control.user_timelimit);
         }
         else if(od_control.user_timelimit<=99)
         {
            ODScrnPrintf("  %d",od_control.user_timelimit);
         }
         else if(od_control.user_timelimit<=999)
         {
            ODScrnPrintf(" %d",od_control.user_timelimit);
         }
         else
         {
            ODScrnPrintf("%d",od_control.user_timelimit);
         }

         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(56,24);

         if(od_control.od_okaytopage==TRUE)
            ODScrnDisplayString("Page Bell ");
         else
            ODScrnDisplayString("          ");

         if(od_control.od_user_keyboard_on)
            ODScrnDisplayString("Kybd ");
         else
            ODScrnDisplayString("     ");

         if(od_control.sysop_next)
            ODScrnDisplayString("Local-Next");
         else
            ODScrnDisplayString("          ");
         break;

      case PEROP_INITIALIZE:
         od_control.key_hangup=0x0000;
         od_control.key_drop2bbs=0x4400;
         od_control.key_dosshell=0x2000;
         od_control.key_chat=0x4100;
         od_control.key_sysopnext=0x3b00;
         od_control.key_lockout=0x8100;
         od_control.key_status[0]=0x0000;
         od_control.key_status[1]=0x0000;
         od_control.key_status[2]=0x0000;
         od_control.key_status[3]=0x0000;
         od_control.key_status[4]=0x0000;
         od_control.key_status[5]=0x0000;
         od_control.key_status[6]=0x0000;
         od_control.key_status[7]=0x0000;
         od_control.key_status[8]=0x0000;
         od_control.key_keyboardoff=0x2500;
         od_control.key_moretime=0x0000;
         od_control.key_lesstime=0x0000;
         od_control.od_page_statusline=-1;
         ODStatAddKey(0x4200);         /* Key to end chat */
         ODStatAddKey(0x4800);         /* Key to add five minutes */
         ODStatAddKey(0x5000);         /* key to subtract five minutes */
         ODStatAddKey(0x7800);         /* key to hangup */
         ODStatAddKey(0x7900);         /* key to hangup */
         ODStatAddKey(0x7a00);         /* key to hangup */
         ODStatAddKey(0x7b00);         /* key to hangup */
         ODStatAddKey(0x7c00);         /* key to hangup */
         ODStatAddKey(0x7d00);         /* key to hangup */
         ODStatAddKey(0x7e00);         /* key to hangup */
         ODStatAddKey(0x7f00);         /* key to hangup */
         ODStatAddKey(0x8000);         /* key to hangup */
         ODStatAddKey(0x3f00);         /* key to toggle bell */
         ODStatAddKey(0x3e00);         /* key to toggle bell */
         break;

      case PEROP_CUSTOMKEY:
         switch((WORD)od_control.od_last_hot)
         {
            case 0x4200:
               od_control.od_chat_active = FALSE;
               break;

            case 0x4800:
               if(od_control.user_timelimit <= 1435)
               {
                  od_control.user_timelimit += 5;
                  bForceStatusUpdate = TRUE;
                  CALL_KERNEL_IF_NEEDED();
               }
               break;

            case 0x5000:
               od_control.user_timelimit -= 5;
               bForceStatusUpdate = TRUE;
               CALL_KERNEL_IF_NEEDED();
               break;

            case 0x7800:
            case 0x7900:
            case 0x7a00:
            case 0x7b00:
            case 0x7c00:
            case 0x7d00:
            case 0x7e00:
            case 0x7f00:
            case 0x8000:
               od_exit(2,TRUE);
               break;

            case 0x3f00:
            case 0x3e00:
               if(od_control.od_okaytopage!=TRUE)
               {
                  od_control.od_okaytopage=TRUE;
               }
               else
               {
                  od_control.od_okaytopage=FALSE;
               }
               bForceStatusUpdate = TRUE;
               CALL_KERNEL_IF_NEEDED();
               break;

            default:
               return;
         }
         od_control.od_last_hot=0;
         break;

      case PEROP_DEINITIALIZE:
         ODStatRemoveKey(0x4200);
         ODStatRemoveKey(0x4800);
         ODStatRemoveKey(0x5000);
         ODStatRemoveKey(0x7800);
         ODStatRemoveKey(0x7900);
         ODStatRemoveKey(0x7a00);
         ODStatRemoveKey(0x7b00);
         ODStatRemoveKey(0x7c00);
         ODStatRemoveKey(0x7d00);
         ODStatRemoveKey(0x7e00);
         ODStatRemoveKey(0x7f00);
         ODStatRemoveKey(0x8000);
         ODStatRemoveKey(0x3e00);
         ODStatRemoveKey(0x3f00);
         break;
   }
}
