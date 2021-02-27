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
 *        File: ODLog.c
 *
 * Description: Implements the logfile subsystem.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Oct 13, 1994  6.00  BP   New file header format.
 *              Dec 09, 1994  6.00  BP   Standardized coding style.
 *              Aug 19, 1995  6.00  BP   32-bit portability.
 *              Nov 16, 1995  6.00  BP   Removed oddoor.h, added odcore.h.
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <stdio.h>
#include <time.h>

#include "OpenDoor.h"
#include "ODCore.h"
#include "ODGen.h"
#include "ODInEx.h"
#include "ODKrnl.h"


/* Private logfile file handle */
static FILE *logfile_pointer;


/* Private helper functions. */
static BOOL ODLogWriteStandardMsg(INT nLogfileMessage);
static void ODLogClose(INT nReason);


/* ----------------------------------------------------------------------------
 * ODLogEnable()
 *
 * This function is called from od_init() when the user explicitly includes the
 * OpenDoors logfile option using the od_control.od_logfile setting.
 *
 * Parameters: None.
 *
 *     Return: void
 */
ODAPIDEF void ODCALL ODLogEnable(void)
{
   /* At this time, this function simply maps to a call to od_log_open(). */
   od_log_open();
}


/* ----------------------------------------------------------------------------
 * od_log_open()
 *
 * Called to begin logfile operations. This is when the first message is
 * written to the logfile, indicating that the user is entering OpenDoors.
 *
 * Parameters: None.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_log_open()
{
   time_t nUnixTime;
   struct tm *ptmTimeRecord;

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_log_open()");

   /* Initialize OpenDoors if not already done. */
   if(!bODInitialized) od_init();

   /* Don't open logfile if it has been disabled in config file, etc. */
   if(od_control.od_logfile_disable) return(TRUE);

   /* Open actual logfile. */
   if((logfile_pointer=fopen(od_control.od_logfile_name, "a")) == NULL)
   {
      return(FALSE);
   }

   /* Get the current time. */
   nUnixTime = time(NULL);
   ptmTimeRecord = localtime(&nUnixTime);

   /* Print logfile tear line. */
   fprintf(logfile_pointer, "\n----------  %s %02d %s %02d, %s\n",
      od_control.od_day[ptmTimeRecord->tm_wday],
      ptmTimeRecord->tm_mday,
      od_control.od_month[ptmTimeRecord->tm_mon],
      ptmTimeRecord->tm_year,
      od_program_name);

   /* Print message of door start up. */
   sprintf(szODWorkString, (char *)od_control.od_logfile_messages[11],
      od_control.user_name);
   od_log_write(szODWorkString);

   /* Set internal function hooks to enable calling of logfile features */
   /* from elsewhere in OpenDoors. */
   pfLogWrite = ODLogWriteStandardMsg;
   pfLogClose = ODLogClose;

   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODLogWriteStandardMsg()                             *** PRIVATE FUNCTION ***
 *
 * Function called to write a standard message to the logfile.
 *
 * Parameters: nLogfileMessage   - Index of the standard message to write to
 *                                 the logfile.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
static BOOL ODLogWriteStandardMsg(INT nLogfileMessage)
{
   if(nLogfileMessage < 0 || nLogfileMessage > 11)
   {
      return(FALSE);
   }

   od_log_write((char *)od_control.od_logfile_messages[nLogfileMessage]);

   if(nLogfileMessage == 8)
   {
      sprintf(szODWorkString, od_control.od_logfile_messages[12],
         od_control.user_reasonforchat);
      szODWorkString[67] = '\0';
      od_log_write(szODWorkString);
   }

   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * od_log_write()
 *
 * Called to write a message to the logfile.
 *
 * Parameters: pszMessage  - Pointer to a string containing the message text.
 *
 *     Return: TRUE on success, or FALSE on failure.
 */
ODAPIDEF BOOL ODCALL od_log_write(char *pszMessage)
{
   char *pszFormat;
   time_t nUnixTime;
   struct tm *ptmTimeRecord;

   /* Verify that OpenDoors has been initialized. */
   if(!bODInitialized) od_init();

   OD_API_ENTRY();

   /* Stop if logfile has been disabled in config file, etc. */
   if(od_control.od_logfile_disable)
   {
      OD_API_EXIT();
      return(TRUE);
   }

   /* If logfile has not yet been opened, then open it. */
   if(logfile_pointer==NULL)
   {
      if(!od_log_open())
      {
         OD_API_EXIT();
         return(FALSE);
      }
   }

   /* Get the current system time. */
   nUnixTime=time(NULL);
   ptmTimeRecord=localtime(&nUnixTime);

   /* Determine which logfile format string to use. */
   if(ptmTimeRecord->tm_hour<10)
   {
      pszFormat=(char *)">  %1.1d:%02d:%02d  %s\n";
   }
   else
   {
      pszFormat=(char *)"> %2.2d:%02d:%02d  %s\n";
   }

   /* Write a line to the logfile. */
   fprintf(logfile_pointer, pszFormat, ptmTimeRecord->tm_hour,
      ptmTimeRecord->tm_min, ptmTimeRecord->tm_sec, pszMessage);

   OD_API_EXIT();
   return(TRUE);
}


/* ----------------------------------------------------------------------------
 * ODLogClose()                                        *** PRIVATE FUNCTION ***
 *
 * Writes final entry to the logfile when OpenDoors is exiting.
 *
 * Parameters: nReason  - Specifies the reason why OpenDoors is exiting.
 *
 *     Return: void
 */
static void ODLogClose(INT nReason)
{
   /* Stop if logfile has been disabled in the config file, etc. */
   if(od_control.od_logfile_disable) return;

   /* If logfile has not been opened, then abort. */
   if(logfile_pointer==NULL) return;

   if(bPreOrExit)
   {
      od_log_write((char *)od_control.od_logfile_messages[13]);
   }
   else if(btExitReason<=5 && btExitReason>=1)
   {
      od_log_write((char *)od_control.od_logfile_messages[btExitReason-1]);
   }
   else
   {
      sprintf(szODWorkString,(char *)od_control.od_logfile_messages[5],nReason);
      od_log_write(szODWorkString);
   }

   /* Close the logfile. */
   fclose(logfile_pointer);

   /* Prevent further use of logfile without first re-opening it. */
   pfLogWrite = NULL;
   pfLogClose = NULL;
   logfile_pointer = NULL;
}
