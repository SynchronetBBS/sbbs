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
 *        File: ODCore.c
 *
 * Description: Implements the core of OpenDoors, including chat mode
 *              and standard input/output functions that are
 *              used throughout OpenDoors.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Oct 19, 1994  6.00  BP   Changed paging hours logic.
 *              Oct 21, 1994  6.00  BP   Further isolated com routines.
 *              Oct 22, 1994  6.00  BP   Name case conversion /w punct.
 *              Dec 08, 1994  6.00  BP   Allow custom chat mode deactivation.
 *              Dec 09, 1994  6.00  BP   Remove global dir entry structure.
 *              Dec 13, 1994  6.00  BP   Remove include of dir.h.
 *              Dec 31, 1994  6.00  BP   Remove #ifndef USEINLINE DOS code.
 *              Dec 31, 1994  6.00  BP   Remove old multitasker definitions.
 *              Jan 01, 1995  6.00  BP   Don't use ODComInbound().
 *              Jan 01, 1995  6.00  BP   _waitdrain() -> ODWaitDrain().
 *              Jan 01, 1995  6.00  BP   Use new millisecond timer functions.
 *              Jan 01, 1995  6.00  BP   Remove od_init() from _remotechar()
 *              Jan 01, 1995  6.00  BP   Split off odkrnl.c from odcore.c
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Moved first_word() to odlist.c
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 16, 1995  6.00  BP   Create odcore.h.
 *              Nov 17, 1995  6.00  BP   Use new input queue mechanism.
 *              Dec 12, 1995  6.00  BP   Added od_set_color().
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 13, 1995  6.00  BP   Moved chat mode code to ODKrnl.h.
 *              Dec 19, 1995  6.00  BP   Request reason for chat outside hours.
 *              Dec 23, 1995  6.00  BP   Allow space to continue at page pause.
 *              Dec 24, 1995  6.00  BP   Added abtGreyBlock.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 03, 1996  6.00  BP   Use OD_API_VAR_DEFN for od_control.
 *              Jan 04, 1996  6.00  BP   tODInQueueEvent -> tODInputEvent.
 *              Jan 23, 1996  6.00  BP   No od_set_statusline() under Win32.
 *              Jan 30, 1996  6.00  BP   Replaced od_yield() with od_sleep().
 *              Jan 30, 1996  6.00  BP   Add ODInQueueGetNextEvent() timeout.
 *              Jan 09, 1996  6.00  BP   ODComOutbound() returns actual size.
 *              Jan 09, 1996  6.00  BP   Reduce kernel calls from od_disp...().
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Mar 21, 1996  6.10  BP   Added od_control_get().
 *              Sep 01, 1996  6.10  BP   Update output area on od_set_per...().
 *              Oct 19, 2001  6.20  RS   od_get_key now ignores linefeeds.
 *              Mar 14, 2002  6.22  RS   Fixed od_get_key(bWait=FALSE)
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include "OpenDoor.h"
#include "ODStr.h"
#include "ODGen.h"
#include "ODPlat.h"
#include "ODCom.h"
#include "ODKrnl.h"
#include "ODScrn.h"
#include "ODCore.h"
#include "ODInQue.h"
#ifdef ODPLAT_WIN32
#include "ODFrame.h"
#endif /* ODPLAT_WIN32 */


/* GLOBAL VARIABLES SHARED THROUGHOUT OPENDOORS. */

/* Global declaration of the OpenDoors control structure. */
OD_API_VAR_DEFN tODControl 
#ifndef _WIN32	/* warning C4229: anachronism used : modifiers on data are ignored */
OD_GLOBAL_CONV 
#endif
od_control;

/* OpenDoors global initialized flag. */
BOOL bODInitialized = FALSE;

/* Global serial port object handle. */
tPortHandle hSerialPort;

/* Global input queue object handle. */
tODInQueueHandle hODInputQueue;

/* Reentrancy control. */
BOOL bIsCallbackActive = FALSE;
BOOL bShellChatActive = FALSE;

/* Global working space. */
char szODWorkString[OD_GLOBAL_WORK_STRING_SIZE];

/* Global instance of the text information structure for general use. */
tODScrnTextInfo ODTextInfo;

/* Logfile function hooks. */
BOOL (*pfLogWrite)(INT) = NULL;
void (*pfLogClose)(INT) = NULL;

/* od_color_config() support for od_printf(). */
char chColorCheck = 0;
char *pchColorEndPos;

/* Status line information. */
BYTE btCurrentStatusLine = STATUS_NONE;
OD_PERSONALITY_CALLBACK *pfCurrentPersonality = NULL;
char szDesiredPersonality[33] = "";
SET_PERSONALITY_FUNC *pfSetPersonality = NULL;

/* Commonly used character sequences. */
char abtBlackBlock[2] = {' ', 0x07};
char abtGreyBlock[2] = {' ', 0x70};
char szBackspaceWithDelete[4] = {8, ' ', 8, 0};

/* Current output area on screen. */
BYTE btOutputTop = 1;
BYTE btOutputBottom = 23;


/* PRIVATE VARIABLES. */

/* Display color varaibles. */
char bAnyColorChangeYet;

/* Static character sequences. */
static char szClearScreen[2] = {12, 0};

/* Lookup table to map colors from PC values to ANSI color values. */
static BYTE abtPCToANSIColorTable[8] = {30, 34, 32, 36, 31, 35, 33, 37};


/* LOCAL HELPER FUNCTIONS. */
static void ODAddANSIParameter(char *szControlSequence, int nParameterValue);


/* ----------------------------------------------------------------------------
 * ODWaitDrain()
 *
 * Waits for up to the specified number of milliseconds for the output serial
 * buffer to drain.
 *
 * Parameters: MaxWait - Specifies the maximum number of milliseconds to wait
 *                       before timing out.
 *
 *     Return: void
 */
void ODWaitDrain(tODMilliSec MaxWait)
{
   int nOutboundSize;
   tODTimer Timer;

   /* If we are operating in local mode, then don't do anything. */
   if(od_control.baud == 0) return;

   /* Otherwise, start a timer that is set to elapse after the maximum */
   /* wait period.                                                     */
   ODTimerStart(&Timer, MaxWait);

   /* Loop until either the outbound buffer is empty, or the */
   /* timer has elapsed.                                     */
   for(;;)
   {
      /* Check whether any data is in the outbound serial queue. */
      ODComOutbound(hSerialPort, &nOutboundSize);

      /* If the queue is empty or the timer has elapsed, then stop */
      /* waiting.                                                  */
      if(nOutboundSize == 0 || ODTimerElapsed(&Timer)) break;

      /* Otherwise, give other tasks a chance to run. */
      od_sleep(0);

      /* Give od_kernel() activities a chance to run. */
      CALL_KERNEL_IF_NEEDED();
   } 
}


/* ----------------------------------------------------------------------------
 * od_clr_scr()
 *
 * Clears the contents of the local and remote screens, if screen clearing is
 * enabled.
 *
 * Parameters: none
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_clr_scr(void)
{
   INT16 nOriginalAttrib;

   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_clr_scr()");

   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Don't clear screen if disabled. */
   if(!od_control.od_always_clear && !(od_control.user_attribute & 2)
      && (od_control.od_extended_info || od_control.od_info_type == CUSTOM))
   {
      OD_API_EXIT();
      return;
   }

   if(od_control.user_rip)
   {
      od_disp("!|*", 3, FALSE);
      if(!od_control.od_default_rip_win)
      {
         od_disp("!|w0000270M12", 13, FALSE);
      }
   }
   
   if(od_control.user_ansi)
   {
      od_disp("\x1b[2J\x1b[1;1H", 10, FALSE);
   }
   else {
	   /* Send ascii 12 to modem, no local echo. */
	   od_disp(szClearScreen, 1, FALSE);
   }

   /* Clear local window. */
   ODScrnClear();

   /* Get color set prior to screen clear. */
   nOriginalAttrib = od_control.od_cur_attrib;

   /* Current color state is unknown. */
   od_control.od_cur_attrib = -1;

   /* Set color to original value. This gurantees that local and */
   /* remote systems both have the same current color set.       */
   od_set_attrib(nOriginalAttrib);

   OD_API_EXIT();
}



/* ----------------------------------------------------------------------------
 * od_input_str()
 *
 * Allows the user to input a string up to the specified length, using
 * characters in the specified range. This string input function is designed
 * to be compatible with all terminal types.
 *
 * Parameters: pszInput   - Pointer to string to store input in.
 *
 *             nMaxLength - Maximum number of characters to permit the user
 *                          to input.
 *
 *             chMin      - The minimum character value to permit. This must
 *                          be at least ASCII 32.
 *
 *             chMax      - The maximum character value to permit.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_input_str(char *pszInput, 
   INT nMaxLength,
   unsigned char chMin,
   unsigned char chMax)
{
   char chKeyPressed;
   INT nPosition;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_input_str()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Start at the beginning of the string. */
   nPosition = 0;

   /* Check that input parameters are valid. */
   if(pszInput == NULL || nMaxLength < 1 || chMin > chMax)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return;
   }

   for(;;)
   {
      chKeyPressed = od_get_key(TRUE);

      /* If user pressed enter. */
      if(chKeyPressed == '\r' || chKeyPressed == '\n')
      {
         /* Terminate the string. */
         pszInput[nPosition] = '\0';

         /* Display CR-LF sequence. */
         od_disp_str("\n\r");

         /* Exit the function. */
         OD_API_EXIT();
         return;
      }

      /* If the user pressed backspace. */
      else if(chKeyPressed == 8)
      {
         /* If we are not currently at the beginning of the string. */
         if(nPosition > 0)
         {
            /* Send backspace sequence. */
            od_disp_str(szBackspaceWithDelete);

            /* Move current position back by one position in the string. */
            --nPosition;
         }
      }

      /* If this is a valid character to place in the string and we have */
      /* not reached the maximum size of the string yet.                 */
      else if(chKeyPressed >= chMin && chKeyPressed <= chMax
         && nPosition < nMaxLength)
      {
         /* Display key that was pressed. */
         od_putch(chKeyPressed);

         /* Add the entered character to the string and increment our */
         /* current position in the string.                           */
         pszInput[nPosition++] = chKeyPressed;
      }
   }
}


/* ----------------------------------------------------------------------------
 * od_clear_keybuffer()
 *
 * Clears any keystrokes from the inbound buffers. Both input from local and
 * remote systems is discarded, by clearing both OpenDoors' common input
 * event queue, and the serial port inbound buffer. This function is called
 * to cause any input by the user prior to the time the function was called
 * to be ignored.
 *
 * Parameters: none
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_clear_keybuffer(void)
{
   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_clear_keybuffer()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Empty any events in the common input event queue. */
   ODInQueueEmpty(hODInputQueue);

   /* If we are not operating in local mode ... */
   if(od_control.baud != 0)
   {
      /* ... then remove any items in the serial port inbound buffer. */
      ODComClearInbound(hSerialPort);
   }

   /* Call the OpenDoors kernel function. */
   CALL_KERNEL_IF_NEEDED();

   OD_API_EXIT();
}

/* ----------------------------------------------------------------------------
 * od_key_pending()
 *
 * Returns TRUE if there's a key pending, FALSE otherwise.
 *
 * Parameters: none
 *
 *     Return: TRUE if character is waiting, FALSE if no character is waiting.
 */
ODAPIDEF BOOL ODCALL od_key_pending(void)
{
   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_get_key()");

   OD_API_ENTRY();

   /* Call the OpenDoors kernel. */
   CALL_KERNEL_IF_NEEDED();

   if(!ODInQueueWaiting(hODInputQueue))
   {
         OD_API_EXIT();
         return(FALSE);
   }

   OD_API_EXIT();
   return(TRUE);
}

/* ----------------------------------------------------------------------------
 * od_get_key()
 *
 * Inputs a single character, optionally waiting for the next character if no
 * character has been received yet. This function returns data received from
 * either the local or remote system, in the order in which it was received.
 *
 * Parameters: bWait - FALSE if od_get_key() should return right away with
 *                     a value of 0 if no characters have been received, or
 *                     TRUE if od_get_key() should wait for the next received
 *                     character.
 *
 *     Return: Character that was received, or 0 if no character is waiting.
 */
ODAPIDEF char ODCALL od_get_key(BOOL bWait)
{
   tODInputEvent InputEvent;

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_get_key()");

   OD_API_ENTRY();

   /* Call the OpenDoors kernel. */
   CALL_KERNEL_IF_NEEDED();

	do {

	   /* If we aren't supposed to wait for input, then check whether any   */
	   /* input is waiting in the input queue, and if not return right away */
	   /* without any data.                                                 */
	   if(!bWait)
	   {
		  if(!ODInQueueWaiting(hODInputQueue))
		  {
			 OD_API_EXIT();
			 return(0);
		  }
	   }

		/* Obtain the next character from the input queue. If we get to this */
		/* point and there is no data waiting in the input queue, then the   */
		/* ODInQueueGetNextEvent() function will block until a character     */
		/* is available in the input queue.                                  */
		ODInQueueGetNextEvent(hODInputQueue, &InputEvent, OD_NO_TIMEOUT);

		/* Only keyboard input events are currently supported by od_get_key(). */
		ASSERT(InputEvent.EventType == EVENT_CHARACTER);

		/* Update OpenDoors control structure member that records whether the */
		/* last input came from the local or remote user.                     */
		od_control.od_last_input = InputEvent.bFromRemote ? 0 : 1;

	} while(InputEvent.chKeyPress == '\n');	/* Ignore line-feed char */

   /* Return the character that was pressed by the user. */
   OD_API_EXIT();
   return(InputEvent.chKeyPress);
}



/* ----------------------------------------------------------------------------
 * od_carrier()
 *
 * Allows programs to determine the current state of the carrier detect
 * signal when OpenDoors' automatic carrier detection has been disabled.
 *
 * Parameters: none
 *
 *     Return: TRUE if the carrier detct signal is present, FALSE if it
 *             isn't. When operating in local mode, this function always
 *             returns FALSE.
 */
ODAPIDEF BOOL ODCALL od_carrier(void)
{
   BOOL bIsCarrier;

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_carrier()");

   /* If we are operating in local mode, then return FALSE. */
   if(od_control.baud == 0)
   {
      od_control.od_error = ERR_NOREMOTE;
      OD_API_EXIT();
      return(FALSE);
   }

   /* In remote mode, obtain the current state of the carrier detect signal. */
   ODComCarrier(hSerialPort, &bIsCarrier);

   /* Return the current state of the carrier detect signal. */
   OD_API_EXIT();
   return(bIsCarrier);
}


/* ----------------------------------------------------------------------------
 * od_repeat()
 *
 * This function displays the same character the specified number of times on
 * the local and remote screens, using any available optimal control sequences
 * under the current display mode.
 *
 * Parameters: chValue - Character to repeat.
 *
 *             btTimes - Number of times to repeat the character.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_repeat(char chValue, BYTE btTimes)
{
   char *pchCurStringPos;
   BYTE btLeft;
   char szBuffer[3];

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_repeat()");

   /* Ensure that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* If the caller asked to repeat the character 0 times, then we can */
   /* safely return right away without doing anything.                 */
   if(btTimes == 0)
   {
      OD_API_EXIT();
      return;
   }

   /* Generate string of repeat characters. */
   pchCurStringPos = szODWorkString; 
   for(btLeft = btTimes; btLeft--;)
   {
      *pchCurStringPos++ = chValue;
   }
   *pchCurStringPos = '\0';

   /* Display repeated string on local screen. */
   ODScrnDisplayString(szODWorkString);

   /* If we are operating in AVATAR mode. */
   if(od_control.user_avatar)
   {
      /* Generate the AVATAR control sequence to repeat this character */
      /* the specified number of times.                                */
      szBuffer[0] = 25;
      szBuffer[1] = chValue;
      szBuffer[2] = btTimes;
      od_disp(szBuffer, 3, FALSE);
   }

   /* If AVATAR mode is not available. */
   else
   {
      /* Send the entire repeated string to the remote system. */
      od_disp(szODWorkString, btTimes, FALSE);
   }

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * od_page()
 *
 * This function is called when the user wished to page the system operator.
 *
 * Parameters: none
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_page(void)
{
   INT16 nCount;
   tODTimer Timer;
   time_t nUnixTime;
   struct tm *TimeBlock;
   INT16 nMinute;
   BOOL bFailed = FALSE;
   INT16 nOriginalAttrib;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_page()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Save current display color attribute. */
   nOriginalAttrib = od_control.od_cur_attrib;

   /* Clear the screen. */
   od_clr_scr();
   od_set_attrib(od_control.od_chat_color1);

   /* Ask reason for chat. */
   od_disp_str(od_control.od_chat_reason);
   od_set_attrib(od_control.od_chat_color2);
   od_putch('[');

   /* Use extended ASCII characters if operating in ANSI or AVATAR mode. */
   if(od_control.user_ansi || od_control.user_avatar)
   {
      od_repeat('Ä',77);
   }
   else
   {
      od_repeat('-',77);
   }
   od_disp_str("]\n\r ");
   od_input_str(od_control.user_reasonforchat,77,32,255);

   /* If the user did not abort sysop paging by entering a blank reason */
   /* for chat.                                                         */
   if(strlen(od_control.user_reasonforchat) != 0)
   {
      /* Indicate that the user wants to chat. */
      od_control.user_wantchat = TRUE;
#ifdef ODPLAT_WIN32
      ODFrameUpdateWantChat();
#endif /* ODPLAT_WIN32 */

      /* Determine whether or not sysop paging should be permitted at */
      /* the current time.                                            */
      nUnixTime = time(NULL);
      TimeBlock = localtime(&nUnixTime);
      nMinute = (60 * TimeBlock->tm_hour) + TimeBlock->tm_min;
      if(od_control.od_pagestartmin < od_control.od_pageendmin)
      {
         if(nMinute < od_control.od_pagestartmin
            || nMinute >= od_control.od_pageendmin)
         {
            bFailed = TRUE;
         }
      }
      else if(od_control.od_pagestartmin > od_control.od_pageendmin)
      {
         if(nMinute < od_control.od_pagestartmin
            && nMinute >= od_control.od_pageendmin)
         {
            bFailed = TRUE;
         }
      }
      else
      {
         bFailed = FALSE;
      }

      /* If paging is set to PAGE_ENABLE, meaning that sysop paging should */
      /* be permitted regardless of the time of day, then allow paging.    */
      if(od_control.od_okaytopage == PAGE_ENABLE)
      {
         bFailed = FALSE;
      }

      /* If paging is explicitly disable by PAGE_DISABLE, or the current */
      /* time of the day is not normally permitted for paging.           */
      if(od_control.od_okaytopage == PAGE_DISABLE || bFailed)
      {
         /* Indicate this to user. */
         od_disp_str("\n\r");
         od_disp_str(od_control.od_no_sysop);
         od_disp_str(od_control.od_press_key);
         od_get_answer("\x0d\x0a");

         /* Return from this function. */
         goto cleanup;
      }

      /* Update status line right away. */
      bForceStatusUpdate = TRUE;
      CALL_KERNEL_IF_NEEDED();

      /* Write sysop page information to the logfile, if the log file */
      /* system is hooked up.                                         */
      if(pfLogWrite != NULL)
      {
         (*pfLogWrite)(8);
      }

      /* Tell the user that we are now paging the system operator. */
      od_set_attrib(od_control.od_chat_color1);
      od_disp_str(od_control.od_paging);

#ifdef OD_TEXTMODE
      /* Display sysop page status line if it exists and the sysop status */
      /* line is currently active.                                        */
      if(od_control.od_page_statusline != -1 && btCurrentStatusLine != 8)
      {
         od_set_statusline(od_control.od_page_statusline);
      }
#endif /* OD_TEXTMODE */

      /* Increment the total number of times that the user has paged */
      /* the sysop.                                                  */
      ++od_control.user_numpages;

      /* Sysop hasn't responded yet. */
      bChatted=FALSE;

      /* Loop for length of sysop page. */
      for(nCount = 0; nCount < od_control.od_page_len; ++nCount)
      {
         /* Start a timer that is set to elapse in exactly one second. */
         ODTimerStart(&Timer, 1000);

         /* Display another period character. */
         od_putch('.');

         /* Abort page if system operator answered */
         if(bChatted) goto cleanup;

         /* Send beep to local and remote systems. */
         od_putch('\a');

         /* Check whether system operator has answered after playing beep. */
         if (bChatted) goto cleanup;

         /* Wait for the timer to elapse, calling od_kernel() so that */
         /* chat mode will start as soon as the sysop presses the     */
         /* chat key.                                                 */
         while(!ODTimerElapsed(&Timer))
         {
            CALL_KERNEL_IF_NEEDED();
         }
      }

      /* If sysop page time has elapsed without a response from the */
      /* sysop, then notify the user.                               */
      od_disp_str(od_control.od_no_response);
      od_disp_str(od_control.od_press_key);
      od_get_answer("\x0d\x0a");
      od_disp_str("\n\r\n\r");
   }

cleanup:
   /* Restore original display color attribute. */
   od_set_attrib(nOriginalAttrib);

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * od_disp()
 *
 * Function to send one or more character to the remote system, optionally
 * also echoing the same characters to the local screen.
 *
 * Parameters: pachBuffer - Pointer to buffer of characters to send.
 *
 *             nSize      - Number of characters to send from the buffer.
 *
 *             bLocalEcho - TRUE to also echo the characters to the local
 *                          screen, FALSE to just send the characters to the
 *                          remote system.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_disp(const char *pachBuffer, INT nSize, BOOL bLocalEcho)
{
   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_disp()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Call the OpenDoors kernel, if needed. */
#ifndef OD_MULTITHREADED
   if(ODTimerElapsed(&RunKernelTimer))
   {
      CALL_KERNEL_IF_NEEDED();
   }
#endif /* !OD_MULTITHREADED */

   /* If we are operating in remote mode, then send the buffer to the */
   /* remote system.                                                  */
   if(od_control.baud != 0)
   {
      ODComSendBuffer(hSerialPort, (BYTE *)pachBuffer, nSize);
   }

   /* If we are also to display the character on the local screen, then */
   /* display the buffer on the local screen.                           */
   if(bLocalEcho)
   {
      ODScrnDisplayBuffer(pachBuffer, nSize);
   }

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * od_disp_str()
 *
 * Displays a string on both the local and remote systems.
 *
 * Parameters: pszToDisplay - Pointer to the string to be displayed.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_disp_str(const char *pszToDisplay)
{
   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_disp_str()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Call the OpenDoors kernel, if needed. */
#ifndef OD_MULTITHREADED
   if(ODTimerElapsed(&RunKernelTimer))
   {
      CALL_KERNEL_IF_NEEDED();
   }
#endif /* !OD_MULTITHREADED */

   /* Send the string to the remote system, if we are running in remote mode. */
   if(od_control.baud != 0)
   {
      ODComSendBuffer(hSerialPort, (BYTE *)pszToDisplay, strlen(pszToDisplay));
   }

   /* Display the screen on the local screen. */
   ODScrnDisplayString(pszToDisplay);

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * od_set_statusline()
 *
 * Switches to one of the available status lines provided by the current
 * personality, or turns off the status line altogether.
 *
 * Parameters: nSetting - Indicates which status line (if any) should be
 *                        activated.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_set_statusline(INT nSetting)
{
#ifdef OD_TEXTMODE
   INT nDistance;
   BYTE btCount
#endif /* OD_TEXTMODE */

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_set_statusline()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY()

#ifdef OD_TEXTMODE

   /* If status line is disabled, then don't do anything. */
   if(!od_control.od_status_on)
   {
      OD_API_EXIT();
      return;
   }

   /* Ensure that the parameter is within the valid range. */
   if(nSetting < 0 || nSetting > 8)
   {
      nSetting = 0;
   }

   /* If the specified status line is already active, and status line */
   /* update isn't being forced, then return without doing anything.  */
   if(!od_control.od_update_status_now && nSetting == btCurrentStatusLine)
   {
      OD_API_EXIT();
      return;
   }

   /* Save the current cursor settings. */
   ODStoreTextInfo();

   /* Reset screen boundary to allow access to the entire screen. */
   ODScrnSetBoundary(1,1,80,25);

   /* If status line is being turned off. */
   if(btCurrentStatusLine == STATUS_NONE)
   {
      if((nDistance = (INT)ODTextInfo.cury - ( 1 + (INT)btOutputBottom
         - (INT)btOutputTop)) > 0)
      {
         ODScrnCopyText(1, (BYTE)((INT)btOutputTop + nDistance), 80,
            (BYTE)((INT)btOutputBottom + nDistance), (BYTE)btOutputTop, 1);
         ODTextInfo.cury = 1 + btOutputBottom - btOutputTop;
      }
      else if(ODTextInfo.cury < btOutputTop)
      {
         ODTextInfo.cury = btOutputTop;
         ODScrnCopyText(1, (BYTE)(btOutputTop + 24 - btOutputBottom), 80, 25,
            btOutputTop, 1);
      }
   }

   od_control.od_current_statusline = btCurrentStatusLine = nSetting;

   if(nSetting == 8)
   {
      ODScrnSetAttribute(0x07);

      for(btCount = 1; btCount <= 25; ++btCount)
      {
         if(btCount < btOutputTop || btCount > btOutputBottom)
         {
            if(btCount == 25)
            {
               ODScrnPutText(80, 25, 80, 25, abtBlackBlock);
               ODScrnSetCursorPos(1, 25);
               ODScrnDisplayString("                                                                               ");
            }
            else
            {
               ODScrnSetCursorPos(1, 24);
               ODScrnDisplayString("                                                                                ");
            }
         }
      }

      ODScrnSetAttribute(ODTextInfo.attribute);
      ODScrnSetCursorPos(ODTextInfo.curx, ODTextInfo.cury);
   }

   else
   {
      ODScrnEnableCaret(FALSE);
      ODScrnEnableScrolling(FALSE);

      (*pfCurrentPersonality)((BYTE)nSetting);

      ODScrnEnableCaret(TRUE);
      ODScrnEnableScrolling(TRUE);

      ODScrnSetBoundary(1, btOutputTop, 80, btOutputBottom);
      ODScrnSetAttribute(ODTextInfo.attribute);
      ODScrnSetCursorPos(ODTextInfo.curx, ODTextInfo.cury);
   }

#else /* !OD_TEXTMODE */

   od_control.od_error = ERR_UNSUPPORTED;

#endif /* !OD_TEXTMODE */

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * ODStoreTextInfo()
 *
 * Stores the current text settings into the OpenDoors global text information
 * structure.
 *
 * Parameters: none
 *
 *     Return: void
 */
void ODStoreTextInfo(void)
{
   ODScrnGetTextInfo(&ODTextInfo);
}


/* ----------------------------------------------------------------------------
 * ODRestoreTextInfo()
 *
 * Restores display settings previously stored by ODStoreTextInfo()
 *
 * Parameters: none
 *
 *     Return: void
 */
void ODRestoreTextInfo(void)
{
   ODScrnSetBoundary(ODTextInfo.winleft, ODTextInfo.wintop,
      ODTextInfo.winright, ODTextInfo.winbottom);
   ODScrnSetAttribute(ODTextInfo.attribute);
   ODScrnSetCursorPos(ODTextInfo.curx, ODTextInfo.cury);
}


/* ----------------------------------------------------------------------------
 * ODStringToName()
 *
 * Reformats a string so that it has the correct capitalization for a name,
 * and removes any trailing line break character.
 *
 * Parameters: pszToConvert - Pointer to the string to reformat.
 *
 *     Return: void
 */
void ODStringToName(char *pszToConvert)
{
   /* Begin by changing the entire string to lower case. */
   strlwr(pszToConvert);

   /* Trim any newline character that may be at the end of the string. */
   if(pszToConvert[strlen(pszToConvert) - 1] == '\n')
   {
      pszToConvert[strlen(pszToConvert) - 1] = '\0';
   }
   /* Trim any CR character that may be at the end of the string. */
   if(pszToConvert[strlen(pszToConvert) - 1] == '\r')
   {
      pszToConvert[strlen(pszToConvert) - 1] = '\0';
   }

   /* Change the first character to lower case. */
   *pszToConvert = toupper(*pszToConvert);

   /* Loop through the rest of the string, capitalizing any other words */
   /* in the string.                                                    */
   while(*pszToConvert)
   {
      switch(*pszToConvert++)
      {
         case ' ':
         case '\t':
         case ',':
         case '.':
         case '-':
            *pszToConvert = toupper(*pszToConvert);
            break;
      }
   }
}


/* ----------------------------------------------------------------------------
 * od_set_color()
 *
 * Sets the current display color for both local and remote output.
 *
 * Parameters: nForeground - New foreground (text) color.
 *
 *             nBackground - New background color.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_set_color(INT nForeground, INT nBackground)
{
   /* Use od_set_attrib() to perform the actual color setting.          */
   /* Here, we rely on od_set_attrib() to look after initialization,    */
   /* API_ENTRY() and API_EXIT() calls, etc. This allows od_set_color() */
   /* (which was previously just a macro) to be implemented with as     */
   /* little overhead as possible.                                      */
   od_set_attrib(nForeground | (nBackground << 4));
}


/* ----------------------------------------------------------------------------
 * od_set_attrib()
 *
 * Sets the current display color for both local and remote output.
 *
 * Parameters: nColor - New Display color to set, or -1 for no change.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_set_attrib(INT nColor)
{
   char szControlSequence[40];

   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_set_attrib()");

   /* Ensure that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* If color value is -1, then make no change. */
   if(nColor == -1)
   {
      OD_API_EXIT();
      return;
   }

   /* If we are operating in AVATAR mode. */
   if(od_control.user_avatar)
   {
      if(od_control.od_cur_attrib != nColor || od_control.od_full_color)
      {
         /* Change local text color. */
         ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib = nColor));

         /* Generate AVATAR control sequence. */
         szControlSequence[0] = 22;
         szControlSequence[1] = 1;
         szControlSequence[2] = nColor;

         /* Send AVATAR control sequence. */
         od_disp(szControlSequence, 3, FALSE);
      }
   }

   /* If we are operating in ANSI mode. */
   else if(od_control.user_ansi)
   {
      bAnyColorChangeYet = FALSE;

      if(od_control.od_cur_attrib == -1 || od_control.od_full_color)
      {
ansi_reset:
         /* Reset ANSI terminal status. */
         ODAddANSIParameter(szControlSequence, 0);

         /* If blink attribute is set. */
         if(nColor & 0x80)
         {
            /* Add it to the ANSI color sequence. */
            ODAddANSIParameter(szControlSequence, 5);
         }

         /* If high intensity attribute is set. */
         if(nColor & 0x08)
         {
            /* Add it to the ANSI color sequence. */
            ODAddANSIParameter(szControlSequence, 1);
         }
      }

      /* If current color is known. */
      else                             
      {
         /* If have to reset flashing or bright. */
         if(((od_control.od_cur_attrib&0x80) &&
            !(nColor & 0x80)) || ((od_control.od_cur_attrib & 0x08)
            && !(nColor & 0x08)))
         {
            /* Must reset entire colour settings. */
            od_control.od_cur_attrib = -1;
            goto ansi_reset;
         }
                                       
         /* If flashing has to be turned on. */
         if((nColor & 0x80) != (od_control.od_cur_attrib & 0x80))
         {
            /* Add it to the ANSI color sequence. */
            ODAddANSIParameter(szControlSequence, 5);
         }

         /* If bright has to be turned on. */
         if((nColor & 0x08) != (od_control.od_cur_attrib & 0x08)
            || od_control.od_cur_attrib == -1)
         {
            /* Add it to the ANSI color sequence. */
            ODAddANSIParameter(szControlSequence, 1);
         }
      }


      /* If foreground color has changed. */
      if((nColor & 0x07) != (od_control.od_cur_attrib & 0x07)
         || od_control.od_cur_attrib == -1 || od_control.od_full_color)
      {
         /* Add translated color to sequence. */
         ODAddANSIParameter(szControlSequence,
            abtPCToANSIColorTable[nColor&0x07]);
      }

      /* If background color has changed. */
      if((nColor & 0x70) != (od_control.od_cur_attrib & 0x70)
         || od_control.od_cur_attrib == -1 || od_control.od_full_color)
      {
         /* Add translated color to sequence. */
         ODAddANSIParameter(szControlSequence,
            abtPCToANSIColorTable[(nColor & 0x70) >> 4] + 10);
      }

      /* If any change in color. */
      if(bAnyColorChangeYet)
      {
         /* Append change-attribute command. */
         strcat(szControlSequence, "m");

         /* Send ANSI sequence to the modem. */
         od_disp(szControlSequence, strlen(szControlSequence), FALSE);
      }

      /* Change local text color. */
      ODScrnSetAttribute((BYTE)(od_control.od_cur_attrib = nColor));
   }
   else
   {
      od_control.od_error = ERR_NOGRAPHICS;
   }

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * ODAddANSIParameter()                                *** PRIVATE FUNCTION ***
 *
 * Adds a parameter to an ANSI color sequence.
 *
 * Parameters: szControlSequence - The contents of the control sequence string
 *                                 generated so far.
 *
 *             nParameterValue   - Value of the parameter to add.
 *
 *     Return: void
 */
static void ODAddANSIParameter(char *szControlSequence, int nParameterValue)
{
   char szTemp[5];

   if(bAnyColorChangeYet)
   {
      sprintf(szTemp, ";%d", nParameterValue);
      strcat(szControlSequence, szTemp);
   }
   else
   {
      bAnyColorChangeYet = TRUE;
      sprintf(szControlSequence, "x[%d", nParameterValue);
      szControlSequence[0] = 27;
   }
}


/* ----------------------------------------------------------------------------
 * od_putch()
 *
 * Displays a character on the local and remote screens.
 *
 * Parameters: chToDisplay - The character to display.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_putch(char chToDisplay)
{
   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_putch()");

   /* Initialize OpenDoors if it hasn't been done already. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Display the character on the local screen. */
   ODScrnDisplayChar(chToDisplay);

   /* If not operating in local mode, then send the character to the */
   /* serial port.                                                   */
   if(od_control.baud)
   {
      ODComSendByte(hSerialPort, chToDisplay);
   }

   /* If it is time to call the kernel, then do so. */
   if(ODTimerElapsed(&RunKernelTimer))
   {
      CALL_KERNEL_IF_NEEDED();
   }

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * od_set_dtr()
 *
 * Changes the state of the DTR line to the modem, if not running in local
 * mode.
 *
 * Parameters: bHigh - TRUE to raise DTR, FALSE to lower it.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_set_dtr(BOOL bHigh)
{
   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_set_dtr()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* If we are running in local mode, then return with an error. */
   if(!od_control.baud)
   {
      od_control.od_error = ERR_NOREMOTE;
      OD_API_EXIT();
      return;
   }

   /* Otherwise, change the state of the DTR line. */
   ODComSetDTR(hSerialPort, bHigh);

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * od_get_answer()
 *
 * Waits for the user to press one of the keys listed in pszOptions. Case is
 * not sensitive, although the pressed key is returned in the same case as it
 * is specified in pszOptions.
 *
 * Parameters: pszOptions - String listing characters to accept.
 *
 *     Return: void
 */
ODAPIDEF char ODCALL od_get_answer(const char *pszOptions)
{
   char *pchPossibleOption;
   char chPressed;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_get_answer()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   for(;;)
   {
      /* Wait for the next key press by the user. */
      chPressed = od_get_key(TRUE);
      chPressed = tolower(chPressed);

      /* Loop through list of possible options. */
      pchPossibleOption = (char *)pszOptions;
      while(*pchPossibleOption)
      {
         /* If the key pressed matches this possible option. */
         if(tolower(*pchPossibleOption) == chPressed)
         {
            /* Then return the character in the case originally specified */
            /* by the caller.                                             */
            OD_API_EXIT();
            return(*pchPossibleOption);
         }

         /* Move on to the next possible option. */
         ++pchPossibleOption;
      }

      /* If the key pressed did not match a possible option, then we */
      /* just loop again, getting the next key.                      */
   }
}


/* ----------------------------------------------------------------------------
 * od_color_config()
 *
 * Determines the color attribute that is described by the provided string.
 * This string is in the same format that is used for specifying colors in the
 * OpenDoors configuration file.
 *
 * Parameters: pszColorDesc - Color description string.
 *
 *     Return: The PC-style color attribute corresponding to the color
 *             description string.
 */
ODAPIDEF BYTE ODCALL od_color_config(char *pszColorDesc)
{
   BYTE btColor = 0x07;
   char szToken[40];
   char *pszStart=(char *)pszColorDesc;
   char *pszEnd;
   BYTE btLength;
   BYTE btIdentifier;
   BOOL bForeground = TRUE;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_color_config()");

   /* Initialize OpenDoros if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   while(*pszStart && *pszStart!=chColorCheck)
   {
      if(*pszStart == ' ' || *pszStart== '\t')
      {
         ++pszStart;
      }
      else
      {
         btLength = 0;
         pszEnd = (char *)pszStart;
         while(*pszEnd && *pszEnd != chColorCheck && *pszEnd != ' '
            && *pszEnd != '\t')
         {
            ++btLength;
            ++pszEnd;
         }

         if(btLength > 39) btLength = 39;
         strncpy(szToken, pszStart, btLength);
         szToken[btLength] = '\0';
         strupr(szToken);

         for(btIdentifier = 0; btIdentifier < 12; ++btIdentifier)
            if(strcmp(od_config_colours[btIdentifier], szToken) == 0)
            {
               if(btIdentifier <= 9)
               {
                  if(btIdentifier >= 8) btIdentifier -= 2;

                  if(bForeground)
                  {
                     bForeground=FALSE;
                     btColor &=~ 0x07;
                     btColor |= btIdentifier;
                  }
                  else
                  {
                     btColor &=~ 0x70;
                     btColor |= (btIdentifier << 4);
                  }
               }

               else if(btIdentifier == 10)
               {
                  btColor |= 0x08;
               }

               else if(btIdentifier == 11)
               {
                  btColor |= 0x80;
               }

               break;
            }

         pszStart = (char *)pszEnd;
      }
   }

   pchColorEndPos = (char *)pszStart;

   OD_API_EXIT();

   return(btColor);
}


/* ----------------------------------------------------------------------------
 * ODPagePrompt()
 *
 * Called to display the page prompt at the end of a screen of text. This page
 * prompt allows the user to stop further display, to display the next page,
 * or to display in continuous (non-stop) mode with page pausing disabled.
 *
 * Parameters: pbPausing   - Pointer to current page pausing enabled flag.
 *
 *     Return: FALSE if display should be continued, or TRUE to abort display.
 */
BOOL ODPagePrompt(BOOL *pbPausing)
{
   INT nPromptLength = strlen(od_control.od_continue);
   tODScrnTextInfo TextInfo;
   BOOL bToReturn = FALSE;
   char chKeyPressed;
   BYTE btCount;

   /* Return right away if page pausing is disabled. */
   if(!*pbPausing) return(FALSE);

   /* Get current text color. */
   ODScrnGetTextInfo(&TextInfo);

   /* Set to prompt color. */
   od_set_attrib(od_control.od_continue_col);

   /* Display page prompt string. */
   od_disp_str(od_control.od_continue);

   /* Restore original text color. */
   od_set_attrib(TextInfo.attribute);

   /* Loop until the user makes a valid choice. */
   for(;;)
   {
      /* Obtain the next key from the user. */
      chKeyPressed = od_get_key(TRUE);

      /* If user chooses to continue. */
      if(chKeyPressed == tolower(od_control.od_continue_yes) ||
         chKeyPressed == toupper(od_control.od_continue_yes) ||
         chKeyPressed == 13 ||
         chKeyPressed == ' ')
      {
         /* Remove the prompt and return. */
         goto finished_pausing;
      }

      /* If user requested nonstop display. */
      else if(chKeyPressed == tolower(od_control.od_continue_nonstop) ||
              chKeyPressed == toupper(od_control.od_continue_nonstop))
      {
         /* Disable page pausing. */
         *pbPausing = FALSE;

         /* Remove the prompt and return. */
         goto finished_pausing;
      }

      /* If user chooses to stop display. */
      else if(chKeyPressed == tolower(od_control.od_continue_no) ||
              chKeyPressed == toupper(od_control.od_continue_no) ||
              chKeyPressed == 's' || chKeyPressed == 'S' || chKeyPressed == 3
              || chKeyPressed == 11 || chKeyPressed == 0x18)
      {
         /* If we are operating in remote mode. */
         if(od_control.baud)
         {
            /* Clear the output buffer. */
            ODComClearOutbound(hSerialPort);
         }

         /* Tell the caller to stop displaying more text. */
         bToReturn = TRUE;

         /* Remove the prompt and return. */
         goto finished_pausing;
      }
   }

finished_pausing:
   /* Remove the pause prompt. */
   for(btCount = 0; btCount < nPromptLength; ++btCount)
   {
      od_disp_str(szBackspaceWithDelete);
   }

   return(bToReturn);
}


/* ----------------------------------------------------------------------------
 * od_control_get()
 *
 * Returns a pointer to the od_control structure containing information
 * and settings associated with the current session.
 *
 * Parameters: None.
 *
 *     Return: A pointer to the od_control structure associated with this
 *             session.
 */
ODAPIDEF tODControl * ODCALL od_control_get(void)
{
   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_disp_str()");

   return(&od_control);
}
