// MClass -- makes classes.dat (and races.dat)

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#ifdef __MSDOS__
#include <malloc.h>
#endif /* __MSDOS__ */

/*
typedef char               BOOL;           // Boolean value, at least 1 bit. 
-- BOOL is defined by defines.h, included in structs.h
*/
#include "structs.h"
#include "keywords.h"

/* defined by 'defines.h' 
#define TRUE    1
#define FALSE   0
*/

struct PClass *PClasses[MAX_PCLASSES], *Races[MAX_PCLASSES];

int TotalItems, TotalRaces = 0, TotalClasses = 0, TotalSpells;

void Deinit_PClasses( struct PClass *PClass[MAX_PCLASSES]);
int Init_PClasses ( struct PClass *PClass[MAX_PCLASSES], char *szFileName );

int main ( void )
{
    FILE *fpData;
    int iTemp;
/*    int StringLength;*/

    TotalRaces = Init_PClasses (Races, "races.txt");
    TotalClasses = Init_PClasses (PClasses, "classes.txt");

    fpData = fopen("races.dat", "wb");

    /* fwrite num of races */
    printf("Writing %d races.\n", TotalRaces);
    fwrite(&TotalRaces, sizeof(_INT16), 1, fpData);
    for (iTemp = 0; iTemp < TotalRaces; iTemp++)
        fwrite(Races[iTemp], sizeof(struct PClass), 1, fpData);

    fclose(fpData);

    fpData = fopen("classes.dat", "wb");
    if (!fpData)
        printf("DOH\n");

    /* fwrite num of classes */
    printf("Writing %d classes.\n", TotalClasses);
    fwrite(&TotalClasses, sizeof(_INT16), 1, fpData);
    for (iTemp = 0; iTemp < TotalClasses; iTemp++)
        fwrite(PClasses[iTemp], sizeof(struct PClass), 1, fpData);

    fclose(fpData);

	Deinit_PClasses(PClasses);
	Deinit_PClasses(Races);
	return(0);
}

void Deinit_PClasses( struct PClass *PClass[MAX_PCLASSES])
{
	int iTemp;

	for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++)
	{
		if (PClass[iTemp])
			free(PClass[iTemp]);
	}
}

int Init_PClasses ( struct PClass *PClass[MAX_PCLASSES], char *szFileName )
{
	FILE *fpPClass;
	char szLine[255], *pcCurrentPos/*, szString[255]*/;
	char szToken[MAX_TOKEN_CHARS + 1]/*, *pcAt*/;
	unsigned int uCount;
	int iKeyWord;
	int CurPClass = -1;
	int iTemp, /*OrigPClass,*/ LastSpellSlot=0;

	fpPClass = fopen(szFileName, "r");
	if (!fpPClass)
	{
        printf("Error opening classes file.\n");
		exit(0);
	}

	/* make all classes NULL pointers */
	for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++)
		PClass[iTemp] = NULL;

	for (;;)
	{
		/* read in a line */
		if (fgets(szLine, 255, fpPClass) == NULL) break;

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
		for(iKeyWord = 0; iKeyWord < MAX_PCLASS_WORDS; ++iKeyWord)
		{
			/* If keyword matches */
			if(_STRCASECMP(szToken, papszPClassKeyWords[iKeyWord]) == 0)
			{
				/* Process token */
				switch (iKeyWord)
				{
					case 0 :	/* NAME of class */
						/* see if out of classes memory yet */
						++CurPClass;
						if (CurPClass == MAX_PCLASSES)
						{
							break;
						}

						/* allocate mem for this room */
						PClass[CurPClass] = malloc(sizeof(struct PClass));
                        memset(PClass[CurPClass], 0, sizeof(struct PClass));

						/* initialize it */
						for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
							PClass[CurPClass]->Attributes[iTemp] = 0;

						PClass[CurPClass]->MaxHP = 10;
						PClass[CurPClass]->MaxSP = 10;
						PClass[CurPClass]->Gold = 0;

						strcpy(PClass[CurPClass]->szName, pcCurrentPos);

						/* set known spells to none */
						for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++)
							PClass[CurPClass]->SpellsKnown[iTemp] = 0;
						LastSpellSlot = 0;

						break;
					case 1 :	/* Agility */
					case 2 :	/* Dexterity */
					case 3 :	/* Strength */
					case 4 :	/* Wisdom */
					case 5 :	/* ArmorStr */
					case 6 :	/* Charisma */
						PClass[CurPClass]->Attributes[iKeyWord - 1] = atoi(pcCurrentPos);
						break;
					case 7 :	/* MaxHP */
						PClass[CurPClass]->MaxHP = atoi(pcCurrentPos);
						break;
					case 8 :	/* Gold */
						PClass[CurPClass]->Gold = atoi(pcCurrentPos);
						break;
					case 9 :	/* MaxSP */
						PClass[CurPClass]->MaxSP = atoi(pcCurrentPos);
						break;
					case 10 :	/* spell */
						PClass[CurPClass]->SpellsKnown[LastSpellSlot] = atoi(pcCurrentPos);
						LastSpellSlot++;
						break;
				}
				break;
			}
		}
	}

	/* since they started at -1 and not 0 */
	CurPClass++;

	fclose(fpPClass);

    return (CurPClass);
}
