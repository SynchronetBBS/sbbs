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
 *        File: ODAuto.c
 *
 * Description: Implements od_autodetect() for automatic detection of
 *              terminal emulation supported by remote system.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Oct 14, 1994  6.00  BP   Standardized coding style.
 *              Dec 31, 1994  6.00  BP   Use new millisecond timer functions.
 *              Nov 12, 1995  6.00  BP   32-bit portability.
 *              Nov 13, 1995  6.00  BP   Fixed non-functioning RIP autodetect.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <ctype.h>

#include "OpenDoor.h"
#include "ODStr.h"
#include "ODTypes.h"
#include "ODGen.h"
#include "ODPlat.h"
#include "ODCore.h"
#include "ODKrnl.h"


/* Private function prototypes. */
static char ODWaitNoCase(char *pszWaitFor, tODMilliSec WaitTime);


/* Number of attempts and timeout values for testing each terminal emulation */
/* protocol.                                                                 */
#define ANSI_TRIES      1
#define ANSI_WAIT       660                         /* Time in milliseconds. */
#define RIP_TRIES       1
#define RIP_WAIT        660                         /* Time in milliseconds. */

/* Strings to use for autodetection. */
#define ANSI_QUERY      "\x1b[6n\r    \r"
#define ANSI_RESPONSE   "\x1b["
#define RIP_QUERY       "\r\x1b[!\r   \r"
#define RIP_RESPONSE    "RIP"

/* Maximum number of characters to match with _waitnocase(). */
#define MATCH_LEN       3


/* ----------------------------------------------------------------------------
 * od_autodetect()
 *
 * Determines the terminal emulation capabilities of the remote communications
 * software, when possible. Turns on ANSI and/or RIP modes if they are
 * supported by the remote system.
 *
 * Parameters: nFlags - Currently unused.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL od_autodetect(INT nFlags)
{
   INT nCount;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_autodetect()");

   /* Initialize OpenDoors if it hasn't aready been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Temporary code that will be optimized out, which prevents a compiler   */
   /* warning from being generated for the currently unused flags parameter. */
   (void)nFlags;

   /* If operating in local mode, turn on ANSI mode, but not RIP. */
   if(od_control.baud == 0)
   {
      od_control.user_ansi = TRUE;
      OD_API_EXIT();
      return;
   }

   /* If user_ansi is not set, attempt to determine ANSI capabilities. */
   if(!od_control.user_ansi)
   {
      /* Clear inbound keyboard buffer. */
      od_clear_keybuffer();

      /* Try twice to test ANSI capabilities. */
      for(nCount = 0; nCount < ANSI_TRIES; ++nCount)
      {
         /* Send a string that an ANSI capable terminal will usually */
         /* respond to. */
         od_disp(ANSI_QUERY, strlen(ANSI_QUERY), FALSE);

         /* Wait for response expected from an ANSI terminal, for up to */
         /* 12/18.2 second. */
         if(ODWaitNoCase(ANSI_RESPONSE, ANSI_WAIT))
         {
            /* If expected sequence was received, turn on ANSI mode and */
            /* exit the loop. */
            od_control.user_ansi = TRUE;
            break;
         }
      }
      od_clear_keybuffer();
   }

   /* If user_rip is not set, attempt to determine RIP capabilities. */
   if(!od_control.user_rip)
   {
      /* Clear inbound keyboard buffer. */
      od_clear_keybuffer();

      /* Try twice to test RIP capabilities. */
      for(nCount = 0; nCount < RIP_TRIES; ++nCount)
      {
         /* Send a string that a RIP capable terminal will usually */
         /* respond to. */
         od_disp(RIP_QUERY, strlen(RIP_QUERY), FALSE);

         /* Wait for response expected from a RIP terminal. */
         if(ODWaitNoCase(RIP_RESPONSE, RIP_WAIT))
         {
            /* If expected sequence was received, turn on RIP mode and */
            /* exit the loop. */
            od_control.user_rip = TRUE;
            break;
         }
      }
      od_clear_keybuffer();
   }

   OD_API_EXIT();
}


/* ----------------------------------------------------------------------------
 * ODWaitNoCase()                                      *** PRIVATE FUNCTION ***
 *
 * Waits up to the specified maximum time for a specified string to be sent
 * from the remote system. String matching is not case sensitive.
 *
 * Parameters: pszWaitFor - String to wait for.
 *
 *             WaitTime   - Maximum time, in milliseconds, to wait.
 *
 *     Return: TRUE on success, FALSE on failure.
 */
static char ODWaitNoCase(char *pszWaitFor, tODMilliSec WaitTime)
{
   tODTimer Timer;
   char szReceived[MATCH_LEN + 1];
   int nCount;
   char chReceived;
   int nMatchChars = MIN(MATCH_LEN, strlen(pszWaitFor));

   ASSERT(pszWaitFor != NULL);
   ASSERT(strlen(pszWaitFor) != 0);
   ASSERT(WaitTime >= 0);

   ODTimerStart(&Timer, WaitTime);

   for(nCount = 0; nCount <= MATCH_LEN; ++nCount)
   {
      szReceived[nCount] = '\0';
   }

   do
   {
      if((chReceived = od_get_key(FALSE)) != 0)
      {
         for(nCount = 0; nCount < MATCH_LEN - 1; ++ nCount)
         {
            szReceived[nCount] = szReceived[nCount + 1];
         }
         szReceived[MATCH_LEN - 1] = chReceived;

         if(strnicmp(szReceived + (MATCH_LEN - nMatchChars), pszWaitFor,
            nMatchChars) == 0)
         {
            return(TRUE);
         }
      }
   } while(!ODTimerElapsed(&Timer));

   return(FALSE);
}
