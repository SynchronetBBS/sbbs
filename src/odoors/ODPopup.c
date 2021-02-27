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
 *        File: ODPopUp.c
 *
 * Description: Implements od_popup_menu(), for displaying a menu in
 *              a window, allowing the user to make a selection using
 *              "hot keys" or by using arrow keys.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Jan 15, 1995  6.00  BP   Free menu structure on menu destroy.
 *              Feb 02, 1995  6.00  BP   Added od_yield() call in for(;;) loop.
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Change valid range of nLevel to 0-10.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Nov 17, 1995  6.00  BP   Use new input queue mechanism.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 23, 1995  6.00  BP   Restore original color on exit.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 04, 1996  6.00  BP   Use od_get_input().
 *              Jan 12, 1996  6.00  BP   Claim exclusive use of arrow keys.
 *              Jan 30, 1996  6.00  BP   Replaced od_yield() with od_sleep().
 *              Jan 31, 1996  6.00  BP   Added timeout for od_get_input().
 *              Jan 31, 1996  6.00  BP   Add ODPopupCheckForKey() wait param.
 *              Jan 31, 1996  6.00  BP   Ignore left & right if !MENU_PULLDOWN.
 *              Feb 13, 1996  6.00  BP   Added od_get_input() flags parameter.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODPlat.h"
#include "ODKrnl.h"
#include "ODStat.h"


/* Configurable od_popup_menu() parameters. */

/* Maximum menu level. */
#define MENU_LEVELS        11

/* Maximum number of items in a menu. */
#define MAX_MENU_ITEMS     21

/* Maximum width of any menu item. */
#define MAX_ITEM_WIDTH     76


/* Other manifest constants. */
#define NO_COMMAND         -10


/* Local data types. */

/* Information on an individual menu item. */
typedef struct
{
   char szItemText[MAX_ITEM_WIDTH + 1];
   BYTE btKeyIndex;
} tMenuItem;

/* Information on a popup menu level. */
typedef struct
{
   tMenuItem *paMenuItems;
   BYTE btNumMenuItems;
   BYTE btWidth;
   BYTE btRight;
   BYTE btBottom;
   BYTE btCursor;
   BYTE btLeft;
   BYTE btTop;
   WORD wFlags;
   void *pWindow;
} tMenuLevelInfo;


/* Private variables. */

/* Array of information on each menu level. */
tMenuLevelInfo MenuLevelInfo[MENU_LEVELS];

/* Current menu settings. */
static BYTE btCorrectItem;
static INT nCommand;
static WORD wCurrentFlags;
static BYTE btCurrentNumMenuItems;
static INT nCurrentLevel;


/* Private helper functions used by od_popup_menu(). */
static void ODPopupCheckForKey(BOOL bWaitForInput);
static void ODPopupDisplayMenuItem(BYTE btLeft, BYTE btTop,
   tMenuItem *paMenuItems, BYTE btItemIndex, BOOL bHighlighted, BYTE btWidth,
   BOOL bPositionCursor);


/* ----------------------------------------------------------------------------
 * od_popup_menu()
 *
 * Displays a popup menu on the local and remote screens.
 *
 * Parameters: pszTitle - Text to show as the window title of the popup menu.
 *                        If no title is desired, this parameter should be set
 *                        to either "" or NULL.
 *
 *             pszText  - String which contains the menu definition. In the
 *                        menu definition string, individual menu items are
 *                        separated by a pipe ('|') character, and hotkeys are
 *                        proceeded by a carat ('^') character.
 *
 *             nLeft    - The 1-based column number of the upper right corner
 *                        of the menu.
 *
 *             nTop     - The 1-based row number of the upper right corner of
 *                        the menu.
 *
 *             nLevel   - Menu level, which must be a value between 0 and
 *                        MENU_LEVELS.
 *
 *             uFlags   - One or more flags, combined by the bitwise or (|)
 *                        operator.
 *
 *     Return: POPUP_ERROR on error, POPUP_ESCAPE if user pressed the Escape
 *             key, POPUP_LEFT if the user choose to move to the next menu to
 *             the left, POPUP_RIGHT if the user choose to move to the next
 *             menu to the right, or a postive value if the user choose an item
 *             from the menu. In this case, the return value is the 1-based
 *             index of the selected menu item.
 */
ODAPIDEF INT ODCALL od_popup_menu(char *pszTitle, char *pszText, INT nLeft,
   INT nTop, INT nLevel, WORD uFlags)
{
   tMenuItem *paMenuItems = NULL;
   BYTE btCount;
   BYTE btWidth;
   BYTE btRight;
   BYTE btBottom;
   BYTE btCursor;
   BYTE btLeft;
   BYTE btTop;
   void *pWindow;
   BYTE btBetweenSize;
   BYTE btTitleSize;
   BYTE btRemaining;
   BYTE btLineCount;
   INT16 nOriginalAttrib;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_popup_menu()");

   /* Initialize OpenDoors, if not already done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Setup od_box_chars appropriately. */
   if(od_control.od_box_chars[BOX_BOTTOM] == 0)
   {
      od_control.od_box_chars[BOX_BOTTOM] = od_control.od_box_chars[BOX_TOP];
   }
   if(od_control.od_box_chars[BOX_RIGHT] == 0)
   {
      od_control.od_box_chars[BOX_RIGHT] = od_control.od_box_chars[BOX_LEFT];
   }

   /* Store initial display color. */
   nOriginalAttrib = od_control.od_cur_attrib;


   /* check level bounds */
   if(nLevel < 0 || nLevel > MENU_LEVELS)
   {
      od_control.od_error = ERR_LIMIT;
      OD_API_EXIT();
      return(POPUP_ERROR);
   }
   /* normalize level */
   nCurrentLevel = nLevel;

   if(MenuLevelInfo[nLevel].pWindow == NULL)
   {
      btLeft = nLeft;
      btTop = nTop;
      wCurrentFlags = uFlags;

      if(pszText == NULL)
      {
         od_control.od_error = ERR_PARAMETER;
         OD_API_EXIT();
         return(POPUP_ERROR);
      }

      if(paMenuItems == NULL)
      {
         if((paMenuItems = malloc(sizeof(tMenuItem) * MAX_MENU_ITEMS)) == NULL)
         {
            od_control.od_error = ERR_PARAMETER;
            OD_API_EXIT();
            return(POPUP_ERROR);
         }
      }
      MenuLevelInfo[nLevel].paMenuItems = paMenuItems;

      btCurrentNumMenuItems = 0;
      btWidth = 0;
      btCount = 0;
      nCommand = NO_COMMAND;
      paMenuItems[0].btKeyIndex = 0;
      while(*pszText && btCurrentNumMenuItems < MAX_MENU_ITEMS)
      {
         switch(*pszText)
         {
            case '|':
                  paMenuItems[btCurrentNumMenuItems++].szItemText[btCount]
                     = '\0';
                  if(btCount > btWidth) btWidth = btCount;
                  btCount = 0;
                  paMenuItems[btCurrentNumMenuItems].btKeyIndex = 0;
               break;

            case '^':
               if(btCount < MAX_ITEM_WIDTH)
               {
                  paMenuItems[btCurrentNumMenuItems].btKeyIndex = btCount;
               }
               break;

            default:
               if(btCount < MAX_ITEM_WIDTH)
               {
                  paMenuItems[btCurrentNumMenuItems].szItemText[btCount++] =
                     *pszText;
               }
         }
         ++pszText;
      }

      /* If we were in the middle of a menu item when we encountered the end */
      /* of the string, then it should form an additional menu entry. This   */
      /* handles the case of a menu string to no terminating | for the last  */
      /* entry.                                                              */
      if(btCount != 0)
      {
         /* null-terminate current menu entry string */
         paMenuItems[btCurrentNumMenuItems++].szItemText[btCount] = '\0';

         /* If this is the widest entry, update he menu width appropriately  */
         if(btCount > btWidth) btWidth = btCount;
      }

      /* If the menu description string does not contain any menu items */
      if(btCurrentNumMenuItems == 0)
      {
         /* Return with parameter error */
         od_control.od_error = ERR_PARAMETER;
         OD_API_EXIT();
         return(POPUP_ERROR);
      }

      /* Adjust menu width to allow title to fit, if possible               */
      /* If a title string was passed, and that string is wider than widest */
      /* menu entry ...                                                     */
      if(pszTitle != NULL && strlen(pszTitle) + 2 > btWidth)
      {
         /* Then width of menu window should be large enough to allow up to */
         /* the first 76 characters of the title to fit.                    */
         btWidth = strlen(pszTitle) + 2 > MAX_ITEM_WIDTH
            ? MAX_ITEM_WIDTH : strlen(pszTitle) + 2;
      }

      /* Based on number and size of menu items, and width of title,         */
      /* determine the bottom, right and inside width of the menu.           */
      btBottom = btTop + btCurrentNumMenuItems + 1;
      btRight = btLeft + btWidth + 3;
      btBetweenSize = (btRight - btLeft) - 1;

      /* If neither ANSI nor AVATAR mode is available, return with an error */
      if(!(od_control.user_ansi || od_control.user_avatar))
      {
         od_control.od_error = ERR_NOGRAPHICS;
         OD_API_EXIT();
         return(POPUP_ERROR);
      }

      /* If menu would "fall off" edge of screen, return with an error */
      if(btLeft < 1 || btTop < 1 || btRight > OD_SCREEN_WIDTH
         || btBottom > OD_SCREEN_HEIGHT || btRight - btLeft < 2
         || btBottom - btTop < 2)
      {
         od_control.od_error = ERR_PARAMETER;
         OD_API_EXIT();
         return(POPUP_ERROR);
      }

      /* Allocate space to store window information. If unable to allocate */
      /* enough space, return with an error.                               */
      if((pWindow = malloc((btRight - btLeft + 1) * 2
         + (btBottom - btTop + 1) * 160)) == NULL)
      {
         od_control.od_error = ERR_MEMORY;
         OD_API_EXIT();
         return(POPUP_ERROR);
      }

      /* Store contents of screen where memu will be drawn in the temporary */
      /* buffer.                                                            */
      if(!od_gettext(btLeft, btTop, btRight, btBottom, pWindow))
      {
         free(pWindow);
         pWindow = NULL;

         /* Note that od_error code has been set in od_gettext(). */
         OD_API_EXIT();
         return(POPUP_ERROR);
      }

      /* Determine number of characters of title to be displayed */
      if(pszTitle == NULL)
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

      od_set_cursor(btTop,btLeft);
      od_set_attrib(od_control.od_menu_border_col);
      od_putch(od_control.od_box_chars[BOX_UPPERLEFT]);
      if(btTitleSize == 0)
      {
         od_repeat(od_control.od_box_chars[BOX_TOP], btBetweenSize);
      }
      else
      {
         od_repeat(od_control.od_box_chars[BOX_TOP],
            btRemaining = ((btBetweenSize - btTitleSize - 2) / 2));
         od_set_attrib(od_control.od_menu_title_col);
         od_putch(' ');
         od_disp(pszTitle,btTitleSize, TRUE);
         od_putch(' ');
         od_set_attrib(od_control.od_menu_border_col);
         od_repeat(od_control.od_box_chars[BOX_TOP],
            (BYTE)(btBetweenSize - btRemaining - btTitleSize - 2));
      }
      od_putch(od_control.od_box_chars[BOX_UPPERRIGHT]);

      btLineCount = btTop + 1;
      btCorrectItem = 0;
      ODPopupCheckForKey(FALSE);
      btCursor = btCorrectItem;
      for(btCount = 0; btCount < btCurrentNumMenuItems
         && btLineCount < btBottom; ++btCount)
      {
         ODPopupCheckForKey(FALSE);
         if(nCommand != NO_COMMAND && !(wCurrentFlags & MENU_KEEP))
         {
            goto exit_now;
         }

         od_set_cursor(btLineCount,btLeft);
         od_putch(od_control.od_box_chars[BOX_LEFT]);
         od_set_attrib(od_control.od_menu_text_col);

         if(btCount == btCursor)
         {
            ODPopupDisplayMenuItem(btLeft, btTop, paMenuItems, btCount,
               TRUE, btWidth, FALSE);
         }
         else
         {
            ODPopupDisplayMenuItem(btLeft, btTop, paMenuItems, btCount,
               FALSE, btWidth, FALSE);
         }

         od_set_attrib(od_control.od_menu_border_col);
         od_putch(od_control.od_box_chars[BOX_RIGHT]);
         ++btLineCount;
      }

      od_set_cursor(btBottom, btLeft);
      od_putch(od_control.od_box_chars[BOX_LOWERLEFT]);
      od_repeat(od_control.od_box_chars[BOX_BOTTOM], btBetweenSize);
      od_putch(od_control.od_box_chars[BOX_LOWERRIGHT]);
      od_set_cursor(btTop + 1, btLeft + 1);
   }
   else
   {
      paMenuItems = MenuLevelInfo[nLevel].paMenuItems;
      btCurrentNumMenuItems = MenuLevelInfo[nLevel].btNumMenuItems;
      btWidth = MenuLevelInfo[nLevel].btWidth;
      btRight = MenuLevelInfo[nLevel].btRight;
      btBottom = MenuLevelInfo[nLevel].btBottom;
      btLeft = MenuLevelInfo[nLevel].btLeft;
      btTop = MenuLevelInfo[nLevel].btTop;
      wCurrentFlags = MenuLevelInfo[nLevel].wFlags;
      pWindow = MenuLevelInfo[nLevel].pWindow;
      btCorrectItem = btCursor = MenuLevelInfo[nLevel].btCursor;
      nCommand = NO_COMMAND;

      if(uFlags & MENU_DESTROY)
      {
         nCommand = POPUP_ESCAPE;
         goto destroy;
      }

      /* Otherwise, position flashing hardware cursor appropriately */
      od_set_cursor(btTop + btCursor + 1, btLeft + 1);
   }

   /* Claim exclusive use of arrow keys. */
   ODStatStartArrowUse();

   for(;;)
   {
      ODPopupCheckForKey(TRUE);
      if(btCorrectItem != btCursor)
      {
         ODPopupDisplayMenuItem(btLeft, btTop, paMenuItems, btCursor,
            FALSE, btWidth, TRUE);
         btCursor = btCorrectItem;
         ODWaitDrain(25);
         ODPopupCheckForKey(FALSE);
         ODPopupDisplayMenuItem(btLeft, btTop, paMenuItems, btCursor,
            TRUE, btWidth, TRUE);
      }

      if(nCommand != NO_COMMAND)
      {
         goto exit_now;
      }
   }

exit_now:
   if((!(wCurrentFlags & MENU_KEEP)) || nCommand <= 0)
   {
destroy:
      od_puttext(btLeft, btTop, btRight, btBottom, pWindow);
      free(pWindow);
      MenuLevelInfo[nLevel].pWindow = NULL;
      if(paMenuItems != NULL)
      {
         free(paMenuItems);
         MenuLevelInfo[nLevel].paMenuItems = NULL;
      }
   }
   else if(wCurrentFlags & MENU_KEEP)
   {
      MenuLevelInfo[nLevel].paMenuItems = paMenuItems;
      MenuLevelInfo[nLevel].btNumMenuItems = btCurrentNumMenuItems;
      MenuLevelInfo[nLevel].btWidth = btWidth;
      MenuLevelInfo[nLevel].btRight = btRight;
      MenuLevelInfo[nLevel].btBottom = btBottom;
      MenuLevelInfo[nLevel].btCursor = btCursor;
      MenuLevelInfo[nLevel].btLeft = btLeft;
      MenuLevelInfo[nLevel].btTop = btTop;
      MenuLevelInfo[nLevel].wFlags = wCurrentFlags;
      MenuLevelInfo[nLevel].pWindow = pWindow;
   }

   /* Restore original display color. */
   od_set_attrib(nOriginalAttrib);

   /* Release exclusive use of arrow keys. */
   ODStatEndArrowUse();

   OD_API_EXIT();
   return(nCommand);
}


/* ----------------------------------------------------------------------------
 * ODPopupCheckForKey()                                *** PRIVATE FUNCTION ***
 *
 * Checks whether or not the user has pressed any key. If one or more keys
 * have been pressed, then these keystrokes are processed. This function
 * returns when no more keys are waiting in the inbound buffer, or when a key
 * has been pressed that requires immediate action (such as the [ENTER] key).
 *
 * Parameters: bWaitForInput - Indicates whether this function should return
 *                             immediately if no input is waiting (FALSE), or
 *                             wait for the next input even before returning
 *                             (TRUE).
 *
 *     Return: void
 */
static void ODPopupCheckForKey(BOOL bWaitForInput)
{
   BYTE btCount;
   tODInputEvent InputEvent;
   BOOL bDoneAnythingYet = FALSE;

   /* Loop, processing keys. If a command has been selected, stop looping */
   /* immediately. If there are no more keys waiting, stop looping        */
   while(nCommand == NO_COMMAND)
   {
      CALL_KERNEL_IF_NEEDED();

      if(!od_get_input(&InputEvent, bWaitForInput && !bDoneAnythingYet
         ? OD_NO_TIMEOUT : 0, GETIN_NORMAL))
      {
         /* Return right away if no input event is waiting. */
         return;
      }

      bDoneAnythingYet = TRUE;

      if(InputEvent.EventType == EVENT_EXTENDED_KEY)
      {
         switch(InputEvent.chKeyPress)
         {
            case OD_KEY_UP:
up_arrow:
               if(btCorrectItem == 0)
               {
                  btCorrectItem = btCurrentNumMenuItems - 1;
               }
               else
               {
                  --btCorrectItem;
               }
               break;

            case OD_KEY_DOWN:
down_arrow:
               if(++btCorrectItem >= btCurrentNumMenuItems)
               {
                  btCorrectItem = 0;
               }
               break;

            case OD_KEY_LEFT:
left_arrow:
               if(wCurrentFlags & MENU_PULLDOWN)
               {
                  nCommand = POPUP_LEFT;
                  return;
               }
               break;

            case OD_KEY_RIGHT:
right_arrow:
               if(wCurrentFlags & MENU_PULLDOWN)
               {
                  nCommand = POPUP_RIGHT;
                  return;
               }
               break;
         }
      }

      else if(InputEvent.EventType == EVENT_CHARACTER)
      {
         if(InputEvent.chKeyPress == '\n' || InputEvent.chKeyPress == '\r')
         {
            nCommand = btCorrectItem + 1;
            return;
         }

         else if(InputEvent.chKeyPress == 27)
         {
            if(wCurrentFlags & MENU_ALLOW_CANCEL)
            {
               nCommand = POPUP_ESCAPE;
               return;
            }
         }

         else
         {
            /* Check whether key is a menu "hot key" */
            for(btCount = 0; btCount < btCurrentNumMenuItems; ++btCount)
            {
               if(toupper(MenuLevelInfo[nCurrentLevel].paMenuItems[btCount]
                  .szItemText[MenuLevelInfo[nCurrentLevel].paMenuItems[btCount]
                  .btKeyIndex]) == toupper(InputEvent.chKeyPress))
               {
                  btCorrectItem = btCount;
                  nCommand = btCorrectItem + 1;
                  return;
               }
            }

            /* At this point, we know that key was not one of the "hot keys" */
            /* Check for 4, 6, 8 and 2 keys as arrow keys.                   */
            if(InputEvent.chKeyPress == '4')
            {
               goto left_arrow;
            }
            else if(InputEvent.chKeyPress == '6')
            {
               goto right_arrow;
            }
            else if(InputEvent.chKeyPress == '8')
            {
               goto up_arrow;
            }
            else if(InputEvent.chKeyPress == '2')
            {
               goto down_arrow;
            }
         }
      }
   }
}


/* ----------------------------------------------------------------------------
 * ODPopupDisplayMenuItem()                            *** PRIVATE FUNCTION ***
 *
 * Displays an individual menu item.
 *
 * Parameters: btLeft          - Column number where the menu item will be
 *                               displayed.
 *
 *             btTop           - Row number where the menu item will be
 *                               displayed.
 *
 *             paMenuItems     - Pointer to array of available menu items.
 *
 *             btItemIndex     - Index into paMenuItems of the menu item that
 *                               is to be displayed.
 *
 *             bHighlighted    - TRUE if the items is to be displayed as
 *                               highlighted, FALSE if it is to be displayed as
 *                               non-highlighted.
 *
 *             btWidth         - Width of the menu item, in characters.
 *
 *             bPositionCursor - TRUE if the cursor needs to be positioned
 *                               prior to drawing the menu item, FALSE if the
 *                               cursor is already in the required position.
 *
 *     Return: void
 */
static void ODPopupDisplayMenuItem(BYTE btLeft, BYTE btTop,
   tMenuItem *paMenuItems, BYTE btItemIndex, BOOL bHighlighted, BYTE btWidth,
   BOOL bPositionCursor)
{
   BYTE btCount;
   char *pchItemText;
   BYTE btKeyPosition;
   BYTE btTextColor;
   BYTE btKeyColor;

   /* Check that parameters are reasonable when operating in debug mode. */
   ASSERT(paMenuItems != NULL);
   ASSERT(btItemIndex < MAX_MENU_ITEMS);
   ASSERT(btWidth < OD_SCREEN_WIDTH);

   ++btLeft;
   ++btTop;

   btTextColor = bHighlighted ? od_control.od_menu_highlight_col
      : od_control.od_menu_text_col;
   btKeyColor = bHighlighted ? od_control.od_menu_highkey_col
      : od_control.od_menu_key_col;

   pchItemText = (char *)(paMenuItems[btItemIndex].szItemText);
   btKeyPosition = paMenuItems[btItemIndex].btKeyIndex;

   if(bPositionCursor) od_set_cursor(btTop + btItemIndex, btLeft);

   od_set_attrib(btTextColor);
   od_putch(' ');

   for(btCount = 0; btCount < btWidth && *pchItemText; ++btCount)
   {
       if(btCount == btKeyPosition)
       {
          od_set_attrib(btKeyColor);
          od_putch(*pchItemText++);
          od_set_attrib(btTextColor);
       }
       else
       {
          od_putch(*pchItemText++);
       }
   }

   od_repeat(' ', (BYTE)((btWidth - btCount) + 1));

   if(bPositionCursor) od_set_cursor(btTop + btItemIndex, btLeft);
}
