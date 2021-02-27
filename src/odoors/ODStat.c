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
 *        File: ODStat.c
 *
 * Description: Helper functions used by various built-in personalities.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Jul 18, 1995  6.00  BP   Fix ODStatGetUserAge() bug.
 *              Nov 13, 1995  6.00  BP   32-bit portability.
 *              Nov 13, 1995  6.00  BP   Created odstat.h.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Jan 12, 1996  6.00  BP   Added ODStatStartArrowUse(), etc.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Mar 13, 1996  6.10  BP   bOnlyShiftArrow -> nArrowUseCount.
 *              Mar 19, 1996  6.10  BP   MSVC15 source-level compatibility.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODStat.h"
#include "ODKrnl.h"


/* Global working string available to all personalities for status line */
/* generation.                                                          */
char szStatusText[80];


/* ----------------------------------------------------------------------------
 * ODStatAddKey()
 *
 * Adds another hot key to the array of custom local keys.
 *
 * Parameters: wKeyCode - IBM scan-code/ASCII-code style key identification
 *                         code to add.
 *
 *     Return: void
 */
void ODStatAddKey(WORD wKeyCode)
{
   if(od_control.od_num_keys < 16)
      od_control.od_hot_key[od_control.od_num_keys++] = wKeyCode;
}


/* ----------------------------------------------------------------------------
 * ODStatRemoveKey()
 *
 * Removes a custom sysop hotkey that was previously added with ODStatAddKey().
 *
 * Parameters: wKeyCode - The scan code / ASCII code key identification code
 *                        to remove.
 *
 *     Return: void
 */
void ODStatRemoveKey(WORD wKeyCode)
{
   INT nCount;

   for(nCount = 0; nCount < od_control.od_num_keys; ++nCount)
      if((WORD)od_control.od_hot_key[nCount] == wKeyCode)
      {
         if(nCount != od_control.od_num_keys - 1)
         {
            od_control.od_hot_key[nCount] =
               od_control.od_hot_key[od_control.od_num_keys-1];
         }
         --od_control.od_num_keys;
         return;
      }
}


/* ----------------------------------------------------------------------------
 * ODStatGetUserAge()
 *
 * Generates a string containing the age, in years, of the current
 * user, based on the current date and the user's birthday.
 *
 * Parameters: pszAge - Pointer to a string where user's age should be stored.
 *
 *     Return: void
 */
void ODStatGetUserAge(char *pszAge)
{
   INT nAge;
   INT n;
   time_t Time;
   struct tm *TimeBlock;

   if(od_control.od_info_type==RA2EXITINFO || od_control.od_info_type==DOORSYS_WILDCAT)
   {
      nAge = atoi(od_control.user_birthday) - 1;

      if(strlen(od_control.user_birthday) == 8 && nAge <= 11)
      {
         if(od_control.user_birthday[6] >= '0'
            && od_control.user_birthday[6] <= '9'
            && od_control.user_birthday[7] >= '0'
            && od_control.user_birthday[7] <= '9')
         {
            if(od_control.user_birthday[3] >= '0'
               && od_control.user_birthday[3] <= '3'
               && od_control.user_birthday[4] >= '0'
               && od_control.user_birthday[4] <= '9')
            {
               Time = time(NULL);
               TimeBlock = localtime(&Time);

               n = (TimeBlock->tm_year % 100)
                  - atoi((char *)od_control.user_birthday + 6);

               if(n < 0) nAge = n + 100; else nAge = n;

               n = atoi(od_control.user_birthday) - 1;
               if(TimeBlock->tm_mon < n)
                  --nAge;
               else if(TimeBlock->tm_mon == n)
               {
                  n=atoi((char *)od_control.user_birthday + 3);

                  if(TimeBlock->tm_mday < n) --nAge;
               }

               sprintf(pszAge, "%d", nAge);
               return;
              }
           }
      }
   }

   strcpy(pszAge, "?");
}


/* ----------------------------------------------------------------------------
 * ODStatStartArrowUse()
 *
 * Called by OpenDoors when it needs to use the arrow keys, and so they
 * shouldn't be used by the status line.
 *
 * Parameters: None
 *
 *     Return: void
 */
void ODStatStartArrowUse(void)
{
   ++nArrowUseCount;
}


/* ----------------------------------------------------------------------------
 * ODStatEndArrowUse()
 *
 * Called by OpenDoors when it no longer needs to use the arrow keys, and so
 * they can again be used by the status line.
 *
 * Parameters: None
 *
 *     Return: void
 */
void ODStatEndArrowUse(void)
{
   ASSERT(nArrowUseCount > 0);
   --nArrowUseCount;
}
