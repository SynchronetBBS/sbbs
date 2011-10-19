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
 *        File: ODEdit.c
 *
 * Description: Implementation of the OpenDoors multi-line editor, which
 *              allows the user to edit strings which may span many lines.
 *              Provides standard text editor features, such as word wrap.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Dec 07, 1995  6.00  BP   Created.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 04, 1996  6.00  BP   Use od_get_input().
 *              Jan 12, 1996  6.00  BP   Claim exclusive use of arrow keys.
 *              Jan 31, 1996  6.00  BP   Added timeout for od_get_input().
 *              Feb 08, 1996  6.00  BP   Finished implementation details.
 *              Feb 13, 1996  6.00  BP   Added od_get_input() flags parameter.
 *              Feb 16, 1996  6.00  BP   New trans. size estimation heuristics.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 24, 1996  6.00  BP   Fixed garbage on [Enter] after w-wrap.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 13, 1996  6.10  BP   Restore cursor position after menu.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Jun 08, 1996  6.10  BP   Added cast in call to alloc function.
 *              Oct 19, 2001  6.20  RS   Eliminated MSVC 6.0 warning.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "OpenDoor.h"
#include "ODTypes.h"
#include "ODGen.h"
#include "ODCore.h"
#include "ODKrnl.h"
#include "ODStat.h"
#include "ODCom.h"
#include "ODScrn.h"


/* ========================================================================= */
/* Misc. definitions.                                                        */
/* ========================================================================= */

/* Macros used by this module. */
#define IS_EOL_CHAR(ch)       ((ch) == '\n' || (ch) == '\r' || (ch) == '\0')

/* Configurable constants. */
#define LINE_ARRAY_GROW_SIZE     20
#define BUFFER_GROW_SIZE         4096
#define ANSI_SCROLL_DISTANCE     7
#define PRE_DRAIN_TIME           10000
#define MAX_TAB_STOP_SIZE        8
#define DEFAULT_TAB_STOP_SIZE    8
#define DEFAULT_LINE_BREAK       "\n"

/* Other manifest constants. */
#define REDRAW_NO_BOUNDARY       0xffff

/* Default editor options. */
static tODEditOptions ODEditOptionsDefault =
{
   1, 1, 80, 23,
   FORMAT_PARAGRAPH_BREAKS,
   NULL,
   NULL,
   EFLAG_NORMAL,
};



/* ========================================================================= */
/* Multiline editor instance structure.                                      */
/* ========================================================================= */

typedef struct
{
   char *pszEditBuffer;
   UINT unBufferSize;
   tODEditOptions *pUserOptions;
   UINT unCurrentLine;
   UINT unCurrentColumn;
   UINT unLineScrolledToTop;
   UINT unAreaWidth;
   UINT unAreaHeight;
   char **papchStartOfLine;
   UINT unLineArraySize;
   UINT unLinesInBuffer;
   BOOL bInsertMode;
   UINT unTabStopSize;
   UINT unScrollDistance;
   char *pszLineBreak;
   char *pszParagraphBreak;
   BOOL bWordWrapLongLines;
   void *pRememberBuffer;
} tEditInstance;



/* ========================================================================= */
/* Editor function prototypes.                                               */
/* ========================================================================= */

/* High level implementation. */
static BOOL ODEditSetupInstance(tEditInstance *pEditInstance,
   char *pszBufferToEdit, UINT unBufferSize, tODEditOptions *pUserOptions);
static void ODEditRedrawArea(tEditInstance *pEditInstance);
static void ODEditDrawAreaLine(tEditInstance *pEditInstance,
   UINT unAreaLineToDraw);
static INT ODEditMainLoop(tEditInstance *pEditInstance);
static void ODEditGotoPreviousLine(tEditInstance *pEditInstance);
static void ODEditGotoNextLine(tEditInstance *pEditInstance);
static BOOL ODEditScrollArea(tEditInstance *pEditInstance, INT nDistance);
static BOOL ODEditRecommendFullRedraw(tEditInstance *pEditInstance,
   UINT unEstPartialRedrawBytes, BOOL bDefault);
static UINT ODEditEstDrawBytes(tEditInstance *pEditInstance,
   UINT unStartRedrawLine, UINT unStartRedrawColumn, UINT unFinishRedrawLine,
   UINT unFinishRedrawColumn);
static UINT ODEditGetCurrentLineInArea(tEditInstance *pEditInstance);
static void ODEditUpdateCursorPos(tEditInstance *pEditInstance);
static void ODEditUpdateCursorIfMoved(tEditInstance *pEditInstance);
static tODResult ODEditEnterText(tEditInstance *pEditInstance,
   char *pszEntered, BOOL bInsertMode);
static void ODEditSetBreakSequence(tEditInstance *pEditInstance,
   char chFirstEOLChar, char chSecondEOLChar);
static BOOL ODEditCursorLeft(tEditInstance *pEditInstance);
static void ODEditDeleteCurrentChar(tEditInstance *pEditInstance);
static void ODEditDeleteCurrentLine(tEditInstance *pEditInstance);
static BOOL ODEditPastEndOfCurLine(tEditInstance *pEditInstance);
static size_t ODEditRememberBufferSize(tEditInstance *pEditInstance);
static void ODEditRememberArea(tEditInstance *pEditInstance,
   void *pRememberedArea);
static void ODEditRedrawChanged(tEditInstance *pEditInstance,
   void *pRememberedArea, UINT unUpperBoundary, UINT unLowerBoundary);
static BOOL ODEditDetermineChanged(tEditInstance *pEditInstance,
   void *pRememberedArea, UINT unUpperBoundary, UINT unLowerBoundary, 
   UINT *punStartRedrawLine, UINT *punStartRedrawColumn,
   UINT *punFinishRedrawLine, UINT *punFinishRedrawColumn);
static void ODEditRedrawSubArea(tEditInstance *pEditInstance,
   UINT unStartRedrawLine, UINT unStartRedrawColumn, UINT unFinishRedrawLine,
   UINT unFinishRedrawColumn);
static void ODEditGetActualCurPos(tEditInstance *pEditInstance,
   UINT *punRow, UINT *punColumn);
static BOOL ODEditIsEOLForMode(tEditInstance *pEditInstance, char chToTest);

/* Low level buffer manipulation functions. */
static BOOL ODEditBufferFormatAndIndex(tEditInstance *pEditInstance);
static UINT ODEditBufferGetLineLength(tEditInstance *pEditInstance,
   UINT unBufferLine);
static UINT ODEditBufferGetTotalLines(tEditInstance *pEditInstance);
static char *ODEditBufferGetCharacter(tEditInstance *pEditInstance,
   UINT unBufferLine, UINT unBufferColumn);
static tODResult ODEditBufferMakeSpace(tEditInstance *pEditInstance,
   UINT unLine, UINT unColumn, UINT unNumChars);
static tODResult ODEditTryToGrow(tEditInstance *pEditInstance,
   UINT unSizeNeeded);



/* ========================================================================= */
/* High level editor implementation.                                         */
/* ========================================================================= */

/* ----------------------------------------------------------------------------
 * od_multiline_edit()
 *
 * Multiline editor function, allows the user to enter or change text that
 * spans multiple lines.
 *
 * Parameters: pszBufferToEdit  - Pointer to '\0'-terminated buffer of text
 *                                to edit.
 *
 *             unBufferSize     - Size of the buffer, in characters.
 *
 *             pEditOptions     - Pointer to a tODEditOptions structure, or
 *                                NULL to use default settings.
 *
 *     Return: An od_multiline_edit()-specific result code.
 */
ODAPIDEF INT ODCALL od_multiline_edit(char *pszBufferToEdit, UINT unBufferSize,
   tODEditOptions *pEditOptions)
{
   tEditInstance EditInstance;
   INT nToReturn;

   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_node_open()");

   /* Initialize OpenDoors if not already done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Validate parameters. */
   if(pszBufferToEdit == NULL || unBufferSize == 0)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(OD_MULTIEDIT_ERROR);
   }

   /* Check the user's terminal supports the required capabilities. */
   if(!(od_control.user_ansi || od_control.user_avatar))
   {
      od_control.od_error = ERR_NOGRAPHICS;
      OD_API_EXIT();
      return(OD_MULTIEDIT_ERROR);
   }

   /* Initialize editor instance information structure. */
   if(!ODEditSetupInstance(&EditInstance, pszBufferToEdit, unBufferSize,
      pEditOptions))
   {
      OD_API_EXIT();
      return(OD_MULTIEDIT_ERROR);
   }

   /* Attempt to build the buffer line index and ensure that the buffer */
   /* conforms to the format specified by the client application.       */
   if(!ODEditBufferFormatAndIndex(&EditInstance))
   {
      od_control.od_error = ERR_MEMORY;
      OD_API_EXIT();
      return(OD_MULTIEDIT_ERROR);
   }

   /* Claim exclusive use of arrow keys. */
   ODStatStartArrowUse();

   /* Ensure that all information in the outbound communications buffer  */
   /* has been sent before starting. This way, we can safely purge the   */
   /* outbound buffer at any time without loosing anything that was sent */
   /* before od_multiline_edit() was called.                             */
   ODWaitDrain(PRE_DRAIN_TIME);

   /* Draw the initial edit area. */
   ODEditRedrawArea(&EditInstance);

   /* Run the main editor loop. */
   nToReturn = ODEditMainLoop(&EditInstance);

   /* Release exclusive use of arrow keys. */
   ODStatEndArrowUse();

   /* Set final information which will be available in the user options */
   /* structure for the client application to access.                   */
   EditInstance.pUserOptions->pszFinalBuffer = EditInstance.pszEditBuffer;
   EditInstance.pUserOptions->unFinalBufferSize = unBufferSize;

   OD_API_EXIT();
   return(nToReturn);
}


/* ----------------------------------------------------------------------------
 * ODEditSetupInstance()                               *** PRIVATE FUNCTION ***
 *
 * Initializes editor instance information structure.
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *             pszBufferToEdit - Buffer pointer provided by client.
 *
 *             unBufferSize    - Initial buffer size, as specified by the
 *                               client.
 *
 *             pUserOptions    - Editor options specified by the client.
 *
 *     Return: TRUE on success, FALSE on failure. In the case of failure,
 *             od_control.od_error is set appropriately.
 */
static BOOL ODEditSetupInstance(tEditInstance *pEditInstance,
   char *pszBufferToEdit, UINT unBufferSize, tODEditOptions *pUserOptions)
{
   ASSERT(pEditInstance != NULL);
   ASSERT(pszBufferToEdit != NULL);

   /* Setup editor instance structure. */
   pEditInstance->pszEditBuffer = pszBufferToEdit;
   pEditInstance->unBufferSize = unBufferSize;
   if(pUserOptions == NULL)
   {
      /* Edit options is just the defaults. */
      pEditInstance->pUserOptions = &ODEditOptionsDefault;
   }
   else
   {
      /* Edit options are supplied by the user. */
      pEditInstance->pUserOptions = pUserOptions;

      /* Initialize any edit options that the user did not setup. */
      /* Check that edit area has been initialized. */
      if(pUserOptions->nAreaLeft == 0)
      {
         pUserOptions->nAreaLeft = ODEditOptionsDefault.nAreaLeft;
      }
      if(pUserOptions->nAreaRight == 0)
      {
         pUserOptions->nAreaRight = ODEditOptionsDefault.nAreaRight;
      }
        if(pUserOptions->nAreaTop == 0)
      {
         pUserOptions->nAreaTop = ODEditOptionsDefault.nAreaTop;
      }
      if(pUserOptions->nAreaBottom == 0)
      {
         pUserOptions->nAreaBottom = ODEditOptionsDefault.nAreaBottom;
      }
   }
   pEditInstance->unCurrentLine = 0;
   pEditInstance->unCurrentColumn = 0;
   pEditInstance->unLineScrolledToTop = 0;
   pEditInstance->papchStartOfLine = NULL;
   pEditInstance->unLineArraySize = 0;
   pEditInstance->unLinesInBuffer = 0;
   pEditInstance->unAreaWidth = (UINT)pEditInstance->pUserOptions->
      nAreaRight - (UINT)pEditInstance->pUserOptions->nAreaLeft + 1;
   pEditInstance->unAreaHeight = (UINT)pEditInstance->pUserOptions->
      nAreaBottom - (UINT)pEditInstance->pUserOptions->nAreaTop + 1;
   pEditInstance->bInsertMode = TRUE;
   pEditInstance->unTabStopSize = DEFAULT_TAB_STOP_SIZE;

   /* Setup line break and paragraph break sequences, if they can be  */
   /* determined at this point. If they can't be determined, set them */
   /* to NULL.                                                        */
   switch(pEditInstance->pUserOptions->TextFormat)
   {
      case FORMAT_FTSC_MESSAGE:
         /* FTSC compliant messages use \r as a paragraph break, and do */
         /* not have any line break characters.                         */
         pEditInstance->pszLineBreak = "";
         pEditInstance->pszParagraphBreak = "\r";
         break;

      case FORMAT_PARAGRAPH_BREAKS:
         /* Paragraph break mode only inserts CR/LF sequences at the end  */
         /* of a paragrah, and word-wraps the text that forms a paragrah. */
         pEditInstance->pszLineBreak = "";
         pEditInstance->pszParagraphBreak = NULL;
         break;

      case FORMAT_LINE_BREAKS:
      case FORMAT_NO_WORDWRAP:
         /* Line break mode and no word wrap mode both terminate every    */
         /* line of the file with a CR/LF sequence, and have no paragrah  */
         /* terminator. In line break mode, word wrap is enabled, whereas */
         /* it is not in FORMAT_NO_WORDWRAP mode.                         */
         pEditInstance->pszLineBreak = NULL;
         pEditInstance->pszParagraphBreak = "";
         break;

      default:
         /* An invalid text format was specified. */
         od_control.od_error = ERR_PARAMETER;
         return(FALSE);
   }

   /* Determine whether long lines sould be word wrapped or character */
   /* wrapped.                                                        */
   pEditInstance->bWordWrapLongLines = (pEditInstance->pUserOptions->TextFormat
      != FORMAT_NO_WORDWRAP);

   /* Attempt to allocate abuffer for remembered data. */
   pEditInstance->pRememberBuffer =
      malloc(ODEditRememberBufferSize(pEditInstance));
   
   if(pEditInstance->pRememberBuffer == NULL)
   {
      od_control.od_error = ERR_MEMORY;
      return(FALSE);
   }

   /* If AVATAR mode or local mode is active, then scroll up or down one */
   /* line at a time.                                                    */
   if(od_control.user_avatar || od_control.baud == 0)
   {
      pEditInstance->unScrollDistance = 1;
   }
   /* In ANSI mode with a remote connection, scroll multiple lines at a */
   /* time. This is the minimum of the default scroll distance, and the */
   /* current height of the edit area - 1.                              */
   else
   {
      pEditInstance->unScrollDistance = MIN(ANSI_SCROLL_DISTANCE,
         pEditInstance->pUserOptions->nAreaBottom
         - pEditInstance->pUserOptions->nAreaTop);
   }

   /* Return with success. */
   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODEditRedrawArea()                                  *** PRIVATE FUNCTION ***
 *
 * Redraws the area of the screen used by the editor.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: void
 */
static void ODEditRedrawArea(tEditInstance *pEditInstance)
{
   UINT unAreaLine;

   ASSERT(pEditInstance != NULL);

   ODScrnEnableCaret(FALSE);

   /* First, remove anything that is still in the outbound communications */
   /* buffer, since whatever it was, it will no longer be visible after   */
   /* the screen redraw anyhow.                                           */
   if(od_control.baud != 0) ODComClearOutbound(hSerialPort);

   /* Loop, drawing every line in the edit area. */
   for(unAreaLine = 0; unAreaLine < pEditInstance->unAreaHeight; ++unAreaLine)
   {
      ODEditDrawAreaLine(pEditInstance, unAreaLine);
   }

   ODScrnEnableCaret(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODEditDrawAreaLine()                                *** PRIVATE FUNCTION ***
 *
 * Redraws the specified line in the area of the screen used by the editor.
 *
 * Parameters: pEditInstance    - Editor instance information structure.
 *
 *             unAreaLineToDraw - 0-based line number in the edit area to be
 *                                redrawn.
 *
 *     Return: void
 */
static void ODEditDrawAreaLine(tEditInstance *pEditInstance,
   UINT unAreaLineToDraw)
{
   UINT unBufferLine;
   UINT unLineLength;

   ASSERT(pEditInstance != NULL);
   ASSERT(unAreaLineToDraw >= 0);
   ASSERT(unAreaLineToDraw < pEditInstance->unAreaHeight);

   /* Determine the buffer line that is displayed on this screen line. */   
   unBufferLine = unAreaLineToDraw + pEditInstance->unLineScrolledToTop;

   /* Position the cursor to the beginning of this line. */
   od_set_cursor((UINT)pEditInstance->pUserOptions->nAreaTop
      + unAreaLineToDraw, (UINT)pEditInstance->pUserOptions->nAreaLeft);

   /* If this line is not beyond the end of the buffer. */
   if(unBufferLine < pEditInstance->unLinesInBuffer)
   {
      /* Determine the length of this buffer line. */
      unLineLength = ODEditBufferGetLineLength(pEditInstance, unBufferLine);

      od_disp(ODEditBufferGetCharacter(pEditInstance, unBufferLine, 0),
         unLineLength, TRUE);
   }
   else
   {
      unLineLength = 0;
   }

   /* If right edge of edit area aligns with the right edge of the screen. */
   if(pEditInstance->pUserOptions->nAreaRight == OD_SCREEN_WIDTH)
   {
      /* Clear the remainder of this line on the screen. */
      od_clr_line();
   }
   else
   {
      /* Place spaces after the end of the current line, up to right edge of */
      /* the edit area.                                                      */
      od_repeat(' ', (BYTE)(pEditInstance->unAreaWidth - unLineLength));
   }
}


/* ----------------------------------------------------------------------------
 * ODEditMainLoop()                                    *** PRIVATE FUNCTION ***
 *
 * Implements the main editor loop, which repeatedly waits for input from the
 * user, until the user chooses to exit.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: Value to be returned by od_multiline_edit.
 */
static INT ODEditMainLoop(tEditInstance *pEditInstance)
{
   tODInputEvent InputEvent;

   ASSERT(pEditInstance != NULL);

   /* Set initial cursor position. */
   ODEditUpdateCursorPos(pEditInstance);

   /* Loop, obtaining keystrokes until the user chooses to exit. */
   for(;;)
   {
      od_get_input(&InputEvent, OD_NO_TIMEOUT, GETIN_NORMAL);
      if(InputEvent.EventType == EVENT_EXTENDED_KEY)
      {
         switch(InputEvent.chKeyPress)
         {
            case OD_KEY_UP:
               /* If we aren't at the start of the file, then move to the */
               /* previous line.                                          */
               if(pEditInstance->unCurrentLine > 0)
               {
                  ODEditGotoPreviousLine(pEditInstance);
                  ODEditUpdateCursorPos(pEditInstance);
               }
               break;

            case OD_KEY_DOWN:
               /* If we aren't at the end of the file, then move to the */
               /* next line.                                            */
               if(pEditInstance->unCurrentLine
                  < ODEditBufferGetTotalLines(pEditInstance) - 1)
               {
                  ODEditGotoNextLine(pEditInstance);
                  ODEditUpdateCursorPos(pEditInstance);
               }
               break;

            case OD_KEY_LEFT:
               /* Attempt to move the cursor left. */
               if(ODEditCursorLeft(pEditInstance))
               {
                  /* If it was possible to move the cursor, then update the */
                  /* cursor position on the screen.                         */
                  ODEditUpdateCursorPos(pEditInstance);
               }
               break;

            case OD_KEY_RIGHT:
               /* In word wrap mode, we allow the cursor to move up to the */
               /* end of this line, and then wrap around to the next line. */
               if(pEditInstance->bWordWrapLongLines)
               {
                  if(pEditInstance->unCurrentColumn < ODEditBufferGetLineLength
                     (pEditInstance, pEditInstance->unCurrentLine))
                  {
                     ++pEditInstance->unCurrentColumn;
                     ODEditUpdateCursorPos(pEditInstance);
                  }
                  else if(pEditInstance->unCurrentLine
                     < ODEditBufferGetTotalLines(pEditInstance) - 1)
                  {
                     ODEditGotoNextLine(pEditInstance);
                     pEditInstance->unCurrentColumn = 0;
                     ODEditUpdateCursorPos(pEditInstance);
                  }
               }

               /* In character wrap mode, we allow the cursor to move up to */
               /* the right edge of the edit area.                          */
               else
               {
                  if(pEditInstance->unCurrentColumn
                     < pEditInstance->unAreaWidth - 1)
                  {
                     ++pEditInstance->unCurrentColumn;
                     ODEditUpdateCursorPos(pEditInstance);
                  }
               }
               break;

            case OD_KEY_HOME:
               pEditInstance->unCurrentColumn = 0;
               ODEditUpdateCursorPos(pEditInstance);
               break;

            case OD_KEY_END:
               pEditInstance->unCurrentColumn = ODEditBufferGetLineLength(
                  pEditInstance, pEditInstance->unCurrentLine);
               ODEditUpdateCursorPos(pEditInstance);
               break;

            case OD_KEY_PGUP:
               if(pEditInstance->unLineScrolledToTop > 0)
               {
                  UINT unDistance = MIN(pEditInstance->unAreaHeight - 1,
                     pEditInstance->unLineScrolledToTop);
                  ODEditScrollArea(pEditInstance, -((INT)unDistance));
                  pEditInstance->unCurrentLine -= unDistance;
                  ODEditUpdateCursorPos(pEditInstance);
               }
               else if(pEditInstance->unCurrentLine != 0)
               {
                  pEditInstance->unCurrentLine = 0;
                  ODEditUpdateCursorPos(pEditInstance);
               }
               break;

            case OD_KEY_PGDN:
               if(pEditInstance->unLineScrolledToTop <
                  pEditInstance->unLinesInBuffer - 1)
               {
                  UINT unDistance = MIN(pEditInstance->unAreaHeight - 1,
                     pEditInstance->unLinesInBuffer
                     - pEditInstance->unLineScrolledToTop - 1);
                  ODEditScrollArea(pEditInstance, (INT)unDistance);
                  pEditInstance->unCurrentLine = MIN(
                     pEditInstance->unCurrentLine + unDistance,
                     pEditInstance->unLinesInBuffer - 1);
                  ODEditUpdateCursorPos(pEditInstance);
               }
            break;

            case OD_KEY_INSERT:
               /* If the insert key is pressed, then toggle insert mode. */
               pEditInstance->bInsertMode = !pEditInstance->bInsertMode;
               break;

            case OD_KEY_DELETE:
               /* Delete the character at the current position. */
               
               /* If we are currently past the end of this line. */
               if(ODEditPastEndOfCurLine(pEditInstance))
               {
                  /* Add spaces to this line to fill it up to the current */
                  /* cursor position.                                     */
                  switch(ODEditBufferMakeSpace(pEditInstance,
                     pEditInstance->unCurrentLine,
                     pEditInstance->unCurrentColumn, 0))
                  {
                     case kODRCUnrecoverableFailure:
                        /* If we encountered an unrecoverable failure, then */
                        /* exit from the editor with a memory allocation    */
                        /* error.                                           */
                        od_control.od_error = ERR_MEMORY;
                        return(OD_MULTIEDIT_ERROR);

                     case kODRCSuccess:
                        /* On success, delete the current character. */
                        ODEditDeleteCurrentChar(pEditInstance);
                        break;

                     default:
                        /* On any other failure, just beep and continue. */
                        od_putch('\a');
                  }
               }
               else
               {
                  /* If we aren't pas the end of the line, then just do a */
                  /* simple delete character operation.                   */
                  ODEditDeleteCurrentChar(pEditInstance);
               }
               break;
         }
      }
      else if(InputEvent.EventType == EVENT_CHARACTER)
      {
         if(InputEvent.chKeyPress == 25)
         {
            /* Control-Y (delete line) has been pressed. */
            ODEditDeleteCurrentLine(pEditInstance);
         }
         else if(InputEvent.chKeyPress == 26
            || InputEvent.chKeyPress == 27)
         {
            /* Escape or control-Z has been pressed. */

            /* If a menu callback function has been provided by */
            /* the client, then call it.                        */
            if(pEditInstance->pUserOptions->pfMenuCallback != NULL)
            {
               /* Call the menu callback function. */
               switch((*pEditInstance->pUserOptions->pfMenuCallback)(NULL))
               {
                  case EDIT_MENU_EXIT_EDITOR:
                     return(OD_MULTIEDIT_SUCCESS);

                  case EDIT_MENU_DO_NOTHING:
                     /* Continue in the editor without doing anything. */
                     break;

                  default:
                     ASSERT(FALSE);
               }

               /* If we are continuing, then restore initial cursor pos. */
               ODEditUpdateCursorPos(pEditInstance);
            }
            else
            {
               /* If a menu key callback function has not been provided, */
               /* then we exit the editor unconditionally.               */
               return(OD_MULTIEDIT_SUCCESS);
            }
         }
         else if(InputEvent.chKeyPress == '\b')
         {
            /* Backspace key has been pressed. */

            /* If the cursor is past the end of the line, then we just move */
            /* the cursor left, without deleting any characters. */
            BOOL bDelete = !ODEditPastEndOfCurLine(pEditInstance);

            /* Backup the cursor one space. */
            if(ODEditCursorLeft(pEditInstance))
            {
               /* If there was space to move the cursor back to, then       */
               /* proceed and remove the character at the current position. */
               if(bDelete)
               {
                  ODEditDeleteCurrentChar(pEditInstance);
               }
               else
               {
                  /* In this case, we must still show the new cursor */
                  /* position.                                       */
                  ODEditUpdateCursorPos(pEditInstance);
               }
            }
         }
         else if(InputEvent.chKeyPress == '\t')
         {
            char szTextToAdd[MAX_TAB_STOP_SIZE + 1];
            UINT unTargetColumn;
            UINT unTargetDistance;

            /* A tab key has been entered. */

            /* Determine the column that this will move the cursor to. */
            ASSERT(pEditInstance->unTabStopSize <= MAX_TAB_STOP_SIZE);
            unTargetColumn = ((pEditInstance->unCurrentColumn / pEditInstance->
               unTabStopSize) + 1) * pEditInstance->unTabStopSize;

            /* In insert mode, then insert spaces into the buffer. */
            if(pEditInstance->bInsertMode)
            {
               /* Determine the number of columns that we need to advance in */
               /* order to reach this target column.                         */
               unTargetDistance = unTargetColumn -
                  pEditInstance->unCurrentColumn;
               ASSERT(unTargetDistance <= MAX_TAB_STOP_SIZE);

               /* Translate this to a string with the appropriate number of */
               /* spaces.                                                   */
               memset(szTextToAdd, ' ', unTargetDistance);
               szTextToAdd[unTargetDistance] = '\0';

               /* Add this to the buffer. */
               if(ODEditEnterText(pEditInstance, szTextToAdd, TRUE) ==
                  kODRCUnrecoverableFailure)
               {
                  od_control.od_error = ERR_MEMORY;
                  return(OD_MULTIEDIT_ERROR);
               }
            }

            /* In overwrite mode, then just advance the cursor position. */
            else
            {
               /* Determine the column where the cursor should be wrapped. */
               UINT unWrapColumn = pEditInstance->bWordWrapLongLines ?
                  ODEditBufferGetLineLength(pEditInstance,
                  pEditInstance->unCurrentLine)
                  : pEditInstance->unAreaWidth;

               if(unTargetColumn < unWrapColumn)
               {
                  pEditInstance->unCurrentColumn = unTargetColumn;
                  ODEditUpdateCursorPos(pEditInstance);
               }
               else if(pEditInstance->unCurrentLine
                  < ODEditBufferGetTotalLines(pEditInstance) - 1)
               {
                  ODEditGotoNextLine(pEditInstance);
                  pEditInstance->unCurrentColumn = 0;
                  ODEditUpdateCursorPos(pEditInstance);
               }
            }
         }
         else if(InputEvent.chKeyPress == '\r')
         {
            char *pszTextToAdd;

            /* The enter key has been pressed. In insert mode, or at the end */
            /* of the buffer in overwrite mode, this adds a new line.        */
            if(pEditInstance->bInsertMode || pEditInstance->unCurrentLine
                  >= ODEditBufferGetTotalLines(pEditInstance) - 1)
            {
               if(!pEditInstance->bInsertMode)
               {
                  /* If we are not in insert mode, begin by positioning the */
                  /* cursor at the end of this line.                        */
                  pEditInstance->unCurrentColumn = ODEditBufferGetLineLength(
                     pEditInstance, pEditInstance->unCurrentLine);
               }

               /* Determine the line/paragraph break sequence to use. */
               if(pEditInstance->pszLineBreak != NULL
                  && strlen(pEditInstance->pszLineBreak) > 0)
               {
                  pszTextToAdd = pEditInstance->pszLineBreak;
               }
               else if(pEditInstance->pszParagraphBreak != NULL
                  && strlen(pEditInstance->pszParagraphBreak) > 0)
               {
                  pszTextToAdd = pEditInstance->pszParagraphBreak;
               }
               else
               {
                  pszTextToAdd = DEFAULT_LINE_BREAK;
               }

               /* Insert the sequence into the buffer. */
               if(ODEditEnterText(pEditInstance, pszTextToAdd, TRUE) ==
                  kODRCUnrecoverableFailure)
               {
                  od_control.od_error = ERR_MEMORY;
                  return(OD_MULTIEDIT_ERROR);
               }
            }
            else
            {
               /* Pressing the enter key in overwrite mode just moves the   */
               /* cursor to the beginning of the next line. In other words, */
               /* it is equivalent to pressing Down arrow followed by home. */
               ODEditGotoNextLine(pEditInstance);
               pEditInstance->unCurrentColumn = 0;
               ODEditUpdateCursorPos(pEditInstance);
            }
         }
         else if(InputEvent.chKeyPress >= 32)
         {
            char szTextToAdd[2];
            szTextToAdd[0] = InputEvent.chKeyPress;
            szTextToAdd[1] = '\0';

            /* A valid buffer character has been entered. */
            if(ODEditEnterText(pEditInstance, szTextToAdd,
               (BOOL)(pEditInstance->bInsertMode
               || ODEditPastEndOfCurLine(pEditInstance)))
               == kODRCUnrecoverableFailure)
            {
               od_control.od_error = ERR_MEMORY;
               return(OD_MULTIEDIT_ERROR);
            }
         }
      }
   }
}


/* ----------------------------------------------------------------------------
 * ODEditGotoPreviousLine()                            *** PRIVATE FUNCTION ***
 *
 * Moves the current cursor position to the previous line, scrolling the screen
 * if necessary to keep the cursor visible.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: void
 */
static void ODEditGotoPreviousLine(tEditInstance *pEditInstance)
{
   ASSERT(pEditInstance != NULL);

   /* If we are already at the first line, then return without doing */
   /* anything.                                                      */
   if(pEditInstance->unCurrentLine == 0) return;

   /* If cursor is at top of edit area, then scroll area */
   /* first.                                             */
   if(ODEditGetCurrentLineInArea(pEditInstance) == 0)
   {
      ODEditScrollArea(pEditInstance,
         -(INT)(MIN(pEditInstance->unScrollDistance,
         pEditInstance->unCurrentLine)));
   }

   /* Move cursor to previous line. */
   --pEditInstance->unCurrentLine;
}


/* ----------------------------------------------------------------------------
 * ODEditGotoNextLine()                                *** PRIVATE FUNCTION ***
 *
 * Advances the current cursor position to the next line, scrolling the screen
 * if necessary to keep the cursor visible.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: void
 */
static void ODEditGotoNextLine(tEditInstance *pEditInstance)
{
   ASSERT(pEditInstance != NULL);

   /* If we are already at the end of the file, then return without */
   /* doing anything.                                               */
   if(pEditInstance->unCurrentLine
      >= ODEditBufferGetTotalLines(pEditInstance) - 1)
   {
      return;
   }

   /* If cursor is at the bottom of the edit area, then scroll area first. */
   if(ODEditGetCurrentLineInArea(pEditInstance)
      == pEditInstance->unAreaHeight - 1)
   {
      ODEditScrollArea(pEditInstance,
         (INT)MIN(pEditInstance->unScrollDistance,
         ODEditBufferGetTotalLines(pEditInstance)
         - pEditInstance->unCurrentLine));
   }

   /* Move cursor to next line. */
   ++pEditInstance->unCurrentLine;
}


/* ----------------------------------------------------------------------------
 * ODEditScrollArea()                                  *** PRIVATE FUNCTION ***
 *
 * Scrolls the edit area up or down the specified distance, redrawing newly
 * "exposed" lines.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *             nDistance     - Number of lines to scroll, where a positive
 *                             value moves the text upwards, and a negative
 *                             value moves the text down.
 *
 *     Return: FALSE if a full redraw has been performed, TRUE if an efficient
 *             scroll command has been used.
 */
static BOOL ODEditScrollArea(tEditInstance *pEditInstance, INT nDistance)
{
   BOOL bUseScrollCommand = FALSE;
   UINT unAreaLine;
   UINT unBufferLine;
   UINT unFirstAreaLineToDraw;
   UINT unLastAreaLineToDraw;
   UINT unPositiveDistance;

   ASSERT(pEditInstance);

   /* If scroll distance is zero, then we don't need to do anything at all. */
   if(nDistance == 0)
   {
      return(TRUE);
   }
   /* Otherwise, obtain the absolute value of the distance as an unsigned */
   /* integer.                                                            */
   else if(nDistance < 0)
   {
      unPositiveDistance = (UINT)-nDistance;
   }
   else
   {
      unPositiveDistance = (UINT)nDistance;
   }

   /* In AVATAR mode, if more than one of the currently visible lines will */
   /* still be visible after scrolling, then we will consider using the    */
   /* scroll operation.                                                    */
   if(od_control.user_avatar
      && ((INT)pEditInstance->unAreaHeight) - ((INT)unPositiveDistance) > 1)
   {
      /* Even under this situation, we only want to use the scroll operation */
      /* if the amount of data still in the outbound buffer + our estimate   */
      /* of the amount of data that will be sent to perform the scroll       */
      /* operation is less than our estimate of the amount of data that      */
      /* would be sent by a complete screen redraw.                          */
      UINT unEstimatedScrollData = ((pEditInstance->unAreaWidth + 4) *
         unPositiveDistance) + 7;

      if(!ODEditRecommendFullRedraw(pEditInstance, unEstimatedScrollData,
         TRUE))
      {
         bUseScrollCommand = TRUE;
      }
   }

   /* In local mode, we can also use the scroll command for efficiency. */
   if(od_control.baud == 0)
   {
      bUseScrollCommand = TRUE;
   }

   /* Area scroll is achieved by one of two means. We either use the scroll */
   /* command, and then draw just the newly visible lines, or we redraw the */
   /* entire edit area, after removing any data from the outbound           */
   /* communications buffer.                                                */

   if(bUseScrollCommand)
   {
      /* Use the od_scroll() function to scroll the screen contents. */
      od_scroll(pEditInstance->pUserOptions->nAreaLeft,
         pEditInstance->pUserOptions->nAreaTop,
         pEditInstance->pUserOptions->nAreaRight,
         pEditInstance->pUserOptions->nAreaBottom,
         nDistance, SCROLL_NO_CLEAR);

      /* Fill the newly visible lines. First, the portion of the area that   */
      /* requires redrawing is determined, and then a loop redraws the lines */
      /* that must be drawn.                                                 */

      /* If we are moving text upwards, exposing new lines at the bottom of */
      /* the area:                                                          */
      if(nDistance > 0)
      {
         ASSERT(pEditInstance->unLineScrolledToTop + unPositiveDistance
            < pEditInstance->unLinesInBuffer);
         pEditInstance->unLineScrolledToTop += unPositiveDistance;
         unFirstAreaLineToDraw = pEditInstance->unAreaHeight
            - (UINT)unPositiveDistance;
         unLastAreaLineToDraw = pEditInstance->unAreaHeight - 1;
      }
      /* Otherwise, we have moved text downwards, exposing new lines at the */
      /* top of the edit area.                                              */
      else
      {
         ASSERT(pEditInstance->unLineScrolledToTop >= unPositiveDistance);
         pEditInstance->unLineScrolledToTop -= unPositiveDistance;
         unFirstAreaLineToDraw = 0;
         unLastAreaLineToDraw = unPositiveDistance - 1;
      }

      ODScrnEnableCaret(FALSE);

      /* Now, redraw the new lines. */
      unBufferLine = unFirstAreaLineToDraw
         + pEditInstance->unLineScrolledToTop;
      for(unAreaLine = unFirstAreaLineToDraw; unAreaLine <=
         unLastAreaLineToDraw; ++unAreaLine, ++unBufferLine)
      {
         /* Draw the entire line. */
         ODEditDrawAreaLine(pEditInstance, unAreaLine);
      }

      ODScrnEnableCaret(TRUE);
   }

   /* Just redraw the entire edit area. */
   else
   {
      /* Adjust the line number that is scrolled to the top of the screen. */
      if(nDistance > 0)
      {
         pEditInstance->unLineScrolledToTop += unPositiveDistance;
      }
      else
      {
         pEditInstance->unLineScrolledToTop -= unPositiveDistance;
      }

      /* Perform redraw, first purging outbound buffer. */
      ODEditRedrawArea(pEditInstance);
   }

   return(bUseScrollCommand);
}


/* ----------------------------------------------------------------------------
 * ODEditRecommendFullRedraw()                         *** PRIVATE FUNCTION ***
 *
 * Determines whether it would be more efficient to add the specified number
 * of bytes to the outbound buffer as part of an incremental redraw, or if
 * it would be more efficient to just purge the outbound buffer and do a
 * complete redraw of the edit area.
 *
 * Parameters: pEditInstance           - Editor instance information structure.
 *
 *             unEstPartialRedrawBytes - Estimate of the number of bytes that
 *                                       would be transmitted if an incremental
 *                                       redraw is performed.
 *
 *             bDefault                - The default action (TRUE for full
 *                                       redraw, FALSE for incremental) if the
 *                                       number of bytes in the outbound buffer
 *                                       cannot be determined.
 *
 *     Return: TRUE if a full redraw is recommended, FALSE if an the
 *             incremental redraw is recommended.
 */
static BOOL ODEditRecommendFullRedraw(tEditInstance *pEditInstance,
   UINT unEstPartialRedrawBytes, BOOL bDefault)
{
   int nOutboundBufferBytes;
   UINT unEstFullRedrawBytes;

   /* In local mode, just return the default action. */
   if(od_control.baud == 0)
   {
      return(bDefault);
   }

   /* Attempt to obtain the number of bytes in the communications outbound */
   /* buffer. Unfortunately, this information may not be available. For    */
   /* example, FOSSIL drivers will only report whether or not there is     */
   /* still data in the outbound buffer, but not a count of the number of  */
   /* bytes in the buffer. Under such a situation, ODComOutbound() returns */
   /* SIZE_NON_ZERO if there is data in the buffer, and 0 if there is no   */
   /* data in the buffer. This is not a problem under OpenDoor's internal  */
   /* serial I/O code, nor is it a problem under Win32's communications    */
   /* facilities.                                                          */
   ODComOutbound(hSerialPort, &nOutboundBufferBytes);

   if(nOutboundBufferBytes == SIZE_NON_ZERO)
   {
      /* We know that there is data in the outbound buffer, but we don't */
      /* know how much, and so we cannot make a recommendation. Instead, */
      /* the default course of action will be taken.                     */
      return(bDefault);
   }

   /* Estimate the # of bytes required for a full redraw of the edit area. */
   unEstFullRedrawBytes = ODEditEstDrawBytes(pEditInstance, 0,
      0, pEditInstance->unAreaHeight - 1, pEditInstance->unAreaWidth);

   /* Recommend a full redraw if the number of bytes for an incremental */
   /* redraw plus the number of bytes already in the outbound buffer    */
   /* exceed the number of bytes required for a full redraw.            */
   if(unEstPartialRedrawBytes + (UINT)nOutboundBufferBytes
      > unEstFullRedrawBytes)
   {
      return(TRUE);
   }
   else
   {
      return(FALSE);
   }
}


/* ----------------------------------------------------------------------------
 * ODEditEstDrawBytes()                                *** PRIVATE FUNCTION ***
 *
 * Estimates the number of bytes which will be transmitted in order to redraw
 * the specified portion of the edit area.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *             unStartRedrawLine    - Line of first character to draw.
 *
 *             unStartRedrawColumn  - Column of first character to draw.
 *
 *             unFinishRedrawLine   - Line of last character to draw.
 *
 *             unFinishRedrawColumn - Column after last character to draw.
 *
 *     Return: A rough estimate of the number of bytes required for the redraw.
 */
static UINT ODEditEstDrawBytes(tEditInstance *pEditInstance,
   UINT unStartRedrawLine, UINT unStartRedrawColumn, UINT unFinishRedrawLine,
   UINT unFinishRedrawColumn)
{
   UINT unAreaLine;
   UINT unBufferLine;
   UINT unLineLength;
   UINT unByteTally = 0;

   /* If we are only drawing text on a single line, then estimate is just  */
   /* the distance between the start and finish redraw column. This number */
   /* is precise only if the cursor is already at the location where       */
   /* output is to begin, and the final cursor position is the location    */
   /* where output finishes. This is in fact the situation when the user   */
   /* is entering new text in the middle of the line - the most common     */
   /* situation that will be encountered.                                  */
   if(unStartRedrawLine == unFinishRedrawLine)
   {
      return(unFinishRedrawColumn - unStartRedrawColumn);
   }

   /* If we are drawing text on multiple lines, then inspect the contents */
   /* of those lines to estimate the number of bytes to be transmitted.   */
   for(unAreaLine = unStartRedrawLine,
      unBufferLine = pEditInstance->unLineScrolledToTop + unStartRedrawLine;
      unAreaLine <= unFinishRedrawLine;
      ++unAreaLine, ++unBufferLine)
   {
      /* Determine the length of this line. */
      if(unBufferLine < pEditInstance->unLinesInBuffer)
      {
         unLineLength = ODEditBufferGetLineLength(pEditInstance, unBufferLine);
         if(unAreaLine == unStartRedrawLine)
         {
            unLineLength -= unStartRedrawColumn;
         }
      }
      else
      {
         unLineLength = 0;
      }

      /* Add the number of characters on this line, along with the number of */
      /* bytes required to reposition the cursor and to clear the unused     */
      /* portion of this line to the tally. This assumes that the edit area  */
      /* spans the entire screen.                                            */
      unByteTally += unLineLength + 7;
   }

   return(unByteTally);
}


/* ----------------------------------------------------------------------------
 * ODEditGetCurrentLineInArea()                        *** PRIVATE FUNCTION ***
 *
 * Determines which line of the edit area the cursor is currently located in.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: 0-based index of the distance from the top of the edit area
 *             (as specified in the user options structure) where the
 *             cursor is currently located.
 */
static UINT ODEditGetCurrentLineInArea(tEditInstance *pEditInstance)
{
   ASSERT(pEditInstance != NULL);

   return(pEditInstance->unCurrentLine - pEditInstance->unLineScrolledToTop);
}


/* ----------------------------------------------------------------------------
 * ODEditUpdateCursorPos()                             *** PRIVATE FUNCTION ***
 *
 * Unconditionally updates the position of the cursor on the screen to 
 * reflect its actual position. Compare with ODEditUpdateCursorIfMoved().
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: void
 */
static void ODEditUpdateCursorPos(tEditInstance *pEditInstance)
{
   ASSERT(pEditInstance != NULL);

   /* Reposition the cursor on the screen. */
   od_set_cursor(ODEditGetCurrentLineInArea(pEditInstance)
      + pEditInstance->pUserOptions->nAreaTop,
      pEditInstance->unCurrentColumn + pEditInstance->pUserOptions->nAreaLeft);
}


/* ----------------------------------------------------------------------------
 * ODEditUpdateCursorIfMoved()                         *** PRIVATE FUNCTION ***
 *
 * Updates the position of the cursor on the screen to reflec its actual
 * position only if we belive it isn't already there. Compare with
 * ODEditUpdateCursorPos().
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: void
 */
static void ODEditUpdateCursorIfMoved(tEditInstance *pEditInstance)
{
   UINT unActualRow;
   UINT unActualColumn;
   ODEditGetActualCurPos(pEditInstance, &unActualRow, &unActualColumn);

   if(!(unActualRow == ODEditGetCurrentLineInArea(pEditInstance)
         + pEditInstance->pUserOptions->nAreaTop
      && unActualColumn == pEditInstance->unCurrentColumn
         + pEditInstance->pUserOptions->nAreaLeft))
   {
      ODEditUpdateCursorPos(pEditInstance);
   }
}


/* ----------------------------------------------------------------------------
 * ODEditEnterText()                                   *** PRIVATE FUNCTION ***
 *
 * Inserts new text at the current cursor position, updating the cursor
 * position accordingly.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *             pszEntered    - The character(s) to be inserted.
 *
 *     Return: kODRCSuccess on success, kODRCSafeFailure if we were unable to
 *             obtain enough buffer space before making any changes, or
 *             kODRCUnrecoverableFailure if the buffer has been changed, but
 *             there was insufficient memory to re-index the buffer.
 */
static tODResult ODEditEnterText(tEditInstance *pEditInstance,
   char *pszEntered, BOOL bInsertMode)
{
   UINT unNumCharsToAdd;
   char *pch;
   tODResult Result;

   ASSERT(pEditInstance != NULL);
   ASSERT(pszEntered != NULL);

   /* Remember initial edit area contents, to permit selective redraw. */
   ODEditRememberArea(pEditInstance, pEditInstance->pRememberBuffer);

   /* Determine the number of characters that are to be added to the buffer. */
   unNumCharsToAdd = strlen(pszEntered);

   /* Make room in the buffer for the new characters, if needed. */
   if(bInsertMode)
   {
      Result = ODEditBufferMakeSpace(pEditInstance,
         pEditInstance->unCurrentLine, pEditInstance->unCurrentColumn,
         unNumCharsToAdd);

      if(Result != kODRCSuccess)
      {
         /* Beep on failure. */
         od_putch('\a');
         return(Result);
      }
   }

   /* Copy the new characters to the buffer. */
   pch = ODEditBufferGetCharacter(pEditInstance,
      pEditInstance->unCurrentLine, pEditInstance->unCurrentColumn);
   memcpy(pch, pszEntered, unNumCharsToAdd);

   /* Move the cursor position to the end of the newly added text. The  */
   /* cursor position may be temporarily assigned to a position that is */
   /* past the end of the edit area or past the end of the buffer.      */
   for(pch = pszEntered; *pch != '\0'; ++pch)
   {
      if(IS_EOL_CHAR(*pch))
      {
         /* A carriage return character advances the cursor to the */
         /* leftmost column on the next line.                      */
         pEditInstance->unCurrentColumn = 0;
         pEditInstance->unCurrentLine++;

         /* If the next character is a different EOL character, and is */
         /* not the end of the string, then skip that character.       */
         if(IS_EOL_CHAR(pch[1]) && pch[1] != '\0' && pch[1] != *pch)
         {
            ++pch;
         }
      }
      else
      {
         /* All other characters move the cursor ahead one column. */
         pEditInstance->unCurrentColumn++;
      }
   }

   /* Reindex and reformat the buffer based on its new contents. */
   if(!ODEditBufferFormatAndIndex(pEditInstance))
   {
      return(kODRCUnrecoverableFailure);
   }

   /* Check whether the cursor is now positioned past the end of the edit */
   /* area, requiring the edit area to be scrolled up.                    */
   if(ODEditGetCurrentLineInArea(pEditInstance)
      >= pEditInstance->unAreaHeight)
   {
      /* We need to scroll the area accordingly. */

      /* Distance to scroll is maximum of the current single-step scroll */
      /* distance, and the distance that the cursor is positioned below  */
      /* the bottom of the current edit area.                            */
      UINT unScrollDistance = MAX(pEditInstance->unScrollDistance,
         ODEditGetCurrentLineInArea(pEditInstance) -
         pEditInstance->unAreaHeight + 1);

      /* Perform actual scroll operation. */
      if(ODEditScrollArea(pEditInstance, (INT)unScrollDistance))
      {
         /* Entire area wasn't redrawn by operation, so some redrawing */
         /* may still be required, within the area of text that was    */
         /* visible before the scroll operation.                       */
         ODEditRedrawChanged(pEditInstance, pEditInstance->pRememberBuffer,
            0, pEditInstance->unAreaHeight - unScrollDistance);
      }
   }

   /* Perform necessary redrawing and reposition the cursor if needed. */
   ODEditRedrawChanged(pEditInstance, pEditInstance->pRememberBuffer,
      REDRAW_NO_BOUNDARY, REDRAW_NO_BOUNDARY);

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODEditSetBreakSequence()                            *** PRIVATE FUNCTION ***
 *
 * If the default line or paragraph break sequence has not yet been set, then
 * this function sets it based on the break sequence that was encountered in
 * the buffer supplied by the client application.
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *             chFirstEOLChar  - First character in the encountered sequence.
 *
 *             chSecondEOLChar - Second character in the encountered sequence.
 *
 *     Return: void
 */
static void ODEditSetBreakSequence(tEditInstance *pEditInstance,
   char chFirstEOLChar, char chSecondEOLChar)
{
   char *pszSequence;

   ASSERT(pEditInstance != NULL);

   if(pEditInstance->pszParagraphBreak != NULL
      && pEditInstance->pszLineBreak != NULL)
   {
      /* In this situation, both the default line break sequence and default */
      /* paragraph break sequence have been set, so there is nothing for us  */
      /* to do.                                                              */
      return;
   }

   /* Obtain a pointer to the encountered sequence. We want to use a static */
   /* string constant for this, so that the string will continue to exist,  */
   /* in unchanged form.                                                    */
   if(chFirstEOLChar == '\r' && chSecondEOLChar == '\0')
   {
      pszSequence = "\r";
   }
   else if(chFirstEOLChar == '\n' && chSecondEOLChar == '\0')
   {
      pszSequence = "\n";
   }
   else if(chFirstEOLChar == '\n' && chSecondEOLChar == '\r')
   {
      pszSequence = "\n\r";
   }
   else if(chFirstEOLChar == '\r' && chSecondEOLChar == '\n')
   {
      pszSequence = "\r\n";
   }
   else
   {
      /* This should never happen: an invalid end of line sequence was */
      /* passed in.                                                    */
      pszSequence = NULL;
      ASSERT(FALSE);
   }

   /* Set the as yet undetermined line/paragraph terminators. */
   if(pEditInstance->pszParagraphBreak == NULL)
   {
      pEditInstance->pszParagraphBreak = pszSequence;
   }

   if(pEditInstance->pszLineBreak == NULL)
   {
      pEditInstance->pszLineBreak = pszSequence;
   }
}


/* ----------------------------------------------------------------------------
 * ODEditCursorLeft()                                  *** PRIVATE FUNCTION ***
 *
 * Attempts to move the cursor left. Called when the user presses the left
 * arrow key, and is also called as part of the process of handling the
 * backspace key.
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *     Return: TRUE on success, or FALSE if the cursor cannot be moved left any
 *             further.
 */
static BOOL ODEditCursorLeft(tEditInstance *pEditInstance)
{
   ASSERT(pEditInstance != NULL);

   /* In word wrap mode, pressing the left key when the cursor */
   /* is past the end of the line jumps the cursor to the end  */
   /* of the line.                                             */
   if(pEditInstance->bWordWrapLongLines &&
      pEditInstance->unCurrentColumn > ODEditBufferGetLineLength
      (pEditInstance, pEditInstance->unCurrentLine))
   {
      pEditInstance->unCurrentColumn = ODEditBufferGetLineLength
         (pEditInstance, pEditInstance->unCurrentLine);
      return(TRUE);
   }

   /* If we are not already at the leftmost column. */
   else if(pEditInstance->unCurrentColumn > 0)
   {
      /* Move left one column. */
      --pEditInstance->unCurrentColumn;
      return(TRUE);
   }
   else if(pEditInstance->bWordWrapLongLines)
   {
      /* In word wrap mode, this will move us up to the end of */
      /* the previous line.                                    */
      if(pEditInstance->unCurrentLine > 0)
      {
         ODEditGotoPreviousLine(pEditInstance);
         pEditInstance->unCurrentColumn = ODEditBufferGetLineLength(
            pEditInstance, pEditInstance->unCurrentLine);
         return(TRUE);
      }
   }

   /* It wasn't possible to move the cursor. */
   return(FALSE);
}


/* ----------------------------------------------------------------------------
 * ODEditDeleteCurrentChar()                           *** PRIVATE FUNCTION ***
 *
 * Deletes the character at the current cursor position, performing necessary
 * redraw. Pressing the delete key causes just this function to be called.
 * Pressing the backspace key causes ODEditCursorLeft() to be called to first
 * move the cursor left before calling ODEditDeleteCurrentChar().
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *     Return: void
 */
static void ODEditDeleteCurrentChar(tEditInstance *pEditInstance)
{
   char *pch;

   ASSERT(pEditInstance != NULL);

   /* Remember initial edit area contents, to permit selective redraw. */
   ODEditRememberArea(pEditInstance, pEditInstance->pRememberBuffer);

   /* Backup the entire buffer contents by one character. */
   pch = ODEditBufferGetCharacter(pEditInstance,
      pEditInstance->unCurrentLine, pEditInstance->unCurrentColumn);
   memmove(pch, pch + 1, strlen(pch + 1) + 1);

   /* Reindex and reformat the buffer based on its new contents. */
   ODEditBufferFormatAndIndex(pEditInstance);

   /* Perform necessary redrawing and reposition the cursor if needed. */
   ODEditRedrawChanged(pEditInstance, pEditInstance->pRememberBuffer,
      REDRAW_NO_BOUNDARY, REDRAW_NO_BOUNDARY);
}


/* ----------------------------------------------------------------------------
 * ODEditDeleteCurrentLine()                           *** PRIVATE FUNCTION ***
 *
 * Removes the entire current line from the buffer.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: void
 */
static void ODEditDeleteCurrentLine(tEditInstance *pEditInstance)
{
   char *pszStartOfThisLine;
   char *pszStartOfNextLine;

   ASSERT(pEditInstance != NULL);

   /* Remember initial edit area contents, to permit selective redraw. */
   ODEditRememberArea(pEditInstance, pEditInstance->pRememberBuffer);

   /* Determine start of this line. */
   pszStartOfThisLine = ODEditBufferGetCharacter(pEditInstance,
      pEditInstance->unCurrentLine, 0);

   if(pEditInstance->unLinesInBuffer == pEditInstance->unCurrentLine + 1)
   {
      /* If this is the last line of the buffer, then we just remove    */
      /* everything from this line, without actually removing the line. */
      *pszStartOfThisLine = '\0';
   }
   else
   {
      /* If this is not the last line of the buffer, then remove this */
      /* entire line, so that the next line will become the current   */
      /* line.                                                        */
      pszStartOfNextLine = ODEditBufferGetCharacter(pEditInstance,
         pEditInstance->unCurrentLine + 1, 0);
      memmove(pszStartOfThisLine, pszStartOfNextLine,
         strlen(pszStartOfNextLine) + 1);
   }

   /* Reset the cursor position to the beginning of the current line. */
   pEditInstance->unCurrentColumn = 0;

   /* Reindex and reformat the buffer based on its new contents. */
   ODEditBufferFormatAndIndex(pEditInstance);

   /* Perform necessary redrawing and reposition the cursor if needed. */
   ODEditRedrawChanged(pEditInstance, pEditInstance->pRememberBuffer,
      REDRAW_NO_BOUNDARY, REDRAW_NO_BOUNDARY);
}


/* ----------------------------------------------------------------------------
 * ODEditPastEndOfCurLine()                            *** PRIVATE FUNCTION ***
 *
 * Determines whether the cursor is currently past the end of the current line.
 * The end of the line is considered to be the first column after the last
 * character on the line. So, on a blank line, a cursor is considered to be
 * past the end if it is in or past the second column (column 0).
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *     Return: TRUE if the cursor is past the end of the current line,
 *             FALSE if it is not.
 */
static BOOL ODEditPastEndOfCurLine(tEditInstance *pEditInstance)
{
   ASSERT(pEditInstance != NULL);

   return(pEditInstance->unCurrentColumn >
      ODEditBufferGetLineLength(pEditInstance, pEditInstance->unCurrentLine));
}


/* ----------------------------------------------------------------------------
 * ODEditRememberBufferSize()                          *** PRIVATE FUNCTION ***
 *
 * Determines the buffer size required by ODEditRememberArea().
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *     Return: the required buffer size, in size_t units (bytes).
 */
static size_t ODEditRememberBufferSize(tEditInstance *pEditInstance)
{
   ASSERT(pEditInstance != NULL);

   return((pEditInstance->unAreaWidth + 1)
         * pEditInstance->unAreaHeight);
}


/* ----------------------------------------------------------------------------
 * ODEditRememberArea()                                *** PRIVATE FUNCTION ***
 *
 * Stores a copy of the text currently displayed in the edit area in the
 * provided buffer, so that it can later be reused to redraw only the portion
 * of the edit area which has been changed by an operation.
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *             pRememberedArea - Pointer to a buffer, which is at least
 *                               the number of bytes specified by
 *                               ODEditRememberBufferSize() in size.
 *
 *     Return: void
 */
static void ODEditRememberArea(tEditInstance *pEditInstance,
   void *pRememberedArea)
{
   UINT unDataLineOffset = 0;
   UINT unDataLineSize;
   UINT unAreaLine;
   UINT unBufferLine;
   UINT unLineLength;
   char *pchStartOfLine;
   char *pchDataLocation;

   ASSERT(pEditInstance != NULL);
   ASSERT(pRememberedArea != NULL);

   /* Determine the length of a single line in the remember buffer. */
   unDataLineSize = pEditInstance->unAreaWidth + 1;

   pchDataLocation = (char *)pRememberedArea + unDataLineOffset;
   for(unBufferLine = pEditInstance->unLineScrolledToTop, unAreaLine = 0;
      unAreaLine < pEditInstance->unAreaHeight;
      ++unAreaLine, ++unBufferLine)
   {
      /* If this line is not beyond the end of the buffer. */
      if(unBufferLine < pEditInstance->unLinesInBuffer)
      {
         /* Determine the length of this buffer line. */
         unLineLength = ODEditBufferGetLineLength(pEditInstance, unBufferLine);

         /* Determine the start location of this buffer line. */
         pchStartOfLine = ODEditBufferGetCharacter(pEditInstance, unBufferLine,
            0);
      }
      else
      {
         /* If this line is beyond the end of the buffer, then it is empty. */
         unLineLength = 0;
         pchStartOfLine = "";
      }

      /* Copy the contents of this line to the data buffer. */
      memcpy(pchDataLocation, pchStartOfLine, unLineLength);
      pchDataLocation[unLineLength] = '\0';

      /* Update the location where data is being stored in the buffer. */
      pchDataLocation += unDataLineSize;
   }
}


/* ----------------------------------------------------------------------------
 * ODEditRedrawChanged()                               *** PRIVATE FUNCTION ***
 *
 * Redraws the portion of the edit area which has been changed by an operation,
 * based on the original edit area contents as stored in a buffer by the
 * ODEditRememberArea() function.
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *             pRememberedArea - Pointer to a buffer that was filled by a
 *                               previous call to ODEditRememberArea().
 *
 *             unUpperBoundary - The first line in the edit area to consider
 *                               redrawing, or REDRAW_NO_BOUNDARY to specify
 *                               the top of the edit area.
 *
 *             unLowerBoundary - The last line in the edit area to consider
 *                               redrawing, or REDRAW_NO_BOUNDARY to specify
 *                               the bottom of the edit area.
 *
 *     Return: void
 */
static void ODEditRedrawChanged(tEditInstance *pEditInstance,
   void *pRememberedArea, UINT unUpperBoundary, UINT unLowerBoundary)
{
   UINT unStartRedrawLine;
   UINT unStartRedrawColumn;
   UINT unFinishRedrawLine;
   UINT unFinishRedrawColumn;
   UINT unEstPartialRedrawBytes;

   ASSERT(pEditInstance != NULL);
   ASSERT(pRememberedArea != NULL);

   /* Determine what portion of the edit area, within the specified upper */
   /* and lower boundaries, has been changed.                             */
   if(!ODEditDetermineChanged(pEditInstance, pRememberedArea, unUpperBoundary,
      unLowerBoundary, &unStartRedrawLine, &unStartRedrawColumn,
      &unFinishRedrawLine, &unFinishRedrawColumn))
   {
      /* Nothing has changed in the edit area. */
      ODEditUpdateCursorIfMoved(pEditInstance);
      return;
   }

   /* Now that we have determined the portion of the edit area that would */
   /* be redraw by a partial (incremental) redraw, we compare the amount  */
   /* of data involved + the amount of data still in the outbound buffer  */
   /* with the amount of data involved in a full redraw. If the amount of */
   /* data in the outbound buffer cannot be determined, we default to     */
   /* using an incremental redraw.                                        */
   unEstPartialRedrawBytes = ODEditEstDrawBytes(pEditInstance,
      unStartRedrawLine, unStartRedrawColumn, unFinishRedrawLine,
      unFinishRedrawColumn);
   if(ODEditRecommendFullRedraw(pEditInstance, unEstPartialRedrawBytes,
      FALSE))
   {
      /* Purge the outbound buffer and do a full redraw. */
      ODEditRedrawArea(pEditInstance);

      /* Move the cursor back to its appropriate position. */
      ODEditUpdateCursorPos(pEditInstance);
   }
   else
   {
      /* Perform a partial (incremental) redraw. */

      /* Now, redraw the portion of the edit area that has been determined to */
      /* require redrawing.                                                   */
      ODEditRedrawSubArea(pEditInstance, unStartRedrawLine, unStartRedrawColumn,
         unFinishRedrawLine, unFinishRedrawColumn);

      /* Now, move the cursor back to its appropriate position, if it isn't */
      /* already there.                                                     */
      ODEditUpdateCursorIfMoved(pEditInstance);
   }
}


/* ----------------------------------------------------------------------------
 * ODEditDetermineChanged()                            *** PRIVATE FUNCTION ***
 *
 * Determines what portion of the edit area, within the specifiede upper and
 * lower boundary, has been changed. Area is specified as row and column
 * position of the first changed characer between boundaries, and the row
 * and column position past the last changed character between the boundaries.
 *
 * Parameters: pEditInstance         - Editor instance information structure.
 *
 *             pRememberedArea       - Pointer to a buffer that was filled by a
 *                                     previous call to ODEditRememberArea().
 *
 *             unUpperBoundary       - The first line in the edit area to
 *                                     consider redrawing, or
 *                                     REDRAW_NO_BOUNDARY to specify the top of 
 *                                     the edit area.
 *
 *             unLowerBoundary       - The last line in the edit area to
 *                                     consider redrawing, or
 *                                     REDRAW_NO_BOUNDARY to specify the bottom
 *                                     of the edit area.
 *
 *             punStartRedrawLine    - Output: Line of first changed character.
 *
 *             punStartRedrawColumn  - Output: Column of first changed char.
 *
 *             punFinishRedrawLine   - Output: Line of last changed character.
 *
 *             punFinishRedrawColumn - Output: Column after last changed char.
 *
 *     Return: TRUE if some text in the edit area has been changed, FALSE
 *             if there is no change.
 */
static BOOL ODEditDetermineChanged(tEditInstance *pEditInstance,
   void *pRememberedArea, UINT unUpperBoundary, UINT unLowerBoundary, 
   UINT *punStartRedrawLine, UINT *punStartRedrawColumn,
   UINT *punFinishRedrawLine, UINT *punFinishRedrawColumn)
{
   BOOL bFoundStart = FALSE;
   BOOL bFoundFinish = FALSE;
   UINT unDataLineOffset = 0;
   UINT unDataLineSize;
   UINT unAreaLine;
   UINT unLineLength;
   UINT unColumn;
   UINT unBufferLine;
   char *pchCurrent;
   char *pchRemembered;

   /* Determine the length of a single line in the remember buffer. */
   unDataLineSize = pEditInstance->unAreaWidth + 1;

   /* If caller specified no upper boundary, then reset upper boundary */
   /* to 0.                                                            */
   if(unUpperBoundary == REDRAW_NO_BOUNDARY) unUpperBoundary = 0;

   /* Likewise, iff caller specified no lower boundary, then reset the */
   /* lower boundary to the bottom of the edit area.                   */
   if(unLowerBoundary == REDRAW_NO_BOUNDARY)
   {
      unLowerBoundary = pEditInstance->unAreaHeight;
   }

   /* Loop through the area within boundaries, determining which */
   /* portion of the edit area has changed.                      */
   for(unBufferLine = pEditInstance->unLineScrolledToTop + unUpperBoundary,
      unAreaLine = unUpperBoundary; unAreaLine < unLowerBoundary;
      ++unAreaLine, ++unBufferLine)
   {
      /* Determine location of corresponding line in remembered data. */
      pchRemembered = (char *)pRememberedArea + unDataLineOffset
         + unDataLineSize * unAreaLine;

      /* If this line is not beyond the end of the buffer. */
      if(unBufferLine < pEditInstance->unLinesInBuffer)
      {
         /* Determine the start location of this buffer line. */
         pchCurrent = ODEditBufferGetCharacter(pEditInstance, unBufferLine, 0);

         /* Determine the length of this buffer line. */
         unLineLength = ODEditBufferGetLineLength(pEditInstance, unBufferLine);
      }
      else
      {
         pchCurrent = "";
         unLineLength = 0;
      }

      /* Start at the first column on this line. */
      unColumn = 0;

      /* Look for any characters that differ. */
      for(;; ++unColumn, ++pchCurrent, ++pchRemembered)
      {
         /* Determine if we are at the end of the remembered line. */
         BOOL bEndOfRemembered = (*pchRemembered == '\0');

         /* Determine if we are at the end of the current buffer line. */
         BOOL bEndOfCurrent = (unColumn == unLineLength);

         /* If we are at the end of either of the buffers (but not both), */
         /* or if these two characters differ, then we have found the     */
         /* start of the area that must be redrawn.                       */
         if(!(bEndOfRemembered && bEndOfCurrent))
         {
            if(bEndOfRemembered || bEndOfCurrent
               || *pchCurrent != *pchRemembered)
            {
               if(bFoundStart)
               {
                  bFoundFinish = FALSE;
               }
               else
               {
                  /* We have found a character that differs. */
                  bFoundStart = TRUE;
                  *punStartRedrawLine = unAreaLine;
                  *punStartRedrawColumn = unColumn;
               }
            }
         }

         /* If we have found the first changed text in the buffer, then we */
         /* are now looking for the last changed text in the buffer.       */
         if(bFoundStart && !bFoundFinish)
         {
            if(*pchCurrent == *pchRemembered)
            {
               bFoundFinish = TRUE;
               *punFinishRedrawLine = unAreaLine;
               *punFinishRedrawColumn = unColumn;
            }
            else if(bEndOfRemembered)
            {
               bFoundFinish = TRUE;
               *punFinishRedrawLine = unAreaLine;
               *punFinishRedrawColumn = unLineLength;
            }
            else if(bEndOfCurrent)
            {
               bFoundFinish = TRUE;
               *punFinishRedrawLine = unAreaLine;
               *punFinishRedrawColumn = unColumn + strlen(pchRemembered);
            }
         }

         /* If we are at the end of either buffers. */
         if(bEndOfRemembered || bEndOfCurrent)
         {
            /* Now, proceed to processing the next line in the edit area. */
            break;
         }
      }
   }

   /* If we haven't found any text in the edit area that has changed. */
   if(!bFoundStart)
   {
      /* Then return indicating no change. */
      return(FALSE);
   }

   /* If we haven't found an end to the portion of the area that has */
   /* changed, then we must redraw up to the end of the edit area.   */
   if(!bFoundFinish)
   {
      *punFinishRedrawLine = unLowerBoundary;
      *punFinishRedrawColumn = unColumn;
   }

   /* Return indicating that ther has been some change. */
   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODEditRedrawSubArea()                               *** PRIVATE FUNCTION ***
 *
 * Redraws the portion of the edit area within the specified range. Redrawing
 * is performed from the location of the start redraw row and column, up to
 * but not includin gthe finish redraw row and column.
 *
 * Parameters: pEditInstance        - Editor instance information structure.
 *
 *             unStartRedrawLine    - Line of first character to draw.
 *
 *             unStartRedrawColumn  - Column of first character to draw.
 *
 *             unFinishRedrawLine   - Line of last character to draw.
 *
 *             unFinishRedrawColumn - Column after last character to draw.
 *
 *     Return: void
 */
static void ODEditRedrawSubArea(tEditInstance *pEditInstance,
   UINT unStartRedrawLine, UINT unStartRedrawColumn, UINT unFinishRedrawLine,
   UINT unFinishRedrawColumn)
{
   UINT unAreaLine;
   UINT unLineLength;
   UINT unBufferLine;
   char *pchCurrent;
   UINT unStartColumn;
   UINT unFinishColumn;
   UINT unScrnStartColumn;
   UINT unTextLength;

   /* Now, perform actual redraw in area that requires redrawing. */
   for(unBufferLine = pEditInstance->unLineScrolledToTop + unStartRedrawLine,
      unAreaLine = unStartRedrawLine; unAreaLine <= unFinishRedrawLine;
      ++unBufferLine, ++unAreaLine)
   {
      BOOL bFirstLine = (unAreaLine == unStartRedrawLine);
      BOOL bLastLine = (unAreaLine == unFinishRedrawLine);
      UINT unScrnRow = (UINT)pEditInstance->pUserOptions->nAreaTop
         + unAreaLine;

      /* If this line is not beyond the end of the buffer. */
      if(unBufferLine < pEditInstance->unLinesInBuffer)
      {
         pchCurrent = ODEditBufferGetCharacter(pEditInstance, unBufferLine, 0);
         unTextLength = unLineLength =
            ODEditBufferGetLineLength(pEditInstance, unBufferLine);
      }
      else
      {
         pchCurrent = "";
         unTextLength = unLineLength = 0;
      }

      /* Move to the position on the first line to begin redraw. */
      if(bFirstLine)
      {
         UINT unActualRow;
         UINT unActualColumn;
         ODEditGetActualCurPos(pEditInstance, &unActualRow, &unActualColumn);

         unStartColumn = unStartRedrawColumn;
         unScrnStartColumn = (UINT)pEditInstance->pUserOptions->nAreaLeft
            + unStartColumn;

         if(unScrnRow != unActualRow || unScrnStartColumn != unActualColumn)
         {
            od_set_cursor(unScrnRow, unScrnStartColumn);
         }

         pchCurrent += unStartRedrawColumn;
         unTextLength -= unStartRedrawColumn;
      }
      else
      {
         unStartColumn = 0;
         unScrnStartColumn = (UINT)pEditInstance->pUserOptions->nAreaLeft;
         od_set_cursor(unScrnRow, unScrnStartColumn);
      }

      /* If this is the last line to redraw, then adjust accordingly. */
      if(bLastLine)
      {
         if(unFinishRedrawColumn < unLineLength)
         {
            unTextLength -= unLineLength - unFinishRedrawColumn;
         }
         unFinishColumn = unFinishRedrawColumn;
      }
      else
      {
         unFinishColumn = pEditInstance->unAreaWidth;
      }

      /* Output changed text. */
      if(unStartColumn < unLineLength)
      {
         od_disp(pchCurrent, unTextLength, TRUE);
         unStartColumn += unTextLength;
      }

      /* If we need to clear the rest of the line. */
      if(unFinishColumn == pEditInstance->unAreaWidth)
      {
         /* If right edge of edit area aligns with the right edge of the */
         /* screen.                                                      */
         if(pEditInstance->pUserOptions->nAreaRight == OD_SCREEN_WIDTH)
         {
            /* Clear the remainder of this line on the screen. */
            od_clr_line();
         }
         else
         {
            /* Place spaces after the end of the current line, up to right */
            /* edge of the edit area.                                      */
            od_repeat(' ', (BYTE)(pEditInstance->unAreaWidth - unLineLength));
         }
      }
      else if(unStartColumn < unFinishColumn)
      {
         od_repeat(' ', (BYTE)(unFinishColumn - unStartColumn));
      }
   }
}


/* ----------------------------------------------------------------------------
 * ODEditGetActualCurPos()                             *** PRIVATE FUNCTION ***
 *
 * Estimates the actual position of the cursor on the screen.
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *             punRow          - Pointer to location where cursor row number
 *                               should be stored.
 *
 *             punColumn       - Pointer to location where cursor column number
 *                               should be stored.
 *
 *     Return: void
 */
static void ODEditGetActualCurPos(tEditInstance *pEditInstance,
   UINT *punRow, UINT *punColumn)
{
   tODScrnTextInfo TextInfo;

   ASSERT(pEditInstance != NULL);
   ASSERT(punRow != NULL);
   ASSERT(punColumn != NULL);

   UNUSED(pEditInstance);

   /* Obtain current cursor position information from the ODScrn module. */
   ODScrnGetTextInfo(&TextInfo);
   *punRow = (UINT)TextInfo.cury;
   *punColumn = (UINT)TextInfo.curx;
}


/* ----------------------------------------------------------------------------
 * ODEditIsEOLForMode()                                *** PRIVATE FUNCTION ***
 *
 * Determines whether the specified character should be treated as an EOL
 * character for the current mode.
 *
 * Parameters: pEditInstance   - Editor instance information structure.
 *
 *     Return: TRUE if this is an EOL character, FALSE otherwise.
 */
static BOOL ODEditIsEOLForMode(tEditInstance *pEditInstance, char chToTest)
{
   switch(pEditInstance->pUserOptions->TextFormat)
   {
      case FORMAT_FTSC_MESSAGE:
         return(chToTest == '\r' || chToTest == '\0');

      default:
         return(IS_EOL_CHAR(chToTest));
   }
}



/* ========================================================================= */
/* Low level buffer manipulation functions.                                  */
/* ========================================================================= */

/* ----------------------------------------------------------------------------
 * ODEditBufferFormatAndIndex()                        *** PRIVATE FUNCTION ***
 *
 * Regenerates the count of lines in the buffer, and the array of pointers to
 * the start of each line.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: TRUE on success, or FALSE if there is not enough memory
 *             available to complete this operation.
 */
static BOOL ODEditBufferFormatAndIndex(tEditInstance *pEditInstance)
{
   char *pch;
   char *pchLastSpace;
   UINT unProcessingColumn;
   UINT unProcessingLine;
   BOOL bAtEndOfBuffer = FALSE;
   BOOL bLineEndedByBreak;
   BOOL bFTSCMode =
      (pEditInstance->pUserOptions->TextFormat == FORMAT_FTSC_MESSAGE);

   ASSERT(pEditInstance != NULL);

   /* Reset current line count. */
   unProcessingLine = 0;

   /* Begin at the beginning of the buffer to edit. */
   pch = pEditInstance->pszEditBuffer;
   ASSERT(pch != NULL);

   /* Loop for each line in the buffer. */
   while(!bAtEndOfBuffer)   
   {
      /* In FTSC mode, skip a line if it begins with a ^A ("kludge lines"). */
      if(bFTSCMode)
      {
         /* Loop while the current line begins with a ^A. */
         while(*pch == 0x01)
         {
            /* Loop until the end of the line is found. */
            while(!ODEditIsEOLForMode(pEditInstance, *pch)) ++pch;

            if(*pch == '\0')
            {
               /* If the line was ended by a null character, then note that */
               /* the end of the buffer has been reached.                   */
               bAtEndOfBuffer = TRUE;
            }
            else
            {
               /* If the line was not ended by a null character, then skip */
               /* the end of line character.                               */
               ++pch;
            }
         }

         continue;
      }

      /* Add the address of the start of this line to the line array. */
      
      /* If the line array is full, then attempt to grow it. */
      ASSERT(unProcessingLine <= pEditInstance->unLineArraySize);
      if(unProcessingLine == pEditInstance->unLineArraySize)
      {
         /* Determine the size to grow the array to. */
         UINT unNewArraySize = pEditInstance->unLineArraySize
            + LINE_ARRAY_GROW_SIZE;

         /* Attempt to reallocate this memory block. */
         char **papchNewLineArray = (char **)realloc(
            pEditInstance->papchStartOfLine, unNewArraySize * sizeof(char *));

         /* If reallocation failed, then return with failure. */
         if(papchNewLineArray == NULL)
         {
            return(FALSE);
         }

         /* Otherwise, update the editor instance information with the new */
         /* array address and array size information.                      */
         pEditInstance->papchStartOfLine = papchNewLineArray;
         pEditInstance->unLineArraySize = unNewArraySize;
      }

      /* Add the address of the start of this line to the array. */
      pEditInstance->papchStartOfLine[unProcessingLine] = pch;

      /* Reset word wrap information. */
      pchLastSpace = NULL;

      /* Now, find the end of this line. */
      bLineEndedByBreak = TRUE;
      unProcessingColumn = 0;
      while(!ODEditIsEOLForMode(pEditInstance, *pch))
      {
         /* If this character is a space, then record the location of the */
         /* last space characters.                                        */
         if(*pch == ' ') pchLastSpace = pch;

         /* Check for characters which must be filtered from the buffer */
         /* in FTSC message mode.                                       */
         if(bFTSCMode)
         {
            if(*pch == 0x0a || ((unsigned char)*pch) == 0x8d)
            {
               /* If this character must be removed, then move rest of */
               /* buffer up by one character, and proceed to next loop */
               /* iteration.                                           */
               memmove(pch, pch + 1, strlen(pch + 1) + 1);
               continue;
            }
         }

         /* Increment count of characters on this line. */
         ++unProcessingColumn;

         /* Check whether we have reached the maximum number of characters */
         /* that will fit on this line.                                    */
         if(unProcessingColumn >= pEditInstance->unAreaWidth - 1)
         {
            if(pEditInstance->bWordWrapLongLines)
            {
               /* If we are to word wrap long lines, then back up to the */
               /* beginning of the last word, if we have encountered any */
               /* space characters.                                      */
               if(pchLastSpace != NULL && pchLastSpace < pch)
               {
                  /* Update current column number accordingly. */
                  unProcessingColumn -= (UINT)(pch - pchLastSpace);

                  /* Back up to position to perform word wrap at. */
                  pch = pchLastSpace;
               }
            }

            /* If we are wrapping text where the cursor is located, then we */
            /* will have to reposition the cursor accordingly.              */
            if(unProcessingLine == pEditInstance->unCurrentLine
               && unProcessingColumn < pEditInstance->unCurrentColumn)
            {
               /* Move the cursor to the next line. */
               pEditInstance->unCurrentLine++;

               /* Adjust the cursor column number to the position where the */
               /* corresponding wrapped text will appear.                   */
               pEditInstance->unCurrentColumn -= unProcessingColumn;
            }

            /* Note that line was not ended by en explicit line break. */
            bLineEndedByBreak = FALSE;

            break;
         }

         /* Move to the next character in the buffer. */
         ++pch;
      }

      /* If we the line was terminated by a '\0', then note that the end of */
      /* the buffer has been reached.                                       */
      if(*pch == '\0')
      {
         bAtEndOfBuffer = TRUE;
      }

      /* If the line was not terminated by a '\0', then find the first */
      /* character of the next line.                                   */
      else
      {
         char chFirstEOLChar = *pch;
         char chSecondEOLChar = '\0';

         /* Move to the next character in the buffer. */
         ++pch;
         
         /* If this character is a different EOL sequence character from the */
         /* already encountered EOL character, then skip past it too.        */
         if(ODEditIsEOLForMode(pEditInstance, chFirstEOLChar) && *pch != '\0'
            && ODEditIsEOLForMode(pEditInstance, *pch)
            && *pch != chFirstEOLChar)
         {
            chSecondEOLChar = *pch;
            ++pch;
         }
         
         /* If we don't already know what form of line/paragraph break to */
         /* use (just a CR, just a LF, a CR/LF sequence or a LF/CR        */
         /* sequence), then use this line termination as an example of    */
         /* what should be used.                                          */
         if(bLineEndedByBreak)
         {
            ODEditSetBreakSequence(pEditInstance, chFirstEOLChar,
               chSecondEOLChar);
         }
      }

      /* Increment the count of the current line number. */
      unProcessingLine++;
   }

   /* Update count of number of lines in the buffer, based on the number */
   /* that we have found.                                                */
   pEditInstance->unLinesInBuffer = unProcessingLine;

   /* Return with success. */
   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODEditBufferGetLineLength()                         *** PRIVATE FUNCTION ***
 *
 * Determines the length of the specified line in the buffer, not including
 * any line terminator characters.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *             unBufferLine  - 0-based index of the line in question.
 *
 *     Return: The number of characters on this line.
 */
static UINT ODEditBufferGetLineLength(tEditInstance *pEditInstance,
   UINT unBufferLine)
{
   char *pch;
   char *pchStartOfLine;
   UINT unCharsBeforeEOL;

   ASSERT(pEditInstance != NULL);
   ASSERT(unBufferLine < pEditInstance->unLinesInBuffer);
   ASSERT(pEditInstance->unLinesInBuffer <= pEditInstance->unLineArraySize);

   /* Get the address of the start of this line in the buffer. */
   pchStartOfLine
      = ODEditBufferGetCharacter(pEditInstance, unBufferLine, 0);

   /* Count the number of characters before the next end of line character. */
   for(pch = pchStartOfLine, unCharsBeforeEOL = 0;
      !ODEditIsEOLForMode(pEditInstance, *pch);
      ++unCharsBeforeEOL, ++pch);

   /* If this is the last line in the buffer, then the number of characers */
   /* before the next end of line character is the length of this line.    */
   if(unBufferLine >= pEditInstance->unLinesInBuffer - 1)
   {
      return(unCharsBeforeEOL);
   }

   /* If this is not the last line in the buffer, then the length of this  */
   /* line is the minimum of the number of characters before the next end  */
   /* of line character and the number of characters before the next line, */
   /* according to the line index information. This is because all lines   */
   /* do not necessarily end with an end-of-line character.                */
   else
   {
       return(MIN(unCharsBeforeEOL, (UINT)(ODEditBufferGetCharacter(
          pEditInstance, unBufferLine + 1, 0) - pchStartOfLine)));
   }
}


/* ----------------------------------------------------------------------------
 * ODEditBufferGetTotalLines()                         *** PRIVATE FUNCTION ***
 *
 * Determines the number of lines in the current edit buffer.
 *
 * Parameters: pEditInstance - Editor instance information structure.
 *
 *     Return: The total number of lines in the buffer.
 */
static UINT ODEditBufferGetTotalLines(tEditInstance *pEditInstance)
{
   ASSERT(pEditInstance != NULL);
   ASSERT(pEditInstance->unLinesInBuffer <= pEditInstance->unLineArraySize);

   /* Return the total number of lines in the buffer. */
   return(pEditInstance->unLinesInBuffer);
}


/* ----------------------------------------------------------------------------
 * ODEditBufferGetCharacter()                          *** PRIVATE FUNCTION ***
 *
 * Obtains the character at the specified position in the buffer.
 *
 * Parameters: pEditInstance  - Editor instance information structure.
 *
 *             unBufferLine   - 0-based index of the line in question.
 *
 *             unBufferColumn - The position in the line of the required
 *                              character.
 *
 *     Return: A pointer to the character at the specified position in the
 *             specified line. The caller can assume that any remaining
 *             character(s) in the line follow this character, but should
 *             not assume that the pointer can be used to access following
 *             lines in the buffer.
 */
static char *ODEditBufferGetCharacter(tEditInstance *pEditInstance,
   UINT unBufferLine, UINT unBufferColumn)
{
   ASSERT(pEditInstance != NULL);
   ASSERT(unBufferLine < pEditInstance->unLinesInBuffer);
   ASSERT(pEditInstance->unLinesInBuffer <= pEditInstance->unLineArraySize);
   ASSERT(unBufferColumn <= ODEditBufferGetLineLength(pEditInstance, unBufferLine));

   /* The position of this character is the position of this line, plus */
   /* the number of characters into the line.                           */
   return(pEditInstance->papchStartOfLine[unBufferLine] + unBufferColumn);
}


/* ----------------------------------------------------------------------------
 * ODEditBufferMakeSpace()                             *** PRIVATE FUNCTION ***
 *
 * Moves the remaining buffer contents by the specified distance to make room
 * for new text. The new space is filled by space (' ') characters. Does not
 * necessarily reindex the buffer before returning, and so this should be done
 * by the caller after the new buffer space has been filled.
 *
 * Parameters: pEditInstance       - Editor instance information structure.
 *
 *             unLine              - Line number to make more room on.
 *
 *             unColumn            - Column number to insert the space.
 *
 *             unNumChars          - Number of characters to make room for.
 *
 *     Return: kODRCSuccess on success, kODRCSafeFailure if we were unable to
 *             obtain enough buffer space before making any changes, or
 *             kODRCUnrecoverableFailure if the buffer has been changed, but
 *             there was insufficient memory to re-index the buffer.
 */
static tODResult ODEditBufferMakeSpace(tEditInstance *pEditInstance,
   UINT unLine, UINT unColumn, UINT unNumChars)
{
   UINT unLineLength;
   UINT unBufferUsed;
   UINT unBufferUnused;
   UINT unRemainingBufferBytes;
   UINT unCount;
   char *pchBufferPos;
   tODResult Result;

   ASSERT(pEditInstance != NULL);
   ASSERT(unLine < pEditInstance->unLinesInBuffer);

   /* Determine the current length of the specified line. */
   unLineLength = ODEditBufferGetLineLength(pEditInstance, unLine);

   /* If a column past the current end of this line was specified, then    */
   /* adjust column and number of characters in order to extend the line   */
   /* up to the specified column as well as adding space beginning at that */
   /* column.                                                              */
   if(unColumn > unLineLength)
   {
      UINT unExtendLineBy = unColumn - unLineLength;
      unColumn -= unExtendLineBy;
      unNumChars += unExtendLineBy;
   }

   /* Now, determine whether the buffer is large enough for the additional */
   /* space that will be added.                                            */
   unBufferUsed = strlen(pEditInstance->pszEditBuffer) + 1;
   unBufferUnused = pEditInstance->unBufferSize - unBufferUsed;
   if(unBufferUnused < unNumChars)
   {
      /* There is not currently sufficient room in the buffer for the new */
      /* characters, then attempt to grow the buffer to make more room.   */
      Result = ODEditTryToGrow(pEditInstance, unBufferUsed + unNumChars);
      if(Result != kODRCSuccess)
      {
         /* On failure, return the result code that indicates the nature */
         /* of the failure.                                              */
         return(Result);
      }
   }

   /* Now, shift the buffer contents from this location forward by */
   /* unNumChars characters.                                       */
   pchBufferPos = ODEditBufferGetCharacter(pEditInstance, unLine, unColumn);
   unRemainingBufferBytes = strlen(pchBufferPos) + 1;
   memmove(pchBufferPos + unNumChars, pchBufferPos, unRemainingBufferBytes);

   /* Next, we fill the new buffer area with space characters. */
   for(unCount = 0; unCount < unNumChars; ++unCount)
   {
      *pchBufferPos++ = ' ';
   }

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODEditTryToGrow()                                   *** PRIVATE FUNCTION ***
 *
 * Attempts to reallocate the buffer to a larger size. This function is called
 * if it is found that the current buffer isn't large enough to complete some
 * operation. If the client application has setup the editor to permit buffer
 * reallocation, then this function will call the realloc callback function
 * supplied by the client application. If buffer growing is not possible, then
 * this function automatically fails.
 *
 * Parameters: pEditInstance       - Editor instance information structure.
 *
 *             unSizeNeeded        - The minimum buffer size that is needed.
 *
 *     Return: kODRCSuccess on success, kODRCSafeFailure if we were unable to
 *             obtain enough buffer space before making any changes, or
 *             kODRCUnrecoverableFailure if the buffer has been changed, but
 *             there was insufficient memory to re-index the buffer.
 */
static tODResult ODEditTryToGrow(tEditInstance *pEditInstance,
   UINT unSizeNeeded)
{
   BOOL bFullReIndexRequired = FALSE;

   ASSERT(pEditInstance != NULL);
   ASSERT(unSizeNeeded > pEditInstance->unBufferSize);

   if(pEditInstance->pUserOptions->pfBufferRealloc != NULL)
   {
      /* If the buffer is growable, then attempt to grow it using the */
      /* realloc function provided by the client application.         */
      UINT unNewBufferSize = MAX(pEditInstance->unBufferSize
         + BUFFER_GROW_SIZE, unSizeNeeded);
      char *pszNewBuffer = (char *)((*pEditInstance->pUserOptions->
         pfBufferRealloc)(pEditInstance->pszEditBuffer, unNewBufferSize));

      /* If we were unable to grow the buffer, then fail now. At this */
      /* point, nothing has changed, and so the buffer information    */
      /* is still intact and valid.                                   */
      if(pszNewBuffer == NULL)
      {
         return(kODRCSafeFailure);
      }

      /* Otherwise, determine whether the entire buffer will now have to */
      /* be reindexed. This is necessary if the reallocated buffer is at */
      /* a new location than the original was.                           */
      if(pszNewBuffer != pEditInstance->pszEditBuffer)
      {
         bFullReIndexRequired = TRUE;
      }

      /* Now, store the new buffer pointer and buffer size information. */
      pEditInstance->pszEditBuffer = pszNewBuffer;
      pEditInstance->unBufferSize = unNewBufferSize;
   }
   else
   {
      /* If the buffer is not growable, then fail right away. */
      return(kODRCSafeFailure);
   }

   /* If a full reindex is required due to buffer reallocation, then do so. */
   if(bFullReIndexRequired)
   {
      if(!ODEditBufferFormatAndIndex(pEditInstance))
      {
         /* If this fails, then return with failure. */
         return(kODRCUnrecoverableFailure);
      }
      bFullReIndexRequired = FALSE;
   }

   /* If we get to this point, we suceeded in growing the buffer to the */
   /* required size, so return with success.                            */
   return(kODRCSuccess);
}
