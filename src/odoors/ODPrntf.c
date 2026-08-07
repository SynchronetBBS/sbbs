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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 *
 *        File: ODPrntf.c
 *
 * Description: Implements the od_printf() function.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   32-bit portability.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 03, 1996  6.00  BP   Use ODVCALL instead of ODCALL.
 *              Jan 04, 1996  6.00  BP   Add missing OD_API_EXIT() at end.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODFormat.h"
#include "ODSafe.h"
#include "ODGen.h"
#include "ODKrnl.h"


/* Size of od_printf() working buffer. Adjust this upwards if you are    */
/* encountering difficulties when calling od_printf() with long strings. */
#define WORK_BUFFER_SIZE   512


/* ----------------------------------------------------------------------------
 * od_printf()
 *
 * The OpenDoors equivalent of the C printf() function, this function performs
 * formatted string output to both the local and remote screens.
 *
 * Parameters: pszFormat - Format string, in the same format as the printf()
 *                         format string.
 *
 *             The semantics of any further parameters are dicated by the
 *             contents of the format string.
 *
 *     Return: void
 */
ODAPIDEF void ODVCALL od_printf(const char *pszFormat,...)
{
   va_list pArgumentList;
   static char *pszWorkBuffer = NULL;
   char *pchCurrent;
   char *pchStart;
   BOOL bNotFound;
   INT nCharCount;
   INT nRequired;
   INT nWritten;
   size_t nRequiredSize;
   static size_t nWorkBufferSize = 0;
   char *pszNewBuffer;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_printf()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Determine the exact size required. A non-NULL probe is used for old
    * bounded-formatting implementations even though its size is zero. */
   va_start(pArgumentList, pszFormat);
   nRequired = ODVsnprintf(szODWorkString, 0, pszFormat, pArgumentList);
   va_end(pArgumentList);

   if(nRequired < 0 || !ODSizeAdd((size_t)nRequired, 1, &nRequiredSize))
   {
      od_control.od_error = ERR_LIMIT;
      OD_API_EXIT();
      return;
   }

   if(nRequiredSize > nWorkBufferSize)
   {
      pszNewBuffer = (char *)realloc(pszWorkBuffer,
         MAX(nRequiredSize, (size_t)WORK_BUFFER_SIZE));
      if(pszNewBuffer == NULL)
      {
         od_control.od_error = ERR_MEMORY;
         OD_API_EXIT();
         return;
      }
      pszWorkBuffer = pszNewBuffer;
      nWorkBufferSize = MAX(nRequiredSize, (size_t)WORK_BUFFER_SIZE);
   }

   /* Copy the arguments after the format string. */
   va_start(pArgumentList, pszFormat);

   /* Perform a bounded string printf to the working buffer. */
   nWritten = ODVsnprintf(pszWorkBuffer, nWorkBufferSize, pszFormat,
      pArgumentList);

   va_end(pArgumentList);

   if(nWritten != nRequired)
   {
      od_control.od_error = ERR_GENERALFAILURE;
      OD_API_EXIT();
      return;
   }

   /* If no color characters are defined, then just display the entire */
   /* buffer in one shot.                                              */
   if(!od_control.od_color_char && !od_control.od_color_delimiter)
       goto quick_print;

   chColorCheck = od_control.od_color_delimiter;

   bNotFound = TRUE;
   pchCurrent = (char *)pszWorkBuffer;
   pchStart = (char *)pszWorkBuffer;
   nCharCount = 0;
   while(*pchCurrent)
   {
      if(*pchCurrent == od_control.od_color_delimiter)
      {
         bNotFound = FALSE;

         if(nCharCount != 0)
         {
            od_disp(pchStart, nCharCount, TRUE);
         }

         if(!*(++pchCurrent))
         {
            chColorCheck = 0;
            OD_API_EXIT();
            return;
         }
         od_set_attrib(od_color_config(pchCurrent));
         if(!*(pchCurrent = (char *)pchColorEndPos))
         {
            chColorCheck = 0;
            OD_API_EXIT();
            return;
         }

         if(!*(++pchCurrent))
         {
            OD_API_EXIT();
            return;
         }
         pchStart = (char *)pchCurrent;
         nCharCount = 0;
      }

      else if(*pchCurrent == od_control.od_color_char)
      {
         bNotFound = FALSE;

         if(nCharCount != 0)
         {
            od_disp(pchStart, nCharCount, TRUE);
         }

         if(!*(++pchCurrent))
         {
            OD_API_EXIT();
            return;
         }
         od_set_attrib(*pchCurrent);

         if(!*(++pchCurrent))
         {
            OD_API_EXIT();
            return;
         }
         pchStart = (char *)pchCurrent;
         nCharCount = 0;
      }
      else
      {
         ++nCharCount;
         ++pchCurrent;
      }
   }

   chColorCheck = 0;

   if(bNotFound)
   {
quick_print:
      /* Display the entire string in one shot. */
      od_disp_str(pszWorkBuffer);
   }
   else if(nCharCount != 0)
   {
      /* If there are remaining characters in the string, then display them. */
      od_disp(pchStart, nCharCount, TRUE);
   }

   OD_API_EXIT();
}
