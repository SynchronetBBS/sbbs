// MItems -- make items

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef __MSDOS__
#include <malloc.h>
#endif /* __MSDOS__ */

/*
typedef char               BOOL;           // Boolean value, at least 1 bit. 
-- BOOL is defined in 'defines.h' included by structs.h
*/
// #include "cinclude.h"
#include "structs.h"
#include "keywords.h"

/* Defined in 'defines.h'
#define TRUE    1
#define FALSE   0
*/

#define NPCS_NOTHERE    0   // status of NPC, not here -- i.e. not in town
#define NPCS_HERE       1

struct item_data *Items[MAX_ITEMS];

int TotalItems, TotalRaces, TotalClasses, TotalSpells;

void Init_Items ( char *szFileName );
void Deinit_Items( void );


// 04/03/2002 [au]
// forgive me, this is a hack.  i don't remember what these were defined as, so i'm gonna make it up here
#define ITM_STAT 0
#define ITM_REQ 1
#define ITM_PEN 2

int main ( int argc, char *argv[] )
{
    FILE *fpData;
    int iTemp;

    if (argc != 3)
    {
        printf("usage:\nmitems items.txt items.dat\n");
        exit(0);
    }


    Init_Items(argv[1]);

    fpData = fopen(argv[2], "wb");

    /* fwrite number of items */
    printf("Writing %d items.\n", TotalItems);
    fwrite(&TotalItems, sizeof(_INT16), 1, fpData);
    for (iTemp = 0; iTemp < TotalItems; iTemp++)
        fwrite(Items[iTemp], sizeof(struct item_data), 1, fpData);

    fclose(fpData);

    Deinit_Items();
	return(0);
}

void Deinit_Items( void )
{
	int iTemp;

	for (iTemp = 0; iTemp < MAX_ITEMS; iTemp++)
	{
		if (Items[iTemp])
			free(Items[iTemp]);
	}
}

void Init_Items ( char *szFileName )
{
	FILE *fpItems;
	char szLine[255], *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	unsigned int uCount;
	int iKeyWord;
	int CurItem = -1;
	int iTemp;
	char TypeOfStat=0;

	fpItems = fopen(szFileName, "r");
	if (!fpItems)
	{
		printf("Error opening items file.\n");
		exit(0);
	}

	/* make all items NULL pointers */
	for (iTemp = 0; iTemp < MAX_ITEMS; iTemp++)
		Items[iTemp] = NULL;

	for (;;)
	{
		/* read in a line */
		if (fgets(szLine, 255, fpItems) == NULL) break;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;
		while(*pcCurrentPos)
		{
			/* skip all comment lines */
			if(*pcCurrentPos=='\n' || *pcCurrentPos=='\r' || *pcCurrentPos==';'
			   || *pcCurrentPos == '%' || *pcCurrentPos == '#')
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
		for(iKeyWord = 0; iKeyWord < MAX_IM_WORDS; ++iKeyWord)
		{
			/* If keyword matches */
			if(_STRCASECMP(szToken, papszItemKeyWords[iKeyWord]) == 0)
			{
				/* Process token */
				switch (iKeyWord)
				{
					case 0 :	/* NAME of item */
						/* see if out of items memory yet */
						++CurItem;
						if (CurItem == MAX_ITEMS)
						{
							break;
						}

						/* allocate mem for this room */
						Items[CurItem] = malloc(sizeof(struct item_data));
                        memset(Items[CurItem], 0, sizeof(struct item_data));

						TypeOfStat = ITM_STAT;

						/* initialize it */
                        Items[CurItem]->Special = FALSE;
                        Items[CurItem]->cType = I_OTHER;


						for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
						{
							Items[CurItem]->Attributes[iTemp] = 0;
							Items[CurItem]->ReqAttributes[iTemp] = 0;
						}

						Items[CurItem]->lCost = 0;
						Items[CurItem]->DiffMaterials = FALSE;
                        Items[CurItem]->Energy = 300;
                        Items[CurItem]->MarketLevel = 0;

                        Items[CurItem]->VillageType = V_ALL;
                        Items[CurItem]->RandLevel = 0;
                        Items[CurItem]->HPAdd = 0;
                        Items[CurItem]->SPAdd = 0;
                        Items[CurItem]->SpellNum = -1;  // no spell

						strcpy(Items[CurItem]->szName, pcCurrentPos);

                        printf("%2d: %s\n", CurItem, Items[CurItem]->szName);

						break;
					case 1 :	/* Type */
						if (_STRCASECMP(pcCurrentPos, "Weapon") == 0)
						{
							Items[CurItem]->cType = I_WEAPON;
						}
						else if (_STRCASECMP(pcCurrentPos, "Armor") == 0)
						{
							Items[CurItem]->cType = I_ARMOR;
						}
						else if (_STRCASECMP(pcCurrentPos, "Shield") == 0)
						{
							Items[CurItem]->cType = I_SHIELD;
						}
                        else if (_STRCASECMP(pcCurrentPos, "Scroll") == 0)
						{
                            Items[CurItem]->cType = I_SCROLL;
						}
                        else if (_STRCASECMP(pcCurrentPos, "Book") == 0)
						{
                            Items[CurItem]->cType = I_BOOK;
						}
                        else if (_STRCASECMP(pcCurrentPos, "Other") == 0)
						{
                            Items[CurItem]->cType = I_OTHER;
						}
						break;
                    case 2 :    /* Special */
                        Items[CurItem]->Special = TRUE;
						break;
					case 3 :	/* Agility */
					case 4 :	/* Dexterity */
					case 5 :	/* Strength */
					case 6 :	/* Wisdom */
					case 7 :	/* Armor */
					case 8 :	/* Charisma */
						if (TypeOfStat == ITM_STAT)
						{
							Items[CurItem]->Attributes[iKeyWord - 3] = atoi(pcCurrentPos);
						}
						else if (TypeOfStat == ITM_REQ)
						{
							Items[CurItem]->ReqAttributes[iKeyWord - 3] = atoi(pcCurrentPos);
						}
						break;
					case 9 :	/* Cost */
						Items[CurItem]->lCost = atol(pcCurrentPos);
						break;
					case 10 :	 /* DiffMaterials */
						Items[CurItem]->DiffMaterials = TRUE;
						break;
					case 11 :	/* normal stats */
						TypeOfStat = ITM_STAT;
						break;
					case 12 :	/* requirements */
						TypeOfStat = ITM_REQ;
						break;
					case 13 :	/* penalties */
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
                        if (!_STRCASECMP(pcCurrentPos, "ALL"))
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
