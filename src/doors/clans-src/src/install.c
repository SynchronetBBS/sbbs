// ONE TO USE!!

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include "platform.h"

#include "console.h"
#include "defines.h"
#include "gum.h"
#include "parsing.h"
#include "u8cp437.h"
#include "video.h"

#ifndef __MSDOS__
#define far
#endif /* !__MSDOS__ */

#define STARTROW    9           // start at row 5

#define CODE1           0x7D
#define CODE2           0x1F

#define MAX_FILES   75          // say 75 files is most in .GUM file for now
#define MAX_FILENAME_LEN 13

#define SKIP        0           // skip if file exists
#define OVERWRITE   1           // always overwrite
#define QUERY       2           // ask user if he wishes to overwrite

#define ALWAYS  2

static void GetGumName(void);
static void ListFiles(void);
static int GetGUM(FILE *fpGUM);
static void install(void);
static bool FileExists(char *szFileName);
static void InitFiles(char *szFileName);
static int WriteType(char *szFileName);
static void Extract(char *szExtractFile, char *szNewName);
static void upgrade(void);

#ifdef __MSDOS__
char far *VideoMem;
long y_lookup[25];
#endif /* __MSDOS__ */
int Overwrite = false;
char szGumName[25], szIniName[25];
static int ini_is_utf8;

struct FileInfo {
	char *szFileName;
	int WriteType;
} FileInfo[MAX_FILES];

#ifdef __unix__
#define MKDIR(dir)              mkdir(dir,0777)
#else
#define MKDIR(dir)              mkdir(dir)
#endif

#ifndef __MSDOS__
static int _dos_setftime(int handle, unsigned short date, unsigned short time);
#endif /* !__MSDOS__ */

static void reset_attribute(void)
{
	textattr(7);
}

static void Display(char *szFileName)
{
	FILE *fpInput;
	char szString[300];
	bool FileFound = false, Done = false;
	int CurLine, lineno = 0;
	int is_utf8 = ini_is_utf8;
	const char *filename = szIniName;

	fpInput = fopen(szIniName, "r");
	if (!fpInput)   return;

	// search for filename
	for (;;) {
		if (!u8_fgets(szString, sizeof(szString), fpInput,
			      is_utf8, filename, &lineno))
			break;

		// get rid of \n and \r
		size_t len = strlen(szString);
		while (len > 0 && (szString[ len - 1] == '\n' || szString[ len - 1] == '\r')) {
			szString[ len - 1] = 0;
			len--;
		}

		if (szString[0] == ':' && strcasecmp(szFileName, &szString[1]) == 0) {
			FileFound = true;
			break;
		}
	}

	if (FileFound == false) {
		fclose(fpInput);

		// search dos for file
		fpInput = fopen(szFileName, "r");
		if (!fpInput) {
			zputs("|04File to display not found\n");
			return;
		}
		is_utf8 = 0;
		filename = szFileName;
		lineno = 0;
	}
	// print file
	CurLine = 0;
	while (!Done) {
		if (!u8_fgets(szString, sizeof(szString), fpInput,
			      is_utf8, filename, &lineno))
			break;

		if (szString[0] == ':')
			break;

		zputs(szString);
		CurLine++;

		if (CurLine == (ScreenLines - STARTROW - 1)) {
			CurLine = 0;
			zputs("[more]");
			if (toupper(cio_getch()) == 'Q')
				Done = true;
			zputs("\r       \r");
		}
	}

	fclose(fpInput);
}

static void detect_utf8_marker(void)
{
	FILE *fp;
	char line[32];

	fp = fopen(szIniName, "r");
	if (!fp)
		return;

	if (fgets(line, sizeof(line), fp)) {
		size_t len = strlen(line);
		while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
			line[--len] = '\0';
		if (strcasecmp(line, "# encoding: utf-8") == 0)
			ini_is_utf8 = 1;
	}
	fclose(fp);
}

static void SystemInit(void)
{
	Video_Init();
	GetGumName();
}

int main(int argc, char **argv)
{
	char szInput[46], szToken[46], szString[128], szToken2[20];

	if (argc != 2) {
		strlcpy(szIniName, "install.ini", sizeof(szIniName));
	}
	else {
		strlcpy(szIniName, argv[1], sizeof(szIniName));

		if (!strchr(szIniName, '.'))
			strlcat(szIniName, ".ini", sizeof(szIniName));
	}

	SystemInit();
	detect_utf8_marker();
	clrscr();

	gotoxy(0, 0);
	Display("TITLE");
	SetScrollRegion(STARTROW, INT_MAX);
	gotoxy(0, STARTROW);

	Display("WELCOME");

	for (;;) {
		zputs("|14> |07");

		// get input
		szInput[0] = 0;
		DosGetStr(szInput, 45, false);

		GetToken(szInput, szToken);

		if (szToken[0] == 0)
			continue;

		if (strcasecmp(szToken, "quit") == 0 || strcasecmp(szToken, "q") == 0)
			break;
		else if (strcasecmp(szToken, "install") == 0)
			install();
		else if (strcasecmp(szToken, "upgrade") == 0)
			upgrade();
		else if (strcasecmp(szToken, "read") == 0)
			Display(szInput);
		else if (strcasecmp(szToken, "list") == 0)
			ListFiles();
		else if (strcasecmp(szToken, "about") == 0)
			Display("About");
		else if (strcasecmp(szToken, "extract") == 0) {
			GetToken(szInput, szToken2);
			Extract(szToken2, szInput);
		}
		else if (strcasecmp(szToken, "cls") == 0) {
			ClearScrollRegion();
			gotoxy(0, STARTROW);
		}
		else if (szToken[0] == '?' || strcasecmp(szToken, "help") == 0)
			Display("Help");
		else {
			snprintf(szString, sizeof(szString), "|04%s not a valid command!\n", szToken);
			zputs(szString);
		}
	}

	ClearScrollRegion();
	gotoxy(0, STARTROW);
	Display("Goodbye");
	Video_Close();
	return(EXIT_SUCCESS);
}

static bool ReadFilename(FILE *fpGUM, char *szEncryptedName, size_t sz)
{
	char *pcTo;

	/* read in filename */
	memset(szEncryptedName, 0, sz);
	if (!fread(szEncryptedName, 1, 1, fpGUM))
		return false;
	if (szEncryptedName[0] == CODE1) {
		pcTo = szEncryptedName;
		do {
			if (!fread(pcTo, 1, 1, fpGUM))
				return false;
		} while(*(pcTo++) != CODE1);
	}
	else {
		if (!fread(&szEncryptedName[1], MAX_FILENAME_LEN - 1, sizeof(char), fpGUM))
			return false;
	}
	return true;
}

/*
 * Seeks forward by offset.
 * Since uint32_t may be the same size as a long, we need to be careful
 * and can't just case to a long.
 */
static int
fseekuc(FILE *fp, uint32_t offset)
{
#if LONG_MAX < UINT32_MAX
	while (offset > LONG_MAX) {
		int ret = fseek(fp, LONG_MAX, SEEK_CUR);
		if (ret)
			return ret;
		offset -= LONG_MAX;
	}
#endif
	if (offset) {
		int ret = fseek(fp, offset, SEEK_CUR);
		if (ret)
			return ret;
	}
	return 0;
}

static int GetGUM(FILE *fpGUM)
{
	char szEncryptedName[PATH_SIZE], cInput;
	char *pcFrom, *pcTo, szFileName[PATH_SIZE], szString[128];
	FILE *fpToFile;
	uint32_t lFileSize, lCompressSize;
	uint16_t date, time;

	ClearAll();

	/* read in filename */
	if (!ReadFilename(fpGUM, szEncryptedName, sizeof(szEncryptedName)))
		return false;

	/* then decrypt it */
	pcFrom = szEncryptedName;
	pcTo = szFileName;
	while (*pcFrom) {
		*pcTo = *pcFrom ^ CODE1;
		pcFrom++;
		pcTo++;
	}
	*pcTo = 0;
	snprintf(szString, sizeof(szString), "|14%-*s |06", MAX_FILENAME_LEN, szFileName);
	zputs(szString);

	// if dirname, makedir
	if (szFileName[0] == '/') {
		snprintf(szString, sizeof(szString), "dir |14%-*s\n", MAX_FILENAME_LEN, szFileName);
		zputs(szString);

		MKDIR(&szFileName[1]

			 );
		return true;
	}

	/* get filesize from psi file */
	if (fread(&lFileSize, sizeof(lFileSize), 1, fpGUM) == 0) {
		fclose(fpGUM);
		System_Error("|04Failed to read filesize!\n");
		return -1;
	}
	lFileSize = SWAP32(lFileSize);

	if (fread(&lCompressSize, sizeof(lCompressSize), 1, fpGUM) == 0) {
		fclose(fpGUM);
		System_Error("|04Failed to read compressed size!\n");
		return -1;
	}
	lCompressSize = SWAP32(lCompressSize);

	// get datestamp
	if (fread(&date, sizeof(date), 1, fpGUM) != 1) {
		fclose(fpGUM);
		System_Error("|04Failed to read datestamp!\n");
		return -1;
	}
	date = SWAP16(date);
	if (fread(&time, sizeof(date), 1, fpGUM) != 1) {
		fclose(fpGUM);
		System_Error("|04Failed to read timestamp!\n");
		return -1;
	}
	time = SWAP16(time);

	if (strcmp(szFileName, "UnixAttr.DAT") == 0) {
#ifdef __unix__
		unlink(szFileName);
#else
		fseekuc(fpGUM, lCompressSize);
		return 1;
#endif
	}

	// get type of input depending on WriteType
	if (FileExists(szFileName) && WriteType(szFileName) == OVERWRITE) {
		// continue -- overwrite file
		zputs("|07- updating - |06");
	}
	else if (WriteType(szFileName) == SKIP && FileExists(szFileName)) {
		// skip file
		zputs("|07- skipping (no update required)\n");
		fseekuc(fpGUM, lCompressSize);
		return 1;
	}
	// else, normal file, do normally
	// if file exists, ask user if he wants to overwrite it
	else if (FileExists(szFileName) && Overwrite != ALWAYS) {
		zputs("|03exists.  overwrite? (yes/no/rename/always) :");

		cInput = GetAnswer("YNAR");

		if (cInput == 'A') {
			zputs("Always\n");
			Overwrite = ALWAYS;
		}
		else if (cInput == 'R') {
			zputs("Rename\n|03enter new file name: ");
			DosGetStr(szFileName, MAX_FILENAME_LEN, false);
			zputs("\n");
		}
		else if (cInput == 'N') {
			zputs("No\n");
			// skip file
			fseekuc(fpGUM, lCompressSize);
			return 1;
		}
		else
			zputs("Yes\n");
	}

	/* open file to write to */
	fpToFile = fopen(szFileName, "wb");
	if (!fpToFile) {
		snprintf(szString, sizeof(szString), "|04Couldn't open %s\n", szFileName);
		zputs(szString);
		return false;
	}

	// === decode here
	decode(fpGUM, fpToFile, zputs);
	_dos_setftime(fileno(fpToFile), date, time);

	fclose(fpToFile);

	snprintf(szString, sizeof(szString), "%" PRIu32 "b ", lFileSize);
	zputs(szString);
	zputs("|15done.\n");

	return true;
}

static void install(void)
{
	FILE *fpGUM;
#ifdef __unix__
	FILE *fpAttr;
	mode_t  tMode;
	char szFileName[PATH_SIZE];
	unsigned tmp;
#endif
	char cInput;

	Display("Install");

	// make sure he wants to
	zputs("\n|07Are you sure you wish to run the install now? (y/n):");
	cInput = GetAnswer("YN");

	if (cInput == 'N') {
		zputs("No\n|04Installation aborted.\n");
		return;
	}
	else
		zputs("Yes\n");

	InitFiles("INSTALL.FILES");
	Overwrite = false;

	fpGUM = fopen(szGumName, "rb");
	if (!fpGUM) {
		printf("Can't find -%s-\n",szGumName);
		zputs("|04No .GUM to blowup!\n");
		return;
	}

	while (GetGUM(fpGUM))
		;

#ifdef __unix__
	fpAttr = fopen("UnixAttr.DAT", "r");
	if (fpAttr) {
		while (fscanf(fpAttr, "%o %s\n", &tmp, szFileName) != EOF) {
			tMode = (mode_t)tmp;
			chmod(szFileName, tMode);
		}
		fclose(fpAttr);
		unlink("UnixAttr.DAT");
	}
#endif

	fclose(fpGUM);

	Display("InstallDone");
}

static void upgrade(void)
{
	FILE *fpGUM;
#ifdef __unix__
	FILE *fpAttr;
	mode_t  tMode;
	char szFileName[MAX_FILENAME_LEN];
	unsigned tmp;
#endif
	char cInput;

	Display("Upgrade");

	// make sure he wants to
	zputs("\n|07Are you sure you wish to upgrade? (y/n):");
	cInput = GetAnswer("YN");

	if (cInput == 'N') {
		zputs("No\n|04Upgrade aborted.\n");
		return;
	}
	else
		zputs("Yes\n");

	InitFiles("UPGRADE.FILES");
	Overwrite = false;

	fpGUM = fopen(szGumName, "rb");
	if (!fpGUM) {
		zputs("|04No .GUM to blowup!\n");
		return;
	}

	while (GetGUM(fpGUM))
		;

#ifdef __unix__
	fpAttr = fopen("UnixAttr.DAT", "r");
	if (fpAttr) {
		while (fscanf(fpAttr, "%o %s\n", &tmp, szFileName) != EOF) {
			tMode = (mode_t)tmp;
			chmod(szFileName, tMode);
		}
		fclose(fpAttr);
		unlink("UnixAttr.DAT");
	}
#endif

	fclose(fpGUM);

	Display("UpgradeDone");
}

static bool FileExists(char *szFileName)
{
	FILE *fp;

	fp = fopen(szFileName, "r");
	if (!fp) {
		return false;
	}

	fclose(fp);
	return true;
}

static void FreeFiles(void)
{
	for (size_t i = 0; i < sizeof(FileInfo) / sizeof(*FileInfo); i++) {
		free(FileInfo[i].szFileName);
		FileInfo[i].szFileName = NULL;
		FileInfo[i].WriteType = QUERY;
	}
}

static void InitFiles(char *szFileName)
{
	// init files to be installed

	FILE *fpInput;
	char szString[PATH_SIZE + 2];
	bool FileFound = false, Done = false;
	int CurFile, lineno = 0;

	FreeFiles();

	fpInput = fopen(szIniName, "r");
	if (!fpInput) {
		zputs("|06usage:   |14install <inifile>\n");
		reset_attribute();
		Video_Close();
		exit(EXIT_FAILURE);
		return;
	}

	// search for filename
	for (;;) {
		if (!u8_fgets(szString, sizeof(szString), fpInput,
			      ini_is_utf8, szIniName, &lineno))
			break;

		// get rid of \n and \r
		while (szString[ strlen(szString) - 1] == '\n' || szString[ strlen(szString) - 1] == '\r')
			szString[ strlen(szString) - 1] = 0;

		if (szString[0] == ':' && strcasecmp(szFileName, &szString[1]) == 0) {
			FileFound = true;
			break;
		}
	}

	if (FileFound == false) {
		fclose(fpInput);
		return;
	}

	// found file list, retrieve filenames and file types
	CurFile = 0;
	while (!Done && CurFile < MAX_FILES) {
		if (!u8_fgets(szString, 128, fpInput,
			      ini_is_utf8, szIniName, &lineno))
			break;

		// get rid of \n and \r
		while (szString[ strlen(szString) - 1] == '\n' || szString[ strlen(szString) - 1] == '\r')
			szString[ strlen(szString) - 1] = 0;

		if (szString[0] == ':')
			break;

		// found another filename, copy it over
		// x filename
		// 0123456789...
		FileInfo[CurFile].szFileName = strdup(&szString[2]);
		FileInfo[CurFile].WriteType = QUERY;

		if (szString[0] == 'o')
			FileInfo[CurFile].WriteType = OVERWRITE;
		else if (szString[0] == 's')
			FileInfo[CurFile].WriteType = SKIP;

		CurFile++;
	}

	fclose(fpInput);
}

/*
 * Recursive, greedy, stupid wildcard matching
 */
static bool Match(const char *re, const char *Candidate)
{
	while (*re) {
		switch (*re) {
			case '?':
				if (!*Candidate)
					return false;
				re++;
				Candidate++;
				break;
			case '*':
				re++;
				for (int remain = (int)strlen(Candidate); remain >= 0; remain--) {
					if (Match(re, &Candidate[remain]))
						return true;
				}
				return false;
			default:
				if (!*Candidate)
					return false;
				if (tolower(*re) != tolower(*Candidate))
					return false;
				re++;
				Candidate++;
				break;
		}
	}
	if (*re == 0 && *Candidate == 0)
		return true;
	return false;
}

static int WriteType(char *szFileName)
{
	int CurFile;

	// search for file
	for (CurFile = 0; CurFile < MAX_FILES; CurFile++) {
		if (FileInfo[CurFile].szFileName) {
			if (Match(FileInfo[CurFile].szFileName, szFileName)) {
				// found file
				return (FileInfo[CurFile].WriteType);
			}
		}
	}

	// if not found, return query
	return QUERY;
}

static void Extract(char *szExtractFile, char *szNewName)
{
	char szEncryptedName[PATH_SIZE], cInput;
	char *pcFrom, *pcTo, szFileName[PATH_SIZE], szString[128];
	FILE *fpToFile, *fpGUM;
	uint32_t lFileSize, lCompressSize;
	uint16_t date, time;

	ClearAll();

	fpGUM = fopen(szGumName, "rb");
	if (!fpGUM) {
		zputs("|04No .GUM to blowup!\n");
		return;
	}

	for (;;) {
		/* read in filename */
		if (!ReadFilename(fpGUM, szEncryptedName, sizeof(szEncryptedName))) {
			zputs("|04file not found!\n");
			fclose(fpGUM);
			return;
		}

		/* then decrypt it */
		pcFrom = szEncryptedName;
		pcTo = szFileName;
		while (*pcFrom) {
			*pcTo = *pcFrom ^ CODE1;
			pcFrom++;
			pcTo++;
		}
		*pcTo = 0;

		// file, do following
		if (szFileName[0] != '/') {
			/* get filesize from psi file */
			if (fread(&lFileSize, sizeof(lFileSize), 1, fpGUM) != 1) {
				zputs("|04Reading file size failed!\n");
				fclose(fpGUM);
				return;
			}
			lFileSize = SWAP32(lFileSize);

			if (fread(&lCompressSize, sizeof(lCompressSize), 1, fpGUM) != 1) {
				zputs("|04Reading compressed size failed!\n");
				fclose(fpGUM);
				return;
			}
			lCompressSize = SWAP32(lCompressSize);

			// get datestamp
			if (fread(&date, sizeof(date), 1, fpGUM) != 1) {
				zputs("|04Reading datestamp failed!\n");
				fclose(fpGUM);
				return;
			}
			date = SWAP16(date);
			if (fread(&time, sizeof(time), 1, fpGUM) != 1) {
				zputs("|04Reading timestamp failed!\n");
				fclose(fpGUM);
				return;
			}

			time = SWAP16(time);
		}
		else {
			// was a dir
			lFileSize = 0;
			lCompressSize = 0;
		}

		// same file? -- if so, keep going, if not, skip
		if (strcasecmp(szExtractFile, szFileName) == 0) {
			break;
		}
		else {
			// skip it
			if (fseekuc(fpGUM, lCompressSize) != 1) {
				zputs("|04Seek failed!\n");
				fclose(fpGUM);
				return;
			}
		}
	}

	if (*szNewName != 0)
		strlcpy(szFileName, szNewName, sizeof(szFileName));

	// if dirname, makedir
	if (szFileName[0] == '/') {
		snprintf(szString, sizeof(szString), "|14%-*s\n", MAX_FILENAME_LEN, szFileName);
		zputs(szString);

		MKDIR(&szFileName[1]);
		fclose(fpGUM);
		return;
	}

	if (FileExists(szFileName)) {
		zputs("|03exists.  overwrite? (yes/no/rename) :");

		cInput = GetAnswer("YNR");

		if (cInput == 'R') {
			zputs("Rename\n|03enter new file name: ");
			DosGetStr(szFileName, MAX_FILENAME_LEN, false);
		}
		else if (cInput == 'N') {
			zputs("No\n");
			// skip file
			if (fseekuc(fpGUM, lCompressSize) != 1) {
				zputs("|04Seek failed!\n");
				fclose(fpGUM);
				return;
			}
			fclose(fpGUM);
			return;
		}
		else
			zputs("Yes\n");
	}

	/* open file to write to */
	fpToFile = fopen(szFileName, "wb");
	if (!fpToFile) {
		fclose(fpGUM);
		snprintf(szString, sizeof(szString), "|04Couldn't write to %s\n", szFileName);
		zputs(szString);
		return;
	}

	snprintf(szString, sizeof(szString), "|06Extracting %s\n", szFileName);
	zputs(szString);

	//== decode it here
	decode(fpGUM, fpToFile, zputs);

	_dos_setftime(fileno(fpToFile), date, time);
	fclose(fpToFile);
	fclose(fpGUM);

	snprintf(szString, sizeof(szString), "(%" PRIu32 " bytes) ", lFileSize);
	zputs(szString);
	zputs("|15Done.\n");
}

static int CheckRow(int row, bool *Done)
{
	if (row >= ScreenLines - STARTROW - 1) {
		zputs("[more]");
		if (toupper(cio_getch()) == 'Q')
			*Done = true;
		zputs("\r       \r");
		return Done ? -1 : 0;
	}
	return row;
}

struct lfn {
	struct lfn *next;
	char *name;
	uint32_t size;
};

struct lfn *LFNHead = NULL;
struct lfn *LFNTail = NULL;
size_t LongestLFN = 0;

static void AddLFN(const char *name, uint32_t size)
{
	struct lfn *n = malloc(sizeof(struct lfn));
	size_t len = strlen(name);
	if (len > LongestLFN)
		LongestLFN = len;
	if (n == NULL)
		System_Error("|06Out of memory\n");
	n->name = strdup(name);
	if (n->name == NULL)
		System_Error("|06Out of memory\n");
	n->size = size;
	n->next = NULL;
	if (LFNHead == NULL) {
		LFNHead = n;
		LFNTail = n;
	}
	else {
		LFNTail->next = n;
		LFNTail = n;
	}
}

static void FreeLFNs(void)
{
	struct lfn *next = NULL;
	for (struct lfn *fn = LFNHead; fn; fn = next) {
		next = fn->next;
		free(fn->name);
		free(fn);
	}
	LFNHead = NULL;
	LFNTail = NULL;
}

static void DisplayLFNs(int row, int column, size_t TotalBytes)
{
	bool Done = false;
	char szString[PATH_SIZE + 32];
	if (column)
		zputs("\n");
	for (struct lfn *fn = LFNHead; fn && !Done; fn = fn->next) {
		if (fn->size == UINT32_MAX) {
			snprintf(szString, sizeof(szString), "|15%*s  |06-- |07      directory\n",
			    (int)LongestLFN, fn->name);
		}
		else {
			snprintf(szString, sizeof(szString), "|15%*s  |06-- |07%9" PRIu32 " bytes\n",
			    (int)LongestLFN, fn->name, fn->size);
			TotalBytes += (size_t)fn->size;
		}
		zputs(szString);
		row++;
		row = CheckRow(row, &Done);
	}
	if (!Done) {
		zputs("\n");
		CheckRow(row, &Done);
		snprintf(szString, sizeof(szString), "|14%zu total bytes\n\n", TotalBytes);
		zputs(szString);
	}
	FreeLFNs();
}

static void ListFiles(void)
{
	char szEncryptedName[PATH_SIZE];
	char *pcFrom, *pcTo, szFileName[PATH_SIZE], szString[128];
	FILE *fpGUM;
	uint32_t lFileSize, lCompressSize;
	size_t TotalBytes = 0;
	int FilesFound = 0;
	uint16_t date, time;
	bool Done = false;
	const int columns = ScreenWidth / 36;
	int column = 0;
	int row = 0;

	fpGUM = fopen(szGumName, "rb");
	if (!fpGUM) {
		zputs("|04No .GUM to blowup!\n");
		return;
	}

	while (!Done) {
		/* read in filename */
		if (!ReadFilename(fpGUM, szEncryptedName, sizeof(szEncryptedName))) {
			// done
			DisplayLFNs(row, column, TotalBytes);
			break;
		}

		/* then decrypt it */
		pcFrom = szEncryptedName;
		pcTo = szFileName;
		while (*pcFrom) {
			*pcTo = *pcFrom ^ CODE1;
			pcFrom++;
			pcTo++;
		}
		*pcTo = 0;

		if (szFileName[0] == '/') {
			if (strlen(szFileName) > 14) {
				AddLFN(szFileName, UINT32_MAX);
				continue;
			}
			snprintf(szString, sizeof(szString), "|15%14s  |06-- |07      directory  ",
					szFileName);
			zputs(szString);

			lCompressSize = 0;
			lFileSize = 0;
		}
		else {
			/* get filesize from psi file */
			if (fread(&lFileSize, sizeof(lFileSize), 1, fpGUM) == 0) {
				fclose(fpGUM);
				System_Error("|04Failed to read file size!\n");
				return;
			}
			lFileSize = SWAP32(lFileSize);
			if (fread(&lCompressSize, sizeof(lCompressSize), 1, fpGUM) != 1) {
				fclose(fpGUM);
				System_Error("|04Failed to read compressed size!\n");
				return;
			}
			lCompressSize = SWAP32(lCompressSize);
			// get datestamp
			if (fread(&date, sizeof(date), 1, fpGUM) != 1) {
				fclose(fpGUM);
				System_Error("|04Failed to read datestamp!\n");
				return;
			}
			date = SWAP16(date);
			if (fread(&time, sizeof(time), 1, fpGUM) != 1) {
				fclose(fpGUM);
				System_Error("|04Failed to read timestamp!\n");
				return;
			}
			time = SWAP16(time);
			if (strcmp(szFileName, "UnixAttr.DAT") == 0) {
				if (fseekuc(fpGUM, lCompressSize)) {
					fclose(fpGUM);
					System_Error("|04Failed to skip UnixAttr.DAT!\n");
					return;
				}
				continue;
			}
			if (strlen(szFileName) > 14) {
				AddLFN(szFileName, lFileSize);
				if (fseekuc(fpGUM, lCompressSize)) {
					fclose(fpGUM);
					System_Error("|04Failed to skip file contents!\n");
					return;
				}
				continue;
			}

			// show dir like DOS :)
			snprintf(szString, sizeof(szString), "|15%14s  |06-- |07%9" PRIu32 " bytes  ",
					szFileName, lFileSize);
			zputs(szString);
		}
		FilesFound++;
		column++;

		if (column >= columns) {
			zputs("\n");
			column = 0;
			row++;
		}

		row = CheckRow(row, &Done);

		fseekuc(fpGUM, lCompressSize);

		TotalBytes += (size_t)lFileSize;
	}
	FreeLFNs();

	if ((FilesFound % 2) != 0)
		zputs("\n");

	fclose(fpGUM);
}

static void GetGumName(void)
{
	// init files to be installed
	FILE *fpInput;
	char szString[PATH_SIZE + 1];
	int lineno = 0;

	FreeFiles();

	fpInput = fopen(szIniName, "r");
	if (!fpInput) {
		// no ini file
		zputs("|06usage:   |14install <inifile>\n");
		reset_attribute();
		Video_Close();
		exit(EXIT_FAILURE);
	}

	// search for filename
	for (;;) {
		if (!u8_fgets(szString, sizeof(szString), fpInput,
			      ini_is_utf8, szIniName, &lineno)) {
			zputs("\n|12Couldn't get GUM filename\n");
			reset_attribute();
			Video_Close();
			exit(EXIT_FAILURE);
			break;
		}

		// get rid of \n and \r
		while (szString[ strlen(szString) - 1] == '\n' || szString[ strlen(szString) - 1] == '\r')
			szString[ strlen(szString) - 1] = 0;

		if (szString[0] == '!') {
			strlcpy(szGumName, &szString[1], sizeof(szGumName));
			break;
		}
	}

	fclose(fpInput);
}

#ifndef __MSDOS__
static int _dos_setftime(int handle, unsigned short date, unsigned short time)
{
	struct tm dos_dt;
	time_t file_dt;
#ifdef _WIN32
	struct _utimbuf tm_buf;
#elif defined(__unix__)
	struct timespec tmv_buf[2];
#endif

	memset(&dos_dt, 0, sizeof(struct tm));
	dos_dt.tm_year = ((date & 0xfe00) >> 9) + 80;
	dos_dt.tm_mon  = ((date & 0x01e0) >> 5) + 1;
	dos_dt.tm_mday = (date & 0x001f);
	dos_dt.tm_hour = (time & 0xf800) >> 11;
	dos_dt.tm_min  = (time & 0x07e0) >> 5;
	dos_dt.tm_sec  = (time & 0x001f) * 2;

	file_dt = mktime(&dos_dt);

#ifdef _WIN32
	tm_buf.actime = tm_buf.modtime = file_dt;
	return (_futime(handle, &tm_buf));
#elif defined(__unix__)
	tmv_buf[0].tv_sec = file_dt;
	tmv_buf[0].tv_nsec = file_dt * 1000000;
	tmv_buf[1].tv_sec = file_dt;
	tmv_buf[1].tv_nsec = file_dt * 1000000;
	return (futimens(handle, tmv_buf));
#else
#error "_dos_setftime needs a setting function, compilation aborted"
#endif
}
#endif
