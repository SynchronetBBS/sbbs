#include "netmail.h"  //The Inter-BBS Kit
#include "OpenDoor.h" // Open doors toolkit

#include "genwrap.h"
#include <string.h>
#include <stdlib.h>

#define NUM_KEYWORDS       13

// Needed to link the systems
#define KEYWORD_LINK_WITH  0
#define KEYWORD_LINK_NAME  1
#define KEYWORD_LINK_LOC   2
#define KEYWORD_LINK_NUMBER  3
#define KEYWORD_LINK_HOST   4
#define KEYWORD_USE_WAGON_WHEEL 5

// This programs config options
#define KEYWORD_TOTAL_DECKS    6
#define KEYWORD_MIN_BET  7
#define KEYWORD_MAX_BET   8
#define KEYWORD_DEFAULT_BET  9
#define KEYWORD_START_MONEY   10
#define KEYWORD_DAILY_MONEY  11
#define KEYWORD_LENGTH  12

extern char wagon_wheel;
extern int thisbbs;
// define external variables
extern unsigned char total_decks, sys_decks;
extern unsigned int min_bet, max_bet, ibet;
extern long int startmoney, dailymoney;
extern int tourneylength;

char *apszKeyWord[NUM_KEYWORDS] = {
					"LinkWith",
					"LinkName",
					"LinkLocation",
					"LinkNumber",
					"LinkHost",
					"Use_Wagon_Wheel",
					// These can be changed for each application
					"Total_Decks",
					"Min_bet",
					"Max_bet",
					"Default_bet",
					"Start_Money",
					"Daily_Money",
					 "Length",
				   };

tIBResult IBReadConfig(tIBInfo *pInfo, char *pszConfigFile)
   {
	int system=0;

   // Set default values for pInfo settings 
   pInfo->nTotalSystems = 0;
   pInfo->paOtherSystem = NULL;

   // Set defaults for our variables (if desired)
   wagon_wheel=FALSE;


   // Process configuration file 
   if(!ProcessConfigFile(pszConfigFile, NUM_KEYWORDS, apszKeyWord,
			 ProcessConfigLine, (void *)pInfo))
	  {
	  return(eFileOpenError);
	  }
		
		// process the temphost information (see who else we need to send files to)
		// see if we are listed in this BBSs host routine
		nextbbs = strtok(temphost, " ");
		do
		{
			for (system = 0; system < pInfo->nTotalSystems; system++)
			{
				if (pInfo->paOtherSystem[system].szNumber == atoi(nextbbs)) pInfo->paOtherSystem[system].szSendTo = TRUE;
			}
			nextbbs = strtok(NULL, " ");
		} while (nextbbs != NULL);

   // else 
   return(eSuccess);
   }

void ProcessConfigLine(INT16 nKeyword, char *pszParameter, void *pCallbackData)
   {
   tIBInfo *pInfo = (tIBInfo *)pCallbackData;
   tOtherNode *paNewNodeArray;

   switch(nKeyword)
	  {

	  case KEYWORD_LINK_WITH:
	 if(pInfo->nTotalSystems == 0)
			{
		pInfo->paOtherSystem = malloc(sizeof(tOtherNode));
		if(pInfo->paOtherSystem == NULL)
		   {
		   break;
		   }
		}
	 else
		{
		if((paNewNodeArray = malloc(sizeof(tOtherNode) *
		 (pInfo->nTotalSystems + 1))) == NULL)
		   {
		   break;
		   }

			memcpy(paNewNodeArray, pInfo->paOtherSystem, sizeof(tOtherNode) *
		   pInfo->nTotalSystems);

		free(pInfo->paOtherSystem);

		pInfo->paOtherSystem = paNewNodeArray;
		}

		 // set the default sendto (my addition)
	 pInfo->paOtherSystem[pInfo->nTotalSystems].szSendTo = FALSE;

	 strncpy(pInfo->paOtherSystem[pInfo->nTotalSystems].szAddress,
		 pszParameter, NODE_ADDRESS_CHARS);
	 pInfo->paOtherSystem[pInfo->nTotalSystems].
		szAddress[NODE_ADDRESS_CHARS] = '\0';
	 ++pInfo->nTotalSystems;
	 break;

			case KEYWORD_LINK_NAME:
	 if(pInfo->nTotalSystems != 0)
		{
		strncpy(pInfo->paOtherSystem[pInfo->nTotalSystems - 1].szSystemName,
			pszParameter, SYSTEM_NAME_CHARS);
		pInfo->paOtherSystem[pInfo->nTotalSystems - 1].
		   szSystemName[SYSTEM_NAME_CHARS] = '\0';
		}
	 break;

	  case KEYWORD_LINK_LOC:
	 if(pInfo->nTotalSystems != 0)
		{
/*
		 strncpy(pInfo->paOtherSystem[pInfo->nTotalSystems - 1].szLocation,
			 pszParameter, LOCATION_CHARS);
			pInfo->paOtherSystem[pInfo->nTotalSystems - 1].
			 szLocation[LOCATION_CHARS] = '\0';
*/
		 }
	 break;
	  // My additions
	  case KEYWORD_LINK_NUMBER:
	 if(pInfo->nTotalSystems != 0)
		{
		pInfo->paOtherSystem[pInfo->nTotalSystems - 1].
				 szNumber = atoi(pszParameter);
		}
	 break;

			case KEYWORD_LINK_HOST:
	 if(pInfo->nTotalSystems != 0)
			{
				// see if the BBS being processed is us, if so store the host info
				// for later processing
				if (pInfo->paOtherSystem[pInfo->nTotalSystems - 1].szNumber == thisbbs)
				{
					strcpy(temphost, pszParameter);
				}
				else
				{
					// see if we are listed in this BBSs host routine
					nextbbs = strtok(pszParameter, " ");
					do
					{
						if (atoi(nextbbs) == thisbbs) pInfo->paOtherSystem[pInfo->nTotalSystems - 1].szSendTo = TRUE;

						nextbbs = strtok(NULL, " ");
					} while (nextbbs != NULL);
				}
			}
	 break;

	case KEYWORD_USE_WAGON_WHEEL:
	 if (stricmp(pszParameter, "TRUE")==0) wagon_wheel = TRUE;
	 else wagon_wheel = FALSE;
	 break;



///////////////   This programs specific config options, these will override the GAC_BJ.CFG options

	  case KEYWORD_TOTAL_DECKS:
	 total_decks=sys_decks=atoi(pszParameter);            
	 break;

	  case KEYWORD_MIN_BET:
	 min_bet = atoi(pszParameter);                
	 break;

	  case KEYWORD_MAX_BET:
	 max_bet = atoi(pszParameter);                
	 break;

	  case KEYWORD_DEFAULT_BET:
	 ibet = atoi(pszParameter);                
	 break;
		
	  case KEYWORD_START_MONEY:
	 startmoney = atoi(pszParameter);             
	 break;

	  case KEYWORD_DAILY_MONEY:
	 dailymoney = atoi(pszParameter);             
	 break;

	  case KEYWORD_LENGTH:
	 tourneylength = atoi(pszParameter);          
	 break;

	  }
   }
