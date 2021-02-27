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
 *        File: ODCom.h
 *
 * Description: Public definitions for serial communications module, which
 *              is implemented in odcom.c
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 21, 1994  6.00  BP   Created.
 *              Jan 01, 1995  6.00  BP   Added ODComWaitEvent().
 *              Dec 21, 1995  6.00  BP   Add ability to use already open port.
 *              Jan 09, 1996  6.00  BP   Supply actual in/out buffer size used.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 06, 1996  6.10  BP   Initial support for Door32 interface.
 *              Oct 19, 2001  6.20  RS   Added TCP/IP socket (telnet) support.
 */

#ifndef _INC_ODCOM
#define _INC_ODCOM

/* odtypes.h is needed for definitions of tODHandle, and BOOL. */
#include "ODTypes.h"

/* Serial I/O handle. */
typedef tODHandle tPortHandle;

/* Flow Control setting manifest constants. */
#define FLOW_NONE           0x00
#define FLOW_RTSCTS         0x02
#define FLOW_DEFAULT        0x80

/* Parity bit setting manifest constants. */
#define ODPARITY_NONE         0x00
#define ODPARITY_ODD          0x08
#define ODPARITY_EVEN         0x18

#define ODPARITY_MASK         0x18

/* Stop bit setting manifest constants. */
#define STOP_ONE            0x00
#define STOP_ONE_POINT_FIVE 0x04                 /* Only with DATABITS_FIVE. */
#define STOP_TWO            0x04         /* Only if not using DATABITS_FIVE. */

#define STOP_MASK           0x04

/* Character length manifest constants. */
#define DATABITS_FIVE       0x00
#define DATABITS_SIX        0x01
#define DATABITS_SEVEN      0x02
#define DATABITS_EIGHT      0x03

#define DATABITS_MASK       0x03

/* FIFO setting constants. */
#define FIFO_DISABLE        0x00
#define FIFO_ENABLE         0x01
#define FIFO_TRIGGER_1      0x00
#define FIFO_TRIGGER_4      0x40
#define FIFO_TRIGGER_8      0x80
#define FIFO_TRIGGER_14     0xc0

/* Misc. manifest constants. */
#define SPEED_UNSPECIFIED   0
#define SIZE_NON_ZERO       -1

/* Serial I/O method settings. */
typedef enum
{
   kComMethodUnspecified,
   kComMethodFOSSIL,
   kComMethodUART,
   kComMethodWin32,
   kComMethodDoor32,
	kComMethodSocket,
	kComMethodStdIO
} tComMethod;

/* Serial I/O event types. */
typedef enum
{
   kNoCarrier
} tComEvent;

/* Serial I/O function prototypes. */
tODResult ODComAlloc(tPortHandle *phPort);
tODResult ODComFree(tPortHandle hPort);
tODResult ODComSetIdleFunction(tPortHandle hPort,
   void (*pfCallback)(void));
tODResult ODComSetFlowControl(tPortHandle hPort, BYTE btFlowControlSetting);
tODResult ODComSetSpeed(tPortHandle hPort, long lSpeed);
tODResult ODComSetPort(tPortHandle hPort, BYTE btPort);
tODResult ODComSetIRQ(tPortHandle hPort, BYTE btIRQLevel);
tODResult ODComSetPortAddress(tPortHandle hPort, int nPortAddress);
tODResult ODComSetWordFormat(tPortHandle hPort, BYTE btWordFormat);
tODResult ODComSetRXBuf(tPortHandle hPort, int nReceiveBufferSize);
tODResult ODComSetTXBuf(tPortHandle hPort, int nTransmitBufferSize);
tODResult ODComSetFIFO(tPortHandle hPort, BYTE btFIFOSetting);
tODResult ODComSetPreferredMethod(tPortHandle hPort, tComMethod Method);
tODResult ODComGetMethod(tPortHandle hPort, tComMethod *pMethod);
tODResult ODComOpen(tPortHandle hPort);
tODResult ODComOpenFromExistingHandle(tPortHandle hPort,
   DWORD dwExistingHandle);
tODResult ODComClose(tPortHandle hPort);
tODResult ODComClearInbound(tPortHandle hPort);
tODResult ODComClearOutbound(tPortHandle hPort);
tODResult ODComInbound(tPortHandle hPort, int *pnInboundWaiting);
tODResult ODComOutbound(tPortHandle hPort, int *pnOutboundWaiting);
tODResult ODComCarrier(tPortHandle hPort, BOOL *pbIsCarrier);
tODResult ODComSetDTR(tPortHandle hPort, BOOL bHigh);
tODResult ODComSendByte(tPortHandle hPort, BYTE btToSend);
tODResult ODComGetByte(tPortHandle hPort, char *pbtNext, BOOL bWait);
tODResult ODComSendBuffer(tPortHandle hPort, BYTE *pbtBuffer, int nSize);
tODResult ODComGetBuffer(tPortHandle hPort, BYTE *pbtBuffer, int nSize,
   int *pnBytesRead);
tODResult ODComWaitEvent(tPortHandle hPort, tComEvent Event);

#endif /* !_INC_ODCOM */
