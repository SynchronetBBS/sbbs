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
 *        File: ODScrn.c
 *
 * Description: Functions used to access the local display screen buffer, which
 *              keeps a copy of the text that is displayed on the remote
 *              terminal. The local display screen buffer also displays the
 *              OpenDoors status lines on some platforms. In addition to
 *              maintaining the current screen buffer, the odscrn.c module
 *              also contains the code to display this buffer on the screen.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Dec 31, 1994  6.00  BP   Remove #ifndef USEINLINE DOS code.
 *              Dec 31, 1994  6.00  BP   Use new multitasker variable.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   32-bit portability.
 *              Nov 14, 1995  6.00  BP   Created odscrn.h.
 *              Nov 14, 1995  6.00  BP   Make screen size configurable.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Nov 21, 1995  6.00  BP   Port to Win32.
 *              Jan 20, 1996  6.00  BP   Prompt for user name if force_local.
 *              Jan 21, 1996  6.00  BP   Added ODScrnShowMessage() and related.
 *              Jan 27, 1996  6.00  BP   Expand tab ('\t') characters.
 *              Jan 27, 1996  6.00  BP   Added ODScrollUpAndInvalidate().
 *              Jan 27, 1996  6.00  BP   Made text-mode window f'ns static.
 *              Jan 31, 1996  6.00  BP   Made them non-static again.
 *              Jan 31, 1996  6.00  BP   Added ODScrnLocalInput().
 *              Feb 06, 1996  6.00  BP   Added od_silent_mode.
 *              Feb 16, 1996  6.00  BP   Make caret visible after local login.
 *              Feb 17, 1996  6.00  BP   Recognize non-ASCII keys under Win32.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 21, 1996  6.00  BP   Forward SC_KEYMENU to frame thread.
 *              Feb 21, 1996  6.00  BP   Don't beep in "silent mode".
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 13, 1996  6.10  BP   Added od_local_win_col.
 *              Mar 17, 1996  6.10  BP   Terminate string in ODScrnLocalInput()
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODPlat.h"
#include "ODScrn.h"
#include "ODUtil.h"
#include "ODFrame.h"
#include "ODInEx.h"
#ifdef ODPLAT_WIN32
#include "ODKrnl.h"
#include "ODRes.h"
#endif /* ODPLAT_WIN32 */


/* ========================================================================= */
/* Definitions of variables used by the local screen module.                 */
/* ========================================================================= */

/* Manifest constants used in this module. */
#define SCREEN_BUFFER_SIZE          (OD_SCREEN_WIDTH * OD_SCREEN_HEIGHT * 2)
#define SCREEN_BUFFER_SEGMENT_SIZE  (SCREEN_BUFFER_SIZE / 16)
#define BYTES_PER_CHAR              2
#define BUFFER_LINE_BYTES           (OD_SCREEN_WIDTH * BYTES_PER_CHAR)
#define LINE_BUFFER_SIZE            (OD_SCREEN_WIDTH + 1)


/* Private variables used by the screen I/O functions. */

#if defined(ODPLAT_DOS) || defined(ODPLAT_NIX)
static void *pAllocatedBufferMemory;
#endif /* ODPLAT_DOS */

/* Far pointer to video buffer. */
static void ODFAR *pScrnBuffer;

/* Current cursor position. */
static BYTE btCursorColumn;
static BYTE btCursorRow;

/* Current output boundaries. */
static BYTE btLeftBoundary;
static BYTE btTopBoundary;
static BYTE btRightBoundary;
static BYTE btBottomBoundary;

/* Current display color. */
static BYTE btCurrentAttribute;

/* Is scrolling enabled. */
static BOOL bScrollEnabled;

#ifdef ODPLAT_DOS
/* Segment address of video buffer. */
static WORD wBufferSegment;
/* Display page to use. */
static BYTE btDisplayPage;
#endif

/* Is cursor currently on. */
static BYTE bCaretOn;

/* Static temporary working buffer. */
static char szBuffer[LINE_BUFFER_SIZE];


/* Private function prototypes. */
static void ODScrnGetCursorPos(void);
static void ODScrnUpdateCaretPos(void);
static void ODScrnScrollUpOneLine(void);
static void ODScrnScrollUpAndInvalidate(void);


/* ========================================================================= */
/* Implementation of the local screen window for the Win32 platform.         */
/* ========================================================================= */

#ifdef ODPLAT_WIN32

/* Screen thread startup information. */
typedef struct
{
   HWND hwndFrame;
   HANDLE hInstance;
} tODScrnThreadInfo;

/* Handle to the screen window. */
static HWND hwndScreenWindow;

/* Does the screen window currently have input focus? */
BOOL bScreenHasFocus;

/* Current font-related information. */
static HFONT hCurrentFont;
static INT nFontCellWidth;
static INT nFontCellHeight;

/* Table to translate from PC text color values used in the screen buffer */
/* to their corresponding RGB values.                                     */
COLORREF acrPCTextColors[] =
{
   RGB(0x00, 0x00, 0x00),
   RGB(0x00, 0x00, 0xc0),
   RGB(0x00, 0xc0, 0x00),
   RGB(0x00, 0xc0, 0xc0),
   RGB(0xc0, 0x00, 0x00),
   RGB(0xc0, 0x00, 0xc0),
   RGB(0xc0, 0xc0, 0x00),
   RGB(0xc0, 0xc0, 0xc0),
   RGB(0x7f, 0x7f, 0x7f),
   RGB(0x00, 0x00, 0xff),
   RGB(0x00, 0xff, 0x00),
   RGB(0x00, 0xff, 0xff),
   RGB(0xff, 0x00, 0x00),
   RGB(0xff, 0x00, 0xff),
   RGB(0xff, 0xff, 0x00),
   RGB(0xff, 0xff, 0xff),
};

/* Table to translate from Windows key codes to OpenDoors key codes. */
typedef struct
{
   int nVirtKey;
   BYTE btODKey;
} tWinKeyToODKey;

tWinKeyToODKey aWinKeyToODKey[] =
{
   {VK_UP,        OD_KEY_UP},
   {VK_DOWN,      OD_KEY_DOWN},
   {VK_LEFT,      OD_KEY_LEFT},
   {VK_RIGHT,     OD_KEY_RIGHT},
   {VK_INSERT,    OD_KEY_INSERT},
   {VK_DELETE,    OD_KEY_DELETE},
   {VK_END,       OD_KEY_END},
   {VK_HOME,      OD_KEY_HOME},
   {VK_PRIOR,     OD_KEY_PGUP},
   {VK_NEXT,      OD_KEY_PGDN},
   {VK_F1,        OD_KEY_F1},
   {VK_F2,        OD_KEY_F2},
   {VK_F3,        OD_KEY_F3},
   {VK_F4,        OD_KEY_F4},
   {VK_F5,        OD_KEY_F5},
   {VK_F6,        OD_KEY_F6},
   {VK_F7,        OD_KEY_F7},
   {VK_F8,        OD_KEY_F8},
   {VK_F9,        OD_KEY_F9},
   {VK_F10,       OD_KEY_F10},
};

/* Utility macros. */
#define COLUMN_AS_XPIXEL(nColumn)   (((INT)(nColumn)) * nFontCellWidth)
#define ROW_AS_YPIXEL(nRow)         (((INT)(nRow)) * nFontCellHeight)
#define XPIXEL_AS_COLUMN(nX)        (((INT)(nX)) / nFontCellWidth)
#define YPIXEL_AS_ROW(nY)           (((INT)(nY)) / nFontCellHeight)

/* User defined messages. */
#define WM_MOVE_YOUR_CARET          (WM_USER + 1)
#define WM_KEYDOWN_RELAY            (WM_USER + 2)

/* Height of the flashing caret, in pixels. */
#define CARET_HEIGHT   3

/* Local function prototypes. */
LRESULT CALLBACK ODScrnWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
   LPARAM lParam);
static HWND ODScrnCreateWin(HWND hwndFrame, HANDLE hInstance);
static void ODScrnMessageLoop(HANDLE hInstance, HWND hwndScreen);
DWORD OD_THREAD_FUNC ODScrnThreadProc(void *pParam);
static void ODScrnPaint(HDC hdc, INT nLeft, INT nTop, INT nRight, INT nBottom);
static void ODScrnInvalidate(BYTE btLeft, BYTE btTop, BYTE btRight,
   BYTE btBottom);
static void ODScrnSetCurrentFont(HWND hwndScreen, HFONT hNewFont);
static void ODScrnSetWinCaretPos(void);


/* ----------------------------------------------------------------------------
 * ODScrnCreateWin()                                   *** PRIVATE FUNCTION ***
 *
 * Creates the local screen window, which covers the client area of the
 * OpenDoors frame window.
 *
 * Parameters: hwndFrame   - Handle to the frame window.
 *
 *             hInstance   - Handle to application instance.
 *
 *     Return: A handle to the newly created window, or NULL on failure.
 */
static HWND ODScrnCreateWin(HWND hwndFrame, HANDLE hInstance)
{
   HWND hwndScreen = NULL;
   WNDCLASS wcScreenWindow;

   ASSERT(hwndFrame != NULL);
   ASSERT(hInstance != NULL);

   /* Register the screen window's window class. */
   memset(&wcScreenWindow, 0, sizeof(wcScreenWindow));
   wcScreenWindow.style = CS_HREDRAW | CS_VREDRAW;
   wcScreenWindow.lpfnWndProc = ODScrnWindowProc;
   wcScreenWindow.cbClsExtra = 0;
   wcScreenWindow.cbWndExtra = 0;
   wcScreenWindow.hInstance = hInstance;
   wcScreenWindow.hIcon = NULL;
   wcScreenWindow.hCursor = LoadCursor(NULL, IDC_ARROW);
   wcScreenWindow.hbrBackground = NULL;
   wcScreenWindow.lpszMenuName = NULL;
   wcScreenWindow.lpszClassName = "ODScreen";

   RegisterClass(&wcScreenWindow);

   /* Create the screen window. */
   if((hwndScreen = CreateWindowEx(
      WS_EX_CLIENTEDGE,
      wcScreenWindow.lpszClassName,
      "",
      WS_CHILD | WS_BORDER,
      0,
      0,
      500,
      300,
      hwndFrame,
      NULL,
      hInstance,
      (LPVOID)hInstance)) == NULL)
   {
      /* On window creation failure, return NULL. */
      return(NULL);
   }

   /* Store handle to screen window for access from screen. */
   hwndScreenWindow = hwndScreen;

   return(hwndScreen);
}


/* ----------------------------------------------------------------------------
 * ODScrnWindowProc()                                  *** PRIVATE FUNCTION ***
 *
 * The local screen window proceedure.
 *
 * Parameters: hwnd   - Handle to the local screen window.
 *
 *             uMsg   - Specifies the message.
 *
 *             wParam - Specifies additional message information. The content
 *                      of this parameter depends on the value of the uMsg
 *                      parameter.
 *
 *             lParam - Specifies additional message information. The content
 *                      of this parameter depends on the value of the uMsg
 *                      parameter.
 *
 *     Return: The return value is the result of the message processing and
 *             depends on the message.
 */
LRESULT CALLBACK ODScrnWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
   LPARAM lParam)
{
   HINSTANCE hInstance;

   ASSERT(hwnd != NULL);

   hInstance = (HINSTANCE)GetWindowLong(hwnd, GWL_USERDATA);

   switch(uMsg)
   {
      case WM_SYSCOMMAND:
         /* We move any SC_KEYMENU WM_SYSCOMMAND messages to the frame */
         /* window's message queue so that the screen window thread    */
         /* can continue to process messages when the menu is          */
         /* activated from the keyboard when the screen window has     */
         /* the keyboard focus. If this isn't done, the menu will not  */
         /* behave correctly when activated this way.                  */
         if(wParam == SC_KEYMENU)
         {
            PostMessage(GetParent(hwnd), uMsg, wParam, lParam);
         }
         else
         {
            return(DefWindowProc(hwnd, uMsg, wParam, lParam));
         }
         break;

      case WM_CREATE:
      {
         CREATESTRUCT *pCreateStruct = (CREATESTRUCT *)lParam;
         hInstance = (HINSTANCE)pCreateStruct->lpCreateParams;
         SetWindowLong(hwnd, GWL_USERDATA, (LONG)hInstance);
         break;
      }

      case WM_PAINT:
      {
         PAINTSTRUCT PaintStruct;
         HDC hdc;

         /* Obtain device context and paint information. */
         hdc = BeginPaint(hwnd, &PaintStruct);

         if(hdc != NULL)
         {
            /* Redraw the portion of the window that has been invalidated. */
            ODScrnPaint(hdc,
               XPIXEL_AS_COLUMN(PaintStruct.rcPaint.left),
               YPIXEL_AS_ROW(PaintStruct.rcPaint.top),
               XPIXEL_AS_COLUMN(PaintStruct.rcPaint.right),
               YPIXEL_AS_ROW(PaintStruct.rcPaint.bottom));

            /* Release device context. */
            EndPaint(hwnd, &PaintStruct);
         }
         break;
      }

      case WM_MOVE_YOUR_CARET:
         ODScrnSetWinCaretPos();
         break;

      case WM_LBUTTONDOWN:
         SetFocus(hwnd);
         break;

      case WM_SETFOCUS:
         /* Turn on the caret when we receive the input focus. */
         /* First, create the caret. */
         CreateCaret(hwnd, NULL, nFontCellWidth, CARET_HEIGHT);

         /* Remember that we now have the input focus. */
         bScreenHasFocus = TRUE;

         /* Update the position of the caret. */
         ODScrnSetWinCaretPos();

         /* Now, make the caret visible. */
         ShowCaret(hwnd);
         break;

      case WM_KILLFOCUS:
         /* Remember that we no longer have the input focus. */
         bScreenHasFocus = FALSE;

         /* Turn off the caret when we loose the input focus. */
         DestroyCaret();
         break;

      case WM_KEYDOWN_RELAY:
      {
         int nVirtKeyPressed = (int)wParam;
         WORD wRepeatCount = LOWORD(lParam);
         int nKeyTableIndex;
         WORD wKey = 0;

         /* Look for a matching key in the OpenDoors key table. */
         for(nKeyTableIndex = 0; nKeyTableIndex < DIM(aWinKeyToODKey);
            ++nKeyTableIndex)
         {
            if(aWinKeyToODKey[nKeyTableIndex].nVirtKey == nVirtKeyPressed)
            {
               wKey = MAKEWORD(0, aWinKeyToODKey[nKeyTableIndex].btODKey);
               break;
            }
         }

         /* If a matching key was found, then add it to the queue. */
         if(wKey != 0)
         {
            while(wRepeatCount--)
            {
               ODKrnlHandleLocalKey(wKey);
            }
         }

         break;
      }

      case WM_CHAR:
      {
         WORD wRepeatCount = LOWORD(lParam);
         BYTE btScanCode = LOBYTE(HIWORD(lParam));
         TCHAR chCharCode = (TCHAR)wParam;
         WORD wKey;
         
         wKey = MAKEWORD(chCharCode, btScanCode);

         /* Loop for each repitition of this key. */
         while(wRepeatCount--)
         {
            ODKrnlHandleLocalKey(wKey);
         }
         break;
      }

      default:
         /* Pass messages that we don't explicitly handle on to the */
         /* default window proc.                                    */
         return(DefWindowProc(hwnd, uMsg, wParam, lParam));
   }

   return(0);
}


/* ----------------------------------------------------------------------------
 * ODScrnPaint()                                       *** PRIVATE FUNCTION ***
 *
 * Draws the specified portion of the screen on the provided device context.
 *
 * Parameters: hdc     - Handle to the device context to draw on.
 *
 *             nLeft   - Left column to draw.
 *
 *             nTop    - Top row to draw.
 *
 *             nRight  - Right column to draw.
 *
 *             nBottom - Bottom row to draw.
 *
 *     Return: void.
 */
static void ODScrnPaint(HDC hdc, INT nLeft, INT nTop, INT nRight, INT nBottom)
{
   INT nIDSavedState;
   INT nCurrentLine;
   INT nStartColumn;
   INT nEndColumn;
   BYTE *pbtBufferContents;
   char achStringToOutput[OD_SCREEN_WIDTH];
   char *pchNextChar;
   BYTE btCurrentAttribute;

   ASSERT(hdc != NULL);
   ASSERT(nLeft >= 0);
   ASSERT(nTop >= 0);
   ASSERT(nRight >= nLeft);
   ASSERT(nBottom >= nTop);

   /* Ensure that parameters are within valid range. */
   if(nRight >= OD_SCREEN_WIDTH) nRight = OD_SCREEN_WIDTH - 1;
   if(nBottom >= OD_SCREEN_HEIGHT) nBottom = OD_SCREEN_HEIGHT - 1;

   /* Save the current state of the device context so that we can restore */
   /* it before returning.                                                */
   nIDSavedState = SaveDC(hdc);

   /* Setup device context for displaying text from the screen buffer. */
   SetBkMode(hdc, OPAQUE);
   SelectObject(hdc, hCurrentFont);

   /* Loop through each line that is to be painted. */
   for(nCurrentLine = nTop; nCurrentLine <= nBottom; ++nCurrentLine)
   {
      /* Obtain a pointer to the first byte representing this line in */
      /* the screen buffer.                                           */
      pbtBufferContents = (BYTE *)(pScrnBuffer) +
         ((nCurrentLine * OD_SCREEN_WIDTH) + nLeft) * 2;

      /* Loop for each portion of this line that can be drawn in a single */
      /* TextOut() call.                                                  */
      for(nStartColumn = nLeft; nStartColumn <= nRight;
         nStartColumn = nEndColumn)
      {
         /* Begin constructing a string containing the text to output */
         /* in this call to TextOut().                                */
         pchNextChar = achStringToOutput;

         /* Determine the color of this portion. */
         btCurrentAttribute = pbtBufferContents[1];

         /* Loop, finding the first column that has an incompatible color. */
         for(nEndColumn = nStartColumn; nEndColumn <= nRight; ++nEndColumn)
         {
            /* Stop looping if we come to a non-equivalent color */
            /* attribute.                                        */
            if(btCurrentAttribute != pbtBufferContents[1])
            {
               break;
            }

            /* Otherwise, add this character to the string to output. */
            *pchNextChar++ = *pbtBufferContents;

            /* Move to the next position in the buffer. */
            pbtBufferContents += 2;
         }

         /* Change current display colors to match the current color */
         /* attribute.                                               */
         SetTextColor(hdc, acrPCTextColors[btCurrentAttribute & 0x0f]);
         SetBkColor(hdc, acrPCTextColors[(btCurrentAttribute & 0xf0) >> 4]);

         /* Output the string. */
         TextOut(hdc,
            COLUMN_AS_XPIXEL(nStartColumn),
            ROW_AS_YPIXEL(nCurrentLine),
            achStringToOutput,
            (nEndColumn - nStartColumn));
      }
   }

   /* Restore the device context to its original state before this function */
   /* was called.                                                           */
   RestoreDC(hdc, nIDSavedState);
}


/* ----------------------------------------------------------------------------
 * ODScrnInvalidate()                                  *** PRIVATE FUNCTION ***
 *
 * Marks the specified area of the screen window as invalid, forcing the
 * screen thread to redraw it.
 *
 * Parameters: btLeft   - The left most column to invalidate.
 *
 *             btTop    - The top most row to invalidate.
 *
 *             btRight  - The right most column to invalidate.
 *
 *             btBottom - The bottom most row to invalidate.
 *
 *     Return: void.
 */
static void ODScrnInvalidate(BYTE btLeft, BYTE btTop, BYTE btRight,
   BYTE btBottom)
{
   RECT rcToInvalidate;

   /* If the screen window has not been created yet, then return without */
   /* doing anything.                                                    */
   if(hwndScreenWindow == NULL) return;

   /* Obtain rectangle in client window coordinates, to be invalidated. */
   rcToInvalidate.left = COLUMN_AS_XPIXEL(btLeft);
   rcToInvalidate.top = ROW_AS_YPIXEL(btTop);
   rcToInvalidate.right = COLUMN_AS_XPIXEL(btRight + 1);
   rcToInvalidate.bottom = ROW_AS_YPIXEL(btBottom + 1);

   /* Mark this rectangle as invalid. */
   InvalidateRect(hwndScreenWindow, &rcToInvalidate, FALSE);
}


/* ----------------------------------------------------------------------------
 * ODScrnSetCurrentFont()                              *** PRIVATE FUNCTION ***
 *
 * Changes the current font to be used for drawing, updating anything that
 * needs updating.
 *
 * Parameters: hwndScreen - Handle to the screen window.
 *
 *             nNewFont   - Handle to the font to switch to.
 *
 *     Return: void.
 */
static void ODScrnSetCurrentFont(HWND hwndScreen, HFONT hNewFont)
{
   HDC hdc;
   INT nIDSavedState;
   TEXTMETRIC TextMetrics;

   /* Obtain a handle to the a device context for the screen window. */
   hdc = GetDC(hwndScreen);

   /* If we are unable to obtian a device context, then return without */
   /* doing anything.                                                  */
   if(hdc == NULL)
   {
      return;
   }

   /* Change the current font. */
   hCurrentFont = hNewFont;

   /* Obtain text metrics from the device context, and then release the */
   /* device context.                                                   */
   nIDSavedState = SaveDC(hdc);
   SelectObject(hdc, hCurrentFont);
   GetTextMetrics(hdc, &TextMetrics);
   RestoreDC(hdc, nIDSavedState);
   ReleaseDC(hwndScreen, hdc);

   /* Determine the new size of a character cell. */
   nFontCellWidth = TextMetrics.tmMaxCharWidth;
   nFontCellHeight = TextMetrics.tmHeight;

   /* Force window sizes to be adjusted for the new font size. */
   ODScrnAdjustWindows();
   ODScrnAdjustWindows();
}


/* ----------------------------------------------------------------------------
 * ODScrnAdjustWindows()
 *
 * Resizes and repositions the screen window to the appropriate size based
 * on the current font, portions of the frame window's client area that are
 * in use, etc. Other windows whose size depends on the size of the screen
 * window are also updated.
 *
 * Parameters: None.
 *
 *     Return: void.
 */
void ODScrnAdjustWindows(void)
{
   INT nNewClientWidth;
   INT nNewClientHeight;
   RECT rcClient;
   RECT rcWindow;
   INT nNonClientWidth;
   INT nNonClientHeight;
   INT nScreenWindowWidth;
   INT nScreenWindowHeight;
   HWND hwndFrame;
   INT nTopFrameUsed;
   INT nBottomFrameUsed;
   HWND hwndScreen;

   hwndScreen = hwndScreenWindow;
   ASSERT(hwndScreen != NULL);

   hwndFrame = GetParent(hwndScreen);
   ASSERT(hwndFrame != NULL);

   /* Determine areas of the frame window's client area that are already */
   /* in use.                                                            */
   nTopFrameUsed = ODFrameGetUsedClientAtTop(hwndFrame);
   nBottomFrameUsed = ODFrameGetUsedClientAtBottom(hwndFrame);

   /* Determine the new required size of the window's client area. */
   nNewClientWidth = nFontCellWidth * OD_SCREEN_WIDTH;
   nNewClientHeight = nFontCellHeight * OD_SCREEN_HEIGHT;

   /* Determine the size of the window's non-client area. */
   GetClientRect(hwndScreen, &rcClient);
   GetWindowRect(hwndScreen, &rcWindow);
   nNonClientWidth = (rcWindow.right - rcWindow.left)
      - (rcClient.right - rcClient.left);
   nNonClientHeight = (rcWindow.bottom - rcWindow.top)
      - (rcClient.bottom - rcClient.top);

   /* Determine the overall size required for the screen window. */
   nScreenWindowWidth = nNewClientWidth + nNonClientWidth;
   nScreenWindowHeight = nNewClientHeight + nNonClientHeight;

   /* Resize the screen window accordingly. */
   SetWindowPos(hwndScreen, NULL, 0, nTopFrameUsed, nScreenWindowWidth,
      nScreenWindowHeight, SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOZORDER);

   /* Resize the OpenDoors frame window (which is the screen window's  */
   /* parent) so that the screen window just fill's the frame window's */
   /* remaining client area.                                           */
   GetClientRect(hwndFrame, &rcClient);
   GetWindowRect(hwndFrame, &rcWindow);
   nNonClientWidth = (rcWindow.right - rcWindow.left)
      - (rcClient.right - rcClient.left);
   nNonClientHeight = (rcWindow.bottom - rcWindow.top)
      - (rcClient.bottom - rcClient.top);

   SetWindowPos(hwndFrame, NULL, 0, 0, nScreenWindowWidth + nNonClientWidth,
      nScreenWindowHeight + nNonClientHeight + nTopFrameUsed
      + nBottomFrameUsed,
      SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER);
}


/* ----------------------------------------------------------------------------
 * ODScrnMessageLoop()                                 *** PRIVATE FUNCTION ***
 *
 * Message loop for OpenDoors screen window thread.
 *
 * Parameters: hInstance   - Handle to current instance.
 *
 *             hwndScreen  - Handle to the screen window.
 *
 *     Return: void.
 */
static void ODScrnMessageLoop(HANDLE hInstance, HWND hwndScreen)
{
   MSG msg;
   HWND hwndFrame;

   ASSERT(hInstance != NULL);
   ASSERT(hwndScreen != NULL);

   /* Obtain a handle to the OpenDoors main frame window. */
   hwndFrame = GetParent(hwndScreen);

   /* Loop, fetching, translating and dispatching messages for any windows */
   /* created by this thread. (GetMessage() blocks when no messages are    */
   /* available.)                                                          */
   while(GetMessage(&msg, NULL, 0, 0))
   {
      if(!ODFrameTranslateAccelerator(hwndFrame, &msg))
      {
         TranslateMessage(&msg);
         if(msg.message == WM_KEYDOWN)
         {
            PostMessage(hwndScreen, WM_KEYDOWN_RELAY, msg.wParam, msg.lParam);
         }
         DispatchMessage(&msg);
      }
   }
}


/* ----------------------------------------------------------------------------
 * ODScrnThreadProc()                                  *** PRIVATE FUNCTION ***
 *
 * Function that execute the OpenDoors screen window thread. This thread's
 * primary task is to draw the screen window contents, when needed.
 *
 * Parameters: pParam   - The thread parameter, which is a pointer to a
 *                        tODScrnThreadInfo structure.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
DWORD OD_THREAD_FUNC ODScrnThreadProc(void *pParam)
{
   tODScrnThreadInfo *pScrnThreadInfo = (tODScrnThreadInfo *)pParam;
   HWND hwndScreen;
   HANDLE hInstance = pScrnThreadInfo->hInstance;
   HWND hwndFrame = pScrnThreadInfo->hwndFrame;

   /* We are now done with the thread startup information structure, */
   /* so deallocate it.                                              */
   free(pScrnThreadInfo);

   /* Create the screen window. */
   hwndScreen = ODScrnCreateWin(hwndFrame, hInstance);

   if(hwndScreen == NULL)
   {
      return(FALSE);
   }

   /* Set the current font for the window. This, in turn will force the  */
   /* window to be adjusted to the appropriate size, and will adjust the */
   /* size of the OpenDoors frame window accordingly.                    */
   ODScrnSetCurrentFont(hwndScreen, GetStockObject(OEM_FIXED_FONT));

   /* Prompt for the user's name before showing the windows, if required. */   
#ifdef ODPLAT_WIN32
   if(bPromptForUserName)
   {
      if(DialogBox(hInstance, MAKEINTRESOURCE(IDD_LOGIN), hwndFrame,
         ODInitLoginDlgProc) == IDCANCEL)
      {
         exit(od_control.od_errorlevel[1]);
      }

      PostMessage(hwndScreen, WM_SETFOCUS, 0, 0L);
   }
#endif /* ODPLAT_WIN32 */

   /* Now, we can make the frame window visible. */
   if(od_control.od_cmd_show == SW_MINIMIZE ||
      od_control.od_cmd_show == SW_SHOWMINIMIZED ||
      od_control.od_cmd_show == SW_SHOWMINNOACTIVE)
   {
      ShowWindow(hwndFrame, SW_SHOWMINNOACTIVE);
   }
   else
   {
      ShowWindow(hwndFrame, SW_RESTORE);
   }

   /* Now, show the screen window. */
   ShowWindow(hwndScreen, SW_SHOW);

   /* Loop, processing messages for the screen window. */
   ODScrnMessageLoop(hInstance, hwndScreen);

   /* Destroy the screen window. */
   DestroyWindow(hwndScreen);

   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODScrnStartWindow()
 *
 * Function that starts up the screen window thread, which in turn creates
 * and manages the screen window.
 *
 * Parameters: hInstance      - Handle to the current application instance.
 *
 *             phScreenThread - Pointer to location where screen thread handle
 *                              should be stored.
 *
 *             hwndFrame      - Handle to already created frame window.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODScrnStartWindow(HANDLE hInstance, tODThreadHandle *phScreenThread,
   HWND hwndFrame)
{
   tODScrnThreadInfo *pScrnThreadInfo;

   ASSERT(hInstance != NULL);
   ASSERT(phScreenThread != NULL);
   ASSERT(hwndFrame != NULL);

   /* Setup thread information to pass into the screen thread at startup. */
   if((pScrnThreadInfo = malloc(sizeof(tODScrnThreadInfo))) == NULL)
   {
      return(kODRCNoMemory);
   }

   pScrnThreadInfo->hInstance = hInstance;
   pScrnThreadInfo->hwndFrame = hwndFrame;

   /* Create the screen thread. */
   return(ODThreadCreate(phScreenThread, ODScrnThreadProc,
      pScrnThreadInfo));
}


/* ----------------------------------------------------------------------------
 * ODScrnSetFocusToWindow()
 *
 * Sets the current input focus to the screen window.
 *
 * Parameters: none
 *
 *     Return: void
 */
void ODScrnSetFocusToWindow(void)
{
   if(hwndScreenWindow != NULL)
   {
      SetFocus(hwndScreenWindow);
   }
}


/* ----------------------------------------------------------------------------
 * ODScrnSetWinCaretPos()
 *
 * Repositions the Windows caret to the position of our cursor, if
 * appropriate.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODScrnSetWinCaretPos(void)
{
   /* Only move the caret if we have focus, and thus we are the one who */
   /* owns the caret.                                                   */
   if(bScreenHasFocus)
   {
      SetCaretPos(COLUMN_AS_XPIXEL(btCursorColumn + btLeftBoundary),
         ROW_AS_YPIXEL(btCursorRow + btTopBoundary + 1) - CARET_HEIGHT);
   }
}


#endif /* ODPLAT_WIN32 */



/* ========================================================================= */
/* Functions used throughout OpenDoors to manipulate local screen buffer.    */
/* ========================================================================= */

/* ----------------------------------------------------------------------------
 * ODScrnInitialize()
 *
 * Initializes the local screen module.
 *
 * Parameters: none
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODScrnInitialize(void)
{
   BOOL bClear = TRUE;

#if defined(ODPLAT_DOS) || defined(ODPLAT_NIX)
   /* In silent mode, we perform all output in a block of memory that is */
   /* never displayed.                                                   */
   /* *nix is always in "silent mode"                                    */
#ifndef ODPLAT_NIX
   if(od_control.od_silent_mode)
   {
#endif
      /* Allocate memory for screen buffer, using standard pointer type */
      /* for current memory model.                                      */
      pAllocatedBufferMemory = malloc(SCREEN_BUFFER_SIZE);

      if(pAllocatedBufferMemory == NULL)
      {
         return(kODRCNoMemory);
      }

      /* Set the screen buffer far pointer to point to the allocated */
      /* buffer.                                                     */
      pScrnBuffer = pAllocatedBufferMemory;
#ifndef ODPLAT_NIX
   }
   else
   {
      BYTE btDisplayMode;

      /* Get current video mode. */
      ASM    push si
      ASM    push di
      ASM    mov ah, 0x0f
      ASM    int 0x10
      ASM    mov btDisplayMode, al
      ASM    pop di
      ASM    pop si

      switch(btDisplayMode & 0x7f)
      {
         /* No need to change mode, already colour 80x25. */
         case 0x02:
         case 0x03:
            wBufferSegment = 0xb800;
            pScrnBuffer = (void ODFAR *)0xb8000000L;
            bClear = TRUE;
            break;

         /* No need to change mode, already monochrome 80x25. */
         case 0x07:
            wBufferSegment = 0xb000;
            pScrnBuffer = (void ODFAR *)0xb0000000L;
            bClear = TRUE;
            break;

         /* Must change mode to monochrome 80x25. */
         case 0x21:
            wBufferSegment = 0xb000;
            pScrnBuffer = (void ODFAR *)0xb0000000L;
            bClear = FALSE;

            /* set mode to 0x07 */
            ASM    push si
            ASM    push di
            ASM    mov ax, 0x0007
            ASM    int 0x10
            ASM    pop di
            ASM    pop si
            break;

         /* Must change mode to colour 80x25. */
         default:
            wBufferSegment = 0xb800;
            pScrnBuffer = (void ODFAR *)0xb8000000L;
            bClear = FALSE;

            /* set mode to 0x03. */
            ASM    push si
            ASM    push di
            ASM    mov ax, 0x0003
            ASM    int 0x10
            ASM    pop di
            ASM    pop si
      }



      /* Adjust address for display page which is being used. */
      ASM    push si
      ASM    push di
      ASM    mov ah, 0x0f
      ASM    int 0x10
      ASM    mov btDisplayPage, bh
      ASM    pop di
      ASM    pop si

      if(btDisplayPage!=0)
      {
         wBufferSegment += (SCREEN_BUFFER_SEGMENT_SIZE * btDisplayPage);
         ((char ODFAR *)pScrnBuffer) += (SCREEN_BUFFER_SIZE * btDisplayPage);
      }

      if(ODMultitasker == kMultitaskerDV)
      {
         /* Determine address of DV screen buffer. */
         /* This doesn't check rows, bh = rows, bl = columns. */
         ASM    mov ax, 0x2b02
         ASM    mov cx, 0x4445
         ASM    mov dx, 0x5351
         ASM    int 0x21
         ASM    cmp bx, 0x1950
         ASM    jne no_change
         ASM    mov wBufferSegment, dx

         (long)pScrnBuffer = ODDWordShiftLeft((long)wBufferSegment, 16);
   no_change: ;
      }
   }
#endif /* ODPLAT_DOS */
#endif /* ODPLAT_DOS/NIX */

#ifdef ODPLAT_WIN32
   /* Allocate memory for screen buffer. */
   pScrnBuffer = malloc(SCREEN_BUFFER_SIZE);

   if(pScrnBuffer == NULL)
   {
      return(kODRCNoMemory);
   }
#endif /* ODPLAT_WIN32 */

   /* Initialize display system variables. */
   btLeftBoundary = 0;
   btRightBoundary = 79;
   btTopBoundary = 0;
   btBottomBoundary = 24;
   btCurrentAttribute = 0x07;
   bScrollEnabled = 1;

   /* Clear local screen. */
   if(bClear)
   {
      ODScrnClear();
   }

   /* Enable flashing cursor. */
   bCaretOn = FALSE;
   ODScrnEnableCaret(TRUE);

   /* Return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODScrnShutdown()
 *
 * De-initializes the screen module.
 *
 * Parameters: none
 *
 *     Return: void
 */
void ODScrnShutdown(void)
{
#ifdef ODPLAT_WIN32
   /* Deallocate screen buffer memory. */
   if(pScrnBuffer != NULL)
   {
      free(pScrnBuffer);
      pScrnBuffer = NULL;
   }
#else /* !ODPLAT_WIN32 */
   /* In silent mode, we must deallocate screen buffer memory. */
   /* *nix is always in silent mode                            */
#ifndef ODPLAT_NIX
   if(od_control.od_silent_mode && pAllocatedBufferMemory != NULL)
   {
#endif
      free(pAllocatedBufferMemory);
      pAllocatedBufferMemory = NULL;
      pScrnBuffer = NULL;
#ifndef ODPLAT_NIX
   }
#endif
#endif
}


/* ----------------------------------------------------------------------------
 * ODScrnSetBoundary()
 *
 * Sets the current boundary area on the screen. All output is constrained
 * within this boundary area.
 *
 * Parameters: btLeft   - 1-based column number of the left edge of the area.
 *
 *             btTop    - 1-based row number of the top edge of the area.
 *
 *             btRight  - 1-based column number of the right edge of the area.
 *
 *             btBottom - 1-based row number of the bottom edge of the area.
 *
 *     Return: void
 */
void ODScrnSetBoundary(BYTE btLeft, BYTE btTop, BYTE btRight, BYTE btBottom)
{
   /* Set internal window location variables. */
   btLeftBoundary = btLeft - 1;
   btRightBoundary = btRight - 1;
   btTopBoundary = btTop - 1;
   btBottomBoundary = btBottom - 1;

   /* Ensure that the cursor is located within the new window boundaries. */
   if(btCursorColumn > btRightBoundary - btLeftBoundary)
   {
      btCursorColumn = btRightBoundary - btLeftBoundary;
   }
   else if(btCursorColumn < btLeftBoundary)
   {
      btCursorColumn = btLeftBoundary;
   }

   if(btCursorRow > btBottomBoundary - btTopBoundary)
   {
      btCursorRow = btBottomBoundary - btTopBoundary;
   }
   else if(btCursorRow < btTopBoundary)
   {
      btCursorRow = btTopBoundary;
   }

   /* Execute the position flashing cursor primitive. */
   ODScrnUpdateCaretPos();
}


/* ----------------------------------------------------------------------------
 * ODScrnSetCursorPos()
 *
 * Sets the current cursor position. The cursor position is where the caret
 * (flashing cursor) appears (if it is currently turned on), and is the
 * location where ODScrnDisplayChar(), ODScrnDisplayString() and ODScrnPrintf()
 * will perform their output. Each of these functions, update the cursor
 * position to the next character cell after the end of their output. Other
 * ODScrn...() functions may also change the current cursor position.
 *
 * Parameters: btColumn - The 1-based column number where the cursor will
 *                        be placed.
 *
 *     Return: void
 */
void ODScrnSetCursorPos(BYTE btColumn, BYTE btRow)
{
   /* Set internal cursor position values. */
   btCursorColumn = btColumn - 1;
   btCursorRow = btRow - 1;

   /* Ensure that cursor falls within the current output window. */
   if(btCursorColumn > btRightBoundary - btLeftBoundary)
      btCursorColumn = btRightBoundary - btLeftBoundary;

   if(btCursorRow > btBottomBoundary - btTopBoundary)
      btCursorRow = btBottomBoundary - btTopBoundary;

   /* Execute the position flashing cursor primitive. */
   ODScrnUpdateCaretPos();
}


/* ----------------------------------------------------------------------------
 * ODScrnSetAttribute()
 *
 * Sets the current display attribute, to be used by ODScrnDisplayChar(),
 * ODScrnDisplayString(), ODScrnPrintf() and ODScrnClear(). The display
 * attribute byte is always in the IBM color attribute format, with the
 * lower 4 bits indicating the foreground color, and the next 3 bits
 * indicating the background color. The upper bit specifies whether the text
 * is flashing, although this code may not actually show flashing text on
 * all platforms.
 *
 * Parameters: btAttribute - The new color attribute to use.
 *
 *     Return: void
 */
void ODScrnSetAttribute(BYTE btAttribute)
{
   /* Set internal display colour attribute. */
   btCurrentAttribute = btAttribute;
}


/* ----------------------------------------------------------------------------
 * ODScrnEnableScrolling()
 *
 * Enables or disables scrolling of text within the currently defined boundary
 * area when a carriage return is sent with the cursor located on the bottom
 * line of bounary area.
 *
 * Parameters: bEnable - TRUE to enable scrolling, FALSE to disable scrolling.
 *
 *     Return: void
 */
void ODScrnEnableScrolling(BOOL bEnable)
{
   /* Stores the current scrolling setting. */
   bScrollEnabled = bEnable;
}


/* ----------------------------------------------------------------------------
 * ODScrnEnableCaret()
 *
 * Turns the caret (flashing indicator of the current cursor location) on or
 * off. Under the Win32 platform, the caret is always active when the
 * window has input focus, and inactive at any other time. Hene, under
 * Win32, this function has no effect.
 *
 * Parameters: bEnable - TRUE to turn on the flashing caret, FALSE to turn it
 *                       off.
 *
 *     Return: void
 */
void ODScrnEnableCaret(BOOL bEnable)
{
#ifdef ODPLAT_DOS
   if(bCaretOn == bEnable) return;

   bCaretOn = bEnable;

   /* Execute the cursor on / off primitive. */
   ASM    push si
   ASM    push di
   ASM    mov ah, 0x03
   ASM    mov bh, btDisplayPage
   ASM    int 0x10

   /* ch = start line, cl = end line. */
   ASM    push cx                
   ASM    mov ah, 0x0f
   ASM    int 0x10
   ASM    pop cx

   /* al = video mode. */
   ASM    push ax
   ASM    and ch, 0x1f
   ASM    mov al, bCaretOn
   ASM    and al, al
   ASM    jnz set_cursor
   /* ch bits 5-6 = blink attr */
   /*               00 = normal */
   /*               01 = invisible */
   ASM    or ch, 0x20
set_cursor:
   ASM    pop ax
   ASM    mov bh, btDisplayPage
   ASM    mov ah, 0x01
   ASM    int 0x10
   ASM    pop di
   ASM    pop si


   if(bCaretOn)
   {
      /* Turn on the local caret, updating its position. */
      ODScrnUpdateCaretPos();
   }
   else
   {
      /* Turn off the local caret. */
      ASM    mov ah, 0x02
      ASM    mov bh, btDisplayPage
      ASM    mov dh, OD_SCREEN_HEIGHT
      ASM    mov dl, OD_SCREEN_WIDTH
      ASM    push si
      ASM    push di
      ASM    int 0x10
      ASM    pop di
      ASM    pop si
   }
#endif /* ODPLAT_DOS */
}


/* ----------------------------------------------------------------------------
 * ODScrnGetTextInfo()
 *
 * Fills a structure with information about the current display settings,
 * including the position of the current boundary area (output window),
 * color attribute and cursor location.
 *
 * Parameters: pTextInfo - Pointer to the structure to store the current text
 *                         settings information in.
 *
 *     Return: void
 */
void ODScrnGetTextInfo(tODScrnTextInfo *pTextInfo)
{
   pTextInfo->wintop = btTopBoundary + 1;
   pTextInfo->winleft = btLeftBoundary + 1;
   pTextInfo->winright = btRightBoundary + 1;
   pTextInfo->winbottom = btBottomBoundary + 1;
   pTextInfo->attribute = btCurrentAttribute;
   pTextInfo->curx = btCursorColumn + 1;
   pTextInfo->cury = btCursorRow + 1;
}


/* ----------------------------------------------------------------------------
 * ODScrnPrintf()
 *
 * Performs formatted output within the current boundary area.
 *
 * Parameters: pszFormat - Format string, which is in the same format as is
 *                         used by the standard C printf() function.
 *
 *             The semantics of additional parameters is specified by the
 *             contents of the pszFormat string.
 *
 *     Return: The standard printf() return value.
 */
INT ODScrnPrintf(char *pszFormat, ...)
{
   va_list pArgumentList;
   INT nToReturn;

   /* Generate string to display. */
   va_start(pArgumentList, pszFormat);
   nToReturn = vsprintf(szBuffer, pszFormat, pArgumentList);
   va_end(pArgumentList);

   /* Ensure that we didn't overrun the buffer. */
   ASSERT(strlen(szBuffer) <= sizeof(szBuffer) - 1);

   /* Display generated string. */
   ODScrnDisplayString(szBuffer);

   /* Return appropriate value. */
   return (nToReturn);
}


/* ----------------------------------------------------------------------------
 * ODScrnDisplayChar()
 *
 * Writes a single character within the current boundary area, advancing the
 * cursor.
 *
 * Parameters: chToOutput  - The character to display.
 *
 *     Return: void
 */
void ODScrnDisplayChar(unsigned char chToOutput)
{
   BYTE ODFAR *pbtDest;
   
   ODScrnGetCursorPos();

   if(btCursorColumn > btRightBoundary - btLeftBoundary)
   {
      btCursorColumn = btRightBoundary - btLeftBoundary;
   }

   if(btCursorRow > btBottomBoundary - btTopBoundary)
   {
      btCursorRow = btBottomBoundary - btTopBoundary;
   }

   switch(chToOutput)
   {
      /* If character is a carriage return. */
      case '\r':
         btCursorColumn = 0;
         break;

      /* If character is a line feed. */
      case '\n':
         /* If cursor is at bottom of output window. */
         if(btCursorRow == btBottomBoundary - btTopBoundary)
         {
            /* Scroll the screen up by one line. */
            ODScrnScrollUpAndInvalidate();
         }
         /* If cursor is not at bottom of output window. */
         else
         {
            /* Move the cursor down one line. */
            ++btCursorRow;
         }
         break;

      case '\b':
         /* If backspace. */
         if(btCursorColumn != 0) --btCursorColumn;
         break;

      case '\t':
         /* If tab character. */
         btCursorColumn = ((btCursorColumn / 8) + 1) * 8;
         if(btCursorColumn > btRightBoundary - btLeftBoundary)
         {
            btCursorColumn = 0;

            /* If moving cursor down one line advances past end of window. */
            if(++btCursorRow > btBottomBoundary - btTopBoundary)
            {
               /* Move cursor back to bottom line of window. */
               btCursorRow = btBottomBoundary - btTopBoundary;

               /* Scroll the screen up by one line. */
               ODScrnScrollUpAndInvalidate();
            }
         }
         break;

      case '\a':
         /* If bell. */
         if(!od_control.od_silent_mode)
         {
#ifdef ODPLAT_DOS
            ASM    mov ah, 0x02
            ASM    mov dl, 7
            ASM    int 0x21
#endif /* ODPLAT_DOS */
#ifdef ODPLAT_WIN32
            MessageBeep(0xffffffff);
#endif /* ODPLAT_WIN32 */
         }
         break;

      /* If character is not a control character. */
      default:
         /* Output character to display buffer. */
         pbtDest = (BYTE ODFAR *)pScrnBuffer
            + ((btTopBoundary + btCursorRow) * BUFFER_LINE_BYTES
            + (btLeftBoundary + btCursorColumn) * BYTES_PER_CHAR);
         *pbtDest++ = chToOutput;
         *pbtDest = btCurrentAttribute;

         ASSERT(pbtDest >= (BYTE ODFAR *)pScrnBuffer);
         ASSERT(pbtDest < (BYTE ODFAR *)pScrnBuffer + SCREEN_BUFFER_SIZE);

#ifdef ODPLAT_WIN32
         /* Force the updated area of the screen window to be redrawn. */
         ODScrnInvalidate((BYTE)(btCursorColumn + btLeftBoundary),
            (BYTE)(btCursorRow + btTopBoundary),
            (BYTE)(btCursorColumn + btLeftBoundary),
            (BYTE)(btCursorRow + btTopBoundary));
#endif /* ODPLAT_WIN32 */

         /* Advance cursor. If at end of line ... */
         if(++btCursorColumn > btRightBoundary - btLeftBoundary)
         {
            /* Wrap cursor if necessary. */
            btCursorColumn = 0;

            /* If moving cursor down one line advances past end of window. */
            if(++btCursorRow > btBottomBoundary - btTopBoundary)
            {
               /* Move cursor back to bottom line of window. */
               btCursorRow = btBottomBoundary - btTopBoundary;

               /* Scroll the screen up by one line. */
               ODScrnScrollUpAndInvalidate();
            }
         }
   }

   /* Execute the update flashing cursor primitive. */
   ODScrnUpdateCaretPos();
}


/* ----------------------------------------------------------------------------
 * ODScrnGetCursorPos()                                *** PRIVATE FUNCTION ***
 *
 * Updates the current cursor position (output position) from the location of
 * the caret (flashing cursor). This function doesn't do anything on the
 * Win32 platform, since we nobody else can reposition the cursor.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODScrnGetCursorPos(void)
{
#ifdef ODPLAT_DOS
   if(!bCaretOn) return;

   ASM    mov ah, 0x03
   ASM    mov bh, btDisplayPage
   ASM    push si
   ASM    push di
   ASM    int 0x10
   ASM    pop di
   ASM    pop si
   ASM    sub dh, btTopBoundary
   ASM    mov btCursorRow, dh
   ASM    sub dl, btLeftBoundary
   ASM    mov btCursorColumn, dl
#endif /* ODPLAT_DOS */
}


/* ----------------------------------------------------------------------------
 * ODScrnUpdateCaretPos()                              *** PRIVATE FUNCTION ***
 *
 * Updates the position of the caret (flashing cursor) from the current cursor
 * location (output position).
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODScrnUpdateCaretPos(void)
{
#ifdef ODPLAT_DOS
   if(!bCaretOn) return;

   /* Update position of flashing cursor on screen */
   ASM    mov ah, 0x02
   ASM    mov bh, btDisplayPage
   ASM    mov dh, btCursorRow
   ASM    add dh, btTopBoundary
   ASM    mov dl, btCursorColumn
   ASM    add dl, btLeftBoundary
   ASM    push si
   ASM    push di
   ASM    int 0x10
   ASM    pop di
   ASM    pop si
#endif /* ODPLAT_DOS */

#ifdef ODPLAT_WIN32
   if(hwndScreenWindow != NULL)
   {
      PostMessage(hwndScreenWindow, WM_MOVE_YOUR_CARET, 0, 0);
   }
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODScrnClear()
 *
 * Clears the text within the currently defined boundary area, setting the
 * display attribute of the entire boundary area to the current display
 * color.
 *
 * Parameters: none
 *
 *     Return: void
 */
void ODScrnClear(void)
{
   WORD ODFAR *pDest = (WORD ODFAR *)pScrnBuffer +
      ((btTopBoundary * OD_SCREEN_WIDTH) + btLeftBoundary);
   WORD wBlank = (((WORD)btCurrentAttribute) << 8) | 32;
   BYTE btCurColumn;
   BYTE btCurLine = (btBottomBoundary - btTopBoundary) + 1;
   BYTE btColumnStart = (btRightBoundary - btLeftBoundary) + 1;
   BYTE btSkip = OD_SCREEN_WIDTH - btColumnStart;

   /* Clear contents of current window. */
   do
   {
      btCurColumn = btColumnStart;
      do
      {
         ASSERT(pDest >= (WORD ODFAR *)pScrnBuffer);
         ASSERT(pDest <= (WORD ODFAR *)pScrnBuffer + 2000);
         *(pDest++) = wBlank;
      } while ((--btCurColumn) != 0);
      pDest += btSkip;
   } while((--btCurLine) != 0);

   /* Move cursor to top left-hand corner of current window. */
   btCursorColumn = btCursorRow = 0;

   /* Execute the update flashing cursor primitive. */
   ODScrnUpdateCaretPos();

#ifdef ODPLAT_WIN32
   /* Force the updated area of the screen window to be redrawn. */
   ODScrnInvalidate(btLeftBoundary, btTopBoundary, btRightBoundary,
      btBottomBoundary);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODScrnScrollUpAndInvalidate()                       *** PRIVATE FUNCTION ***
 *
 * Scrolls the entire screen up by one line, only if scrolling is enabled.
 * If scrolling is performed, invalidates area that was scrolled. Scrolling
 * is accomplished using ODScrnScrollUpOneLine().
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODScrnScrollUpAndInvalidate(void)
{
   /* If scrolling is enabled. */
   if(bScrollEnabled)
   {
      /* Execute the scroll primitive. */
      ODScrnScrollUpOneLine();

#ifdef ODPLAT_WIN32
      /* Force the updated area of the screen window to be redrawn. */
      ODScrnInvalidate(btLeftBoundary, btTopBoundary, btRightBoundary,
         btBottomBoundary);
#endif /* ODPLAT_WIN32 */
   }
}


/* ----------------------------------------------------------------------------
 * ODScrnScrollUpOneLine()                             *** PRIVATE FUNCTION ***
 *
 * Scrolls the area within the current output boundary up one line, leaving the
 * newly created line at the bottom of the area blank, with the current display
 * attribute.
 *
 * Parameters: none
 *
 *     Return: void
 */
static void ODScrnScrollUpOneLine(void)
{
   WORD ODFAR *pwDest = (WORD ODFAR *)pScrnBuffer
      + (btTopBoundary * OD_SCREEN_WIDTH + btLeftBoundary);
   WORD ODFAR *pwSource;
   BYTE btCurColumn;
   BYTE btCurLine = btBottomBoundary - btTopBoundary;
   BYTE btColumnStart = btRightBoundary - btLeftBoundary + 1;
   BYTE btSkip = OD_SCREEN_WIDTH - btColumnStart;
   WORD wBlank = (((WORD)btCurrentAttribute) << 8) | 32;

   pwSource = pwDest + OD_SCREEN_WIDTH;

   ASSERT(btSkip >= 0 && btSkip <= OD_SCREEN_WIDTH);

   /* Move text in area of window up one line. */
   do
   {
      btCurColumn = btColumnStart;
      do
      {
         ASSERT(pwDest >= (WORD ODFAR *)pScrnBuffer);
         ASSERT(pwDest <= (WORD ODFAR *)pScrnBuffer + 2000);
         ASSERT(pwSource >= (WORD ODFAR *)pScrnBuffer);
         ASSERT(pwSource <= (WORD ODFAR *)pScrnBuffer+2000);
         *(pwDest++) = *(pwSource++);
      } while((--btCurColumn) != 0);
      pwDest += btSkip;
      pwSource += btSkip;
   } while ((--btCurLine) != 0);

   /* Clear newly created line at bottom of window. */
   btCurColumn = btColumnStart;
   do
   {
      ASSERT(pwDest >= (WORD ODFAR *)pScrnBuffer);
      ASSERT(pwDest <= (WORD ODFAR *)pScrnBuffer + 2000);
      *(pwDest++) = wBlank;
   } while((--btCurColumn) != 0);
}


/* ----------------------------------------------------------------------------
 * ODScrnGetText()
 *
 * Copies a portion of the currently displayed text and corresponding color
 * attributes to a buffer provided by the caller.
 *
 * Parameters: btLeft    - Column number of the left edge of the area to copy
 *                         from.
 *
 *             btTop     - Row number of the top edge of the area to copy from.
 *
 *             btRight   - Column number of the right edge of the area to copy
 *                         from.
 *
 *             btBottom  - Row number of the bottom edge of the area to copy
 *                         from.
 *
 *             pbtBuffer - A pointer to the buffer to copy to. It is the
 *                         caller's responsibility to ensure that this buffer
 *                         is large enough. This buffer must be at least
 *                         2 x (Width of area) x (Height of area) bytes in size.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
BOOL ODScrnGetText(BYTE btLeft, BYTE btTop, BYTE btRight, BYTE btBottom,
   void *pbtBuffer)
{
   WORD *pwBuffer = (WORD *)pbtBuffer;
   WORD ODFAR *pSource = (WORD ODFAR *)pScrnBuffer
      + ((((--btTop) + btTopBoundary)
      * OD_SCREEN_WIDTH) + btLeftBoundary + (--btLeft));
   BYTE btCurColumn;
   BYTE btCurLine = (--btBottom) - btTop + 1;
   BYTE btColumnStart = (--btRight) - btLeft + 1;
   BYTE btSkip = OD_SCREEN_WIDTH - btColumnStart;

   ASSERT(btLeft >= 0);
   ASSERT(btTop >= 0);
   ASSERT(btRight <= btRightBoundary - btLeftBoundary);
   ASSERT(btBottom <= btBottomBoundary - btTopBoundary);
   ASSERT(pbtBuffer);

   /* Copy contents of screen block to buffer */
   do 
   {
      btCurColumn = btColumnStart;
      do
      {
         ASSERT(pSource >= (WORD ODFAR *)pScrnBuffer);
         ASSERT(pSource <= (WORD ODFAR *)pScrnBuffer + 2000);
         ASSERT(pwBuffer >= (WORD *)pbtBuffer);
         ASSERT(pwBuffer <= (WORD *)pbtBuffer + 2000);
         *(pwBuffer++) = *(pSource++);
      } while ((--btCurColumn) != 0);
      pSource += btSkip;
   } while((--btCurLine) != 0);

   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODScrnPutText()
 *
 * Changes the currently displayed text and corresponding color attributes in
 * the specified area, to the values taken from the buffer. This buffer is in
 * the same format as is produce by the ODScrnGetText() function.
 *
 * Parameters: btLeft    - Column number of the left edge of the area to copy
 *                         to.
 *
 *             btTop     - Row number of the top edge of the area to copy to.
 *
 *             btRight   - Column number of the right edge of the area to copy
 *                         to.
 *
 *             btBottom  - Row number of the bottom edge of the area to copy
 *                         to.
 *
 *             pbtBuffer - A pointer to the buffer to copy from.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
BOOL ODScrnPutText(BYTE btLeft, BYTE btTop, BYTE btRight, BYTE btBottom,
   void *pbtBuffer)
{
   WORD *pwBuffer = (WORD *)pbtBuffer;
   WORD ODFAR *pDest = (WORD ODFAR *)pScrnBuffer
      + ((((--btTop) + btTopBoundary)
      * OD_SCREEN_WIDTH) + btLeftBoundary + (--btLeft));
   BYTE btCurColumn;
   BYTE btCurLine = (--btBottom) - btTop + 1;
   BYTE btColumnStart = (--btRight) - btLeft + 1;
   BYTE btSkip = OD_SCREEN_WIDTH - btColumnStart;

   ASSERT(btLeft >= 0 && btTop >= 0);
   ASSERT(btLeft <= btRightBoundary - btLeftBoundary);
   ASSERT(btTop <= btBottomBoundary - btTopBoundary);
   ASSERT(btRight >= 0 && btBottom >= 0);
   ASSERT(btRight <= btRightBoundary - btLeftBoundary);
   ASSERT(btBottom <= btBottomBoundary - btTopBoundary);
   ASSERT(pbtBuffer != NULL);

   /* Copy contents of screen block to buffer. */
   do
   {
      btCurColumn = btColumnStart;
      do
      {
         ASSERT(pDest >= (WORD ODFAR *)pScrnBuffer);
         ASSERT(pDest <= (WORD ODFAR *)pScrnBuffer + 2000);
         ASSERT(pwBuffer >= (WORD *)pbtBuffer);
         ASSERT(pwBuffer <= (WORD *)pbtBuffer + 2000);
         *(pDest++) = *(pwBuffer++);
      } while ((--btCurColumn) != 0);
      pDest += btSkip;
   } while((--btCurLine) != 0);

#ifdef ODPLAT_WIN32
   /* Force the updated area of the screen window to be redrawn. */
   ODScrnInvalidate((BYTE)(btLeftBoundary + btLeft), 
      (BYTE)(btTopBoundary + btTop),
      (BYTE)(btRightBoundary + btRight),
      (BYTE)(btBottomBoundary + btBottom));
#endif /* ODPLAT_WIN32 */

   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODScrnDisplayString()
 *
 * Copies the contents of a string to the display, using the currently set
 * color attributes. The cursor location is updated to the end of the string
 * on the screen.
 *
 * Parameters: pszString - Pointer to the string to display.
 *
 *     Return: void.
 */
void ODScrnDisplayString(const char *pszString)
{
   ODScrnDisplayBuffer(pszString, strlen(pszString));
}


/* ----------------------------------------------------------------------------
 * ODScrnDisplayBuffer()
 *
 * Copies the contents of a buffer to the display, using the currently set
 * color attributes. The cursor location is updated to the end of the text
 * displayed to the screen.
 *
 * Parameters: pBuffer         - Pointer to a buffer containing the
 *                               character(s) to display.
 *
 *             nCharsToDisplay - Count of number of characters to display from
 *                               the buffer.
 *
 *     Return: void.
 */
void ODScrnDisplayBuffer(const char *pBuffer, INT nCharsToDisplay)
{
   const char *pchCurrentChar = pBuffer;
   INT nCharsLeft = nCharsToDisplay;
   BYTE ODFAR *pDest;
   BYTE btLeftColumn;
   BYTE btAttribute = btCurrentAttribute;
   BYTE btCurrentColumn;
   BYTE btBottom = btBottomBoundary - btTopBoundary;
#ifdef ODPLAT_WIN32
   BOOL bAnythingInvalid = FALSE;
   BYTE btLeftMost;
   BYTE btRightMost;
   BYTE btTopMost;
   BYTE btBottomMost;
#endif /* ODPLAT_WIN32 */

   ASSERT(pBuffer != NULL);
   ASSERT(nCharsToDisplay >= 0);

   ODScrnGetCursorPos();

   if(btCursorColumn > btRightBoundary - btLeftBoundary)
   {
      btCursorColumn = btRightBoundary - btLeftBoundary;
   }

   if(btCursorRow > btBottomBoundary - btTopBoundary)
   {
      btCursorRow = btBottomBoundary - btTopBoundary;
   }

   btCurrentColumn = btCursorColumn;

   btLeftColumn = btRightBoundary - (btCurrentColumn + btLeftBoundary);
   pDest = (BYTE ODFAR *) pScrnBuffer + (((btTopBoundary + btCursorRow)
      * BUFFER_LINE_BYTES)
      + (btLeftBoundary + btCursorColumn) * BYTES_PER_CHAR);

   while(nCharsLeft--)
   {
      ASSERT(pDest >= (BYTE ODFAR *)pScrnBuffer);
      ASSERT(pDest <= (BYTE ODFAR *)pScrnBuffer + SCREEN_BUFFER_SIZE);
      switch(*pchCurrentChar)
      {
         case '\r':
            btCurrentColumn = 0;
            btLeftColumn = btRightBoundary - btLeftBoundary;
            pDest = (BYTE ODFAR *)pScrnBuffer + ((btTopBoundary + btCursorRow)
               * BUFFER_LINE_BYTES + btLeftBoundary * BYTES_PER_CHAR);
            pchCurrentChar++;
            break;
         case '\n':
            if (btCursorRow < btBottom)
            {
               ++btCursorRow;
               pDest += BUFFER_LINE_BYTES;
            }
            else if(bScrollEnabled)
            {
               ODScrnScrollUpOneLine();
#ifdef ODPLAT_WIN32
               /* Entire boundary area is now invalid. */
               bAnythingInvalid = TRUE;
               btLeftMost = btLeftBoundary;
               btRightMost = btRightBoundary;
               btTopMost = btTopBoundary;
               btBottomMost = btBottomBoundary;
#endif /* ODPLAT_WIN32 */
            }
            pchCurrentChar++;
            break;

         case '\a':
            /* If bell */
            if(!od_control.od_silent_mode)
            {
#ifdef ODPLAT_DOS
               ASM    mov ah, 0x02
               ASM    mov dl, 7
               ASM    int 0x21
#endif /* ODPLAT_DOS */
#ifdef ODPLAT_WIN32
               MessageBeep(0xffffffff);
#endif /* ODPLAT_WIN32 */
               pchCurrentChar++;
            }
            break;

         case '\t':
            /* If tab character. */
            btCurrentColumn = ((btCurrentColumn / 8) + 1) * 8;
            if(btCurrentColumn > btRightBoundary - btLeftBoundary)
            {
               btCurrentColumn = 0;

               /* If moving cursor down one line advances past end of window. */
               if(++btCursorRow > btBottomBoundary - btTopBoundary)
               {
                  /* Move cursor back to bottom line of window. */
                  btCursorRow = btBottomBoundary - btTopBoundary;

                  /* If scrolling is enabled. */
                  if(bScrollEnabled)
                  {
                     /* Execute the scroll primitive .*/
                     ODScrnScrollUpOneLine();
#ifdef ODPLAT_WIN32
                     /* Entire boundary area is now invalid. */
                     bAnythingInvalid = TRUE;
                     btLeftMost = btLeftBoundary;
                     btRightMost = btRightBoundary;
                     btTopMost = btTopBoundary;
                     btBottomMost = btBottomBoundary;
#endif /* ODPLAT_WIN32 */
                  }
               }
            }

            /* Determine new buffer destination address. */
            pDest = (BYTE ODFAR *) pScrnBuffer
               + (((btTopBoundary + btCursorRow) * BUFFER_LINE_BYTES)
               + (btLeftBoundary + btCursorColumn) * BYTES_PER_CHAR);
            break;

         case '\b':
            if(btCurrentColumn > 0)
            {
               --btCurrentColumn;
               pDest-=2;
               btLeftColumn++;
            }
            pchCurrentChar++;
            break;

         default:
            *(pDest++) = *(pchCurrentChar++);
            *(pDest++) = btAttribute;

#ifdef ODPLAT_WIN32
            /* Expand area to invalidate, if needed. */
            if(!bAnythingInvalid)
            {
               bAnythingInvalid = TRUE;
               btLeftMost = btLeftBoundary + btCurrentColumn;
               btRightMost = btLeftBoundary + btCurrentColumn;
               btTopMost = btTopBoundary + btCursorRow;
               btBottomMost = btTopBoundary + btCursorRow;
            }
            else
            {
               BYTE btColumn = btLeftBoundary + btCurrentColumn;
               BYTE btRow = btTopBoundary + btCursorRow;

               if(btColumn < btLeftMost) btLeftMost = btColumn;
               if(btColumn > btRightMost) btRightMost = btColumn;
               if(btRow < btTopMost) btTopMost = btRow;
               if(btRow > btBottomMost) btBottomMost = btRow;
            }
#endif /* ODPLAT_WIN32 */

            if(btLeftColumn--)
            {
               ++btCurrentColumn;
            }
            else
            {
               btCurrentColumn = 0;
               btLeftColumn = btRightBoundary - btLeftBoundary;

               if(btCursorRow < btBottom)
               {
                  ++btCursorRow;
               }
               else if(bScrollEnabled)
               {
                  ODScrnScrollUpOneLine();
#ifdef ODPLAT_WIN32
                  /* Entire boundary area is now invalid. */
                  bAnythingInvalid = TRUE;
                  btLeftMost = btLeftBoundary;
                  btRightMost = btRightBoundary;
                  btTopMost = btTopBoundary;
                  btBottomMost = btBottomBoundary;
#endif /* ODPLAT_WIN32 */
               }

               pDest = (BYTE ODFAR *)pScrnBuffer
                  + ((btTopBoundary + btCursorRow)
                  * BUFFER_LINE_BYTES + btLeftBoundary * BYTES_PER_CHAR);
            }
      }
   }

   btCursorColumn = btCurrentColumn;
   ODScrnUpdateCaretPos();

#ifdef ODPLAT_WIN32
   if(bAnythingInvalid)
   {
      /* Force the updated area of the screen window to be redrawn. */
      ODScrnInvalidate(btLeftMost, btTopMost, btRightMost,
         btBottomMost);
   }
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODScrnCopyText()
 *
 * Copies the contents of the specified area on the screen to another location
 * on the screen. The destination location must be such that the entire area
 * specified as the source can be displayed without falling off the edge of the
 * screen.
 *
 * Parameters: btLeft       - Column number of the left edge of the area to
 *                            copy from.
 *
 *             btTop        - Row number of the top edge of the area to copy
 *                            from.
 *
 *             btRight      - Column number of the right edge of the area to
 *                            copy from.
 *
 *             btBottom     - Row number of the bottom edge of the area to
 *                            copy from.
 *
 *             btDestColumn - Column number where the upper right corner of
 *                            the area should be copied to.
 *
 *             btDestRow    - Row number where the upper right cornder of the
 *                            area should be copied to.
 *
 *     Return: TRUE on success, or FALSE on failure. May fail due to
 *             insufficient available memory.
 */
BOOL ODScrnCopyText(BYTE btLeft, BYTE btTop, BYTE btRight, BYTE btBottom,
   BYTE btDestColumn, BYTE btDestRow)
{
   void *pScrnBuffer;

   ASSERT(btLeft >= 0 && btTop >= 0);
   ASSERT(btLeft <= btRightBoundary - btLeftBoundary);
   ASSERT(btTop <= btBottomBoundary - btTopBoundary);
   ASSERT(btRight >= 0 && btBottom >= 0);
   ASSERT(btRight <= btRightBoundary - btLeftBoundary);
   ASSERT(btBottom <= btBottomBoundary - btTopBoundary);
   ASSERT(btDestColumn >= 0 && btDestRow >= 0);
   ASSERT(btDestColumn <= btRightBoundary - btLeftBoundary);
   ASSERT(btDestRow <= btBottomBoundary - btTopBoundary);

   if(   !(btLeft <= btRightBoundary - btLeftBoundary
         && btTop <= btBottomBoundary - btTopBoundary)
      || !(btRight <= btRightBoundary - btLeftBoundary
         && btBottom <= btBottomBoundary - btTopBoundary)
      || !(btDestColumn <= btRightBoundary - btLeftBoundary
         && btDestRow <= btBottomBoundary - btTopBoundary))
   {
      return(FALSE);
   }


   if((pScrnBuffer = malloc((btRight - btLeft + 1) * (btBottom - btTop + 1)
      * BYTES_PER_CHAR)) == NULL)
   {
      /* Insufficient memory, return with failure. */
      return (FALSE);
   }

   ODScrnGetText(btLeft, btTop, btRight, btBottom, pScrnBuffer);
   ODScrnPutText(btDestColumn, btDestRow,
      (BYTE)(btRight + (btDestColumn - btLeft)),
      (BYTE)(btBottom + (btDestRow - btTop)), pScrnBuffer);
   free(pScrnBuffer);

   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODScrnClearToEndOfLine()
 *
 * Clears the contents of the current line, from the current cursor location
 * to the end of the line.
 *
 * Parameters: none
 *
 *     Return: void
 */
void ODScrnClearToEndOfLine(void)
{
   unsigned char btCharsToDelete = btRightBoundary
      - (btLeftBoundary + btCursorColumn);
   BYTE ODFAR *pDest = (BYTE ODFAR *) pScrnBuffer
      + (((btTopBoundary + btCursorRow) * BUFFER_LINE_BYTES)
      + (btLeftBoundary + btCursorColumn) * BYTES_PER_CHAR);
   BYTE btAttribute = btCurrentAttribute;

   while(btCharsToDelete--)
   {
      *(pDest++) = ' ';
      *(pDest++) = btAttribute;
   }

#ifdef ODPLAT_WIN32
   /* Force the updated area of the screen window to be redrawn. */
   ODScrnInvalidate((BYTE)(btLeftBoundary + btCursorColumn),
      (BYTE)(btTopBoundary + btCursorRow), btRightBoundary,
      (BYTE)(btTopBoundary + btCursorRow));
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODScrnCreateWindow()
 *
 * Creates a text-based window on the local terminal.
 *
 * Parameters: btLeft           - Column numebr of the left of the window.
 *
 *             btTop            - Row number of the top of the window.
 *
 *             btRight          - Column number of the right of the window.
 *
 *             btBottom         - Row number of the bottom of the window.
 *
 *             btAttribute      - Display attribute for the window boarder and
 *                                the area inside the window.
 *
 *             pszTitle         - Pointer to a string containing the title to
 *                                display, or "" for none.
 *
 *             btTitleAttribute - Display attribute for the title text.
 *
 *     Return: void
 */
#ifdef OD_TEXTMODE
void *ODScrnCreateWindow(BYTE btLeft, BYTE btTop, BYTE btRight,
   BYTE btBottom, BYTE btAttribute, char *pszTitle, BYTE btTitleAttribute)
{
   void *pUnder;
   INT nBetween;
   INT nCount;
   INT nFirst;
   char *pszString;
   int nTitleWidth;

   ASSERT(pszTitle != NULL);

   /* Alocate space to store screen contents "under" window. */
   if((pUnder = malloc((btRight - btLeft + 1) * (btBottom - btTop + 1)
      * BYTES_PER_CHAR + 4 * sizeof(BYTE))) == NULL)
   {
      return(NULL);
   }

   /* Store the window's position in the buffer. */
   ((BYTE *)pUnder)[0] = btLeft;
   ((BYTE *)pUnder)[1] = btTop;
   ((BYTE *)pUnder)[2] = btRight;
   ((BYTE *)pUnder)[3] = btBottom;

   /* Retrieve screen contents in window area. */
   ODScrnGetText(btLeft, btTop, btRight, btBottom, ((BYTE *)pUnder) + 4);

   /* Determine area between left & right of window, distance of line before */
   /* title, and distance of line after title.                               */
   if(strlen(pszTitle) == 0)
   {
      nTitleWidth = 0;
   }
   else
   {
      nTitleWidth = strlen(pszTitle) + 2;
   }
   nCount = (nBetween = btRight - btLeft - 1) - nTitleWidth;
   nCount -= (nFirst = nCount / 2);

   /* Prepare to begin drawing window at upper left corner */
   ODScrnSetCursorPos(btLeft, btTop);
   ODScrnSetAttribute(btAttribute);

   /* Draw first line of window */
   ODScrnDisplayChar((unsigned char)214);
   while(nFirst--) ODScrnDisplayChar((unsigned char)196);
   if(strlen(pszTitle) != 0)
   {
      ODScrnSetAttribute(btTitleAttribute);
      ODScrnDisplayChar(' ');
      ODScrnDisplayString(pszTitle);
      ODScrnDisplayChar(' ');
      ODScrnSetAttribute(btAttribute);
   }
   while(nCount--) ODScrnDisplayChar((unsigned char)196);
   ODScrnDisplayChar((unsigned char)183);

   /* Build string for working lines */
   pszString = szBuffer;
   *pszString++ = (unsigned char)186;
   nCount = nBetween;
   while(nCount--) *pszString++ = ' ';
   *pszString++ = (unsigned char)186;
   *pszString++ = '\0';

   /* Draw working lines of window */
   for(nCount = btTop + 1; nCount < btBottom; ++nCount)
   {
      ODScrnSetCursorPos(btLeft, (BYTE)nCount);
      ODScrnDisplayString(szBuffer);
   }

   /* Draw last line of window */
   ODScrnSetCursorPos(btLeft, btBottom);
   ODScrnDisplayChar((unsigned char)211);
   while(nBetween--) ODScrnDisplayChar((unsigned char)196);
   ODScrnDisplayChar((unsigned char)189);

   /* return pointer to buffer */
   return(pUnder);
}
#endif /* OD_TEXTMODE */


/* ----------------------------------------------------------------------------
 * ODScrnDestroyWindow()
 *
 * Removes a text-based window that was created by ODScrnCreateWindow().
 *
 * Parameters: pWindow   - Pointer to the buffer returned by the corresponding
 *                         call to ODScrnCreateWindow().
 *
 *     Return: void
 */
#ifdef OD_TEXTMODE
void ODScrnDestroyWindow(void *pWindow)
{
   BYTE btLeft;
   BYTE btTop;
   BYTE btRight;
   BYTE btBottom;
   BYTE *pabtWindow = (BYTE *)pWindow;

   ASSERT(pWindow != NULL);

   /* Determine the location of the window. */
   btLeft = pabtWindow[0];
   btTop = pabtWindow[1];
   btRight = pabtWindow[2];
   btBottom = pabtWindow[3];

   /* Restore original screen contents under the window. */
   ODScrnPutText(btLeft, btTop, btRight, btBottom, ((BYTE *)pWindow) + 4);

   /* Deallocate window buffer. */
   free(pWindow);
}
#endif /* OD_TEXTMODE */


/* ----------------------------------------------------------------------------
 * ODScrnLocalInput()
 *
 * Inputs a string, only displaying input on local screen.
 *
 * Parameters: btLeft    - Column number of the left end of the input
 *                         field.
 *
 *             btRow     - Row number where the input field appears.
 *
 *             pszString - Location where user's input should be stored. Must
 *                         be initialized.
 *
 *             nMaxChars - The maximum number of characters that may be
 *                         accepted for input into the string.
 *
 *     Return: void
 */
#ifdef OD_TEXTMODE
void ODScrnLocalInput(BYTE btLeft, BYTE btRow, char *pszString,
   BYTE btMaxChars)
{
   BYTE btCount;
   BYTE btCurrentPos;
   BOOL bAnyKeysPressed = FALSE;
   tODInputEvent InputEvent;

   /* Draw initial input field. */
   ODScrnSetCursorPos(btLeft, btRow);
   ODScrnDisplayString(pszString);
   for(btCount = strlen(pszString); btCount <= btMaxChars; ++btCount)
   {
      ODScrnDisplayChar(177);
   }

   /* Start with the cursor at the end of the input field. */
   btCurrentPos = strlen(pszString);

   /* Loop until the user presses enter. */
   for(;;)
   {
      /* Position the cursor at the appropriate location. */
      ODScrnSetCursorPos((BYTE)(btLeft + btCurrentPos), btRow);

      /* Obtain the next input event. */
      ODInQueueGetNextEvent(hODInputQueue, &InputEvent, OD_NO_TIMEOUT);

      switch(InputEvent.chKeyPress)
      {
         case '\b':
            /* If user presses [Backspace], then move back if we are not at */
            /* the left of the input field.                                 */
            if(btCurrentPos > 0)
            {
               /* Backspace, removing last character from string. */
               btCurrentPos--;
               ODScrnSetCursorPos((BYTE)(btLeft + btCurrentPos), btRow);
               ODScrnDisplayChar(177);
               pszString[btCurrentPos] = '\0';
            }
            break;

         case '\n':
         case '\r':
            /* If user presses [Enter], then exit from the function. */
            return;

         case '\0':
            /* In the case of a multi-character sequence, skip the next */
            /* character from the input queue.                          */
            ODInQueueGetNextEvent(hODInputQueue, &InputEvent, OD_NO_TIMEOUT);
            break;

         default:
            /* If this is a valid string character for the string. */
            if(InputEvent.chKeyPress >= ' ')
            {
               /* If no keys have been pressed yet, then erase the entire */
               /* string first.                                           */
               if(!bAnyKeysPressed)
               {
                  btCurrentPos = 0;
                  ODScrnSetCursorPos(btLeft, btRow);
                  for(btCount = 0; btCount <= btMaxChars; ++btCount)
                  {
                     ODScrnDisplayChar(177);
                  }
                  ODScrnSetCursorPos(btLeft, btRow);
               }

               /* If we are not at the end of the string, then add the */
               /* character to the string.                             */
               if(btCurrentPos < btMaxChars)
               {
                  /* Display the new character. */
                  ODScrnDisplayChar(InputEvent.chKeyPress);

                  /* Add the character to the string. */
                  pszString[btCurrentPos] = InputEvent.chKeyPress;

                  /* Update the current cursor position. */
                  ++btCurrentPos;

                  /* Terminate the string. */
                  pszString[btCurrentPos] = '\0';
               }
            }
      }

      /* Note that a key has now been pressed. */
      bAnyKeysPressed = TRUE;
   }
}
#endif /* OD_TEXTMODE */


/* ----------------------------------------------------------------------------
 * ODScrnShowMessage()
 *
 * Displays a message window with the specified message text. Unlike the
 * Windows MessageBox() function, this message box is removed by the caller
 * of the function rather than the user.
 *
 * Parameters: pszText - Pointer to message text to be displayed. This string
 *                       must continue to exist until after the
 *                       ODScrnRemoveMessage() function is called.
 *
 *             nFlags  - Currently unused, must be 0.
 *
 *     Return: A pointer which must be passed to ODScrnRemoveMessage() in
 *             order to remove this message from the screen. A return value
 *             of NULL does not necessarily indicate window creation failure,
 *             and should still be passed to a corresponding call to
 *             ODScrnRemoveMessage().
 */
void *ODScrnShowMessage(char *pszText, int nFlags)
{
   ASSERT(pszText != NULL);
   ASSERT(nFlags == 0);

   /* In silent mode, this function does nothing. */
   if(od_control.od_silent_mode) return(NULL);

#ifdef ODPLAT_WIN32

   /* Place a message in the frame window's message queue, asking it to  */
   /* create the message window.                                         */
   PostMessage(GetParent(hwndScreenWindow), WM_SHOW_MESSAGE, (WPARAM)nFlags,
      (LPARAM)pszText);

   return(NULL);

#else /* !ODPLAT_WIN32 */
   {
      int nWindowWidth;
      int nLeftColumn;
      char szMessage[74];
      void *pWindow;

      UNUSED(nFlags);

      ODStringCopy(szMessage, pszText, sizeof(szMessage));

      ODStoreTextInfo();

      nWindowWidth = strlen(szMessage) + 4;
      nLeftColumn = 40 - (nWindowWidth / 2);
      if((pWindow = ODScrnCreateWindow((BYTE)nLeftColumn, 10,
         (BYTE)(nLeftColumn + (nWindowWidth - 1)), 14,
         od_control.od_local_win_col, "", od_control.od_local_win_col))
         == NULL)
      {
         return(NULL);
      }

      ODScrnSetCursorPos((BYTE)(42 - (nWindowWidth / 2)), 12);
      ODScrnDisplayString(szMessage);
      ODRestoreTextInfo();

      ODScrnEnableCaret(FALSE);

      return(pWindow);
   }
#endif /* !ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODScrnRemoveMessage()
 *
 * Removes a message that was shown by a previous call to ODScrnShowMessage().
 *
 * Parameters: pMessageInfo - Pointer to the buffer returned by the
 *                            corresponding call to ODScrnShowMessage().
 *
 *     Return: void
 */
void ODScrnRemoveMessage(void *pMessageInfo)
{
   /* In silent mode, this function does nothing. */
   if(od_control.od_silent_mode) return;

#ifdef ODPLAT_WIN32
   /* Place a message in the frame window's message queue, asking it to  */
   /* remove the message window.                                         */
   SendMessage(GetParent(hwndScreenWindow), WM_REMOVE_MESSAGE, 0, 0L);
#else /* !ODPLAT_WIN32 */
   /* If pMessageInfo is NULL, then we do nothing. */
   if(pMessageInfo == NULL) return;

   ODStoreTextInfo();
   ODScrnDestroyWindow(pMessageInfo);
   ODRestoreTextInfo();
   ODScrnEnableCaret(TRUE);
#endif /* !ODPLAT_WIN32 */
}

