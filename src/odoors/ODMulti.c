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
 *        File: ODMulti.c
 *
 * Description: Code for multiple personality system, which allows
 *              a status line / function key personality to be dynamically
 *              selected at run-time.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 14, 1995  6.00  BP   Added include of odscrn.h.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 23, 1996  6.00  BP   Disable MPS under Win32 version.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Sep 01, 1996  6.10  BP   Update output area on od_set_per...().
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <time.h>
#include <stdio.h>

#include "OpenDoor.h"
#include "ODStr.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODScrn.h"
#include "ODInEx.h"
#include "ODKrnl.h"


/* Maximum number of personalities that may be installed at once. */
#define MAX_PERSONALITIES  12


/* Information on installed personalities. */
typedef struct
{
   char szName[33];
   INT nStatusTopLine;
   INT nStatusBottomLine;
   OD_PERSONALITY_PROC *pfPersonalityFunction;
} tPersonalityInfo;

static tPersonalityInfo aPersonalityInfo[MAX_PERSONALITIES]=
{
   {"STANDARD", 1, 23, pdef_opendoors},
   {"REMOTEACCESS", 1, 23, pdef_ra},
   {"WILDCAT", 1, 23, pdef_wildcat},
   {"PCBOARD", 1, 23, pdef_pcboard}
};


/* Private variables. */
static INT nPersonalities = 5;
static INT nCurrentPersonality = 255;


/* ----------------------------------------------------------------------------
 * ODMPSEnable()
 *
 * This function is called from within od_init() when the user enables the
 * multiple personality system.
 *
 * Parameters: None.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL ODMPSEnable(void)
{
   pfSetPersonality = od_set_personality;
}


/* ----------------------------------------------------------------------------
 * od_set_personality()
 *
 * Sets the current personality to the one that is specified in pszName.
 *
 * Parameters: pszName - The name of the personality to switch to.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_set_personality(const char *pszName)
{
#ifdef OD_TEXTMODE
   BYTE btNewPersonality;
   char szNameToMatch[33];
   tPersonalityInfo *pNewPersonalityInfo;
#endif /* OD_TEXTMODE */

   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_set_personality()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();
   
#ifdef OD_TEXTMODE
   /* Check for valid parameters. */
   if(strlen(pszName) == 0)
   {
      od_control.od_error = ERR_PARAMETER;
      OD_API_EXIT();
      return(FALSE);
   }

   /* Build personality name to match. */
   strncpy(szNameToMatch, pszName, 32);
   szNameToMatch[32] = '\0';
   strupr(szNameToMatch);

   /* Loop through installed personalities, checking for a match. */
   for(btNewPersonality = 0; btNewPersonality < nPersonalities;
      ++btNewPersonality)
   {
      /* If the name of this personality matches the one we are looking for. */
      if(strcmp(szNameToMatch, aPersonalityInfo[btNewPersonality].szName) == 0)
      {
         if(btNewPersonality != nCurrentPersonality)
         {
            /* Remove current status line from the screen .*/
            od_set_statusline(8);

            /* Initialize the new personality. */
            if(nCurrentPersonality != 255)
               (*(OD_PERSONALITY_CALLBACK *)pfCurrentPersonality)(22);
            od_control.od_page_statusline = -1;
            pNewPersonalityInfo = 
               &aPersonalityInfo[nCurrentPersonality=btNewPersonality];
            bRAStatus = TRUE;
            (*(OD_PERSONALITY_CALLBACK *)pNewPersonalityInfo
               ->pfPersonalityFunction)(20);
            ODScrnSetBoundary(1, (BYTE)pNewPersonalityInfo->nStatusTopLine, 80,
               (BYTE)pNewPersonalityInfo->nStatusBottomLine);
            pfCurrentPersonality
               = pNewPersonalityInfo->pfPersonalityFunction;
            btCurrentStatusLine = 255;

            /* Update output area. */
            btOutputTop = (BYTE)pNewPersonalityInfo->nStatusTopLine;
            btOutputBottom = (BYTE)pNewPersonalityInfo->nStatusBottomLine;

            /* Draw the new statusline. */
            od_set_statusline(0);
         }

         OD_API_EXIT();
         return(TRUE);
      }
   }

   OD_API_EXIT();
   od_control.od_error = ERR_LIMIT;
   return(FALSE);

#else /* !OD_TEXTMODE */

   /* The multiple personality system is not supported under this platform. */
   od_control.od_error = ERR_UNSUPPORTED;

   /* Return with failure. */
   OD_API_EXIT();
   return(FALSE);

#endif /* !OD_TEXTMODE */
}



/* ----------------------------------------------------------------------------
 * od_add_personality()
 *
 * Installs a new personality into the set of available personalities.
 *
 * Parameters: pszName        - Pointer to string containing the name of
 *                              the new personality.
 *
 *             btOutputTop    - Index of the top line of the status bar.
 *
 *             btOutputBottom - Index of the bottom line of the status bar.
 *
 *             pfPerFunc      - Pointer to the callback function which
 *                              implements this personality.
 *
 *     Return: TRUE on success or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_add_personality(const char *pszName, BYTE btOutputTop,
   BYTE btOutputBottom, OD_PERSONALITY_PROC *pfPerFunc)
{
   /* Log function entry if running in trace mode */
   TRACE(TRACE_API, "od_add_personality()");

#ifdef OD_TEXTMODE

   /* Check that we haven't exceeded the limit on the total number of */
   /* installed personalities.                                        */
   if(nPersonalities == MAX_PERSONALITIES)
   {
      od_control.od_error = ERR_LIMIT;
      return(FALSE);
   }

   /* Store information on this new personality. */
   strncpy(aPersonalityInfo[nPersonalities].szName, pszName, 32);
   aPersonalityInfo[nPersonalities].szName[32] = '\0';
   strupr(aPersonalityInfo[nPersonalities].szName);
   aPersonalityInfo[nPersonalities].nStatusTopLine = btOutputTop;
   aPersonalityInfo[nPersonalities].nStatusBottomLine = btOutputBottom;
   aPersonalityInfo[nPersonalities].pfPersonalityFunction = pfPerFunc;

   /* Increment total number of personalities. */
   ++nPersonalities;

   /* Return with success. */
   return(TRUE);

#else /* !OD_TEXTMODE */

   /* The multiple personality system is not supported under this platform. */
   od_control.od_error = ERR_UNSUPPORTED;

   /* Return with failure. */
   return(FALSE);

#endif /* !OD_TEXTMODE */
}
