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
 *        File: ODInQue.h
 *
 * Description: OpenDoors input queue management. This input queue is where
 *              all input events (e.g. keystrokes) from both local and remote
 *              systems are combined into a single stream.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Nov 16, 1995  6.00  BP   Created.
 *              Nov 17, 1995  6.00  BP   Added multithreading support.
 *              Jan 04, 1996  6.00  BP   tODInQueueEvent -> tODInputEvent.
 *              Jan 30, 1996  6.00  BP   Replaced od_yield() with od_sleep().
 *              Jan 30, 1996  6.00  BP   Add semaphore timeout.
 *              Jan 30, 1996  6.00  BP   Add ODInQueueGetNextEvent() timeout.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdlib.h>
#include <string.h>

#include "OpenDoor.h"
#include "ODGen.h"
#include "ODInQue.h"
#include "ODPlat.h"
#include "ODKrnl.h"


/* Input queue handle structure. */
typedef struct
{
   tODInputEvent *paEvents;
   INT nQueueEntries;
   INT nInIndex;
   INT nOutIndex;
   time_t nLastActivityTime;
#ifdef OD_MULTITHREADED
   tODSemaphoreHandle hItemCountSemaphore;
   tODSemaphoreHandle hAddEventSemaphore;
#endif /* OD_MULTITHREADED */
} tInputQueueInfo;


/* ----------------------------------------------------------------------------
 * ODInQueueAlloc()
 *
 * Allocates a new input queue.
 *
 * Parameters: phInQueue         - Pointer to location where a handle to the
 *                                 newly allocated input queue should be
 *                                 stored.
 *
 *             nInitialQueueSize - The minimum number of events that the
 *                                 input queue should be able to hold.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODInQueueAlloc(tODInQueueHandle *phInQueue, INT nInitialQueueSize)
{
   tInputQueueInfo *pInputQueueInfo = NULL;
   tODInputEvent *pInputQueue = NULL;
   tODResult Result = kODRCNoMemory;

   ASSERT(phInQueue != NULL);

   if(phInQueue == NULL) return(kODRCInvalidCall);

   /* Attempt to allocate a serial port information structure. */
   pInputQueueInfo = malloc(sizeof(tInputQueueInfo));

   /* If memory allocation failed, return with failure. */
   if(pInputQueueInfo == NULL) goto CleanUp;

   /* Initialize semaphore handles to NULL. */
#ifdef OD_MULTITHREADED
   pInputQueueInfo->hItemCountSemaphore = NULL;
   pInputQueueInfo->hAddEventSemaphore = NULL;
#endif /* OD_MULTITHREADED */
   
   /* Attempt to allocate space for the queue itself. */
   pInputQueue = calloc(nInitialQueueSize, sizeof(tODInputEvent));
   if(pInputQueue == NULL) goto CleanUp;

   /* Create semaphores if this is a multithreaded platform. */
#ifdef OD_MULTITHREADED
   if(ODSemaphoreAlloc(&pInputQueueInfo->hItemCountSemaphore, 0,
      nInitialQueueSize) != kODRCSuccess)
   {
      goto CleanUp;
   }

   if(ODSemaphoreAlloc(&pInputQueueInfo->hAddEventSemaphore, 1, 1)
      != kODRCSuccess)
   {
      goto CleanUp;
   }
#endif /* OD_MULTITHREADED */

   /* Initialize input queue information structure. */
   pInputQueueInfo->paEvents = pInputQueue;
   pInputQueueInfo->nQueueEntries = nInitialQueueSize;
   pInputQueueInfo->nInIndex = 0;
   pInputQueueInfo->nOutIndex = 0;

   /* Convert intut queue information structure pointer to a handle. */
   *phInQueue = ODPTR2HANDLE(pInputQueueInfo, tInputQueueInfo);

   /* Reset the time of the last activity. */
   ODInQueueResetLastActivity(*phInQueue);

   Result = kODRCSuccess;

CleanUp:
   if(Result != kODRCSuccess)
   {
#ifdef OD_MULTITHREADED
      if(pInputQueueInfo != NULL
         && pInputQueueInfo->hItemCountSemaphore != NULL)
      {
         ODSemaphoreFree(pInputQueueInfo->hItemCountSemaphore);
      }

      if(pInputQueueInfo != NULL
         && pInputQueueInfo->hAddEventSemaphore != NULL)
      {
         ODSemaphoreFree(pInputQueueInfo->hAddEventSemaphore);
      }
#endif /* OD_MULTITHREADED */

      if(pInputQueue != NULL) free(pInputQueue);
      if(pInputQueueInfo != NULL) free(pInputQueueInfo);
      *phInQueue = ODPTR2HANDLE(NULL, tInputQueueInfo);
   }

   /* Return with the appropriate result code. */
   return(Result);
}


/* ----------------------------------------------------------------------------
 * ODInQueueFree()
 *
 * Destroys an input queue that was previously created by ODInQueueAlloc().
 *
 * Parameters: hInQueue - Handle to the input queue to destroy.
 *
 *     Return: void
 */
void ODInQueueFree(tODInQueueHandle hInQueue)
{
   tInputQueueInfo *pInputQueueInfo = ODHANDLE2PTR(hInQueue, tInputQueueInfo);

   ASSERT(pInputQueueInfo != NULL);

   /* Deallocate semaphores, if appropriate. */
#ifdef OD_MULTITHREADED
   ASSERT(pInputQueueInfo->hItemCountSemaphore != NULL);
   ODSemaphoreFree(pInputQueueInfo->hItemCountSemaphore);
#endif /* OD_MULTITHREADED */

   /* Deallocate the input queue itself. */
   ASSERT(pInputQueueInfo->paEvents != NULL);
   free(pInputQueueInfo->paEvents);

   /* Deallocate port information structure. */
   free(pInputQueueInfo);
}


/* ----------------------------------------------------------------------------
 * ODInQueueWaiting()
 *
 * Determines whether or not an event is currently waiting in the input queue.
 *
 * Parameters: hInQueue - Handle to the input queue to check.
 *
 *     Return: TRUE if there is one or more waiting events, or FALSE if the
 *             queue is empty.
 */
BOOL ODInQueueWaiting(tODInQueueHandle hInQueue)
{
   tInputQueueInfo *pInputQueueInfo = ODHANDLE2PTR(hInQueue, tInputQueueInfo);
   BOOL bEventWaiting;

   ASSERT(pInputQueueInfo != NULL);

   /* There is data waiting in the queue if the in index is not equal to */
   /* the out index.                                                     */
   bEventWaiting = (pInputQueueInfo->nInIndex != pInputQueueInfo->nOutIndex);

   return(bEventWaiting);
}


/* ----------------------------------------------------------------------------
 * ODInQueueAddEvent()
 *
 * Adds a new event to the input queue.
 *
 * Parameters: hInQueue  - Handle to the input queue to add an event to.
 *
 *             pEvent    - Pointer to the event structure to obtain the
 *                         event information from.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODInQueueAddEvent(tODInQueueHandle hInQueue,
   tODInputEvent *pEvent)
{
   tInputQueueInfo *pInputQueueInfo = ODHANDLE2PTR(hInQueue, tInputQueueInfo);
   INT nNextInPos;

   ASSERT(pInputQueueInfo != NULL);
   ASSERT(pEvent != NULL);
   if(pInputQueueInfo == NULL || pEvent == NULL) return(kODRCInvalidCall);

   /* Serialize access to add event function. */
#ifdef OD_MULTITHREADED
   ODSemaphoreDown(pInputQueueInfo->hAddEventSemaphore, OD_NO_TIMEOUT);
#endif /* OD_MULTITHREADED */

   /* Reset the time of the last activity. */
   ODInQueueResetLastActivity(hInQueue);

   /* Determine what the next in index would be after this addition to the */
   /* queue.                                                               */
   nNextInPos = (pInputQueueInfo->nInIndex + 1)
      % pInputQueueInfo->nQueueEntries;

   /* If the queue is full, then return an out of space error. */
   if(nNextInPos == pInputQueueInfo->nOutIndex)
   {
      /* Allow further access to input queue. */
#ifdef OD_MULTITHREADED
      ODSemaphoreUp(pInputQueueInfo->hAddEventSemaphore, 1);
#endif /* OD_MULTITHREADED */

      return(kODRCNoMemory);
   }

   /* Otherwise, add the new event to the input queue. */
   memcpy(&pInputQueueInfo->paEvents[pInputQueueInfo->nInIndex], pEvent,
      sizeof(tODInputEvent));

   /* Update queue in index. */
   pInputQueueInfo->nInIndex = nNextInPos;

   /* Increment queue items count semaphore. */
#ifdef OD_MULTITHREADED
   ODSemaphoreUp(pInputQueueInfo->hItemCountSemaphore, 1);
#endif /* OD_MULTITHREADED */

   /* Allow further access to add event function. */
#ifdef OD_MULTITHREADED
   ODSemaphoreUp(pInputQueueInfo->hAddEventSemaphore, 1);
#endif /* OD_MULTITHREADED */

   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODInQueueGetNextEvent()
 *
 * Obtains the next event from the input queue. If no events are currently
 * waiting in the input queue, this function blocks until an item is added
 * to the queue, or the maximum wait time is reached.
 *
 * Parameters: hInQueue - Handle to the input queue to obtain the next event
 *                        from.
 *
 *             pEvent   - Pointer to structure to store input event information
 *                        in.
 *
 *             Timeout  - Maximum time, in milliseconds, to wait for next input
 *                        event. A value of OD_NO_TIMEOUT causes this function
 *                        to only return when an input event is obtained.
 *
 *     Return: kODRCSuccess on succes, or kODRCTimeout if the maximum wait time
 *             is exceeded.
 */
tODResult ODInQueueGetNextEvent(tODInQueueHandle hInQueue,
   tODInputEvent *pEvent, tODMilliSec Timeout)
{
   tInputQueueInfo *pInputQueueInfo = ODHANDLE2PTR(hInQueue, tInputQueueInfo);

   ASSERT(pInputQueueInfo != NULL);
   ASSERT(pEvent != NULL);

#ifdef OD_MULTITHREADED

   /* In multithreaded implementations, we wait for there to be an item in  */
   /* the queue by decrementing the queue size semaphore. This will cause   */
   /* this thread to be blocked until an event is added to the queue, if it */
   /* is currently empty.                                                   */

   if(ODSemaphoreDown(pInputQueueInfo->hItemCountSemaphore, Timeout)==kODRCTimeout)
      return(kODRCTimeout);

#else /* !OD_MULTITHREADED */

   /* In non-multithreaded implementations, we check queue in and out     */
   /* indicies to determine whether there are any events waiting in the   */
   /* queue. If the queue is empty we loop, calling od_kernel() to check  */
   /* for new events and od_yeild() to give more time to other processors */
   /* if there is nothing for us to do, until an event is added to the    */
   /* queue.                                                              */
   if(pInputQueueInfo->nInIndex == pInputQueueInfo->nOutIndex)
   {
      tODTimer Timer;

      /* If a timeout has been specified, then start timer to keep track */
      /* of how long we have been waiting.                               */
      if(Timeout != 0 && Timeout != OD_NO_TIMEOUT)
      {
         ODTimerStart(&Timer, Timeout);
      }

      /* As soon as we see that there is nothing in the queue, we do an */
      /* od_kernel() call to check for new input.                       */
      CALL_KERNEL_IF_NEEDED();

      /* As long as we don't have new input, we loop, yielding to other */
      /* processes, and then giving od_kernel() a chance to run.        */
      while(pInputQueueInfo->nInIndex == pInputQueueInfo->nOutIndex)
      {
         /* If a timeout has been specified, then ensure that the maximum */
         /* wait time has not elapsed.                                    */
         if(Timeout != 0 && Timeout != OD_NO_TIMEOUT
            && ODTimerElapsed(&Timer))
         {
            return(kODRCTimeout);
         }

         /* Yield the processor to other tasks. */
         od_sleep(0);

         /* Call od_kernel(). */
         CALL_KERNEL_IF_NEEDED();
      }
   }

#endif /* !OD_MULTITHREADED */

   /* Copy next input event from the queue into the caller's structure. */
   memcpy(pEvent, &pInputQueueInfo->paEvents[pInputQueueInfo->nOutIndex],
      sizeof(tODInputEvent));

   /* Move out pointer to the next queue item, wrapping back to the start */
   /* of the queue if needed.                                             */
   pInputQueueInfo->nOutIndex
      = (pInputQueueInfo->nOutIndex + 1) % pInputQueueInfo->nQueueEntries;

   /* Now, return with success. */
   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODInQueueEmpty()
 *
 * Removes all events from the input queue.
 *
 * Parameters: hInQueue - Handle to the input queue to be emptied.
 *
 *     Return: void
 */
void ODInQueueEmpty(tODInQueueHandle hInQueue)
{
   tODInputEvent InputEvent;

   ASSERT(hInQueue != NULL);

   /* Remove all items from the queue. */
   while(ODInQueueWaiting(hInQueue))
   {
      ODInQueueGetNextEvent(hInQueue, &InputEvent, OD_NO_TIMEOUT);
   }
}


/* ----------------------------------------------------------------------------
 * ODInQueueGetLastActivity()
 *
 * Returns the time of the last input activity. This is the latest of the time
 * that the queue was created, the time of the last call to
 * ODInQueueAddEvent() on this input queue, and the time of the last call to
 * ODInQueueResetLastActivity() on this input queue.
 *
 * Parameters: hInQueue - Handle to the input queue.
 *
 *     Return: void
 */
time_t ODInQueueGetLastActivity(tODInQueueHandle hInQueue)
{
   tInputQueueInfo *pInputQueueInfo = ODHANDLE2PTR(hInQueue, tInputQueueInfo);

   ASSERT(pInputQueueInfo != NULL);

   /* Returns the last activity time. */
   return(pInputQueueInfo->nLastActivityTime);
}


/* ----------------------------------------------------------------------------
 * ODInQueueResetLastActivity()
 *
 * Resets the time of the last input activity to the current time.
 *
 * Parameters: hInQueue - Handle to the input queue.
 *
 *     Return: void
 */
void ODInQueueResetLastActivity(tODInQueueHandle hInQueue)
{
   tInputQueueInfo *pInputQueueInfo = ODHANDLE2PTR(hInQueue, tInputQueueInfo);

   ASSERT(pInputQueueInfo != NULL);

   /* Resets the last activity time to the current time. */
   pInputQueueInfo->nLastActivityTime = time(NULL);
}
