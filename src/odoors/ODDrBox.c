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
 *        File: ODDrBox.c
 *
 * Description: Implements the od_draw_box() function.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODKrnl.h"


/* ----------------------------------------------------------------------------
 * od_draw_box()
 *
 * Draws a box on the local and remote screens, using the box characters
 * specified in od_control.od_box_chars. Unlike the window functions, this
 * function does not store the original contents of the screen where the box
 * is drawn.
 *
 * Parameters: btLeft   - Column number of the left side of the box.
 *
 *             btTop    - Row number of the top side of the box.
 *
 *             btRight  - Column number of hte right side of the box.
 *
 *             btBottom - Row number of the bottom side of the box.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_draw_box(BYTE btLeft, BYTE btTop, BYTE btRight,
   BYTE btBottom)
{
   /* Number of current line being drawn. */
   BYTE btLine;

   /* X size of window. */
   BYTE btBetweenSize = (btRight - btLeft) - 1;

   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_draw_box()");

   /* Ensure that OpenDoors has been initialized */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Setup od_box_chars appropriately */
   if(od_control.od_box_chars[BOX_BOTTOM] == 0)
   {
      od_control.od_box_chars[BOX_BOTTOM] = od_control.od_box_chars[BOX_TOP];
   }
   if(od_control.od_box_chars[BOX_RIGHT] == 0)
   {
      od_control.od_box_chars[BOX_RIGHT] = od_control.od_box_chars[BOX_LEFT];
   }

   /* Check that required display capabilities are supported. */
   if(!(od_control.user_ansi || od_control.user_avatar))
   {
      od_control.od_error = ERR_NOGRAPHICS;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Check that parameters are within valid range. */
   if(btLeft<1 || btTop<1 || btRight>80 || btBottom>25)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Move to top corner, if needed. */
   od_set_cursor(btTop, btLeft);

   /* Display left corner character. */
   od_putch(od_control.od_box_chars[BOX_UPPERLEFT]);

   /* Display top line. */
   od_repeat(od_control.od_box_chars[BOX_TOP], btBetweenSize);

   /* Display right corner character. */
   od_putch(od_control.od_box_chars[BOX_UPPERRIGHT]);

   /* If AVATAR display mode is available. */
   if(od_control.user_avatar)                    
   {
      /* Display first left vertical line. */
      od_set_cursor(btTop + 1, btLeft);
      od_putch(od_control.od_box_chars[BOX_LEFT]);

      /* Fill in the center of the window. */
      od_emulate(22);
      od_emulate(12);
      od_emulate((BYTE)od_control.od_cur_attrib);
      od_emulate((BYTE)((btBottom - btTop) - 1));
      od_emulate(btBetweenSize);

      /* Display first right vertical line. */
      od_set_cursor(btTop + 1, btRight);
      od_putch(od_control.od_box_chars[BOX_RIGHT]);

      /* Display remaining vertical lines. */
      for(btLine = btTop + 2; btLine < btBottom; ++btLine)
      {
         /* Move to the start of the line. */
         od_set_cursor(btLine, btLeft);

         /* Display left line character. */
         od_putch(od_control.od_box_chars[BOX_LEFT]);

         /* Move to line start. */
         od_set_cursor(btLine, btRight);

         /* Display right line character. */
         od_putch(od_control.od_box_chars[BOX_RIGHT]);
      }
   }

   /* If AVATAR mode is not available. */
   else
   {
      /* Loop through middle lines of window. */
      for(btLine = btTop + 1; btLine < btBottom; ++btLine)
      {
         /* Move to the start of the line. */
         od_set_cursor(btLine,btLeft);

         /* Display left line character. */
         od_putch(od_control.od_box_chars[BOX_LEFT]);

         /* Display the blank area. */
         od_repeat(' ', btBetweenSize);

         /* Display the right line character. */
         od_putch(od_control.od_box_chars[BOX_RIGHT]);
      }
   }

   /* Move to bottom corner. */
   od_set_cursor(btBottom, btLeft);

   /* Display left corner character. */
   od_putch(od_control.od_box_chars[BOX_LOWERLEFT]);

   /* Display bottom line. */
   od_repeat(od_control.od_box_chars[BOX_BOTTOM], btBetweenSize);

   /* Display right corner character. */
   od_putch(od_control.od_box_chars[BOX_LOWERRIGHT]);

   /* Return with success. */
   OD_API_EXIT();
   return(TRUE);
}
