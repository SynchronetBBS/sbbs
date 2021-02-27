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
 *        File: ODGraph.c
 *
 * Description: Implements special ANSI/AVATAR control functions.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Nov 16, 1995  6.00  BP   Moved local vars here from odcore.c.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 13, 1996  6.10  BP   Added od_get_cursor().
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODScrn.h"
#include "ODKrnl.h"


/* Local private variables. */
static char szANSIClearLine[3] = {27, '[', 'K'};
static char szAvatarClearLine[2] = {22, 7};


/* ----------------------------------------------------------------------------
 * od_clr_line()
 *
 * Clears the contents of the current line, from the current cursor position
 * to the end of the line. This function affects both local and remote
 * screens.
 *
 * Parameters: None.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_clr_line(void)
{
   char *pchLine;
   INT nCharsLeft;
   INT nCount;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_clr_line()");

   /* Ensure that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Obtain the current cursor position. */
   ODScrnGetTextInfo(&ODTextInfo);

   /* Calculate the number of columns that are to be erased. */
   nCharsLeft = 80 - ODTextInfo.curx;

   /* If either ANSI or AVATAR mode is available, then we first */
   /* clear the line on the local screen without affecting the  */
   /* remote screen.                                            */
   if(od_control.user_avatar || od_control.user_ansi)
   {
      pchLine = (char *)szODWorkString;
      for(nCount = 0; nCount <= nCharsLeft; ++nCount) *pchLine++ = ' ';
      *pchLine = '\0';
      ODScrnEnableScrolling(0);
      ODScrnDisplayString(szODWorkString);
      ODScrnEnableScrolling(1);
      ODScrnSetCursorPos(ODTextInfo.curx, ODTextInfo.cury);
   }

   /* If AVATAR mode is active. */
   if(od_control.user_avatar)
   {
      /* Transmit the two-character AVATAR clear to end of line sequence. */
      od_disp(szAvatarClearLine, 2, FALSE);
   }

   /* If ANSI mode is active. */
   else if(od_control.user_ansi)
   {
      /* Transmit the three-character ANSI clear to end of line sequence. */
      od_disp(szANSIClearLine, 3, FALSE);
   }

   /* If we are operating in plain-ASCII mode. */
   else
   {
      /* Generate a sequence of space characters followed by backspace */
      /* characters.                                                   */
      pchLine = (char *)szODWorkString;
      for(nCount = 0; nCount < nCharsLeft; ++nCount) *pchLine++ = ' ';
      for(nCount = 0; nCount < nCharsLeft; ++nCount) *pchLine++ = 8;
      *pchLine='\0';

      /* Send this sequence to both the local and remote screens. */
      od_disp(szODWorkString, strlen(szODWorkString), TRUE);
   }

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * od_set_cursor()
 *
 * Moves the position of the cursor on both local and remote screens. This
 * function is available in all display modes other than plain-ASCII.
 *
 * Parameters: nRow     - 1-based index of the row to position the cursor in.
 *
 *             nColumn  - Index of the column to position the cursor in.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_set_cursor(INT nRow, INT nColumn)
{
   static char szControlSequence[40];

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_set_cursor()");

   /* Ensure that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Check validity of parameters. */
   if(nRow < 1 || nColumn < 1)
   {
      od_control.od_error = ERR_PARAMETER;
      return;
   }

   /* If AVATAR mode is on. */
   if(od_control.user_avatar)
   {
      /* Position the local cursor. */
      ODScrnSetCursorPos((BYTE)nColumn, (BYTE)nRow);

      /* Generate the AVATAR control sequence to position the remote cursor. */
      szControlSequence[0] = 22;
      szControlSequence[1] = 8;
      szControlSequence[2] = nRow;
      szControlSequence[3] = nColumn;

      /* Transmit the AVATAR control sequence to the remote terminal. */
      od_disp(szControlSequence, 4, FALSE);
   }

   /* If ANSI mode is on. */
   else if(od_control.user_ansi)
   {
      /* Generate the ANSI control sequence to position the remote cursor. */
      sprintf(szControlSequence, "x[%d;%dH", nRow, nColumn);
      szControlSequence[0] = 27;

      /* Transmit the ANSI control seequence to the remote terminal. */
      od_disp(szControlSequence, strlen(szControlSequence), FALSE);

      /* Position the cursor on the local screen. */
      ODScrnSetCursorPos((BYTE)nColumn, (BYTE)nRow);
   }
   else
   {
      /* If neither ANSI nor AVATAR modes are available, indicate this */
      /* in the error code in od_control.                              */
      od_control.od_error = ERR_NOGRAPHICS;
   }

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * od_get_cursor()
 *
 * Returns our best estimate of the current position of the cursor on the
 * remote screen.
 *
 * Parameters: pnRow    - 1-based index of the row to position the cursor in.
 *
 *             pnColumn - Index of the column to position the cursor in.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_get_cursor(INT *pnRow, INT *pnColumn)
{
   tODScrnTextInfo TextInfo;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_get_cursor()");

   /* Ensure that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Check for parameter validity. */
   if(pnRow == NULL && pnColumn == NULL)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return;
   }

   /* Obtain current state of local screen. */
   ODScrnGetTextInfo(&TextInfo);

   /* Set the caller's parameters to the current row and column, if each */
   /* of these parameters were supplied.                                 */
   if(pnRow != NULL) *pnRow = (INT)TextInfo.cury;
   if(pnColumn != NULL) *pnColumn = (INT)TextInfo.curx;

   OD_API_EXIT();
}
