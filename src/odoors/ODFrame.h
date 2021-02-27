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
 *        File: ODFrame.h
 *
 * Description: Defines the public interface to the OpenDoors frame window.
 *              This file is only applicable when building the Win32 version
 *              of OpenDoors.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Aug 20, 1995  6.00  BP   Created.
 *              Jan 20, 1996  6.00  BP   Made ODFrameCenter...() shared.
 *              Feb 17, 1996  6.00  BP   Add ...Accelerator() return value.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 */

#ifndef _INC_ODFRAME
#define _INC_ODFRAME

#include "ODPlat.h"
#include "ODGen.h"

#ifdef ODPLAT_WIN32

/* Public frame window functions. */
tODResult ODFrameStart(HANDLE hInstance, tODThreadHandle *phFrameThread);
INT ODFrameGetUsedClientAtTop(HWND hwndFrame);
INT ODFrameGetUsedClientAtBottom(HWND hwndFrame);
BOOL ODFrameTranslateAccelerator(HWND hwndFrame, LPMSG pMsg);
void ODFrameUpdateCmdUI(void);
void ODFrameUpdateTimeDisplay(void);
void ODFrameUpdateWantChat(void);
void ODFrameCenterWindowInParent(HWND hwndChild);

/* User defined messages that are handled by the frame window. */
#define WM_SHOW_MESSAGE             (WM_USER + 1)
#define WM_REMOVE_MESSAGE           (WM_USER + 2)

#endif /* ODPLAT_WIN32 */

#endif /* _INC_ODFRAME */
