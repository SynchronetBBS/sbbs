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
 *        File: ODStand.c
 *
 * Description: Implements the OpenDoors standard (default) personality.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Nov 13, 1995  6.00  BP   32-bit portability.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 22, 1995  6.00  BP   Added od_connect_speed.
 *              Dec 24, 1995  6.00  BP   Fixed black square at pos 25x80.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODScrn.h"
#include "ODInEx.h"


/* ----------------------------------------------------------------------------
 * pdef_opendoors()
 *
 * Personality function for the OpenDoors standard status line / function key
 * personality.
 *
 * Parameters: btOperation - Indicates personality operation to be performed.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL pdef_opendoors(BYTE btOperation)
{
   switch(btOperation)
   {
      case PEROP_DISPLAY1:
         /* Display standard status line. */
         ODScrnSetAttribute(0x70);
         ODScrnSetCursorPos(1, 24);
         ODScrnDisplayString(od_control.od_status_line[0]);
         ODScrnSetCursorPos(1, 24);
         ODScrnPrintf(od_control.od_status_line[1], od_control.user_name,
            od_control.user_location, od_control.od_connect_speed);
         ODScrnSetCursorPos(77, 24);
         if(od_control.od_node < 1000)
         {
            ODScrnPrintf("%d]", od_control.od_node);
         }
         else
         {
            ODScrnDisplayString("?]");
         }
         ODScrnSetCursorPos(1,25);
         ODScrnDisplayString(od_control.od_status_line[2]);

         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);

         ODScrnSetCursorPos(11,25);
         ODScrnPrintf("%u",od_control.user_security);
         ODScrnSetCursorPos(24,25);
         ODScrnPrintf(od_control.od_time_left,od_control.user_timelimit);
         if(od_control.user_ansi)
         {
            ODScrnSetCursorPos(40,25);
            ODScrnDisplayString("[ANSI]");
         }

         if(od_control.user_avatar)
         {
            ODScrnSetCursorPos(47,25);
            ODScrnDisplayString("[AVT]");
         }

         if(od_control.sysop_next)
         {
            ODScrnSetCursorPos(35,25);
            ODScrnDisplayString(od_control.od_sysop_next);
         }

         if(od_control.user_wantchat)
         {
            ODScrnSetCursorPos(57,25);
            ODScrnSetAttribute(0xf0);
            ODScrnDisplayString(od_control.od_want_chat);
         }

         if(!od_control.od_user_keyboard_on)
         {
            ODScrnSetCursorPos(58,24);
            ODScrnSetAttribute(0xf0);
            ODScrnDisplayString(od_control.od_no_keyboard);
         }
         break;

      case PEROP_DISPLAY8:
         /* Display help status line. */
         ODScrnSetAttribute(0x70);
         ODScrnPutText(80, 25, 80, 25, abtGreyBlock);
         ODScrnSetCursorPos(1,24);
         ODScrnDisplayString(od_control.od_help_text);
         ODScrnSetCursorPos(1,25);
         /* Display copyright inforomation. */
         if(bUserFull)/**/
         {
            ODScrnDisplayString(od_control.od_help_text2);
         }
         else
         {
            ODScrnDisplayString(OD_VER_UNREG_STAT);
         }
         break;

      case PEROP_UPDATE1:
         /* Update primary status line. */
         ODScrnSetAttribute(0x70);

         /* Update user's time limit. */
         ODScrnSetCursorPos(24,25);
         ODScrnPrintf(od_control.od_time_left,od_control.user_timelimit);

         /* Update "sysop next" setting. */
         ODScrnSetCursorPos(35,25);
         if(od_control.sysop_next)
         {
            ODScrnDisplayString(od_control.od_sysop_next);
         }
         else
         {
            ODScrnDisplayString("     ");
         }

         /* Update ANSI mode indicator. */
         if(od_control.user_ansi)
         {
            ODScrnDisplayString("[ANSI] ");
         }
         else
         {
            ODScrnDisplayString("       ");
         }

         /* Update AVATAR mode indicator. */
         if(od_control.user_avatar)
         {
            ODScrnDisplayString("[AVT] ");
         }
         else
         {
            ODScrnDisplayString("      ");
         }

         /* Update keyboard-off indicator. */
         ODScrnSetCursorPos(58,24);
         if(od_control.od_user_keyboard_on)
         {
            ODScrnDisplayString("          ");
         }
         else
         {
            ODScrnSetAttribute(0xf0);
            ODScrnDisplayString(od_control.od_no_keyboard);
         }

         /* Update want-chat indicator. */
         ODScrnSetCursorPos(57,25);
         if(od_control.user_wantchat)
         {
            ODScrnSetAttribute(0xf0);
            ODScrnDisplayString(od_control.od_want_chat);
         }
         else
         {
            ODScrnDisplayString("           ");
         }
         break;

      case PEROP_INITIALIZE:
         od_control.key_hangup=0x2300;
         od_control.key_drop2bbs=0x2000;
         od_control.key_dosshell=0x2400;
         od_control.key_chat=0x2e00;
         od_control.key_sysopnext=0x3100;
         od_control.key_lockout=0x2600;
         od_control.key_status[0]=0x3b00;
         od_control.key_status[1]=0x0000;
         od_control.key_status[2]=0x0000;
         od_control.key_status[3]=0x0000;
         od_control.key_status[4]=0x0000;
         od_control.key_status[5]=0x0000;
         od_control.key_status[6]=0x0000;
         od_control.key_status[7]=0x4300;
         od_control.key_status[8]=0x4400;
         od_control.key_keyboardoff=0x2500;
         od_control.key_moretime=0x4800;
         od_control.key_lesstime=0x5000;
         od_control.od_page_statusline=-1;
         break;
   }
}
