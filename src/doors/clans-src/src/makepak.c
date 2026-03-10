// MakePak -- creates .PAK file using PAK.LST file

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

#include "defines.h"
#include "myopen.h"
#include "parsing.h"
#include "structs.h"
#include "u8cp437.h"

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
		exit(EXIT_FAILURE);
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
		exit(EXIT_FAILURE);
	}

	fpList = fopen(szPakList, "r");
	if (!fpList) {
		printf("can't open %s\n", szPakList);
		fclose(fpPakFile);
		exit(EXIT_FAILURE);
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

		if (szFileAlias[0] != '/') {
			printf("Bad alias '%s': must start with '/'\n"
			       "  Example: /n/foo, not @pak/n/foo\n",
			       szFileAlias);
			fclose(fpPakFile);
			fclose(fpList);
			exit(EXIT_FAILURE);
		}

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
	uint8_t fhbuf[BUF_SIZE_FileHeader];

	int is_utf8;
	fpInput = u8_fopen(pszFileName, "rb", &is_utf8);
	if (!fpInput) {
		printf("Couldn't read in %s\n", pszFileName);
		return;
	}

	lPakSize += BUF_SIZE_FileHeader;

	if (is_utf8) {
		// UTF-8 text file: read line-by-line, convert to CP437,
		// buffer the result, then write to pak.
		char line[4096];
		int lineno = 0;
		size_t bufcap = 64000;
		size_t buflen = 0;
		char *buf = malloc(bufcap);
		if (!buf) {
			fputs("out of memory!", stderr);
			fflush(stderr);
			fclose(fpInput);
			exit(1);
		}

		while (u8_fgets(line, sizeof(line), fpInput, is_utf8,
		                pszFileName, &lineno) != NULL) {
			size_t n = strlen(line);
			if (buflen + n > bufcap) {
				bufcap = (buflen + n) * 2;
				char *tmp = realloc(buf, bufcap);
				if (!tmp) {
					fputs("out of memory!", stderr);
					fflush(stderr);
					free(buf);
					fclose(fpInput);
					exit(1);
				}
				buf = tmp;
			}
			memcpy(buf + buflen, line, n);
			buflen += n;
		}

		fclose(fpInput);

		if (buflen > INT32_MAX) {
			printf("Converted size %zu unsupported\n", buflen);
			free(buf);
			return;
		}

		FileHeader.lFileSize = (int32_t)buflen;
		strlcpy(FileHeader.szFileName, pszFileAlias,
		        sizeof(FileHeader.szFileName));
		FileHeader.lStart = lPakSize;
		FileHeader.lEnd = lPakSize + FileHeader.lFileSize;
		lPakSize += FileHeader.lFileSize;

		s_FileHeader_s(&FileHeader, fhbuf, sizeof(fhbuf));
		fwrite(fhbuf, sizeof(fhbuf), 1, fpPakFile);
		if (buflen > 0)
			fwrite(buf, buflen, 1, fpPakFile);

		free(buf);
	}
	else {
		// Binary/CP437 file: copy verbatim in 64KB chunks.
		long lTemp;
		size_t CurByte, NumBytes;
		char FAR *Chunk;

		fseek(fpInput, 0, SEEK_END);
		lTemp = ftell(fpInput);
		if (lTemp < INT32_MIN || lTemp > INT32_MAX) {
			printf("File size %ld unsupported\n", lTemp);
			fclose(fpInput);
			return;
		}
		FileHeader.lFileSize = (int32_t)lTemp;

		if (fseek(fpInput, 0, SEEK_SET)) {
			puts("Failed to seek to start");
			fclose(fpInput);
			return;
		}

		strlcpy(FileHeader.szFileName, pszFileAlias,
		        sizeof(FileHeader.szFileName));
		FileHeader.lStart = lPakSize;
		FileHeader.lEnd = lPakSize + FileHeader.lFileSize;
		lPakSize += FileHeader.lFileSize;

		s_FileHeader_s(&FileHeader, fhbuf, sizeof(fhbuf));
		fwrite(fhbuf, sizeof(fhbuf), 1, fpPakFile);

		CurByte = 0;
		NumBytes = (size_t)FileHeader.lFileSize;

		Chunk = calloc(1, 64000);
		if (!Chunk) {
			fputs("out of memory!", stderr);
			fflush(stderr);
			fclose(fpInput);
			exit(1);
		}
		while ((int32_t)CurByte != FileHeader.lFileSize) {
			if (NumBytes > 64000) {
				if (fread(Chunk, 64000, 1, fpInput) != 1) {
					puts("Read error");
					break;
				}
				if (fwrite(Chunk, 64000, 1, fpPakFile) != 1) {
					puts("Write error");
					break;
				}
				NumBytes -= 64000;
				CurByte += 64000;
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
}
