/* ex_diag.c - Diagnostic door program, written to test environment in which
 *             an OpenDooors door will run. Reads configuration settings from
 *             command line and configuration file, and displays diagnostic
 *             information on the local (and when possible, remote) screens.
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "OpenDoor.h"

/******************/
/* Wrapper macros */
/******************/
#if defined(__unix__)
	#if !defined(stricmp)
		#define stricmp(x,y)            strcasecmp(x,y)
		#define strnicmp(x,y,z)         strncasecmp(x,y,z)
        #endif
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

char *BoolAsStr(int bValue);
void ParseStandardCommandLine(int nArgCount, char *papszArguments[]);
static void AdvanceToNextArg(int *pnCurrentArg, int nArgCount,
			     char *pszOption);
static void GetNextArgName(int *pnCurrentArg, int nArgCount,
			   char *papszArguments[], char *pszString,
			   int nStringSize);
static tCommandLineParameter GetCommandLineParameter(char *pszArgument);


int main(int argc, char *argv[])
{
   char sz[80];
   int n;

   /* Parse command-line. */
   ParseStandardCommandLine(argc, argv);

   /* Initialize OpenDoors. */
   od_init();

   od_clr_scr();

   od_printf("OpenDoors has been initialized.\n\r");
   for(;;)
   {
      od_printf("\n\rPOST-INITIALIZATION DIAGNOSTIC INFORMATION:\n\r");
      od_printf("    Running in REMOTE mode : %s\n\r",
         BoolAsStr(od_control.baud));
      od_printf("   Port <-> Modem BPS Rate : %lu\n\r", od_control.baud);
      od_printf("        Serial Port Number : %d (COM%d:)\n\r", od_control.port,
         od_control.port + 1);
      od_printf("         Serial I/O Method : ");
      switch(od_control.od_com_method)
      {
         case COM_FOSSIL:
            od_printf("FOSSIL Driver\n\r");
            break;
         case COM_INTERNAL:
            od_printf("OpenDoors Internal I/O Module\n\r");
            break;
         case COM_SOCKET:
            od_printf("TCP Socket/Telnet\n\r");
            break;
         default:
            od_printf("Unknown\n\r");
            break;
      }
      od_printf("            Drop File Type : ");
      switch(od_control.od_info_type)
      {
         case DORINFO1:
            od_printf("DORINFO?.DEF\n\r");
            break;
         case EXITINFO:
            od_printf("Basic EXITINFO.BBS & DORINFO1.DEF\n\r");
            break;
         case RA1EXITINFO:
            od_printf("RA 1.x EXITINFO.BBS & DORINFO1.DEF\n\r");
            break;
         case CHAINTXT:
            od_printf("CHAIN.TXT\n\r");
            break;
         case SFDOORSDAT:
            od_printf("SFDOORS.DAT\n\r");
            break;
         case CALLINFO:
            od_printf("CALLINFO.BBS\n\r");
            break;
         case DOORSYS_GAP:
            od_printf("GAP style DOOR.SYS\n\r");
            break;
         case DOORSYS_DRWY:
            od_printf("DoorWay DOOR.SYS\n\r");
            break;
         case QBBS275EXITINFO:
            od_printf("QuickBBS 2.75+ EXITINFO.BBS\n\r");
            break;
         case CUSTOM:
            od_printf("User-Defined Custom Format\n\r");
            break;
         case DOORSYS_WILDCAT:
            od_printf("WildCat! DOOR.SYS\n\r");
            break;
         case RA2EXITINFO:
            od_printf("RA 2.x+ EXITINFO.BBS & DORINFO1.DEF\n\r");
            break;
         case NO_DOOR_FILE:
            od_printf("No Drop File in Use\n\r");
            break;
         case DOOR32SYS:
            od_printf("Door32.sys\n\r");
            break;
         default:
            od_printf("Unknown Type\n\r");
            break;
      }
      od_printf("       ANSI Mode Available : %s\n\r",
         BoolAsStr(od_control.user_ansi));
      od_printf("     AVATAR Mode Available : %s\n\r",
         BoolAsStr(od_control.user_avatar));
      od_printf("    RIP Graphics Available : %s\n\r",
         BoolAsStr(od_control.user_rip));
      od_printf("         User's Time Limit : %d\n\r", od_control.user_timelimit);
      od_printf("          User's Full Name : %s\n\r", od_control.user_name);

      od_printf("\n\rChoose Option: [E]xit, [T]yping Test,");
      if(od_control.od_com_method == COM_INTERNAL)
      {
         od_printf(" [I]nternal I/O Diags,");
      }
      od_printf("\n\r");
      od_printf("               [A]utodetect ANSI/RIP, [R]e-Display, [D]isplay Tests\n\r");
		n=od_get_answer("eitard");
      switch(n)
      {
         case 'e':
            od_clr_scr();
            od_printf("\n\rExit - Are You Sure (Y/N)? ");
            if(od_get_answer("yn") == 'y')
            {
               return(0);
            }
            break;

         case 'i':
            od_clr_scr();
            od_printf("INTERNAL SERIAL I/O DIAGNOSTIC INFORMATION:\n\r");
            od_printf("  Serial Port Base Address : %x\n\r",
               od_control.od_com_address);
            od_printf("           IRQ Line Number : %d\n\r",
               od_control.od_com_irq);
            od_printf("       Receive Buffer Size : %d\n\r",
               od_control.od_com_rx_buf);
            od_printf("      Transmit Buffer Size : %d\n\r",
               od_control.od_com_tx_buf);
            od_printf(" Use FIFO Buffer, if avail : %s\n\r",
               BoolAsStr(!od_control.od_com_no_fifo));
            od_printf("         FIFO Trigger Size : %d\n\r",
               od_control.od_com_fifo_trigger);

            od_printf("\n\rPress [ENTER] to return.\n\r");
            od_get_answer("\n\r");
            break;

         case 't':
            od_clr_scr();
            od_printf("\n\rTyping Test - Type any text below:\n\r");
            od_printf("[------------------------------------------------------"
                      "-----------------------]\n\r");
            od_input_str(sz, 79, 0, 255);
            od_printf("\n\rEntered Text:\n\r%s\n\r", sz);
            od_printf("\n\rPress [ENTER] to return.\n\r");
            od_get_answer("\n\r");
            break;

         case 'a':
            od_clr_scr();
            od_printf("\n\rAutodetecting ANSI/RIP mode ...\n\r");
            od_printf("(Detected modes will be turned on.)\n\r");
            od_autodetect(0);
            od_printf("\n\rDone, press [ENTER] to return.\n\r");
            od_get_answer("\n\r");
            break;

         case 'd':
            od_clr_scr();
            od_printf("CLEAR SCREEN TEST\n\r");
            od_printf("About to test clear screen. The screen should\n\r");
            od_printf("be cleared before the next test if screen\n\r");
            od_printf("clearing is enabled.\n\r");
            od_printf("\n\rPress [ENTER] to perform test.\n\r");
            od_get_answer("\n\r");
            od_clr_scr();

            od_printf("CARRIAGE RETURN TEST:\n\r");
            od_printf("This should not be visible\r");
            od_printf("This should cover it up...\n\r\n\r");
            od_printf("The text \"This should not be visible\" will\n\r");
            od_printf("appear above if this test failed.\n\r");
            od_printf("\n\rPress [ENTER] to perform next test.\n\r");
            od_get_answer("\n\r");

            od_clr_scr();
            od_printf("COLOR TEST:\n\r\n\r");
            for(n = 0; n < 256; ++n)
            {
               od_set_attrib(n);
               od_printf("%x", n % 16);
               if(n % 32 == 31)
               {
                  od_set_attrib(0x07);
                  od_printf("\n\r");
               }
            }
            od_printf("\n\rIf ANSI or AVATAR modes are available, the\n\r");
            od_printf("above test pattern should print in color.\n\r");
            od_printf("\n\rPress [ENTER] to perform next test.\n\r");
            od_get_answer("\n\r");

            od_clr_scr();
            od_printf("CURSOR POSITIONING TEST:\n\r");
            for(n = 15; n > 2; --n)
            {
               od_set_cursor(n, n);
               od_printf("\\");
            }
            for(n = 15; n > 2; --n)
            {
               od_set_cursor(n, 17 - n);
               od_printf("/");
            }

            od_set_cursor(17, 1);
            od_printf("If ANSI or AVATAR modes are available, a large X\n\r");
            od_printf("should appear on lines 3 to 15.\n\r");
            od_printf("\n\rPress [ENTER] to return.\n\r");
            od_get_answer("\n\r");
            break;
      }

      od_clr_scr();
   }

   /* Return with success. */
   return(0);
}


char *BoolAsStr(int bValue)
{
   return(bValue ? "Yes (TRUE)" : "No (FALSE)");
}


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
   int bFirst = TRUE;

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
