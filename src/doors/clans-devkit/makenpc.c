// MakeNPC

#include <stdio.h>
#include <time.h>
#ifdef __MSDOS__
#include <malloc.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
typedef char               BOOL;           // Boolean value, at least 1 bit. 
-- BOOL is defined by 'defines.h' 
*/
#include "structs.h"
#include "keywords.h"

/* Defined in 'defines.h'
#define TRUE    1
#define FALSE   0
*/

// areas to wander
#define WN_NONE     0       // not a wanderer
#define WN_CHURCH   1
#define WN_STREET   2
#define WN_MARKET   3
#define WN_TOWNHALL 5
#define WN_THALL    6
#define WN_REBEL    7
#define WN_MINE     8

#define NPCS_NOTHERE    0   // status of NPC, not here -- i.e. not in town
#define NPCS_HERE       1

int TotalItems, TotalRaces, TotalClasses, TotalSpells;

void GetToken ( char *szString, char *szToken );
void Init_NPCs ( char *szInfile, char *szOutfile );

#if defined(__BORLANDC__) && defined(__MSDOS__)
extern unsigned _stklen = 32000U;
#endif /* __BORLANDC__ */

int main (int argc, char **argv)
{
    if (argc != 3)
    {
        printf("\nformat:\n MakeNPC [infile.txt] [outfile.npc]\n");
        exit(0);
    }

    printf("NPC is %d bytes\n", sizeof(struct NPCInfo));

    Init_NPCs( argv[1], argv[2] );
	return(0);
}

void Init_NPCs ( char *szInfile, char *szOutfile )
{
    struct NPCInfo *NPCInfo;
    FILE *fpNPC, *fpNPCDat;
	char szLine[255], *pcCurrentPos, szString[255];
	char szToken[MAX_TOKEN_CHARS + 1]/*, *pcAt*/;
	unsigned int uCount;
	int iKeyWord;
    int CurNPC = -1;
    int iTemp; /*, OrigNPC*/
	int TopicsKnown=0;

    fpNPC = fopen(szInfile, "r");
    if (!fpNPC)
	{
        printf("Error reading %s\n", szInfile);
		exit(0);
	}
    fpNPCDat = fopen(szOutfile, "wb");
    if (!fpNPCDat)
	{
        printf("Error writing %s\n", szOutfile);
		exit(0);
	}

    NPCInfo = malloc(sizeof(struct NPCInfo));

	for (;;)
	{
		/* read in a line */
        if (fgets(szLine, 255, fpNPC) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;
		while(*pcCurrentPos)
		{
            /* skip all comment lines */
			if(*pcCurrentPos=='\n' || *pcCurrentPos=='\r' || *pcCurrentPos==';'
               || *pcCurrentPos == '#')
			{
				*pcCurrentPos='\0';
				break;
			}
			++pcCurrentPos;
		 }

		/* Search for beginning of first token on line */
		pcCurrentPos=(char *)szLine;
		while(*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

		/* If no token was found, proceed to process the next line */
		if(!*pcCurrentPos) continue;

		/* Get first token from line */
		uCount=0;
		while(*pcCurrentPos && !isspace(*pcCurrentPos))
		{
			if(uCount<MAX_TOKEN_CHARS) szToken[uCount++]=*pcCurrentPos;
			++pcCurrentPos;
		}
		if(uCount<=MAX_TOKEN_CHARS)
			szToken[uCount]='\0';
		else
			szToken[MAX_TOKEN_CHARS]='\0';

		/* Find beginning of keyword parameter */
		while(*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

		/* Trim trailing spaces from setting string */
		for(uCount=strlen(pcCurrentPos)-1;uCount>0;--uCount)
		{
			if(isspace(pcCurrentPos[uCount]))
			{
				pcCurrentPos[uCount]='\0';
			}
		else
			{
				break;
			}
		}

		if (szToken[0] == '$')
			break;

		/* Loop through list of keywords */
        for(iKeyWord = 0; iKeyWord < MAX_NPC_WORDS; ++iKeyWord)
		{
			/* If keyword matches */
            if(_STRCASECMP(szToken, papszNPCKeyWords[iKeyWord]) == 0)
			{
				/* Process token */
				switch (iKeyWord)
				{
                    case 12 :   /* Index of NPC */
                        // write old NPC, if there was one, to file
                        if (CurNPC != -1)
                        {
                            NPCInfo->KnownTopics =  TopicsKnown;
                            fwrite(NPCInfo, sizeof(struct NPCInfo), 1, fpNPCDat);
                        }
                        ++CurNPC;
                        printf("%s\n", pcCurrentPos);

                        /* initialize NPC */
                        memset(NPCInfo, 0, sizeof(struct NPCInfo));
                        strcpy(NPCInfo->szIndex, pcCurrentPos);

                        // no topics known just yet
                        NPCInfo->IntroTopic.Active = FALSE;
                        for (iTemp = 0; iTemp < MAX_TOPICS; iTemp++)
                            NPCInfo->Topics[iTemp].Active = FALSE;

                        NPCInfo->Loyalty = 5;    // average loyalty
                        NPCInfo->Roamer = FALSE;
                        NPCInfo->WhereWander = WN_NONE;
                        NPCInfo->NPCPCIndex = -1;
                        NPCInfo->MaxTopics = -1;     // no maximum
                        NPCInfo->OddsOfSeeing = 0;
                        NPCInfo->szHereNews[0] = 0;
                        strcpy(NPCInfo->szQuoteFile, "/q/NpcQuote");
                        NPCInfo->szMonFile[0] = 0;

                        TopicsKnown = 0;
						break;
                    case 1 :    // loyalty
                        NPCInfo->Loyalty = atoi(pcCurrentPos);
                        break;
                    case 2 :    // KnownTopic
                        NPCInfo->Topics[TopicsKnown].Active = TRUE;
                        NPCInfo->Topics[TopicsKnown].Known = TRUE;
                        NPCInfo->Topics[TopicsKnown].ClanInfo = FALSE;

                        GetToken(pcCurrentPos, szString);
                        printf("KnownTopic = %s\n", pcCurrentPos);

                        // topic as it appears in the quote file
                        strcpy(NPCInfo->Topics[TopicsKnown].szFileName,
                            szString);

                        // topic name as it appears to user
                        strcpy(NPCInfo->Topics[TopicsKnown].szName,
                            pcCurrentPos);

                        TopicsKnown++;
                        break;
                    case 3 :    // Topic
                        NPCInfo->Topics[TopicsKnown].Active = TRUE;
                        NPCInfo->Topics[TopicsKnown].Known = FALSE;
                        NPCInfo->Topics[TopicsKnown].ClanInfo = FALSE;

                        GetToken(pcCurrentPos, szString);
                        printf("Topic = %s\n", pcCurrentPos);

                        // topic as it appears in the quote file
                        strcpy(NPCInfo->Topics[TopicsKnown].szFileName,
                            szString);

                        // topic name as it appears to user
                        strcpy(NPCInfo->Topics[TopicsKnown].szName,
                            pcCurrentPos);

                        TopicsKnown++;
                        break;
                    case 4 :    // where does he wander.
                        // leave it for now
                        if (_STRCASECMP(pcCurrentPos, "Church") == 0)
                        {
                            NPCInfo->WhereWander = WN_CHURCH;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "Street") == 0)
                        {
                            NPCInfo->WhereWander = WN_STREET;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "Market") == 0)
                        {
                            NPCInfo->WhereWander = WN_MARKET;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "Town Hall") == 0)
                        {
                            NPCInfo->WhereWander = WN_TOWNHALL;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "Training Hall") == 0)
                        {
                            NPCInfo->WhereWander = WN_THALL;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "Rebel Menu") == 0)
                        {
                            NPCInfo->WhereWander = WN_REBEL;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "Mine") == 0)
                        {
                            NPCInfo->WhereWander = WN_MINE;
                        }
                        else
                            printf("\aIncorrect usage: %s\n", pcCurrentPos);
                        break;
                    case 5 :    // which NPC in the NPC.PC is he?
                        NPCInfo->NPCPCIndex = atoi(pcCurrentPos);
                        break;
                    case 6 :
                        NPCInfo->MaxTopics = atoi(pcCurrentPos);
                        break;
                    case 7 :    // odds of seing
                        NPCInfo->OddsOfSeeing = atoi(pcCurrentPos);
                        printf("Odds of seeing are %d%%\n", NPCInfo->OddsOfSeeing);
                        break;
                    case 8 :    // introtopic
                        NPCInfo->IntroTopic.Active = TRUE;
                        NPCInfo->IntroTopic.Known = FALSE;
                        NPCInfo->IntroTopic.ClanInfo = FALSE;

                        // topic name as it appears to user
                        strcpy(NPCInfo->IntroTopic.szFileName, pcCurrentPos);
                        printf("Intro topic = %s\n", pcCurrentPos);
                        break;
                    case 9 :    // here news
                        if (strlen(pcCurrentPos) > 69)
                            pcCurrentPos[69] = 0;
                        strcpy(NPCInfo->szHereNews, pcCurrentPos);
                        break;
                    case 0 :    // name of NPC
                        strcpy(NPCInfo->szName, pcCurrentPos);
                        break;
                    case 10 :   // quotefile to use
                        strcpy(NPCInfo->szQuoteFile, pcCurrentPos);
                        break;
                    case 11 :   // .mon file to use
                        strcpy(NPCInfo->szMonFile, pcCurrentPos);
                        break;
				}
				break;
			}
		}
	}

    // write the last NPC to file
    if (CurNPC != -1)
    {
        NPCInfo->KnownTopics =  TopicsKnown;
        fwrite(NPCInfo, sizeof(struct NPCInfo), 1, fpNPCDat);
    }

	/* since they started at -1 and not 0 */
    CurNPC++;

    printf("%d NPCs found.\n%ld bytes used", CurNPC, (long) CurNPC*sizeof(struct NPCInfo));

    fclose(fpNPC);
    fclose(fpNPCDat);
    free(NPCInfo);
}

void GetToken ( char *szString, char *szToken )
{
	char *pcCurrentPos;
	unsigned int uCount;

	/* Ignore all of line after comments or CR/LF char */
	pcCurrentPos=(char *)szString;
	while(*pcCurrentPos)
	{
		if(*pcCurrentPos=='\n' || *pcCurrentPos=='\r')
		{
			*pcCurrentPos='\0';
			break;
		}
		++pcCurrentPos;
	}

	/* Search for beginning of first token on line */
	pcCurrentPos = (char *)szString;
	while(*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

	/* If no token was found, proceed to process the next line */
	if(!*pcCurrentPos)
	{
		szToken[0] = 0;
		szString[0] = 0;
		return;
	}

	/* Get first token from line */
	uCount=0;
	while(*pcCurrentPos && !isspace(*pcCurrentPos))
	{
		if(uCount<MAX_TOKEN_CHARS) szToken[uCount++]=*pcCurrentPos;
		++pcCurrentPos;
	}
	if(uCount<=MAX_TOKEN_CHARS)
		szToken[uCount]='\0';
	else
		szToken[MAX_TOKEN_CHARS]='\0';

	/* Find beginning of configuration option parameters */
	while(*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

	/* Trim trailing spaces from setting string */
	for(uCount=strlen(pcCurrentPos)-1;uCount>0;--uCount)
	{
		if(isspace(pcCurrentPos[uCount]))
		{
			pcCurrentPos[uCount]='\0';
		}
		else
		{
			break;
		}
	}

	strcpy(szString, pcCurrentPos);
}
