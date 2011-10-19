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
 *        File: ODEmu.c
 *
 * Description: Code for the TTY/ANSI/AVATAR terminal emulation routines,
 *              including .ASC/.ANS/.AVT/.RIP file display functions.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Oct 21, 1994  6.00  BP   Further isolated com routines.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Dec 31, 1994  6.00  BP   Remove #ifndef USEINLINE DOS code.
 *              Jun 07, 1995  6.00  BP   Added od_emu_simulate_modem.
 *              Jul 18, 1995  6.00  BP   Fixed warning in call to _ulongdiv().
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 15, 1995  6.00  BP   Terminal emulation speed optimization.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 24, 1995  6.00  BP   Use od_connect_speed for modem sim.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 21, 1996  6.00  BP   Use ODScrnShowMessage().
 *              Jan 09, 1996  6.00  BP   ODComOutbound() returns actual size.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 06, 1996  6.10  BP   Prevent TC generated N_LXMUL@ call.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Oct 18, 2001  6.11  MD   Added od_send_file_section()
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

#include "OpenDoor.h"
#include "ODStr.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODCom.h"
#include "ODTypes.h"
#include "ODScrn.h"
#include "ODKrnl.h"
#include "ODUtil.h"


/* Manifest constants. */
#define MODEM_SIMULATOR_TICK 54L

#define LEVEL_NONE      0
#define LEVEL_ASCII     1
#define LEVEL_ANSI      2
#define LEVEL_AVATAR    3
#define LEVEL_RIP       4


/* Local helper function prototypes. */
static void ODEmulateFromBuffer(char *pszBuffer, BOOL bRemoteEcho);
static FILE *ODEmulateFindCompatFile(const char *pszBaseName, INT *pnLevel);
static void ODEmulateFillArea(BYTE btLeft, BYTE btTop, BYTE btRight,
   BYTE btBottom, char chToFillWith);


/* Current terminal emulator state variables. */
static BYTE btANSISeqLevel = 0;
static INT anANSIParams[10];
static char szCurrentParam[4] = "";
static BYTE btCurrentParamLength;
static BYTE btSavedColumn=1;
static BYTE btSavedRow = 1;
static char szToRepeat[129];
static BYTE btRepeatCount;
static BYTE btAvatarSeqLevel = 0;
static char chPrevParam;
static BYTE btNumParams;
static BYTE btDefaultAttrib = 7;
static BOOL bAvatarInsertMode = FALSE;
static INT8 btScrollLines;
static BYTE btScrollLeft, btScrollTop, btScrollRight, btScrollBottom;

/* Variables for tracking hotkeys while displaying a menu file. */
static char *pszCurrentHotkeys=NULL;
static char chHotkeyPressed;

/* Lookup table to map colors from ANSI values to PC color values. */
static BYTE abtANSIToPCColorTable[8] = {0, 4, 2, 6, 1, 5, 3, 7};


/* ----------------------------------------------------------------------------
 * od_hotkey_menu()
 *
 * Displays a .ASC/.ANS/.AVT/.RIP file, just as od_send_file() does. However,
 * unlike od_send_file(), od_hotkey_menu() also allows checks for keypresses
 * by the user. If any of the hotkeys listed in pszHotKeys are pressed while
 * the file is being displayed, or after the file has finished being displayed,
 * od_hotkey_menu() will return this value to the caller. This allows menu
 * screens to be easily displayed from on-disk files, while permitting the user
 * to choose a menu item at any time.
 *
 * Parameters: pszFileName  - Pointer to the filename to display.
 *
 *             pszHotKeys   - Pointer to a string listing the valid keys.
 *                            Keys are not case sensitive.
 *
 *             bWait        - If TRUE, od_hotkey_menu() will not return until
 *                            the user presses one of the valid keys. If FALSE,
 *                            then od_hotkey_menu() will return as soon as it
 *                            has finished displaying the file, even if the
 *                            user has not pressed any key;.
 *
 *     Return: The key pressed by the user, or '\0' if no key was pressed.
 */
ODAPIDEF char ODCALL od_hotkey_menu(char *pszFileName, char *pszHotKeys,
   BOOL bWait)
{
   char chPressed;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_hotkey_menu()");

   /* Ensure that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   if(!pszHotKeys)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return('\0');
   }

   /* Store pointer to string of hotkeys in global hotkey array for access */
   /* from od_send_file(). */
   pszCurrentHotkeys = (char *)pszHotKeys;

   /* Clear the hotkey status variable. */
   chHotkeyPressed = '\0';

   /* Display the menu file using od_send_file() primitive. */
   if(!od_send_file(pszFileName))
   {
      OD_API_EXIT();
      return('\0');
   }

   /* Clear the global hotkey array. */
   pszCurrentHotkeys = NULL;

   /* If the file display was interrupted by the pressing of one of the */
   /* hotkeys, return the pressed hotkey immediately.                   */
   if(chHotkeyPressed != '\0')
   {
      OD_API_EXIT();
      return(chHotkeyPressed);
   }

   /* If no hotkey has been pressed key, and the wait flag has been set, */
   /* wait for the user to press a valid hotkey.                         */
   if(bWait)
   {
      /* Wait for a valid hotkey using the od_get_answer() primitive. */
      chPressed = od_get_answer(pszHotKeys);

      /* If a remote user is connected on this node. */
      if(od_control.baud)
      {
         /* Clear the outbound buffer. */
         ODComClearOutbound(hSerialPort);
      }

      /* Return the hotkey pressed by the user. */
      OD_API_EXIT();
      return(chPressed);
   }

   /* No hotkey has been pressed, so return 0.                          */
   /* (Since 0 is used to terminate the string of valid hotkeys, it can */
   /* never be a valid hotkey itself, and is therefore a safe value to  */
   /* indicate the "no key press" state.)                               */
   OD_API_EXIT();
   return(0);
}


/* ----------------------------------------------------------------------------
 * od_send_file()
 *
 * Displays a .ASC/.ANS/.AVT/.RIP file to the local and remote screens. If
 * OpenDoors is unable to display the required file format locally, an
 * equivalent file that is locally displayable is selected. If no such
 * equivalent file can be found, then a message box is displayed, indicating
 * that the file is being transmitted to the remote system.
 *
 * Parameters: pszFileName - The name of the file to send. This parameter may
 *                           explicitly specify the full filename and
 *                           extension, or the extension may be omitted. In the
 *                           case that the extension is omitted, this function
 *                           automatically selects the appropriate file type
 *                           for the current display mode (RIP, ANSI, etc.).
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_send_file(const char *pszFileName)
{
   FILE *pfRemoteFile;
   FILE *pfLocalFile = NULL;
   BOOL bAnythingLocal = TRUE;
   void *pWindow;
   INT nFileLevel;
   BYTE btCount;
   BOOL bPausing;
   char chKey;
   char *pchParsing;
   char szMessage[74];

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_send_file()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   if(!pszFileName)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Initialize local variables. */
   btCount = 2;

   /* Turn on page pausing, if available. */
   bPausing = od_control.od_page_pausing;

   /* If operating in auto-filename mode (no extension specified). */
   if(strchr(pszFileName, '.') == NULL)
   {                                
      /* Begin by searching for a .RIP file. */
      nFileLevel = LEVEL_RIP;

      /* If no .ASC/.ANS/.AVT/.RIP file. */      
      if((pfRemoteFile = ODEmulateFindCompatFile(pszFileName, &nFileLevel))
         == NULL)
      {
         /* Then return with an error. */
         od_control.od_error = ERR_FILEOPEN;
         OD_API_EXIT();
         return(FALSE);
      }

      /* If the file found was a .RIP. */
      if(nFileLevel == LEVEL_RIP)
      {
         /* Search for file to display locally. */
         nFileLevel = LEVEL_AVATAR;

         /* No page pausing with .RIP display. */
         bPausing = FALSE;

         if((pfLocalFile = ODEmulateFindCompatFile(pszFileName, &nFileLevel))
            == NULL)
         {
            /* If there is no further .ASC/.ANS/.AVT/.RIP files, then no */
            /* local display.                                            */
            bAnythingLocal = FALSE;
         }
      }
      else if(nFileLevel == LEVEL_NONE)
      {
         od_control.od_error = ERR_FILEOPEN;
         OD_API_EXIT();
         return(FALSE);
      }

      /* Get filename of remote file. */
      strcpy(szODWorkString, pszFileName);
      strcat(szODWorkString, ".rip");
      strupr(szODWorkString);
   }
   else
   {
      /* If the full filename was specified, then attempt to open that file. */
      if((pfRemoteFile = fopen(pszFileName,"rb")) == NULL)
      {
         /* If unable to open file, then return. */
         od_control.od_error = ERR_FILEOPEN;
         OD_API_EXIT();
         return(FALSE);
      }

      strcpy(szODWorkString, pszFileName);
      strupr(szODWorkString);

      if(strstr(szODWorkString, ".rip"))
      {
         /* No page pausing during .RIP display. */
         bPausing = FALSE;

         /* Disable local display. */
         bAnythingLocal = FALSE;
      }
   }

   /* Set default colour to grey on black. */
   btDefaultAttrib = 0x07;

   /* Reset all terminal emulation. */
   btAvatarSeqLevel = 0;
   btANSISeqLevel = 0;

   /* Turn off AVATAR insert mode. */
   bAvatarInsertMode = FALSE;

   /* Reset [S]top/[P]ause control key status. */
   chLastControlKey = 0;

   if(!bAnythingLocal)
   {
      strcpy(szODWorkString, od_control.od_sending_rip);
      strcat(szODWorkString, pszFileName);
      ODStringCopy(szMessage, szODWorkString, sizeof(szMessage));

      pWindow = ODScrnShowMessage(szMessage, 0);
   }

   /* Loop to display each line in the file(s) with page pausing, etc. */
   for(;;)
   {
      /* Call the OpenDoors kernel routine. */
      CALL_KERNEL_IF_NEEDED();

      /* If hotkeys are active. */
      if(pszCurrentHotkeys != NULL)
      {
         /* If a key is waiting in buffer. */
         while((chKey = (char)tolower(od_get_key(FALSE))) != 0)
         {
            /* Check for key in hotkey string. */
            pchParsing = (char *)pszCurrentHotkeys;
            while(*pchParsing)
            {
               /* If key is found. */
               if(tolower(*pchParsing) == chKey)
               {
                  /* Return, indicating that hotkey was pressed. */
                  chHotkeyPressed = *pchParsing;
                  goto abort_send;
               }
               ++pchParsing;
            }
         }
      }

      /* If a control key has been pressed. */
      if(chLastControlKey)
      {
         switch(chLastControlKey)
         {
            /* If it was a stop control key. */
            case 's':
               if(od_control.od_list_stop)
               {
                  /* If enabled, clear keyboard buffer. */
abort_send:
                  /* If operating in remote mode. */
                  if(od_control.baud)
                  {
                     /* Clear the outbound FOSSIL buffer. */
                     ODComClearOutbound(hSerialPort);
                  }

                  /* Return from function. */
                  goto end_transmission;
               }
               break;

            /* If control key was "pause". */
            case 'p':
               /* If pause is enabled. */
               if(od_control.od_list_pause)
               {
                  /* Clear keyboard buffer. */
                  od_clear_keybuffer();

                  /* Wait for any keypress. */
                  od_get_key(TRUE);
               }
         }

         /* Clear control key status. */
         chLastControlKey = 0;
      }

      /* Get next line, if any. */
      if(fgets(szODWorkString, OD_GLOBAL_WORK_STRING_SIZE-1, pfRemoteFile) == NULL)
      {
         /* If different local file. */
         if(pfLocalFile)
         {
            /* Display rest of it. */
            while(fgets(szODWorkString, OD_GLOBAL_WORK_STRING_SIZE-1, pfLocalFile))
            {
                /* Pass each line to terminal emulator. */
               ODEmulateFromBuffer(szODWorkString, FALSE);
            }
         }

         /* Return from od_send_file(). */
         goto end_transmission;
      }

      /* Set parsepos = last char in globworkstr. */
      pchParsing = (char *)&szODWorkString;
      while(*++pchParsing) ;
      --pchParsing;

      /* Check for end of page state. */
      if((*pchParsing == '\r' || *pchParsing == '\n') &&
         ++btCount >= od_control.user_screen_length && bPausing)
      {
         /* Display page pause prompt. */
         if(ODPagePrompt(&bPausing))
         {
            /* If user chose to abort, then return from od_send_file(). */
            goto end_transmission;
         }

         /* Reset line count. */
         btCount = 2;
      }


      /* If the file is also to be displayed locally. */
      if(bAnythingLocal)
      {
         /* If the local file is different from the remote file, then obtain */
         /* the next line from the local file.                               */
         if(pfLocalFile)
         {
            od_disp(szODWorkString, strlen(szODWorkString), FALSE);

            if(fgets(szODWorkString, OD_GLOBAL_WORK_STRING_SIZE-1, pfLocalFile) == NULL)
            {
               while(fgets(szODWorkString, OD_GLOBAL_WORK_STRING_SIZE-1, pfRemoteFile))
               {
                  od_disp(szODWorkString, strlen(szODWorkString), FALSE);
               }

               /* Return from od_send_file(). */
               goto end_transmission;
            }

            ODEmulateFromBuffer(szODWorkString, FALSE);
         }
         else
         {
            /* Pass the string through the local terminal emulation */
            /* system, and send a copy to the remote system.        */
            if(od_control.od_no_ra_codes)
            {
               ODEmulateFromBuffer(szODWorkString, FALSE);
               od_disp(szODWorkString, strlen(szODWorkString), FALSE);
            }
            else
            {
               ODEmulateFromBuffer(szODWorkString,TRUE);
            }
         }
      }
      else
      {
         /* If the file is not being displayed locally, then just send the */
         /* entire line to the remote system (if any).                     */
         od_disp(szODWorkString,strlen(szODWorkString),FALSE);
      }
   }

end_transmission:

   /* Close remote file. */
   fclose(pfRemoteFile);

   /* If there is a different local file, then close it too. */
   if(pfLocalFile)
   {
      fclose(pfLocalFile);
   }

   /* If we are not displaying anything on the local system. */
   if(!bAnythingLocal)
   {
      /* Wait while file is being sent. */
      if(od_control.baud != 0)
      {
         int nOutboundSize;
         do
         {
            CALL_KERNEL_IF_NEEDED();
            ODComOutbound(hSerialPort, &nOutboundSize);
         } while(nOutboundSize != 0);
      }

      /* Get rid of the window. */
      ODScrnRemoveMessage(pWindow);
   }

   OD_API_EXIT();
   return(TRUE);
}



/* ----------------------------------------------------------------------------
 * od_send_file_section()
 *
 * Displays a .ASC/.ANS/.AVT/.RIP multi-section file to the local and remote
 * screens.  If OpenDoors is unable to display the required file format locally,
 * an equivalent file that is locally displayable is selected. If no such
 * equivalent file can be found, then a message box is displayed, incdicating
 * that the file is being transmitted to the remote system.
 *
 * Note: This function works virtually identical to od_send_file() except it
 *       checks for the section headers (if found).
 *
 * Parameters: pszFileName - The name of the file to send. This parameter may
 *                           explicitly specify the full filename and
 *                           extension, or the extension may be omitted. In the
 *                           case that the extension is omitted, this function
 *                           automatically selects the appropriate file type
 *                           for the current display mode (RIP, ANSI, etc.).
 *             pszSectionname - Name of the section in which to send. This 
 *                              parameter must include only the section name 
 *                              and not the @# delimiter.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_send_file_section(char *pszFileName, char *pszSectionName)
{
   FILE *pfRemoteFile;
   FILE *pfLocalFile = NULL;
   BOOL bAnythingLocal = TRUE;
   void *pWindow;
   INT nFileLevel;
   BYTE btCount;
   BOOL bPausing;
   char chKey;
   char *pchParsing;
   char szMessage[74];
   char szFullSectionName[256];
   BOOL bSectionFound = FALSE;
   UINT uSectionNameLength;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_send_file_section()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   if(!pszFileName || !pszSectionName)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Initialize local variables. */
   btCount = 2;

   /* Turn on page pausing, if available. */
   bPausing = od_control.od_page_pausing;

   /* If operating in auto-filename mode (no extension specified). */
   if(strchr(pszFileName, '.') == NULL)
   {                                
      /* Begin by searching for a .RIP file. */
      nFileLevel = LEVEL_RIP;

      /* If no .ASC/.ANS/.AVT/.RIP file. */      
      if((pfRemoteFile = ODEmulateFindCompatFile(pszFileName, &nFileLevel))
         == NULL)
      {
         /* Then return with an error. */
         od_control.od_error = ERR_FILEOPEN;
         OD_API_EXIT();
         return(FALSE);
      }

      /* If the file found was a .RIP. */
      if(nFileLevel == LEVEL_RIP)
      {
         /* Search for file to display locally. */
         nFileLevel = LEVEL_AVATAR;

         /* No page pausing with .RIP display. */
         bPausing = FALSE;

         if((pfLocalFile = ODEmulateFindCompatFile(pszFileName, &nFileLevel))
            == NULL)
         {
            /* If there is no further .ASC/.ANS/.AVT/.RIP files, then no */
            /* local display.                                            */
            bAnythingLocal = FALSE;
         }
      }
      else if(nFileLevel == LEVEL_NONE)
      {
         od_control.od_error = ERR_FILEOPEN;
         OD_API_EXIT();
         return(FALSE);
      }

      /* Get filename of remote file. */
      strcpy(szODWorkString, pszFileName);
      strcat(szODWorkString, ".rip");
      strupr(szODWorkString);
   }
   else
   {
      /* If the full filename was specified, then attempt to open that file. */
      if((pfRemoteFile = fopen(pszFileName,"rb")) == NULL)
      {
         /* If unable to open file, then return. */
         od_control.od_error = ERR_FILEOPEN;
         OD_API_EXIT();
         return(FALSE);
      }

      strcpy(szODWorkString, pszFileName);
      strupr(szODWorkString);

      if(strstr(szODWorkString, ".rip"))
      {
         /* No page pausing during .RIP display. */
         bPausing = FALSE;

         /* Disable local display. */
         bAnythingLocal = FALSE;
      }
   }

   /* Set default colour to grey on black. */
   btDefaultAttrib = 0x07;

   /* Reset all terminal emulation. */
   btAvatarSeqLevel = 0;
   btANSISeqLevel = 0;

   /* Turn off AVATAR insert mode. */
   bAvatarInsertMode = FALSE;

   /* Reset [S]top/[P]ause control key status. */
   chLastControlKey = 0;

   if(!bAnythingLocal)
   {
      strcpy(szODWorkString, od_control.od_sending_rip);
      strcat(szODWorkString, pszFileName);
      ODStringCopy(szMessage, szODWorkString, sizeof(szMessage));

      pWindow = ODScrnShowMessage(szMessage, 0);
   }

   /* Create section name information */
   strcpy(szFullSectionName, "@#");
   strncat(szFullSectionName, pszSectionName, 254);

   /* Get the length of the section name in it's full form */
   uSectionNameLength = strlen(szFullSectionName); 

   /* Loop to display each line in the file(s) with page pausing, etc. */
   for(;;)
   {
      /* Call the OpenDoors kernel routine. */
      CALL_KERNEL_IF_NEEDED();

      /* If hotkeys are active. */
      if(pszCurrentHotkeys != NULL)
      {
         /* If a key is waiting in buffer. */
         while((chKey = (char)tolower(od_get_key(FALSE))) != 0)
         {
            /* Check for key in hotkey string. */
            pchParsing = (char *)pszCurrentHotkeys;
            while(*pchParsing)
            {
               /* If key is found. */
               if(tolower(*pchParsing) == chKey)
               {
                  /* Return, indicating that hotkey was pressed. */
                  chHotkeyPressed = *pchParsing;
                  goto abort_send;
               }
               ++pchParsing;
            }
         }
      }

      /* If a control key has been pressed. */
      if(chLastControlKey)
      {
         switch(chLastControlKey)
         {
            /* If it was a stop control key. */
            case 's':
               if(od_control.od_list_stop)
               {
                  /* If enabled, clear keyboard buffer. */
abort_send:
                  /* If operating in remote mode. */
                  if(od_control.baud)
                  {
                     /* Clear the outbound FOSSIL buffer. */
                     ODComClearOutbound(hSerialPort);
                  }

                  /* Return from function. */
                  goto end_transmission;
               }
               break;

            /* If control key was "pause". */
            case 'p':
               /* If pause is enabled. */
               if(od_control.od_list_pause)
               {
                  /* Clear keyboard buffer. */
                  od_clear_keybuffer();

                  /* Wait for any keypress. */
                  od_get_key(TRUE);
               }
         }

         /* Clear control key status. */
         chLastControlKey = 0;
      }

      /* Get next line, if any. */
      if(fgets(szODWorkString, OD_GLOBAL_WORK_STRING_SIZE-1, pfRemoteFile) == NULL)
      {
         /* If different local file. */
         if(pfLocalFile)
         {
            /* Display rest of it. */
            while(fgets(szODWorkString, OD_GLOBAL_WORK_STRING_SIZE-1, pfLocalFile))
            {
               if (!bSectionFound && strncmp(szFullSectionName, szODWorkString, uSectionNameLength) == 0) 
               {
                  /* Section Found, allow all lines up to EOF or new section to be displayed */
                  bSectionFound = TRUE;
                  /* Read next line, ignore the section line */
                  continue;
               } 
               else if (!bSectionFound) 
               {
                  /* Section not found yet, continue loop */
                  continue;
               } 
               else if (bSectionFound && strncmp(szODWorkString, "@#", 2) == 0) 
               {
                  /* New Section Intercepted */
                  goto end_transmission;
               }
                /* Pass each line to terminal emulator. */
               ODEmulateFromBuffer(szODWorkString, FALSE);
            }
         }

         /* Return from od_send_file(). */
         goto end_transmission;
      }

      /* Check for section header @# + pszSectionName */
      if (!bSectionFound && strncmp(szFullSectionName, szODWorkString, uSectionNameLength) == 0) 
      {
         /* Flag the section as found */
         bSectionFound = TRUE;
         /* Read next line, ignore section line */
         continue;
      } 
      else if (!bSectionFound) 
      {
         /* Section hasn't been found yet */
         continue;
      } 
      else if (bSectionFound && strncmp(szODWorkString, "@#", 2) == 0) 
      {
         /* New section found, terminate send */
         goto end_transmission;
      }

      /* Set parsepos = last char in globworkstr. */
      pchParsing = (char *)&szODWorkString;
      while(*++pchParsing) ;
      --pchParsing;

      /* Check for end of page state. */
      if((*pchParsing == '\r' || *pchParsing == '\n') &&
         ++btCount >= od_control.user_screen_length && bPausing)
      {
         /* Display page pause prompt. */
         if(ODPagePrompt(&bPausing))
         {
            /* If user chose to abort, then return from od_send_file(). */
            goto end_transmission;
         }

         /* Reset line count. */
         btCount = 2;
      }


      /* If the file is also to be displayed locally. */
      if(bAnythingLocal)
      {
         /* If the local file is different from the remote file, then obtain */
         /* the next line from the local file.                               */
         if(pfLocalFile)
         {
            od_disp(szODWorkString, strlen(szODWorkString), FALSE);

            if(fgets(szODWorkString, OD_GLOBAL_WORK_STRING_SIZE-1, pfLocalFile) == NULL)
            {
               while(fgets(szODWorkString, OD_GLOBAL_WORK_STRING_SIZE-1, pfRemoteFile))
               {
                  od_disp(szODWorkString, strlen(szODWorkString), FALSE);
               }

               /* Return from od_send_file(). */
               goto end_transmission;
            }

            ODEmulateFromBuffer(szODWorkString, FALSE);
         }
         else
         {
            /* Pass the string through the local terminal emulation */
            /* system, and send a copy to the remote system.        */
            if(od_control.od_no_ra_codes)
            {
               ODEmulateFromBuffer(szODWorkString, FALSE);
               od_disp(szODWorkString, strlen(szODWorkString), FALSE);
            }
            else
            {
               ODEmulateFromBuffer(szODWorkString,TRUE);
            }
         }
      }
      else
      {
         /* If the file is not being displayed locally, then just send the */
         /* entire line to the remote system (if any).                     */
         od_disp(szODWorkString,strlen(szODWorkString),FALSE);
      }
   }

end_transmission:

   /* Close remote file. */
   fclose(pfRemoteFile);

   /* If there is a different local file, then close it too. */
   if(pfLocalFile)
   {
      fclose(pfLocalFile);
   }

   /* If we are not displaying anything on the local system. */
   if(!bAnythingLocal)
   {
      /* Wait while file is being sent. */
      if(od_control.baud != 0)
      {
         int nOutboundSize;
         do
         {
            CALL_KERNEL_IF_NEEDED();
            ODComOutbound(hSerialPort, &nOutboundSize);
         } while(nOutboundSize != 0);
      }

      /* Get rid of the window. */
      ODScrnRemoveMessage(pWindow);
   }
   
   /* If the section was not found, return FALSE for error */
   if (!bSectionFound)
   {
      OD_API_EXIT();
      return (FALSE);   
   }

   OD_API_EXIT();
   return(TRUE);
}



/* ----------------------------------------------------------------------------
 * ODEmulateFindCompatFile()                           *** PRIVATE FUNCTION ***
 *
 * Searches for an .ASC/.ANS/.AVT/.RIP file that is compatible with the
 * specified display capabilities level.
 *
 * Parameters: pszBaseName - Base filename to use.
 *
 *             pnLevel     - Highest file level to search for. This value is
 *                           updated to indicate the type of file found.
 *
 *     Return: A pointer to a now-open file of the required type, or NULL if
 *             no match was found.
 */
static FILE *ODEmulateFindCompatFile(const char *pszBaseName, INT *pnLevel)
{
   FILE *pfCompatibleFile;

   ASSERT(pszBaseName != NULL);
   ASSERT(pnLevel != NULL);
   ASSERT(*pnLevel >= 0 && *pnLevel <= 4);

   /* Loop though .RIP/.AVT/.ANS/.ASC extensions. */
   for(;*pnLevel > LEVEL_NONE; --*pnLevel)
   {
      /* Get base-filename passed in. */
      strcpy(szODWorkString, pszBaseName);

      /* Try current extension. */
      switch(*pnLevel)
      {
         case LEVEL_RIP:
            if(!od_control.user_rip) continue;
            strcat(szODWorkString, ".rip");
            break;

         case LEVEL_AVATAR:
            if(!od_control.user_avatar) continue;
            strcat(szODWorkString, ".avt");
            break;

         case LEVEL_ANSI:
            if(!od_control.user_ansi) continue;
            strcat(szODWorkString, ".ans");
            break;

         case LEVEL_ASCII:
            strcat(szODWorkString, ".asc");
            break;
      }

      /* If we are able to open this file, then return a pointer to */
      /* the file.                                                  */
      if((pfCompatibleFile = fopen(szODWorkString,"rb")) != NULL)
         return(pfCompatibleFile);
   }

   /* Return NULL if no file was found. */
   return(NULL);
}


/* ----------------------------------------------------------------------------
 * od_disp_emu()
 *
 * Sends an entire string to both local and remote systems. The characters
 * displayed locally are fed through the local terminal emulation sub-system,
 * allowing aribtrary ANSI/AVATAR control sequences to be displayed both
 * locally and remotely.
 *
 * Parameters: pszToDisplay - Pointer to string to display.
 *
 *             bRemoteEcho  - TRUE if characters should also be transmitted
 *                            to the remote system.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_disp_emu(const char *pszToDisplay, BOOL bRemoteEcho)
{
   BOOL bTranslateRemote;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_disp_emu()");

   /* Ensure that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Send pszToDisplay to remote system. */
   if(bRemoteEcho)
   {
      if(od_control.od_no_ra_codes)
      {
         od_disp(pszToDisplay, strlen(pszToDisplay), FALSE);
         bTranslateRemote = FALSE;
      }
      else
      {
         bTranslateRemote = TRUE;
      }
   }
   else
   {
      bTranslateRemote = FALSE;
   }

   /* Pass string to be emulated to local terminal emulation function. */
   ODEmulateFromBuffer(pszToDisplay, bTranslateRemote);

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * od_emulate()
 *
 * Sends a single character to both local and remote systems. The characters
 * displayed locally are fed through the local terminal emulation sub-system,
 * allowing aribtrary ANSII/AVATAR control sequences to be displayed both
 * locally and remotely.
 *
 * Parameters: chToEmulate - Character to feed through the terminal emulator.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_emulate(char chToEmulate)
{
   static char szBuffer[2];

   *szBuffer = chToEmulate;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_emulate()");

   /* Ensure that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Pass character to be emulated to local terminal emulation function. */
   ODEmulateFromBuffer(szBuffer, TRUE);

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * ODEmulateFromBuffer()                               *** PRIVATE FUNCTION ***
 *
 * Displays a string on the local screen, interpreting any terminal emulation
 * control sequences. The string may also be sent to the remote system at the
 * same time.
 *
 * Parameters: pszBuffer   - Pointer to the string to transmit.
 *
 *             bRemoteEcho - TRUE if string should also be sent to the remote
 *                           system, FALSE if it should not be.
 *
 *     Return: void
 */
static void ODEmulateFromBuffer(char *pszBuffer, BOOL bRemoteEcho)
{
   char chCurrent;
   static tODScrnTextInfo TextInfo;
   INT nTemp;
   BOOL bEchoThisChar;
   INT nCharsPerTick;
   INT nCharsThisTick;
   tODTimer ModemSimTimer;

   ASSERT(pszBuffer != NULL);

   /* If we should simulate modem transmission speed. */
   if(od_control.od_emu_simulate_modem)
   {
      DWORD lCharsPerSecond;

      /* Determine character per second rate to simulate. */
      if(od_control.baud == 0)
      {
         lCharsPerSecond = 960L;
      }
      else
      {
         ODDWordDivide(&lCharsPerSecond, (DWORD *)NULL,
            od_control.od_connect_speed, 10L);
      }

      /* Determine number of characters to send per timer tick. */
      lCharsPerSecond = ODDWordMultiply(lCharsPerSecond, MODEM_SIMULATOR_TICK);
      ODDWordDivide(&lCharsPerSecond, (DWORD *)NULL, lCharsPerSecond, 1000L);
      nCharsPerTick = (INT)lCharsPerSecond;

      /* Start tick timer. */
      ODTimerStart(&ModemSimTimer, MODEM_SIMULATOR_TICK);

      /* Reset number of characters that we have sent during this tick. */
      nCharsThisTick = 0;
   }

   while(*pszBuffer)
   {
      /* Read the next character from the buffer into local variable for */
      /* access speed efficiency.                                        */
      chCurrent = *pszBuffer;

      /* If we should simulate modem transmission speed. */
      if(od_control.od_emu_simulate_modem)
      {
         /* If we have now sent all of the characters that should be sent */
         /* during this tick.                                             */
         if(nCharsThisTick++ >= nCharsPerTick)
         {
            /* Wait for timer to elapse. */
            ODTimerWaitForElapse(&ModemSimTimer);

            /* Restart tick timer. */
            ODTimerStart(&ModemSimTimer, MODEM_SIMULATOR_TICK);

            /* Reset characters sent in this tick. */
            nCharsThisTick = 0;
         }
      }

      bEchoThisChar = bRemoteEcho;

      /* Switch according to ANSI emulator state. */
      switch(btANSISeqLevel)
      {
         /* If we are not in the middle of an ANSI command string. */
         case 0:
            /* Switch according to current character. */
            switch(chCurrent)
            {
               /* If this is the escape character. */
               case 27:
                  /* If we are not in the middle of an AVATAR sequence. */
                  if(btAvatarSeqLevel == 0)
                  {
                     /* Then treat this as the start an ANSI sequence. */
                     btANSISeqLevel = 1;
                     break;
                  }

                  /* Deliberate fallthrough to default case. */

               /* If not start of an ANSI sequence. */
               default:
                  /* Check our position in AVATAR sequence. */
                  switch(btAvatarSeqLevel)
                  {
                     /* If not in middle of an AVATAR command. */
                     case 0:
                        /* Check the character we've been sent. */
                        switch(chCurrent)
                        {
                           /* Check for QBBS/RA pause for keypress code. */
                           case 0x01:
                              if(od_control.od_no_ra_codes)
                              {
                                 goto output_next_char;
                              }

                              /* Wait for user to press [ENTER] key. */
                              od_get_answer("\n\r");
                              bEchoThisChar = FALSE;
                              break;

                           /* QBBS/RA ^F user parameters. */
                           case 0x06:
                              if(od_control.od_no_ra_codes)
                              {
                                 goto output_next_char;
                              }
                              btAvatarSeqLevel = 21;
                              bEchoThisChar = FALSE;
                              break;

                           /* QBBS/RA ^K user parameters. */
                           case 0x0b:
                              if(od_control.od_no_ra_codes)
                              {
                                 goto output_next_char;
                              }
                              btAvatarSeqLevel = 22;
                              bEchoThisChar = FALSE;
                              break;

                           case 0x0c:
                              bAvatarInsertMode = FALSE;
                              ODScrnSetAttribute((BYTE)(
                                 od_control.od_cur_attrib = btDefaultAttrib));
                              ODScrnClear();
                              break;

                           case 0x19:
                              bAvatarInsertMode = FALSE;
                              btAvatarSeqLevel = 1;
                              break;

                           case 0x16:   /* ^V */
                              btAvatarSeqLevel = 3;
                              break;

                           default:
   output_next_char:
                              /* Output next character. */
                              if(bAvatarInsertMode)
                              {
                                 ODScrnGetTextInfo(&TextInfo);
                                 if(TextInfo.curx < 80)
                                 {
                                    ODScrnCopyText(TextInfo.curx,
                                       TextInfo.cury, 79, TextInfo.cury,
                                       (BYTE)(TextInfo.curx + 1),
                                       TextInfo.cury);
                                 }
                                 ODScrnDisplayChar(chCurrent);
                              }

                              else
                              {
                                 ODScrnDisplayChar(chCurrent);
                              }
                        }
                        break;

                     case 1:
                        bAvatarInsertMode = FALSE;
                        chPrevParam = chCurrent;
                        btAvatarSeqLevel = 2;
                        break;

                     case 2:
                        for(nTemp = 0; nTemp < (INT)chCurrent;
                           ++nTemp)
                        {
                           ODScrnDisplayChar(chPrevParam);
                        }
                        btAvatarSeqLevel = 0;
                        break;

                     case 3:
                        switch(chCurrent)
                        {
                           case 0x01:
                              bAvatarInsertMode = FALSE;
                              btAvatarSeqLevel = 4;
                              break;

                           case 0x02:
                              bAvatarInsertMode = FALSE;
                              ODScrnGetTextInfo(&TextInfo);
                              ODScrnSetAttribute((BYTE)
                                 (od_control.od_cur_attrib =
                                 TextInfo.attribute | 0x80));
                              btAvatarSeqLevel = 0;
                              break;

                           case 0x03:
                              bAvatarInsertMode = FALSE;
                              ODScrnGetTextInfo(&TextInfo);
                              if(TextInfo.cury > 1)
                              {
                                 ODScrnSetCursorPos(TextInfo.curx,
                                    (BYTE)(TextInfo.cury - 1));
                              }
                              btAvatarSeqLevel = 0;
                              break;

                           case 0x04:
                              bAvatarInsertMode = FALSE;
                              ODScrnGetTextInfo(&TextInfo);
                              if(TextInfo.cury < 25)
                              {
                                 ODScrnSetCursorPos(TextInfo.curx,
                                    (BYTE)(TextInfo.cury + 1));
                              }
                              btAvatarSeqLevel = 0;
                              break;

                           case 0x05:
                              bAvatarInsertMode = FALSE;
                              ODScrnGetTextInfo(&TextInfo);
                              if(TextInfo.curx > 1)
                              {
                                 ODScrnSetCursorPos((BYTE)(TextInfo.curx - 1),
                                    TextInfo.cury);
                              }
                              btAvatarSeqLevel = 0;
                              break;

                           case 0x06:
                              bAvatarInsertMode = FALSE;
                              ODScrnGetTextInfo(&TextInfo);
                              if(TextInfo.curx < 80)
                              {
                                 ODScrnSetCursorPos((BYTE)(TextInfo.curx + 1),
                                    TextInfo.cury);
                              }
                              btAvatarSeqLevel = 0;
                              break;

                           case 0x07:
                              bAvatarInsertMode = FALSE;
                              ODScrnClearToEndOfLine();
                              btAvatarSeqLevel = 0;
                              break;

                           case 0x08:
                              bAvatarInsertMode = FALSE;
                              btAvatarSeqLevel = 5;
                              break;

                           case 0x09:   /* ^I */
                              bAvatarInsertMode = TRUE;
                              btAvatarSeqLevel = 0;
                              break;

                           case 0x0a:   /* ^J */
                              btScrollLines = -1;
                              btAvatarSeqLevel = 7;
                              break;

                           case 0x0b:   /* ^K */
                              btScrollLines = 1;
                              btAvatarSeqLevel = 7;
                              break;

                           case 0x0c:   /* ^L */
                              btAvatarSeqLevel = 14;
                              break;

                           case 0x0d:   /* ^M */
                              btAvatarSeqLevel = 15;
                              break;

                           case 0x0e:   /* ^N */
                              ODScrnGetTextInfo(&TextInfo);
                              if(TextInfo.curx < 80)
                              {
                                 ODScrnCopyText((BYTE)(TextInfo.curx + 1),
                                    TextInfo.cury, 80, TextInfo.cury,
                                    TextInfo.curx, TextInfo.cury);
                              }

                              ODScrnEnableScrolling(FALSE);
                              ODScrnSetCursorPos(80, TextInfo.cury);
                              ODScrnDisplayChar(' ');
                              ODScrnEnableScrolling(TRUE);
                              ODScrnSetCursorPos(TextInfo.curx, TextInfo.cury);

                              btAvatarSeqLevel = 0;
                              break;

                           case 0x19:   /* ^Y */
                              btAvatarSeqLevel = 19;
                              break;

                           default:
                              btAvatarSeqLevel = 0;
                        }
                        break;

                     case 4:
                        ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                           = chCurrent));
                        btAvatarSeqLevel = 0;
                        break;

                     case 5:
                        chPrevParam = chCurrent;
                        btAvatarSeqLevel = 6;
                        break;

                     case 6:
                        ODScrnSetCursorPos(chCurrent, chPrevParam);
                        btAvatarSeqLevel = 0;
                        break;

                     case 7:
                        if(btScrollLines < 1)
                        {
                           btScrollLines = chCurrent;
                        }
                        else
                        {
                           btScrollLines = -chCurrent;
                        }
                        btAvatarSeqLevel = 8;
                        break;

                     case 8:
                        btScrollTop = chCurrent;
                        btAvatarSeqLevel = 9;
                        break;

                     case 9:
                        btScrollLeft = chCurrent;
                        btAvatarSeqLevel = 10;
                        break;

                     case 10:
                        btScrollBottom = chCurrent;
                        btAvatarSeqLevel = 11;
                        break;

                     case 11:
                        btScrollRight = chCurrent;
                        btAvatarSeqLevel = 12;
                        break;

                     case 12:
                        if(btScrollLines == 0
                           || abs(btScrollLines) > 
                           (btScrollBottom - btScrollTop))
                        {
                           ODEmulateFillArea(btScrollLeft, btScrollTop,
                              btScrollRight, btScrollBottom, ' ');
                        }

                        else if(btScrollLines < 0)
                        {
                           ODScrnCopyText(btScrollLeft, btScrollTop,
                              btScrollRight,
                              (BYTE)(btScrollBottom + btScrollLines),
                              btScrollLeft,
                              (BYTE)(btScrollTop - btScrollLines));
                           ODEmulateFillArea(btScrollLeft, btScrollTop,
                              btScrollRight,
                              (BYTE)(btScrollTop - (btScrollLines - 1)), ' ');
                        }

                        else
                        {
                           ODScrnCopyText(btScrollLeft,
                              (BYTE)(btScrollTop + btScrollLines),
                              btScrollRight, btScrollBottom,
                              btScrollLeft, btScrollTop);
                           ODEmulateFillArea(btScrollLeft,
                              (BYTE)(btScrollBottom - (btScrollLines - 1)),
                              btScrollRight, btScrollBottom, ' ');
                        }
                        btAvatarSeqLevel = 0;
                        break;

                     case 14:
                        btScrollLines = (chCurrent & 0x7f);
                        btScrollRight = ' ';
                        btAvatarSeqLevel = 17;
                        break;

                     case 15:
                        btScrollLines = (chCurrent & 0x7f);
                        btAvatarSeqLevel = 16;
                        break;

                     case 16:
                        btScrollRight = chCurrent;
                        btAvatarSeqLevel = 17;
                        break;

                     case 17:
                        btScrollTop = chCurrent;
                        btAvatarSeqLevel = 18;
                        break;

                     case 18:
                        ODScrnGetTextInfo(&TextInfo);
                        ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                           = btScrollLines));
                        ODEmulateFillArea(TextInfo.curx, TextInfo.cury,
                           (BYTE)(TextInfo.curx + chCurrent),
                           (BYTE)(TextInfo.cury + btScrollTop), btScrollRight);
                        btAvatarSeqLevel = 0;
                        break;

                     case 19:
                        btScrollLines = (chCurrent & 0x7f);
                        szToRepeat[btRepeatCount = 0] = '\0';
                        btAvatarSeqLevel = 20;
                        break;

                     case 20:
                        if(btRepeatCount < btScrollLines)
                        {
                           szToRepeat[btRepeatCount] = chCurrent;
                           szToRepeat[++btRepeatCount] = '\0';
                        }
                        else
                        {
                           for(btRepeatCount = 0; btRepeatCount <
                              (BYTE)chCurrent;++btRepeatCount)
                           {
                              ODScrnDisplayString(szToRepeat);
                           }
                           btAvatarSeqLevel = 0;
                        }
                        break;

                     /* RA/QBBS ^F control codes. */
                     case 21:
                        bEchoThisChar = FALSE;
                        switch(chCurrent)
                        {
                           case 'A':
                              od_disp_str(od_control.user_name);
                              break;
                           case 'B':
                              od_disp_str(od_control.user_location);
                              break;
                           case 'C':
                              od_disp_str(od_control.user_password);
                              break;
                           case 'D':
                              od_disp_str(od_control.user_dataphone);
                              break;
                           case 'E':
                              od_disp_str(od_control.user_homephone);
                              break;
                           case 'F':
                              od_disp_str(od_control.user_lastdate);
                              break;
                           case 'G':
                              od_disp_str(od_control.user_lasttime);
                              break;
                           case 'H':
                              btScrollLines = 0;
                              goto show_flags;
                           case 'I':
                              btScrollLines = 1;
                              goto show_flags;
                           case 'J':
                              btScrollLines = 2;
                              goto show_flags;
                           case 'K':
                              btScrollLines = 3;
   show_flags:                for(btRepeatCount = 0; btRepeatCount < 8;
                                 ++btRepeatCount)
                              {
                                 if((od_control.user_flags[btScrollLines] >>
                                    btRepeatCount) & 0x01)
                                 {
                                    szToRepeat[btRepeatCount] = 'X';
                                 }
                                 else
                                 {
                                    szToRepeat[btRepeatCount] = '-';
                                 }
                              }
                              szToRepeat[btRepeatCount] = '\0';
                              od_disp_str(szToRepeat);
                              break;
                           case 'L':
                              od_printf("%lu", od_control.user_net_credit);
                              break;
                           case 'M':
                              od_printf("%u", od_control.user_messages);
                              break;
                           case 'N':
                              od_printf("%u", od_control.user_lastread);
                              break;
                           case 'O':
                              od_printf("%u", od_control.user_security);
                              break;
                           case 'P':
                              od_printf("%u", od_control.user_numcalls);
                              break;
                           case 'Q':
                              od_printf("%ul", od_control.user_uploads);
                              break;
                           case 'R':
                              od_printf("%ul", od_control.user_upk);
                              break;
                           case 'S':
                              od_printf("%ul", od_control.user_downloads);
                              break;
                           case 'T':
                              od_printf("%ul", od_control.user_downk);
                              break;
                           case 'U':
                              od_printf("%d", od_control.user_time_used);
                              break;
                           case 'V':
                              od_printf("%d", od_control.user_screen_length);
                              break;
                           case 'W':
                              btRepeatCount = 0;
                              while(od_control.user_name[btRepeatCount])
                              {
                                 if((szToRepeat[btRepeatCount]
                                    = od_control.user_name[btRepeatCount])
                                    == ' ')
                                 {
                                    szToRepeat[btRepeatCount] = '\0';
                                    break;
                                 }
                                 ++btRepeatCount;
                              }
                              od_disp_str(szToRepeat);
                              break;
                           case 'X':
                              if(od_control.user_ansi)
                              {
                                 od_disp_str("ON");
                              }
                              else
                              {
                                 od_disp_str("OFF");
                              }
                              break;
                           case 'Y':
                              if(od_control.user_attribute & 0x04)
                              {
                                 od_disp_str("ON");
                              }
                              else
                              {
                                 od_disp_str("OFF");
                              }
                              break;
                           case 'Z':
                              if(od_control.user_attribute & 0x02)
                              {
                                 od_disp_str("ON");
                              }
                              else
                              {
                                 od_disp_str("OFF");
                              }
                              break;
                           case '0':
                              if(od_control.user_attribute & 0x40)
                              {
                                 od_disp_str("ON");
                              }
                              else
                              {
                                 od_disp_str("OFF");
                              }
                              break;
                           case '1':
                              if(od_control.user_attribute & 0x80)
                              {
                                 od_disp_str("ON");
                              }
                              else
                              {
                                 od_disp_str("OFF");
                              }
                              break;
                           case '2':
                              if(od_control.user_attrib2 & 0x01)
                              {
                                 od_disp_str("ON");
                              }
                              else
                              {
                                 od_disp_str("OFF");
                              }
                              break;
                           case '3':
                              od_disp_str(od_control.user_handle);
                              break;
                           case '4':
                              od_disp_str(od_control.user_firstcall);
                              break;
                           case '5':
                              od_disp_str(od_control.user_birthday);
                              break;
                           case '6':
                              od_disp_str(od_control.user_subdate);
                              break;
                           case '7':
                              /* days until subscrption expiry */
                              break;
                           case '8':
                              if(od_control.user_attrib2 & 0x02)
                              {
                                 od_disp_str("ON");
                              }
                              else
                              {
                                 od_disp_str("OFF");
                              }
                              break;
                           case '9':
                              od_printf("%lu:%lu",
                                 od_control.user_uploads,
                                 od_control.user_downloads);
                              break;
                           case ':':
                              od_printf("%lu:%lu",
                                 od_control.user_upk,
                                 od_control.user_downk);
                              break;
                           case ';':
                              if(od_control.user_attrib2 & 0x04)
                              {
                                 od_disp_str("ON");
                              }
                              else
                              {
                                 od_disp_str("OFF");
                              }
                        }
                        btAvatarSeqLevel=0;
                        break;

                     /* QBBS/RA ^K control codes. */
                     case 22:
                        bEchoThisChar = FALSE;
                        switch(chCurrent)
                        {
                           case 'A':
                              od_printf("%lu", od_control.system_calls);
                              break;
                           case 'B':
                              od_disp_str(od_control.system_last_caller);
                              break;
                           case 'C':
                              /* number of active messages */
                              break;
                           case 'D':
                              /* system starting message number */
                              break;
                           case 'E':
                              /* system ending message number */
                              break;
                           case 'F':
                              /* number of times user has paged sysop */
                              break;
                           case 'G':
                              /* day of the week (Monday, Tuesday, etc.) */
                              break;
                           case 'H':
                              /* number of users in user file */
                              break;
                           case 'I':
                              /* Time in 24 hour format */
                              break;
                           case 'J':
                              /* today's date */
                              break;
                           case 'K':
                              /* minutes connected this call */
                              break;
                           case 'L':
                              /* Seconds connected (0) */
                              break;
                           case 'M':
                              od_printf("%d", od_control.user_time_used);
                              break;
                           case 'N':
                              od_disp_str("00");
                              break;
                           case 'O':
                              /* Minutes remaining today */
                              od_printf("%d", od_control.user_timelimit);
                              break;
                           case 'P':
                              /* seconds remaining today (0) */
                              break;
                           case 'Q':
                              od_printf("0", od_control.user_timelimit);
                              break;
                           case 'R':      /* current baud rate */
                              od_printf("0", od_control.baud);
                              break;
                           case 'S':
                              /* day of the week (MON, TUE) */
                              break;
                           case 'T':
                              /* Daily download limit (in K) */
                              break;
                           case 'U':
                              /* Minutes until next system event */
                              break;
                           case 'V':
                              ODScrnDisplayString(od_control.event_starttime);
                              break;
                           case 'W':
                              /* line number (from command line) */
                              break;
                           case 'X':
                              od_exit(2, TRUE);
                              break;
                           case 'Y':
                              /* Name of current msg area */
                              break;
                           case 'Z':
                              /* name of current file area */
                              break;
                           case '0':
                              /* # of messages in area */
                              break;
                           case '1':
                              /* # of message area */
                              break;
                           case '2':
                              /* # of file area */
                              break;
                        }
                        btAvatarSeqLevel = 0;
                  }
            }
            break;

         case 1:
            switch(chCurrent)
            {
               case '[':
                  btANSISeqLevel = 2;
                  btCurrentParamLength = 0;
                  btNumParams = 0;
                  break;

               default:
                  btANSISeqLevel = 0;
                  ODScrnDisplayChar(27);
                  ODScrnDisplayChar(chCurrent);
            }
            break;

         default:
            if((chCurrent >= '0' && chCurrent <= '9') || chCurrent == '?')
            {
               if(btCurrentParamLength < 3)
               {
                  szCurrentParam[btCurrentParamLength] = chCurrent;
                  szCurrentParam[++btCurrentParamLength] = '\0';
               }
               else
               {
                  btANSISeqLevel = 0;
               }
            }

            else if(chCurrent == ';')
            {
               if(btNumParams < 10)
               {
                  if(btCurrentParamLength != 0)
                  {
                     if(strcmp(szCurrentParam, "?9") == 0)
                     {
                        anANSIParams[btNumParams] = -2;
                     }
                     else
                     {
                        anANSIParams[btNumParams] = atoi(szCurrentParam);
                     }
                     szCurrentParam[0] = '\0';
                     btCurrentParamLength = 0;
                     ++btNumParams;
                  }
                  else
                  {
                     anANSIParams[btNumParams++] = -1;
                  }
               }
               else
               {
                  btANSISeqLevel = 0;
               }
            }

            else
            {
               btANSISeqLevel = 0;

               if(btCurrentParamLength != 0 && btNumParams < 10)
               {
                  if(strcmp(szCurrentParam,"?9") == 0)
                  {
                     anANSIParams[btNumParams] = -2;
                  }
                  else
                  {
                     anANSIParams[btNumParams] = atoi(szCurrentParam);
                  }
                  szCurrentParam[0] = '\0';
                  btCurrentParamLength = 0;
                  ++btNumParams;
               }

               ODScrnGetTextInfo(&TextInfo);

               switch(chCurrent)
               {
                  case 'A':
                     if(btNumParams == 0) anANSIParams[0] = 1;
                     if((nTemp = TextInfo.cury - anANSIParams[0]) < 1)
                     {
                        nTemp = 1;
                     }
                     if(nTemp > 25) nTemp=25;
                     ODScrnSetCursorPos(TextInfo.curx, (BYTE)nTemp);
                     break;

                  case 'B':
                     if(btNumParams == 0) anANSIParams[0] = 1;
                     if((nTemp = TextInfo.cury + anANSIParams[0]) > 25)
                     {
                        nTemp = 25;
                     }
                     if(nTemp < 1) nTemp = 1;
                     ODScrnSetCursorPos(TextInfo.curx, (BYTE)nTemp);
                     break;

                  case 'C':
                     if(btNumParams == 0) anANSIParams[0] = 1;
                     if((nTemp=TextInfo.curx + anANSIParams[0]) > 80)
                     {
                        nTemp = 80;
                     }
                     if(nTemp < 1) nTemp = 1;
                     ODScrnSetCursorPos((BYTE)nTemp, TextInfo.cury);
                     break;

                  case 'D':
                     if(btNumParams == 0) anANSIParams[0] = 1;
                     if((nTemp = TextInfo.curx - anANSIParams[0]) < 1)
                     {
                        nTemp = 1;
                     }
                     if(nTemp > 80) nTemp = 80;
                     ODScrnSetCursorPos((BYTE)nTemp, TextInfo.cury);
                     break;

                  case 'H':
                  case 'f':
                     if(btNumParams >= 2)
                     {
                        if(anANSIParams[0] == -1)
                        {
                           ODScrnSetCursorPos((BYTE)anANSIParams[1], 1);
                        }
                        else
                        {
                           ODScrnSetCursorPos((BYTE)anANSIParams[1],
                              (BYTE)anANSIParams[0]);
                        }
                     }
                     else if(btNumParams == 1)
                     {
                        if(anANSIParams[0] <= 0)
                        {
                           ODScrnSetCursorPos(1, TextInfo.cury);
                        }
                        else
                        {
                           ODScrnSetCursorPos(1, (BYTE)anANSIParams[0]);
                        }
                     }
                     else /* if(num_params==0) */
                     {
                        ODScrnSetCursorPos(1, 1);
                     }
                     break;

                  case 'J':
                     if(btNumParams >= 1 && anANSIParams[0] == 2)
                     {
                        /* Clear entire screen. */
                        ODScrnClear();
                     }
                     else if(btNumParams == 0 || anANSIParams[0] == 0)
                     {
                        /* Not supported - Clears from cursor to end of */
                        /* screen.                                      */
                     }
                     else if(btNumParams>=1 && anANSIParams[0]==1)
                     {
                        /* Not supported - Clears from beginning of screen to */
                        /* cursor.                                            */
                     }
                     break;

                  case 'K':
                     if(btNumParams == 0 || anANSIParams[0] == 0)
                     {
                        /* Clear to end of line. */
                        ODScrnClearToEndOfLine();
                     }
                     else if(btNumParams >= 1 && anANSIParams[0] == 1)
                     {
                        /* Not supported - should clear to beginning of line. */
                     }
                     else if(btNumParams >= 1 && anANSIParams[0] == 2)
                     {
                        /* Not supported - should clear entire line. */
                     }
                     break;

                  case 'm':
                     for(nTemp = 0; nTemp < btNumParams; ++nTemp)
                     {
                        if(anANSIParams[nTemp] == 0)
                        {
                           ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                              = TextInfo.attribute = 0x07));
                        }
                        else if(anANSIParams[nTemp] == 1)
                        {
                           ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                              = TextInfo.attribute
                              = TextInfo.attribute | 0x08));
                        }
                        else if(anANSIParams[nTemp] == 2)
                        {
                           ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                              = TextInfo.attribute
                              = TextInfo.attribute & (~0x08)));
                        }
                        else if(anANSIParams[nTemp] == 4)
                        {
                           ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                              = TextInfo.attribute
                              = (TextInfo.attribute & 0xf8) | (1)));
                        }
                        else if(anANSIParams[nTemp] == 5)
                        {
                           ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                              = TextInfo.attribute
                              = TextInfo.attribute | 0x80));
                        }
                        else if(anANSIParams[nTemp] == 7)
                        {
                           ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                              = TextInfo.attribute
                              = (TextInfo.attribute << 4)
                              | (TextInfo.attribute >> 4)));
                        }
                        else if(anANSIParams[nTemp] == 8)
                        {
                           ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                              = TextInfo.attribute
                              = (TextInfo.attribute & 0xf0)
                              | ((TextInfo.attribute >> 4) & 0x07)));
                        }
                        else if(anANSIParams[nTemp] >= 30
                           && anANSIParams[nTemp] <= 37)
                        {
                           ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                              = TextInfo.attribute
                              = (TextInfo.attribute & 0xf8)
                              + abtANSIToPCColorTable[
                              (anANSIParams[nTemp] - 30)]));
                        }
                        else if(anANSIParams[nTemp] >= 40
                           && anANSIParams[nTemp]<=47)
                        {
                           ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib
                            = TextInfo.attribute
                            = (TextInfo.attribute & 0x8f)
                            + (abtANSIToPCColorTable[
                            anANSIParams[nTemp] - 40] << 4)));
                        }
                     }
                     break;

                  case 's':
                     btSavedColumn = TextInfo.curx;
                     btSavedRow = TextInfo.cury;
                     break;

                  case 'u':
                     ODScrnSetCursorPos(btSavedColumn, btSavedRow);
                     break;

                  case '@':
                     /* Not supported - inserts spaces at cursor. */
                     break;

                  case 'P':
                     /* Not supported - deletes characters at cursor. */
                     break;

                  case 'L':
                     /* Not supported - inserts lines at cursor. */
                     break;

                  case 'M':
                     /* Not supported - deletes lines at cursor. */
                     break;

                  case 'r':
                     /* Not supported - sets scrolling zone - 1st param is */
                     /* top row, 2nd param is bottom row. Cursor may go    */
                     /* outside zone, but no scrolling occurs there.       */
                     /* Also resets cursor to row 1, column 1.             */
                     /* If only one param, bottom row is bottom of screen. */
                     break;

                  case 'h':
                     if(btNumParams >= 1 && anANSIParams[0] == 4)
                     {
                        /* Not suppored - turn insert mode on. */
                     }
                     else if(btNumParams >= 1 && anANSIParams[0] == -2)
                     {
                        /* Home cursor. */
                        ODScrnSetCursorPos(1, 1);
                     }
                     break;

                  case 'l':
                     if(btNumParams >= 1 && anANSIParams[0] == 4)
                     {
                        /* Not suppored - turn insert mode off. */
                     }
                     break;

                  case 'E':
                     /* Not supported - repeat CRLF specified # of times. */
                     break;

                  case 'F':
                     /* Not supported - repeat reverse CRLF specified # */
                     /* of times. Also not suppored ESC M - reverse     */
                     /* linefeed, ESC D - LF, ESC E - CRLF              */
                     break;
               }
            }
      }

      if(bEchoThisChar && od_control.baud != 0)
      {
         ODComSendByte(hSerialPort, chCurrent);
      }

      ++pszBuffer;
   }
}


/* ----------------------------------------------------------------------------
 * ODEmulateFillArea()                                 *** PRIVATE FUNCTION ***
 *
 * Fills an area of the local screen with the specified character, in the
 * current display color.
 *
 * Parameters: btLeft       - The left column of the area to fill.
 *
 *             btTop        - The top row of the area to fill.
 *
 *             btRight      - The right column of the area to fill.
 *
 *             btBottom     - The bottom row of the area to fill.
 *
 *             chToFillWith - Character to fill in the specified area with.
 *
 *     Return: void
 */
static void ODEmulateFillArea(BYTE btLeft, BYTE btTop, BYTE btRight,
   BYTE btBottom, char chToFillWith)
{
   BYTE btCount;
   BYTE btLast;
   static char szTemp[81];
   static tODScrnTextInfo TextInfo;

   ODScrnGetTextInfo(&TextInfo);

   btLast = btRight - btLeft;

   for(btCount=0; btCount <= btLast; ++btCount)
   {
      szTemp[btCount] = chToFillWith;
   }
   szTemp[btCount] = 0;

   ODScrnEnableScrolling(FALSE);

   for(btCount = btTop; btCount <= btBottom; ++btCount)
   {
      ODScrnSetCursorPos(btLeft, btCount);
      ODScrnDisplayString(szTemp);
   }

   ODScrnSetCursorPos(TextInfo.curx, TextInfo.cury);

   ODScrnEnableScrolling(TRUE);
}
