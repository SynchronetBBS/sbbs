// MakePak -- creates .PAK file using PAK.LST file

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __MSDOS__
# include <alloc.h>
# include <malloc.h>
#endif /* __MSDOS__ */
#include <ctype.h>

#include "defines.h"
#include "myopen.h"
#include "structs.h"

#define MAX_TOKEN_CHARS 32

// Functions
void GetToken(char *szString, char *szToken);
void AddToPak(char *pszFileName, char *pszFileAlias, FILE *fpPakFile);

// Global Data
int32_t lPakSize;

int main(int argc, char *argv[])
{
	FILE *fpPakFile, *fpList;
	char szFileName[30], szFileAlias[30];       // filealias = filename in .pak
	char szLine[255], szPakName[30], szPakList[30];

	printf("MAKEPAK utility by Allen Ussher\n\n");

	if (argc != 3) {
		printf("Format:  MAKEPAK [.PAK to produce] [PAKfile listing]\n");
		exit(0);
	}


	if (argc == 3) {
		strcpy(szPakName, argv[1]);
		strcpy(szPakList, argv[2]);
	}
	else {
		strcpy(szPakName, "mypak.pak");
		strcpy(szPakList, "pak.lst");
	}

	fpPakFile = fopen(szPakName, "wb");
	if (!fpPakFile) {
		printf("can't open %s\n", szPakName);
		exit(0);
	}

	fpList = fopen(szPakList, "r");
	if (!fpList) {
		printf("can't open %s\n", szPakList);
		fclose(fpPakFile);
		exit(0);
	}

	lPakSize = 0;

	for (;;) {
		if (fgets(szLine, 255, fpList) == NULL)
			break;

		// skip blank lines
		if (szLine[0] == '\r' || szLine[0] == '#' || szLine[0] == '\n')
			continue;

		GetToken(szLine, szFileName);
		GetToken(szLine, szFileAlias);

		printf("Processing %-20s %-20s\n", szFileName, szFileAlias);

		if (szFileName[0] == 0 || szFileAlias[0] == 0)
			break;


		AddToPak(szFileName, szFileAlias, fpPakFile);
	}

	fclose(fpPakFile);
	fclose(fpList);
	return(0);
}

void AddToPak(char *pszFileName, char *pszFileAlias, FILE *fpPakFile)
{
	struct FileHeader FileHeader;
	FILE *fpInput;
	long CurByte, NumBytes;
	char FAR *Chunk;
	uint8_t fhbuf[BUF_SIZE_FileHeader];

	fpInput = fopen(pszFileName, "rb");
	if (!fpInput) {
		printf("Couldn't read in %s\n", pszFileName);
		return;
	}

	lPakSize += BUF_SIZE_FileHeader;

	// get size of file
	fseek(fpInput, 0L, SEEK_END);
	FileHeader.lFileSize = ftell(fpInput);

	// go back to start of file
	fseek(fpInput, 0L, SEEK_SET);

	// get header info
	strcpy(FileHeader.szFileName, pszFileAlias);
	FileHeader.lStart = lPakSize;
	FileHeader.lEnd = lPakSize + FileHeader.lFileSize;
	lPakSize += FileHeader.lFileSize;

	// write header to pakfile
	s_FileHeader_s(&FileHeader, fhbuf, sizeof(fhbuf));
	fwrite(fhbuf, sizeof(fhbuf), 1, fpPakFile);

	// read in file and transfer to pakfile
	CurByte = 0;
	NumBytes = FileHeader.lFileSize;

	Chunk = calloc(1, 64000);
	if (!Chunk) {
		fputs("out of memory!", stderr);
		fflush(stderr);
		exit(1);
	}
	while (CurByte != FileHeader.lFileSize) {
		if (NumBytes > 64000L) {
			fread(Chunk, 64000L, 1, fpInput);
			fwrite(Chunk, 64000L, 1, fpPakFile);

			NumBytes -= 64000L;
			CurByte += 64000L;
		}
		else {
			fread(Chunk, NumBytes, 1, fpInput);
			fwrite(Chunk, NumBytes, 1, fpPakFile);

			CurByte += NumBytes;
			NumBytes -= NumBytes;
		}
	}

	fclose(fpInput);

	free(Chunk);
}

void GetToken(char *szString, char *szToken)
{
	char *pcCurrentPos;
	unsigned int uCount;

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
	if (!*pcCurrentPos) {
		szToken[0] = 0;
		szString[0] = 0;
		return;
	}

	/* Get first token from line */
	uCount=0;
	while (*pcCurrentPos && !isspace(*pcCurrentPos)) {
		if (uCount<MAX_TOKEN_CHARS) szToken[uCount++]=*pcCurrentPos;
		++pcCurrentPos;
	}
	if (uCount<=MAX_TOKEN_CHARS)
		szToken[uCount]='\0';
	else
		szToken[MAX_TOKEN_CHARS]='\0';

	/* Find beginning of configuration option parameters */
	while (*pcCurrentPos && isspace(*pcCurrentPos)) ++pcCurrentPos;

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
	memmove(szString, pcCurrentPos, strlen(pcCurrentPos) + 1);
}
