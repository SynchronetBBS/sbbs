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
 *        File: ODPCB.c
 *
 * Description: Implements the PC-Board personality.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 14, 1995  6.00  BP   32-bit portability.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 22, 1995  6.00  BP   Added od_connect_speed.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 03, 1996  6.00  BP   Display connect speed with %lu.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <stdio.h>
#include <time.h>

#include "OpenDoor.h"
#include "ODStr.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODScrn.h"
#include "ODKrnl.h"
#include "ODUtil.h"
#include "ODStat.h"


/* ----------------------------------------------------------------------------
 * pdef_pcboard()
 *
 * Personality function for the PC-Board like status line / function key
 * personality.
 *
 * Parameters: btOperation - Indicates personality operation to be performed.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL pdef_pcboard(BYTE btOperation)
{
   static char szTemp[81];
   BYTE btInfoType = od_control.od_info_type;


   switch(btOperation)
   {
      case PEROP_DISPLAY1:
         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(1, 24);
         ODScrnDisplayString("                                                      ALT-H=Help                                                                                                ");
         ODScrnSetCursorPos(3, 24);
         if(od_control.baud != 0)
         {
            ODScrnPrintf("(%lu) ", od_control.od_connect_speed);
         }
         else
         {
            ODScrnDisplayString("(Local) ");
         }
         sprintf(szTemp, "%s - %s", od_control.user_name,
            od_control.user_location);
         szTemp[42] = '\0';
         strupr(szTemp);
         ODScrnDisplayString(szTemp);
         ODScrnSetCursorPos(1,25);
         if(od_control.user_ansi || od_control.user_avatar
            || od_control.user_rip)
         {
            ODScrnDisplayChar('G');
         }
         else
         {
            ODScrnDisplayChar('A');
         }
         if(btInfoType == RA1EXITINFO || btInfoType == RA2EXITINFO
            || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnPrintf(" (%s)",od_control.user_firstcall);
         }
         ODScrnSetCursorPos(15, 25);
         ODScrnPrintf("Sec(0)=%u  ",od_control.user_security);
         if(od_control.od_extended_info || btInfoType == DOORSYS_GAP
            || btInfoType == CHAINTXT || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnPrintf("Times On=%u  ", od_control.user_numcalls);
         }
         if(od_control.od_extended_info || btInfoType == SFDOORSDAT
            || btInfoType == DOORSYS_GAP || btInfoType == DOORSYS_WILDCAT)
         {
            ODScrnPrintf("Up:Dn=%lu:%lu", od_control.user_uploads,
               od_control.user_downloads);
         }
         ODScrnSetCursorPos(70, 25);
         ODScrnPrintf("%4d", od_control.user_timelimit);

         od_control.key_status[0] = 0x0000;
         od_control.key_status[1] = 0x2300;
         break;

      case PEROP_DISPLAY2:
         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(1, 24);
         ODScrnDisplayString("  Alt-> N=Next X=DOS F1/F2=Time                                                   2=LkOut 5=SHELL 8=HngUp 10=Chat                                               ");

         od_control.key_status[0] = 0x2300;
         od_control.key_status[1] = 0x0000;
         break;

      case PEROP_UPDATE1:
         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(70, 25);
         ODScrnPrintf("%4d", od_control.user_timelimit);
         break;

      case PEROP_INITIALIZE:
         od_control.key_hangup = 0x4200;
         od_control.key_drop2bbs = 0x2d00;
         od_control.key_dosshell = 0x3f00;
         od_control.key_chat = 0x4400;
         od_control.key_sysopnext = 0x3100;
         od_control.key_lockout = 0x3c00;
         od_control.key_status[0] = 0x0000;
         od_control.key_status[1] = 0x2300;
         od_control.key_status[2] = 0x0000;
         od_control.key_status[3] = 0x0000;
         od_control.key_status[4] = 0x0000;
         od_control.key_status[5] = 0x0000;
         od_control.key_status[6] = 0x0000;
         od_control.key_status[7] = 0x0000;
         od_control.key_status[8] = 0x0000;
         od_control.key_keyboardoff = 0x2500;
         od_control.key_moretime = 0x0000;
         od_control.key_lesstime = 0x0000;
         ODStatAddKey(0x6900);
         ODStatAddKey(0x6800);
         od_control.od_page_statusline = 0;
         break;

      case PEROP_CUSTOMKEY:
         switch(od_control.od_last_hot)
         {
            /* Key to add five minutes. */
            case 0x6900:
               if(od_control.user_timelimit <= 1435 &&
                  od_control.user_timelimit >= 5)
               {
                  od_control.user_timelimit += 5;
                  bForceStatusUpdate = TRUE;
                  CALL_KERNEL_IF_NEEDED();
               }
               else if(od_control.user_timelimit < 5)
               {
                  od_control.user_timelimit++;
                  bForceStatusUpdate = TRUE;
                  CALL_KERNEL_IF_NEEDED();
               }
               break;

            /* Subtract five minutes from the user's remaining time. */
            case 0x6800:
               if(od_control.user_timelimit > 5)
               {
                  od_control.user_timelimit -= 5;
                  bForceStatusUpdate = TRUE;
                  CALL_KERNEL_IF_NEEDED();
               }
               else if(od_control.user_timelimit > 1)
               {
                  --od_control.user_timelimit;
                  bForceStatusUpdate = TRUE;
                  CALL_KERNEL_IF_NEEDED();
               }
               break;

            default:
               return;
         }
         od_control.od_last_hot = 0;
         break;
   }
}
