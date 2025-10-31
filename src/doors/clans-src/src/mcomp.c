/*

The Clans BBS Door Game
Copyright (C) 1997-2002 Allen Ussher

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

/* Monster Compiler */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "defines.h"
#include "serialize.h"
#include "structs.h"

#define MAX_SPELLS          40
#define MAX_MEMBERS         20
#define MAX_ITEMS_HELD      30
#define MAX_MONSTERS        255
#define MAX_MON_WORDS       14
#define MAX_TOKEN_CHARS     32
#define NUM_ATTRIBUTES      6

static char *papszMonKeyWords[MAX_MON_WORDS] = {
	"Name",
	"HP",
	"Agility",
	"Dexterity",
	"Strength",
	"Wisdom",
	"ArmorStr",
	"Weapon",
	"Shield",
	"Armor",
	"Difficulty",
	"SP",
	"Spell",
	"Undead"
};

static struct pc TmpMonster;

int main(int argc, char *argv[])
{
	FILE *fpMonIn, *fpMonOut;
	char szLine[255], *pcCurrentPos;
//  char szString[255];
	char szToken[MAX_TOKEN_CHARS + 1];
//  char *pcAt;
	uint16_t uCount;
	int16_t iKeyWord;
	int16_t iTemp;
//  int16_t OrigMonIn;
	int16_t MonIndex[MAX_MONSTERS];  /* difficulties of all monsters,
                                    0 means no monster */
	int16_t CurMonster = -1, LastSpellSlot = 0;
	uint8_t mBuf[BUF_SIZE_pc];

	if (argc != 3) {
		printf("Format:  mcomp <monster.txt> <output.mon>\n\n");
		exit(0);
	}

	fpMonIn = fopen(argv[1], "r");
	if (!fpMonIn) {
		printf("Error opening %s.\n", argv[1]);
		exit(0);
	}

	fpMonOut = fopen(argv[2], "w+b");
	if (!fpMonOut) {
		printf("Error opening %s.\n", argv[2]);
		exit(0);
	}

	/* generate index while doing list */

	/* for now, write a blank index */
	for (iTemp = 0; iTemp < MAX_MONSTERS; iTemp++)
		MonIndex[iTemp] = 0;

	fwrite(MonIndex, sizeof(MonIndex), 1, fpMonOut);

	for (;;) {
		/* read in a line */
		if (fgets(szLine, 255, fpMonIn) == NULL) break;

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
		for (iKeyWord = 0; iKeyWord < MAX_MON_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (strcasecmp(szToken, papszMonKeyWords[iKeyWord]) == 0) {
				/* Process token */
				switch (iKeyWord) {
					case 0 :    /* NAME of monster */
						/* write previous monster to file then */
						if (CurMonster != -1) {
							s_pc_s(&TmpMonster, mBuf, sizeof(mBuf));
							fwrite(mBuf, sizeof(mBuf), 1, fpMonOut);
						}

						/* see if out of items memory yet */
						++CurMonster;
						if (CurMonster == MAX_MONSTERS) {
							break;
						}

						/* initialize it */
						memset(&TmpMonster, 0, sizeof(struct pc));
						strlcpy(TmpMonster.szName, pcCurrentPos, sizeof(TmpMonster.szName));
						TmpMonster.HP = TmpMonster.MaxHP = 30;
						TmpMonster.SP = TmpMonster.MaxSP = 0;

						for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
							TmpMonster.Attributes[iTemp] = 10;

						TmpMonster.Status = Here;

						TmpMonster.Weapon = 0;
						TmpMonster.Shield = 0;
						TmpMonster.Armor = 0;

						TmpMonster.WhichRace = -1;
						TmpMonster.WhichClass = -1;

						TmpMonster.Difficulty = 0;
						TmpMonster.Level = 0;
						TmpMonster.Undead = false;

						for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++)
							TmpMonster.SpellsKnown[iTemp] = 0;
						for (iTemp = 0; iTemp < 10; iTemp++)
							TmpMonster.SpellsInEffect[iTemp].SpellNum = -1;
						LastSpellSlot = 0;

						printf("Monster : %s\n", TmpMonster.szName);
						break;
					case 1 :    /* HP */
						printf("    - hp  : %d\n", atoi(pcCurrentPos));
						TmpMonster.HP = TmpMonster.MaxHP = atoi(pcCurrentPos);
						break;
					case 2 :    /* agility */
					case 3 :    /* Dexterity */
					case 4 :    /* Strength */
					case 5 :    /* Wisdom */
					case 6 :    /* ArmorStr */
						TmpMonster.Attributes[iKeyWord - 2] = atoi(pcCurrentPos);
						break;
					case 7 :    /* Weapon */
						TmpMonster.Weapon = atoi(pcCurrentPos);

						printf("    - wep: %d\n", TmpMonster.Weapon);
						break;
					case 8 :    /* Shield */
						TmpMonster.Shield = atoi(pcCurrentPos);

						printf("    - shi: %d\n", TmpMonster.Shield);
						break;
					case 9 :    /* Armor */
						TmpMonster.Armor = atoi(pcCurrentPos);

						printf("    - amr: %d\n", TmpMonster.Armor);
						break;
					case 10 :    /* difficulty */
						TmpMonster.Difficulty =
							MonIndex[CurMonster] = atoi(pcCurrentPos);

						TmpMonster.Level = TmpMonster.Difficulty;

						printf("    - dif: %d\n", MonIndex[CurMonster]);
						MonIndex[CurMonster] = SWAP16(MonIndex[CurMonster]);
						break;
					case 11 :    /* SP */
						printf("    - sp  : %d\n", atoi(pcCurrentPos));
						TmpMonster.SP = TmpMonster.MaxSP = atoi(pcCurrentPos);
						break;
					case 12 :   /* spell! */
						TmpMonster.SpellsKnown[LastSpellSlot] = atoi(pcCurrentPos);
						LastSpellSlot++;
						break;
					case 13 :   // undead
						TmpMonster.Undead = true;
						break;


				}
				break;
			}
		}
	}

	/* write last monster */
	s_pc_s(&TmpMonster, mBuf, sizeof(mBuf));
	fwrite(mBuf, sizeof(mBuf), 1, fpMonOut);

	/* rewrite index */
	fseek(fpMonOut, 0L, SEEK_SET);
	fwrite(MonIndex, sizeof(MonIndex), 1, fpMonOut);

	/* since they started at -1 and not 0 */
	CurMonster++;

	printf("%d Monster found.\n%ld bytes used", CurMonster, (long)CurMonster * BUF_SIZE_pc);

	fclose(fpMonIn);
	fclose(fpMonOut);
	return(0);
}
