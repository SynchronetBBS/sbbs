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
 *        File: ODRes.h
 *
 * Description: OpenDoors resource-related definitions. This file is only
 *              applicable when building the Win32 version of OpenDoors.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Dec 02, 1995  6.00  BP   Created.
 *              Jan 20, 1996  6.00  BP   Added login dialog box.
 *              Jan 21, 1996  6.00  BP   Added message dialog box.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 23, 1996  6.00  BP   Remove unused IDs.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 14, 1996  6.10  BP   Added configuration menu option.
 */

/* Resource IDs. */
#define IDR_FRAME_MENU                  200
#define IDD_ABOUT                       201
#define IDI_OPENDOORS                   202
#define IDB_TOOLBAR                     203
#define IDR_FRAME                       204
#define IDD_LOGIN                       205
#define IDI_MESSAGE_INFO                206
#define IDD_MESSAGE                     207


/* Dialog box control IDs. (The same values can safely be used by other */
/* dialog boxes.)                                                       */
/* Help dialog box. */
#define IDC_DOORNAME                    1000
#define IDC_COPYRIGHT                   1001
#define IDC_VERSION                     1002

/* Login dialog box. */
#define IDC_USER_NAME                   1000

/* Message dialog box. */
#define IDC_MESSAGE_ICON                1000
#define IDC_MESSAGE_TEXT1               1001


/* Command IDs. */
#define ID_DOOR_CHATMODE                50000
#define ID_DOOR_USERKEYBOARDOFF         50001
#define ID_DOOR_SYSOPNEXT               50002
#define ID_DOOR_HANGUP                  50003
#define ID_VIEW_STATUSBAR               50006
#define ID_USER_ADDONEMINUTE            50007
#define ID_USER_ADDFIVEMINUTES          50008
#define ID_USER_SUBTRACTONEMINUTE       50009
#define ID_USER_SUBTRACTFIVEMINUTES     50010
#define ID_USER_INACTIVITYTIMER         50011
#define ID_HELP_ABOUT                   50012
#define ID_HELP_CONTENTS                50013
#define ID_VIEW_TOOL_BAR                50014
#define ID_DOOR_EXIT                    50015
#define ID_DOOR_LOCKOUT                 50016
#define ID_VIEW_STAT_BAR                50017
#define ID_DOOR_CONFIG                  50018
