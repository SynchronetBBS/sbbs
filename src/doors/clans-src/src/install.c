// ONE TO USE!!

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef __MSDOS__
# include <dos.h>
#elif defined(_WIN32)
# include <windows.h>
# include <sys/utime.h>
# include <direct.h> /* mkdir */
#elif defined(__unix__)
# include <sys/time.h>
# include <unistd.h>
#endif /* __MSDOS__ */
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include "unix_wrappers.h"
#include "win_wrappers.h"

#include "console.h"
#include "cmdline.h"
#include "defines.h"
#include "gum.h"
#include "parsing.h"
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
static unsigned short _dos_setftime(int, unsigned short, unsigned short);
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
	int CurLine;

	fpInput = fopen(szIniName, "r");
	if (!fpInput)   return;

	// search for filename
	for (;;) {
		if (!fgets(szString, sizeof(szString), fpInput))
			break;

		// get rid of \n and \r
		while (szString[ strlen(szString) - 1] == '\n' || szString[ strlen(szString) - 1] == '\r')
			szString[ strlen(szString) - 1] = 0;

		if (szString[0] == ':' && stricmp(szFileName, &szString[1]) == 0) {
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
	}
	// print file
	CurLine = 0;
	while (!Done) {
		if (!fgets(szString, sizeof(szString), fpInput))
			break;

		if (szString[0] == ':')
			break;

		zputs(szString);
		CurLine++;

		if (CurLine == (ScreenLines - STARTROW - 1)) {
			CurLine = 0;
			zputs("[more]");
			if (toupper(getch()) == 'Q')
				Done = true;
			zputs("\r       \r");
		}
	}

	fclose(fpInput);
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

		if (stricmp(szToken, "quit") == 0 || stricmp(szToken, "q") == 0)
			break;
		else if (stricmp(szToken, "install") == 0)
			install();
		else if (stricmp(szToken, "upgrade") == 0)
			upgrade();
		else if (stricmp(szToken, "read") == 0)
			Display(szInput);
		else if (stricmp(szToken, "list") == 0)
			ListFiles();
		else if (stricmp(szToken, "about") == 0)
			Display("About");
		else if (stricmp(szToken, "extract") == 0) {
			GetToken(szInput, szToken2);
			Extract(szToken2, szInput);
		}
		else if (stricmp(szToken, "cls") == 0) {
			ClearScrollRegion();
			gotoxy(0, STARTROW);
		}
		else if (szToken[0] == '?' || stricmp(szToken, "help") == 0)
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
	fread(&lFileSize, sizeof(lFileSize), 1, fpGUM);
	lFileSize = SWAP32(lFileSize);

	fread(&lCompressSize, sizeof(lCompressSize), 1, fpGUM);
	lCompressSize = SWAP32(lCompressSize);

	// get datestamp
	fread(&date, sizeof(date), 1, fpGUM);
	date = SWAP16(date);
	fread(&time, sizeof(date), 1, fpGUM);
	time = SWAP16(time);

	if (strcmp(szFileName, "UnixAttr.DAT") == 0)
		unlink(szFileName);

	// get type of input depending on WriteType
	if (FileExists(szFileName) && WriteType(szFileName) == OVERWRITE) {
		// continue -- overwrite file
		zputs("|07- updating - |06");
	}
	else if (WriteType(szFileName) == SKIP && FileExists(szFileName)) {
		// skip file
		zputs("|07- skipping (no update required)\n");
		fseek(fpGUM, lCompressSize, SEEK_CUR);
		return 1;
	}
	// else, normal file, do normally
	// if file exists, ask user if he wants to overwrite it
	else if (FileExists(szFileName) && Overwrite != ALWAYS) {
		zputs("|03exists.  overwrite? (yes/no/rename/always) :");

		cInput = get_answer("YNAR");

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
			fseek(fpGUM, lCompressSize, SEEK_CUR);
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

	fclose(fpToFile);

	fpToFile = fopen(szFileName, "r+b");
	_dos_setftime(fileno(fpToFile), date, time);
	fclose(fpToFile);



	snprintf(szString, sizeof(szString), "%" PRId32 "b ", lFileSize);
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
	cInput = get_answer("YN");

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
			tMode = tmp;
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
	cInput = get_answer("YN");

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
			tMode = tmp;
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
	for (int i = 0; i < sizeof(FileInfo) / sizeof(*FileInfo); i++) {
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
	int CurFile;

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
		if (!fgets(szString, sizeof(szString), fpInput))
			break;

		// get rid of \n and \r
		while (szString[ strlen(szString) - 1] == '\n' || szString[ strlen(szString) - 1] == '\r')
			szString[ strlen(szString) - 1] = 0;

		if (szString[0] == ':' && stricmp(szFileName, &szString[1]) == 0) {
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
		if (!fgets(szString, 128, fpInput))
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
				for (int remain = strlen(Candidate); remain >= 0; remain--) {
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
	int32_t lFileSize, lCompressSize;
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
			fread(&lFileSize, sizeof(lFileSize), 1, fpGUM);
			lFileSize = SWAP32(lFileSize);

			fread(&lCompressSize, sizeof(lCompressSize), 1, fpGUM);
			lCompressSize = SWAP32(lCompressSize);

			// get datestamp
			fread(&date, sizeof(date), 1, fpGUM);
			date = SWAP16(date);
			fread(&time, sizeof(time), 1, fpGUM);
			time = SWAP16(time);
		}
		else {
			// was a dir
			lFileSize = 0;
			lCompressSize = 0;
		}

		// same file? -- if so, keep going, if not, skip
		if (stricmp(szExtractFile, szFileName) == 0) {
			break;
		}
		else {
			// skip it
			fseek(fpGUM, lCompressSize, SEEK_CUR);
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

		cInput = get_answer("YNR");

		if (cInput == 'R') {
			zputs("Rename\n|03enter new file name: ");
			DosGetStr(szFileName, MAX_FILENAME_LEN, false);
		}
		else if (cInput == 'N') {
			zputs("No\n");
			// skip file
			fseek(fpGUM, lCompressSize, SEEK_CUR);
			fclose(fpGUM);
			return;
		}
		else
			zputs("Yes\n");
	}

	/* open file to write to */
	fpToFile = fopen(szFileName, "wb");
	if (!fpToFile) {
		snprintf(szString, sizeof(szString), "|04Couldn't write to %s\n", szFileName);
		zputs(szString);
		return;
	}

	snprintf(szString, sizeof(szString), "|06Extracting %s\n", szFileName);
	zputs(szString);

	//== decode it here
	decode(fpGUM, fpToFile, zputs);

	fclose(fpToFile);
	fclose(fpGUM);

	fpToFile = fopen(szFileName, "r+b");
	_dos_setftime(fileno(fpToFile), date, time);
	fclose(fpToFile);

	snprintf(szString, sizeof(szString), "(%" PRId32 " bytes) ", lFileSize);
	zputs(szString);
	zputs("|15Done.\n");
}

int CheckRow(int row, bool *Done)
{
	if (row >= ScreenLines - STARTROW - 1) {
		zputs("[more]");
		if (toupper(getch()) == 'Q')
			*Done = true;
		zputs("\r       \r");
		return Done ? -1 : 0;
	}
	return row;
}

struct lfn {
	struct lfn *next;
	char *name;
	int32_t size;
};

struct lfn *LFNHead = NULL;
struct lfn *LFNTail = NULL;
size_t LongestLFN = 0;

static void AddLFN(const char *name, int32_t size)
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
	LFNTail->next = n;
	LFNTail = n;
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

static void DisplayLFNs(int row, int column, uint32_t TotalBytes)
{
	bool Done = false;
	char szString[PATH_SIZE + 32];
	if (column)
		zputs("\n");
	for (struct lfn *fn = LFNHead; fn && !Done; fn = fn->next) {
		if (fn->size == -1) {
			snprintf(szString, sizeof(szString), "|15%*s  |06-- |07      directory\n",
			    (int)LongestLFN, fn->name);
		}
		else {
			snprintf(szString, sizeof(szString), "|15%*s  |06-- |07%9" PRId32 " bytes\n",
			    (int)LongestLFN, fn->name, fn->size);
			TotalBytes += fn->size;
		}
		zputs(szString);
		row++;
		row = CheckRow(row, &Done);
	}
	if (!Done) {
		zputs("\n");
		row = CheckRow(row, &Done);
		snprintf(szString, sizeof(szString), "|14%" PRId32 " total bytes\n\n", TotalBytes);
		zputs(szString);
	}
	FreeLFNs();
}

static void ListFiles(void)
{
	char szEncryptedName[PATH_SIZE];
	char *pcFrom, *pcTo, szFileName[PATH_SIZE], szString[128];
	FILE *fpGUM;
	int32_t lFileSize, TotalBytes = 0, lCompressSize;
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
			fclose(fpGUM);
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
				AddLFN(szFileName, -1);
				continue;
			}
			snprintf(szString, sizeof(szString), "|15%14s  |06-- |07      directory  ",
					szFileName);
			zputs(szString);

			lCompressSize = 0L;
			lFileSize = 0L;
		}
		else {
			/* get filesize from psi file */
			fread(&lFileSize, sizeof(lFileSize), 1, fpGUM);
			lFileSize = SWAP32(lFileSize);
			fread(&lCompressSize, sizeof(lCompressSize), 1, fpGUM);
			lCompressSize = SWAP32(lCompressSize);
			// get datestamp
			fread(&date, sizeof(date), 1, fpGUM);
			date = SWAP16(date);
			fread(&time, sizeof(time), 1, fpGUM);
			time = SWAP16(time);
			if (strcmp(szFileName, "UnixAttr.DAT") == 0) {
				fseek(fpGUM, lCompressSize, SEEK_CUR);
				continue;
			}
			if (strlen(szFileName) > 14) {
				AddLFN(szFileName, lFileSize);
				fseek(fpGUM, lCompressSize, SEEK_CUR);
				continue;
			}

			// show dir like DOS :)
			snprintf(szString, sizeof(szString), "|15%14s  |06-- |07%9" PRId32 " bytes  ",
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

		fseek(fpGUM, lCompressSize, SEEK_CUR);

		TotalBytes += lFileSize;
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
		if (!fgets(szString, sizeof(szString), fpInput)) {
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
static unsigned short _dos_setftime(int handle, unsigned short date, unsigned short time)
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
