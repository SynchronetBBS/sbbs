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
 *        File: ODWin.c
 *
 * Description: Implements the od_window_...() functions for creating
 *              and removing text-mode windows.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Nov 01, 1994  6.00  BP   Include stdlib.h for malloc prototype.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Dec 12, 1994  6.00  BP   Set od_error on window remove failure.
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <stdlib.h>

#include "OpenDoor.h"
#include "ODGen.h"
#include "ODCore.h"
#include "ODKrnl.h"


/* ----------------------------------------------------------------------------
 * od_window_create()
 *
 * Creates a window on the screen, storing information on the original screen
 * contents "under" the window in order to restore the screen after the window
 * is removed. A window that is created with this function must be destroyed
 * by od_window_remove() in order to free up memory that is allocated by this
 * function.
 *
 * Parameters: nLeft        - 1-based column number of left edge of window.
 *
 *             nTop         - 1-based row number of top edge of window.
 *
 *             nRight       - 1-based column number of right edge of window.
 *
 *             nBottom      - 1-based row number of bottom edge of window.
 *
 *             pszTitle     - Pointer to a string containing title for window.
 *                            If this string is empty, no title is displayed.
 *
 *             btBoarderCol - Colour of window boarder.
 *
 *             btTitleCol   - Colour of window title.
 *
 *             btInsideCol  - Colour of rest of window.
 *
 *             nReserved    - Should always be 0 for this version.
 *
 *     Return: Pointer to window description buffer (which must later be
 *             passed to od_window_remove(), or NULL on failure.
 */
ODAPIDEF void * ODCALL od_window_create(INT nLeft, INT nTop, INT nRight,
   INT nBottom, char *pszTitle, BYTE btBorderCol, BYTE btTitleCol,
   BYTE btInsideCol, INT nReserved)
{
   BYTE btLine;
   BYTE btBetweenSize;
   void *pBuffer;
   BYTE btTitleSize;
   BYTE btRemaining;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_window_create()");

   /* Ensure that OpenDoors has been initialized */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   nReserved &= 0x00;

   btBetweenSize = (nRight - nLeft) - 1;

   /* Setup od_box_chars appropriately. */
   if(od_control.od_box_chars[BOX_BOTTOM]==0)
   {
      od_control.od_box_chars[BOX_BOTTOM] = od_control.od_box_chars[BOX_TOP];
   }
   if(od_control.od_box_chars[BOX_RIGHT]==0)
   {
      od_control.od_box_chars[BOX_RIGHT] = od_control.od_box_chars[BOX_LEFT];
   }

   /* Ensure that the current display mode can support the capabilities */
   /* required to display and remove windows.                           */
   if(!(od_control.user_ansi || od_control.user_avatar))
   {
      od_control.od_error = ERR_NOGRAPHICS;
      OD_API_EXIT();
      return(NULL);
   }

   /* Validate parameters. */
   if(nLeft < 1 || nTop < 1 || nRight > 80 || nBottom > 25 || nRight-nLeft < 2
      || nBottom-nTop < 2)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(NULL);
   }

   /* Allocate a buffer large enough to hold all window information. */
   if((pBuffer = malloc((nRight - nLeft + 1) * 2 + (nBottom - nTop + 1) * 160
      + 4)) == NULL)
   {
      od_control.od_error = ERR_MEMORY;
      OD_API_EXIT();
      return(NULL);
   }

   /* Store current contents of screen where window will be drawn. */
   if(!od_gettext(nLeft, nTop, nRight, nBottom, (char *)pBuffer+4))
   {
      free(pBuffer);

      /* Note: od_control.od_error code has been set by od_gettext(). */

      OD_API_EXIT();
      return(NULL);
   }

   /* Store window information in buffer. */
   ((char *)pBuffer)[0]=nLeft;
   ((char *)pBuffer)[1]=nTop;
   ((char *)pBuffer)[2]=nRight;
   ((char *)pBuffer)[3]=nBottom;

   /* Determine number of characters of title to display. */
   if(pszTitle==NULL)
   {
      btTitleSize = 0;
   }
   else
   {
      if((btTitleSize = strlen(pszTitle)) > (btBetweenSize - 4))
      {
         btTitleSize = btBetweenSize - 4;
      }
   }

   /* Move to position of window's top corner, prepare to begin drawing the */
   /* window.                                                               */
   od_set_cursor(nTop,nLeft);
   od_set_attrib(btBorderCol);

   /* Display corner character. */
   od_putch(od_control.od_box_chars[BOX_UPPERLEFT]);

   /* If there is no title, display top line all in one piece. */
   if(btTitleSize == 0)
   {
      /* Display top line. */
      od_repeat(od_control.od_box_chars[BOX_TOP],btBetweenSize);
   }
   else
   {
      /* If there is a title, display the top line with a title centered in */
      /* it.                                                                */
      od_repeat(od_control.od_box_chars[BOX_TOP],btRemaining =
         ((btBetweenSize - btTitleSize - 2) / 2));
      od_set_attrib(btTitleCol);
      od_putch(' ');
      od_disp(pszTitle,btTitleSize,TRUE);
      od_putch(' ');
      od_set_attrib(btBorderCol);
      od_repeat(od_control.od_box_chars[BOX_TOP],
         (BYTE)(btBetweenSize - btRemaining - btTitleSize - 2));
   }

   /* Display top right corner character. */
   od_putch(od_control.od_box_chars[BOX_UPPERRIGHT]);

   /* If AVATAR mode is available. */
   if(od_control.user_avatar)
   {
      /* Display first left verticle line. */
      od_set_cursor(nTop + 1, nLeft);
      od_putch(od_control.od_box_chars[BOX_LEFT]);

      /* Fill in center of window with AVATAR clear area control sequence. */
      od_emulate(22);
      od_emulate(12);
      od_emulate(btInsideCol);
      od_emulate((BYTE)((nBottom - nTop) - 1));
      od_emulate(btBetweenSize);

      od_set_attrib(btBorderCol);
      od_set_cursor(nTop + 1 , nRight);

      /* Display first right verticle line. */
      od_putch(od_control.od_box_chars[BOX_RIGHT]);

      /* Display remaining verticle lines. */
      for(btLine=nTop+2;btLine<nBottom;++btLine)
      {
         /* Move to line start and display left line character. */
         od_set_cursor(btLine,nLeft);
         od_putch(od_control.od_box_chars[BOX_LEFT]);

         /* Move to line start and display right line character. */
         od_set_cursor(btLine,nRight);
         od_putch(od_control.od_box_chars[BOX_RIGHT]);
      }
   }

   /* If AVATAR mode is not available. */
   else
   {
      /* Loop through middle lines of window. */
      for(btLine=nTop+1;btLine<nBottom;++btLine)
      {
         /* Move to line start and display left line character. */
         od_set_cursor(btLine,nLeft);
         od_putch(od_control.od_box_chars[BOX_LEFT]);

         /* Set window colour. */
         od_set_attrib(btInsideCol);

         /* display blank area between left and right vertical lines. */
         od_repeat(' ',btBetweenSize);

         /* Set border colour. */
         od_set_attrib(btBorderCol);

         /* Display right line. */
         od_putch(od_control.od_box_chars[BOX_RIGHT]);
      }
   }

   /* Display bottom border of window */
   od_set_cursor(nBottom,nLeft);
   od_putch(od_control.od_box_chars[BOX_LOWERLEFT]);
   od_repeat(od_control.od_box_chars[BOX_BOTTOM],btBetweenSize);
   od_putch(od_control.od_box_chars[BOX_LOWERRIGHT]);

   /* Return a pointer to the window information buffer. */
   OD_API_EXIT();
   return(pBuffer);
}


/* ----------------------------------------------------------------------------
 * od_window_remove()
 *
 * Removes window from the screen, restoring the screen contents that where
 * in the window area when the window was first created.
 *
 * Parameters: pWinInfo - Pointer to buffer returned by od_window_create().
 *                        This buffer is deallocated before od_window_remove()
 *                        returns.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_window_remove(void *pWinInfo)
{
   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_window_remove()");

   /* Ensure that OpenDoors has been initialized */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   if(pWinInfo == NULL)
   {
      /* Set error code and return with failure. */
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   if(!od_puttext(((char *)pWinInfo)[0], ((char *)pWinInfo)[1], ((char *)pWinInfo)[2], ((char *)pWinInfo)[3], (char *)pWinInfo + 4))
   {
      /* Deallocate memory assigned to window information structure. */
      free(pWinInfo);

      /* Note: od_control.od_error code has been set by od_puttext(). */

      /* Return with failure. */
      OD_API_EXIT();
      return(FALSE);
   }

   /* Deallocate memory assigned to window information structure. */
   free(pWinInfo);

   /* Return with success. */
   OD_API_EXIT();
   return(TRUE);
}
