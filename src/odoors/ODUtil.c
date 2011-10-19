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
 *        File: ODUtil.c
 *
 * Description: Implements the non-platform specific utility functions that
 *              are defined in odutil.h. Platform specific utility functions
 *              are implemented in odplat.c.
 *
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Nov 01, 1994  6.00  BP   Created.
 *              Dec 31, 1994  6.00  BP   Added ODMakeFilename().
 *              Nov 13, 1995  6.00  BP   32-bit portability.
 *              Nov 23, 1995  6.00  BP   Added ODDWordDivide().
 *              Nov 23, 1995  6.00  BP   Added ODDStringHasTail().
 *              Nov 23, 1995  6.00  BP   Added ODFileSize().
 *              Nov 24, 1995  6.00  BP   ODMakeFilename(): handle empty path.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 06, 1996  6.10  BP   Added ODDWordMultiply().
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <stdio.h>

#include "OpenDoor.h"
#include "ODStr.h"
#include "ODUtil.h"
#include "ODGen.h"


/* ========================================================================= */
/* General string manipulation functions.                                    */
/* ========================================================================= */

/* ----------------------------------------------------------------------------
 * ODStringCopy()
 *
 * Safely copies one string to another. Unlike strncpy(), ODStringCopy()
 * ensures that the destination string is always '\0' terminated.
 *
 * Parameters: pszDest     - Pointer to destination string to which to copy
 *                           characters.
 *
 *             pszSource   - Pointer to source string from which to copy
 *                           characters.
 *
 *             nSizeOfDest - Maximum number of characters to place in pszDest,
 *                           INCLUDING the '\0' string terminator.
 *
 *     Return: void
 */
void ODStringCopy(char *pszDest, CONST char *pszSource, INT nSizeofDest)
{
   ASSERT(pszDest != NULL);
   ASSERT(pszSource != NULL);
   ASSERT(nSizeofDest > 0);

   /* Copy at most the specified number of bytes from source to dest, using */
   /* (presumably well optimized) strncpy().                                */
   strncpy(pszDest, pszSource, nSizeofDest);

   /* Ensure that destination string is '\0' terminated. This will not */
   /* already be the case if strlen(pszSource) >= nSizeofDest.         */
   pszDest[nSizeofDest - 1] = '\0';
}


/* ----------------------------------------------------------------------------
 * ODStringCToPascal()
 *
 * Converts a string from C's zero-terminated string format to Pascal's
 * length byte + string data format.
 *
 * Parameters: psPascalString    - Pointer to the destination string.
 *
 *             btMaxPascalLength - Size of the destination string, as declared
 *                                 in Pascal.
 *
 *             pszCString        - Pointer to the source string, in C format.
 *
 *     Return: A pointer to psPascalString.
 */
char *ODStringCToPascal(char *psPascalString, BYTE btMaxPascalLength,
   char *pszCString)
{
   BYTE btCStringLength = strlen(pszCString);

   ASSERT(psPascalString != NULL);
   ASSERT(btMaxPascalLength > 0);
   ASSERT(pszCString != NULL);

   memcpy((char *)psPascalString + 1,
      pszCString, *psPascalString = (btCStringLength < btMaxPascalLength)
      ? btCStringLength : btMaxPascalLength);
   return(psPascalString);
}


/* ----------------------------------------------------------------------------
 * ODStringPascalToC()
 *
 * Converts a string from Pascal's length byte + string data format to C's
 * zero-terminated string format.
 *
 * Parameters: pszCString     - Pointer to destination string.
 *
 *             psPascalString - Pointer to Pascal format source string.
 *
 *             btMaxLength    - Length of C string.
 *
 *     Return: A pointer to pszCString.
 */
char *ODStringPascalToC(char *pszCString, char *psPascalString,
   BYTE btMaxLength)
{
   ASSERT(pszCString != NULL);
   ASSERT(psPascalString != NULL);
   ASSERT(btMaxLength > 0);

   if(*(BYTE *)psPascalString <= btMaxLength)
   {
      memcpy(pszCString, (char *)psPascalString + 1, *psPascalString);
      pszCString[(int)psPascalString[0]] = '\0';
   }
   else
   {
      pszCString[0] = '\0';
   }
   return(pszCString);
}


/* ----------------------------------------------------------------------------
 * ODStringHasTail()
 *
 * Determines whether a string ends in exactly the specified sequence of
 * characters.
 *
 * Parameters: pszFullString  - String to examine.
 *
 *             pszTail        - String to look for at the end of
 *                              pszFullString.
 *
 *     Return: TRUE if the pszFullString does end with pszTail, FALSE if
 *             it does not.
 */
BOOL ODStringHasTail(char *pszFullString, char *pszTail)
{
   INT nTailLength = strlen(pszTail);
   INT nFullStringLength = strlen(pszFullString);

   ASSERT(pszFullString != NULL);
   ASSERT(pszTail != NULL);

   if(nFullStringLength < nTailLength)
   {
      return(FALSE);
   }

   return(stricmp(pszFullString + (nFullStringLength - nTailLength), pszTail) == 0);
}


/* ========================================================================= */
/* File-related functions.                                                   */
/* ========================================================================= */

/* ----------------------------------------------------------------------------
 * ODMakeFilename()
 *
 * Generates a fully-qualified filename from a path and base filename.
 *
 * Parameters: pszOut      - String to store generated filename in.
 *
 *             pszPath     - Directory name. May be the same as pszOut, or
 *                           may be different.
 *
 *             pszFilename - Base filename.
 *
 *             nMaxOutSize - Size of pszOut. This value should be one more
 *                           than the maximum number of characters to be
 *                           stored in the output string.
 *
 *     Return: kODRCSuccess on success, or an error code on failure.
 */
tODResult ODMakeFilename(char *pszOut, CONST char *pszPath,
   CONST char *pszFilename, INT nMaxOutSize)
{
   /* Validate parameters in debug mode */
   ASSERT(pszPath != NULL);
   ASSERT(pszFilename != NULL);
   ASSERT(pszOut != NULL);
   ASSERT(pszFilename != pszOut);
   ASSERT(nMaxOutSize > 0);

   /* Check that there is enough room in the destination string to hold */
   /* both source strings plus possibly an additional \-seperator.      */
   if((INT)(strlen(pszPath) + strlen(pszFilename) + 1) > nMaxOutSize - 1)
   {
      return(kODRCFilenameTooLong);
   }

   /* Copy path to output filename, if the addresses are different. */
   if(pszPath != pszOut)
   {
      strcpy(pszOut, pszPath);
   }

   /* Ensure there is a trailing backslash, if path was not empty. */
#ifdef ODPLAT_NIX

#else
   if(pszOut[strlen(pszOut) - 1] != DIRSEP && strlen(pszOut) > 0)
   {
      strcat(pszOut, DIRSEP_STR);
   }
#endif

   /* Append base filename. */
   strcat(pszOut, pszFilename);

   return(kODRCSuccess);
}


/* ----------------------------------------------------------------------------
 * ODFileSize()
 *
 * Determines the size of a currently open file.
 *
 * Parameters: pfFile - Pointer to an already open file to examine.
 *
 *     Return: The size of the file. In the case of a file that is open in
 *             binary mode, this will be the file length in bytes.
 */
DWORD ODFileSize(FILE *pfFile)
{
   DWORD dwOriginal;
   DWORD dwFileSize;

   ASSERT(pfFile != NULL);

   dwOriginal = ftell(pfFile);
   fseek(pfFile, 0L, SEEK_END);
   dwFileSize = ftell(pfFile);
   fseek(pfFile, dwOriginal, SEEK_SET);
   return(dwFileSize);
}


/* ========================================================================= */
/* DWORD math functions.                                                     */
/* ========================================================================= */

/* ----------------------------------------------------------------------------
 * ODDWordShiftLeft()
 *
 * Shifts a DWORD to the left by the specified number of bits.
 *
 * Parameters: dwValue    - Value to be shifted.
 *
 *             btDistance - Distance to shift dwValue by.
 *
 *     Return: Result of the shift operation.
 */
DWORD ODDWordShiftLeft(DWORD dwValue, BYTE btDistance)
{
   WORD wUpper;
   WORD wLower;

   wLower = (WORD)dwValue;
   wUpper = *(WORD *)(((BYTE *)(&dwValue)) + 2);

   while(btDistance--)
   {
      wUpper <<= 1;
      wUpper |= (wLower & 0x8000) >> 15;
      wLower <<= 1;
   }

   dwValue = wLower;
   *(WORD *)(((BYTE *)(&dwValue)) + 2) = wUpper;

   return(dwValue);
}


/* ----------------------------------------------------------------------------
 * ODDWordShiftRight()
 *
 * Shifts a DWORD to the right by the specified number of bits.
 *
 * Parameters: dwValue    - Value to be shifted.
 *
 *             btDistance - Distance to shift dwValue by.
 *
 *     Return: Result of the shift operation.
 */
DWORD ODDWordShiftRight(DWORD dwValue, BYTE btDistance)
{
   WORD wUpper;
   WORD wLower;

   wLower = (WORD)dwValue;
   wUpper = *(WORD *)(((BYTE *)(&dwValue)) + 2);

   while(btDistance--)
   {
      wLower >>= 1;
      wLower |= (wUpper & 0x0001) << 15;
      wUpper >>= 1;
   }

   dwValue=wLower;
   *(WORD *)(((BYTE *)(&dwValue)) + 2) = wUpper;
   return(dwValue);
}


/* ----------------------------------------------------------------------------
 * ODDWordDivide()
 *
 * Divides one DWORD by another DWORD, calculating the quotient and remainder.
 *
 * Parameters: pdwQuotient   - Location where the quotient should be stored,
 *                             or NULL if quotient is not required.
 *
 *             pdwRemainder  - Location where remainder should be stored,
 *                             or NULL if remainder is not required.
 *
 *             dwDividend    - Dividend to be divided by divisor.
 *
 *             dwDivisor     - Divisor to divide dividend by.
 *
 *     Return: TRUE on success or FALSE on failure.
 */
BOOL ODDWordDivide(DWORD *pdwQuotient, DWORD *pdwRemainder,
   DWORD dwDividend, DWORD dwDivisor)
{
   INT nTimes = 0;
   DWORD dwQuotient;
   DWORD dwRemainder;

   /* Check for divide by zero in debug versions. */
   ASSERT(dwDivisor != 0);

   /* Check that divisor is not zero. (An attempt to divide by zero will */
   /* put this algorithm into an infinite loop, rather than triggering   */
   /* a divide fault.)                                                   */
   if(dwDivisor == 0L)
   {
      return(FALSE);
   }

   /* Initialize remainder to be entire dividend */
   dwRemainder = dwDividend;

   /* Initialize quotient to 0 */
   dwQuotient = 0L;

   /* Determine largest required multiple of divisor */
   while(dwRemainder >=  dwDivisor)
   {
      dwDivisor = ODDWordShiftLeft(dwDivisor, 1);
      ++nTimes;
   }

   /* Loop across for all multiples of divisor, beginning with the largest */
   do
   {
      dwQuotient = ODDWordShiftLeft(dwQuotient, 1);

      /* If current remainder is >= this multiple of the divisor */
      if(dwRemainder >= dwDivisor)
      {
         /* Subtract the multiple of the divisor from the remainder */
         dwRemainder -= dwDivisor;

         /* The next bit of the quotient should be a 1 */
         dwQuotient |= 1L;
      }

      /* Divide current multiple of divisor by two */
      dwDivisor = ODDWordShiftRight(dwDivisor, 1);

      /* Repeat for all multiples of the divisor */
   } while(nTimes--);

   /* If caller asked for quotient, then return it */
   if(pdwQuotient != NULL)
   {
      *pdwQuotient = dwQuotient;
   }

   /* If caller asked for remainder, then return it */
   if(pdwRemainder != NULL)
   {
      *pdwRemainder = dwRemainder;
   }

   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODDWordDivide()
 *
 * Multiplies one DWORD by another, returning the product. Multiplication
 * is performed by using at most 32 additions.
 *
 * Parameters: dwMultiplicand - The multiplicand.
 *
 *             dwMultiplier   - The multiplier.
 *
 *     Return: Result of the multiplication.
 */
DWORD ODDWordMultiply(DWORD dwMultiplicand, DWORD dwMultiplier)
{
   DWORD dwResult = 0;

   /* Loop while multiplier is not zero */
   while(dwMultiplier != 0)
   {
      /* If least significant bit of multiplier is set */
      if(dwMultiplier & 0x00000001)
      {
         /* Add multiplicand to product */
         dwResult += dwMultiplicand;
      }

      /* Shift multiplicand left one bit */
      dwMultiplicand = ODDWordShiftLeft(dwMultiplicand, 1);

      /* Shift multiplier right one bit */
      dwMultiplier = ODDWordShiftRight(dwMultiplier, 1);
   }

   /* Return the final result to the caller. */
   return(dwResult);
}
