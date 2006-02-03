/* tip2.c - Example program to demonstrate how to send or receive files */
/*          using DSZ, from within an OpenDoors program.                */

/* Include required header files. */
#include <stdio.h>
#include <assert.h>
#include "opendoor.h"

/* File transfer protocol enumeration. */
typedef enum
{
   XModem,
   XModemCRC,
   XModem1K,
   YModem,
   YModemG,
   ZModem
} eProtocol;

/* Function prototypes. */
int TransferFile(char *pszFilename, eProtocol Protocol, char bReceive);
eProtocol ChooseProtocol(void);
void AddParameter(char **papszArguments, int *pnCount, char *pszNewArgument);

/* Manifest constants. */
#define ARGS_ARRAY_SIZE 10

/* Global variable with DSZ filename. */
char szDSZFilename[80] = "DSZ";


/* Program's execution begins here. */
main()
{
   char chAnswer;
   char bReceive;
   eProtocol Protocol;
   char szFilename[73];
   int bSuccess;

   od_printf("OpenDoors file transfer demo.\n\r\n\r");

   /* Get file transfer direction from user. */
   od_printf("Do you wish to [U]pload or [D]ownload? ");
   chAnswer = od_get_answer("UD");
   if(chAnswer == 'U')
   {
      od_printf("Upload\n\r\n\r");
      bReceive = TRUE;
   }
   else
   {
      od_printf("Download\n\r\n\r");
      bReceive = FALSE;
   }

   /* Get file transfer protocol from user. */
   Protocol = ChooseProtocol();

   /* Get filename. */
   od_printf("\n\rEnter filename(s) : ");
   od_input_str(szFilename, 72, ' ', 255);
   od_printf("\n\r");

   /* Perform file transfer. */
   bSuccess = TransferFile(szFilename, Protocol, bReceive);

   /* Display result of file transfer to user. */
   od_clr_scr();
   if(bSuccess)
   {
      od_printf("File transfer successful.\n\r");
   }
   else
   {
      od_printf("File transfer not completed.\n\r");
   }

   /* Prompt user to exit program. */
   od_printf("Press [Enter] to return to BBS.\n\r");
   od_get_answer("\n\r");

   /* Return control to calling BBS software. */
   od_exit(0, FALSE);

   return(0);
}


/* Function to allow user to choose a file transfer protocol. */
eProtocol ChooseProtocol(void)
{
   eProtocol Protocol;
   char chAnswer;

   od_printf("Available file transfer protocols:\n\r");
   od_printf("    [X] XModem\n\r");
   od_printf("    [C] XModem/CRC\n\r");
   od_printf("    [1] XModem/1K\n\r");
   od_printf("    [Y] YModem\n\r");
   od_printf("    [G] YModem-G\n\r");
   od_printf("    [Z] ZModem\n\r\n\r");
   od_printf("Please select a protocol: ");

   chAnswer = od_get_answer("XC1YGZ");

   switch(chAnswer)
   {
      case 'X':
         od_printf("XModem\n\r");
         Protocol = XModem;
         break;
      case 'C':
         od_printf("XModem/CRC\n\r");
         Protocol = XModemCRC;
         break;
      case '1':
         od_printf("XModem/1K\n\r");
         Protocol = XModem1K;
         break;
      case 'Y':
         od_printf("YModem\n\r");
         Protocol = YModem;
         break;
      case 'G':
         od_printf("YModem-G\n\r");
         Protocol = YModemG;
         break;
      case 'Z':
      default:
         od_printf("ZModem\n\r");
         Protocol = ZModem;
         break;
   }

   return(Protocol);
}


/* Function to send or receive a file to/from remote system. */
int TransferFile(char *pszFilename, eProtocol Protocol, char bReceive)
{
   char szPort[7];
   char *apszArguments[ARGS_ARRAY_SIZE];
   int nArgCount = 0;

   /* Filename cannot be NULL. */
   assert(pszFilename != NULL);

   /* Ensure that we are not operating in local mode. */
   if(od_control.baud == 0)
   {
      od_printf("Operating in local mode; file transfer not possible.\n\r");
      return(FALSE);
   }

   /* Generate DSZ command line */

   /* Begin with executable filename. */
   AddParameter(apszArguments, &nArgCount, szDSZFilename);

   /* Add port parameter. */
   AddParameter(apszArguments, &nArgCount, "port");
   sprintf(szPort, "%d", od_control.port + 1);
   AddParameter(apszArguments, &nArgCount, szPort);

   /* Restrict inbound files to current drive and directory. */
   AddParameter(apszArguments, &nArgCount, "restrict");

   /* Generate DSZ protocol parameters from specified protocol and direction. */
   if(bReceive)
   {
      switch(Protocol)
      {
         case XModem:
         case XModem1K:
            AddParameter(apszArguments, &nArgCount, "rx");
            break;
         case XModemCRC:
            AddParameter(apszArguments, &nArgCount, "rc");
            break;
         case YModem:
            AddParameter(apszArguments, &nArgCount, "rb");
            break;
         case YModemG:
            AddParameter(apszArguments, &nArgCount, "rb");
            AddParameter(apszArguments, &nArgCount, "-g");
            break;
         case ZModem:
            AddParameter(apszArguments, &nArgCount, "rz");
            break;
         default:
            assert(FALSE);
      }
   }
   else
   {
      switch(Protocol)
      {
         case XModem:
         case XModemCRC:
            AddParameter(apszArguments, &nArgCount, "sx");
            break;
         case XModem1K:
            AddParameter(apszArguments, &nArgCount, "sx");
            AddParameter(apszArguments, &nArgCount, "-k");
            break;
         case YModem:
         case YModemG:
            AddParameter(apszArguments, &nArgCount, "sb");
            break;
         case ZModem:
            AddParameter(apszArguments, &nArgCount, "sz");
            break;
         default:
            assert(FALSE);
      }
   }

   /* Add filename parameter to command line. */
   AddParameter(apszArguments, &nArgCount, pszFilename);

   /* Display prompt to user providing */
   od_printf("Begin your transfer now, or press [Ctrl]-[X] several times to abort.\n\r");

   /* Execute command using the OpenDoors od_spawn() function. */
   return(od_spawnvpe(P_WAIT, apszArguments[0], apszArguments, NULL) == 0);
}


/* Function to add next parameter to array of parameters to pass to */
/* od_spawnvpe().                                                   */
void AddParameter(char **papszArguments, int *pnCount, char *pszNewArgument)
{
   assert(*pnCount < ARGS_ARRAY_SIZE - 1);
   assert(papszArguments != NULL);
   assert(pnCount != NULL);
   assert(pszNewArgument != NULL);

   /* Add next argument to array. */
   papszArguments[(*pnCount)++] = pszNewArgument;

   /* Ensure that the array is always NULL-terminated. */
   papszArguments[*pnCount] = NULL;
}
