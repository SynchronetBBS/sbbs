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
 *              Jan 04, 1996  6.00  BP   Moved event type defs to OpenDoor.h.
 *              Jan 04, 1996  6.00  BP   tODInQueueEvent -> tODInputEvent.
 *              Jan 30, 1996  6.00  BP   Add ODInQueueGetNextEvent() timeout.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 */

#ifndef _INC_ODINQUE
#define _INC_ODINQUE

#include <time.h>

#include "ODTypes.h"

/* OpenDoors input queue handle. */
typedef tODHandle tODInQueueHandle;

/* Input queue functions. */
tODResult ODInQueueAlloc(tODInQueueHandle *phInQueue,  INT nInitialQueueSize);
void ODInQueueFree(tODInQueueHandle hInQueue);
BOOL ODInQueueWaiting(tODInQueueHandle hInQueue);
tODResult ODInQueueAddEvent(tODInQueueHandle hInQueue,
   tODInputEvent *pEvent);
tODResult ODInQueueGetNextEvent(tODInQueueHandle hInQueue,
   tODInputEvent *pEvent, tODMilliSec Timeout);
void ODInQueueEmpty(tODInQueueHandle hInQueue);
time_t ODInQueueGetLastActivity(tODInQueueHandle hInQueue);
void ODInQueueResetLastActivity(tODInQueueHandle hInQueue);

#endif /* _INC_ODINQUE */
