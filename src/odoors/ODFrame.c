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
 *        File: ODFrame.c
 *
 * Description: Implements the OpenDoors frame window which provides the
 *              menu, toolbar, and status bar. The frame window's client
 *              area contains the display window which shows the door's
 *              output as the remote user would see it. This file should
 *              not be built into non-Windows versions of OpenDoors.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Aug 20, 1995  6.00  BP   Created.
 *              Dec 20, 1995  6.00  BP   Remember toolbar & statusbar settings.
 *              Dec 22, 1995  6.00  BP   Added od_connect_speed.
 *              Jan 20, 1996  6.00  BP   Made ODFrameCenter...() shared.
 *              Jan 21, 1996  6.00  BP   Added ODScrnShowMessage() and related.
 *              Feb 17, 1996  6.00  BP   Add ...Accelerator() return value.
 *              Feb 17, 1996  6.00  BP   Pass WM_MENUSELECT to DefWindowProc().
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 21, 1996  6.00  BP   Fixed user keyboard off command.
 *              Feb 22, 1996  6.00  BP   Allow escape to close Help About box.
 *              Feb 23, 1996  6.00  BP   Properly update when toolbar turned on
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 14, 1996  6.10  BP   Added configuration menu option.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>

#include "windows.h"
#include "commctrl.h"

#include "OpenDoor.h"
#include "ODRes.h"
#include "ODFrame.h"
#include "ODGen.h"
#include "ODScrn.h"
#include "ODKrnl.h"

#ifdef ODPLAT_WIN32

/* Frame window information structure. */
typedef struct
{
   HINSTANCE hInstance;
   BOOL bToolbarOn;
   HWND hwndToolbar;
   BOOL bStatusBarOn;
   HWND hwndStatusBar;
   HWND hwndTimeEdit;
   HWND hwndTimeUpDown;
   BOOL bWantsChatIndicator;
   HACCEL hacclFrameCommands;
   HWND hwndMessageWindow;
   char *pszCurrentMessage;
   int nCurrentMessageFlags;
} tODFrameWindowInfo;

/* Toolbar button information. */
TBBUTTON atbButtons[] =
{
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {0,   ID_DOOR_CHATMODE,          TBSTATE_ENABLED,  TBSTYLE_BUTTON, 0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {1,   ID_DOOR_USERKEYBOARDOFF,   TBSTATE_ENABLED,  TBSTYLE_BUTTON, 0, 0},
   {2,   ID_DOOR_SYSOPNEXT,         TBSTATE_ENABLED,  TBSTYLE_BUTTON, 0, 0},
   {0,   0,                         TBSTATE_ENABLED,  TBSTYLE_SEP,    0, 0},
   {3,   ID_DOOR_HANGUP,            TBSTATE_ENABLED,  TBSTYLE_BUTTON, 0, 0},
   {4,   ID_DOOR_LOCKOUT,           TBSTATE_ENABLED,  TBSTYLE_BUTTON, 0, 0},
   {5,   ID_DOOR_EXIT,              TBSTATE_ENABLED,  TBSTYLE_BUTTON, 0, 0},
};

/* Other toolbar settings. */
#define NUM_TOOLBAR_BITMAPS   6
#define MIN_TIME              0
#define MAX_TIME              1440

/* Pointer to default edit box window procedure. */
WNDPROC pfnDefEditProc = NULL;
WNDPROC pfnDefToolbarProc = NULL;

/* Global frame window handle. */
static HWND hwndCurrentFrame;

/* Status bar settings. */
#define NUM_STATUS_PARTS      2
#define NODE_PART_WIDTH       65

/* Child window IDs. */
#define ID_TOOLBAR            1000
#define ID_TIME_EDIT          1001
#define ID_TIME_UPDOWN        1002
#define ID_STATUSBAR          1003


/* Private function prototypes. */
static HWND ODFrameCreateToolbar(HWND hwndParent, HANDLE hInstance,
   tODFrameWindowInfo *pWindowInfo);
static void ODFrameDestroyToolbar(HWND hwndToolbar,
   tODFrameWindowInfo *pWindowInfo);
static HWND ODFrameCreateStatusBar(HWND hwndParent, HANDLE hInstance);
static void ODFrameSetMainStatusText(HWND hwndStatusBar);
static void ODFrameDestroyStatusBar(HWND hwndStatusBar);
static void ODFrameSizeStatusBar(HWND hwndStatusBar);
LRESULT CALLBACK ODFrameWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
   LPARAM lParam);
LRESULT CALLBACK ODFrameToolbarProc(HWND hwnd, UINT uMsg, WPARAM wParam,
   LPARAM lParam);
static void ODFrameUpdateTimeLeft(tODFrameWindowInfo *pWindowInfo);
LRESULT CALLBACK ODFrameTimeEditProc(HWND hwnd, UINT uMsg, WPARAM wParam,
   LPARAM lParam);
BOOL CALLBACK ODFrameAboutDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
   LPARAM lParam);
static HWND ODFrameCreateWindow(HANDLE hInstance);
static void ODFrameDestroyWindow(HWND hwndFrame);
static void ODFrameMessageLoop(HANDLE hInstance, HWND hwndFrame);
DWORD OD_THREAD_FUNC ODFrameThreadProc(void *pParam);
BOOL CALLBACK ODFrameMessageDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
   LPARAM lParam);


/* ----------------------------------------------------------------------------
 * ODFrameCreateWindow()                               *** PRIVATE FUNCTION ***
 *
 * Creates the OpenDoors frame window and its children.
 *
 * Parameters: hInstance   - Handle to application instance.
 *
 *     Return: A handle to the newly created window, or NULL on failure.
 */
static HWND ODFrameCreateWindow(HANDLE hInstance)
{
   HWND hwndFrameWindow = NULL;
   WNDCLASS wcFrameWindow;
   tODFrameWindowInfo *pWindowInfo = NULL;
   tODThreadHandle hScreenThread;
   HKEY hOpenDoorsKey;
   DWORD cbData;

   /* Register the main frame window's window class. */
   memset(&wcFrameWindow, 0, sizeof(wcFrameWindow));
   wcFrameWindow.style = CS_HREDRAW | CS_VREDRAW;
   wcFrameWindow.lpfnWndProc = ODFrameWindowProc;
   wcFrameWindow.cbClsExtra = 0;
   wcFrameWindow.cbWndExtra = 0;
   wcFrameWindow.hInstance = hInstance;
   if(od_control.od_app_icon != NULL)
   {
      wcFrameWindow.hIcon = od_control.od_app_icon;
   }
   else
   {
      wcFrameWindow.hIcon
         = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_OPENDOORS));
   }
   wcFrameWindow.hCursor = LoadCursor(NULL, IDC_ARROW);
   wcFrameWindow.hbrBackground = NULL;
   wcFrameWindow.lpszMenuName = MAKEINTRESOURCE(IDR_FRAME_MENU);
   wcFrameWindow.lpszClassName = "ODFrame";

   RegisterClass(&wcFrameWindow);

   /* Setup window information structure. */
   pWindowInfo = malloc(sizeof(tODFrameWindowInfo));
   if(!pWindowInfo)
   {
      return(NULL);
   }
   pWindowInfo->hInstance = hInstance;
   pWindowInfo->hwndTimeEdit = NULL;
   pWindowInfo->hwndTimeUpDown = NULL;
   pWindowInfo->bWantsChatIndicator = FALSE;
   pWindowInfo->hwndMessageWindow = NULL;

   /* Determine whether or not the toolbar and status bar are on. */
   RegCreateKey(HKEY_CURRENT_USER, "Software\\Pirie\\OpenDoors",
      &hOpenDoorsKey);

   cbData = sizeof(pWindowInfo->bToolbarOn);
   if(RegQueryValueEx(hOpenDoorsKey, "ToolBarOn", NULL, NULL, 
      (LPBYTE)&pWindowInfo->bToolbarOn,
      &cbData) != ERROR_SUCCESS)
   {
      pWindowInfo->bToolbarOn = TRUE;
      RegSetValueEx(hOpenDoorsKey, "ToolBarOn", 0, REG_DWORD,
         (LPBYTE)&pWindowInfo->bToolbarOn,
         sizeof(pWindowInfo->bToolbarOn));
   }

   cbData = sizeof(pWindowInfo->bStatusBarOn);
   if(RegQueryValueEx(hOpenDoorsKey, "StatusBarOn", NULL, NULL, 
      (LPBYTE)&pWindowInfo->bStatusBarOn,
      &cbData) != ERROR_SUCCESS)
   {
      pWindowInfo->bStatusBarOn = TRUE;
      RegSetValueEx(hOpenDoorsKey, "StatusBarOn", 0, REG_DWORD,
         (LPBYTE)&pWindowInfo->bStatusBarOn,
         sizeof(pWindowInfo->bStatusBarOn));
   }

   RegCloseKey(hOpenDoorsKey);

   /* Create the main frame window. */
   if((hwndFrameWindow = CreateWindowEx(
      0L,
      wcFrameWindow.lpszClassName,
      od_control.od_prog_name,
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_BORDER | WS_MINIMIZEBOX,
      CW_USEDEFAULT,
      0,
      0,
      0,
      NULL,
      NULL,
      hInstance,
      pWindowInfo)) == NULL)
   {
      /* On window creation failure, return NULL. */
      return(NULL);
   }

   /* Load accelerator table for the  frame window. */
   pWindowInfo->hacclFrameCommands
      = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_FRAME));

   /* Create the OpenDoors toolbar. On failure, we will continue anyhow. */
   if(pWindowInfo->bToolbarOn)
   {
      pWindowInfo->hwndToolbar =
         ODFrameCreateToolbar(hwndFrameWindow, hInstance, pWindowInfo);
   }

   /* Create the status bar. On failure, we will continue anyhow. */
   if(pWindowInfo->bStatusBarOn)
   {
      pWindowInfo->hwndStatusBar =
         ODFrameCreateStatusBar(hwndFrameWindow, hInstance);
   }

   /* Updates state of the window from whether or not the user has */
   /* requested a chat with the sysop.                             */
   ODFrameUpdateWantChat();

   /* Create the local screen window, which occupies the remaining */
   /* client area of the frame window.                             */
   ODScrnStartWindow(hInstance, &hScreenThread, hwndFrameWindow);

   return(hwndFrameWindow);
}


/* ----------------------------------------------------------------------------
 * ODFrameCreateToolbar()                              *** PRIVATE FUNCTION ***
 *
 * Creates the OpenDoors toolbar.
 *
 * Parameters: hwndParent  - Handle to the parent window.
 *
 *             hInstance   - Handle to the executable file's module instance.
 *
 *             pWindowInfo - Pointer to frame window information structure.
 *
 *     Return: A handle to the toolbar on success, or NULL on failure.
 */
static HWND ODFrameCreateToolbar(HWND hwndParent, HANDLE hInstance,
   tODFrameWindowInfo *pWindowInfo)
{
   HWND hwndToolbar = NULL;
   HWND hwndTimeEdit = NULL;
   HWND hwndTimeUpDown = NULL;
   HWND hwndToolTip;
   BOOL bSuccess = FALSE;

   ASSERT(hwndParent != NULL);
   ASSERT(hInstance != NULL);
   ASSERT(pWindowInfo != NULL);

   /* First, attempt to create the toolbar window. */
   hwndToolbar = CreateToolbarEx(hwndParent,
      WS_CHILD | WS_BORDER | WS_VISIBLE | TBSTYLE_TOOLTIPS,
      ID_TOOLBAR, NUM_TOOLBAR_BITMAPS, hInstance, IDB_TOOLBAR,
      atbButtons, DIM(atbButtons), 0, 0, 0, 0, sizeof(TBBUTTON));

   if(hwndToolbar == NULL)
   {
      goto CleanUp;
   }

   /* Change the window proc for the toolbar window to our own, keeping a */
   /* pointer to the original window proc.                                */
   pfnDefToolbarProc = (WNDPROC)GetWindowLong(hwndToolbar, GWL_WNDPROC);
   SetWindowLong(hwndToolbar, GWL_WNDPROC, (LONG)ODFrameToolbarProc);

   /* Next, create an edit control on the toolbar, to allow the user's */
   /* time remaining online to be adjusted.                            */
   hwndTimeEdit = CreateWindowEx(WS_EX_STATICEDGE, "EDIT", "",
      WS_CHILD | WS_BORDER | WS_VISIBLE | ES_LEFT,
      0, 0, 70, 22, hwndToolbar, (HMENU)ID_TIME_EDIT, hInstance, NULL);

   if(hwndTimeEdit == NULL)
   {
      goto CleanUp;
   }

   /* Now that the edit window has the appropriate parent, we set its */
   /* position accordingly. */
   SetWindowPos(hwndTimeEdit, NULL, 2, 2, 0, 0,
      SWP_NOZORDER | SWP_NOSIZE);

   /* Set font of the edit control to be the standard non-bold font. */
   SendMessage(hwndTimeEdit, WM_SETFONT,
      (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(FALSE, 0));

   /* Change the window proc for the edit window to our own, keeping a */
   /* pointer to the original window proc. */
   pfnDefEditProc = (WNDPROC)GetWindowLong(hwndTimeEdit, GWL_WNDPROC);
   SetWindowLong(hwndTimeEdit, GWL_WNDPROC, (LONG)ODFrameTimeEditProc);

   /* Add the time edit control to the tooltip control. */

   /* Obtain a handle to the toolbar's tooltip control. */
   hwndToolTip = (HWND)SendMessage(hwndToolbar, TB_GETTOOLTIPS, 0, 0);
   if(hwndToolTip)
   {
      TOOLINFO ToolInfo;

      /* Fill TOOLINFO structure. */
      ToolInfo.cbSize = sizeof(ToolInfo);
      ToolInfo.uFlags = TTF_IDISHWND | TTF_CENTERTIP;
      ToolInfo.lpszText = "User's Time Remaining";
      ToolInfo.hwnd = hwndParent;
      ToolInfo.uId = (UINT)hwndTimeEdit;
      ToolInfo.hinst = hInstance;

      /* Setup tooltips for the time edit box. */
      SendMessage(hwndToolTip, TTM_ADDTOOL, 0, (LPARAM)&ToolInfo);
   }

   /* Now, we create an up-down control to buddy with the edit control. */
   hwndTimeUpDown = CreateWindowEx(0L, UPDOWN_CLASS, "",
      WS_CHILD | WS_BORDER | WS_VISIBLE | UDS_ARROWKEYS |
      UDS_ALIGNRIGHT, 0, 0, 8, 8,
         hwndToolbar, (HMENU)ID_TIME_UPDOWN, hInstance, NULL);

   if(hwndTimeUpDown == NULL)
   {
      goto CleanUp;
   }

   /* Set the up-down control's buddy control to be the edit control that */
   /* we just created.                                                    */
   SendMessage(hwndTimeUpDown, UDM_SETBUDDY, (LONG)hwndTimeEdit, 0L);

   /* Set the valid range of values for the edit control. */
   SendMessage(hwndTimeUpDown, UDM_SETRANGE, 0L, MAKELONG(MAX_TIME, MIN_TIME));

   /* Store handles to time limit edit and up-down controls. */
   pWindowInfo->hwndTimeEdit = hwndTimeEdit;
   pWindowInfo->hwndTimeUpDown = hwndTimeUpDown;

   /* Next, we set the default text for the edit control. */
   ODFrameUpdateTimeLeft(pWindowInfo);

   /* Return with success. */
   bSuccess = TRUE;

CleanUp:
   if(!bSuccess)
   {
      /* On failure, free any allocated resources. */
      if(hwndTimeUpDown != NULL)
      {
         DestroyWindow(hwndTimeUpDown);
      }
      if(hwndTimeEdit != NULL)
      {
         DestroyWindow(hwndTimeUpDown);
      }
      if(hwndToolbar != NULL)
      {
         DestroyWindow(hwndToolbar);
         hwndToolbar = NULL;
      }
   }

   /* Return handle to newly created toolbar, or NULL on failure. */
   return(hwndToolbar);
}


/* ----------------------------------------------------------------------------
 * ODFrameDestroyToolbar()                             *** PRIVATE FUNCTION ***
 *
 * Destroys the OpenDoors toolbar.
 *
 * Parameters: hwndToolbar - Handle to previously created toolbar.
 *
 *             pWindowInfo - Pointer to frame window information structure.
 *
 *     Return: void.
 */
static void ODFrameDestroyToolbar(HWND hwndToolbar,
   tODFrameWindowInfo *pWindowInfo)
{
   ASSERT(hwndToolbar != NULL);
   ASSERT(pWindowInfo != NULL);

   /* Destroy the time up-down control, and NULL its handle in the frame */
   /* window information structure.                                      */
   DestroyWindow(pWindowInfo->hwndTimeUpDown);
   pWindowInfo->hwndTimeUpDown = NULL;

   /* Destroy the time edit control, and NULL its handle in the frame window */
   /* information structure.                                                 */
   DestroyWindow(pWindowInfo->hwndTimeEdit);
   pWindowInfo->hwndTimeEdit = NULL;

   /* Now, destroy the toolbar itself. */
   DestroyWindow(hwndToolbar);
}


/* ----------------------------------------------------------------------------
 * ODFrameCreateStatusBar()                            *** PRIVATE FUNCTION ***
 *
 * Creates the OpenDoors status bar.
 *
 * Parameters: hwndParent  - Handle to the parent window.
 *
 *             hInstance   - Handle to the executable file's module instance.
 *
 *     Return: A handle to the status bar on success, or NULL on failure.
 */
static HWND ODFrameCreateStatusBar(HWND hwndParent, HANDLE hInstance)
{
   HWND hwndStatusBar = NULL;
   char szStatusText[20];

   ASSERT(hwndParent != NULL);

   /* Create the status bar window. */
   hwndStatusBar = CreateWindowEx(0L, STATUSCLASSNAME, "",
      WS_CHILD | WS_VISIBLE, 0, 0, 0, 0,
      hwndParent, (HMENU)ID_STATUSBAR, hInstance, NULL);

   if(hwndStatusBar == NULL)
   {
      return(NULL);
   }

   /* Set the size of the status bar parts from the size of the frame */
   /* window.                                                         */
   ODFrameSizeStatusBar(hwndStatusBar);

   /* Add the user's name, location and connection info string. */
   ODFrameSetMainStatusText(hwndStatusBar);

   /* Add the node number string. */
   sprintf(szStatusText, "Node %d", od_control.od_node);
   SendMessage(hwndStatusBar, SB_SETTEXT, (WPARAM)1, (LPARAM)szStatusText);

   return(hwndStatusBar);
}


/* ----------------------------------------------------------------------------
 * ODFrameSetMainStatusText()                          *** PRIVATE FUNCTION ***
 *
 * Updates the text that is displayed in the main pane of the status bar.
 *
 * Parameters: hwndStatusBar - Handle to the status bar.
 *
 *     Return: void.
 */
static void ODFrameSetMainStatusText(HWND hwndStatusBar)
{
   char szStatusText[160];

   ASSERT(hwndStatusBar != NULL);

   /* Generate base status bar text, with the user's name, location and */
   /* connection information.                                           */
   if(od_control.baud == 0)
   {
      sprintf(szStatusText, "%s of %s in local mode",
         od_control.user_name,
         od_control.user_location);
   }
   else
   {
      sprintf(szStatusText, "%s of %s at %ldbps",
         od_control.user_name,
         od_control.user_location,
         od_control.od_connect_speed);
   }

   /* If the user has paged the sysop, then include reason for chat if */
   /* it is available.                                                 */
   if(od_control.user_wantchat && strlen(od_control.user_reasonforchat) > 0)
   {
      strcat(szStatusText, " (Reason for chat: \"");
      strcat(szStatusText, od_control.user_reasonforchat);
      strcat(szStatusText, "\")");
   }

   /* Update status bar text in the main status bar pane with the newly */
   /* generated string.                                                 */
   SendMessage(hwndStatusBar, SB_SETTEXT, (WPARAM)0, (LPARAM)szStatusText);
}


/* ----------------------------------------------------------------------------
 * ODFrameDestroyStatusBar()                           *** PRIVATE FUNCTION ***
 *
 * Destroys the OpenDoors status bar.
 *
 * Parameters: hwndStatusBar    - Handle to previously created status bar.
 *
 *     Return: void.
 */
static void ODFrameDestroyStatusBar(HWND hwndStatusBar)
{
   DestroyWindow(hwndStatusBar);
}


/* ----------------------------------------------------------------------------
 * ODFrameSizeStatusBar()                              *** PRIVATE FUNCTION ***
 *
 * Creates the OpenDoors status bar.
 *
 * Parameters: hwndStatusBar  - Handle to existing status bar window.
 *
 *     Return: void.
 */
static void ODFrameSizeStatusBar(HWND hwndStatusBar)
{
   int anWidths[NUM_STATUS_PARTS];
   int nStatusWidth;
   RECT rcStatusBar;

   /* Determine the total width of the status bar. */
   GetWindowRect(hwndStatusBar, &rcStatusBar);
   nStatusWidth = rcStatusBar.right - rcStatusBar.left;

   /* Calculate the width of the parts from the total width. */
   anWidths[0] = nStatusWidth - NODE_PART_WIDTH;
   anWidths[1] = -1;

   /* Update the status bar part settings. */
   SendMessage(hwndStatusBar, SB_SETPARTS, NUM_STATUS_PARTS,
      (LPARAM)anWidths);
}


/* ----------------------------------------------------------------------------
 * ODFrameGetUsedClientAtTop()
 *
 * Determines height in pixels of the space used at the top of the
 * frame window's client area, by the toolbar, etc.
 *
 * Parameters: hwndFrame - Handle to the OpenDoors frame window.
 *
 *     Return: The height of the used space, in pixels.
 */
INT ODFrameGetUsedClientAtTop(HWND hwndFrame)
{
   tODFrameWindowInfo *pWindowInfo;
   RECT rcWindow;

   pWindowInfo = (tODFrameWindowInfo *)GetWindowLong(hwndFrame, GWL_USERDATA);

   if(!pWindowInfo->bToolbarOn) return(0);

   GetWindowRect(pWindowInfo->hwndToolbar, &rcWindow);

   return(rcWindow.bottom - rcWindow.top - 2);
}


/* ----------------------------------------------------------------------------
 * ODFrameGetUsedClientAtBottom()
 *
 * Determines height in pixels of the space used at the bottom of the
 * frame window's client area, by the status bar, etc.
 *
 * Parameters: hwndFrame - Handle to the OpenDoors frame window.
 *
 *     Return: The height of the used space, in pixels.
 */
INT ODFrameGetUsedClientAtBottom(HWND hwndFrame)
{
   tODFrameWindowInfo *pWindowInfo;
   RECT rcWindow;

   pWindowInfo = (tODFrameWindowInfo *)GetWindowLong(hwndFrame, GWL_USERDATA);

   if(!pWindowInfo->bStatusBarOn) return(0);

   GetWindowRect(pWindowInfo->hwndStatusBar, &rcWindow);

   return(rcWindow.bottom - rcWindow.top - 1);
}


/* ----------------------------------------------------------------------------
 * ODFrameWindowProc()                                 *** PRIVATE FUNCTION ***
 *
 * The OpenDoors frame window proceedure.
 *
 * Parameters: hwnd   - Handle to the OpenDoors frame window.
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
LRESULT CALLBACK ODFrameWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
   LPARAM lParam)
{
   tODFrameWindowInfo *pWindowInfo;

   pWindowInfo = (tODFrameWindowInfo *)GetWindowLong(hwnd, GWL_USERDATA);

   switch(uMsg)
   {
      case WM_CREATE:
      {
         /* At window creation time, store a pointer to the window */
         /* information structure in window's user data.           */
         CREATESTRUCT *pCreateStruct = (CREATESTRUCT *)lParam;
         pWindowInfo = (tODFrameWindowInfo *)pCreateStruct->lpCreateParams;
         SetWindowLong(hwnd, GWL_USERDATA, (LONG)pWindowInfo);

         /* Update the enabled and checked states of frame window commands. */
         ODFrameUpdateCmdUI();

         /* If the client has not provided a help callback function, then */
         /* remove the Contents item from the help menu.                  */
         if(od_control.od_help_callback == NULL)
         {
            RemoveMenu(GetMenu(hwnd), ID_HELP_CONTENTS, MF_BYCOMMAND);
         }

         if(od_control.od_config_callback == NULL)
         {
            RemoveMenu(GetMenu(hwnd), ID_DOOR_CONFIG, MF_BYCOMMAND);
         }
         break;
      }

      case WM_CLOSE:
         /* If door exit has been chosen, confirm with local user. */
         if(MessageBox(hwnd,
            "You are about to terminate this session and return the user to the BBS.\nDo you wish to proceed?",
            od_control.od_prog_name,
            MB_ICONQUESTION | MB_YESNO) == IDYES)
         {
            /* Normal door exit (drop to BBS) is implemented by the */
            /* WM_DESTROY handler.                                  */
            ODFrameDestroyWindow(hwnd);
         }
         break;

      case WM_DESTROY:
         /* If toolbar is on, then it must be destroyed when the frame */
         /* window is destroyed.                                       */
         if(pWindowInfo->bToolbarOn)
         {
            ODFrameDestroyToolbar(GetDlgItem(hwnd, ID_TOOLBAR), pWindowInfo);
         }

         /* If status bar is on, then it must be destroyed when the frame */
         /* window is destroyed.                                          */
         if(pWindowInfo->bStatusBarOn)
         {
            ODFrameDestroyStatusBar(GetDlgItem(hwnd, ID_STATUSBAR));
         }

         /* Now, force OpenDoors to shutdown. */
         ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_DROPTOBBS);

         /* When the frame window is destroyed, it is the window proc's   */
         /* responsiblity to deallocate the window information structure. */
         free(pWindowInfo);
         SetWindowLong(hwnd, GWL_USERDATA, (LONG)NULL);

         /* Reset current frame window handle. */
         hwndCurrentFrame = NULL;
         break;

      case WM_SETFOCUS:
         /* Whenver input focus is set to the frame window, pass the input */
         /* focus on to the screen window, which fills most of our client  */
         /* area.                                                          */
         ODScrnSetFocusToWindow();
         break;

      case WM_TIMER:
         /* If the window flash timer has elapsed, then flash the window. */
         FlashWindow(hwnd, TRUE);
         break;

      case WM_COMMAND:
         /* An OpenDoors-defined command has been selected, so switch on */
         /* the command ID.                                              */
         switch(LOWORD(wParam))
         {
            case ID_HELP_ABOUT:
               /* Display the OpenDoors default about box. */
               DialogBox(pWindowInfo->hInstance, MAKEINTRESOURCE(IDD_ABOUT),
                  hwnd, ODFrameAboutDlgProc);
               break;

            case ID_HELP_CONTENTS:
               /* Call the client's help callback function, if one was */
               /* provided.                                            */
               if(od_control.od_help_callback != NULL)
               {
                  (*od_control.od_help_callback)();
               }
               break;

            case ID_DOOR_CONFIG:
               if(od_control.od_config_callback != NULL)
               {
                  (*od_control.od_config_callback)();
               }
               break;

            case ID_DOOR_EXIT:
               /* On request for normal door exit (drop to BBS), just send  */
               /* a close message to this window. This will prompt to       */
               /* confirm exit, and then shutdown OpenDoors if appropriate. */
               PostMessage(hwnd, WM_CLOSE, 0, 0L);
               break;

            case ID_DOOR_CHATMODE:
               /* If chat mode is currently active, then end it. */
               if(od_control.od_chat_active)
               {
                  ODKrnlEndChatMode();
               }
               /* If chat mode is not currently active, then start it. */
               else
               {
                  ODKrnlStartChatThread(TRUE);
               }
               break;

            case ID_DOOR_USERKEYBOARDOFF:
               /* If user keyboard off command has been chosen, then toggle */
               /* keyboard off mode on or off.                              */
               od_control.od_user_keyboard_on
                  = !od_control.od_user_keyboard_on;

               /* Update the keyboard off menu item and toolbar button. */
               CheckMenuItem(GetMenu(hwnd), ID_DOOR_USERKEYBOARDOFF,
                  MF_BYCOMMAND | (od_control.od_user_keyboard_on
                  ? MF_UNCHECKED : MF_CHECKED));
               SendMessage(GetDlgItem(hwnd, ID_TOOLBAR), TB_CHECKBUTTON,
                  ID_DOOR_USERKEYBOARDOFF,
                  MAKELONG(!od_control.od_user_keyboard_on, 0));
               break;

            case ID_DOOR_SYSOPNEXT:
               /* If sysop next command has been chosen, then toggle the */
               /* sysop next flag on or off.                             */
               od_control.sysop_next = !od_control.sysop_next;

               /* Update the sysop next menu item and toolbar button. */
               CheckMenuItem(GetMenu(hwnd), ID_DOOR_SYSOPNEXT, MF_BYCOMMAND |
                  (od_control.sysop_next ? MF_CHECKED : MF_UNCHECKED));
               SendMessage(GetDlgItem(hwnd, ID_TOOLBAR), TB_CHECKBUTTON,
                  ID_DOOR_SYSOPNEXT, MAKELONG(od_control.sysop_next, 0));
               break;

            case ID_DOOR_HANGUP:
               /* If hangup command has been chosen, then confirm with the */
               /* local user.                                              */
               if(MessageBox(hwnd,
                  "You are about to disconnect this user. Do you wish to proceed?",
                  od_control.od_prog_name,
                  MB_ICONQUESTION | MB_YESNO) == IDYES)
               {
                  ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_HANGUP);
               }
               break;

            case ID_DOOR_LOCKOUT:
               /* If lockout command has been chosen, the confirm with the */
               /* local user.                                              */
               if(MessageBox(hwnd,
                  "You are about to lock out this user. Do you wish to proceed?",
                  od_control.od_prog_name,
                  MB_ICONQUESTION | MB_YESNO) == IDYES)
               {
                  /* Set the user's access security level to 0. */
                  od_control.user_security = 0;

                  ODKrnlForceOpenDoorsShutdown(ERRORLEVEL_HANGUP);
               }
               break;

            case ID_VIEW_TOOL_BAR:
            {
               HKEY hOpenDoorsKey;

               /* If toolbar on/off command has been chosen ... */
               if(pWindowInfo->bToolbarOn)
               {
                  /* If the toolbar is on, then turn it off. */
                  ODFrameDestroyToolbar(GetDlgItem(hwnd, ID_TOOLBAR),
                     pWindowInfo);
                  pWindowInfo->bToolbarOn = FALSE;
                  CheckMenuItem(GetMenu(hwnd), ID_VIEW_TOOL_BAR,
                     MF_BYCOMMAND | MF_UNCHECKED);
               }
               else
               {
                  /* If the toolbar is off, then turn it on. */
                  pWindowInfo->hwndToolbar = ODFrameCreateToolbar(hwnd,
                     pWindowInfo->hInstance, pWindowInfo);
                  pWindowInfo->bToolbarOn = TRUE;
                  CheckMenuItem(GetMenu(hwnd), ID_VIEW_TOOL_BAR,
                     MF_BYCOMMAND | MF_CHECKED);
                  ODFrameUpdateCmdUI();
               }

               /* Adjust window sizes accordingly. */
               ODScrnAdjustWindows();

               /* Update the toolbar setting in the registry. */
               RegCreateKey(HKEY_CURRENT_USER, "Software\\Pirie\\OpenDoors",
                  &hOpenDoorsKey);
               RegSetValueEx(hOpenDoorsKey, "ToolBarOn", 0, REG_DWORD,
                  (LPBYTE)&pWindowInfo->bToolbarOn,
                  sizeof(pWindowInfo->bToolbarOn));
               RegCloseKey(hOpenDoorsKey);
               break;
            }

            case ID_VIEW_STAT_BAR:
            {
               HKEY hOpenDoorsKey;

               /* If the status bar on/off command has been chosen ... */
               if(pWindowInfo->bStatusBarOn)
               {
                  /* If the status bar is on, then turn it off. */
                  pWindowInfo->bStatusBarOn = FALSE;
                  CheckMenuItem(GetMenu(hwnd), ID_VIEW_STAT_BAR,
                     MF_BYCOMMAND | MF_UNCHECKED);
                  ODFrameDestroyStatusBar(GetDlgItem(hwnd, ID_STATUSBAR));
               }
               else
               {
                  /* If the status bar is off, then turn it on. */
                  pWindowInfo->bStatusBarOn = TRUE;
                  CheckMenuItem(GetMenu(hwnd), ID_VIEW_STAT_BAR,
                     MF_BYCOMMAND | MF_CHECKED);
                  pWindowInfo->hwndStatusBar =
                     ODFrameCreateStatusBar(hwnd, pWindowInfo->hInstance);
               }

               /* Adjust window sizes accordingly. */
               ODScrnAdjustWindows();

               /* Update the status bar setting in the registry. */
               RegCreateKey(HKEY_CURRENT_USER, "Software\\Pirie\\OpenDoors",
                  &hOpenDoorsKey);
               RegSetValueEx(hOpenDoorsKey, "StatusBarOn", 0, REG_DWORD,
                  (LPBYTE)&pWindowInfo->bStatusBarOn,
                  sizeof(pWindowInfo->bStatusBarOn));
               RegCloseKey(hOpenDoorsKey);
               break;
            }

            case ID_USER_ADDONEMINUTE:
               /* If add one minute command has been chosen, then        */
               /* increment the user's time, up to the maximum allowable */
               /* time.                                                  */
               if(od_control.user_timelimit < MAX_TIME)
               {
                  od_control.user_timelimit++;
                  ODFrameUpdateTimeLeft(pWindowInfo);
               }
               break;

            case ID_USER_ADDFIVEMINUTES:
               /* If add five minutes command has been chosen, then */
               /* adjust the user's time accordingly.               */
               od_control.user_timelimit =
                  MIN(od_control.user_timelimit + 5, MAX_TIME);
               ODFrameUpdateTimeLeft(pWindowInfo);
               break;

            case ID_USER_SUBTRACTONEMINUTE:
               /* If subtract one minute command has been chosen, then */
               /* adjust the user's time accordingly.                  */
               if(od_control.user_timelimit > MIN_TIME)
               {
                  od_control.user_timelimit--;
                  ODFrameUpdateTimeLeft(pWindowInfo);
               }
               break;

            case ID_USER_SUBTRACTFIVEMINUTES:
               /* If the subtract five mintues command has been chosen, */
               /* then adjust the user's time accordingly.              */
               od_control.user_timelimit =
                  MAX(od_control.user_timelimit - 5, MIN_TIME);
               ODFrameUpdateTimeLeft(pWindowInfo);
               break;

            case ID_USER_INACTIVITYTIMER:
               /* If the user inactivity timer command has been chosen, */
               /* then toggle the timer on or off.                      */
               od_control.od_disable_inactivity =
                  !od_control.od_disable_inactivity;
               CheckMenuItem(GetMenu(hwnd), ID_USER_INACTIVITYTIMER,
                  MF_BYCOMMAND | (od_control.od_disable_inactivity ?
                  MF_UNCHECKED : MF_CHECKED));
               break;

            case ID_TIME_EDIT:
            {
               /* If the user's time remaining has been directly edited, */
               /* then adjust the time limit accordingly.                */
               if(HIWORD(wParam) == EN_CHANGE)
               {
                  char szTimeText[40];
                  GetWindowText((HWND)lParam, szTimeText, sizeof(szTimeText));
                  od_control.user_timelimit = atoi(szTimeText);

                  /* Do not allow the time limit to fall outside of the */
                  /* valid range.                                       */
                  od_control.user_timelimit =
                     MAX(MIN_TIME, od_control.user_timelimit);
                  od_control.user_timelimit =
                     MIN(MAX_TIME, od_control.user_timelimit);

                  /* Update the position of the up-down control. */
                  SendMessage(pWindowInfo->hwndTimeUpDown, UDM_SETPOS, 0,
                     (LPARAM)MAKELONG(od_control.user_timelimit, 0));
               }
            }

            default:
               return(TRUE);
         }
         return(FALSE);

      case WM_NOTIFY:
         /* A control parent notification message has been sent. */
         switch(((LPNMHDR)lParam)->code)
         {
            case TTN_NEEDTEXT:
            {
               /* This is the message from the tool tip control, requesting */
               /* the appropriate string to display for the current toolbar */
               /* item.                                                     */
               LPTOOLTIPTEXT lpToolTipText = (LPTOOLTIPTEXT)lParam;
               switch(lpToolTipText->hdr.idFrom)
               {
                  case ID_DOOR_EXIT:
                     lpToolTipText->lpszText = "Exit To BBS";
                     break;
                  case ID_DOOR_CHATMODE:
                     lpToolTipText->lpszText = "Chat Mode";
                     break;
                  case ID_DOOR_USERKEYBOARDOFF:
                     lpToolTipText->lpszText = "User Keyboard Off";
                     break;
                  case ID_DOOR_SYSOPNEXT:
                     lpToolTipText->lpszText = "Sysop Next";
                     break;
                  case ID_DOOR_HANGUP:
                     lpToolTipText->lpszText = "Hangup";
                     break;
                  case ID_DOOR_LOCKOUT:
                     lpToolTipText->lpszText = "Lockout";
                     break;
               }
               break;
            }
         }
         break;

      case WM_VSCROLL:
         /* A scrolling action has taken place. */

         /* If it is the time limit up-down control that has scrolled. */
         if((HWND)lParam == pWindowInfo->hwndTimeUpDown)
         {
            int nPos = HIWORD(wParam);

            /* Adjust the user's time limit. */
            od_control.user_timelimit = MAX(MIN(nPos, MAX_TIME), MIN_TIME);

            /* Update the time left displayed in the edit box. */
            ODFrameUpdateTimeLeft(pWindowInfo);
         }
         break;

      case WM_SIZE:
         /* The OpenDoors frame window has been resized, so its contents */
         /* must now be resized accordingly.                             */

         /* Pass the message on to the status bar window, so that it will */
         /* automatically adjust its own position and overall size.       */
         SendMessage(GetDlgItem(hwnd, ID_STATUSBAR), WM_SIZE, wParam, lParam);

         /* Now, adjust the size of each part of the status bar. */
         ODFrameSizeStatusBar(GetDlgItem(hwnd, ID_STATUSBAR));

         /* Pass the message on to the toolbar, so that it will resize */
         /* iteself.                                                   */
         SendMessage(GetDlgItem(hwnd, ID_TOOLBAR), WM_SIZE, wParam, lParam);
         break;

      case WM_MENUSELECT:
         /* If the user has selected an item on the menu, then we should */
         /* update the status bar accordingly.                           */
         if(HIWORD(wParam) == 0xFFFF)
         {
            /* If menu is being exited, then turn off the status bar simple */
            /* mode.                                                        */
            HWND hwndStatusBar = GetDlgItem(hwnd, ID_STATUSBAR);

            SendMessage(hwndStatusBar, SB_SIMPLE, (WPARAM)FALSE, 0L);
         }
         else
         {
            char szCommandString[160] = "";
            HWND hwndStatusBar = GetDlgItem(hwnd, ID_STATUSBAR);

            /* A new menu item is being selected. */

            /* If this item is on the system menu, then provide the strings */
            /* for any of those menu items.                                 */
            if(HIWORD(wParam) & MF_SYSMENU)
            {
               switch(LOWORD(wParam))
               {
                  case SC_SIZE:
                     strcpy(szCommandString,
                        "Resizes this window.");
                     break;
                  case SC_MOVE:
                     strcpy(szCommandString,
                        "Moves this window.");
                     break;
                  case SC_MINIMIZE:
                     strcpy(szCommandString,
                        "Collapses this window to an icon.");
                     break;
                  case SC_MAXIMIZE:
                     strcpy(szCommandString,
                        "Expands this window to fill the screen.");
                     break;
                  case SC_CLOSE:
                     strcpy(szCommandString,
                        "Closes this window, and returns the user to the BBS.");
                     break;
                  case SC_RESTORE:
                     strcpy(szCommandString,
                        "Restores this window to normal size.");
                     break;
                  case SC_TASKLIST:
                     strcpy(szCommandString,
                        "");
                     break;
               }
            }
            else
            {
               /* If this item is on the window menu provided by OpenDoors, */
               /* then load the status bar string for this command ID.      */
               LoadString(pWindowInfo->hInstance, LOWORD(wParam),
                  szCommandString, sizeof(szCommandString));
            }

            /* Switch the status bar into simple (single-paned) mode. */
            SendMessage(hwndStatusBar, SB_SIMPLE, (WPARAM)TRUE, 0L);

            /* Set the text for the status bar. */
            SendMessage(hwndStatusBar, SB_SETTEXT, (WPARAM)255 | SBT_NOBORDERS,
               (LPARAM)szCommandString);
         }
         return(DefWindowProc(hwnd, uMsg, wParam, lParam));

      case WM_SHOW_MESSAGE:
         if(pWindowInfo->hwndMessageWindow == NULL)
         {
            pWindowInfo->pszCurrentMessage = (char *)lParam;
            pWindowInfo->nCurrentMessageFlags = (int)wParam;

            /* Create the message window. */
            DialogBoxParam(pWindowInfo->hInstance,
               MAKEINTRESOURCE(IDD_MESSAGE), hwnd, ODFrameMessageDlgProc,
               (LPARAM)pWindowInfo);
         }
         break;

      case WM_REMOVE_MESSAGE:
         if(pWindowInfo->hwndMessageWindow != NULL)
         {
            PostMessage(pWindowInfo->hwndMessageWindow, WM_COMMAND,
               MAKELONG(IDOK, 0), 0L);
            pWindowInfo->hwndMessageWindow = NULL;
         }
         break;

      default:
         /* Pass messages that we don't explicitly handle on to the */
         /* default window proc.                                    */
         return(DefWindowProc(hwnd, uMsg, wParam, lParam));
   }

   return(0);
}


/* ----------------------------------------------------------------------------
 * ODFrameUpdateCmdUI()
 *
 * Updates the enabled and checked state of OpenDoors commands that may change.
 *
 * Parameters: None.
 *
 *     Return: void.
 */
void ODFrameUpdateCmdUI(void)
{
   HWND hwndFrame = hwndCurrentFrame;
   HMENU hMenu = GetMenu(hwndFrame);
   HWND hwndToolbar = GetDlgItem(hwndFrame, ID_TOOLBAR);
   tODFrameWindowInfo *pWindowInfo;

   if(hwndFrame == NULL) return;

   /* Obtain window information structure. */
   pWindowInfo = (tODFrameWindowInfo *)GetWindowLong(hwndFrame, GWL_USERDATA);
   if(pWindowInfo == NULL) return;

   /* Check or uncheck the toolbar and status bar menu items. */
   CheckMenuItem(hMenu, ID_VIEW_TOOL_BAR, MF_BYCOMMAND |
      (pWindowInfo->bToolbarOn ? MF_CHECKED : MF_UNCHECKED));
   CheckMenuItem(hMenu, ID_VIEW_STAT_BAR, MF_BYCOMMAND |
      (pWindowInfo->bStatusBarOn ? MF_CHECKED : MF_UNCHECKED));

   /* Check or uncheck the inactivity timer menu item. */
   CheckMenuItem(hMenu, ID_USER_INACTIVITYTIMER, MF_BYCOMMAND |
      (od_control.od_disable_inactivity ? MF_UNCHECKED : MF_CHECKED));

   /* Check or uncheck the sysop next menu item and toolbar button. */
   CheckMenuItem(hMenu, ID_DOOR_SYSOPNEXT, MF_BYCOMMAND |
      (od_control.sysop_next ? MF_CHECKED : MF_UNCHECKED));
   SendMessage(hwndToolbar, TB_CHECKBUTTON,
      ID_DOOR_SYSOPNEXT, MAKELONG(od_control.sysop_next, 0));

   /* Check or uncheck the keyboard off menu item and toolbar button. */
   CheckMenuItem(hMenu, ID_DOOR_USERKEYBOARDOFF, MF_BYCOMMAND |
      (od_control.od_user_keyboard_on ? MF_UNCHECKED : MF_CHECKED));
   SendMessage(hwndToolbar, TB_CHECKBUTTON,
      ID_DOOR_USERKEYBOARDOFF,
      MAKELONG(!od_control.od_user_keyboard_on, 0));

   /* Update the chat mode menu item and toolbar button. */
   CheckMenuItem(hMenu, ID_DOOR_CHATMODE, MF_BYCOMMAND |
      (od_control.od_chat_active ? MF_CHECKED : MF_UNCHECKED));
   SendMessage(hwndToolbar, TB_CHECKBUTTON, ID_DOOR_CHATMODE,
      MAKELONG(od_control.od_chat_active, 0));
}


/* ----------------------------------------------------------------------------
 * ODFrameUpdateTimeDisplay()
 *
 * Updates the remaining time online that is displayed anywhere by the frame
 * window. Uses ODFrameUpdateTimeLeft().
 *
 * Parameters: None.
 *
 *     Return: void.
 */
void ODFrameUpdateTimeDisplay(void)
{
   tODFrameWindowInfo *pWindowInfo;

   /* If there is no current frame window, then return without doing */
   /* anything.                                                      */
   if(hwndCurrentFrame == NULL) return;

   pWindowInfo = (tODFrameWindowInfo *)GetWindowLong(hwndCurrentFrame,
      GWL_USERDATA);
   ASSERT(pWindowInfo != NULL);

   ODFrameUpdateTimeLeft(pWindowInfo);
}


/* ----------------------------------------------------------------------------
 * ODFrameUpdateWantChat()
 *
 * Updates the state of the flashing wants-chat indicator on the frame window.
 *
 * Parameters: None.
 *
 *     Return: void.
 */
void ODFrameUpdateWantChat(void)
{
   tODFrameWindowInfo *pWindowInfo;

   /* If there is no current frame window, then return without doing */
   /* anything.                                                      */
   if(hwndCurrentFrame == NULL) return;

   pWindowInfo = (tODFrameWindowInfo *)GetWindowLong(hwndCurrentFrame,
      GWL_USERDATA);
   ASSERT(pWindowInfo != NULL);

   /* If the status bar is on, then update the text displayed in the */
   /* status bar's main pane.                                        */
   if(pWindowInfo->bStatusBarOn)
   {
      ODFrameSetMainStatusText(pWindowInfo->hwndStatusBar);
   }

   /* Toggle the state of the wants-chat indicator, of needed. */
   if(pWindowInfo->bWantsChatIndicator && !od_control.user_wantchat)
   {
      /* Restore original window text. */
      SetWindowText(hwndCurrentFrame, od_control.od_prog_name);

      /* Restore the window flash to its original state. */
      FlashWindow(hwndCurrentFrame, FALSE);

      /* Destroy the Windows timer. */
      KillTimer(hwndCurrentFrame, 1);

      /* Record that wants chat indicator is now off. */
      pWindowInfo->bWantsChatIndicator = FALSE;
   }
   else if (!pWindowInfo->bWantsChatIndicator && od_control.user_wantchat)
   {
      /* Set window title to include the wants chat indicator. */
      char szNewWindowTitle[sizeof(od_control.od_prog_name) + 20];
      sprintf(szNewWindowTitle, "%s - User Wants Chat",
         od_control.od_prog_name);
      SetWindowText(hwndCurrentFrame, szNewWindowTitle);

      /* Start the flashing the window. */
      SetTimer(hwndCurrentFrame, 1,
         GetCaretBlinkTime(), NULL);

      /* Record that wants chat indicator is now on. */
      pWindowInfo->bWantsChatIndicator = TRUE;
   }
}


/* ----------------------------------------------------------------------------
 * ODFrameDestroyWindow()                              *** PRIVATE FUNCTION ***
 *
 * Destroys the OpenDoors frame window and its children.
 *
 * Parameters: hwndFrame   - Handle to the window previously created by
 *                           ODFrameCreateWindow().
 *
 *     Return: void.
 */
static void ODFrameDestroyWindow(HWND hwndFrame)
{
   tODFrameWindowInfo *pWindowInfo;

   ASSERT(hwndFrame != NULL);

   /* Obtain a pointer to the frame window information structure. */
   pWindowInfo = (tODFrameWindowInfo *)GetWindowLong(hwndFrame, GWL_USERDATA);

   /* At this point, deallocate the accelerator table. */
   if(pWindowInfo->hacclFrameCommands != NULL)
   {
      DestroyAcceleratorTable(pWindowInfo->hacclFrameCommands);
   }

   /* Destroying the main frame window will automatically cause its */
   /* children to be destroyed.                                     */
   DestroyWindow(hwndFrame);
}


/* ----------------------------------------------------------------------------
 * ODFrameToolbarProc()                                *** PRIVATE FUNCTION ***
 *
 * The toolbar window proceedure.
 *
 * Parameters: hwnd   - Handle to the toolbar window.
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
LRESULT CALLBACK ODFrameToolbarProc(HWND hwnd, UINT uMsg, WPARAM wParam,
   LPARAM lParam)
{
   switch(uMsg)
   {
      /* Forward needed message to the main frame window proceedure. */
      case WM_VSCROLL:
      case WM_COMMAND:
         SendMessage(GetParent(hwnd), uMsg, wParam, lParam);
         break;
   }

   /* Pass all messages on to the default toolbar window proceedure. */
   return(CallWindowProc(pfnDefToolbarProc, hwnd, uMsg, wParam, lParam));
}


/* ----------------------------------------------------------------------------
 * ODFrameUpdateTimeLeft()                             *** PRIVATE FUNCTION ***
 *
 * Updates the displayed time remaining from od_control.user_timelimit.
 *
 * Parameters: pWindowInfo - Pointer to frame window information structure.
 *
 *     Return: void.
 */
static void ODFrameUpdateTimeLeft(tODFrameWindowInfo *pWindowInfo)
{
   char szTimeLeft[12];
   RECT rcWindow;

   if(pWindowInfo->hwndTimeEdit == NULL)
   {
      /* If the time limit edit control does not exist (i.e., if the       */
      /* toolbar is not currently on), then there is nothing for us to do. */
      return;
   }

   /* Generate the string to be displayed in the edit control. */
   sprintf(szTimeLeft, "%d min.", od_control.user_timelimit);

   /* Set the edit control's text to the new string. */
   SetWindowText(pWindowInfo->hwndTimeEdit, szTimeLeft);

   /* Force edit control to be redrawn. (Except for rightmost pixel */
   /* column.)                                                      */
   GetWindowRect(pWindowInfo->hwndTimeEdit, &rcWindow);
   rcWindow.right--;
   InvalidateRect(pWindowInfo->hwndTimeEdit, &rcWindow, TRUE);

   /* Set the position of the up-down control to match. */
   SendMessage(pWindowInfo->hwndTimeUpDown, UDM_SETPOS, 0,
      (LPARAM)MAKELONG(od_control.user_timelimit, 0));
}


/* ----------------------------------------------------------------------------
 * ODFrameTimeEditProc()                               *** PRIVATE FUNCTION ***
 *
 * The time edit window proceedure. Relays mouse messages from the edit box
 * to the tooltip control, and then passes all messages on to the standard
 * edit box window proceedure.
 *
 * Parameters: hwnd   - Handle to the time edit window.
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
LRESULT CALLBACK ODFrameTimeEditProc(HWND hwnd, UINT uMsg, WPARAM wParam,
   LPARAM lParam)
{
   switch(uMsg)
   {
      case WM_MOUSEMOVE:
      case WM_LBUTTONDOWN:
      case WM_LBUTTONUP:
      {
         MSG msg;
         HWND hwndToolTip;

         /* Setup message structure. */
         msg.lParam = lParam;
         msg.wParam = wParam;
         msg.message = uMsg;
         msg.hwnd = hwnd;

         /* Obtain handle to the tooltip window. */
         hwndToolTip = (HWND)SendMessage(GetParent(hwnd), TB_GETTOOLTIPS,
            0, 0);

         /* Relay the message to the tooltip window. */
         SendMessage(hwndToolTip, TTM_RELAYEVENT, 0, (LPARAM)(LPMSG)&msg);

         break;
      }
   }

   /* Pass all messages on to the default edit box window proceedure. */
   return(CallWindowProc(pfnDefEditProc, hwnd, uMsg, wParam, lParam));
}


/* ----------------------------------------------------------------------------
 * ODFrameAboutDlgProc()
 *
 * DialogProc for the OpenDoors default Help About dialog box.
 *
 * Parameters: hwndDlg  - Window handle to the dialog box.
 *
 *             uMsg     - Message ID.
 *
 *             wParam   - First message parameter.
 *
 *             lParam   - Second message parameter.
 *
 *     Return: TRUE if message is processed, FALSE otherwise.
 */
BOOL CALLBACK ODFrameAboutDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
   LPARAM lParam)
{
   switch(uMsg)
   {
      case WM_INITDIALOG:
         /* At dialog box creation time, update the text in the about      */
         /* box with any information provided by the OpenDoors programmer. */

         /* If a program name has been provided, then display it. */
         if(strcmp(od_control.od_prog_name, OD_VER_SHORTNAME) != 0)
         {
            SetWindowText(GetDlgItem(hwndDlg, IDC_DOORNAME),
               od_control.od_prog_name);
         }

         /* If copyright information has been provided, then display it. */
         if(strlen(od_control.od_prog_copyright) > 0)
         {
            SetWindowText(GetDlgItem(hwndDlg, IDC_COPYRIGHT),
               od_control.od_prog_copyright);
         }

         /* If program version information has been provided, then display */
         /* it.                                                            */
         if(strlen(od_control.od_prog_version) > 0)
         {
            SetWindowText(GetDlgItem(hwndDlg, IDC_VERSION),
               od_control.od_prog_version);
         }

         /* Center the about dialog box in the area occupied by the */
         /* main frame window.                                      */
         ODFrameCenterWindowInParent(hwndDlg);

         return(TRUE);

      case WM_COMMAND:
         /* If a command has been chosen. */
         switch(LOWORD(wParam))
         {
            case IDCANCEL:
            case IDOK:
               /* If the OK button has been pressed, then close the dialog. */
               EndDialog(hwndDlg, IDOK);
               break;
         }
         return(TRUE);

      default:
         /* Otherwise, indicate that this message has not been processed. */
         return(FALSE);
   }
}


/* ----------------------------------------------------------------------------
 * ODFrameCenterWindowInParent()                       *** PRIVATE FUNCTION ***
 *
 * Repositions the specified window so that it is centered in its parent.
 *
 * Parameters: hwndChild - The window to reposition.
 *
 *     Return: void.
 */
void ODFrameCenterWindowInParent(HWND hwndChild)
{
   HWND hwndParent;
   RECT rcParent;
   RECT rcChild;
   INT nChildWidth;
   INT nChildHeight;
   INT nParentWidth;
   INT nParentHeight;

   ASSERT(hwndChild != NULL);

   /* Obtain a handle to the parent window. */
   hwndParent = GetParent(hwndChild);
   ASSERT(hwndParent != NULL);

   /* Obtain the bounding boxes of both windows. */
   GetWindowRect(hwndChild, &rcChild);
   GetWindowRect(hwndParent, &rcParent);
   
   /* Determine the height and width of both windows. */
   nChildWidth = rcChild.right - rcChild.left;
   nChildHeight = rcChild.bottom - rcChild.top;
   nParentWidth = rcParent.right - rcParent.left;
   nParentHeight = rcParent.bottom - rcParent.top;

   /* Move the child to the center of the parent. */
   SetWindowPos(hwndChild, NULL,
      rcParent.left + (nParentWidth - nChildWidth) / 2,
      rcParent.top + (nParentHeight - nChildHeight) / 2,
      0, 0, SWP_NOSIZE | SWP_NOZORDER);
}


/* ----------------------------------------------------------------------------
 * ODFrameMessageLoop()                                *** PRIVATE FUNCTION ***
 *
 * Message loop for OpenDoors local UI thread (frame window handling).
 *
 * Parameters: hInstance   - Handle to current instance.
 *
 *             hwndFrame   - Handle to the frame window.
 *
 *     Return: void.
 */
static void ODFrameMessageLoop(HANDLE hInstance, HWND hwndFrame)
{
   MSG msg;

   ASSERT(hInstance != NULL);
   ASSERT(hwndFrame != NULL);

   /* Loop, fetching, translating and dispatching messages for any windows */
   /* created by this thread. (GetMessage() blocks when no messages are    */
   /* available.)                                                          */
   while(GetMessage(&msg, NULL, 0, 0))
   {
      if(!ODFrameTranslateAccelerator(hwndFrame, &msg))
      {
         TranslateMessage(&msg);
         DispatchMessage(&msg);
      }
   }
}


/* ----------------------------------------------------------------------------
 * ODFrameTranslateAccelerator()
 *
 * Translates WM_KEYDOWN or WM_SYSKEYDOWN messages to frame window commands,
 * if needed, based on the frame window's accelerator table.
 *
 * Parameters: hwndFrame  - Handle to the OpenDoors main frame window.
 *
 *             pMsg       - Pointer to the message that may require
 *                          translation.
 *
 *     Return: TRUE if message was translated, FALSE if not.
 */
BOOL ODFrameTranslateAccelerator(HWND hwndFrame, LPMSG pMsg)
{
   tODFrameWindowInfo *pWindowInfo;

   ASSERT(hwndFrame != NULL);
   ASSERT(pMsg != NULL);

   /* Obtain a pointer to the frame window information structure. */
   pWindowInfo = (tODFrameWindowInfo *)GetWindowLong(hwndFrame, GWL_USERDATA);
   ASSERT(pWindowInfo != NULL);

   /* Perform accelerator translation, based on the frame window's */
   /* accelerator table, sending any resulting WM_COMMAND messages */
   /* to the frame window.                                         */
   return(TranslateAccelerator(hwndFrame, pWindowInfo->hacclFrameCommands,
      pMsg) != 0);
}


/* ----------------------------------------------------------------------------
 * ODFrameThreadProc()                                 *** PRIVATE FUNCTION ***
 *
 * Function that execute the OpenDoors frame window thread.
 *
 * Parameters: pParam   - The thread parameter, which must be the handle to the
 *                        current application instance.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
DWORD OD_THREAD_FUNC ODFrameThreadProc(void *pParam)
{
   HWND hwndFrame;
   HANDLE hInstance = (HANDLE)pParam;

   /* Create the frame window. */
   hwndFrame = ODFrameCreateWindow(hInstance);

   if(hwndFrame == NULL)
   {
      return(FALSE);
   }

   /* Store a pointer to the frame window. */
   hwndCurrentFrame = hwndFrame;

   /* Loop, processing messages for the frame window. */
   ODFrameMessageLoop(hInstance, hwndFrame);

   /* Destroy the frame window. */
   ODFrameDestroyWindow(hwndFrame);

   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODFrameStart()
 *
 * Function that starts up the frame window.
 *
 * Parameters: hInstance     - Handle to the current application instance.
 *
 *             phFrameThread - Pointer to the frame thread handle.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODFrameStart(HANDLE hInstance, tODThreadHandle *phFrameThread)
{
   return(ODThreadCreate(phFrameThread, ODFrameThreadProc,
      (void *)hInstance));
}


/* ----------------------------------------------------------------------------
 * ODFrameMessageDlgProc()
 *
 * Dialog proceedure for the OpenDoors message window.
 *
 * Parameters: hwndDlg  - Window handle to the dialog box.
 *
 *             uMsg     - Message ID.
 *
 *             wParam   - First message parameter.
 *
 *             lParam   - Second message parameter.
 *
 *     Return: TRUE if message is processed, FALSE otherwise.
 */
BOOL CALLBACK ODFrameMessageDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
   LPARAM lParam)
{
   switch(uMsg)
   {
      case WM_INITDIALOG:
      {
         tODFrameWindowInfo *pWindowInfo;

         pWindowInfo = (tODFrameWindowInfo *)lParam;

         ASSERT(pWindowInfo != NULL);

         pWindowInfo->hwndMessageWindow = hwndDlg;
         
         /* Set the message window title. */
         SetWindowText(hwndDlg, od_control.od_prog_name);

         /* Change the text displayed in the message window. */
         SetWindowText(GetDlgItem(hwndDlg, IDC_MESSAGE_TEXT1),
            (char *)pWindowInfo->pszCurrentMessage);

         /* Center window in parent window. */
         ODFrameCenterWindowInParent(hwndDlg);

         return(FALSE);
      }

      case WM_COMMAND:
         /* If a command has been chosen. */
         switch(LOWORD(wParam))
         {
            case IDOK:
               /* If the OK button has been pressed, then close the dialog. */
               EndDialog(hwndDlg, IDOK);
               break;
         }
         return(TRUE);

      default:
         /* Indicate that this message has not been processed. */
         return(FALSE);
   }

   return(TRUE);
}


#endif /* ODPLAT_WIN32 */
