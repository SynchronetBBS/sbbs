// MSpells -- make spells

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#ifdef __MSDOS__
#include <malloc.h>
#endif /* __MSDOS__ */

/* defined by 'defines.h' included by 'structs.h'
typedef char               BOOL;           // Boolean value, at least 1 bit. */
// #include "cinclude.h"
#include "structs.h"
#include "keywords.h"

/* defined in 'defines.h'
#define TRUE    1
#define FALSE   0
*/

struct Spell *Spells[MAX_SPELLS];

int TotalSpells;

void Deinit_Spells( struct Spell *Spells[MAX_SPELLS]);
void Init_Spells ( struct Spell *Spells[MAX_SPELLS], char *szFileName );

int main ( int argc, char *argv[] )
{
    FILE *fpData;
    int iTemp;
    int StringLength;

    printf("MSpells FUW/91b1+\n");

    if (argc != 3)
    {
        printf("Format:  mspells <spells.txt> <spells.dat>\n\n");
        exit(0);
    }

    Init_Spells ( Spells, argv[1]);

    fpData = fopen(argv[2], "wb");
    if (!fpData) return(1);

    /* fwrite num of spells */
    printf("-- Writing %d spells.\n\n", TotalSpells);
    fwrite(&TotalSpells, sizeof(_INT16), 1, fpData);
    for (iTemp = 0; iTemp < TotalSpells; iTemp++)
    {
        fwrite(Spells[iTemp], sizeof(struct Spell), 1, fpData);

        /* fwrite strings */
        if (Spells[iTemp]->pszDamageStr)
            StringLength = strlen(Spells[iTemp]->pszDamageStr) + 1;
        else
            StringLength = 0;

        fwrite(&StringLength, sizeof(_INT16), 1, fpData);
        if (StringLength)
            fwrite(Spells[iTemp]->pszDamageStr, StringLength, 1, fpData);

        if (Spells[iTemp]->pszHealStr)
            StringLength = strlen(Spells[iTemp]->pszHealStr) + 1;
        else
            StringLength = 0;

        fwrite(&StringLength, sizeof(_INT16), 1, fpData);
        if (StringLength)
            fwrite(Spells[iTemp]->pszHealStr, StringLength, 1, fpData);

        if (Spells[iTemp]->pszModifyStr)
            StringLength = strlen(Spells[iTemp]->pszModifyStr) + 1;
        else
            StringLength = 0;

        fwrite(&StringLength, sizeof(_INT16), 1, fpData);
        if (StringLength)
            fwrite(Spells[iTemp]->pszModifyStr, StringLength, 1, fpData);

        if (Spells[iTemp]->pszWearoffStr)
            StringLength = strlen(Spells[iTemp]->pszWearoffStr) + 1;
        else
            StringLength = 0;

        fwrite(&StringLength, sizeof(_INT16), 1, fpData);
        if (StringLength)
            fwrite(Spells[iTemp]->pszWearoffStr, StringLength, 1, fpData);

        if (Spells[iTemp]->pszStatusStr)
            StringLength = strlen(Spells[iTemp]->pszStatusStr) + 1;
        else
            StringLength = 0;

        fwrite(&StringLength, sizeof(_INT16), 1, fpData);
        if (StringLength)
            fwrite(Spells[iTemp]->pszStatusStr, StringLength, 1, fpData);

        if (Spells[iTemp]->pszOtherStr)
            StringLength = strlen(Spells[iTemp]->pszOtherStr) + 1;
        else
            StringLength = 0;

        fwrite(&StringLength, sizeof(_INT16), 1, fpData);
        if (StringLength)
            fwrite(Spells[iTemp]->pszOtherStr, StringLength, 1, fpData);

        if (Spells[iTemp]->pszUndeadName)
            StringLength = strlen(Spells[iTemp]->pszUndeadName) + 1;
        else
            StringLength = 0;

        fwrite(&StringLength, sizeof(_INT16), 1, fpData);
        if (StringLength)
            fwrite(Spells[iTemp]->pszUndeadName, StringLength, 1, fpData);
    }

    fclose(fpData);

    Deinit_Spells(Spells);
	return(0);
}

void Deinit_Spells( struct Spell *Spells[MAX_SPELLS])
{
    int iTemp;

    for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++)
    {
        if (Spells[iTemp])
        {
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

void Init_Spells ( struct Spell *Spells[MAX_SPELLS], char *szFileName )
{
    FILE *fpSpell;
	char szLine[255], *pcCurrentPos;
	char szToken[MAX_TOKEN_CHARS + 1];
	unsigned int uCount;
	int iKeyWord;
    int CurSpell = -1;
    int iTemp;

    fpSpell = fopen(szFileName, "r");
    if (!fpSpell)
	{
        printf("\aError opening spells file %s.\n", szFileName);
		exit(0);
	}

    /* make all spells NULL pointers */
    for (iTemp = 0; iTemp < MAX_SPELLS; iTemp++)
        Spells[iTemp] = NULL;

	for (;;)
	{
		/* read in a line */
        if (fgets(szLine, 255, fpSpell) == NULL) break;

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
        for(iKeyWord = 0; iKeyWord < MAX_SPELL_WORDS; ++iKeyWord)
		{
			/* If keyword matches */
            if(_STRCASECMP(szToken, papszSpellKeyWords[iKeyWord]) == 0)
			{
				/* Process token */
				switch (iKeyWord)
				{
                    case 0 :    /* NAME of spell */
                        /* see if out of spells memory yet */
                        ++CurSpell;
                        if (CurSpell == MAX_SPELLS)
						{
							break;
						}

                        printf("%s", pcCurrentPos);
                        if (CurSpell % 10 == 0 && CurSpell)
                            printf("\n");
                        else
                            printf(", ");

						/* allocate mem for this room */
                        Spells[CurSpell] = malloc(sizeof(struct Spell));
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
                        Spells[CurSpell]->Friendly = FALSE;
                        Spells[CurSpell]->Target = TRUE;
                        Spells[CurSpell]->StrengthCanReduce = FALSE;
                        Spells[CurSpell]->WisdomCanReduce = FALSE;
                        Spells[CurSpell]->MultiAffect = FALSE;
                        Spells[CurSpell]->Garbage = FALSE;

                        strcpy(Spells[CurSpell]->szName, pcCurrentPos);
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
                        if (_STRCASECMP(pcCurrentPos, "Heal") == 0)
                        {
                            //printf("is heal ");
                            Spells[CurSpell]->TypeFlag |= SF_HEAL;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "Damage") == 0)
                        {
                            //printf("is damage ");
                            Spells[CurSpell]->TypeFlag |= SF_DAMAGE;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "Modify") == 0)
                        {
                            //printf("is modify ");
                            Spells[CurSpell]->TypeFlag |= SF_MODIFY;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "Incapacitate") == 0)
                        {
                            //printf("is incap ");
                            Spells[CurSpell]->TypeFlag |= SF_INCAPACITATE;
                        }
                        else if (_STRCASECMP(pcCurrentPos, "RaiseUndead") == 0)
                        {
                            //printf("is raiseundead ");
                            Spells[CurSpell]->TypeFlag |= SF_RAISEUNDEAD;
                            printf("flag = %d\n", Spells[CurSpell]->TypeFlag);
                        }
                        else if (_STRCASECMP(pcCurrentPos, "BanishUndead") == 0)
                        {
                            //printf("is banish ");
                            Spells[CurSpell]->TypeFlag |= SF_BANISHUNDEAD;
                        }
                        //printf("flag = %d\n", Spells[CurSpell]->TypeFlag);
						break;
                    case 9 :    /* HealStr */
                        //od_printf("HealStr = %s\n\r", pcCurrentPos);
                        Spells[CurSpell]->pszHealStr = malloc( strlen(pcCurrentPos) + 1 );
                        strcpy(Spells[CurSpell]->pszHealStr, pcCurrentPos);
						break;
                    case 10 :    /* DamageStr */
                        Spells[CurSpell]->pszDamageStr = malloc( strlen(pcCurrentPos) + 1 );
                        strcpy(Spells[CurSpell]->pszDamageStr, pcCurrentPos);
						break;
                    case 11 :    /* ModifyStr */
                        Spells[CurSpell]->pszModifyStr = malloc( strlen(pcCurrentPos) + 1 );
                        strcpy(Spells[CurSpell]->pszModifyStr, pcCurrentPos);
						break;
                    case 12 :    /* SP */
                        Spells[CurSpell]->SP = atoi(pcCurrentPos);
						break;
                    case 13 :   /* friendly */
                        Spells[CurSpell]->Friendly = TRUE;
                        break;
                    case 14 :   /* notarget */
                        Spells[CurSpell]->Target = FALSE;
                        break;
                    case 15 :   /* Level */
                        Spells[CurSpell]->Level = atoi(pcCurrentPos);
                        break;
                    case 16 :    /* WearoffStr */
                        Spells[CurSpell]->pszWearoffStr = malloc( strlen(pcCurrentPos) + 1 );
                        strcpy(Spells[CurSpell]->pszWearoffStr, pcCurrentPos);
						break;
                    case 17 :    /* energy */
                        Spells[CurSpell]->Energy = atoi(pcCurrentPos);
						break;
                    case 18 :    /* StatusStr */
                        Spells[CurSpell]->pszStatusStr = malloc( strlen(pcCurrentPos) + 1 );
                        strcpy(Spells[CurSpell]->pszStatusStr, pcCurrentPos);
						break;
                    case 19 :    /* OtherStr */
                        Spells[CurSpell]->pszOtherStr = malloc( strlen(pcCurrentPos) + 1 );
                        strcpy(Spells[CurSpell]->pszOtherStr, pcCurrentPos);
						break;
                    case 20 :    /* UndeadName */
                        Spells[CurSpell]->pszUndeadName = malloc( strlen(pcCurrentPos) + 1 );
                        strcpy(Spells[CurSpell]->pszUndeadName, pcCurrentPos);
						break;
                    case 21 :   /* strengthcanreduce */
                        Spells[CurSpell]->StrengthCanReduce = FALSE;
                        break;
                    case 22 :   /* wisdomcanreduce */
                        Spells[CurSpell]->WisdomCanReduce = FALSE;
                        break;
                    case 23 :   /* MultiAffect */
                        Spells[CurSpell]->MultiAffect = TRUE;
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

