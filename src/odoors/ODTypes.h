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
 *        File: ODTypes.c
 *
 * Description: Defines special data types used by OpenDoors.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 15, 1994  6.00  BP   Created.
 *              Jan 30, 1996  6.00  BP   Added kODRCTimeout.
 *              Feb 08, 1996  6.00  BP   Added kODRCSafeFailure.
 *              Feb 08, 1996  6.00  BP   Added kODRCUnrecoverableFailure.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 */

#ifndef _INC_ODTYPES
#define _INC_ODTYPES

/* Generic handle data type. */
typedef void * tODHandle;
#define ODHANDLE2PTR(h, t) ((t *) h)
#define ODPTR2HANDLE(p, t) ((tODHandle) p)

/* OpenDoors function call result codes. */
typedef enum
{
   kODRCSuccess,
   kODRCGeneralFailure,
   kODRCNoMemory,
   kODRCNothingWaiting,
   kODRCNoMatch,
   kODRCEndOfFile,
   kODRCNoPortAddress,
   kODRCNoUART,
   kODRCInvalidCall,
   kODRCFileAccessError,
   kODRCFilenameTooLong,
   kODRCTimeout,
   kODRCSafeFailure,
   kODRCUnrecoverableFailure,
   kODRCUnsupported
} tODResult;

/* Callback function types. */
#ifdef _MSC_VER
typedef void ODCALL OD_COMPONENT_CALLBACK(void);
typedef void ODCALL OD_PERSONALITY_CALLBACK(BYTE btOperation);
#else /* !_MSC_VER */
typedef void OD_COMPONENT_CALLBACK(void);
typedef void OD_PERSONALITY_CALLBACK(BYTE btOperation);
#endif /* !_MSC_VER */

#endif /* !_INC_ODTYPES */
