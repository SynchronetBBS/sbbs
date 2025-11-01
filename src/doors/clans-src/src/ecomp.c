// Event Compiler for The Clans

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "defines.h"
#include "myopen.h"
#include "parsing.h"
#include "serialize.h"
#include "structs.h"
#include "unix_wrappers.h"
#include "win_wrappers.h"

#define MAX_TOKEN_CHARS     32
#define MAX_EVA_WORDS       34

static char *papszEvaKeyWords[MAX_EVA_WORDS] = {
	"Event",
	"Result",
	"Text",
	"Option",
	"End",
	"FalseFlag",
	"Prompt",
	"Fight",
	"TellQuest",
	"DoneQuest",
	"AddNews",
	"AddEnemy",
	"ResetEnemies",
	"Chat",
	"SetFlag",
	"ClearFlag",
	"Jump",
	"Heal",
	"TakeGold",
	"GiveGold",
	"GiveXP",
	"GiveItem",
	"Pause",
	"GiveFight",
	"GiveFollowers",
	"GivePoints",
	"EndQ",
	"Topic",
	"TellTopic",
	"JoinClan",
	"EndChat",
	"Input",
	"Display",
	"GetKey"
};

#define CHECK_SZLEGAL() do { \
	size_t TempLen = strlen(szLegal) + 1; \
	if (TempLen > 255) { \
		printf("String too long (%zu characters) for %s at %d\n", TempLen - 1, papszEvaKeyWords[(int)iKeyWord], CurLine); \
		return EXIT_FAILURE; \
	} \
} while(0)

#define ADD_SIZE_BYTE(x) do { \
	size_t TempLen = (x); \
	if (TempLen > 255) { \
		printf("Data too long (%zu bytes) for %s at %d\n", TempLen, papszEvaKeyWords[(int)iKeyWord], CurLine); \
		return EXIT_FAILURE; \
	} \
	((uint8_t*)Buffer)[BufferPtr++] = (uint8_t)TempLen; \
} while(0)
#define MALLOC_SZ	64000
int main(int argc, char *argv[])
{
	FILE *fpEvent, *fpOut;
	char szLine[255], *pcCurrentPos, szString[255], *pcBrace, szLegal[255],
	szLabel[30], szLabel1[30], szLabel2[30], szLabel3[30];
	char szToken[MAX_TOKEN_CHARS + 1], *Buffer, cKey, cTemp;
	char iKeyWord;
	int CurLine;
	int iTemp;
	bool EventInBuffer, NoMatch, CommentOn;
	int Errors = 0;
	struct EventHeader EventHeader = {0};
	uint8_t ehbuf[BUF_SIZE_EventHeader];
	size_t BufferPtr;

	printf("ECOMP (dk0.10) by Allen Ussher\n\n");

	if (argc != 3) {
		printf("Format: ecomp eventfile.txt outfile.[e/q]\n");
		exit(0);
	}

	fpEvent = fopen(argv[1], "r");
	if (!fpEvent) {
		printf("Error opening file\n");
		return(1);
	}

	fpOut = fopen(argv[2], "wb");
	if (!fpOut) {
		printf("Error opening fileout\n");
		fclose(fpEvent);
		return(1);
	}

	// allocate memory for our huge buffer
	Buffer = calloc(1, MALLOC_SZ);
	if (!Buffer) {
		printf("Error allocating mem\n");
		exit(0);
	}
	BufferPtr = 0;          // which element are we currently pointing to?
	EventInBuffer = false;  // true if carrying event in our buffer

	CurLine = 0;
	CommentOn = false;
	for (;;) {
		/* read in a line */
		if (fgets(szLine, 155, fpEvent) == NULL) break;

		CurLine++;

		/* Ignore all of line after comments or CR/LF char */
		pcCurrentPos=(char *)szLine;
		ParseLine(pcCurrentPos);

		//printf("%3d:  Processing %s\n", CurLine, pcCurrentPos);

		/* If no token was found, proceed to process the next line */
		if (!*pcCurrentPos) continue;


		// see if line starts with {}'s
		if (*pcCurrentPos == '{') {
			pcBrace = strchr(pcCurrentPos, '}');

			strlcpy(szLegal, pcCurrentPos + 1, sizeof(szLegal));
			szLegal[(pcBrace - pcCurrentPos) - 1 ] = 0;

			// use this legal string later

			// make "{xxx}yyy" into "yyy"
			memmove(pcCurrentPos, pcBrace+1, strlen(pcBrace + 1) + 1);

			//printf("legal is %s\nstring is %s\n", szLegal, pcCurrentPos);
			// printf("%3d: {} used: %s\n", CurLine, szLegal);
		}
		else
			szLegal[0] = 0;     // no legal string

		GetToken(pcCurrentPos, szToken);

		// if comment, comment on
		if (strcmp(szToken, ">>") == 0) {
			CommentOn = !CommentOn;

			if (CommentOn)
				printf("%3d: comment begins\n", CurLine);
			else
				printf("%3d: comment ends\n", CurLine);
			continue;
		}

		if (CommentOn)
			continue;

		if (szToken[0] == '$')
			break;

		/* Loop through list of keywords */
		NoMatch = true;
		for (iKeyWord = 0; iKeyWord < MAX_EVA_WORDS; ++iKeyWord) {
			/* If keyword matches */
			if (strcasecmp(szToken, papszEvaKeyWords[(int)iKeyWord]) == 0) {
				NoMatch = false;
				/* Process token */
				switch (iKeyWord) {
					case 0 :    // Event
						if (EventInBuffer) {
							Buffer[BufferPtr++] = 26;
							Buffer[BufferPtr++] = 0;    // no szLegal either
							Buffer[BufferPtr++] = 0;

							// write to buffer
							EventInBuffer = false;
							if (BufferPtr > INT32_MAX) {
								printf("Event too long (%zu bytes)!\n", BufferPtr);
								return EXIT_FAILURE;
							}

							EventHeader.EventSize = (int32_t)BufferPtr;
							s_EventHeader_s(&EventHeader, ehbuf, sizeof(ehbuf));
							fwrite(&ehbuf, sizeof(ehbuf), 1, fpOut);
							fwrite(Buffer, BufferPtr, 1, fpOut);

							BufferPtr = 0;
						}

						// initialize this new event
						EventInBuffer = true;

						EventHeader.Event = true;
						memset(EventHeader.szName, 0, sizeof(EventHeader.szName));
						strlcpy(EventHeader.szName, pcCurrentPos, sizeof(EventHeader.szName));
						printf("%3d: Found event %s\n", CurLine, pcCurrentPos);
						break;
					case 1 :    // Result
					case 27:    // Topic
						if (EventInBuffer) {
							Buffer[BufferPtr++] = 26;
							Buffer[BufferPtr++] = 0;    // no szLegal either
							Buffer[BufferPtr++] = 0;


							// write to buffer
							EventInBuffer = false;

							if (BufferPtr > INT32_MAX) {
								printf("Event too long (%zu bytes)!\n", BufferPtr);
								return EXIT_FAILURE;
							}

							EventHeader.EventSize = (int32_t)BufferPtr;
							s_EventHeader_s(&EventHeader, ehbuf, sizeof(ehbuf));
							fwrite(&ehbuf, sizeof(ehbuf), 1, fpOut);
							fwrite(Buffer, BufferPtr, 1, fpOut);

							BufferPtr = 0;
						}

						// initialize this new event
						EventInBuffer = true;

						EventHeader.Event = false;
						memset(EventHeader.szName, 0, sizeof(EventHeader.szName));
						strlcpy(EventHeader.szName, pcCurrentPos, sizeof(EventHeader.szName));

						break;
					case 2 :    // Text
					case 6 :    // Prompt
					case 10 :   // AddNews
					case 32 :   // Display
						// put command in buffer
						Buffer[ BufferPtr++ ] = iKeyWord;

						// put szLegal in buffer
						CHECK_SZLEGAL();
						((uint8_t*)Buffer)[ BufferPtr ] = (uint8_t)(strlen(szLegal) + 1);
						BufferPtr++;
						strlcpy(&Buffer[BufferPtr], szLegal, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLegal) + 1);

						if (*pcCurrentPos == 0) {
							// no text, data length is 1 -- just a '\0'
							ADD_SIZE_BYTE(1);
							Buffer[ BufferPtr++ ] = 0;
						}
						else {
							/* must skip the " which starts the line */
							ADD_SIZE_BYTE(strlen(&pcCurrentPos[1]) + 1);
							strlcpy(&Buffer[ BufferPtr ], &pcCurrentPos[1], MALLOC_SZ - BufferPtr);
							BufferPtr += (strlen(&pcCurrentPos[1]) + 1);
						}
						break;
					case 3  :   // Option
					case 33  :  // GetKey
						// put command in buffer
						Buffer[ BufferPtr++ ] = iKeyWord;

						// put szLegal in buffer
						CHECK_SZLEGAL();
						((uint8_t*)Buffer)[ BufferPtr ] = (uint8_t)(strlen(szLegal) + 1);
						BufferPtr++;
						strlcpy(&Buffer[BufferPtr], szLegal, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLegal) + 1);

						if (pcCurrentPos[0] == '~')
							cKey = 13;  // Enter
						else
							cKey = pcCurrentPos[0];

						GetToken(&pcCurrentPos[1], szLabel);

						ADD_SIZE_BYTE(sizeof(char) + strlen(szLabel) + sizeof(char));
						Buffer[ BufferPtr++ ] = cKey;
						strlcpy(&Buffer[ BufferPtr ], szLabel, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLabel) + sizeof(char));

						//printf("%3d:  option:  %c - %s\n", CurLine, cKey, szLabel);
						break;
					case 7  :   // Fight
						// put command in buffer
						Buffer[ BufferPtr++ ] = iKeyWord;

						// put szLegal in buffer
						CHECK_SZLEGAL();
						((uint8_t*)Buffer)[ BufferPtr ] = (uint8_t)(strlen(szLegal) + 1);
						BufferPtr++;
						strlcpy(&Buffer[BufferPtr], szLegal, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLegal) + 1);


						GetToken(pcCurrentPos, szLabel1);
						GetToken(pcCurrentPos, szLabel2);
						GetToken(pcCurrentPos, szLabel3);

						// write datalength
						ADD_SIZE_BYTE(strlen(szLabel1) + strlen(szLabel2) +
						    strlen(szLabel3) + sizeof(char) * 3);

						// write to file the 3 labels
						ADD_SIZE_BYTE(strlen(szLabel1) + 1);
						strlcpy(&Buffer[ BufferPtr ], szLabel1, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLabel1) + 1);

						ADD_SIZE_BYTE(strlen(szLabel2) + 1);
						strlcpy(&Buffer[ BufferPtr ], szLabel2, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLabel2) + 1);

						ADD_SIZE_BYTE(strlen(szLabel3) + 1);
						strlcpy(&Buffer[ BufferPtr ], szLabel3, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLabel3) + 1);
						break;
					case 8  :   // TellQuest
					case 13 :   // Chat
					case 14 :   // SetFlag
					case 15 :   // ClearFlag
					case 16 :   // Jump
					case 17 :   // Heal
					case 18 :   // TakeGold
					case 19 :   // GiveGold
					case 20 :   // GiveXP
					case 21 :   // GiveItem
					case 23 :   // GiveFight
					case 24 :   // GiveFollowers
					case 25 :   // GivePoints
					case 28 :   // TellTopic
						// put command in buffer
						Buffer[ BufferPtr++ ] = iKeyWord;

						// put szLegal in buffer
						CHECK_SZLEGAL();
						((uint8_t*)Buffer)[ BufferPtr ] = (uint8_t)(strlen(szLegal) + 1);
						BufferPtr++;
						strlcpy(&Buffer[BufferPtr], szLegal, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLegal) + 1);

						ADD_SIZE_BYTE(strlen(pcCurrentPos) + 1);
						strlcpy(&Buffer[ BufferPtr ], pcCurrentPos, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(pcCurrentPos) + 1);
						break;
					case 4  :   // End
					case 9  :   // DoneQuest
					case 22 :   // Pause
					case 29 :   // JoinClan
					case 30 :   // EndChat
						Buffer[ BufferPtr++ ] = iKeyWord;

						// put szLegal in buffer
						CHECK_SZLEGAL();
						((uint8_t*)Buffer)[ BufferPtr ] = (uint8_t)(strlen(szLegal) + 1);
						BufferPtr++;
						strlcpy(&Buffer[BufferPtr], szLegal, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLegal) + 1);

						// no datalength, so put 0
						Buffer[ BufferPtr++ ] = 0;
						break;
					case 11 :   // AddEnemy
						// put command in buffer
						Buffer[ BufferPtr++ ] = iKeyWord;

						// put szLegal in buffer
						CHECK_SZLEGAL();
						((uint8_t*)Buffer)[ BufferPtr ] = (uint8_t)(strlen(szLegal) + 1);
						BufferPtr++;
						strlcpy(&Buffer[BufferPtr], szLegal, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLegal) + 1);

						// get monster filename
						GetToken(pcCurrentPos, szLabel);        // filename
						iTemp = atoi(pcCurrentPos);             // # of beast
						if (iTemp < 0 || iTemp > 255) {
							printf("Invalid beast number in %s %d at %d\n", papszEvaKeyWords[(int)iKeyWord], iTemp, CurLine);
							return EXIT_FAILURE;
						}
						cTemp = (char)iTemp;
						//printf("%3d:  addenemy %s %d\n", CurLine, szLabel, cTemp);

						ADD_SIZE_BYTE(strlen(szLabel) + 2);
						strlcpy(&Buffer[ BufferPtr ], szLabel, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLabel) + 1);
						Buffer[ BufferPtr++ ] = cTemp;
						break;
					case 26 :   // EndQ
						if (EventInBuffer) {
							// put an EndQ command there
							Buffer[BufferPtr++] = 26;
							Buffer[BufferPtr++] = 0;    // no szLegal either
							Buffer[BufferPtr++] = 0;

							// write to buffer
							EventInBuffer = false;

							if (BufferPtr > INT32_MAX) {
								printf("Event too long (%zu bytes)!\n", BufferPtr);
								return EXIT_FAILURE;
							}

							EventHeader.EventSize = (int32_t)BufferPtr;
							s_EventHeader_s(&EventHeader, ehbuf, sizeof(ehbuf));
							fwrite(&ehbuf, sizeof(ehbuf), 1, fpOut);
							fwrite(Buffer, BufferPtr, 1, fpOut);

							BufferPtr = 0;
						}
						break;
					case 31 :   // Input
						// put command in buffer
						Buffer[ BufferPtr++ ] = iKeyWord;

						// put szLegal in buffer
						CHECK_SZLEGAL();
						((uint8_t*)Buffer)[ BufferPtr ] = (uint8_t)(strlen(szLegal) + 1);
						BufferPtr++;
						strlcpy(&Buffer[BufferPtr], szLegal, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szLegal) + 1);

						GetToken(pcCurrentPos, szLabel);
						strlcpy(szString, pcCurrentPos, sizeof(szString));

						ADD_SIZE_BYTE(strlen(szLabel) + strlen(szString) + 2*sizeof(char));

						// write length of szLabel
						ADD_SIZE_BYTE(strlen(szLabel) + sizeof(char));

						// write label
						strlcpy(&Buffer[ BufferPtr ], szLabel, MALLOC_SZ - BufferPtr);
						// increment pointer
						BufferPtr += (strlen(szLabel) + sizeof(char));

						ADD_SIZE_BYTE(strlen(szString) + sizeof(char));
						strlcpy(&Buffer[ BufferPtr ], szString, MALLOC_SZ - BufferPtr);
						BufferPtr += (strlen(szString) + sizeof(char));

						//printf("%3d:  option:  %c - %s\n", CurLine, cKey, szLabel);
						break;
				}
				break;
			}
		}
		if (NoMatch) {
			printf("%3d:  Invalid command %s\n", CurLine, szToken);
			Errors++;
		}
	}

	// if event in memory, flush it to file
	if (EventInBuffer) {
		Buffer[BufferPtr++] = 26;
		Buffer[BufferPtr++] = 0;    // no szLegal either
		Buffer[BufferPtr++] = 0;

		// write to buffer
		EventInBuffer = false;

		if (BufferPtr > INT32_MAX) {
			printf("Event too long (%zu bytes)!\n", BufferPtr);
			return EXIT_FAILURE;
		}

		EventHeader.EventSize = (int32_t)BufferPtr;
		s_EventHeader_s(&EventHeader, ehbuf, sizeof(ehbuf));
		fwrite(&ehbuf, sizeof(ehbuf), 1, fpOut);
		fwrite(Buffer, BufferPtr, 1, fpOut);

		BufferPtr = 0;
	}
	free(Buffer);

	fclose(fpOut);
	fclose(fpEvent);

	printf("Done!\n\n%d error(s)\n\n", Errors);
	return(0);
}
