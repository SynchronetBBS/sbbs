// MClass -- makes classes.dat (and races.dat)

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __MSDOS__
# include <malloc.h>
#endif /* __MSDOS__ */

#include "defines.h"
#include "k_classes.h"
#include "myopen.h"
#include "serialize.h"
#include "structs.h"
#include "unix_wrappers.h"

static struct PClass *PClasses[MAX_PCLASSES], *Races[MAX_PCLASSES];

static int16_t TotalRaces = 0, TotalClasses = 0;

static void Deinit_PClasses(struct PClass *PClass[MAX_PCLASSES]);
static int16_t Init_PClasses(struct PClass *PClass[MAX_PCLASSES], char *szFileName);

int main(void)
{
	FILE *fpData;
	int iTemp;
	int16_t tmp16;
	uint8_t pcbuf[BUF_SIZE_PClass];

	TotalRaces = Init_PClasses(Races, "races.txt");
	TotalClasses = Init_PClasses(PClasses, "classes.txt");

	fpData = fopen("races.dat", "wb");

	/* fwrite num of races */
	printf("Writing %d races.\n", TotalRaces);
	tmp16 = SWAP16(TotalRaces);
	fwrite(&tmp16, sizeof(tmp16), 1, fpData);
	for (iTemp = 0; iTemp < TotalRaces; iTemp++) {
		s_PClass_s(Races[iTemp], pcbuf, sizeof(pcbuf));
		fwrite(pcbuf, sizeof(pcbuf), 1, fpData);
	}

	fclose(fpData);

	fpData = fopen("classes.dat", "wb");
	if (!fpData)
		printf("DOH\n");

	/* fwrite num of classes */
	printf("Writing %d classes.\n", TotalClasses);
	tmp16 = SWAP16(TotalClasses);
	fwrite(&tmp16, sizeof(tmp16), 1, fpData);
	for (iTemp = 0; iTemp < TotalClasses; iTemp++) {
		s_PClass_s(PClasses[iTemp], pcbuf, sizeof(pcbuf));
		fwrite(pcbuf, sizeof(pcbuf), 1, fpData);
	}

	fclose(fpData);

	Deinit_PClasses(PClasses);
	Deinit_PClasses(Races);
	return(0);
}

static void Deinit_PClasses(struct PClass *PClass[MAX_PCLASSES])
{
	int iTemp;

	for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++) {
		if (PClass[iTemp])
			free(PClass[iTemp]);
	}
}

static int16_t Init_PClasses(struct PClass *PClass[MAX_PCLASSES], char *szFileName)
{
	FILE *fpPClass;
	char szLine[255], *pcCurrentPos/*, szString[255]*/;
	char szToken[MAX_TOKEN_CHARS + 1]/*, *pcAt*/;
	unsigned int uCount;
	int iKeyWord;
	int16_t CurPClass = -1;
	int iTemp, /*OrigPClass,*/ LastSpellSlot=0;

	fpPClass = fopen(szFileName, "r");
	if (!fpPClass) {
		printf("Error opening classes file.\n");
		exit(0);
	}

	/* make all classes NULL pointers */
	for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++)
		PClass[iTemp] = NULL;

	for (;;) {
		/* read in a line */
		if (fgets(szLine, 255, fpPClass) == NULL) break;

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
		while (*pcCurrentPos && isspace(*pcCurrentPos))
			++pcCurrentPos;

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos)
			continue;

		/* Get first token from line */
		uCount=0;
		while (*pcCurrentPos && !isspace(*pcCurrentPos)) {
			if (uCount<MAX_TOKEN_CHARS)
				szToken[uCount++]=*pcCurrentPos;
			++pcCurrentPos;
		}
		if (uCount<=MAX_TOKEN_CHARS)
			szToken[uCount]='\0';
		else
			szToken[MAX_TOKEN_CHARS]='\0';

		/* Find beginning of keyword parameter */
		while (*pcCurrentPos && isspace(*pcCurrentPos))
			++pcCurrentPos;

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
		for (iKeyWord = 0; iKeyWord < MAX_PCLASS_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (stricmp(szToken, papszPClassKeyWords[iKeyWord]) == 0) {
				/* Process token */
				switch (iKeyWord) {
					case 0 :    /* NAME of class */
						/* see if out of classes memory yet */
						++CurPClass;
						if (CurPClass == MAX_PCLASSES) {
							break;
						}

						/* allocate mem for this room */
						PClass[CurPClass] = calloc(1, sizeof(struct PClass));
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
					case 1 :    /* Agility */
					case 2 :    /* Dexterity */
					case 3 :    /* Strength */
					case 4 :    /* Wisdom */
					case 5 :    /* ArmorStr */
					case 6 :    /* Charisma */
						PClass[CurPClass]->Attributes[iKeyWord - 1] = atoi(pcCurrentPos);
						break;
					case 7 :    /* MaxHP */
						PClass[CurPClass]->MaxHP = atoi(pcCurrentPos);
						break;
					case 8 :    /* Gold */
						PClass[CurPClass]->Gold = atoi(pcCurrentPos);
						break;
					case 9 :    /* MaxSP */
						PClass[CurPClass]->MaxSP = atoi(pcCurrentPos);
						break;
					case 10 :   /* spell */
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
