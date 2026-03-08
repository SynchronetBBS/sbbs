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

// Compiles the language file, and optionally generates mstrings.h.
//
// Usage: langcomp <strings.txt> [strings.xl [mstrings.h]]
//
// When mstrings.h path is provided, langcomp reads optional symbolic names
// embedded in strings.txt and writes an auto-generated C header.
//
// strings.txt line format:
//   NNNN [ST_MACRONAME ]<string content>
//
// The macro name is optional.  It is recognized when the token immediately
// after the 4-digit ID matches ST_[A-Z0-9_]+ followed by a space.

#include <stdio.h>
#include <stdlib.h>
#include "defines.h"
#include <string.h>
#include <ctype.h>
#include "platform.h"

#include "serialize.h"

void Convert(char *Dest, char *From);

struct Language Language;

uint8_t serBuf[4096];

// Symbolic names for each string slot (empty string = no macro defined)
#define MACRONAME_SZ	64
static char MacroNames[2000][MACRONAME_SZ];

#define MALLOC_SZ	64000
int main(int argc, char *argv[])
{
	FILE *fFrom, *fTo;
	int CurString;
	size_t iTemp;
	char TempString[800], String[800], FromFile[PATH_SIZE], ToFile[PATH_SIZE];
	char HeaderFile[PATH_SIZE];
	int writeHeader = 0;

	printf("The Clans Language File Compiler v.1.1\n\n");

	if (argc < 2 || argc > 4) {
		printf("Usage: langcomp <strings.txt> [strings.xl [mstrings.h]]\n");
		exit(EXIT_FAILURE);
	}

	strlcpy(FromFile, argv[1], sizeof(FromFile));
	if (argc >= 3) {
		strlcpy(ToFile, argv[2], sizeof(ToFile));
	} else {
		for (iTemp = strlen(FromFile); iTemp>0; iTemp--)
			if (FromFile[iTemp] == '.')
				break;

		if (iTemp == 0) {
			strlcpy(ToFile, FromFile, sizeof(ToFile));
			strlcat(ToFile, ".xl", sizeof(ToFile));
			strlcat(FromFile, ".txt", sizeof(FromFile));
		}
		else {
			FromFile[iTemp] = 0;
			strlcpy(ToFile, FromFile, sizeof(ToFile));
			strlcat(ToFile, ".xl", sizeof(ToFile));
			FromFile[iTemp] = '.';
		}
	}

	if (argc == 4) {
		strlcpy(HeaderFile, argv[3], sizeof(HeaderFile));
		writeHeader = 1;
	}

	fFrom = fopen(FromFile, "r");
	if (!fFrom) {
		printf("Error opening %s\n", FromFile);
		exit(EXIT_FAILURE);
	}

	// initialize the big string
	strlcpy(Language.Signature, "The Clans Language File v1.0\x1A", sizeof(Language.Signature));
	for (iTemp = 0;  iTemp < 2000; iTemp++) {
		Language.StrOffsets[iTemp] = 0;
		MacroNames[iTemp][0] = '\0';
	}
	Language.NumBytes = 0;

	Language.BigString = malloc(MALLOC_SZ);
	if (!Language.BigString) {
		printf("Couldn't allocate enough memory to run!");
		fclose(fFrom);
		exit(EXIT_FAILURE);
	}

	// just to ensure it works out, make the first string in BigString = ""
	Language.BigString[0] = 0;
	Language.NumBytes = 1;

	// read through line by line
	for (;;) {
		if (!fgets(TempString, 800, fFrom)) break;

		if (TempString[0] == '#')   continue;       // skip if comment line
		if (TempString[0] == ' ')   continue;       // skip if comment line

		// break down string
		// get rid of \n and \r
		if (TempString[0]) {
			while (TempString[0] && (TempString[ strlen(TempString) - 1] == '\n' || TempString[ strlen(TempString) - 1] == '\r'))
				TempString[ strlen(TempString) - 1] = 0;
		}

		// convert @@ at end to \n
		if (TempString[0] && TempString[1]) {
			if (TempString[ strlen(TempString) - 2] == '@' &&
					TempString[ strlen(TempString) - 1] == '@') {
				TempString[ strlen(TempString) - 2] = 0;
				strlcat(TempString, "\n", sizeof(TempString));
			}
		}

		CurString = atoi(TempString);
		if (CurString == 0) continue;

		// Check for optional macro name after the 4-digit ID.
		// A macro name starts with "ST_" and contains only uppercase
		// letters, digits, and underscores, followed by a space.
		char *content = &TempString[5];
		if (strncmp(content, "ST_", 3) == 0) {
			size_t nameLen = 3;
			while (content[nameLen] && content[nameLen] != ' ' &&
			       (isupper((unsigned char)content[nameLen]) ||
			        isdigit((unsigned char)content[nameLen]) ||
			        content[nameLen] == '_')) {
				nameLen++;
			}
			if (content[nameLen] == ' ' && nameLen < MACRONAME_SZ) {
				// Valid macro name found
				strlcpy(MacroNames[CurString], content, nameLen + 1);
				content = &content[nameLen + 1];
			}
		}

		// convert string's special language codes
		Convert(String, content);

		Language.StrOffsets[CurString] = Language.NumBytes;
		strlcpy(&Language.BigString[ Language.NumBytes ], String, MALLOC_SZ - Language.NumBytes);
		if (strlen(String) + 1 > UINT16_MAX) {
			printf("Overlong string %03d\n", CurString);
			exit(EXIT_FAILURE);
		}
		Language.NumBytes += (uint16_t)(strlen(String)+1); // must also include zero byte
	}

	fclose(fFrom);

	// now write it to the binary file
	fTo = fopen(ToFile, "wb");
	if (!fTo) {
		printf("Error opening %s\n", ToFile);
		free(Language.BigString);
		exit(EXIT_FAILURE);
	}

	s_Language_s(&Language, serBuf, sizeof(serBuf));
	fwrite(serBuf, BUF_SIZE_Language, 1, fTo);
	fwrite(Language.BigString, Language.NumBytes, 1, fTo);

	fclose(fTo);

	free(Language.BigString);

	printf("Done!\n\n%u bytes were used.  (max 64000).\n", Language.NumBytes);

	// Optionally write the auto-generated C header
	if (!writeHeader)
		return 0;

	FILE *fHdr = fopen(HeaderFile, "w");
	if (!fHdr) {
		printf("Error opening header file %s\n", HeaderFile);
		return 1;
	}

	fprintf(fHdr,
		"/* AUTO-GENERATED by langcomp from strings.txt -- DO NOT EDIT\n"
		" * Regenerate by running: gmake -C data/ strings.xl\n"
		" */\n"
		"#ifndef MSTRINGS_H_\n"
		"#define MSTRINGS_H_\n"
		"\n");

	int count = 0;
	for (iTemp = 0; iTemp < 2000; iTemp++) {
		if (MacroNames[iTemp][0]) {
			fprintf(fHdr,
				"#define %-24s (&Language->BigString[Language->StrOffsets[%zu]])\n",
				MacroNames[iTemp], iTemp);
			count++;
		}
	}

	fprintf(fHdr, "\n#endif /* MSTRINGS_H_ */\n");
	fclose(fHdr);

	printf("Wrote %d macro definitions to %s\n", count, HeaderFile);
	return 0;
}

/* IMPORTANT!!!!   Format of the compiled language (*.XL)


    struct
    {
        char Signature[30];         // "X-Engine Language File v1.0"

        uint16_t StrOffsets[2000];       // offsets for up to 500 strings

        char far *BigString;        // All 500 strings jumbled together into
                                    // one
    } LanguageHeader;
*/

void Convert(char *Dest, char *From)
{
	int16_t dCurChar = 0, fCurChar = 0;

	Dest[dCurChar] = 0;    // make it 0 length

	while (From[fCurChar]) {
		if (From[fCurChar] == '^') {
			if (From[fCurChar+1] == 'M') {
				Dest[dCurChar] = '\n';
				dCurChar++;
				fCurChar += 2;
			}
			else if (From[fCurChar+1] == '[') {
				Dest[dCurChar] = 27;
				dCurChar++;
				fCurChar += 2;
			}
			else if (From[fCurChar+1] == '-') {
				Dest[dCurChar] = 0;
				dCurChar++;
				fCurChar += 2;
			}
			else if (From[fCurChar+1] == 'H') {
				Dest[dCurChar] = '\b';
				dCurChar++;
				fCurChar += 2;
			}
			else if (From[fCurChar+1] == 'N') {
				Dest[dCurChar] = '\r';
				dCurChar++;
				fCurChar += 2;
			}
			else if (From[fCurChar+1] == 'G') {
				Dest[dCurChar] = 7;
				dCurChar++;
				fCurChar += 2;
			}
			else {
				Dest[dCurChar] = From[fCurChar];
				dCurChar++;
				fCurChar++;
			}
		}
		else {
			Dest[dCurChar] = From[fCurChar];

			fCurChar++;
			dCurChar++;
		}
	}

	Dest[dCurChar] = 0;

}
