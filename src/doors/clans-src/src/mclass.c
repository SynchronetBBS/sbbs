// MClass -- makes classes.dat (and races.dat)

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "platform.h"

#include "console.h"
#include "defines.h"
#include "k_classes.h"
#include "myopen.h"
#include "serialize.h"
#include "structs.h"

static struct PClass *PClasses[MAX_PCLASSES];

static int16_t Total = 0;

static void Deinit_PClasses();
static int16_t Init_PClasses(char *szFileName);

int main(int argc, char **argv)
{
	FILE *fpData;
	int iTemp;
	int16_t tmp16;
	uint8_t pcbuf[BUF_SIZE_PClass];

	if (argc != 3) {
		printf("usage:\nmclass class.txt class.cls\n");
		exit(1);
	}

	Total = Init_PClasses(argv[1]);

	fpData = fopen(argv[2], "wb");
	if (!fpData) {
		printf("Error opening class output file");
		exit(1);
	}

	/* fwrite num of races */
	printf("Writing %d classes.\n", Total);
	tmp16 = SWAP16S(Total);
	fwrite(&tmp16, sizeof(tmp16), 1, fpData);
	for (iTemp = 0; iTemp < Total; iTemp++) {
		s_PClass_s(PClasses[iTemp], pcbuf, sizeof(pcbuf));
		fwrite(pcbuf, sizeof(pcbuf), 1, fpData);
	}

	fclose(fpData);

	Deinit_PClasses();
	return(0);
}

static void Deinit_PClasses()
{
	int iTemp;

	if (PClasses == NULL)
		return;
	for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++) {
		if (PClasses[iTemp])
			free(PClasses[iTemp]);
	}
}

static int16_t Init_PClasses(char *szFileName)
{
	FILE *fpPClass;
	char szLine[255], *pcCurrentPos/*, szString[255]*/;
	char szToken[MAX_TOKEN_CHARS + 1]/*, *pcAt*/;
	size_t uCount;
	int iKeyWord;
	int16_t CurPClass = -1;
	int iTemp, /*OrigPClass,*/ LastSpellSlot=0;

	fpPClass = fopen(szFileName, "r");
	if (!fpPClass) {
		printf("Error opening classes file.\n");
		exit(1);
	}

	/* make all classes NULL pointers */
	for (iTemp = 0; iTemp < MAX_PCLASSES; iTemp++)
		PClasses[iTemp] = NULL;

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
			if (strcasecmp(szToken, papszPClassKeyWords[iKeyWord]) == 0) {
				/* Process token */
				switch (iKeyWord) {
					case 0 :    /* NAME of class */
						/* see if out of classes memory yet */
						++CurPClass;
						if (CurPClass == MAX_PCLASSES) {
							break;
						}

						/* allocate mem for this room */
						PClasses[CurPClass] = calloc(1, sizeof(struct PClass));
						CheckMem(PClasses[CurPClass]);

						PClasses[CurPClass]->MaxHP = 10;
						PClasses[CurPClass]->MaxSP = 10;
						PClasses[CurPClass]->Gold = 0;

						strlcpy(PClasses[CurPClass]->szName, pcCurrentPos, sizeof(PClasses[CurPClass]->szName));

						/* set known spells to none */
						for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++)
							PClasses[CurPClass]->SpellsKnown[iTemp] = 0;
						LastSpellSlot = 0;

						break;
					case 1 :    /* Agility */
					case 2 :    /* Dexterity */
					case 3 :    /* Strength */
					case 4 :    /* Wisdom */
					case 5 :    /* ArmorStr */
					case 6 :    /* Charisma */
						iTemp = atoi(pcCurrentPos);
						if (iTemp < CHAR_MIN || iTemp > CHAR_MAX) {
							printf("Invalid ability score %d\n", iTemp);
							exit(EXIT_FAILURE);
						}
						PClasses[CurPClass]->Attributes[iKeyWord - 1] = (char)iTemp;
						break;
					case 7 :    /* MaxHP */
						iTemp = atoi(pcCurrentPos);
						if (iTemp < INT16_MIN || iTemp > INT16_MAX) {
							printf("Invalid MaxHP %d\n", iTemp);
							exit(EXIT_FAILURE);
						}
						PClasses[CurPClass]->MaxHP = (int16_t)iTemp;
						break;
					case 8 :    /* Gold */
						iTemp = atoi(pcCurrentPos);
						if (iTemp < INT16_MIN || iTemp > INT16_MAX) {
							printf("Invalid Gold %d\n", iTemp);
							exit(EXIT_FAILURE);
						}
						PClasses[CurPClass]->Gold = (int16_t)iTemp;
						break;
					case 9 :    /* MaxSP */
						iTemp = atoi(pcCurrentPos);
						if (iTemp < INT16_MIN || iTemp > INT16_MAX) {
							printf("Invalid MaxSP %d\n", iTemp);
							exit(EXIT_FAILURE);
						}
						PClasses[CurPClass]->MaxSP = (int16_t)iTemp;
						break;
					case 10 :   /* spell */
						iTemp = atoi(pcCurrentPos);
						if (iTemp < CHAR_MIN || iTemp > CHAR_MAX) {
							printf("Invalid ability score %d\n", iTemp);
							exit(EXIT_FAILURE);
						}
						PClasses[CurPClass]->SpellsKnown[LastSpellSlot] = (char)iTemp;
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
