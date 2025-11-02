// MakePak -- creates .PAK file using PAK.LST file

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __MSDOS__
# include <alloc.h>
# include <malloc.h>
#endif /* __MSDOS__ */
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "defines.h"
#include "myopen.h"
#include "parsing.h"
#include "structs.h"

#define MAX_TOKEN_CHARS 32

// Functions
static void AddToPak(char *pszFileName, char *pszFileAlias, FILE *fpPakFile);

// Global Data
static int32_t lPakSize;

int main(int argc, char *argv[])
{
	FILE *fpPakFile, *fpList;
	char szFileName[PATH_SIZE], szFileAlias[30];       // filealias = filename in .pak
	char szLine[255], szPakName[PATH_SIZE], szPakList[PATH_SIZE];

	printf("MAKEPAK utility by Allen Ussher\n\n");

	if (argc != 3) {
		printf("Format:  MAKEPAK [.PAK to produce] [PAKfile listing]\n");
		exit(0);
	}


	if (argc == 3) {
		strlcpy(szPakName, argv[1], sizeof(szPakName));
		strlcpy(szPakList, argv[2], sizeof(szPakList));
	}
	else {
		strlcpy(szPakName, "mypak.pak", sizeof(szPakName));
		strlcpy(szPakList, "pak.lst", sizeof(szPakList));
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

static void AddToPak(char *pszFileName, char *pszFileAlias, FILE *fpPakFile)
{
	struct FileHeader FileHeader;
	FILE *fpInput;
	size_t CurByte, NumBytes;
	char FAR *Chunk;
	uint8_t fhbuf[BUF_SIZE_FileHeader];
	long lTemp;

	fpInput = fopen(pszFileName, "rb");
	if (!fpInput) {
		printf("Couldn't read in %s\n", pszFileName);
		return;
	}

	lPakSize += BUF_SIZE_FileHeader;

	// get size of file
	fseek(fpInput, 0L, SEEK_END);
	lTemp = ftell(fpInput);
	if (lTemp < INT32_MIN || lTemp > INT32_MAX) {
		printf("File size %ld unsupported\n", lTemp);
		fclose(fpInput);
		return;
	}
	FileHeader.lFileSize = (int32_t)lTemp;

	// go back to start of file
	if (fseek(fpInput, 0L, SEEK_SET)) {
		puts("Failed to seek to start");
		fclose(fpInput);
		return;
	}

	// get header info
	strlcpy(FileHeader.szFileName, pszFileAlias, sizeof(FileHeader.szFileName));
	FileHeader.lStart = lPakSize;
	FileHeader.lEnd = lPakSize + FileHeader.lFileSize;
	lPakSize += FileHeader.lFileSize;

	// write header to pakfile
	s_FileHeader_s(&FileHeader, fhbuf, sizeof(fhbuf));
	fwrite(fhbuf, sizeof(fhbuf), 1, fpPakFile);

	// read in file and transfer to pakfile
	CurByte = 0;
	NumBytes = (size_t)FileHeader.lFileSize;

	Chunk = calloc(1, 64000);
	if (!Chunk) {
		fputs("out of memory!", stderr);
		fflush(stderr);
		fclose(fpInput);
		exit(1);
	}
	while (CurByte != FileHeader.lFileSize) {
		if (NumBytes > 64000L) {
			if (fread(Chunk, 64000L, 1, fpInput) != 1) {
				puts("Read error");
				break;
			}
			if (fwrite(Chunk, 64000L, 1, fpPakFile) != 1) {
				puts("Write error");
				break;
			}

			NumBytes -= 64000L;
			CurByte += 64000L;
		}
		else {
			if (fread(Chunk, NumBytes, 1, fpInput) != 1) {
				puts("Read error");
				break;
			}
			if (fwrite(Chunk, NumBytes, 1, fpPakFile) != 1) {
				puts("Write error");
				break;
			}

			CurByte += NumBytes;
			NumBytes -= NumBytes;
		}
	}

	fclose(fpInput);

	free(Chunk);
}
