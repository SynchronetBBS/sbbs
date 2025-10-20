/* Chew -- Program to make .GUM encrypted files */

/*
 * Compression code taken from SIXPACK.C by Philip G. Gage.
 * For original source code either do a search for sixpack.c
 * or go here:
 *
 * http://www.ifi.uio.no/in383/src/ddj/sixpack/
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "win_wrappers.h"

#include "defines.h"
#include "gum.h"
#include "parsing.h"
#include "structs.h"

#define MAX_TOKEN_CHARS 32
#define CODE1           0x7D
#define CODE2           0x1F

#define MAX_FILENAME_LEN 13

#ifndef __MSDOS__
static unsigned _dos_getftime(int, uint16_t *, uint16_t *);
#endif
static void AddGUM(FILE *fpGUM, char *pszFileName);
static void AddDir(FILE *fpGUM, char *pszDirName);

static long TotalBytes = 0;

int main(int argc, char **argv)
{
	FILE *fpGUM;
	FILE *fpList;
#ifdef __unix__
	FILE *fpAttr;
	struct stat tStat;
#endif
	char szLine[255], szFileName[PATH_SIZE], szGumName[PATH_SIZE], szFileList[PATH_SIZE];

	if (argc == 3) {
		strlcpy(szGumName, argv[1], sizeof(szGumName));
		strlcpy(szFileList, argv[2], sizeof(szFileList));
	}
	else {
		strlcpy(szGumName, "archive.gum", sizeof(szGumName));
		strlcpy(szFileList, "files.lst", sizeof(szFileList));
	}

	//initialize();

	printf("CHEW v0.01\n\nChewing to %s\n\n", szGumName);

	fpGUM = fopen(szGumName, "wb");
	if (!fpGUM) {
		printf("Couldn't create %s file!\n", szGumName);
		return(0);
	}

	fpList = fopen(szFileList, "r");
	if (!fpList) {
		fclose(fpGUM);
		printf("Couldn't read listing file: %s\n", szFileList);
		return(0);
	}

#ifdef __unix__
	fpAttr = fopen("UnixAttr.DAT", "wb");
	if (!fpAttr) {
		printf("Couldn't create UnixAttr.DAT file\n");
		return(0);
	}
#endif

	for (;;) {
		if (fgets(szLine, 255, fpList) == NULL)
			break;

		GetToken(szLine, szFileName);

		if (szFileName[0] == 0 || szFileName[0] == '#')
			continue;

		if (szFileName[0] == '/') {
			// directory to create
			AddDir(fpGUM, szFileName);
#ifdef __unix__
			stat(szFileName, &tStat);
			tStat.st_mode&=511;
			fprintf(fpAttr, "%o %s\n", tStat.st_mode, szFileName+1);
#endif
		}
		else {
			AddGUM(fpGUM, szFileName);
#ifdef __unix__
			stat(szFileName, &tStat);
			tStat.st_mode&=511;
			fprintf(fpAttr, "%o %s\n", tStat.st_mode, szFileName);
#endif
		}
	}

#ifdef __unix__
	fclose(fpAttr);
	AddGUM(fpGUM, "UnixAttr.DAT");
	unlink("UnixAttr.DAT");
#endif

	fclose(fpGUM);
	fclose(fpList);

	//printf("output is %ld bytes\n", TotalBytes);
	return(0);
}

static void AddGUM(FILE *fpGUM, char *pszFileName)
{
	char szKey[80];
	/*    char acChunk[1024];
	    char acEncryptedChunk[1024]; */
	char szEncryptedName[MAX_FILENAME_LEN];
	char *pcFrom, *pcTo;
	FILE *fpFromFile;
	int32_t lFileSize, Offset1, Offset2;
	int32_t lCompressSize;
	int32_t tmp32;
	int /*BytesRead,*/ iTemp;
	uint16_t date, time;
	uint16_t tmp16;

	ClearAll();

	for (iTemp = 0; iTemp < MAX_FILENAME_LEN; iTemp++)
		szEncryptedName[iTemp] = 0;

	/* encrypt the filename */
	pcFrom = pszFileName;
	pcTo = szEncryptedName;

	printf("Adding file: %s ", pszFileName);

	while (*pcFrom) {
		*pcTo = *pcFrom ^ CODE1;
		pcFrom++;
		pcTo++;
	}
	*pcTo = 0;
	//printf("Encrypted name = %s\n", szEncryptedName);

	/* make key using filename */
	snprintf(szKey, sizeof(szKey), "%s%x%x", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);
	//printf("key = '%s%x%x'\n", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);

	pcTo = szKey;
	while (*pcTo) {
		*pcTo ^= CODE2;
		pcTo++;
	}


	/* write it to file */
	fwrite(&szEncryptedName, sizeof(szEncryptedName), sizeof(char), fpGUM);

	/* write filesize to psi file */
	fpFromFile = fopen(pszFileName, "rb");
	if (!fpFromFile) {
		printf("\aCouldn't open \"%s\"\n", pszFileName);
		exit(0);
	}
	fseek(fpFromFile, 0L, SEEK_END);
	lFileSize = ftell(fpFromFile);
	//printf(" (%ld bytes) ", lFileSize);
	fseek(fpFromFile, 0L, SEEK_SET);
	tmp32 = SWAP32(lFileSize);
	fwrite(&tmp32, sizeof(tmp32), 1, fpGUM);

	// record this offset since we come back later to write the compressed
	// size
	Offset1 = ftell(fpGUM);
	tmp32 = 0;
	fwrite(&tmp32, sizeof(tmp32), 1, fpGUM);

	// write datestamp
	_dos_getftime(fileno(fpFromFile), &date, &time);
	tmp16 = SWAP16(date);
	fwrite(&tmp16, sizeof(tmp16), 1, fpGUM);
	tmp16 = SWAP16(time);
	fwrite(&tmp16, sizeof(tmp16), 1, fpGUM);

	//=== encode here
	encode(fpFromFile, fpGUM);
	//printf("Packed from %ld bytes to %ld bytes\n",bytes_in,bytes_out);
	TotalBytes += bytes_out;
	//=== end here

	// we write the compressed size now
	Offset2 = ftell(fpGUM);
	fseek(fpGUM, Offset1, SEEK_SET);
	lCompressSize = bytes_out;
	tmp32 = SWAP32(lCompressSize);
	fwrite(&tmp32, sizeof(tmp32), 1, fpGUM);
	fseek(fpGUM, Offset2, SEEK_SET);

	bytes_in = 0;
	bytes_out = 0;

	fclose(fpFromFile);
	printf("Done.\n");

}

static void AddDir(FILE *fpGUM, char *pszDirName)
{
	char szKey[80];
	char szEncryptedName[MAX_FILENAME_LEN];
	char *pcFrom, *pcTo;
	int iTemp;

	for (iTemp = 0; iTemp < MAX_FILENAME_LEN; iTemp++)
		szEncryptedName[iTemp] = 0;

	/* encrypt the filename */
	pcFrom = pszDirName;
	pcTo = szEncryptedName;

	printf("Adding directory: %s\n", pszDirName);

	while (*pcFrom) {
		*pcTo = *pcFrom ^ CODE1;
		pcFrom++;
		pcTo++;
	}
	*pcTo = 0;
	//printf("Encrypted name = %s\n", szEncryptedName);

	/* make key using filename */
	snprintf(szKey, sizeof(szKey), "%s%x%x", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);
	//printf("key = '%s%x%x'\n", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);

	pcTo = szKey;
	while (*pcTo) {
		*pcTo ^= CODE2;
		pcTo++;
	}

	// write dir name to file as is
	fwrite(szEncryptedName, sizeof(szEncryptedName), 1, fpGUM);
}

#ifndef __MSDOS__
static unsigned _dos_getftime(int handle, uint16_t *datep, uint16_t *timep)
{
	struct stat file_stats;
	struct tm *file_datetime;
	struct tm file_dt;

	if (fstat(handle, &file_stats) != 0) {
		*datep = (1 << 5) | 1;
		*timep = 0;
		return EBADF;
		fputs("fstat failed", stderr);
		fflush(stderr);
		exit(1);
	}

	file_datetime = localtime(&file_stats.st_mtime);
	if (!file_datetime) {
		fputs("localtime failed", stderr);
		fflush(stderr);
		exit(1);
	}
	memcpy(&file_dt, file_datetime, sizeof(struct tm));

	*datep = 0;
	*datep = ((file_dt.tm_mday) & 0x1f);
	*datep |= ((file_dt.tm_mon + 1) & 0x0f) << 5;
	*datep |= ((file_dt.tm_year - 80) & 0x7f) << 9;

	*timep = 0;
	*timep = (((file_dt.tm_sec + 2) / 2) & 0x1f);
	*timep |= ((file_dt.tm_min) & 0x3f) << 5;
	*timep |= ((file_dt.tm_hour) & 0x1f) << 11;

	return 0;
}
#endif /* !__MSDOS__ */
