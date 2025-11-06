#ifndef THE_CLANS__PARSING___H
#define THE_CLANS__PARSING___H

#include "defines.h"

void ParseLine(char *szString);
/*
 * This function will take a string and remove comments (#'s and ;'s) and
 * then remove leading and trailing whitespaces.  Anything in quotes is
 * ignored.
 */

void GetToken(char *szString, char *szToken);
/*
 * From the string, this gets the next available token and removes it
 * from the string so that the string begins with the NEXT token.
 */

#endif
