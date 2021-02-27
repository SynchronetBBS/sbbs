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
 *        File: ODUtil.h
 *
 * Description: Contains prototypes and definitions for use by non-platform
 *              specific utility functions. These functions are implemented in
 *              odutil.c. Platform-specific utility functions are defined in
 *              odplat.h, and implemented in odplat.c.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Nov 01, 1994  6.00  BP   Created.
 *              Dec 31, 1994  6.00  BP   Added ODMakeFilename().
 *              Nov 13, 1995  6.00  BP   32-bit portability.
 *              Nov 23, 1995  6.00  BP   Added ODDWordDivide().
 *              Nov 23, 1995  6.00  BP   Added ODDStringHasTail().
 *              Nov 23, 1995  6.00  BP   Added ODFileSize().
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 06, 1996  6.10  BP   Added ODDWordMultiply().
 */

#ifndef _INC_ODUTIL
#define _INC_ODUTIL

#include "ODGen.h"
#include "ODTypes.h"
#include "OpenDoor.h"

/* General string manipulation functions. */
void ODStringCopy(char *pszDest, CONST char *pszSource, INT nSizeofDest);
char *ODStringCToPascal(char *psPascalString, BYTE btMaxPascalLength,
   char *pszCString);
char *ODStringPascalToC(char *pszCString, char *psPascalString,
   BYTE btMaxLength);
BOOL ODStringHasTail(char *pszFullString, char *pszTail);

/* File-related functions. */
tODResult ODMakeFilename(char *pszOut, CONST char *pszPath,
   CONST char *pszFilename, INT nMaxOutSize);
DWORD ODFileSize(FILE *pfFile);

/* DWORD math functions. */
DWORD ODDWordShiftLeft(DWORD dwValue, BYTE btDistance);
DWORD ODDWordShiftRight(DWORD dwValue, BYTE btDistance);
BOOL ODDWordDivide(DWORD *pdwQuotient, DWORD *pdwRemainder,
   DWORD dwDividend, DWORD dwDivisor);
DWORD ODDWordMultiply(DWORD dwMultiplicand, DWORD dwMultiplier);

#endif /* !_INC_ODUTIL */
