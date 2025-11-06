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
#include "platform.h"

#include "defines.h"
#include "language.h"
#include "parsing.h"

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
