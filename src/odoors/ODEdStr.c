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
 *        File: ODEdStr.c
 *
 * Description: Implementation of od_edit_str(). This is the advanced line
 *              editing function which requires ANSI or AVATAR graphics.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Dec 31, 1994  6.00  BP   Use ODTimerSleep() instead of loop.
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Nov 17, 1995  6.00  BP   Use new input queue mechanism.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 23, 1995  6.00  BP   Added EDIT_FLAG_SHOW_SIZE.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 04, 1996  6.00  BP   Use od_get_input().
 *              Jan 12, 1996  6.00  BP   Claim exclusive use of arrow keys.
 *              Jan 31, 1996  6.00  BP   Added timeout for od_get_input().
 *              Feb 10, 1996  6.00  BP   Fixed ...SHOW_SIZE /w ...PERMALITERAL.
 *              Feb 13, 1996  6.00  BP   Added od_get_input() flags parameter.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Apr 08, 1996  6.10  BP   Make 'C' use word capitalization.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODPlat.h"
#include "ODKrnl.h"
#include "ODStat.h"


/* Current od_edit_str() state and settings. */
static INT anCurrentFormatOffset[80];
static BOOL abCurrentFormatLiteral[80];
static char szCurrentOriginalString[81];
static char *pszCurrentInput;
static char *pszCurrentFormat;
static unsigned char nCurrentStringLength;
static char chCurrentBlank;


/* Private helper functions used by od_edit_str(). */
static BOOL ODEditIsCharValidForPos(char chEntered, INT nPosition);
static char ODEditAsCharForPos(char chEntered, INT nPosition);
static void ODEditDisplayPermaliteral(WORD nFlags);


/* ----------------------------------------------------------------------------
 * od_edit_str()
 *
 * Provides more advanced editing capabilities than od_get_str(), requiring
 * ANSI, AVATAR or RIP modes.
 *
 * Parameters: pszInput          - Pointer to string where inputted text is
 *                                 stored.
 *
 *             pszFormat         - Pointer to format string, which specifies
 *                                 the format of inputted text.
 *
 *             nRow              - The row number where the input field should
 *                                 begin.
 *
 *             nColumn           - The column number where the input field
 *                                 should begin.
 *
 *             btNormalColour    - Color of normal text.
 *
 *             btHighlightColour - Color of highlighted text.
 *
 *             chBlank           - Character to display blanks with.
 *
 *             nFlags            - Specifies one or more flags, combined with
 *                                 the bitwise-or operator.
 *
 *     Return: One of a number of possible EDIT_RETURN_ values, which indicate
 *             why the function returned.
 */
ODAPIDEF WORD ODCALL od_edit_str(char *pszInput, char *pszFormat, INT nRow,
   INT nColumn, BYTE btNormalColour, BYTE btHighlightColour,
   char chBlank, WORD nFlags)
{
   char chTemp;
   unsigned int nCount;
   unsigned char chCurrentValue;
   char *pchCurrent;
   unsigned int nCursorPos;
   INT nKeysPressed = 0;
   WORD wToReturn;
   BOOL bInsertMode = TRUE;
   char chAddAtEnd = '\0';
   BOOL bNormal = TRUE;
   tODInputEvent InputEvent;

   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_edit_str()");

   /* Verify that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Store pointers to current input string and current format string. */
   pszCurrentInput=(char *)pszInput;
   pszCurrentFormat=(char *)pszFormat;

   /* Check that the parameters passed in are valid. */
   if(pszCurrentInput == NULL || pszCurrentFormat == NULL || nRow < 1
      || nColumn < 1)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(EDIT_RETURN_ERROR);
   }

   /* Initially, the maximum length of input string is 0. */
   nCurrentStringLength = 0;

   /* The type that is being examined. */
   chCurrentValue = 0;

   /* Counter of position in format string. */
   nCount = 0;

   /* Loop until we reach the end fo the format string. */
   for(pchCurrent = pszCurrentFormat; *pchCurrent;)
   {
      /* Get next character from format string. */
      chTemp = *pchCurrent++;

      /* If current character is not a literal value. */
      if(chCurrentValue == '\0')
      {
         /* If format string has " or ' characters, then this is the */
         /* beginning of a literal string.                           */
         if(chTemp == 39 || chTemp == 34)
         {
            chCurrentValue = chTemp;
         }

         /* If this is not a literal character, and not a space character... */
         else if(chTemp != 32)
         {
            /* Check that we haven't exceeded the maximum allowable string */
            /* length.                                                     */
            if(nCurrentStringLength >= 80)
            {
               od_control.od_error = ERR_PARAMETER;
               OD_API_EXIT();
               return(EDIT_RETURN_ERROR);
            }

            /* Record format character's position. */
            anCurrentFormatOffset[nCurrentStringLength] = nCount;

            /* Record that this character is not a literal. */
            abCurrentFormatLiteral[nCurrentStringLength] = FALSE;

            /* Increment length of input string. */
            ++nCurrentStringLength;
         }
      }

      /* If this is a literal character. */
      else
      {
         /* Check for end of literal string. */
         if(chTemp == chCurrentValue)
         {
            /* If found, stop literal string processing */
            chCurrentValue = '\0';
         }
         else
         {
            /* Check that we haven't exceeded the maximum allowable string */
            /* length.                                                     */
            if(nCurrentStringLength >= 80)
            {
               od_control.od_error = ERR_PARAMETER;
               OD_API_EXIT();
               return(EDIT_RETURN_ERROR);
            }

            /* Record character's position. */
            anCurrentFormatOffset[nCurrentStringLength] = nCount;

            /* Record that character IS a literal value. */
            abCurrentFormatLiteral[nCurrentStringLength] = TRUE;

            /* Increment length of input string. */
            ++nCurrentStringLength;
         }
      }

      /* Increment format string position. */
      ++nCount;
   }

   /* Check that there is at least one character permitted in the input */
   /* string. If not, return with a parameter error.                    */
   if(nCurrentStringLength==0)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(EDIT_RETURN_ERROR);
   }

   /* If editing an existing string. */
   if(nFlags & EDIT_FLAG_EDIT_STRING)
   {
      /* Check for valid existing input string. */
      if(strlen(pszCurrentInput) > nCurrentStringLength)
      {
         pszCurrentInput[nCurrentStringLength] = '\0';
      }

      /* Start with cursor at the end of the string. */
      nCursorPos = strlen(pszCurrentInput);
   }

   /* If we are not editing an existing string. */
   else
   {
      /* Blank-out current string contents. */
      pszCurrentInput[0] = '\0';

      /* Set cursor to beginning of string. */
      nCursorPos = 0;
   }

   /* Store original string, in case user cancels. */
   strcpy(szCurrentOriginalString,pszCurrentInput);

   /* Set appropriate text color. */
   od_set_attrib(btHighlightColour);

   /* Determine appropriate blank character */
   chCurrentBlank = (nFlags & EDIT_FLAG_PASSWORD_MODE) ? ' ' : chBlank;

   /* Turn off insert mode if the strict input or permaliteral flags were */
   /* specified.                                                          */
   if((nFlags & EDIT_FLAG_STRICT_INPUT) || (nFlags & EDIT_FLAG_PERMALITERAL))
   {
      bInsertMode = FALSE;
   }

   /* If the no-initial-redraw flag is not set, then do initial redraw. */
   if(!(nFlags & EDIT_FLAG_NO_REDRAW))
   {
      /* Set to redraw position. */
      od_set_cursor(nRow, nColumn);

      if(nFlags & EDIT_FLAG_PASSWORD_MODE)
      {
         /* If we are in password mode, then just draw password blanks. */
         od_repeat(chBlank, (BYTE)strlen(pszCurrentInput));
      }
      else
      {
         /* Otherwise, display the actual string. */
         od_disp_str(pszCurrentInput);
      }

      if(nFlags & EDIT_FLAG_PERMALITERAL)
      {
         /* If we are in permaliteral mode, then fill the remaining edit */
         /* field with the literal characters.                           */
         ODEditDisplayPermaliteral(nFlags);
      }
      else
      {
         /* Otherwise, fill the remaining edit field with the blank */
         /* character.                                              */
         BYTE btRemaining
            = (BYTE)(nCurrentStringLength - strlen(pszCurrentInput));
         if(!(nFlags & EDIT_FLAG_SHOW_SIZE)) ++btRemaining;
         od_repeat(chCurrentBlank, btRemaining);
      }
   }

   /* Claim exclusive use of arrow keys. */
   ODStatStartArrowUse();

   /* Set the cursor to appropriate position. */
   od_set_cursor(nRow, nColumn + nCursorPos);

   /* Normally, we start the input loop at the keep_going tag. */
   if(bNormal) goto keep_going;

   for(;;)
   {
      /* If auto-accept mode has been specified ... */
      if(nFlags & EDIT_FLAG_AUTO_ENTER)
      {
         /* ... then check whether we have reached the end of the string. */
         if(strlen(pszCurrentInput) == nCurrentStringLength)
         {
            /* Indicate that input has been accepted, rather than cancelled. */
            wToReturn = EDIT_RETURN_ACCEPT;

            /* Return the current string to the caller, if it is valid. */
            goto try_to_accept;
         }
      }

keep_going:
      /* Check whether we have reached a literal character in permaliteral */
      /* mode. If so, we will move past the permanent literal characters   */
      /* automatically.                                                    */
      if((nFlags & EDIT_FLAG_PERMALITERAL)
         && (nCursorPos < nCurrentStringLength))
      {
         if(abCurrentFormatLiteral[nCursorPos])
         {
            if(nCursorPos < strlen(pszCurrentInput))
            {
               goto pressed_right_arrow;
            }
            chTemp = pszCurrentFormat[anCurrentFormatOffset[nCursorPos]];
            ++nKeysPressed;
            goto try_this_character;
         }
      }

get_another_key:
      /* Block, waiting for the next key pressed by the user. */

      od_get_input(&InputEvent, OD_NO_TIMEOUT, GETIN_NORMAL);
      
      /* Increment total number of keystrokes. */
      ++nKeysPressed;

      if(InputEvent.EventType == EVENT_EXTENDED_KEY)
      {
         switch(InputEvent.chKeyPress)
         {
            case OD_KEY_UP:
            case OD_KEY_SHIFTTAB:
               if(nFlags & EDIT_FLAG_FIELD_MODE)
               {
                  wToReturn = EDIT_RETURN_PREVIOUS;
                  goto try_to_accept;
               }
               break;

            case OD_KEY_DOWN:
pressed_down_arrow:
               if(nFlags & EDIT_FLAG_FIELD_MODE)
               {
                  wToReturn = EDIT_RETURN_NEXT;
                  goto try_to_accept;
               }
               break;

            case OD_KEY_RIGHT:
pressed_right_arrow:
               /* If we are not at the end of the string. */
               if(nCursorPos < strlen(pszCurrentInput))
               {
                  /* Move input position right. */
                  nCursorPos++;

                  /* Move the cursor on screen. */
                  od_set_cursor(nRow, nColumn + nCursorPos);
               }
               if(chAddAtEnd)
               {
                  chAddAtEnd = 0;
                  goto add_another_key;
               }
               break;

            case OD_KEY_LEFT:
pressed_left_arrow:
               /* If we are not at the beginning of the string. */
               if(nCursorPos > 0)
               {
                  /* Move input position left. */
                  nCursorPos--;

                  /* Move cursor on screen. */
                  od_set_cursor(nRow, nColumn + nCursorPos);
               }

               /* If we are moving past a permanent literal character, */
               /* then continue moving further left, if possible.      */
               if((nFlags & EDIT_FLAG_PERMALITERAL)
                  && abCurrentFormatLiteral[nCursorPos] && nCursorPos > 0)
               {
                  goto pressed_left_arrow;
               }
               break;

         case OD_KEY_HOME:
            /* If we are not at the beginning of the string. */
            if(nCursorPos != 0)
            {
               /* Move input position to the beginning of the string. */
               nCursorPos = 0;

               /* Move the cursor on the screen. */
               od_set_cursor(nRow, nColumn);
            }
            break;

         case OD_KEY_END:
            /* If we are not at the end of the string .*/
            if(nCursorPos != strlen(pszCurrentInput))
            {
               /* Set the input position to the end of the string. */
               nCursorPos=strlen(pszCurrentInput);

               /* Move cursor on screen. */
               od_set_cursor(nRow,nColumn+nCursorPos);
            }
            break;

         case OD_KEY_DELETE:
pressed_delete:
            /* Check whether delete key is permitted at this time. */
            if(!(nFlags & EDIT_FLAG_STRICT_INPUT)
               && nCursorPos < strlen(pszCurrentInput)
               && !(nFlags & EDIT_FLAG_PERMALITERAL))
            {
               /* Move remaining line, if any, to the left */
               chCurrentValue = strlen(pszCurrentInput) - 1;
               for(nCount = nCursorPos; nCount < chCurrentValue; ++nCount)
               {
                  od_putch(
                     pszCurrentInput[nCount] = pszCurrentInput[nCount + 1]);
               }

               /* Erase the last character. */
               pszCurrentInput[chCurrentValue] = '\0';  

               /* Blank out last character. */
               od_putch(chCurrentBlank);

               /* Move the cursor on the screen. */
               od_set_cursor(nRow, nColumn + nCursorPos);

               /* Update changes to string. */
               goto check_cursor_char;
            }
            break;

         case OD_KEY_INSERT:
            if(!(nFlags & EDIT_FLAG_STRICT_INPUT)
               && !(nFlags & EDIT_FLAG_PERMALITERAL))
            {
               /* Toggle insert setting. */
               bInsertMode = !bInsertMode;
            }
            break;

         }
      }
      else if(InputEvent.EventType == EVENT_CHARACTER)
      {
         chTemp = InputEvent.chKeyPress;
try_this_character:

         if(chTemp == 27)
         {
            /* If cancel key is allowed ... */
            if(nFlags & EDIT_FLAG_ALLOW_CANCEL)
            {
               /* Reset the input string to the original contents. */
               strcpy(pszCurrentInput, szCurrentOriginalString);

               /* Indicate that return reason was due to user cancelling. */
               wToReturn = EDIT_RETURN_CANCEL;

               /* Return after redrawing the original string in the input */
               /* field.                                                  */
               goto exit_and_redraw;
            }
         }


         /* If user pressed [Enter] or [Ctrl]-[Z]. */
         else if(chTemp == 13 || chTemp == 26)
         {
            /* User has accepted input. */
            wToReturn = EDIT_RETURN_ACCEPT;

            /* Return if input string is valid. */
            goto try_to_accept;
         }

         /* If the backspace key has been pressed. */
         else if(chTemp == 8)
         {
   backspace_again:
            /* If we are not already at the beginning of the string. */
            if(nCursorPos > 0)
            {
               if(nFlags & EDIT_FLAG_PERMALITERAL)
               {
                  for(nCount = 0;nCount < nCursorPos; ++nCount)
                  {
                     if(!abCurrentFormatLiteral[nCount]) goto continue_deletion;
                  }
                  goto get_another_key;
               }

   continue_deletion:
               /* If we are at the end of the string. */
               if(nCursorPos == strlen(pszCurrentInput))
               {
                  /* Erase last char in string. */
                  pszCurrentInput[--nCursorPos] = '\0';

                  if((nFlags & EDIT_FLAG_PERMALITERAL)
                     && abCurrentFormatLiteral[nCursorPos])
                  {
                     goto backspace_again;
                  }
                  else
                  {
                     /* Move to new cursor pos. */
                     od_set_cursor(nRow,nColumn+nCursorPos);

                     /* Blank old character. */
                     od_putch(chCurrentBlank);

                     /* Move again to cursor pos. */
                     od_set_cursor(nRow,nColumn+nCursorPos);
                  }
               }

               /* If we are in the middle of the string and we are not in */
               /* string input mode.                                      */
               else if(!(nFlags & EDIT_FLAG_STRICT_INPUT)
                  && !(nFlags & EDIT_FLAG_PERMALITERAL))
               {
                  /* Move cursor left. */
                  --nCursorPos;

                  /* Move cursor on screen. */
                  od_set_cursor(nRow, nColumn + nCursorPos);

                  /* Goto standard delete handler. */
                  goto pressed_delete;
               }
            }
         }

         /* If this is a next field request. */
         else if(chTemp == 9)
         {
            /* Goto down arrow handler. */
            goto pressed_down_arrow;
         }

         /* If Control-Y. */
         else if(chTemp == 25)
         {
            /* Erase entire contents of line. */
            goto kill_whole_line;
         }

         else
         {
            /* If this is the first key pressed, and we are in autodelete mode. */
            if(nKeysPressed == 1 && (nFlags & EDIT_FLAG_AUTO_DELETE))
            {
   kill_whole_line:
               /* If string is not empty. */
               if(strlen(pszCurrentInput) != 0)
               {
                  /* Move to beginning of string. */
                  od_set_cursor(nRow,nColumn);

                  /* Blank out the entire string contents. */
                  od_repeat(chCurrentBlank, (BYTE)strlen(pszCurrentInput));
               }

               /* Move to new cursor position. */
               od_set_cursor(nRow,nColumn);

               /* Update insert position. */
               nCursorPos = 0;

               /* Blank out the current string contents. */
               pszCurrentInput[0] = '\0';
            }

   add_another_key:
            if(!ODEditIsCharValidForPos(chTemp,nCursorPos))
            {
               /* If character is not a valid input char. */
               if(abCurrentFormatLiteral[nCursorPos])
               {
                  if(nCursorPos < strlen(pszCurrentInput))
                  {
                     if(pszCurrentInput[nCursorPos] == 
                        pszCurrentFormat[anCurrentFormatOffset[nCursorPos]])
                     {
                        chAddAtEnd = chTemp;
                        goto pressed_right_arrow;
                     }
                  }
                  chAddAtEnd = chTemp;
                  chTemp = pszCurrentFormat[anCurrentFormatOffset[nCursorPos]];
               }
               else
               {
                  continue;
               }
            }

            /* Convert character to correct value, if applicable. */
            chTemp = ODEditAsCharForPos(chTemp, nCursorPos);

            /* If we are at end of string. */
            if(nCursorPos >= strlen(pszCurrentInput))
            {
               /* Reset original cursor position */
               nCursorPos = strlen(pszCurrentInput);

               /* If there is room to add a char. */
               if(nCursorPos < nCurrentStringLength)
               {
                  /* If password mode */
                  if(nFlags & EDIT_FLAG_PASSWORD_MODE)
                  {
                     /* Display the password character. */
                     od_putch(chBlank);
                  }
                  /* If not in password mode. */
                  else
                  {
                     /* Display the character. */
                     od_putch(chTemp);
                  }

                  /* Store the character. */
                  pszCurrentInput[nCursorPos] = chTemp;

                  /* Add a new string terminator. */
                  pszCurrentInput[++nCursorPos] = '\0';
               }
            }

            /* If in insert mode, but not at end of string. */
            else if(bInsertMode)
            {
               /* If room in string. */
               if(strlen(pszCurrentInput) < nCurrentStringLength)
               {
                  /* If in password mode. */
                  if(nFlags & EDIT_FLAG_PASSWORD_MODE)
                  {
                     /* Move to end. */
                     od_set_cursor(nRow,nColumn+strlen(pszCurrentInput));

                     /* Add another password character. */
                     od_putch(chBlank);
                  }

                  /* If not in password mode. */
                  else
                  {
                     /* Display the new character. */
                     od_putch(chTemp);

                     /* Loop through rest of string. */
                     for(nCount = nCursorPos; nCount < strlen(pszCurrentInput);
                        ++nCount)
                     {
                        /* Display the next remaining character. */
                        od_putch(pszCurrentInput[nCount]);
                     }
                  }

                  pszCurrentInput[(strlen(pszCurrentInput) + 1)] = '\0';

                  /* Sift remaining characters forward. */
                  for(nCount = strlen(pszCurrentInput); nCount > nCursorPos;
                     --nCount)
                  {                     
                     pszCurrentInput[nCount] = pszCurrentInput[nCount-1];
                  }

                  /* Add new char in space. */
                  pszCurrentInput[nCursorPos++] = chTemp;

                  /* Move to new cursor position. */
                  od_set_cursor(nRow, nColumn + nCursorPos);
               }
               else
               {
                  goto get_another_key;
               }
            }

            /* If we are in overwrite mode, but not at end of string. */
            else
            {
               /* If password mode. */
               if(nFlags & EDIT_FLAG_PASSWORD_MODE)
               {
                  /* Display the password character. */
                  od_putch(chBlank);
               }
               /* If not in password mode. */
               else
               {
                  /* Display the character. */
                  od_putch(chTemp);
               }

               /* Add character to string. */
               pszCurrentInput[nCursorPos++] = chTemp;
            }

            /* If not at end of possible string. */
            if(nCursorPos < nCurrentStringLength)
            {
               /* If the next character is literal constant. */
               if(abCurrentFormatLiteral[nCursorPos])
               {
                  chTemp = pszCurrentFormat[anCurrentFormatOffset[nCursorPos]];
                  goto add_another_key;
               }
            }

            if(chAddAtEnd)
            {
               chTemp = chAddAtEnd;
               chAddAtEnd = 0;
               goto add_another_key;
            }


   check_cursor_char:
            /* If there is a character under cursor. */
            if(nCursorPos < strlen(pszCurrentInput))
            {
               /* If character corresponds to the format string. */
               if(ODEditIsCharValidForPos(pszCurrentInput[nCursorPos],
                  nCursorPos))
               {
                  /* Determine correct character for this position. */
                  chTemp = ODEditAsCharForPos(pszCurrentInput[nCursorPos],
                     nCursorPos);

                  /* If actual character is not correct. */
                  if(chTemp != pszCurrentInput[nCursorPos])
                  {                        
                     /* Change character to correct value. */
                     pszCurrentInput[nCursorPos] = chTemp;

                     /* If password mode. */
                     if(nFlags & EDIT_FLAG_PASSWORD_MODE)
                     {
                        /* Display the password character. */
                        od_putch(chBlank);
                     }

                     /* If not in password mode. */
                     else
                     {
                        /* Display the character. */
                        od_putch(chTemp);
                     }

                     /* Reset cursor position. */
                     od_set_cursor(nRow, nColumn + nCursorPos);
                  }
               }
            }
         }
      }
   }

   /* Accept string if it is valid. */
try_to_accept:
   /* If string must be filled. */
   if(nFlags & EDIT_FLAG_FILL_STRING)
   {
      /* If string is not filled, don't return. */
      if(strlen(pszCurrentInput) != nCurrentStringLength) goto keep_going;
   }

   /* Loop through string .... */
   for(nCount = 0; nCount < strlen(pszCurrentInput); ++nCount)
   {
      /* ... testing each character for validity. */
      if(!ODEditIsCharValidForPos(pszCurrentInput[nCount], nCount))
         goto keep_going;
   }

   /* Initially, assume that the string has not been changed. */
   chCurrentValue = FALSE;

   /* Loop through the string. */
   for(nCount = 0; nCount < strlen(pszCurrentInput); ++nCount)
   {
      /* Find correct value for each character. */
      chTemp = ODEditAsCharForPos(pszCurrentInput[nCount], nCount);

      /* If character is not correct. */
      if(chTemp != pszCurrentInput[nCount])           
      {
         /* Change char to correct value */
         pszCurrentInput[nCount] = chTemp;

         /* Remember that string has been changed. */
         chCurrentValue = TRUE;
      }
   }

   /* If permaliteral mode. */
   if(nFlags & EDIT_FLAG_LEAVE_BLANK)
   {
      /* Count # of literal characters. */
      nCount = 0;
      while(nCount<strlen(pszCurrentInput))
      {
         if(abCurrentFormatLiteral[nCount])
         {
            ++nCount;
         }
         else
         {
            break;
         }
      }

      /* If only literals in string. */
      if(strlen(pszCurrentInput) == nCount && nCount > 0)
      {
         /* Then they shouldn't be here. */
         pszCurrentInput[0] = '\0';
         goto exit_and_redraw;
      }
   }

   /* Always redraw if string was changed. */
   if(chCurrentValue) goto exit_and_redraw;

   /* If no-redraw flag not set. */
   if(!(nFlags & EDIT_FLAG_NO_REDRAW))
   {
exit_and_redraw:
      /* Set appropriate text colour. */
      od_set_attrib(btNormalColour);

      /* Set to redraw position. */
      od_set_cursor(nRow,nColumn);

      /* If password mode. */
      if(nFlags & EDIT_FLAG_PASSWORD_MODE)
      {
         /* Display blanked-out string. */
         od_repeat(chBlank, (BYTE)strlen(pszCurrentInput));
      }
      else
      {
         /* Display actual string. */
         od_disp_str(pszCurrentInput);
      }

      /* If we should keep the background. */
      if(nFlags & EDIT_FLAG_KEEP_BLANK)
      {
         /* Then redraw background. */
         if(nFlags & EDIT_FLAG_PERMALITERAL)
         {
            ODEditDisplayPermaliteral(nFlags);
         }
         else
         {
            od_repeat(chCurrentBlank,
               (BYTE)(nCurrentStringLength - strlen(pszCurrentInput) + 1));
         }
      }
      /* If we should erase the background ... */
      else
      {
         /* ... then do it. */
         od_repeat(' ',
            (BYTE)(nCurrentStringLength - strlen(pszCurrentInput) + 1));
      }
   }

   /* Release exclusive use of arrow keys. */
   ODStatEndArrowUse();

   /* Return with appropriate return value. */
   OD_API_EXIT();
   return(wToReturn);
}



/* ----------------------------------------------------------------------------
 * ODEditIsCharValidForPos()                           *** PRIVATE FUNCTION ***
 *
 * Determines whether or not the entered character can be accepted as a valid
 * character (after any possible conversion by ODEditAsCharForPos() is applied)
 * for the specified position in the string.
 *
 * Parameters: chEntered   - The character entered by the user.
 *
 *             nPosition   - The position in the string where this character
 *                           would be inserted.
 *
 *     Return: TRUE if this character should be accepted, FALSE if not.
 */
static BOOL ODEditIsCharValidForPos(char chEntered, INT nPosition)
{
   /* If this character is a literal. */
   if(abCurrentFormatLiteral[nPosition])
   {                                     
      /* Check required literal character. */
      if(chEntered != pszCurrentFormat[anCurrentFormatOffset[nPosition]])
      {                                          
         /* If this is not the correct literal character, then do not */
         /* permit it to be entered in this position. */
         return(FALSE);
      }
      return(TRUE);
   }

   /* If this position has a corresponding format control character, */
   /* then check that control character. The execution path will     */
   /* continue out of this switch statement (rather than returning   */
   /* to the calling function) if and only if the entered character  */
   /* is valid for the format character specified.                   */
   switch(pszCurrentFormat[anCurrentFormatOffset[nPosition]])
   {
      /* Only numerical characters are to be permitted. */
      case '#':
         if(chEntered < '0' || chEntered > '9') return(FALSE);
         break;

      /* Only numerical and space characters are to be permitted. */
      case '%':
         if((chEntered < '0' || chEntered > '9') && chEntered != ' ')
         {
            return(FALSE);
         }
         break;

      /* Only floating point number characters are to be permitted. */
      case '9':
         if(chEntered >= '0' && chEntered <= '9') break;
         if(chEntered == '.' || chEntered == '+' || chEntered == '-') break;
         return(FALSE);

      /* Only "printable" characters are to be permitted. */
      case '*':
         if(chEntered < 32) return(FALSE);
         break;

      /* City name characters are to be permitted. */
      case 'C':
      case 'c':
         if(chEntered >= 'A' && chEntered <= 'Z') break;
         if(chEntered >= 'a' && chEntered <= 'z') break;
         if(chEntered == ' ' || chEntered == ',' || chEntered == '.') break;
         if(chEntered == '*' || chEntered == '?') break;
         return(FALSE);

      /* If only alphabetic characters are to be permitted. */
      case 'A':
      case 'a':
      case 'L':
      case 'l':
      case 'M':
      case 'm':
      case 'U':
      case 'u':
         if(chEntered>='A' && chEntered<='Z') break;
         if(chEntered>='a' && chEntered<='z') break;
         if(chEntered==' ') break;
         return(FALSE);

      /* If only date characters are to be permitted. */
      case 'D':
      case 'd':
         if(chEntered>='0' && chEntered<='9') break;
         if(chEntered=='-' || chEntered=='/') break;
         return(FALSE);

      /* If only MS-DOS filename characters are to be permitted. */
      case 'F':
      case 'f':
         if(chEntered >= 'A' && chEntered <= 'Z') break;
         if(chEntered >= '0' && chEntered <= '9') break;
         if(chEntered >= 'a' && chEntered <= 'z') break;
         switch(chEntered)
         {
            /* Filename separators. */
            case ':':
            case '.':
            case DIRSEP:

            /* Wildcard characters. */
            case '?':
            case '*':

            /* Other valid symbols in filenames */
            case '#':
            case '$':
            case '&':
            case '\'':
            case '(':
            case '>':
            case '-':
            case '@':
            case '_':
            case '!':
            case '{':
            case '}':
            case '~':
               return(TRUE);
         }

         return(FALSE);

      /* If only hexidecimal characters are to be permitted. */
      case 'H':
      case 'h':
         if(chEntered>='0' && chEntered<='9') break;
         if(chEntered>='A' && chEntered<='F') break;
         if(chEntered>='a' && chEntered<='f') break;
         return(FALSE);

      /* If only telephone number characters are to be permitted. */
      case 'T':                                          
      case 't':
         if(chEntered >= '0' && chEntered <= '9') break;
         if(chEntered == '-' || chEntered == '(' || chEntered == ')'
            || chEntered == ' ' || chEntered == '+')
         {
            break;
         }
         return(FALSE);

      /* If filenames with wildcards are to be permitted. */
      case 'W':
      case 'w':
         if(chEntered >= 'A' && chEntered <= 'Z') break;
         if(chEntered >= 'a' && chEntered <= 'z') break;
         if(chEntered == ':' || chEntered == '.' || chEntered == DIRSEP
            || chEntered == '*' || chEntered == '?')
         {
            break;
         }
         return(FALSE);

      /* If alpha-numeric characters are to be permitted. */
      case 'X':
      case 'x':
         if(chEntered >= 'A' && chEntered <= 'Z') break;
         if(chEntered >= 'a' && chEntered <= 'z') break;
         if(chEntered >= '0' && chEntered <= '9') break;
         if(chEntered == ' ') break;
         return(FALSE);

      /* If this is a Yes/No field. */
      case 'Y':
      case 'y':
         if(chEntered == 'y' || chEntered == 'n' || chEntered == 'Y'
            || chEntered == 'N')
         {
            break;
         }
         return(FALSE);
   }

   /* If execution gets to this point, then the character has been approved. */
   return(TRUE);
}



/* ----------------------------------------------------------------------------
 * ODEditAsCharForPos()                                *** PRIVATE FUNCTION ***
 *
 * Converts the character entered by the user to a valid character for this
 * position in the string. For example, for fields that are set to all
 * upper case, this function converts the entered characte to its upper case
 * equivalent.
 *
 * Parameters: chEntered   - Character that was entered by the user.
 *
 *             nPosition   - Position in the string where the character is to
 *                           be entered.
 *
 *     Return: The actual character to add to the input string at this
 *             position.
 */
static char ODEditAsCharForPos(char chEntered, INT nPosition)
{
   /* If this character is a literal. */
   if(abCurrentFormatLiteral[nPosition])
   {                                      
      /* Return the only valid char for this position. */
      return(pszCurrentFormat[anCurrentFormatOffset[nPosition]]);
   }

   /* If this position has a corresponding format control character, */
   /* then check that control character. */
   switch(pszCurrentFormat[anCurrentFormatOffset[nPosition]])
   {
      /* If Yes/No characters are required. */
      case 'Y':
      case 'y':
         return(toupper(chEntered));

      /* If filename characters are required. */
      case 'F':                                       
      case 'f':
         return(toupper(chEntered));

      /* If lower case characters are required. */
      case 'L':
      case 'l':
         return(tolower(chEntered));

      /* If upper case characters are required. */
      case 'U':
      case 'u':
         return(toupper(chEntered));

      /* If automatic capitalization is required. */
      case 'M':
      case 'm':
      case 'C':
      case 'c':
         /* First character is always upper case. */
         if(nPosition == 0) return(toupper(chEntered));

         /* Check for other base cases. */
         if(abCurrentFormatLiteral[nPosition-1]) return(toupper(chEntered));
         if(toupper(pszCurrentFormat[anCurrentFormatOffset[nPosition]]) != 'M'
            && toupper(pszCurrentFormat[anCurrentFormatOffset[nPosition]])
            != 'C')
         {
            return(toupper(chEntered));
         }

         /* If previous character is a word delimiter, then this character */
         /* should be uppper case.                                         */
         if(pszCurrentInput[nPosition-1] == ' '
            || pszCurrentInput[nPosition-1] == '.'
            || pszCurrentInput[nPosition-1] == ','
            || pszCurrentInput[nPosition-1] == '-')
         {
            return(toupper(chEntered));                                             /* Otherwise, this should be lower */
         }

         /* Otherwise, this character should be lower-case. */
         return(tolower(chEntered));
   }

   return(chEntered);
}


/* ----------------------------------------------------------------------------
 * ODEditDisplayPermaliteral()                         *** PRIVATE FUNCTION ***
 *
 * Displays permaliterals (characters specified in the format string that
 * should be returned in the input string, but which the user may never
 * change).
 *
 * Parameters: nFlags - Flags parameter that was passed into od_edit_str().
 *
 *     Return: void
 */
static void ODEditDisplayPermaliteral(WORD nFlags)
{
   INT nCount;
   BYTE btRepeat = 0;

   for(nCount = strlen(pszCurrentInput); nCount <= nCurrentStringLength;
      ++nCount)
   {
      if(nCount != nCurrentStringLength)
      {
         if(abCurrentFormatLiteral[nCount])
         {
            if(btRepeat > 0)
            {
               od_repeat(chCurrentBlank, btRepeat);
               btRepeat = 0;
            }
            od_putch(pszCurrentFormat[anCurrentFormatOffset[nCount]]);
         }
         else
         {
            ++btRepeat;
         }
      }
      else
      {
         if(!(nFlags & EDIT_FLAG_SHOW_SIZE))
         {
            ++btRepeat;
         }
      }
   }

   if(btRepeat > 0) od_repeat(chCurrentBlank, btRepeat);
}
