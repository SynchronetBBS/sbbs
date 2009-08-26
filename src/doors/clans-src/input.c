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

/*
 * Input ADT
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifndef __FreeBSD__
#include <malloc.h>
#endif
#ifdef __unix__
#include "unix_wrappers.h"
#endif


#include "structs.h"
#include <OpenDoor.h>
#include "language.h"
#include "mstrings.h"
#include "myopen.h"
#include "door.h"
#include "help.h"

extern struct Language *Language;

// ------------------------------------------------------------------------- //

_INT16 Similar(char *string, char *word)
/*
 * Returns TRUE if the first N chars of string match the first N chars of
 * word.  N being the strlen of word.
 */
{
	_INT16 NumLetters = strlen(word);
	_INT16 CurLetter;

	for (CurLetter = 0; CurLetter < NumLetters; CurLetter++) {
		if (string[CurLetter] == 0)
			break;

		if (toupper(string[CurLetter]) != toupper(word[CurLetter]))
			break;
	}

	if (CurLetter != NumLetters)
		return FALSE;
	else
		return TRUE;
}


_INT16 InsideStr(char *SubString, char *FullString, _INT16 AtStart)
/*
 * Returns TRUE if SubString appears within FullString.
 *
 * PRE: AtStart is TRUE if you only want to check the start of the string.
 */
{
	_INT16 NumLetters = strlen(FullString);
	_INT16 CurLetter = 0;

	if (*SubString == 0)
		return FALSE;

	if (AtStart == FALSE) {
		/* if AtStart set, only checks start of string for match */
		for (CurLetter = 0; CurLetter < NumLetters; CurLetter++) {
			//od_printf("Comparing letter #%d:  ", CurLetter);
			if (Similar(&FullString[CurLetter], SubString))
				return TRUE;
		}
	}
	else {
		if (Similar(&FullString[CurLetter], SubString))
			return TRUE;
	}

	//rputs("Returning FALSE.");

	return FALSE;
}


void ListChoices(char **apszChoices, _INT16 NumChoices, _INT16 DisplayType)
/*
 * This lists the choices used by GetStringChoice.
 *
 * PRE: DisplayType determines how the choices are listed, wide or long.
 */
{
	_INT16 iTemp;
	char szString[128];

	if (DisplayType == DT_WIDE) {
		rputs(ST_HELPLINE);

		for (iTemp = 0; iTemp < NumChoices; iTemp++) {
			if (iTemp%3 == 0)
				rputs(" ");

			sprintf(szString, "%-25s ", apszChoices[iTemp]);
			rputs(szString);

			if ((iTemp+1)%3 == 0)
				rputs("\n");
		}

		if (iTemp%3 != 0)
			rputs("\n");

		/* put footer */
		rputs(ST_HELPLINE);
	}
	else if (DisplayType == DT_LONG) {
		rputs(ST_LONGLINE);
		rputs("|07");

		for (iTemp = 0; iTemp < NumChoices; iTemp++) {
			sprintf(szString, " %-s\n", apszChoices[iTemp]);
			rputs(szString);
		}

		/* put footer */
		rputs(ST_LONGLINE);
	}
}


void GetStringChoice(char **apszChoices, _INT16 NumChoices, char *szPrompt,
					 _INT16 *UserChoice, BOOL ShowChoicesInitially, _INT16 DisplayType, BOOL AllowBlank)
/*
 * This will choose a string from a listing and return which was chosen in
 * UserChoice.
 *
 * PRE:   apszChoices is an array of the choices (strings)
 *        NumChoices contains how many total choices (strings) there are
 *        szPrompt contains the string to prompt user with.
 *        ShowChoicesInitially = TRUE if you wish to list the choices
 *          when this is called.
 *        DisplayType determines how the choices are listed.
 *        AllowBlank = TRUE if you wish to allow the user to choose nothing
 *          (i.e. press enter) or = FALSE if you want the user to choose
 *          one of the options.
 */
{
	char szUserInput[40], Key, *szString;
	BOOL ShowedTopic, WantsHelp, BackSpace, Inside;
	_INT16 iTemp, LastTopicFound = -1, CurChar = 0, TimesInStr = 0, TopicFound = 0;

	/* init stuff */
	szUserInput[0] = 0;

	ShowedTopic = FALSE;
	WantsHelp = FALSE;

	/* show all topics */

	szString = MakeStr(128);

	if (ShowChoicesInitially) {
		/* show all choices */
		ListChoices(apszChoices, NumChoices, DisplayType);
	}

	for (;;) {
		/* enter topic or ? etc... */
		rputs(szPrompt);

		/* user enters words and stuff */
		for (;;) {
			/* enter a char */
			/* and update szString */
			od_sleep(0);
			Key = od_get_key(TRUE);

			if (Key == '?' && CurChar == 0) {
				rputs(ST_DISPLAYTOPICS);
				WantsHelp = TRUE;
				break;
			}

			/* if user presses [Enters] exit loop */
			if (Key == '\r' || Key == '\n') {
				rputs("\n");
				break;
			}
			else if (Key == '\x19') {
				/* ctrl-Y */
				if (ShowedTopic && LastTopicFound != -1)
					for (iTemp = 0; iTemp < (signed)strlen(apszChoices[LastTopicFound]); iTemp++)
						rputs("\b \b");
				else
					for (iTemp = 0; iTemp < (signed)strlen(szUserInput); iTemp++)
						rputs("\b \b");

				szUserInput[0] = 0;
				CurChar = 0;
				ShowedTopic = FALSE;

				continue;
			}

			/* if backspace */
			else if (Key == '\b' || Key == 127) {
				BackSpace = TRUE;
				if (CurChar != 0) {
					if (!ShowedTopic)
						rputs("\b \b");

					CurChar--;
					szUserInput[CurChar] = 0;
				}
			}

			/* if already 29 letters, stop */
			else if (CurChar == 30)
				continue;
			else if (iscntrl(Key))
				continue;

			/* else, make next letter equal to key */
			else {
				BackSpace = FALSE;
				szUserInput[CurChar++] = Key;
				szUserInput[CurChar] = 0;
			}

			/* Display the szString as is IF at least 2 szStrings have the subszString */
			TimesInStr = 0;
			for (iTemp = 0; iTemp < NumChoices; iTemp++) {
				if (stricmp(szUserInput, apszChoices[iTemp]) == 0) {
					TopicFound = iTemp;
					TimesInStr = 1;
					break;
				}

				Inside = InsideStr(szUserInput, apszChoices[iTemp], FALSE);

				if (Inside) {
					TopicFound = iTemp;
					TimesInStr++;
				}

			}

			if (TimesInStr == 1)
				rputs(ST_COLOR14);
			else
				rputs(ST_COLOR07);

			if (ShowedTopic && TimesInStr == 1 && LastTopicFound == TopicFound) {
				/* same topic, do nothing */
			}
			/* if typing something does nothing, still show topic word, so
			   no change */
			else if (ShowedTopic && TimesInStr == 1 && LastTopicFound != TopicFound) {
				/* show new one */
				for (iTemp = 0; iTemp < (signed)strlen(apszChoices[LastTopicFound]); iTemp++) {
					rputs("\b \b");
				}
				rputs(apszChoices[TopicFound]);
				ShowedTopic = TRUE;
			}
			/* if typing changes from showtopic to user topic, show user topic */
			else if (ShowedTopic && TimesInStr != 1) {
				for (iTemp = 0; iTemp < (signed)strlen(apszChoices[LastTopicFound]); iTemp++) {
					rputs("\b \b");
				}
				rputs(szUserInput);
				ShowedTopic = FALSE;
			}
			/* if typing changes from user topic to showtopic, show topic */
			else if (ShowedTopic == FALSE && TimesInStr == 1) {
				/* erase old szString */
				if (!BackSpace)
					for (iTemp = 0; iTemp < CurChar-1; iTemp++) {
						rputs("\b \b");
					}
				else if (BackSpace)
					for (iTemp = 0; iTemp < CurChar; iTemp++) {
						rputs("\b \b");
					}

				/* show new one */
				rputs(apszChoices[TopicFound]);
				ShowedTopic = TRUE;
			}
			/* else show char */
			else if (!BackSpace)
				od_putch(szUserInput[CurChar-1]);

			LastTopicFound = TopicFound;
		}

		if (WantsHelp) {
			ListChoices(apszChoices, NumChoices, DisplayType);

			WantsHelp = FALSE;
			szUserInput[0] = 0;
			CurChar = 0;
			ShowedTopic = FALSE;

			continue;
		}

		// user quit
		if (szUserInput[0] == 0 && AllowBlank)
			break;

		if (TimesInStr == 1) {
			rputs("\n");
			*UserChoice = TopicFound;
			free(szString);
			return;
		}
		else
			rputs(ST_INVALIDTOPIC);

		szUserInput[0] = 0;
		CurChar = 0;
		ShowedTopic = FALSE;
	}

	free(szString);

	// none found
	*UserChoice = -1;
}

// ------------------------------------------------------------------------- //

void GetStr(char *InputStr, _INT16 MaxChars, BOOL HiBit)
/*
 * This function allows the user to input a string of MaxChars length and
 * place it in InputStr.
 *
 * PRE: InputStr MAY contain a string which contains a value, HiBit
 *      can be used to toggle whether the user can enter hibit ascii.
 */
{
	_INT16 CurChar;
	unsigned char InputCh;
	char Spaces[85] = "                                                                                     ";
	char BackSpaces[85] = "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
	char TempStr[190];

	Spaces[MaxChars] = 0;
	BackSpaces[MaxChars] = 0;

	CurChar = strlen(InputStr);

	rputs(Spaces);
	rputs(BackSpaces);
	od_disp_str(InputStr);

	for (;;) {
		od_sleep(0);
		InputCh = od_get_key(TRUE);

		if (InputCh == '\b' || InputCh == 127) {
			if (CurChar>0) {
				CurChar--;
				rputs("\b \b");
			}
		}
		else if (InputCh == '\r' || InputCh == '\n') {
			rputs("|16\n");
			InputStr[CurChar]=0;
			break;
		}
		else if (InputCh== '' || InputCh == '\x1B') { // ctrl-y
			InputStr [0] = 0;
			strcpy(TempStr, BackSpaces);
			TempStr[ CurChar ] = 0;
			rputs(TempStr);
			Spaces[MaxChars] = 0;
			BackSpaces[MaxChars] = 0;
			rputs(Spaces);
			rputs(BackSpaces);
			CurChar = 0;
		}
		else if (InputCh >= '' && HiBit == FALSE)
			continue;
		else if (InputCh == 0)
			continue;
		//      else if (iscntrl(InputCh) && InputCh < 30 || HiBit == FALSE)
		else if (iscntrl(InputCh) && InputCh < 30)
			continue;
		else if (isalpha(InputCh) && CurChar && InputStr[CurChar-1] == SPECIAL_CODE)
			continue;
		else { /* valid character input */
			if (CurChar==MaxChars)   continue;
			InputStr[CurChar++]=InputCh;
			InputStr[CurChar] = 0;
			od_putch(InputCh);
		}
	}
}

// ------------------------------------------------------------------------- //

_INT16 GetChoice(char *DisplayFile, char *Prompt, char *Options[], char *Keys, char DefChar, BOOL ShowTime)
/*
 * This function allows the user to choose an option from the options
 * listed.  Works much like FE's input system.
 *
 * PRE: DisplayFile contains the name of file to display.
 *      Prompt contains the prompt user will see.
 *      Options contains the array of options.
 *      Keys contains the corresponding keys to input to choose options.
 *      DefChar MUST contain the default character the user will choose if he
 *        presses Enter.
 *      ShowTime is set to TRUE if the time is to be displayed in prompt.
 */
{
	_INT16 cTemp, DefChoice;
	char KeysAndEnter[50], Choice, Spaces[] = "                      \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
	char TimeStr[40];
	_INT16 HoursLeft, MinutesLeft;

	HoursLeft = (od_control.user_timelimit - od_control.user_time_used)/60;
	MinutesLeft = (od_control.user_timelimit - od_control.user_time_used)%60;

	if (MinutesLeft < 0)
		MinutesLeft = 0;

	sprintf(TimeStr, " |0H[|0I%02d:%02d|0H] ", HoursLeft, MinutesLeft);

	/* figure out default char */
	for (cTemp = 0; cTemp < (signed)strlen(Keys); cTemp++) {
		if (DefChar == Keys[cTemp])
			break;
	}
	DefChoice = cTemp;

	/* KeysAndEnter is just Keys[] + "\r" */
	strcpy(KeysAndEnter, Keys);
	strcat(KeysAndEnter, "\r\n ");

	cTemp = strlen(KeysAndEnter);

	/* now get input */

	/* Display file */
	if (DisplayFile[0]) {
		if (strchr(DisplayFile, '.'))
			Display(DisplayFile);
		else
			Help(DisplayFile, ST_MENUSHLP);
	}

	/* time */
	if (ShowTime)
		rputs(TimeStr);

	/* Display prompt, and default option */
	rputs(Prompt);
	rputs(Options[DefChoice]);

	/* make sure it's not the special key! */
	do
		Choice = od_get_answer(KeysAndEnter);
	while (Choice == '\xFF');

	if (Choice == '\r' || Choice == DefChar || Choice == ' ' || Choice == '\n') {
		cTemp = DefChoice;
	}
	else
		for (cTemp = 0; cTemp < (signed)strlen(Keys); cTemp++) {
			if (Choice == Keys[cTemp])
				break;
		}

	/* display choice */
	rputs("\r");

	HoursLeft = (od_control.user_timelimit - od_control.user_time_used)/60;
	MinutesLeft = (od_control.user_timelimit - od_control.user_time_used)%60;
	sprintf(TimeStr, " |0H[|0J%02d:%02d|0H] ", HoursLeft, MinutesLeft);

	if (ShowTime)
		rputs(TimeStr);

	rputs(Prompt);
	rputs(Spaces);
	rputs("|15");
	rputs(Options[cTemp]);
	rputs("\n\n");

	return (Keys[cTemp]);

}


// ------------------------------------------------------------------------- //

long GetLong(char *Prompt, long DefaultVal, long Maximum)
/*
 * This allows the user to input a long integer.
 *
 */
{
	char string[155], NumString[13], InputChar, DefMax[40];
	_INT16 NumDigits, CurDigit = 0, cTemp;
	long TenPower;

	/* init screen */
	rputs(Prompt);

	sprintf(DefMax, " |08(|15%ld|07; %ld|08) |15", DefaultVal, Maximum);
	rputs(DefMax);

	/* NumDigits contains amount of digits allowed using max. value input */

	TenPower = 10;
	for (NumDigits = 1; NumDigits < 11; NumDigits++) {
		if (labs(Maximum) < TenPower)
			break;

		TenPower *= 10;
	}
	if (Maximum < 0L)
		NumDigits++;

	/* now get input */
	for (;;) {
		InputChar = od_get_answer("0123456789><.,\r\n\b\x19\x7f");

		if (isdigit(InputChar)) {
			if (CurDigit < NumDigits) {
				NumString[CurDigit++] = InputChar;
				NumString[CurDigit] = 0;
				od_putch(InputChar);
			}

		}
		else if (InputChar == '\b' || InputChar == 127) {
			if (CurDigit > 0) {
				CurDigit--;
				NumString[CurDigit] = 0;
				rputs("\b \b");
			}
		}
		else if (InputChar == '>' || InputChar == '.') {
			/* get rid of old value, by showing backspaces */
			for (cTemp = 0; cTemp < CurDigit; cTemp++)
				rputs("\b \b");

			sprintf(string, "%-ld", Maximum);
			string[NumDigits] = 0;
			rputs(string);

			strcpy(NumString, string);
			CurDigit = NumDigits;
		}
		else if (InputChar == '<' || InputChar == ',' || InputChar == 25) {
			/* get rid of old value, by showing backspaces */
			for (cTemp = 0; cTemp < CurDigit; cTemp++)
				rputs("\b \b");

			CurDigit = 0;
			string[CurDigit] = 0;

			strcpy(NumString, string);
		}
		else if (InputChar == '\r' || InputChar == '\n') {
			if (CurDigit == 0) {
				sprintf(string, "%-ld", DefaultVal);
				string[NumDigits] = 0;
				rputs(string);

				strcpy(NumString, string);
				CurDigit = NumDigits;

				rputs("\n");
				break;
			}
			/* see if number too high, if so, make it max */
			else if (atol(NumString) > Maximum) {
				/* get rid of old value, by showing backspaces */
				for (cTemp = 0; cTemp < CurDigit; cTemp++)
					rputs("\b \b");

				sprintf(string, "%-ld", Maximum);
				string[NumDigits] = 0;
				rputs(string);

				strcpy(NumString, string);
				CurDigit = NumDigits;
			}
			else { /* is a valid value */
				rputs("\n");
				break;
			}
		}
	}

	return (atol(NumString));
}


