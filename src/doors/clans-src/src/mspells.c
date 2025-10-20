// MSpells -- make spells

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "defines.h"
#include "k_spells.h"
#include "language.h"
#include "myopen.h"
#include "serialize.h"
#include "structs.h"

static struct Spell *Spells[MAX_SPELLS];

static int TotalSpells;

static void Deinit_Spells(struct Spell *Spells[MAX_SPELLS]);
static void Init_Spells(struct Spell *Spells[MAX_SPELLS], char *szFileName);

int main(int argc, char *argv[])
{
	FILE *fpData;
	int iTemp;
	int StringLength;
	uint16_t tmp16;
	uint8_t sbuf[BUF_SIZE_Spell];

	printf("MSpells FUW/91b1+\n");

	if (argc != 3) {
		printf("Format:  mspells <spells.txt> <spells.dat>\n\n");
		exit(0);
	}

	Init_Spells(Spells, argv[1]);

	fpData = fopen(argv[2], "wb");
	if (!fpData) return(1);

	/* fwrite num of spells */
	printf("-- Writing %d spells.\n\n", TotalSpells);
	tmp16 = SWAP16(TotalSpells);
	fwrite(&tmp16, sizeof(tmp16), 1, fpData);
	for (iTemp = 0; iTemp < TotalSpells; iTemp++) {
		s_Spell_s(Spells[iTemp], sbuf, sizeof(sbuf));
		fwrite(sbuf, sizeof(sbuf), 1, fpData);

		/* fwrite strings */
		if (Spells[iTemp]->pszDamageStr)
			StringLength = strlen(Spells[iTemp]->pszDamageStr) + 1;
		else
			StringLength = 0;

		tmp16 = SWAP16(StringLength);
		fwrite(&tmp16, sizeof(tmp16), 1, fpData);
		if (StringLength)
			fwrite(Spells[iTemp]->pszDamageStr, StringLength, 1, fpData);

		if (Spells[iTemp]->pszHealStr)
			StringLength = strlen(Spells[iTemp]->pszHealStr) + 1;
		else
			StringLength = 0;

		tmp16 = SWAP16(StringLength);
		fwrite(&tmp16, sizeof(tmp16), 1, fpData);
		if (StringLength)
			fwrite(Spells[iTemp]->pszHealStr, StringLength, 1, fpData);

		if (Spells[iTemp]->pszModifyStr)
			StringLength = strlen(Spells[iTemp]->pszModifyStr) + 1;
		else
			StringLength = 0;

		tmp16 = SWAP16(StringLength);
		fwrite(&tmp16, sizeof(tmp16), 1, fpData);
		if (StringLength)
			fwrite(Spells[iTemp]->pszModifyStr, StringLength, 1, fpData);

		if (Spells[iTemp]->pszWearoffStr)
			StringLength = strlen(Spells[iTemp]->pszWearoffStr) + 1;
		else
			StringLength = 0;

		tmp16 = SWAP16(StringLength);
		fwrite(&tmp16, sizeof(tmp16), 1, fpData);
		if (StringLength)
			fwrite(Spells[iTemp]->pszWearoffStr, StringLength, 1, fpData);

		if (Spells[iTemp]->pszStatusStr)
			StringLength = strlen(Spells[iTemp]->pszStatusStr) + 1;
		else
			StringLength = 0;

		tmp16 = SWAP16(StringLength);
		fwrite(&tmp16, sizeof(tmp16), 1, fpData);
		if (StringLength)
			fwrite(Spells[iTemp]->pszStatusStr, StringLength, 1, fpData);

		if (Spells[iTemp]->pszOtherStr)
			StringLength = strlen(Spells[iTemp]->pszOtherStr) + 1;
		else
			StringLength = 0;

		tmp16 = SWAP16(StringLength);
		fwrite(&tmp16, sizeof(tmp16), 1, fpData);
		if (StringLength)
			fwrite(Spells[iTemp]->pszOtherStr, StringLength, 1, fpData);

		if (Spells[iTemp]->pszUndeadName)
			StringLength = strlen(Spells[iTemp]->pszUndeadName) + 1;
		else
			StringLength = 0;

		tmp16 = SWAP16(StringLength);
		fwrite(&tmp16, sizeof(tmp16), 1, fpData);
		if (StringLength)
			fwrite(Spells[iTemp]->pszUndeadName, StringLength, 1, fpData);
	}

	fclose(fpData);

	Deinit_Spells(Spells);
	return(0);
}

static void Deinit_Spells(struct Spell *Spells[MAX_SPELLS])
{
	int iTemp;

	for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++) {
		if (Spells[iTemp]) {
			/* free strings first */
			if (Spells[iTemp]->pszHealStr)
				free(Spells[iTemp]->pszHealStr);
			if (Spells[iTemp]->pszUndeadName)
				free(Spells[iTemp]->pszUndeadName);
			if (Spells[iTemp]->pszDamageStr)
				free(Spells[iTemp]->pszDamageStr);
			if (Spells[iTemp]->pszModifyStr)
				free(Spells[iTemp]->pszModifyStr);
			if (Spells[iTemp]->pszWearoffStr)
				free(Spells[iTemp]->pszWearoffStr);
			if (Spells[iTemp]->pszStatusStr)
				free(Spells[iTemp]->pszStatusStr);
			if (Spells[iTemp]->pszOtherStr)
				free(Spells[iTemp]->pszOtherStr);

			free(Spells[iTemp]);
		}
	}
}

static void Init_Spells(struct Spell *Spells[MAX_SPELLS], char *szFileName)
{
	FILE *fpSpell;
	char szLine[255], *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	size_t uCount;
	int iKeyWord;
	int CurSpell = -1;
	int iTemp;

	fpSpell = fopen(szFileName, "r");
	if (!fpSpell) {
		printf("\aError opening spells file %s.\n", szFileName);
		exit(0);
	}

	/* make all spells NULL pointers */
	for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++)
		Spells[iTemp] = NULL;

	for (;;) {
		/* read in a line */
		if (fgets(szLine, 255, fpSpell) == NULL) break;

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
		for (iKeyWord = 0; iKeyWord < MAX_SPELL_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (stricmp(szToken, papszSpellKeyWords[iKeyWord]) == 0) {
				/* Process token */
				switch (iKeyWord) {
					case 0 :    /* NAME of spell */
						/* see if out of spells memory yet */
						++CurSpell;
						if (CurSpell == MAX_SPELLS) {
							break;
						}

						printf("%s", pcCurrentPos);
						if (CurSpell % 10 == 0 && CurSpell)
							printf("\n");
						else
							printf(", ");

						/* allocate mem for this room */
						Spells[CurSpell] = calloc(1, sizeof(struct Spell));
						memset(Spells[CurSpell], 0, sizeof(struct Spell));

						/* initialize it */
						for (iTemp = 0; iTemp < NUM_ATTRIBUTES; iTemp++)
							Spells[CurSpell]->Attributes[iTemp] = 0;

						Spells[CurSpell]->Level = 0;    /* 0 == doesn't matter */
						Spells[CurSpell]->Value = 0;
						Spells[CurSpell]->Energy = 40;
						Spells[CurSpell]->pszDamageStr = NULL;
						Spells[CurSpell]->pszHealStr = NULL;
						Spells[CurSpell]->pszModifyStr = NULL;
						Spells[CurSpell]->pszWearoffStr = NULL;
						Spells[CurSpell]->pszStatusStr = NULL;
						Spells[CurSpell]->pszOtherStr = NULL;
						Spells[CurSpell]->pszUndeadName = NULL;
						Spells[CurSpell]->SP = 1;
						Spells[CurSpell]->Friendly = false;
						Spells[CurSpell]->Target = true;
						Spells[CurSpell]->StrengthCanReduce = false;
						Spells[CurSpell]->WisdomCanReduce = false;
						Spells[CurSpell]->MultiAffect = false;
						Spells[CurSpell]->Garbage = false;

						strlcpy(Spells[CurSpell]->szName, pcCurrentPos, sizeof(Spells[CurSpell]->szName));
						Spells[CurSpell]->TypeFlag = 0;

						//od_printf("SPELL: %s\n\r", Spells[CurSpell]->szName);
						break;
					case 1 :    /* Agility */
					case 2 :    /* Dexterity */
					case 3 :    /* Strength */
					case 4 :    /* Wisdom */
					case 5 :    /* ArmorStr */
					case 6 :    /* Charisma */
						Spells[CurSpell]->Attributes[iKeyWord-1] = atoi(pcCurrentPos);

						break;
					case 7 :    /* Value */
						Spells[CurSpell]->Value = atoi(pcCurrentPos);

						//printf("    - val: %+d\n", Spells[CurSpell]->Value);
						break;
					case 8 :    /* Flag */
						if (stricmp(pcCurrentPos, "Heal") == 0) {
							//printf("is heal ");
							Spells[CurSpell]->TypeFlag |= SF_HEAL;
						}
						else if (stricmp(pcCurrentPos, "Damage") == 0) {
							//printf("is damage ");
							Spells[CurSpell]->TypeFlag |= SF_DAMAGE;
						}
						else if (stricmp(pcCurrentPos, "Modify") == 0) {
							//printf("is modify ");
							Spells[CurSpell]->TypeFlag |= SF_MODIFY;
						}
						else if (stricmp(pcCurrentPos, "Incapacitate") == 0) {
							//printf("is incap ");
							Spells[CurSpell]->TypeFlag |= SF_INCAPACITATE;
						}
						else if (stricmp(pcCurrentPos, "RaiseUndead") == 0) {
							//printf("is raiseundead ");
							Spells[CurSpell]->TypeFlag |= SF_RAISEUNDEAD;
							printf("flag = %d\n", Spells[CurSpell]->TypeFlag);
						}
						else if (stricmp(pcCurrentPos, "BanishUndead") == 0) {
							//printf("is banish ");
							Spells[CurSpell]->TypeFlag |= SF_BANISHUNDEAD;
						}
						//printf("flag = %d\n", Spells[CurSpell]->TypeFlag);
						break;
					case 9 :    /* HealStr */
						//od_printf("HealStr = %s\n\r", pcCurrentPos);
						Spells[CurSpell]->pszHealStr = strdup(pcCurrentPos);
						break;
					case 10 :    /* DamageStr */
						Spells[CurSpell]->pszDamageStr = strdup(pcCurrentPos);
						break;
					case 11 :    /* ModifyStr */
						Spells[CurSpell]->pszModifyStr = strdup(pcCurrentPos);
						break;
					case 12 :    /* SP */
						Spells[CurSpell]->SP = atoi(pcCurrentPos);
						break;
					case 13 :   /* friendly */
						Spells[CurSpell]->Friendly = true;
						break;
					case 14 :   /* notarget */
						Spells[CurSpell]->Target = false;
						break;
					case 15 :   /* Level */
						Spells[CurSpell]->Level = atoi(pcCurrentPos);
						break;
					case 16 :    /* WearoffStr */
						Spells[CurSpell]->pszWearoffStr = strdup(pcCurrentPos);
						break;
					case 17 :    /* energy */
						Spells[CurSpell]->Energy = atoi(pcCurrentPos);
						break;
					case 18 :    /* StatusStr */
						Spells[CurSpell]->pszStatusStr = strdup(pcCurrentPos);
						break;
					case 19 :    /* OtherStr */
						Spells[CurSpell]->pszOtherStr = strdup(pcCurrentPos);
						break;
					case 20 :    /* UndeadName */
						Spells[CurSpell]->pszUndeadName = strdup(pcCurrentPos);
						break;
					case 21 :   /* strengthcanreduce */
						Spells[CurSpell]->StrengthCanReduce = false;
						break;
					case 22 :   /* wisdomcanreduce */
						Spells[CurSpell]->WisdomCanReduce = false;
						break;
					case 23 :   /* MultiAffect */
						Spells[CurSpell]->MultiAffect = true;
						break;
				}
				break;
			}
		}
	}

	/* since they started at -1 and not 0 */
	CurSpell++;

	//printf("%d spells found.\n%ld bytes used", CurSpell, (long) CurSpell*sizeof(struct Spell));

	fclose(fpSpell);

	TotalSpells = CurSpell;

}
