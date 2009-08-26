
void Strip(char *szString);
/*
 * This function will strip any leading and trailing spaces from the
 * string.
 */

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

void RemovePipes(char *pszSrc, char *pszDest);
/*
 * This function removes any colour codes from the string and outputs the
 * result to the destination.
 *
 */

void PadString(char *szString, _INT16 PadLength);
/*
 * This takes the string and pads it with spaces.
 */

__BOOL iscodechar(char c);
/*
 * Returns TRUE if the character is a digit or within 'A' and 'F'
 * (Used mainly to see if it is a valid char for `xx codes.
 *
 */
