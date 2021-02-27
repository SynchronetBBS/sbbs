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
 *        File: ODBlock.c
 *
 * Description: Implements the text block manipulation functions.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODScrn.h"
#include "ODKrnl.h"


/* Set to TRUE when od_puttext() should leave the cursor in its original */
/* position */
static BOOL bScrollAction = TRUE;



/* ----------------------------------------------------------------------------
 * od_puttext()
 *
 * Displays the contents of the buffer passed in block. Leaves cursor in
 * original position, unless bScrollAction is FALSE. Leaves colour at
 * original value.
 *
 * Parameters: nLeft   - Column number of left edge of block of text to
 *                       transfer, where 1 is the leftmost column of the
 *                       screen.
 *
 *             nTop    - Row number of the top edge of block of text to
 *                       to transfer, where 1 is the top row of the screen.
 *
 *             nRight  - Column number of the right edge of block.
 *
 *             nBottom - Row number of bottom edge of block.
 *
 *             pBlock  - Pointer to buffer that has been filled in the format
 *                       used by od_gettext().
 *
 *     Return: TRUE on success, FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_puttext(INT nLeft, INT nTop, INT nRight, INT nBottom,
   void *pBlock)
{
   INT nRowLength = nRight - nLeft +1;
   INT nRowBytes = nRowLength * 2;
   char *pchTest;
   char *pchMemory;
   char *pBuffer=NULL;
   char *pchScreenBlock;
   INT nBlockRow = 0;
   INT nOutRow;
   INT nOutColour = 999;
   INT nOutColumn, nCheckColumn;
   char *pchMemBlock;
   INT nMoveCost = od_control.user_avatar ? 4 : 7;
   BYTE btMaxRight, btMaxBottom;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_puttext()");

   /* Ensure that OpenDoors is initialized before proceeding. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Get current display setting profile. */
   ODScrnGetTextInfo(&ODTextInfo);

   /* Calculate the maximum values for bottom and right of block. */
   btMaxRight=ODTextInfo.winright-ODTextInfo.winleft+1;
   btMaxBottom=ODTextInfo.winbottom-ODTextInfo.wintop+1;

   /* Check that parameters seem reasonable. */
   if(nLeft<1 || nTop<1 || nRight>btMaxRight || nBottom>btMaxBottom
      || nTop > nBottom || nLeft > nRight || pBlock==NULL)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Ensure that ANSI and/or AVATAR mode is available. */
   if(!od_control.user_ansi && !od_control.user_avatar)
   {
      od_control.od_error = ERR_NOGRAPHICS;
      OD_API_EXIT();
      return(FALSE);
   }

   /* If OpenDoors is operating in remote mode. */
   if(od_control.baud != 0)
   {
      /* Allocate temporary buffer to store original screen contents while */
      /* buffer paste is being performed.                                  */
      if((pBuffer=malloc(nRowBytes*(nBottom-nTop+1)))==NULL)
      {
         od_control.od_error = ERR_MEMORY;
         OD_API_EXIT();
         return(FALSE);
      }

      /* Get current screen contents of area to be pasted into, storing */
      /* into the temporary buffer.                                     */
      if(!ODScrnGetText((BYTE)nLeft, (BYTE)nTop, (BYTE)nRight, (BYTE)nBottom,
         pBuffer))
      {
         od_control.od_error = ERR_PARAMETER;
         free(pBuffer);
         OD_API_EXIT();
         return(FALSE);
      }
   }

   /* Display the block to be pasted on the local screen. */
   if(!ODScrnPutText((BYTE)nLeft, (BYTE)nTop, (BYTE)nRight, (BYTE)nBottom,
      pBlock))
   {
      od_control.od_error = ERR_PARAMETER;
      if(pBuffer)
         free(pBuffer);
      OD_API_EXIT();
      return(FALSE);
   }

   /* If operating in remote mode. */
   if(od_control.baud != 0)
   {
      /* Loop for each line in the buffer to be pasted */
      for(nOutRow=nTop;nOutRow<=nBottom;++nOutRow,++nBlockRow)
      {
         /* Setup pointer to beginning of line of original screen contents. */
         pchScreenBlock=(char *)pBuffer+(nRowBytes*nBlockRow);

         /* Setup pointer to beginning of line of block to be displayed. */
         pchMemBlock=(char *)pBlock+(nRowBytes*nBlockRow);

         /* Loop for each column on this line. */
         for(nOutColumn=0;nOutColumn<nRowLength;)
         {
            /* Loop from this character onwards, counting number of */
            /* characters that don't need to be changed. */
            nCheckColumn=nOutColumn;
            pchMemory=((char *)pchMemBlock)+(nCheckColumn<<1);
            pchTest=((char *)pchScreenBlock)+(nCheckColumn<<1);
            for(;nCheckColumn<nRowLength;++nCheckColumn)
            {
               if(od_control.od_full_put) break;

               /* If both buffers have space characters. */
               if((*pchMemory==' ' || *pchMemory==0) && (*pchTest==' ' || *pchTest=='\0'))
               {
                  /* If background colours differ, then stop comparison loop. */
                  if((pchTest[1]&0x70) != (pchMemory[1]&0x70))
                  {
                     break;
                  }
               }

               /* If both have different character and colour attributes. */
               else if(*((WORD *)pchTest) != *((WORD *)pchMemory))
               {
                  /* Then stop comparison loop now. */
                  break;
               }

               /* Increment source and background pointers by two bytes. */
               pchTest+=2;
               pchMemory+=2;
            }

            /* If no futher text to change on this line. */
            if(nCheckColumn==nRowLength)
            {
               /* Move to the next line. */
               goto next_line;
            }

            /* If this is the first text to be displayed on this line. */
            if(nOutColumn == 0)
            {
               /* Move the cursor to the first text to be changed on line. */
               nOutColumn = nCheckColumn;

               /* If AVATAR mode is available. */
               if(od_control.user_avatar)
               {
                  /* Send the avatar cursor positioning command. */
                  szODWorkString[0]=22;
                  szODWorkString[1]=8;
                  szODWorkString[2]=nOutRow;
                  szODWorkString[3]=nLeft+nOutColumn;
                  od_disp(szODWorkString,4,FALSE);
               }
               else
               {
                  /* Otherwise, send the ANSI cursor positioning command. */
                  sprintf(szODWorkString,"x[%d;%dH",nOutRow,nLeft + nOutColumn);
                  szODWorkString[0]=27;
                  od_disp(szODWorkString, strlen(szODWorkString), FALSE);
               }
            }

            /* If the number of characters after current cursor position  */
            /* which must be changed, is greater than the number of       */
            /* characters required to reposition the cursor on this line, */
            /* then move the cursor to skip unchanged characters.         */
            else if((nCheckColumn-nOutColumn)>nMoveCost)
            {
               nOutColumn=nCheckColumn;
               /* If AVATAR mode is available. */
               if(od_control.user_avatar)
               {
                  /* Advance cursor appropriate number of characters */
                  /* using the AVATAR cursor position command.       */
                  szODWorkString[0]=22;
                  szODWorkString[1]=8;
                  szODWorkString[2]=nOutRow;
                  szODWorkString[3]=nLeft+nOutColumn;
                  od_disp(szODWorkString,4,FALSE);
               }
               else
               {
                  /* Otherwise, advance cursor appropriate number of      */
                  /* characters using the ANSI cursor position command.   */
                  sprintf(szODWorkString,"x[%d;%dH",nOutRow,nLeft + nOutColumn);
                  szODWorkString[0]=27;
                  od_disp(szODWorkString,strlen(szODWorkString),FALSE);
               }
            }

            /* Output text for the number of characters found to be */
            /* different.                                           */
            pchTest=(char *)&pchMemBlock[nOutColumn*2];
            for(;nOutColumn<=nCheckColumn;++nOutColumn)
            {
               if(pchTest[1] != nOutColour)
               {
                  od_set_attrib(nOutColour=pchTest[1]);
               }
               od_disp(pchTest,1,FALSE);
               pchTest++;
               pchTest++;
            }
         }
next_line:
         ;
      }

      /* If not disabled, update cursor position. */
      if(bScrollAction)
      {
         od_set_cursor(ODTextInfo.cury,ODTextInfo.curx);
      }

      /* Deallocate temporary buffer. */
      free(pBuffer);
   }

   /* Restore the original display attribute. */
   od_set_attrib(ODTextInfo.attribute);

   /* Return with success. */
   OD_API_EXIT();
   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * od_gettext()
 *
 * Retrieves text from the screen (based on what is current displayed on the
 * local display), storing it in the buffer provided by the caller.
 *
 * Parameters: nLeft   - Column number of left edge of block of text to
 *                       transfer, where 1 is the leftmost column of the
 *                       screen.
 *
 *             nTop    - Row number of the top edge of block of text to
 *                       to transfer, where 1 is the top row of the screen.
 *
 *             nRight  - Column number of the right edge of block.
 *
 *             nBottom - Row number of bottom edge of block.
 *
 *             pBlock  - Pointer to buffer large enough to hold two bytes
 *                       for each character in the block.
 *
 *     Return: TRUE on success, FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_gettext(INT nLeft, INT nTop, INT nRight, INT nBottom,
   void *pBlock)
{
   BYTE btMaxRight, btMaxBottom;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_gettext()");

   /* Initialize OpenDoors if not already done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   ODScrnGetTextInfo(&ODTextInfo);

   btMaxRight=ODTextInfo.winright-ODTextInfo.winleft+1;
   btMaxBottom=ODTextInfo.winbottom-ODTextInfo.wintop+1;
   if(nLeft<1 || nTop<1 || nRight>btMaxRight || nBottom>btMaxBottom || !pBlock)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   if(!od_control.user_ansi && !od_control.user_avatar)
   {
      od_control.od_error = ERR_NOGRAPHICS;
      OD_API_EXIT();
      return(FALSE);
   }

   OD_API_EXIT();
   return(ODScrnGetText((BYTE)nLeft, (BYTE)nTop, (BYTE)nRight, (BYTE)nBottom,
      pBlock));
}


/* ----------------------------------------------------------------------------
 * od_scroll()
 *
 * Scrolls the specified area of the screen by the specified number of
 * lines, in either the up or down directions. The cursor is left at its
 * original locaiton, and the display attribute is left at its original
 * setting. New lines are created in the current display colour.
 *
 * Parameters: nLeft     - Column number of left edge of area to scroll, where
 *                         1 is the leftmost column of the screen.
 *
 *             nTop      - Row number of the top edge of the area to scroll,
 *                         where 1 is the top row of the screen.
 *
 *             nRight    - Column number of the right edge of area to scroll.
 *
 *             nBottom   - Row number of bottom edge of area to scroll.
 *
 *             nDistance - Number of lines to scroll the text. A value of 0
 *                         has no effect on the specified area, a positive
 *                         value moves the text upwards (leaving blank lines
 *                         at the bottom of the specified area), while a
 *                         negative value moves the text upwards. If the
 *                         distance is equal to the number of lines in the
 *                         area, then the entire area is cleared.
 *
 *             nFlags    - Bitwise-or (|) of SCROLL_* flags. SCROLL_NORMAL
 *                         is the default. SCROLL_NO_CLEAR does not clear
 *                         the new area at the top/bottom of the scroll
 *                         region if doing so would be slower.
 *
 *     Return: TRUE on success, FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_scroll(INT nLeft, INT nTop, INT nRight, INT nBottom,
   INT nDistance, WORD nFlags)
{
   BYTE btWidth, btHeight;
   BYTE btCount;
   BYTE btFirst, btLast;
   char szAVTSeq[7];
   void *pBlock;
   char szBlank[81];
   BYTE btKeepHeight;
   BYTE btMaxRight;
   BYTE btMaxBottom;
   tODScrnTextInfo TextState;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_scroll()");

   /* Ensure that OpenDoors has been initialized before proceeding. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Get current display setting information. */
   ODScrnGetTextInfo(&TextState);

   /* Determine the height and width of the area to be scrolled. */
   btWidth=nRight-nLeft+1;
   btHeight=nBottom-nTop+1;

   /* Determine the number of lines currently in the area that will still */
   /* be visible after scrolling.                                         */
   btKeepHeight=btHeight-((nDistance>=0) ? nDistance : -nDistance);

   /* Determine the maximum bottom and left coordinates of an area to be */
   /* scrolled.                                                          */
   btMaxRight=TextState.winright-TextState.winleft+1;
   btMaxBottom=TextState.winbottom-TextState.wintop+1;

   /* Check that parameters are valid. */
   if(nLeft<1 || nTop<1 || nRight>btMaxRight || nBottom>btMaxBottom ||
      nLeft > nRight || nTop > nBottom)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Check that ANSI or AVATAR graphics mode is available. */
   if(!od_control.user_ansi && !od_control.user_avatar)
   {
      od_control.od_error = ERR_NOGRAPHICS;
      OD_API_EXIT();
      return(FALSE);
   }

   /* If distance to be scrolled is 0, then we are finished already and */
   /* can return immediately.                                           */
   if(nDistance == 0)
   {
      OD_API_EXIT();
      return(TRUE);
   }

   /* If distance is positive, then we are moving text upwards. */
   if(nDistance>0)
   {
      /* Ensure that distance is not greater than size of scrolled area. */
      if(nDistance>btHeight)
      {
         nDistance=btHeight;
      }

      /* Calculate first and last line to be moved. */
      btFirst=nBottom-(nDistance-1);
      btLast=nBottom;
   }

   /* If distance is negative, then we are moving text downwards. */
   else /* if(nDistance<0) */
   {
      /* Ensure that distance is not greater than size of scrolled area. */
      if(nDistance<-btHeight)
      {
         nDistance=-btHeight;
      }

      /* Calculate first and last line to be moved. */
      btFirst=nTop;
      btLast=nTop-nDistance-1;
   }

   /* If AVATAR mode is available */
   if(od_control.user_avatar)
   {
      /* Generate AVATAR sequence which causes the remote terminal to */
      /* scroll an area of its screen.                                */
      szAVTSeq[0]=22;

      /* If scrolling text upwards. */
      if(nDistance>0)
      {
         /* Specify control sequence for scrolling upwards. */
         szAVTSeq[1]=10;
         szAVTSeq[2]=nDistance;

         /* Move text appropriate direction on local screen. */
         ODScrnCopyText((BYTE)nLeft, (BYTE)(nTop + nDistance), (BYTE)nRight,
            (BYTE)nBottom, (BYTE)nLeft, (BYTE)nTop);
      }
      /* If scrolling text downwards. */
      else /* if(disatnce<0) */
      {
         /* Specify control sequence for scrolling downwards. */
         szAVTSeq[1]=11;
         szAVTSeq[2]=-nDistance;

         /* Move text appropriate direction on local screen. */
         ODScrnCopyText((BYTE)nLeft, (BYTE)nTop, (BYTE)nRight,
            (BYTE)(nBottom + nDistance), (BYTE)nLeft, (BYTE)(nTop - nDistance));
      }

      /* Specify area to be scrolled to the AVATAR terminal. */
      szAVTSeq[3]=nTop;
      szAVTSeq[4]=nLeft;
      szAVTSeq[5]=nBottom;
      szAVTSeq[6]=nRight;

      /* Send the control sequence to the AVATAR terminal. */
      od_disp(szAVTSeq,7,FALSE);

      /* Generate string containing a blank line of text. */
      for(btCount=0;btCount<btWidth;++btCount) szBlank[btCount]=' ';
      szBlank[btCount]='\0';

      /* Blank-out lines that will no longer be visiable. */
      for(;btFirst<=btLast;++btFirst)
      {
         ODScrnSetCursorPos((BYTE)nLeft, btFirst);
         ODScrnDisplayString(szBlank);
      }

      /* Reset cursor position on local display. */
      ODScrnSetCursorPos(TextState.curx,TextState.cury);
   }

   /* Otherwise, we are using ANSI mode. */
   else /* if(od_control.user_ansi) */
   {
      /* If any of the original text will still be available after */
      /* scrolling.                                                */
      if(btKeepHeight>0)
      {
         /* Allocate some temporary memory to hold text to be "got". */
         if((pBlock=malloc(btKeepHeight*btWidth*2))==NULL)
         {
            /* If memory allocation failed, then scrolling fails. */
            od_control.od_error = ERR_MEMORY;
            OD_API_EXIT();
            return(FALSE);
         }

         /* If we are scrolling text upwards. */
         if(nDistance > 0)
         {
            /* Move text that will still be visible, using od_gettext() */
            /* and od_puttext().                                        */
            od_gettext(nLeft,nTop+nDistance,nRight,nBottom,pBlock);
            bScrollAction=FALSE;
            od_puttext(nLeft,nTop,nRight,nBottom-nDistance,pBlock);
            bScrollAction=TRUE;
         }

         /* If we are scrolling text downwards. */
         else /* if(nDistance < 0) */
         {
            /* Move text that will still be visible, using od_gettext() */
            /* and od_puttext().                                        */
            od_gettext(nLeft,nTop,nRight,nBottom+nDistance,pBlock);
            bScrollAction=FALSE;
            od_puttext(nLeft,nTop-nDistance,nRight,nBottom,pBlock);
            bScrollAction=TRUE;
         }

         /* Deallocate temporary memory block. */
         free(pBlock);
      }

      /* If new area clearing has not been disabled. */
      if(!(nFlags&SCROLL_NO_CLEAR))
      {
         /* Loop for lines that should be blank. */
         for(;btFirst<=btLast;++btFirst)
         {
            /* Move cursor to the beginning of this line. */
            od_set_cursor(btFirst,nLeft);

            /* If right boarder of area to be scrolled is the edge of the */
            /* screen, then we can use a quick control sequence to clear  */
            /* the rest of the line. Call od_clr_line() to do this.       */
            if(nRight == 80)
            {
               od_clr_line();
            }

            /* If right boarder of area to be scrolled is not at the edge */
            /* of the screen, then each line must be manually erased, by  */
            /* sending the appropriate number of blanks (spaces).         */
            else /* if(right != 80) */
            {
               od_repeat(' ',btWidth);
            }
         }
      }

      /* Reset the cursor to its original position. */
      od_set_cursor(TextState.cury,TextState.curx);
   }

   /* Return with success */
   OD_API_EXIT();
   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * od_save_screen()
 *
 * Stores the contents of the entire screen into a buffer, along with
 * the current cursor location and display colour. Supports all display
 * modes.
 *
 * Parameters: pBuffer - Pointer to a buffer of at least 4004 bytes in size,
 *                       where the information on the current screen state
 *                       will be stored.
 *
 *     Return: TRUE on success, FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_save_screen(void *pBuffer)
{
   char btHeight;
   tODScrnTextInfo TextState;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_save_screen()");

   /* Ensure that OpenDoors is initialized before proceeding. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Check parameters and current output window size. */
   ODScrnGetTextInfo(&TextState);
   if(TextState.winleft!=1 || TextState.winright!=80 || !pBuffer)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Store current cursor location in buffer. */
   ((char *)pBuffer)[0]=TextState.curx;
   ((char *)pBuffer)[1]=TextState.cury;

   /* Store current display colour in buffer. */
   ((char *)pBuffer)[2]=TextState.attribute;

   /* Store height of buffer stored. */
   ((char *)pBuffer)[3]=btHeight=TextState.winbottom-TextState.wintop+1;

   /* Store screen contents using local screen gettext() function. */
   OD_API_EXIT();
   return(ODScrnGetText(1,1,80,btHeight,(char *)pBuffer+4));
}


/* ----------------------------------------------------------------------------
 * od_restore_screen()
 *
 * Restores the screen contents, along with the current text colour and cursor
 * location, as previously stored by od_save_screen().
 *
 * Parameters: pBuffer - Pointer to buffer which was filled by a previous call
 *                       to od_save_screen().
 *
 *     Return: TRUE on success, FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_restore_screen(void *pBuffer)
{
   char *pch;
   char btPos;
   char chLast;
   char *pchTextBuffer;
   char btHeight;
   int nToReturn=TRUE;
   tODScrnTextInfo TextState;
   char btLine;
   char btDistance=0;
   char btCursorRow;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_restore_screen()");

   /* Ensure that OpenDoors is initialized before proceeding. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Check parameters and current output window size. */
   ODScrnGetTextInfo(&TextState);
   if(TextState.winleft!=1 || TextState.winright!=80 || !pBuffer)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Determine current window height were text will be restored. */
   btHeight=TextState.winbottom-TextState.wintop+1;

   /* If current display window height is too small for entire buffer */
   /* to be re-displayed.                                             */
   if(btHeight<((char *)pBuffer)[3])
   {
      /* Then we will always display as much of the END of the buffer */
      /* as possible.                                                 */
      btDistance = btHeight - ((char *)pBuffer)[3];
   }
   else if(btHeight > ((char *)pBuffer)[3])
   {
      /* Otherwise, ensure that we don't try to display more lines that */
      /* are in the buffer.                                             */
      btHeight=((char *)pBuffer)[3];
   }

   /* Clear the remote and local screens. */
   od_clr_scr();

   /* If ANSI or AVATAR modes are available. */
   if(od_control.user_avatar || od_control.user_ansi)
   {
      /* Then we can efficiently display the buffer using od_puttext(). */
      bScrollAction=FALSE;
      nToReturn=od_puttext(1,1,80,btHeight,(char *)pBuffer+(4+((int)btDistance*160)));
      bScrollAction=TRUE;

      /* Restore original cursor location. */
      od_set_cursor(((char *)pBuffer)[1],((char *)pBuffer)[0]);

      /* Restore original display attribute. */
      od_set_attrib(((char *)pBuffer)[2]);
   }

   /* If we are operating in ASCII mode. */
   else /* if (!od_control.od_avatar && !od_control.caller_ansi) */
   {
      /* Then the buffer is displayed one line at a time, beginning  */
      /* at the top of the screen, up to the saved cusrsor location. */

      /* Set pointer to beginning of buffer to be displayed. */
      pchTextBuffer=(char *)pBuffer+4;

      /* Get final cursor row number. */
      btCursorRow=((char *)pBuffer)[1];

      /* Loop for each line in the buffer. */
      for(btLine=1;btLine<=btHeight;++btLine)
      {
         /* Set pointer to last character of line. */
         pch=(char *)pchTextBuffer+158;

         /* Loop backwards until a non-blank character is found, or we */
         /* reach the beginning of the line.                           */
         for(chLast=80;chLast>1;)
         {
            /* If this is a blank character. */
            if(*pch==32 || *pch==0)
            {
               /* Move to previous character. */
               --chLast;
               pch-=2;
            }

            /* If this is not a blank character, then stop looping. */
            else
            {
               break;
            }
         }

         /* If this is the line on which the cursor resides. */
         if(btLine==btCursorRow)
         {
            /* If last non-blank character of line is at or past the final */
            /* cursor location, then we backup the last character to be    */
            /* displayed to the cursor before the final cursor position.   */
            /* This code could be improved to be able to display text on   */
            /* the entire cursor line by displaying the entire line,       */
            /* sending a C/R, and redisplaying first portion of line to    */
            /* end up with the cursor in the desired position.             */
            if(chLast>=((char *)pBuffer)[0])
            {
               chLast=((char *)pBuffer)[0]-1;
            }
         }

         /* Display all characters on this line */
         pch = (char *)pchTextBuffer;
         for(btPos=1;btPos<=chLast;++btPos)
         {
            od_putch(*pch);
            pch+=2;
         }

         /* If this is the row where the cursor should be left, then we */
         /* stop displaying now.                                        */
         if(btLine==btCursorRow)
         {
            break;
         }

         /* If cursor hasn't been wrapped, then we should send a C/R - */
         /* L/F sequence.                                              */
         if(chLast != 80)
         {
            od_disp_str("\n\r");
            pchTextBuffer+=160;
         }
      }
   }

   /* Return with the appropriate success/failure status. */
   OD_API_EXIT();
   return(nToReturn);
}
