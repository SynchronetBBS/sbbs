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
 *        File: ODGetIn.c
 *
 * Description: Implements the od_get_input() function, which obtains the
 *              next input event of any type, optionally performing
 *              translation.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Jan 04, 1996  6.00  BP   Created.
 *              Jan 30, 1996  6.00  BP   Tweaked TREAT_ESC_AS_ANSI_TIMEOUT.
 *              Jan 30, 1996  6.00  BP   Replaced od_yield() with od_sleep().
 *              Jan 30, 1996  6.00  BP   Add ODInQueueGetNextEvent() timeout.
 *              Jan 31, 1996  6.00  BP   Added timeout for od_get_input().
 *              Feb 13, 1996  6.00  BP   Added od_get_input() flags parameter.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 19, 1996  6.00  BP   Switched to table implementation.
 *              Feb 25, 1996  6.00  BP   Added new control sequences to table.
 *              Feb 27, 1996  6.00  BP   Added od_max_key_latency.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stddef.h>
#include <string.h>

#include "OpenDoor.h"
#include "ODInQue.h"
#include "ODCore.h"
#include "ODKrnl.h"

/* Control sequence table definitions. */
typedef struct
{
   char *pszSequence;
   char chExtendedKey;
   BOOL bIsControlKey;
} tODKeySequence;

tODKeySequence aKeySequences[] =
{
   /* VT-52 control sequences. */
   {"\033A", OD_KEY_UP, FALSE},
   {"\033B", OD_KEY_DOWN, FALSE},
   {"\033C", OD_KEY_RIGHT, FALSE},
   {"\033D", OD_KEY_LEFT, FALSE},
   {"\033H", OD_KEY_HOME, FALSE},
   {"\033K", OD_KEY_END, FALSE},
   {"\033P", OD_KEY_F1, FALSE},
   {"\033Q", OD_KEY_F2, FALSE},
   {"\033?w", OD_KEY_F3, FALSE},
   {"\033?x", OD_KEY_F4, FALSE},
   {"\033?t", OD_KEY_F5, FALSE},
   {"\033?u", OD_KEY_F6, FALSE},
   {"\033?q", OD_KEY_F7, FALSE},
   {"\033?r", OD_KEY_F8, FALSE},
   {"\033?p", OD_KEY_F9, FALSE},

   /* Control sequences common to VT-100/VT-102/VT-220/VT-320/ANSI. */
   {"\033[A", OD_KEY_UP, FALSE},
   {"\033[B", OD_KEY_DOWN, FALSE},
   {"\033[C", OD_KEY_RIGHT, FALSE},
   {"\033[D", OD_KEY_LEFT, FALSE},
   {"\033[M", OD_KEY_PGUP, FALSE},
   {"\033[H\x1b[2J", OD_KEY_PGDN, FALSE},
   {"\033[H", OD_KEY_HOME, FALSE},
   {"\033[K", OD_KEY_END, FALSE},
   {"\033OP", OD_KEY_F1, FALSE},
   {"\033OQ", OD_KEY_F2, FALSE},
   {"\033OR", OD_KEY_F3, FALSE},
   {"\033OS", OD_KEY_F4, FALSE},

   /* VT-220/VT-320 specific control sequences. */
   {"\033[17~", OD_KEY_F6, FALSE},
   {"\033[18~", OD_KEY_F7, FALSE},
   {"\033[19~", OD_KEY_F8, FALSE},
   {"\033[20~", OD_KEY_F9, FALSE},
   {"\033[21~", OD_KEY_F10, FALSE},

   /* ANSI-specific control sequences. */
   {"\033[L", OD_KEY_HOME, FALSE},
   {"\033Ow", OD_KEY_F3, FALSE},
   {"\033Ox", OD_KEY_F4, FALSE},
   {"\033Ot", OD_KEY_F5, FALSE},
   {"\033Ou", OD_KEY_F6, FALSE},
   {"\033Oq", OD_KEY_F7, FALSE},
   {"\033Or", OD_KEY_F8, FALSE},
   {"\033Op", OD_KEY_F9, FALSE},

   /* PROCOMM-specific control sequences (non-keypad alternatives). */
   {"\033OA", OD_KEY_UP, FALSE},
   {"\033OB", OD_KEY_DOWN, FALSE},
   {"\033OC", OD_KEY_RIGHT, FALSE},
   {"\033OD", OD_KEY_LEFT, FALSE},
   {"\033OH", OD_KEY_HOME, FALSE},
   {"\033OK", OD_KEY_END, FALSE},

   /* Other standard control sequences. */
   {"\x16\t", OD_KEY_INSERT, TRUE},

   /* OpenDoors-specific alternatives. */
   {"\x7f", OD_KEY_DELETE, FALSE},
   {"\x5", OD_KEY_UP, TRUE},
   {"\x18", OD_KEY_DOWN, TRUE},
   {"\x13", OD_KEY_LEFT, TRUE},
   {"\x4", OD_KEY_RIGHT, TRUE},
   {"\x7", OD_KEY_DELETE, TRUE},
   {"\x16", OD_KEY_INSERT, TRUE},
};

/* Constant that indicates no match has been found. */
#define NO_MATCH DIM(aKeySequences)

/* Configurable constants. */

/* The time, in milliseconds, to wait for a second character in a control */
/* sequence, before deciding that none is going to come.                  */
#define MAX_CHARACTER_LATENCY 250

/* Size of inbound sequence buffer. */
#define SEQUENCE_BUFFER_SIZE 10

/* Current control sequence received state. */
static char szCurrentSequence[SEQUENCE_BUFFER_SIZE] = "";
static tODTimer SequenceFailTimer;
static BOOL bSequenceFromRemote;
static int nMatchedSequence = NO_MATCH;
static BOOL bDoorwaySequence = FALSE;
static BOOL bTimerActive = FALSE;

/* Local private function prototypes. */
static void ODGetInResetSequence(void);



/* ----------------------------------------------------------------------------
 * od_get_input()
 *
 * Obtains the next input event of any type, optionally performing
 * translation on input events.
 *
 * Parameters: pInputEvent  - Pointer to a tODInputEvent structure, which
 *                            will be filled by information on the next input
 *                            event, if any is obtained.
 *
 *             TimeToWait   - Number of milliseconds to wait for input to be
 *                            available. A value of 0 causes od_get_input()
 *                            to return immediately if no input is waiting,
 *                            while a value of OD_NO_TIMEOUT causes the
 *                            function to never return unless input has been
 *                            obtained.
 *
 *             wFlags       - Flags which customize od_get_input()'s behaviour.
 *
 *     Return: TRUE if an input event was obtained, FALSE if not.
 */
ODAPIDEF BOOL ODCALL od_get_input(tODInputEvent *pInputEvent,
   tODMilliSec TimeToWait, WORD wFlags)
{
   BOOL bGotEvent;
   tODTimer TotalWaitTimer;
   tODInputEvent LastInputEvent;
   tODMilliSec TimeLeft;
   tODMilliSec MaximumWait;
   int nSequence;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_get_input()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Check for parameter validity. */
   if(pInputEvent == NULL)
   {
      od_control.od_error = ERR_PARAMETER;
      goto FunctionExit;
   }

   /* Call the OpenDoors kernel, if applicable */
   CALL_KERNEL_IF_NEEDED();

   /* Start a timer if the caller has specified a timeout. */
   if(TimeToWait != 0 && TimeToWait != OD_NO_TIMEOUT)
   {
      ODTimerStart(&TotalWaitTimer, TimeToWait);
   }

   /* Loop until we have a valid input event, or until we should exit for */
   /* some other reason.                                                  */
   bGotEvent = FALSE;
   while(!bGotEvent)
   {
      /* If we aren't supposed to wait for input, then fail if there is */
      /* nothing waiting in the queue.                                  */
      if(TimeToWait == 0)
      {
         if(!ODInQueueWaiting(hODInputQueue)) break;
      }

      /* If a maximum wait timeout has been specified, then determine how */
      /* much of that time is left.                                       */
      if(TimeToWait != 0 && TimeToWait != OD_NO_TIMEOUT)
      {
         TimeLeft = ODTimerLeft(&TotalWaitTimer);
      }
      else
      {
         TimeLeft = OD_NO_TIMEOUT;
      }

      /* Determine the maximum time to wait for the next input event. */
      if(bTimerActive)
      {
         MaximumWait = MIN(TimeLeft, ODTimerLeft(&SequenceFailTimer));
      }
      else
      {
         MaximumWait = TimeLeft;
      }

      /* Otherwise, attempt to obtain the next input event. */
      if(MaximumWait == 0 ||
         ODInQueueGetNextEvent(hODInputQueue, &LastInputEvent, MaximumWait)
         != kODRCSuccess)
      {
         if(TimeToWait != OD_NO_TIMEOUT ||
            (bTimerActive && ODTimerElapsed(&SequenceFailTimer)
            && szCurrentSequence[0] == 27 && strlen(szCurrentSequence) == 1))
         {
            /* If no input event could be obtained within the specified */
            /* then return with failure.                                */
            break;
         }
      }

      /* If no translation is required, then just return this event. */
      if((wFlags & GETIN_RAW)
         || LastInputEvent.EventType != EVENT_CHARACTER)
      {
         bGotEvent = TRUE;
         memcpy(pInputEvent, &LastInputEvent, sizeof(tODInputEvent));
      }
      else
      {
         /* We have a character event in translation mode. */

         /* First, check whether this is a doorway sequence, which is */
         /* handled differently than all other control sequences,     */
         if(bDoorwaySequence)
         {
            pInputEvent->bFromRemote = LastInputEvent.bFromRemote;
            pInputEvent->chKeyPress = LastInputEvent.chKeyPress;
            pInputEvent->EventType = EVENT_EXTENDED_KEY;
            bGotEvent = TRUE;
            break;
         }
         else if(LastInputEvent.chKeyPress == '\0')
         {
            bDoorwaySequence = TRUE;
            continue;
         }

         /* If sequence buffer is full, then reset the buffer and coninue. */
         /* Possible improvement: Is this what really should be done?      */
         if(strlen(szCurrentSequence) >= SEQUENCE_BUFFER_SIZE - 1)
         {
            ODGetInResetSequence();
            continue;
         }

         /* Otherwise, add this character to the sequence buffer. */
         szCurrentSequence[strlen(szCurrentSequence)] =
            LastInputEvent.chKeyPress;
         bSequenceFromRemote = LastInputEvent.bFromRemote;

         /* Search for a matching control sequence. */
         for(nMatchedSequence = 0; nMatchedSequence < DIM(aKeySequences);
            ++nMatchedSequence)
         {
            /* Skip sequences that use control characters if required. */
            if((wFlags & GETIN_RAWCTRL)
               && aKeySequences[nMatchedSequence].bIsControlKey)
            {
               continue;
            }

            /* Stop loop if we have a match. */
            if(strcmp(szCurrentSequence,
               aKeySequences[nMatchedSequence].pszSequence) == 0)
            {
               break;
            }
         }

         /* If we have a full match of a control sequence. */
         if(nMatchedSequence != NO_MATCH)
         {
            /* Check whether there is another, longer sequence that may */
            /* match.                                                   */
            if(!bTimerActive || !ODTimerElapsed(&SequenceFailTimer))
            {
               for(nSequence = 0; nSequence < DIM(aKeySequences);
                  ++nSequence)
               {
                  /* Skip sequences that use control characters if required. */
                  if((wFlags & GETIN_RAWCTRL)
                     && aKeySequences[nSequence].bIsControlKey)
                  {
                     continue;
                  }

                  /* Stop loop if we have found another possible match. */
                  if(strlen(szCurrentSequence) <
                     strlen(aKeySequences[nSequence].pszSequence)

                     && strncmp(szCurrentSequence,
                     aKeySequences[nSequence].pszSequence,
                     strlen(szCurrentSequence)) == 0

                     && nSequence != nMatchedSequence)
                  {
                     break;
                  }
               }

               /* If there is another possible match, we cannot determine */
               /* whether this is the sequence we want until the maximum  */
               /* character latency has passed.                           */
               if(nSequence < DIM(aKeySequences)) continue;
            }

            /* Return resulting event. */
            pInputEvent->bFromRemote = bSequenceFromRemote;
            pInputEvent->chKeyPress =
               aKeySequences[nMatchedSequence].chExtendedKey;
            pInputEvent->EventType = EVENT_EXTENDED_KEY;
            bGotEvent = TRUE;
            break;
         }

         /* Start a timer that will elapse if no further control sequence */
         /* characters are received within the maximum latency time.      */
         if(od_control.od_max_key_latency != 0)
         {
            ODTimerStart(&SequenceFailTimer, od_control.od_max_key_latency);
         }
         else
         {
            ODTimerStart(&SequenceFailTimer, MAX_CHARACTER_LATENCY);
         }
         bTimerActive = TRUE;

         /* We only get here if we don't fully match a control sequence.   */

         /* If this was the first character of a control sequence, we only */
         /* continue looking for the rest of the sequence if this is a     */
         /* possible start of the sequence.                                */
         if(strlen(szCurrentSequence) == 1)
         {
            for(nSequence = 0; nSequence < DIM(aKeySequences); ++nSequence)
            {
               /* Skip sequences that use control characters if required. */
               if((wFlags & GETIN_RAWCTRL)
                  && aKeySequences[nSequence].bIsControlKey)
               {
                  continue;
               }

               /* Stop loop if we have found a complete match. */
               if(szCurrentSequence[0] ==
                  aKeySequences[nSequence].pszSequence[0])
               {
                  break;
               }
            }

            /* If this is not a possible control sequence start, then return */
            /* this event in unmodified form.                                */
            if(nSequence == NO_MATCH)
            {
               bGotEvent = TRUE;
               memcpy(pInputEvent, &LastInputEvent, sizeof(tODInputEvent));
            }
         }
      }
   }

FunctionExit:
   /* If we don't have an input event to return. */
   if(!bGotEvent)
   {
      /* If we have found a complete sequence already, and the seqeuence */
      /* timer has elapsed, then return that event.                      */
      if(bTimerActive && ODTimerElapsed(&SequenceFailTimer))
      {
         if(nMatchedSequence != NO_MATCH)
         {
            /* Return resulting event. */
            pInputEvent->bFromRemote = bSequenceFromRemote;
            pInputEvent->chKeyPress =
               aKeySequences[nMatchedSequence].chExtendedKey;
            pInputEvent->EventType = EVENT_EXTENDED_KEY;
            bGotEvent = TRUE;
         }
         else
         {
            /* If the sequence began with an escape key, then return an escape */
            /* key event.                                                      */
            if(szCurrentSequence[0] == 27 && strlen(szCurrentSequence) == 1)
            {
               pInputEvent->bFromRemote = bSequenceFromRemote;
               pInputEvent->chKeyPress = szCurrentSequence[0];
               pInputEvent->EventType = EVENT_CHARACTER;
               bGotEvent = TRUE;
            }
         }
      }
   }

   /* On success, reset current sequence buffer. */
   if(bGotEvent)
   {
      ODGetInResetSequence();
   }

   /* Exit function with appropriate return value. */
   OD_API_EXIT();

   return(bGotEvent);
}


/* ----------------------------------------------------------------------------
 * ODGetInResetSequence()                              *** PRIVATE FUNCTION ***
 *
 * Resets (empties) the current sequence buffer.
 *
 * Parameters: None
 *
 *     Return: void
 */
static void ODGetInResetSequence(void)
{
   memset(szCurrentSequence, '\0', SEQUENCE_BUFFER_SIZE);
   nMatchedSequence = NO_MATCH;
   bDoorwaySequence = FALSE;
   bTimerActive = FALSE;
}
