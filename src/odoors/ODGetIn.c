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
 *              Apr 11, 2005  6.23  SH   Fix hang forver on ESC press and latency timeout.
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
   {"\033[1~",  OD_KEY_HOME, FALSE}, /* Windows XP telnet.exe.  Is actually FIND */
   {"\033[2~",  OD_KEY_INSERT, FALSE},
   {"\033[3~",  OD_KEY_DELETE, FALSE},
   {"\033[4~",  OD_KEY_END, FALSE},  /* Windows XP telnet.exe.  Is actually SELECT */
   {"\033[5~",  OD_KEY_PGUP, FALSE},
   {"\033[6~",  OD_KEY_PGDN, FALSE},
   {"\033[17~", OD_KEY_F6, FALSE},
   {"\033[18~", OD_KEY_F7, FALSE},
   {"\033[19~", OD_KEY_F8, FALSE},
   {"\033[20~", OD_KEY_F9, FALSE},
   {"\033[21~", OD_KEY_F10, FALSE},
   {"\033[23~", OD_KEY_F11, FALSE},
   {"\033[24~", OD_KEY_F12, FALSE},

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
#if 0	// Unused...
static tODTimer SequenceFailTimer;
static BOOL bSequenceFromRemote;
static int nMatchedSequence = NO_MATCH;
static BOOL bTimerActive = FALSE;
#endif
static BOOL bDoorwaySequence = FALSE;
static BOOL bDoorwaySequencePending = FALSE;

/* Local private function prototypes. */
static int ODGetCodeIfLongest(WORD wFlags);
static int ODHaveStartOfSequence(WORD wFlags);
static int ODLongestFullCode(WORD wFlags);
static void ODShiftSeq(int chars);

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
   tODInputEvent LastInputEvent;
   int nSequence;
   int nSequenceLen;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_get_input()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Check for parameter validity. */
   if(pInputEvent == NULL)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Call the OpenDoors kernel, if applicable */
   CALL_KERNEL_IF_NEEDED();

   /* If you don't know better, it's a remote char */
   /* Local chars are always correctly received (no ANSI sequences) */
   LastInputEvent.bFromRemote = TRUE;

   if((!bDoorwaySequence) && bDoorwaySequencePending && (!szCurrentSequence[0])) {
      bDoorwaySequencePending=FALSE;
      bDoorwaySequence=TRUE;
   }

   /* If no pending input string, wait for the first keystroke */
   if((!szCurrentSequence[0]) && (!bDoorwaySequence)) {
      if(ODInQueueGetNextEvent(hODInputQueue, &LastInputEvent, TimeToWait)
            != kODRCSuccess) {
         OD_API_EXIT();
         return(FALSE);
      }

      /* If you have a *local* char, send it immediately */
      if((!LastInputEvent.bFromRemote) && (LastInputEvent.chKeyPress != 0)
#if 0
            && (LastInputEvent.EventType == EVENT_EXTENDED_KEY)) {
#else
            ) {
#endif
         memcpy(pInputEvent, &LastInputEvent, sizeof(tODInputEvent));
         OD_API_EXIT();
         return(TRUE);
      }

      /* IF the received char is a NULL, this is the start of a doorway char */
      if(!LastInputEvent.chKeyPress)
         bDoorwaySequence = TRUE;
      else {
         szCurrentSequence[0]=LastInputEvent.chKeyPress;
         szCurrentSequence[1]=0;
      }
   }

   nSequenceLen=strlen(szCurrentSequence);

   CALL_KERNEL_IF_NEEDED();

   /* If you don't have the start of a sequence, and it's not doorway mode, you have
    * a char.  It's that simple.
    */
   if((!bDoorwaySequence) && (!ODHaveStartOfSequence(wFlags))) {
      pInputEvent->chKeyPress = szCurrentSequence[0];
      pInputEvent->EventType = EVENT_CHARACTER;
      pInputEvent->bFromRemote = LastInputEvent.bFromRemote;
      /* Shift the sequence buffer over one */
      ODShiftSeq(1);
      OD_API_EXIT();
      return(TRUE);
   }

   /* Now... if the current sequence IS the longest valid one, return it
    * immediately.  If it's sequence leftovers, it HAS to be a remote key
    * since local chars are #1 always doorway mode and #2 have no delay
    * betwixt them.
    */
   if((nSequence = ODGetCodeIfLongest(wFlags)) != NO_MATCH) {
      pInputEvent->chKeyPress = aKeySequences[nSequence].chExtendedKey;
      pInputEvent->EventType = EVENT_EXTENDED_KEY;
      pInputEvent->bFromRemote = LastInputEvent.bFromRemote;
      /* Shift the sequence buffer... being sure to copy the terminator */
      ODShiftSeq(strlen(aKeySequences[nSequence].pszSequence));
      OD_API_EXIT();
      return(TRUE);
   }

   /* Now, continue adding chars, waiting at MOST MAX_CHARACTER_LATENCY between them */
   CALL_KERNEL_IF_NEEDED();
   while((!bDoorwaySequencePending)
            && (ODInQueueGetNextEvent(hODInputQueue, &LastInputEvent, MAX_CHARACTER_LATENCY)
            == kODRCSuccess)) {
      /* If you are looking for a doorway sequence, any char completes it (honest!) */
      /* Further, thanks to some lack of planning, it's EXACTLY THE SAME as the char,
       * only it's extended.
       */
      if(bDoorwaySequence) {
         memcpy(pInputEvent, &LastInputEvent, sizeof(tODInputEvent));
         pInputEvent->EventType = EVENT_EXTENDED_KEY;
         bDoorwaySequence=FALSE;
         OD_API_EXIT();
         return(TRUE);
      }

      /* If we get a 0, we *WILL BE* looking for a doorway sequence.  But NOT
       * until the current sequence buffer is drained!
       */
      if(LastInputEvent.chKeyPress == 0) {
         bDoorwaySequencePending=TRUE;
         break;
      }

      /* If you have a *local* char, send it immediately */
      if((!LastInputEvent.bFromRemote) && (LastInputEvent.chKeyPress != 0)
#if 0
            && (LastInputEvent.EventType == EVENT_EXTENDED_KEY)) {
#else
            ) {
#endif
         memcpy(pInputEvent, &LastInputEvent, sizeof(tODInputEvent));
         OD_API_EXIT();
         return(TRUE);
      }

      /* Put this char into the sequence buffer */
      szCurrentSequence[nSequenceLen++]=LastInputEvent.chKeyPress;
      szCurrentSequence[nSequenceLen]=0;

      /* When you have the longest possible sequence, you ARE done */
      if((nSequence = ODGetCodeIfLongest(wFlags)) != NO_MATCH) {
         pInputEvent->chKeyPress = aKeySequences[nSequence].chExtendedKey;
         pInputEvent->EventType = EVENT_EXTENDED_KEY;
         pInputEvent->bFromRemote = LastInputEvent.bFromRemote;
         /* Shift the sequence buffer... being sure to copy the terminator */
         ODShiftSeq(strlen(aKeySequences[nSequence].pszSequence));
         OD_API_EXIT();
         return(TRUE);
      }
      CALL_KERNEL_IF_NEEDED();
   }

   /* If we were looking for a doorway sequence, tough, we didn't get it. */
   if(bDoorwaySequence) {
      pInputEvent->chKeyPress = 0;
      pInputEvent->EventType = EVENT_CHARACTER;
      pInputEvent->bFromRemote = LastInputEvent.bFromRemote;
      bDoorwaySequence=FALSE;
      OD_API_EXIT();
      return(TRUE);
   }

   /* Now, if we have any kind of sequence, we'll settle for it. */
   if((nSequence = ODLongestFullCode(wFlags)) != NO_MATCH) {
      pInputEvent->chKeyPress = aKeySequences[nSequence].chExtendedKey;
      pInputEvent->EventType = EVENT_EXTENDED_KEY;
      pInputEvent->bFromRemote = LastInputEvent.bFromRemote;
      /* Shift the sequence buffer... being sure to copy the terminator */
      ODShiftSeq(strlen(aKeySequences[nSequence].pszSequence));
      OD_API_EXIT();
      return(TRUE);
   }

   /* If we don't have a complete sequence, send a single char */
   pInputEvent->chKeyPress = szCurrentSequence[0];
   if(szCurrentSequence[0]) {
      pInputEvent->EventType = EVENT_CHARACTER;
      pInputEvent->bFromRemote = LastInputEvent.bFromRemote;
      /* Shift the sequence buffer over one */
      ODShiftSeq(1);
   }
   OD_API_EXIT();
   /* Return false if something broke */
   return(pInputEvent->chKeyPress);
}

/* ----------------------------------------------------------------------------
 * ODLongestFullCode()                                 *** PRIVATE FUNCTION ***
 *
 * Return the index of the longest full code that matches the start of the
 * sequence buffer
 *
 * Parameters: wFlags from od_get_input()
 *
 *     Return: void
 */
static int ODLongestFullCode(WORD wFlags)
{
   int CurrLen=0;
   int seqlen;
   int i;
   int retval=NO_MATCH;;

   if(wFlags & GETIN_RAW)
      return(NO_MATCH);
   for(i = 0; i < DIM(aKeySequences); ++i) {
      if((wFlags & GETIN_RAWCTRL) && aKeySequences[i].bIsControlKey) {
         continue;
      }
      seqlen=strlen(aKeySequences[i].pszSequence);
      if(seqlen>CurrLen) {
         if(strncmp(aKeySequences[i].pszSequence, szCurrentSequence, seqlen)==0) {
            retval=i;
            CurrLen=seqlen;
         }
      }
   }

   return(retval);
}

/* ----------------------------------------------------------------------------
 * ODHaveStartOfSequence()                             *** PRIVATE FUNCTION ***
 *
 * If the current sequence buffer is the start of a valid sequence, return TRUE
 *
 * Parameters: wFlags from od_get_input()
 *
 *     Return: void
 */
static int ODHaveStartOfSequence(WORD wFlags)
{
   int seqlen1;
   int seqlen2;
   int i;

   if(wFlags & GETIN_RAW)
      return(FALSE);
   seqlen1=strlen(szCurrentSequence);
   for(i = 0; i < DIM(aKeySequences); ++i) {
      if((wFlags & GETIN_RAWCTRL) && aKeySequences[i].bIsControlKey) {
         continue;
      }
      seqlen2=strlen(aKeySequences[i].pszSequence);
      if(seqlen1<seqlen2)
         seqlen2=seqlen1;
      if(strncmp(aKeySequences[i].pszSequence, szCurrentSequence, seqlen2)==0) {
         return(TRUE);
      }
   }

   return(FALSE);
}

/* ----------------------------------------------------------------------------
 * ODGetCodeIfLongest()                                *** PRIVATE FUNCTION ***
 *
 * Returns the index of the entry in the sequence buffer only if there
 * are no longer sequences that could start the same way.
 *
 * Parameters: wFlags from od_get_input()
 *
 *     Return: void
 */
static int ODGetCodeIfLongest(WORD wFlags)
{
   int CurrLen=0;
   int seqlen1;
   int seqlen2;
   int i;
   int retval=NO_MATCH;;

   if(wFlags & GETIN_RAW)
      return(NO_MATCH);
   seqlen1=strlen(szCurrentSequence);
   for(i = 0; i < DIM(aKeySequences); ++i) {
      if((wFlags & GETIN_RAWCTRL) && aKeySequences[i].bIsControlKey) {
         continue;
      }
      seqlen2=strlen(aKeySequences[i].pszSequence);
      if(seqlen2>CurrLen) {
         if(seqlen2<=seqlen1) {	/* The sequence would be completed in buffer */
            if(strncmp(aKeySequences[i].pszSequence, szCurrentSequence, seqlen2)==0) {
               retval=i;
               CurrLen=seqlen2;
            }
         }
         else {		/* Possible partial sequence */
            if(strncmp(aKeySequences[i].pszSequence, szCurrentSequence, seqlen1)==0) {
               return(NO_MATCH);
            }
         }
      }
   }

   return(retval);
}

/* ----------------------------------------------------------------------------
 * ODShiftSeq()                                        *** PRIVATE FUNCTION ***
 *
 * Shifts szCurrentSequence left the specified number of chars
 *
 * Parameters: int chars to shift
 *
 *     Return: void
 */
static void ODShiftSeq(int chars)
{
   char *in;
   char *out;

   if(!chars)
      return;
   out=szCurrentSequence;
   in=out+chars;

   if(in>strchr(szCurrentSequence,0))
      return;
   while(*in) {
      *(out++)=*(in++);
   }
   *out=*in;
}

