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
 *        File: ODScrn.h
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
 *              Nov 14, 1995  6.00  BP   Created.
 *              Jan 21, 1996  6.00  BP   Added ODScrnShowMessage() and related.
 *              Jan 27, 1996  6.00  BP   Made text-mode window f'ns static.
 *              Jan 31, 1996  6.00  BP   Made them non-static again.
 *              Jan 31, 1996  6.00  BP   Added ODScrnLocalInput().
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 */

#ifndef _INC_ODSCRN
#define _INC_ODSCRN

#include "ODTypes.h"
#include "ODPlat.h"

/* Text information structure. */
typedef struct
{
   unsigned char winleft;
   unsigned char wintop;
   unsigned char winright;
   unsigned char winbottom;
   unsigned char attribute;
   unsigned char curx;
   unsigned char cury;
} tODScrnTextInfo;


/* Screen buffer initialization and shutdown functions. */
tODResult ODScrnInitialize(void);
void ODScrnShutdown(void);

/* Basic text output functions. */
void ODScrnDisplayChar(unsigned char chToOutput);
void ODScrnDisplayBuffer(const char *pBuffer, INT nCharsToDisplay);
void ODScrnDisplayString(const char *pszString);
INT ODScrnPrintf(char *pszFormat, ...);

/* Functions for manipulating rectangular areas of the screen buffer. */
BOOL ODScrnGetText(BYTE btLeft, BYTE btTop, BYTE btRight, BYTE btBottom,
   void *pbtBuffer);
BOOL ODScrnPutText(BYTE btLeft, BYTE btTop, BYTE btRight, BYTE btBottom,
   void *pbtBuffer);
BOOL ODScrnCopyText(BYTE btLeft, BYTE btTop, BYTE btRight, BYTE btBottom,
   BYTE btDestColumn, BYTE btDestRow);

/* Functions for clearing portions of the screen buffer. */
void ODScrnClear(void);
void ODScrnClearToEndOfLine(void);

/* Functions for setting or obtaining current display settings. */
void ODScrnSetBoundary(BYTE btLeft, BYTE btTop, BYTE btRight, BYTE btBottom);
void ODScrnSetCursorPos(BYTE btColumn, BYTE btRow);
void ODScrnSetAttribute(BYTE btAttribute);
void ODScrnEnableScrolling(BOOL bEnable);
void ODScrnEnableCaret(BOOL bEnable);
void ODScrnGetTextInfo(tODScrnTextInfo *pTextInfo);

/* Functions for displaying OpenDoors message window. */
void *ODScrnShowMessage(char *pszText, int nFlags);
void ODScrnRemoveMessage(void *pMessageInfo);

/* Additional local output functions for text mode based versions. */
#ifdef OD_TEXTMODE
void *ODScrnCreateWindow(BYTE btLeft, BYTE btTop, BYTE btRight,
   BYTE btBottom, BYTE btAttribute, char *pszTitle, BYTE btTitleAttribute);
void ODScrnDestroyWindow(void *pWindow);
void ODScrnLocalInput(BYTE btLeft, BYTE btRow, char *pszString,
   BYTE btMaxChars);
#endif /* OD_TEXTMODE */

/* Functions for local screen window under Win32 version. */
#ifdef ODPLAT_WIN32
tODResult ODScrnStartWindow(HANDLE hInstance, tODThreadHandle *phScreenThread,
   HWND hwndFrame);
void ODScrnSetFocusToWindow(void);
void ODScrnAdjustWindows(void);
#endif /* ODPLAT_WIN32 */

#endif /* _INC_ODSCRN */
