#include "netmail.h"  //The Inter-BBS Kit
// #include "\gac_Cs\od6\opendoor.h" // Open doors toolkit

#include "genwrap.h"
#include <string.h>
#include <stdlib.h>

extern tIBResult PrivateReadConfig(tIBInfo *pInfo, char *pszConfigFile);

#define NUM_KEYWORDS       7

// Needed to link the systems
#define KEYWORD_LINK_WITH  0
#define KEYWORD_LINK_NAME  1
#define KEYWORD_LINK_LOC   2
#define KEYWORD_LINK_NUMBER  3
#define KEYWORD_LINK_HOST   4
#define KEYWORD_USE_WAGON_WHEEL 5
#define KEYWORD_CHECK_FOR_DUPES 6

extern char wagon_wheel;
extern INT16 thisbbs;
extern char checkForDupes;


char *apszKeyWord[NUM_KEYWORDS] = {
					"LinkWith",
					"LinkName",
					"LinkLocation",
					"LinkNumber",
					"LinkHost",
					"Use_Wagon_Wheel",
			   "Check_For_Dupes",
				   };

tIBResult IBReadConfig(tIBInfo *pInfo, char *pszConfigFile)
   {
	INT16 system=0;

   // Set default values for pInfo settings 
   pInfo->nTotalSystems = 0;
   pInfo->paOtherSystem = NULL;

   // Set defaults for our variables (if desired)
   wagon_wheel=FALSE;
   checkForDupes = TRUE;


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

   // Re-Access the file, with the program's unique Inter-BBS options
   return(PrivateReadConfig(pInfo, pszConfigFile));
   
   // else 
   // 12/96 return(eSuccess);

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
	case KEYWORD_CHECK_FOR_DUPES:
	 if (stricmp(pszParameter, "TRUE")==0) checkForDupes = TRUE;
	 else checkForDupes = FALSE;
	 break;



	  }
   }
