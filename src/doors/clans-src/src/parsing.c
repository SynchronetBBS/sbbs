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
 * Parsing functions
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "defines.h"
#include "language.h"
#include "parsing.h"

// ------------------------------------------------------------------------- //

void Strip(char *szString)
/*
 * This function will strip any leading and trailing spaces from the
 * string.
 */
{
	char *start = szString;
	char *end;
	size_t len;

	// Set start to first non-space character
	while(isspace(*start))
		start++;

	// Set end to the string terminator
	end = strchr(start, 0);

	/*
	 * Set end to last non-space character
	 * if there are no non-space characters, end ends up as start-1
	 */
	end--;
	while (end >= start && isspace(*end))
		end--;

	// Early return if it's all whitespace
	if (end < start) {
		*szString = 0;
		return;
	}

	// Set len to the number of non-whitespace characters
	len = (size_t)(end - start) + 1;
	memmove(szString, start, len);
	szString[len] = 0;
}

// ------------------------------------------------------------------------- //

void ParseLine(char *szString)
/*
 * This function will take a string and remove comments (#'s and ;'s) and
 * then remove leading and trailing whitespaces.  Anything in quotes is
 * ignored.
 */
{
	char *pcCurrentPos;
	bool MetQuote = false;

	pcCurrentPos = szString;

	while (*pcCurrentPos) {
		if (*pcCurrentPos == '"')
			MetQuote = !MetQuote;

		if (*pcCurrentPos=='\n' || *pcCurrentPos=='\r' || *pcCurrentPos==';'
				|| *pcCurrentPos == '#') {
			if (!MetQuote) {
				*pcCurrentPos='\0';
				break;
			}
		}
		++pcCurrentPos;
	}

	pcCurrentPos = szString;
	while (*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

	memmove(szString, pcCurrentPos, strlen(pcCurrentPos) + 1);

	// parse second time to get rid of comment's end
	Strip(szString);
}

// ------------------------------------------------------------------------- //

void GetToken(char *szString, char *szToken)
/*
 * From the string, this gets the next available token and removes it
 * from the string so that the string begins with the NEXT token.
 */
{
	char *pcCurrentPos;
	size_t uCount;

	/* Ignore all of line after comments or CR/LF char */
	pcCurrentPos=(char *)szString;
	while (*pcCurrentPos) {
		if (*pcCurrentPos=='\n' || *pcCurrentPos=='\r') {
			*pcCurrentPos='\0';
			break;
		}
		++pcCurrentPos;
	}

	/* Search for beginning of first token on line */
	pcCurrentPos = (char *)szString;
	while (*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

	/* If no token was found, proceed to process the next line */
	if (!(*pcCurrentPos)) {
		szToken[0] = 0;
		szString[0] = 0;
		return;
	}

	/* Get first token from line */
	uCount=0;
	while (*pcCurrentPos && !isspace(*pcCurrentPos)) {
		if (uCount < MAX_TOKEN_CHARS)
			szToken[uCount++] = *pcCurrentPos;
		++pcCurrentPos;
	}
	if (uCount <= MAX_TOKEN_CHARS)
		szToken[uCount]='\0';
	else
		szToken[MAX_TOKEN_CHARS]='\0';

	/* Find beginning of configuration option parameters */
	while (*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

	/* Trim trailing spaces from setting string */
	if (*pcCurrentPos) {
		for (uCount = strlen(pcCurrentPos) - 1; uCount > 0; --uCount) {
			if (isspace(pcCurrentPos[uCount])) {
				pcCurrentPos[uCount]='\0';
			}
			else {
				break;
			}
		}
	}

	memmove(szString, pcCurrentPos, strlen(pcCurrentPos) + 1);
}

// ------------------------------------------------------------------------- //

bool iscodechar(char c)
/*
 * Returns true if the character is a digit or within 'A' and 'F'
 * (Used mainly to see if it is a valid char for `xx codes.
 *
 */
{
	if ((c <= 'F' && c >= 'A')  || (isdigit(c) && (c >= 0 && c <= 127)))
		return true;
	else
		return false;
}


void RemovePipes(char *pszSrc, char *pszDest)
/*
 * This function removes any colour codes from the string and outputs the
 * result to the destination.
 *
 */
{
	while (*pszSrc) {
		if (*pszSrc == '|' && isdigit(*(pszSrc+1)) && isalnum(*(pszSrc+2)))
			pszSrc += 3;
		else if (*pszSrc == '`' && iscodechar(*(pszSrc+1)) && iscodechar(*(pszSrc+2)))
			pszSrc += 3;
		else {
			/* else is normal */
			*pszDest = *pszSrc;
			++pszSrc;
			++pszDest;
		}
	}
	*pszDest = 0;
}

// ------------------------------------------------------------------------- //

void PadString(char *szString, int16_t PadLength)
/*
 * This takes the string and pads it with spaces.
 */
{
	int16_t iTemp;
	char szTemp[255], *pc;

	RemovePipes(szString, szTemp);

	pc = strchr(szString, 0);

	// if less than the required length, pad
	if ((signed)strlen(szTemp) < PadLength) {
		for (iTemp = 0; iTemp < PadLength - ((signed)strlen(szTemp)); iTemp++) {
			*pc = ' ';
			pc++;
		}

		*pc = 0;
	}
}

