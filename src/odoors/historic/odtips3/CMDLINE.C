#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "opendoor.h"
#include "cmdline.h"


#ifndef BOOL
typedef int BOOL;
#endif

typedef enum
{
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
   kParamUnknown
} tCommandLineParameter;


static void AdvanceToNextArg(int *pnCurrentArg, int nArgCount,
			     char *pszOption);
static void GetNextArgName(int *pnCurrentArg, int nArgCount,
			   char *papszArguments[], char *pszString,
			   int nStringSize);
static tCommandLineParameter GetCommandLineParameter(char *pszArgument);


void ParseStandardCommandLine(int nArgCount, char *papszArguments[])
{
   char *pszCurrentArg;
   int nCurrentArg;

   for(nCurrentArg = 1; nCurrentArg < nArgCount; ++nCurrentArg)
   {
      pszCurrentArg = papszArguments[nCurrentArg];

      switch(GetCommandLineParameter(pszCurrentArg))
      {
	 case kParamLocal:
	    od_control.od_force_local = TRUE;
	    break;

	 case kParamBPS:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    od_control.baud = atol(papszArguments[nCurrentArg]);
	    break;

	 case kParamPort:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    od_control.port = atoi(papszArguments[nCurrentArg]);
	    break;

	 case kParamHelp:
	    printf("AVALIABLE COMMAND LINE PARAMETERS:\n");
            printf(" -L or -LOCAL     - Causes door to operate in local mode, without requiring a\n");
            printf("                    door information (drop) file.\n");
            printf(" -DROPFILE x      - Door information file directory or directory+filename.\n");
            printf(" -N x or -NODE x  - Sets the node number to use.\n");
            printf(" -B x or -BPS x   - Sets the serial port <---> modem bps (baud) rate to use.\n");
            printf(" -P x or -PORT x  - Sets the serial port to use, were 0=COM1, 1=COM2, etc.\n");
            printf(" -ADDRESS x       - Sets serial port address in decimal NOT hexidecimal\n");
            printf("                    (only has effect if FOSSIL driver is not being used).\n");
            printf(" -IRQ x           - Sets the serial port IRQ line (only has effect if FOSSIL\n");
            printf("                    driver is not being used).\n");
            printf(" -NOFOSSIL        - Disables use of FOSSIL driver, even if available.\n");
            printf(" -NOFIFO          - Disables use of 16550 FIFO buffers (only if FOSSIL driver\n");
            printf("                    is not being used).\n");
            printf(" -PERSONALITY x   - Sets the sysop status line / function key personality to\n");
            printf("                    use - one of Standard, PCBoard, RemoteAccess or Wildcat.\n");
            printf(" -MAXTIME x       - Sets the maximum number of minutes that any user will be\n");
            printf("                    permitted to access the door.\n");
            printf(" -USERNAME x      - Name of user who is currently online.\n");
            printf(" -TIMELEFT x      - User's time remaining online.\n");
            printf(" -SECURITY x      - User's security level.\n");
            printf(" -LOCATION x      - Location from which user is calling.\n");
            printf(" -?, -H or -HELP  - Displays command-line help and exits.\n");
	    exit(1);
	    break;

	 case kParamNode:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    od_control.od_node = atoi(papszArguments[nCurrentArg]);
	    break;

	 case kParamPersonality:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    if(stricmp(papszArguments[nCurrentArg], "Standard") == 0)
	    {
	       od_control.od_default_personality = PER_OPENDOORS;
	    }
	    else if(stricmp(papszArguments[nCurrentArg], "PCBoard") == 0)
	    {
	       od_control.od_default_personality = PER_PCBOARD;
	    }
	    else if(stricmp(papszArguments[nCurrentArg], "RemoteAccess") == 0)
	    {
	       od_control.od_default_personality = PER_RA;
	    }
	    else if(stricmp(papszArguments[nCurrentArg], "Wildcat") == 0)
	    {
	       od_control.od_default_personality = PER_WILDCAT;
	    }
	    else
	    {
	       printf("Unknown personality: %s\n", papszArguments[nCurrentArg]);
	       exit(1);
	    }
	    break;

	 case kParamMaxTime:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    od_control.od_maxtime = atoi(papszArguments[nCurrentArg]);
	    break;

	 case kParamAddress:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    od_control.od_com_address = atoi(papszArguments[nCurrentArg]);
	    break;

	 case kParamIRQ:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    od_control.od_com_irq = atoi(papszArguments[nCurrentArg]);
	    break;

	 case kParamNoFOSSIL:
	    od_control.od_no_fossil = TRUE;
	    break;

	 case kParamNoFIFO:
	    od_control.od_com_no_fifo = TRUE;
	    break;

	 case kParamDropFile:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    strncpy(od_control.info_path, papszArguments[nCurrentArg],
	       sizeof(od_control.info_path) - 1);
	    od_control.info_path[sizeof(od_control.info_path) - 1] = '\0';
	    break;

	 case kParamUserName:
	    GetNextArgName(&nCurrentArg, nArgCount, papszArguments,
	       od_control.user_name, sizeof(od_control.user_name));
	    break;

	 case kParamTimeLeft:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    od_control.user_timelimit = atoi(papszArguments[nCurrentArg]);
	    break;

	 case kParamSecurity:
	    AdvanceToNextArg(&nCurrentArg, nArgCount, pszCurrentArg);
	    od_control.user_security = atoi(papszArguments[nCurrentArg]);
	    break;

	 case kParamLocation:
	    GetNextArgName(&nCurrentArg, nArgCount, papszArguments,
	       od_control.user_location, sizeof(od_control.user_location));
	    break;

	 default:
	    printf("Unrecognized command line option: %s\n", pszCurrentArg);
	    exit(1);
	    break;
      }
   }
}


static void AdvanceToNextArg(int *pnCurrentArg, int nArgCount, char *pszOption)
{
   if(++*pnCurrentArg >= nArgCount)
   {
      printf("Missing parameter for option: %s\n", pszOption);
      exit(1);
   }
}


static void GetNextArgName(int *pnCurrentArg, int nArgCount,
			   char *papszArguments[], char *pszString,
			   int nStringSize)
{
   BOOL bFirst = TRUE;

   if((*pnCurrentArg) + 1 >= nArgCount)
   {
      printf("Missing parameter for option: %s\n",
         papszArguments[(*pnCurrentArg) - 1]);
      exit(1);
   }

   pszString[0] = '\0';

   while(++*pnCurrentArg < nArgCount)
   {
      if(GetCommandLineParameter(papszArguments[*pnCurrentArg])
	 != kParamUnknown)
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


static tCommandLineParameter GetCommandLineParameter(char *pszArgument)
{
   if(*pszArgument == '-' || *pszArgument == '/')
   {
      ++pszArgument;
   }

   if(stricmp(pszArgument, "L") == 0
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
   else if(stricmp(pszArgument, "DROPFILE") == 0)
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
   else
   {
      return(kParamUnknown);
   }
}


void NoDoorFileHandler(void)
{
   /* Alter OpenDoors behaviour, so that we proceed with defaults if  */
   /* no door information file is available, rather than exiting with */
   /* an error. Set od_no_file_func to point to this function.        */
   if(strlen(od_control.user_name) == 0)
   {
      strcpy(od_control.user_name, "Unknown User");
   }
   if(strlen(od_control.user_location) == 0)
   {
      strcpy(od_control.user_location, "Unknown Location");
   }
   if(od_control.user_timelimit == 0)
   {
      od_control.user_timelimit = 30;
   }

   od_control.od_info_type = CUSTOM;
}
