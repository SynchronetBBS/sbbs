
void Language_Init(char *szLangFile);
/*
 * The language file is loaded.  If it isn't found, the system is shut
 * down.
 */

void Language_Close(void);
/*
 * The language file is freed from memory.
 */

void LoadStrings(_INT16 StartIndex, _INT16 NumStrings, char *szStrings[]);
/*
 * This function loads NumStrings of strings from the language file.
 * starting with string StartIndex.
 *
 * PRE: StartIndex is the first string's index.
 *      NumStrings is the number of strings to load.
 *
 */

char *MakeStr(_INT16 length);
/*
 * This returns a pointer to a malloc'd string of length length.
 */

void CheckMem(void *Test);
/*
 * Gives system error if the pointer is NULL.
 */

