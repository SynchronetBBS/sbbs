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
 *        File: ODCmdLn.c
 *
 * Description: Implementation of od_parse_cmd_line() function, which
 *              parses standard command-line parameters for OpenDors programs.
 *
 *   Revisions: Date          Ver   Who  Change
 *              ---------------------------------------------------------------
 *              Jan 29, 1995  6.00  BP   Created.
 *              Aug 19, 1995  6.00  BP   Cleaned up indentations.
 *              Nov 12, 1995  6.00  BP   32-bit portability.
 *              Nov 12, 1995  6.00  BP   Added -CONFIG parameter.
 *              Dec 21, 1995  6.00  BP   Added -HANDLE parameter.
 *              Dec 24, 1995  6.00  BP   puts() -> printf().
 *              Dec 30, 1995  6.00  BP   Added ODCALL for calling convention.
 *              Jan 03, 1996  6.00  BP   Recognize -D for -DROPFILE.
 *              Jan 03, 1996  6.00  BP   Parameters must begin with - or /.
 *              Feb 06, 1996  6.00  BP   Added -SILENT for od_silent_mode.
 *              Feb 19, 1996  6.00  BP   Changed version number to 6.00.
 *              Feb 20, 1996  6.00  BP   Added bParsedCmdLine.
 *              Feb 21, 1996  6.00  BP   Make cmd line options overriding.
 *              Feb 25, 1996  6.00  BP   Fix -P COMx.
 *              Feb 27, 1996  6.00  BP   Add -P COMx to command line help.
 *              Mar 03, 1996  6.10  BP   Begin version 6.10.
 *              Apr 08, 1996  6.10  BP   Added command-line parsing callbacks.
 *					 Apr 24, 2002  6.22  RS   Added -SOCKET parameter.
 *              Aug 10, 2003  6.23  SH   *nix support
 */

#define BUILDING_OPENDOORS

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "OpenDoor.h"
#include "ODStr.h"
#include "ODPlat.h"
#include "ODCore.h"
#include "ODInEx.h"


/* Maximum number of command-line arguments. Any additional arguments will */
/* simply be ignored.                                                      */
#define MAX_ARGS 32

/* Size of temporary string that will hold any options following custom      */
/* command-line keywords that are passed to the client application's handler */
/* function.                                                                 */
#define CUSTOM_OPTION_SIZE 80

/* Command-line parameter identifiers. */
typedef enum
{
   kParamConfigFile,
   kParamLocal,
   kParamBPS,
   kParamPort,
   kParamNode,
   kParamHelp,
   kParamPersonality,
   kParamMaxTime,
   kParamAddress,
   kParamIRQ,
   kParamNoFOSSIL,
   kParamNoFIFO,
   kParamDropFile,
   kParamUserName,
   kParamTimeLeft,
   kParamSecurity,
   kParamLocation,
   kParamGraphics,
   kParamBBSName,
   kParamPortHandle,
   kParamSocketDescriptor,
   kParamSilentMode,
   kParamOption,
   kParamUnknown
} tCommandLineParameter;


/* Private function prototypes. */
static void ODAdvanceToNextArg(INT *pnCurrentArg, INT nArgCount,
   char *pszOption);
static void ODGetNextArgName(INT *pnCurrentArg, INT nArgCount,
   char *papszArguments[], char *pszString, size_t nStringSize);
static tCommandLineParameter ODGetCommandLineParameter(char *pszArgument);


/* Private variables. */
#define CONFIG_FILENAME_SIZE 80
static char szConfigFilename[CONFIG_FILENAME_SIZE];


/* ----------------------------------------------------------------------------
 * od_parse_cmd_line()
 *
 * Function to parse an OpenDoors program's command-line, interpreting
 * standard command-line parameters. This is one of the few OpenDoors APIs
 * that will not automatically initialize OpenDoors if it hasn't already
 * been done. This is because od_parse_cmd_line() performs setup that must
 * be done prior to OpenDoors initialization.
 *
 * Parameters: FOR NON-WIN32 VERSIONS:
 *
 *             nArgCount      - Number of command line arguments, as passed to
 *                              main() in argc.
 *
 *             papszArguments - Pointer to array of pointers to string
 *                              arguments, as passed to main() in argv. The
 *                              first element of this array (usually the
 *                              full path and filename of the executable)
 *                              is ignored.
 *
 *             FOR WIN32 VERSION:
 *
 *             pszCmdLine     - Pointer to the command line string, as passed
 *                              to WinMain().
 *
 *     Return: void
 */
#ifdef ODPLAT_WIN32
ODAPIDEF void ODCALL od_parse_cmd_line(LPSTR pszCmdLine)
#else /* !ODPLAT_WIN32 */
ODAPIDEF void ODCALL od_parse_cmd_line(INT nArgCount, char *papszArguments[])
#endif /* !ODPLAT_WIN32 */
{
   char *pszCurrentArg;
   INT nCurrentArg;
   INT n;
#ifdef ODPLAT_WIN32
   INT nArgCount;
   char *papszArguments[MAX_ARGS];
   char *pszCmdLineCopy;
   char *pchCurrent
#endif /* ODPLAT_WIN32 */

   /* Log function entry if running in trace mode. */
   TRACE(TRACE_API, "od_parse_cmd_line()");

#ifdef ODPLAT_WIN32
   /* Attempt to allocate space for a copy of the command line. */
   pszCmdLineCopy = malloc(strlen(pszCmdLine) + 1);
   if(pszCmdLineCopy == NULL)
   {
      od_control.od_error = ERR_MEMORY;
      return;
   }

   /* Copy the command line text into our working copy. */
   strcpy(pszCmdLineCopy, pszCmdLine);

   /* Loop, building papszArguments and nArgCount. */
   pchCurrent = pszCmdLineCopy;
   for(nArgCount = 0; nArgCount < MAX_ARGS && *pchCurrent != '\0'; ++nArgCount)
   {
      /* Store address of the next command line argument. */
      papszArguments[nArgCount] = pchCurrent;

      /* Skip forward to the next white space. */
      while(*pchCurrent != '\0' && !isspace(*pchCurrent))
      {
         ++pchCurrent;
      }

      /* Replace white space characters with '\0' string terminators, until */
      /* we reach the next command line argument, or the end of the string. */
      while(*pchCurrent != '\0' && isspace(*pchCurrent))
      {
         *pchCurrent = '\0';
         ++pchCurrent;
      }
   }
#endif /* ODPLAT_WIN32 */

#ifndef ODPLAT_WIN32
   /* Check validity of parameters. */
   if(papszArguments == NULL)
   {
      od_control.od_error = ERR_PARAMETER;
      return;
   }
#endif /* !ODPLAT_WIN32 */

   /* Record that od_parse_cmd_line() has been called. */
   bParsedCmdLine = TRUE;

   /* Initialize variables that are not initialized in od_init() if */
   /* od_parse_cmd_line is specified.                               */
   od_control.user_ansi = TRUE;
   od_control.user_timelimit = 60;

#ifdef ODPLAT_WIN32
   for(nCurrentArg = 0; nCurrentArg < nArgCount; ++nCurrentArg)
#else /* !ODPLAT_WIN32 */
   for(nCurrentArg = 1; nCurrentArg < nArgCount; ++nCurrentArg)
#endif /* !ODPLAT_WIN32 */
   {
      pszCurrentArg = papszArguments[nCurrentArg];

      switch(ODGetCommandLineParameter(pszCurrentArg))
      {
         case kParamConfigFile:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            strncpy(szConfigFilename, papszArguments[nCurrentArg],
               sizeof(szConfigFilename) - 1);
            szConfigFilename[sizeof(szConfigFilename) - 1] = '\0';
            od_control.od_config_filename = szConfigFilename;
            break;

         case kParamLocal:
            od_control.od_force_local = TRUE;
            break;

         case kParamBPS:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            od_control.baud = atol(papszArguments[nCurrentArg]);
            wPreSetInfo |= PRESET_BPS;
            break;

         case kParamPort:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            if(strnicmp(papszArguments[nCurrentArg], "COM", 3) == 0)
            {
               od_control.port = atoi(papszArguments[nCurrentArg] + 3) - 1;
            }
            else
            {
               od_control.port = atoi(papszArguments[nCurrentArg]);
            }
            wPreSetInfo |= PRESET_PORT;
            break;

         case kParamHelp:
            if(od_control.od_cmd_line_help_func != NULL)
            {
               (*od_control.od_cmd_line_help_func)();
               exit(0);
            }

#ifdef ODPLAT_WIN32
            sprintf(szODWorkString, "%s Command Line Options",
               strlen(od_control.od_prog_name) > 0 ? od_control.od_prog_name
               : OD_VER_SHORTNAME);
            if(od_control.od_cmd_line_help != NULL)
            {
               MessageBox(NULL, od_control.od_cmd_line_help, szODWorkString,
                  MB_ICONINFORMATION | MB_OK);
            }
            else
            {
               MessageBox(NULL,
                  "(Note that some options can be overriden by configuration or drop files.)\n"
                  "\n"
                  "-C x or -CONFIG x\t- Specfies configuration filename.\n"
                  "-L or -LOCAL\t- Causes door to operate in local mode, without requiring a drop file.\n"
                  "-D or -DROPFILE x\t- Door information file directory and/or filename.\n"
                  "-N x or -NODE x\t- Sets the node number to use.\n"
                  "-B x or -BPS x\t- Sets the serial port <---> modem bps (baud) rate to use.\n"
                  "-P x or -PORT x\t- Sets serial port to use. For COM1: use -P 0 or -P COM1, for COM2: use -P 1 or -P COM2, etc.\n"
                  "-HANDLE x\t- Provides an already open serial port handle.\n"
						"-SOCKET x\t- Provides an already open TCP/IP socket descriptor.\n"
                  "-SILENT\t\t- Operate in silent mode, with no local display.\n"
                  "-MAXTIME x\t- Sets the maximum number of minutes that user will be permitted to access the door.\n"
                  "-G or -GRAPHICS\t- Unless followed by 0 or N, turns on ANSI display mode.\n"
                  "-BBSNAME x\t- Name of BBS.\n"
                  "-USERNAME x\t- Name of user who is currently online.\n"
                  "-TIMELEFT x\t- User's time remaining online.\n"
                  "-SECURITY x\t- User's security level.\n"
                  "-LOCATION x\t- Location from which user is calling.\n"
                  "-?, -H or -HELP\t- Displays command-line help and exits.",
                  szODWorkString, MB_ICONINFORMATION | MB_OK);
            }
#else /* !ODPLAT_WIN32 */
            printf("AVALIABLE COMMAND LINE OPTIONS ");
            if(od_control.od_cmd_line_help != NULL)
            {
               printf(od_control.od_cmd_line_help);
            }
            else
            {
               printf("(Some can be overriden by config/drop file)\n");
               printf(" -C or -CONFIG    - Specfies configuration filename.\n");
               printf(" -L or -LOCAL     - Causes door to operate in local mode, without requiring a\n");
               printf("                    door information (drop) file.\n");
               printf(" -D or -DROPFILE  - Door information file directory and/or filename.\n");
               printf(" -N x or -NODE x  - Sets the node number to use.\n");
               printf(" -B x or -BPS x   - Sets the serial port <---> modem bps (baud) rate to use.\n");
               printf(" -P x or -PORT x  - Sets serial port to use. For COM1: use -P 0 or -P COM1, for\n");
               printf("                    COM2: use -P 1 or -P COM2, etc.\n");
               printf(" -ADDRESS x       - Sets serial port address in HEXIDECIMAL (if no FOSSIL).\n");
               printf(" -IRQ x           - Sets the serial port IRQ line (if FOSSIL is not used).\n");
               printf(" -NOFOSSIL        - Disables use of FOSSIL driver, even if available.\n");
               printf(" -NOFIFO          - Disables use of 16550 FIFO buffers (only if no FOSSIL).\n");
               printf(" -MAXTIME x       - Sets the maximum number of minutes that any user will be\n");
               printf("                    permitted to access the door, regardless of time left.\n");
               printf(" -SILENT          - Operate in silent mode, with no local display.\n");
               printf(" -G or -GRAPHICS  - Unless followed by 0 or N, turns on ANSI display mode.\n");
               printf(" -BBSNAME x       - Name of BBS.\n");
               printf(" -USERNAME x      - Name of user who is currently online.\n");
               printf(" -TIMELEFT x      - User's time remaining online.\n");
               printf(" -SECURITY x      - User's security level.\n");
               printf(" -LOCATION x      - Location from which user is calling.\n");
               printf(" -?, -H or -HELP  - Displays command-line help and exits.\n");
            }
#endif /* !ODPLAT_WIN32 */
            exit(1);
            break;

         case kParamNode:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            od_control.od_node = atoi(papszArguments[nCurrentArg]);
            break;

         case kParamMaxTime:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            od_control.od_maxtime = atoi(papszArguments[nCurrentArg]);
            break;

         case kParamAddress:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            od_control.od_com_address =
               (WORD)strtol(papszArguments[nCurrentArg], NULL, 16);
            break;

         case kParamIRQ:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            od_control.od_com_irq = atoi(papszArguments[nCurrentArg]);
            break;

         case kParamNoFOSSIL:
            od_control.od_no_fossil = TRUE;
            break;

         case kParamNoFIFO:
            od_control.od_com_no_fifo = TRUE;
            break;

         case kParamDropFile:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            strncpy(od_control.info_path, papszArguments[nCurrentArg],
               sizeof(od_control.info_path) - 1);
            od_control.info_path[sizeof(od_control.info_path) - 1] = '\0';
            break;

         case kParamUserName:
            ODGetNextArgName(&nCurrentArg, nArgCount, papszArguments,
            od_control.user_name, sizeof(od_control.user_name));
            break;

         case kParamTimeLeft:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            od_control.user_timelimit = atoi(papszArguments[nCurrentArg]);
            break;

         case kParamSecurity:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            od_control.user_security = atoi(papszArguments[nCurrentArg]);
            break;

         case kParamLocation:
            ODGetNextArgName(&nCurrentArg, nArgCount, papszArguments,
            od_control.user_location, sizeof(od_control.user_location));
            break;

         case kParamGraphics:
            n = nCurrentArg;
            if(++n < nArgCount)
            {
               if(atoi(papszArguments[n]) == 0 ||
                  stricmp(papszArguments[n], "N") == 0)
               {
                  od_control.user_ansi = FALSE;
                  ++nCurrentArg;
                  break;
               }
            }
            od_control.user_ansi = TRUE;
            break;

         case kParamBBSName:
            ODGetNextArgName(&nCurrentArg, nArgCount, papszArguments,
               od_control.system_name, sizeof(od_control.system_name));
            break;

         case kParamSocketDescriptor:
				od_control.od_use_socket = TRUE;
				/* fall through */

         case kParamPortHandle:
            ODAdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
            od_control.od_open_handle = atoi(papszArguments[nCurrentArg]);
            break;

         case kParamSilentMode:
            od_control.od_silent_mode = TRUE;
            break;

         case kParamUnknown:
            /* If the client application provided a custom command line   */
            /* handler function, then pass this unrecognized command-line */
            /* parameter and any options to that callback function.       */
            if(od_control.od_cmd_line_handler != NULL)
            {
               char szCustomOptions[CUSTOM_OPTION_SIZE];
               ODGetNextArgName(&nCurrentArg, nArgCount, papszArguments,
                  szCustomOptions, sizeof(szCustomOptions));
               (*od_control.od_cmd_line_handler)(pszCurrentArg,
                  szCustomOptions);
            }
            break;
      }
   }

#ifdef ODPLAT_WIN32
   free(pszCmdLineCopy);
#endif /* ODPLAT_WIN32 */
}


/* ----------------------------------------------------------------------------
 * ODAdvanceToNextArg()                                *** PRIVATE FUNCTION ***
 *
 * Moves to the argument for a particular command line option.
 *
 * Parameters: pnCurrentArg - Pointer to current argument number.
 *
 *             nArgCount    - Total number of arguments available.
 *
 *             pszOption    - Pointer to command line option name.
 *
 *     Return: void
 */
static void ODAdvanceToNextArg(INT *pnCurrentArg, INT nArgCount,
   char *pszOption)
{
   ASSERT(pnCurrentArg != NULL);
   ASSERT(pszOption != NULL);

   if(++*pnCurrentArg >= nArgCount)
   {
      printf("Missing parameter for option: %s\n", pszOption);
      exit(1);
   }
}


/* ----------------------------------------------------------------------------
 * ODGetNextArgName()                                  *** PRIVATE FUNCTION ***
 *
 * Obtains a multi-word name from command-line.
 *
 * Parameters: pnCurrentArg   - Pointer to integer storing current argument
 *                              number.
 *
 *             nArgCount      - The total number of command-line argument
 *
 *             papszArguments - Pointer to array of pointers to string
 *                              arguments, as passed to main() in argv.
 *
 *             pszString      - Pointer to string in which name will string
 *                              be stored.
 *
 *             nStringSize    - Size of the string.
 *
 *     Return: void
 */
static void ODGetNextArgName(INT *pnCurrentArg, INT nArgCount,
   char *papszArguments[], char *pszString, size_t nStringSize)
{
   BOOL bFirst = TRUE;

   ASSERT(pnCurrentArg != NULL);
   ASSERT(papszArguments != NULL);
   ASSERT(pszString != NULL);
   ASSERT(nStringSize > 0);

   if((*pnCurrentArg) + 1 >= nArgCount)
   {
      printf("Missing parameter for option: %s\n",
         papszArguments[(*pnCurrentArg) - 1]);
      exit(1);
   }

   pszString[0] = '\0';

   while(++*pnCurrentArg < nArgCount)
   {
      if(ODGetCommandLineParameter(papszArguments[*pnCurrentArg])
         != kParamOption)
      {
         --*pnCurrentArg;
         break;
      }

      if(strlen(pszString) >= nStringSize - 1)
      {
         break;
      }

      if(!bFirst)
      {
         strcat(pszString, " ");
      }

      strncat(pszString, papszArguments[*pnCurrentArg],
         strlen(pszString) - nStringSize - 1);
      pszString[nStringSize - 1] = '\0';

      bFirst = FALSE;
   }

}


/* ----------------------------------------------------------------------------
 * ODGetCommandLineParameter()                         *** PRIVATE FUNCTION ***
 *
 * Determines which command-line option, if any, is specified by an argument
 * string.
 *
 * Parameters: pszArgument - Pointer to string containing raw command-line
 *                           argument.
 *
 *     Return: A tCommandLineParameter, identifying which command-line option,
 *             if any, matches the argument string.
 */
static tCommandLineParameter ODGetCommandLineParameter(char *pszArgument)
{
   ASSERT(pszArgument != NULL);

   if(*pszArgument == '-' || *pszArgument == '/')
   {
      ++pszArgument;
   }
   else
   {
      return(kParamOption);
   }

   if(stricmp(pszArgument, "C") == 0
      || stricmp(pszArgument, "CONFIG") == 0
      || stricmp(pszArgument, "CONFIGFILE") == 0
      || stricmp(pszArgument, "CFGFILE") == 0
      || stricmp(pszArgument, "CFG") == 0)
   {
      return(kParamConfigFile);
   }
   else if(stricmp(pszArgument, "L") == 0
      || stricmp(pszArgument, "LOCAL") == 0)
   {
      return(kParamLocal);
   }
   else if(stricmp(pszArgument, "B") == 0
      || stricmp(pszArgument, "BPS") == 0
      || stricmp(pszArgument, "BAUD") == 0)
   {
      return(kParamBPS);
   }
   else if(stricmp(pszArgument, "P") == 0
      || stricmp(pszArgument, "PORT") == 0)
   {
      return(kParamPort);
   }
   else if(stricmp(pszArgument, "N") == 0
      || stricmp(pszArgument, "NODE") == 0)
   {
      return(kParamNode);
   }
   else if(stricmp(pszArgument, "?") == 0
      || stricmp(pszArgument, "H") == 0
      || stricmp(pszArgument, "HELP") == 0)
   {
      return(kParamHelp);
   }
   else if(stricmp(pszArgument, "PERSONALITY") == 0)
   {
      return(kParamPersonality);
   }
   else if(stricmp(pszArgument, "MAXTIME") == 0)
   {
      return(kParamMaxTime);
   }
   else if(stricmp(pszArgument, "ADDRESS") == 0)
   {
      return(kParamAddress);
   }
   else if(stricmp(pszArgument, "IRQ") == 0)
   {
      return(kParamIRQ);
   }
   else if(stricmp(pszArgument, "NOFOSSIL") == 0)
   {
      return(kParamNoFOSSIL);
   }
   else if(stricmp(pszArgument, "NOFIFO") == 0)
   {
      return(kParamNoFIFO);
   }
   else if(stricmp(pszArgument, "DROPFILE") == 0 ||
      stricmp(pszArgument, "D") == 0)
   {
      return(kParamDropFile);
   }
   else if(stricmp(pszArgument, "USERNAME") == 0)
   {
      return(kParamUserName);
   }
   else if(stricmp(pszArgument, "TIMELEFT") == 0)
   {
      return(kParamTimeLeft);
   }
   else if(stricmp(pszArgument, "SECURITY") == 0)
   {
      return(kParamSecurity);
   }
   else if(stricmp(pszArgument, "LOCATION") == 0)
   {
      return(kParamLocation);
   }
   else if(stricmp(pszArgument, "GRAPHICS") == 0
      || stricmp(pszArgument, "G") == 0)
   {
      return(kParamGraphics);
   }
   else if(stricmp(pszArgument, "BBSNAME") == 0)
   {
      return(kParamBBSName);
   }
   else if(stricmp(pszArgument, "HANDLE") == 0)
   {
      return(kParamPortHandle);
   }
   else if(stricmp(pszArgument, "SOCKET") == 0)
   {
      return(kParamSocketDescriptor);
   }
   else if(stricmp(pszArgument, "SILENT") == 0)
   {
      return(kParamSilentMode);
   }
   else
   {
      return(kParamUnknown);
   }
}
