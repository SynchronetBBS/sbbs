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
 *        File: ODList.c
 *
 * Description: Implements the od_list_files() function for displaying
 *              a FILES.BBS file.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Oct 21, 1994  6.00  BP   Further isolated com routines.
 *              Dec 09, 1994  6.00  BP   Use new directory access functions.
 *              Dec 31, 1994  6.00  BP   Remove #ifndef USEINLINE DOS code.
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 11, 1995  6.00  BP   Moved functions from odcore.c
 *              Nov 11, 1995  6.00  BP   Removed register keyword.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 12, 1995  6.00  BP   Added entry, exit and kernel macros.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODCom.h"
#include "ODPlat.h"
#include "ODKrnl.h"
#include "ODUtil.h"


/* Filename component identifies. */
#define WILDCARDS 0x01
#define EXTENSION 0x02
#define FILENAME  0x04
#define DIRECTORY 0x08
#define DRIVE     0x10


/* Local private helper function prototypes. */
static void ODListFilenameMerge(char *pszEntirePath, const char *pszDrive,
   const char *pszDir, const char *pszName, const char *pszExtension);
static char *ODListGetFirstWord(char *pszInStr, char *pszOutStr);
static char *ODListGetRemainingWords(char *pszInStr);
static INT ODListFilenameSplit(const char *pszEntirePath, char *pszDrive,
   char *pszDir, char *pszName, char *pszExtension);


/* ----------------------------------------------------------------------------
 * od_list_files()
 *
 * Displays a list of files available for download, using an extended version
 * of the standard FILES.BBS format index file.
 *
 * Parameters: pszFileSpec - Directory name where the FILES.BBS file can be
 *                           found, or full path and filename of a FILES.BBS
 *                           format index file.
 *
 *     Return: TRUE on success or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_list_files(char *pszFileSpec)
{
   BYTE btLineCount = 2;
   BOOL bPausing;
   static char szLine[513];
   static char szFilename[80];
   static char szDrive[3];
   static char szDir[70];
   static char szTemp1[9];
   static char szTemp2[5];
   static char szBaseName[9];
   static char szExtension[5];
   static char szDirectory[100];
   INT nFilenameInfo;
   FILE *pfFilesBBS;
   static char *pszCurrent;
   BOOL bIsDir;
   BOOL bUseNextLine = TRUE;
   tODDirHandle hDir;
   tODDirEntry DirEntry;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_list_files()");

   /* Initialize OpenDoors if it hasn't already been done. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Check user's page pausing setting. */
   bPausing = od_control.od_page_pausing;

   if(od_control.od_extended_info) bPausing = od_control.user_attribute & 0x04;

   /* Parse directory parameter. */
   if(pszFileSpec == NULL)
   {
      strcpy(szODWorkString, ".");
      strcpy(szDirectory, "."DIRSEP_STR);
   }
   else if(*pszFileSpec == '\0')
   {
      strcpy(szODWorkString, ".");
      strcpy(szDirectory, "."DIRSEP_STR);
   }
   else
   {
      strcpy(szODWorkString, pszFileSpec);
      strcpy(szDirectory, pszFileSpec);
      if(szODWorkString[strlen(szODWorkString) - 1] == DIRSEP)
      {
         szODWorkString[strlen(szODWorkString) - 1] = '\0';
      }
   }

   /* Get directory information on path. */
   if(ODDirOpen(szODWorkString, DIR_ATTRIB_ARCH | DIR_ATTRIB_RDONLY
      | DIR_ATTRIB_DIREC, &hDir) != kODRCSuccess)
   {
      od_control.od_error = ERR_FILEOPEN;
      OD_API_EXIT();
      return(FALSE);
   }

   if(ODDirRead(hDir, &DirEntry) != kODRCSuccess)
   {
      ODDirClose(hDir);
      od_control.od_error = ERR_FILEOPEN;
      OD_API_EXIT();
      return(FALSE);
   }

   ODDirClose(hDir);

   /* If it is a directory. */
   if(DirEntry.wAttributes & DIR_ATTRIB_DIREC)
   {
      /* Append FILES.BBS to directory name & open. */
      bIsDir = TRUE;
      ODMakeFilename(szODWorkString, szODWorkString, "FILES.BBS",
         sizeof(szODWorkString));
      if((pfFilesBBS = fopen(szODWorkString, "r")) == NULL)
      {
         od_control.od_error = ERR_FILEOPEN;
         OD_API_EXIT();
         return(FALSE);
      }
   }

   /* If it is not a directory. */
   else
   {
      bIsDir = FALSE;
      if((pfFilesBBS = fopen(szODWorkString,"r")) == NULL)
      {
         od_control.od_error = ERR_FILEOPEN;
         OD_API_EXIT();
         return(FALSE);
      }
   }


   /* Ignore previously pressed control keys. */
   chLastControlKey = 0;


   /* Loop until the end of the FILES.BBS file has been reached. */
   for(;;)
   {
      if(fgets(szLine, 512, pfFilesBBS) == NULL) break;

      if(!bUseNextLine)
      {
         if(szLine[strlen(szLine) - 1] == '\n')
         {
            bUseNextLine = TRUE;
         }
         continue;
      }

      if(szLine[strlen(szLine) - 1] == '\n')
      {
         szLine[strlen(szLine) - 1] = '\0';
      }
      else
      {
         bUseNextLine = FALSE;
      }
      if(szLine[strlen(szLine) - 1] == '\r')
      {
         szLine[strlen(szLine) - 1] = '\0';
      }

      if(chLastControlKey != 0)
      {
         switch(chLastControlKey)
         {
            case 's':
               if(od_control.od_list_stop)
               {
                  if(od_control.baud)
                  {
                     ODComClearOutbound(hSerialPort);
                  }
                  od_clear_keybuffer();
                  fclose(pfFilesBBS);
                  OD_API_EXIT();
                  return(TRUE);
               }
               break;

            case 'p':
               if(od_control.od_list_pause)
               {
                  od_clear_keybuffer();
                  od_get_key(TRUE);
               }
         }
         chLastControlKey = 0;
      }

      /* Determine whether or not this is a comment line. */
      if(szLine[0] == ' ' || strlen(szLine) == 0)

      {
         /* If so, display the line in comment color. */
         od_set_attrib(od_control.od_list_title_col);
         od_disp_str(szLine);
         od_disp_str("\n\r");
         ++btLineCount;
      }

      /* If the line is not a comment. */
      else                             
      {
         /* Extract the first word of the line, */
         ODListGetFirstWord(szLine, szFilename);

         /* And extract the filename. */
         nFilenameInfo = ODListFilenameSplit(szFilename, szDrive, szDir,
            szBaseName, szExtension);
         if(!((nFilenameInfo & DRIVE) || (nFilenameInfo & DIRECTORY)))
         {
            if(bIsDir)
            {
               ODMakeFilename(szDirectory, szDirectory, szFilename,
                  sizeof(szDirectory));
               strcpy(szFilename, szDirectory);
            }
            else
            {
               ODListFilenameSplit(szDirectory, szDrive, szDir, szTemp1,
                  szTemp2);
               ODListFilenameMerge(szFilename, szDrive, szDir, szBaseName,
                  szExtension);
            }
         }

         /* Search for the filespec in directory. */
         if(ODDirOpen(szFilename, DIR_ATTRIB_ARCH | DIR_ATTRIB_RDONLY, &hDir)
            == kODRCSuccess)
         {
            /* Display information on every file that matches. */
            while(ODDirRead(hDir, &DirEntry) == kODRCSuccess)
            {
               od_set_attrib(od_control.od_list_name_col);
               od_printf("%-12.12s  ", DirEntry.szFileName);
               od_set_attrib(od_control.od_list_size_col);
               od_printf("%-6ld   ", DirEntry.dwFileSize);
               od_set_attrib(od_control.od_list_comment_col);
               pszCurrent = ODListGetRemainingWords(szLine);
               if(strlen(pszCurrent) <= 56)
               {
                  od_disp_str(pszCurrent);
                  od_disp_str("\n\r");
               }
               else
               {
                  od_printf("%-56.56s\n\r", pszCurrent);
               }
               ++btLineCount;
            }

            ODDirClose(hDir);
         }

         /* Otherwise, indicate that the file is "Offline". */
         else
         {
            ODListFilenameMerge(szFilename, "", "", szBaseName, szExtension);
            od_set_attrib(od_control.od_list_name_col);
            od_printf("%-12.12s ", szFilename);
            od_set_attrib(od_control.od_list_offline_col);
            od_disp_str(od_control.od_offline);
            od_set_attrib(od_control.od_list_comment_col);

            od_printf("%-56.56s\n\r", ODListGetRemainingWords(szLine));
            ++btLineCount;
         }
      }

      /* Check for end of screen & page pausing. */
      if(btLineCount >= od_control.user_screen_length && bPausing)
      {
         /* Provide page pausing at end of each screen. */
         if(ODPagePrompt(&bPausing))
         {
            fclose(pfFilesBBS);
            OD_API_EXIT();
            return(TRUE);
         }

         /* Reset the line number counter. */
         btLineCount = 2;
      }
   }

   /* When finished, close the file. */
   fclose(pfFilesBBS);

   /*  Return with success. */
   OD_API_EXIT();
   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODListFilenameMerge()                               *** PRIVATE FUNCTION ***
 *
 * Builds a fully-qualified path name from the provided path component
 * strings.
 *
 * Parameters: pszEntirePath - Pointer to the destination string where the
 *                             generated path should be stored.
 *
 *             pszDrive      - Pointer to the drive string.
 *
 *             pszDir        - Pointer to the directory string.
 *
 *             pszName       - Pointer to the base filename string.
 *
 *             pszExtension  - Pointer to the extension name string.
 *
 *     Return: void
 */
static void ODListFilenameMerge(char *pszEntirePath, const char *pszDrive,
   const char *pszDir, const char *pszName, const char *pszExtension)
{
   if(pszEntirePath == NULL) return;

   pszEntirePath[0] = '\0';

   if(pszDrive != NULL)
   {
      strcpy(pszEntirePath, pszDrive);
   }
   if(pszDir != NULL)
   {
      strcat(pszEntirePath, pszDir);
   }
   if(pszName != NULL)
   {
      strcat(pszEntirePath,pszName);
   }
   if(pszExtension != NULL)
   {
      strcat(pszEntirePath,pszExtension);
   }
}


/* ----------------------------------------------------------------------------
 * ODListFilenameSplit()                               *** PRIVATE FUNCTION ***
 *
 * Splits the provided path string into drive, directory name, file base name
 * and file extension components.
 *
 * Parameters: pszEntirePath - A string containing the path to split.
 *
 *             pszDrive      - A string where the drive letter should be stored.
 *
 *             pszDir        - A string where the directory name should be
 *                             stored.
 *
 *             pszName       - A string where the base filename should be
 *                             stored.
 *
 *             pszExtension  - A string where the filename extension should be
 *                             stored.
 *
 *     Return: One or more flags indicating which components where found in the
 *             provided path name.
 */
static INT ODListFilenameSplit(const char *pszEntirePath, char *pszDrive,
   char *pszDir, char *pszName, char *pszExtension)
{
   char *pchCurrentPos;
   char *pchStart;
   BYTE btSize;
   INT nToReturn;

   ASSERT(pszEntirePath != NULL);
   ASSERT(pszDrive != NULL);
   ASSERT(pszDir != NULL);
   ASSERT(pszName != NULL);
   ASSERT(pszExtension != NULL);

   pchStart = (char *)pszEntirePath;
   nToReturn = 0;

   if((pchCurrentPos = strrchr(pchStart,':')) == NULL)
   {
      pszDrive[0] = '\0';
   }
   else
   {
      btSize = (int)(pchCurrentPos - pchStart) + 1;
      if(btSize > 2) btSize = 2;
      strncpy(pszDrive, pchStart, btSize);
      pszDrive[btSize] = '\0';
      pchStart = pchCurrentPos + 1;
      nToReturn |= DRIVE;
   }

   if((pchCurrentPos = strrchr(pchStart, DIRSEP))==NULL)
   {
      pszDir[0] = '\0';
   }
   else
   {
      btSize = (int)(pchCurrentPos - pchStart) + 1;
      strncpy(pszDir,pchStart,btSize);
      pszDir[btSize] = '\0';
      pchStart = pchCurrentPos + 1;
      nToReturn |= DIRECTORY;
   }

   if(strchr(pchStart,'*') != NULL || strchr(pchStart, '?') != NULL)
   {
      nToReturn |= WILDCARDS;
   }

   if((pchCurrentPos = strrchr(pchStart, '.')) == NULL)
   {
      if(pchStart =='\0')
      {
         pszExtension[0] = '\0';
         pszName[0] = '\0';
      }
      else
      {
         pszExtension[0] = '\0';
         btSize = strlen(pchStart);
         if (btSize > 8) btSize = 0;
         strncpy(pszName, pchStart, btSize);
         pszName[btSize] = '\0';
         nToReturn |= FILENAME;
      }
   }
   else
   {
      nToReturn |= FILENAME;
      nToReturn |= EXTENSION;

      btSize = (int)(pchCurrentPos - pchStart);

      if(btSize > 8) btSize = 8;

      strncpy(pszName, pchStart, btSize);
      pszName[btSize] = '\0';

      btSize = strlen(pchCurrentPos);
      if(btSize > 4) btSize = 4;
      strncpy(pszExtension, pchCurrentPos, btSize);
      pszExtension[btSize]='\0';
   }

   return(nToReturn);
}


/* ----------------------------------------------------------------------------
 * ODListGetFirstWord()                                *** PRIVATE FUNCTION ***
 *
 * Returns the first word in a string containing a series of words separated by
 * one or more spaced.
 *
 * Parameters: pszInStr  - String to look in.
 *
 *             pszOutStr - Buffer to store result in. This buffer should be at
 *                         least as long as the pszInStr string.
 *
 *     Return: Pointer to the pszOutStr that was passed in.
 */
static char *ODListGetFirstWord(char *pszInStr, char *pszOutStr)
{
   char *pchOut = (char *)pszOutStr;

   ASSERT(pszInStr != NULL);
   ASSERT(pszOutStr != NULL);

   while(*pszInStr && *pszInStr != ' ')
   {
      *pchOut++ = *pszInStr++;
   }
   *pchOut = '\0';

   return(pszOutStr);
}


/* ----------------------------------------------------------------------------
 * ODListGetRemainingWords()                           *** PRIVATE FUNCTION ***
 *
 * Obtains the remaining words in a string, after the first word. This function
 * is a companion to ODListGetFirstWord(), which obtains just the first word
 * in a string of many words.
 *
 * Parameters: pszInStr  - String to look at.
 *
 *     Return: A pointer to the position in a string of the second word.
 */
static char *ODListGetRemainingWords(char *pszInStr)
{
   char *pchStartOfRemaining = (char *)pszInStr;

   /* Skip over the first word in the string. */
   while(*pchStartOfRemaining && *pchStartOfRemaining != ' ')
   {
      ++pchStartOfRemaining;
   }

   /* Skip over any spaces after the first word. */
   while(*pchStartOfRemaining && *pchStartOfRemaining == ' ')
   {
      ++pchStartOfRemaining;
   }

   /* Return pointer to the rest of the string. */
   return((char *)pchStartOfRemaining);
 }
