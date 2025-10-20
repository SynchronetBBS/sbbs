// MItems -- make items

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "defines.h"
#include "k_items.h"
#include "myopen.h"
#include "serialize.h"
#include "structs.h"

#define NPCS_NOTHERE    0   // status of NPC, not here -- i.e. not in town
#define NPCS_HERE       1

static struct item_data *Items[MAX_ITEMS];

static int16_t TotalItems;

static void Init_Items(char *szFileName);
static void Deinit_Items(void);


// 04/03/2002 [au]
// forgive me, this is a hack.  i don't remember what these were defined as, so i'm gonna make it up here
#define ITM_STAT 0
#define ITM_REQ 1
#define ITM_PEN 2

int main(int argc, char *argv[])
{
	FILE *fpData;
	int iTemp;
	uint8_t ibuf[BUF_SIZE_item_data];
	int16_t tmp16;

	if (argc != 3) {
		printf("usage:\nmitems items.txt items.dat\n");
		exit(0);
	}

	Init_Items(argv[1]);

	fpData = fopen(argv[2], "wb");

	/* fwrite number of items */
	printf("Writing %d items.\n", TotalItems);
	tmp16 = SWAP16(TotalItems);
	fwrite(&tmp16, sizeof(tmp16), 1, fpData);
	for (iTemp = 0; iTemp < TotalItems; iTemp++) {
		s_item_data_s(Items[iTemp], ibuf, sizeof(ibuf));
		fwrite(ibuf, sizeof(ibuf), 1, fpData);
	}

	fclose(fpData);

	Deinit_Items();
	return(0);
}

static void Deinit_Items(void)
{
	int iTemp;

	for (iTemp = 0; iTemp < MAX_ITEMS; iTemp++) {
		if (Items[iTemp])
			free(Items[iTemp]);
	}
}

static void Init_Items(char *szFileName)
{
	FILE *fpItems;
	char szLine[255], *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	size_t uCount;
	int iKeyWord;
	int CurItem = -1;
	int iTemp;
	char TypeOfStat=0;

	fpItems = fopen(szFileName, "r");
	if (!fpItems) {
		printf("Error opening items file.\n");
		exit(0);
	}

	/* make all items NULL pointers */
	for (iTemp = 0; iTemp < MAX_ITEMS; iTemp++)
		Items[iTemp] = NULL;

	for (;;) {
		/* read in a line */
		if (fgets(szLine, 255, fpItems) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;
		while (*pcCurrentPos) {
			/* skip all comment lines */
			if (*pcCurrentPos=='\n' || *pcCurrentPos=='\r' || *pcCurrentPos==';'
					|| *pcCurrentPos == '%' || *pcCurrentPos == '#') {
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
		for (iKeyWord = 0; iKeyWord < MAX_IM_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (stricmp(szToken, papszItemKeyWords[iKeyWord]) == 0) {
				/* Process token */
				switch (iKeyWord) {
					case 0 :    /* NAME of item */
						/* see if out of items memory yet */
						++CurItem;
						if (CurItem == MAX_ITEMS) {
							break;
						}

						/* allocate mem for this room */
						Items[CurItem] = calloc(1, sizeof(struct item_data));
						memset(Items[CurItem], 0, sizeof(struct item_data));

						TypeOfStat = ITM_STAT;

						/* initialize it */
						Items[CurItem]->Special = false;
						Items[CurItem]->cType = I_OTHER;


						for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++) {
							Items[CurItem]->Attributes[iTemp] = 0;
							Items[CurItem]->ReqAttributes[iTemp] = 0;
						}

						Items[CurItem]->lCost = 0;
						Items[CurItem]->DiffMaterials = false;
						Items[CurItem]->Energy = 300;
						Items[CurItem]->MarketLevel = 0;

						Items[CurItem]->VillageType = V_ALL;
						Items[CurItem]->RandLevel = 0;
						Items[CurItem]->HPAdd = 0;
						Items[CurItem]->SPAdd = 0;
						Items[CurItem]->SpellNum = -1;  // no spell

						strlcpy(Items[CurItem]->szName, pcCurrentPos, sizeof(Items[CurItem]->szName));

						printf("%2d: %s\n", CurItem, Items[CurItem]->szName);

						break;
					case 1 :    /* Type */
						if (stricmp(pcCurrentPos, "Weapon") == 0) {
							Items[CurItem]->cType = I_WEAPON;
						}
						else if (stricmp(pcCurrentPos, "Armor") == 0) {
							Items[CurItem]->cType = I_ARMOR;
						}
						else if (stricmp(pcCurrentPos, "Shield") == 0) {
							Items[CurItem]->cType = I_SHIELD;
						}
						else if (stricmp(pcCurrentPos, "Scroll") == 0) {
							Items[CurItem]->cType = I_SCROLL;
						}
						else if (stricmp(pcCurrentPos, "Book") == 0) {
							Items[CurItem]->cType = I_BOOK;
						}
						else if (stricmp(pcCurrentPos, "Other") == 0) {
							Items[CurItem]->cType = I_OTHER;
						}
						break;
					case 2 :    /* Special */
						Items[CurItem]->Special = true;
						break;
					case 3 :    /* Agility */
					case 4 :    /* Dexterity */
					case 5 :    /* Strength */
					case 6 :    /* Wisdom */
					case 7 :    /* Armor */
					case 8 :    /* Charisma */
						if (TypeOfStat == ITM_STAT) {
							Items[CurItem]->Attributes[iKeyWord - 3] = atoi(pcCurrentPos);
						}
						else if (TypeOfStat == ITM_REQ) {
							Items[CurItem]->ReqAttributes[iKeyWord - 3] = atoi(pcCurrentPos);
						}
						break;
					case 9 :    /* Cost */
						Items[CurItem]->lCost = atol(pcCurrentPos);
						break;
					case 10 :    /* DiffMaterials */
						Items[CurItem]->DiffMaterials = true;
						break;
					case 11 :   /* normal stats */
						TypeOfStat = ITM_STAT;
						break;
					case 12 :   /* requirements */
						TypeOfStat = ITM_REQ;
						break;
					case 13 :   /* penalties */
						TypeOfStat = ITM_PEN;
						break;
					case 14 :    /* Energy */
						Items[CurItem]->Energy = atoi(pcCurrentPos);
						break;
					case 15 :    /* Uses */
						Items[CurItem]->Energy = atoi(pcCurrentPos);
						break;
					case 16 :    /* Spell # */
						Items[CurItem]->SpellNum = atoi(pcCurrentPos) - 1;
						printf("spellnum = %d\n", Items[CurItem]->SpellNum);
						break;
					case 17 :    /* Market Level */
						Items[CurItem]->MarketLevel = atoi(pcCurrentPos);
						break;
					case 18 :   // villagetypes
						if (!stricmp(pcCurrentPos, "ALL"))
							Items[CurItem]->VillageType = V_ALL;
						else
							Items[CurItem]->VillageType = atoi(pcCurrentPos);
						break;
					case 19 :   // randlevel
						Items[CurItem]->RandLevel = atoi(pcCurrentPos);
						break;
					case 20 :   // HP
						Items[CurItem]->HPAdd = atoi(pcCurrentPos);
						break;
					case 21 :   // SP
						Items[CurItem]->SPAdd = atoi(pcCurrentPos);
						break;
				}
				break;
			}
		}
	}

	/* since they started at -1 and not 0 */
	CurItem++;

	TotalItems = CurItem;
	//pause();

	fclose(fpItems);
}
