// MakeNPC

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __MSDOS__
# include <malloc.h>
#endif
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "defines.h"
#include "k_npcs.h"
#include "myopen.h"
#include "parsing.h"
#include "serialize.h"
#include "structs.h"

static void Init_NPCs(char *szInfile, char *szOutfile);

#if defined(__BORLANDC__) && defined(__MSDOS__)
extern unsigned _stklen = 32000U;
#endif /* __BORLANDC__ */

int main(int argc, char **argv)
{
	if (argc != 3) {
		printf("\nformat:\n MakeNPC [infile.txt] [outfile.npc]\n");
		exit(0);
	}

	printf("NPC is %d bytes\n", BUF_SIZE_NPCInfo);

	Init_NPCs(argv[1], argv[2]);
	return(0);
}

static void Init_NPCs(char *szInfile, char *szOutfile)
{
	struct NPCInfo *NPCInfo;
	FILE *fpNPC, *fpNPCDat;
	char szLine[255], *pcCurrentPos, szString[255];
	char szToken[MAX_TOKEN_CHARS + 1]/*, *pcAt*/;
	size_t uCount;
	int iKeyWord;
	int CurNPC = -1;
	int iTemp; /*, OrigNPC*/
	int TopicsKnown=0;
	uint8_t nbuf[BUF_SIZE_NPCInfo];

	fpNPC = fopen(szInfile, "r");
	if (!fpNPC) {
		printf("Error reading %s\n", szInfile);
		exit(0);
	}
	fpNPCDat = fopen(szOutfile, "wb");
	if (!fpNPCDat) {
		printf("Error writing %s\n", szOutfile);
		exit(0);
	}

	NPCInfo = calloc(1, sizeof(struct NPCInfo));

	for (;;) {
		/* read in a line */
		if (fgets(szLine, 255, fpNPC) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;
		while (*pcCurrentPos) {
			/* skip all comment lines */
			if (*pcCurrentPos=='\n' || *pcCurrentPos=='\r' || *pcCurrentPos==';'
					|| *pcCurrentPos == '#') {
				*pcCurrentPos='\0';
				break;
			}
			++pcCurrentPos;
		}

		/* Search for beginning of first token on line */
		pcCurrentPos=(char *)szLine;
		while (*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;

		/* Get first token from line */
		uCount=0;
		while (*pcCurrentPos && !isspace(*pcCurrentPos)) {
			if (uCount<MAX_TOKEN_CHARS) szToken[uCount++]=*pcCurrentPos;
			++pcCurrentPos;
		}
		if (uCount<=MAX_TOKEN_CHARS)
			szToken[uCount]='\0';
		else
			szToken[MAX_TOKEN_CHARS]='\0';

		/* Find beginning of keyword parameter */
		while (*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

		/* Trim trailing spaces from setting string */
		if (*pcCurrentPos) {
			for (uCount=strlen(pcCurrentPos)-1; uCount>0; --uCount) {
				if (isspace(pcCurrentPos[uCount])) {
					pcCurrentPos[uCount]='\0';
				}
				else {
					break;
				}
			}
		}

		if (szToken[0] == '$')
			break;

		/* Loop through list of keywords */
		for (iKeyWord = 0; iKeyWord < MAX_NPC_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (strcasecmp(szToken, papszNPCKeyWords[iKeyWord]) == 0) {
				/* Process token */
				switch (iKeyWord) {
					case 12 :   /* Index of NPC */
						// write old NPC, if there was one, to file
						if (CurNPC != -1) {
							NPCInfo->KnownTopics =  TopicsKnown;
							s_NPCInfo_s(NPCInfo, nbuf, sizeof(nbuf));
							fwrite(nbuf, sizeof(nbuf), 1, fpNPCDat);
						}
						++CurNPC;
						printf("%s\n", pcCurrentPos);

						/* initialize NPC */
						memset(NPCInfo, 0, sizeof(struct NPCInfo));
						strlcpy(NPCInfo->szIndex, pcCurrentPos, sizeof(NPCInfo->szIndex));

						// no topics known just yet
						NPCInfo->IntroTopic.Active = false;
						for (iTemp = 0; iTemp < MAX_TOPICS; iTemp++)
							NPCInfo->Topics[iTemp].Active = false;

						NPCInfo->Loyalty = 5;    // average loyalty
						NPCInfo->Roamer = false;
						NPCInfo->WhereWander = WN_NONE;
						NPCInfo->NPCPCIndex = -1;
						NPCInfo->MaxTopics = -1;     // no maximum
						NPCInfo->OddsOfSeeing = 0;
						NPCInfo->szHereNews[0] = 0;
						strlcpy(NPCInfo->szQuoteFile, "/q/NpcQuote", sizeof(NPCInfo->szQuoteFile));
						NPCInfo->szMonFile[0] = 0;

						TopicsKnown = 0;
						break;
					case 1 :    // loyalty
						NPCInfo->Loyalty = atoi(pcCurrentPos);
						break;
					case 2 :    // KnownTopic
						NPCInfo->Topics[TopicsKnown].Active = true;
						NPCInfo->Topics[TopicsKnown].Known = true;
						NPCInfo->Topics[TopicsKnown].ClanInfo = false;

						GetToken(pcCurrentPos, szString);
						printf("KnownTopic = %s\n", pcCurrentPos);

						// topic as it appears in the quote file
						strlcpy(NPCInfo->Topics[TopicsKnown].szFileName, szString, sizeof(NPCInfo->Topics[TopicsKnown].szFileName));

						// topic name as it appears to user
						strlcpy(NPCInfo->Topics[TopicsKnown].szName, pcCurrentPos, sizeof(NPCInfo->Topics[TopicsKnown].szName));

						TopicsKnown++;
						break;
					case 3 :    // Topic
						NPCInfo->Topics[TopicsKnown].Active = true;
						NPCInfo->Topics[TopicsKnown].Known = false;
						NPCInfo->Topics[TopicsKnown].ClanInfo = false;

						GetToken(pcCurrentPos, szString);
						printf("Topic = %s\n", pcCurrentPos);

						// topic as it appears in the quote file
						strlcpy(NPCInfo->Topics[TopicsKnown].szFileName, szString, sizeof(NPCInfo->Topics[TopicsKnown].szFileName));

						// topic name as it appears to user
						strlcpy(NPCInfo->Topics[TopicsKnown].szName, pcCurrentPos, sizeof(NPCInfo->Topics[TopicsKnown].szName));

						TopicsKnown++;
						break;
					case 4 :    // where does he wander.
						// leave it for now
						if (strcasecmp(pcCurrentPos, "Church") == 0) {
							NPCInfo->WhereWander = WN_CHURCH;
						}
						else if (strcasecmp(pcCurrentPos, "Street") == 0) {
							NPCInfo->WhereWander = WN_STREET;
						}
						else if (strcasecmp(pcCurrentPos, "Market") == 0) {
							NPCInfo->WhereWander = WN_MARKET;
						}
						else if (strcasecmp(pcCurrentPos, "Town Hall") == 0) {
							NPCInfo->WhereWander = WN_TOWNHALL;
						}
						else if (strcasecmp(pcCurrentPos, "Training Hall") == 0) {
							NPCInfo->WhereWander = WN_THALL;
						}
						else if (strcasecmp(pcCurrentPos, "Rebel Menu") == 0) {
							NPCInfo->WhereWander = WN_REBEL;
						}
						else if (strcasecmp(pcCurrentPos, "Mine") == 0) {
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
						NPCInfo->IntroTopic.Active = true;
						NPCInfo->IntroTopic.Known = false;
						NPCInfo->IntroTopic.ClanInfo = false;

						// topic name as it appears to user
						strlcpy(NPCInfo->IntroTopic.szFileName, pcCurrentPos, sizeof(NPCInfo->IntroTopic.szFileName));
						printf("Intro topic = %s\n", pcCurrentPos);
						break;
					case 9 :    // here news
						if (strlen(pcCurrentPos) > 69)
							pcCurrentPos[69] = 0;
						strlcpy(NPCInfo->szHereNews, pcCurrentPos, sizeof(NPCInfo->szHereNews));
						break;
					case 0 :    // name of NPC
						strlcpy(NPCInfo->szName, pcCurrentPos, sizeof(NPCInfo->szName));
						break;
					case 10 :   // quotefile to use
						strlcpy(NPCInfo->szQuoteFile, pcCurrentPos, sizeof(NPCInfo->szQuoteFile));
						break;
					case 11 :   // .mon file to use
						strlcpy(NPCInfo->szMonFile, pcCurrentPos, sizeof(NPCInfo->szMonFile));
						break;
				}
				break;
			}
		}
	}

	// write the last NPC to file
	if (CurNPC != -1) {
		NPCInfo->KnownTopics =  TopicsKnown;
		s_NPCInfo_s(NPCInfo, nbuf, sizeof(nbuf));
		fwrite(nbuf, sizeof(nbuf), 1, fpNPCDat);
	}

	/* since they started at -1 and not 0 */
	CurNPC++;

	printf("%d NPCs found.\n%ld bytes used", CurNPC, (long)CurNPC * BUF_SIZE_NPCInfo);

	fclose(fpNPC);
	fclose(fpNPCDat);
	free(NPCInfo);
}
