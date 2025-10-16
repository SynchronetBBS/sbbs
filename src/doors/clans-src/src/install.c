// ONE TO USE!!

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#ifdef __MSDOS__
# include <dos.h>
# include <conio.h>
#elif defined(_WIN32)
# include <windows.h>
# include <conio.h>
# include <sys/utime.h>
# include <direct.h> /* mkdir */
#elif defined(__unix__)
# include <curses.h>
# include <sys/time.h>
# include <unistd.h>
# include "unix_wrappers.h"
#endif /* __MSDOS__ */
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "defines.h"
#include "gum.h"
#include "parsing.h"

#ifndef __MSDOS__
#define far
#endif /* !__MSDOS__ */

#define STARTROW    9           // start at row 5
#ifdef __MSDOS__
#define COLOR   0xB800
#define MONO    0xB000
#endif /* __MSDOS__ */
#define CODE1           0x7D
#define CODE2           0x1F
#define MAX_FILES   75          // say 75 files is most in .GUM file for now
#define MAX_FILENAME_LEN 13

#define SKIP        0           // skip if file exists
#define OVERWRITE   1           // always overwrite
#define QUERY       2           // ask user if he wishes to overwrite

#define ALWAYS  2
#define TRUE    1
#define FALSE   0
#define MAX_TOKEN_CHARS 32

void GetGumName(void);
void ListFiles(void);
void kputs(char *szString);
int GetGUM(FILE *fpGUM);
void install(void);
char get_answer(char *szAllowableChars);
BOOL FileExists(char *szFileName);
void InitFiles(char *szFileName);
int WriteType(char *szFileName);
void Extract(char *szExtractFile, char *szNewName);
void upgrade(void);
__inline void get_screen_dimension(int *lines, int *width);
void gotoxy(int x, int y);

#ifdef __MSDOS__
char far *VideoMem;
long y_lookup[25];
#endif /* __MSDOS__ */
int Overwrite = FALSE;
char szGumName[25], szIniName[25];

struct FileInfo {
	char szFileName[14];
	int WriteType;
} FileInfo[MAX_FILES];

#ifdef __unix__
#define MKDIR(dir)              mkdir(dir,0777)
short curses_color(short color);
#else
#define MKDIR(dir)              mkdir(dir)
#endif

#ifndef __MSDOS__
unsigned short _dos_setftime(int, unsigned short, unsigned short);
#endif /* !__MSDOS__ */

void reset_attribute(void)
{
#ifdef __MSDOS__
	textattr(7);
#elif defined(_WIN32)
	SetConsoleTextAttribute(
		GetStdHandle(STD_OUTPUT_HANDLE),
		(WORD)(FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_RED));
#elif defined(__unix__)
	attrset(A_NORMAL|COLOR_PAIR(0));
	refresh();
#endif
}

void Display(char *szFileName)
{
	FILE *fpInput;
	char szString[300];
	BOOL FileFound = FALSE, Done = FALSE;
	int CurLine;

	fpInput = fopen(szIniName, "r");
	if (!fpInput)   return;

	// search for filename
	for (;;) {
		if (!fgets(szString, 300, fpInput))
			break;

		// get rid of \n and \r
		while (szString[ strlen(szString) - 1] == '\n' || szString[ strlen(szString) - 1] == '\r')
			szString[ strlen(szString) - 1] = 0;

		if (szString[0] == ':' && stricmp(szFileName, &szString[1]) == 0) {
			FileFound = TRUE;
			break;
		}
	}

	if (FileFound == FALSE) {
		fclose(fpInput);

		// search dos for file
		fpInput = fopen(szFileName, "r");
		if (!fpInput) {
			kputs("|04File to display not found\n");
			return;
		}
	}
	// print file
	CurLine = 0;
	while (!Done) {
		if (!fgets(szString, 300, fpInput))
			break;

		if (szString[0] == ':')
			break;

		kputs(szString);
		CurLine++;

		if (CurLine == (25-STARTROW-1)) {
			CurLine = 0;
			kputs("[more]");
			if (toupper(getch()) == 'Q')
				Done = TRUE;
			kputs("\r       \r");
		}
	}

	fclose(fpInput);
}

#ifdef _WIN32
void display_win32_error(void)
{
	LPVOID msg;

	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM|
		FORMAT_MESSAGE_ALLOCATE_BUFFER|
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPSTR)&msg,
		0,
		NULL);

	fprintf(stderr, "System Error: %s\n", (LPSTR)msg);
	fflush(stderr);

	LocalFree(msg);
}
#endif

void ScrollUp(void)
{
#ifdef __MSDOS__
	asm {
		mov al,1    // number of lines
		mov ch,STARTROW // starting row
		mov cl,0    // starting column
		mov dh, 24  // ending row
		mov dl, 79  // ending column
		mov bh, 7   // color
		mov ah, 6
		int 10h     // do the scroll
	}
#elif defined(_WIN32)
	HANDLE handle_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	SMALL_RECT scroll_rect;
	COORD top_left = { 0, 0 };
	CHAR_INFO char_info;

	GetConsoleScreenBufferInfo(handle_stdout, &screen_buffer);
	scroll_rect.Top = 1;
	scroll_rect.Left = 0;
	scroll_rect.Bottom = screen_buffer.dwSize.Y;
	scroll_rect.Right = screen_buffer.dwSize.X;

	char_info.Attributes = screen_buffer.wAttributes;
	char_info.Char.UnicodeChar = (TCHAR)' ';

	if (!ScrollConsoleScreenBuffer(handle_stdout,
								   &scroll_rect, NULL, top_left, &char_info)) {
		display_win32_error();
		reset_attribute();
		exit(0);
	}
#elif defined (__unix__)
	int sizey, sizex;
	int x,y;
	chtype this;

	get_screen_dimension(&sizey, &sizex);

	for (y=STARTROW; y<sizey; y++) {
		for (x=0; x<sizex; x++) {
			this=mvinch(y+1,x);
			mvaddch(y,x,this);
		}
	}
	scrollok(stdscr,FALSE);
	gotoxy(0, sizey-1);
	clrtobot();
	refresh();
#else
#pragma message ("ScrollUp() Undefined")
#endif
}

void get_xy(int *x, int *y)
{
#ifdef __MSDOS__
	struct text_info TextInfo;

	gettextinfo(&TextInfo);
	*x = TextInfo.curx-1;
	*y = TextInfo.cury-1;
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;

	GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&screen_buffer);
	*x = screen_buffer.dwCursorPosition.X;
	*y = screen_buffer.dwCursorPosition.Y;
#elif defined(__unix__)
	getyx(stdscr,*y,*x);
#else
	*x = 0;
	*y = 0;
#endif
}

void get_screen_dimension(int *lines, int *width)
{
#ifdef __MSDOS__
	/* default to 80x25 */
	*lines = 25;
	*width = 80;
#elif defined(_WIN32)
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	GetConsoleScreenBufferInfo(
		GetStdHandle(STD_OUTPUT_HANDLE),
		&screen_buffer);
	*lines = screen_buffer.dwSize.Y;
	*width = screen_buffer.dwSize.X;
#elif defined(__unix__)
	getmaxyx(stdscr,*lines,*width);
#else
	*lines = 0;
	*width = 0;
#endif
}

void put_character(int x, int y, char ch, unsigned short int attribute)
{
#ifdef __MSDOS__
	VideoMem[(long)(y_lookup[(long) y]+ (long)(x<<1))] = (unsigned char)ch;
	VideoMem[(long)(y_lookup[(long) y]+ (long)(x<<1) + 1L)] = (unsigned char)attribute;
#elif defined(_WIN32)
	DWORD bytes_written;
	COORD cursor_pos;
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(stdout_handle, cursor_pos);

	SetConsoleTextAttribute(
		stdout_handle,
		attribute);
	WriteConsole(
		stdout_handle,
		&ch,
		1,
		&bytes_written,
		NULL);
#elif defined(__unix__)
	short fg, bg;
	int   attrs=A_NORMAL;

	pair_content(attribute,&fg,&bg);
	if (attribute & 8)  {
		attrs |= A_BOLD;
	}
	if (attribute & 128)
		attrs |= A_BLINK;
	attrset(attrs);
	mvaddch(y, x, ch|COLOR_PAIR(attribute+1));
#else
	/* Ignore Attribute */
#endif
}

#ifdef _WIN32
void gotoxy(int x, int y)
{
	COORD cursor_pos;
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);

	cursor_pos.X = x;
	cursor_pos.Y = y;

	SetConsoleCursorPosition(stdout_handle, cursor_pos);
}

void clrscr(void)
{
	HANDLE std_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	COORD top_left = { 0, 0 };
	DWORD cells_written;

	GetConsoleScreenBufferInfo(std_handle, &screen_buffer);

	FillConsoleOutputCharacter(
		std_handle,
		(TCHAR)' ',
		screen_buffer.dwSize.X * screen_buffer.dwSize.Y,
		top_left,
		&cells_written);

	FillConsoleOutputAttribute(
		std_handle,
		(WORD)7,
		screen_buffer.dwSize.X * screen_buffer.dwSize.Y,
		top_left,
		&cells_written);

	SetConsoleCursorPosition(
		std_handle,
		top_left);
}
#endif /* _WIN32 */

#ifdef __unix__
void gotoxy(int x, int y)
{
	move(y, x);
	refresh();
}

void clrscr(void)
{
	clear();
}
#endif /* __unix__ */

void kputs(char *szString)
{
	char number[3];
	int cur_char, attr;
	char foreground, background, cur_attr;
	int i, j,  x,y;
	static char o_fg = 7, o_bg = 0;
	int scr_lines, scr_width;

	get_xy(&x, &y);
	get_screen_dimension(&scr_lines, &scr_width);

	cur_attr = o_fg | o_bg;
	cur_char=0;

	for (;;) {
		if (x == scr_width) {
			x = 0;
			y++;

			if (y == scr_lines) {
				/* scroll line up */
				ScrollUp();
				y = scr_lines - 1;
				break;
			}
		}
		if (y == scr_lines) {
			/* scroll line up */
			ScrollUp();
			y = scr_lines - 1;
		}
		if (szString[cur_char] == 0)
			break;
		if (szString[cur_char] == '\b') {
			x--;
			cur_char++;
			continue;
		}
		if (szString[cur_char] == '\r') {
			x = 0;
			cur_char++;
			continue;
		}
		if (szString[cur_char]=='|')    {
			if (isdigit(szString[cur_char+1]) && isdigit(szString[cur_char+2]))  {
				number[0]=szString[cur_char+1];
				number[1]=szString[cur_char+2];
				number[2]=0;

				attr=atoi(number);
				if (attr>15)    {
					background=attr-16;
					o_bg = background << 4;
					attr = o_bg | o_fg;
					cur_attr = attr;
				}
				else    {
					foreground=attr;
					o_fg=foreground;
					attr = o_fg | o_bg;
					cur_attr = attr;
				}
				cur_char += 3;
			}
			else    {
				put_character(x, y, szString[cur_char], cur_attr);

				cur_char++;
			}
		}
		else if (szString[cur_char] == '\n')  {
			y++;
			if (y == scr_lines) {
				/* scroll line up */
				ScrollUp();
				y = scr_lines - 1;
			}
			cur_char++;
			x = 0;
		}
		else if (szString[cur_char] == 9)  {  /* tab */
			cur_char++;
			for (i=0; i<8; i++)   {
				j = i + x;
				if (j > scr_width) break;
				put_character(j, y, ' ', cur_attr);
			}
			x += 8;
		}
		else    {
			put_character(x, y, szString[cur_char], cur_attr);

			cur_char++;
			x++;
		}
	}

	if (x == scr_width) {
		x = 0;
		y++;

		if (y == scr_lines) {
			/* scroll line up */
			ScrollUp();
			y = scr_lines - 1;
		}
	}
	gotoxy(x,y);
#ifdef __unix__
	refresh();
#endif
}

void kcls(void)
{
#ifdef __MSDOS__
	asm {
		mov al,0    // number of lines
		mov ch,STARTROW // starting row
		mov cl,0    // starting column
		mov dh, 24  // ending row
		mov dl, 79  // ending column
		mov bh, 7   // color
		mov ah, 6
		int 10h     // do the scroll
	}
#elif defined(_WIN32)
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD top_corner;
	CONSOLE_SCREEN_BUFFER_INFO screen_buffer;
	DWORD display_len, written;

	top_corner.X = 0;
	top_corner.Y = STARTROW;

	GetConsoleScreenBufferInfo(stdout_handle, &screen_buffer);

	display_len = (screen_buffer.dwSize.Y - STARTROW) *
				  (screen_buffer.dwSize.X);

	FillConsoleOutputCharacter(
		stdout_handle,
		(TCHAR)' ',
		display_len,
		top_corner,
		&written);

	FillConsoleOutputAttribute(
		stdout_handle,
		(WORD)(FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_RED),
		display_len,
		top_corner,
		&written);
#elif defined(__unix__)
	move(STARTROW,0);
	attrset(A_NORMAL|COLOR_PAIR(0));
	clrtobot();
	refresh();
#else
	/* No Function */
#endif
}

void GetLine(char *InputStr, int MaxChars)
{
	int CurChar;
	unsigned char InputCh;
	char Spaces[85] = "                                                                                     ";
	char BackSpaces[85] = "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
	char TempStr[100], szString[100];

	Spaces[MaxChars] = 0;
	BackSpaces[MaxChars] = 0;

	CurChar = strlen(InputStr);

	kputs(Spaces);
	kputs(BackSpaces);
	kputs(InputStr);

	for (;;) {
		InputCh = getch();

		if (InputCh == '\b') {
			if (CurChar>0) {
				CurChar--;
				kputs("\b \b");
			}
		}
		else if (InputCh == '\r') {
			kputs("|16\n");
			InputStr[CurChar]=0;
			break;
		}
		else if (InputCh== '' || InputCh == '\x1B') { // ctrl-y
			InputStr [0] = 0;
			strcpy(TempStr, BackSpaces);
			TempStr[ CurChar ] = 0;
			kputs(TempStr);
			Spaces[MaxChars] = 0;
			BackSpaces[MaxChars] = 0;
			kputs(Spaces);
			kputs(BackSpaces);
			CurChar = 0;
		}
		else if (InputCh >= '')
			continue;
		else if (InputCh == 0)
			continue;
		else if (iscntrl(InputCh))
			continue;
		else {  /* valid character input */
			if (CurChar == MaxChars)
				continue;

			InputStr[CurChar++]=InputCh;
			InputStr[CurChar] = 0;
			sprintf(szString, "%c", InputCh);
			kputs(szString);
		}
	}
}

char far *vid_address(void)
{
#ifdef __MSDOS__
	int tmp1, tmp2;
	long VideoType;                 /* B800 = color monitor */

	asm {
		mov bx,0040h
		mov es,bx
		mov dx,es:63h
		add dx,6
		mov tmp1, dx           // move this into status port
	};
	VideoType = COLOR;

	asm {
		mov bx,es:10h
		and bx,30h
		cmp bx,30h
		jne FC1
	};
	VideoType = MONO;

FC1:

	if (VideoType == MONO)
		return ((void far *) 0xB0000000L) ;
	else
		return ((void far *) 0xB8000000L) ;
#else
	return NULL;
#endif
}


void SystemInit(void)
{
#ifdef __MSDOS__
	int iTemp;

	VideoMem = vid_address();
	for (iTemp = 0; iTemp < 25;  iTemp++)
		y_lookup[ iTemp ] = iTemp * 160;
#endif /* __MSDOS__ */
#ifdef __unix__
	short fg;
	short bg;
	short pair=0;

	// Initialize Curses lib.
	initscr();
	cbreak();
	noecho();
	nonl();
//  intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	scrollok(stdscr,FALSE);
	start_color();
	clear();
	refresh();

	// Set up color pairs
	for (bg=0; bg<8; bg++)  {
		for (fg=0; fg<16; fg++) {
			init_pair(++pair,curses_color(fg),curses_color(bg));
		}
	}

#endif
	GetGumName();
}

#ifdef __unix__
short curses_color(short color)
{
	switch (color) {
		case 0 :
			return(COLOR_BLACK);
		case 1 :
			return(COLOR_BLUE);
		case 2 :
			return(COLOR_GREEN);
		case 3 :
			return(COLOR_CYAN);
		case 4 :
			return(COLOR_RED);
		case 5 :
			return(COLOR_MAGENTA);
		case 6 :
			return(COLOR_YELLOW);
		case 7 :
			return(COLOR_WHITE);
		case 8 :
			return(COLOR_BLACK);
		case 9 :
			return(COLOR_BLUE);
		case 10 :
			return(COLOR_GREEN);
		case 11 :
			return(COLOR_CYAN);
		case 12 :
			return(COLOR_RED);
		case 13 :
			return(COLOR_MAGENTA);
		case 14 :
			return(COLOR_YELLOW);
		case 15 :
			return(COLOR_WHITE);
	}
	return(0);
}
#endif /* __unix__ */

int main(int argc, char **argv)
{
	char szInput[46], szToken[46], szString[128], szToken2[20];

	if (argc != 2) {
		strcpy(szIniName, "install.ini");
	}
	else {
		strcpy(szIniName, argv[1]);

		if (!strchr(szIniName, '.'))
			strcat(szIniName, ".ini");
	}

	SystemInit();
	clrscr();

	gotoxy(1, 1);
	Display("TITLE");
	gotoxy(1, STARTROW+1);

	Display("WELCOME");

	for (;;) {
		kputs("|14> |07");

		// get input
		szInput[0] = 0;
		GetLine(szInput, 45);

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
			kcls();
			gotoxy(1, STARTROW+1);
		}
		else if (szToken[0] == '?' || stricmp(szToken, "help") == 0)
			Display("Help");
		else {
			sprintf(szString, "|04%s not a valid command!\n", szToken);
			kputs(szString);
		}
	}

	kcls();
	gotoxy(1, STARTROW+1);
	Display("Goodbye");
	return(0);
}

int GetGUM(FILE *fpGUM)
{
	char szKey[80];
	char szEncryptedName[MAX_FILENAME_LEN], cInput;
	char *pcFrom, *pcTo, szFileName[MAX_FILENAME_LEN], szString[128];
	FILE *fpToFile;
	uint32_t lFileSize, lCompressSize;
	uint16_t date, time;

	ClearAll();

	/* read in filename */
	if (!fread(&szEncryptedName, sizeof(szEncryptedName), sizeof(char), fpGUM))
		return FALSE;

	/* then decrypt it */
	pcFrom = szEncryptedName;
	pcTo = szFileName;
	while (*pcFrom) {
		*pcTo = *pcFrom ^ CODE1;
		pcFrom++;
		pcTo++;
	}
	*pcTo = 0;
	sprintf(szString, "|14%-*s |06", MAX_FILENAME_LEN, szFileName);
	kputs(szString);

	/* make key using filename */
	sprintf(szKey, "%s%x%x", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);
	// printf("key = '%s%x%x'\n", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);

	pcTo = szKey;
	while (*pcTo) {
		*pcTo ^= CODE2;
		pcTo++;
	}

	// if dirname, makedir
	if (szFileName[0] == '/') {
		sprintf(szString, "dir |14%-*s\n", MAX_FILENAME_LEN, szFileName);
		kputs(szString);

		MKDIR(&szFileName[1]

			 );
		return TRUE;
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

	// get type of input depending on WriteType
	if (FileExists(szFileName) && WriteType(szFileName) == OVERWRITE) {
		// continue -- overwrite file
		kputs("|07- updating - |06");
	}
	else if (WriteType(szFileName) == SKIP && FileExists(szFileName)) {
		// skip file
		kputs("|07- skipping (no update required)\n");
		fseek(fpGUM, lCompressSize, SEEK_CUR);
		return 1;
	}
	// else, normal file, do normally
	// if file exists, ask user if he wants to overwrite it
	else if (FileExists(szFileName) && Overwrite != ALWAYS) {
		kputs("|03exists.  overwrite? (yes/no/rename/always) :");

		cInput = get_answer("YNAR");

		if (cInput == 'A') {
			Overwrite = ALWAYS;
		}
		else if (cInput == 'R') {
			kputs("\n|03enter new file name: ");
			GetLine(szFileName, MAX_FILENAME_LEN);
		}
		else if (cInput == 'N') {
			// skip file
			fseek(fpGUM, lCompressSize, SEEK_CUR);
			return 1;
		}
	}

	/* open file to write to */
	fpToFile = fopen(szFileName, "wb");
	if (!fpToFile) {
		sprintf(szString, "|04Couldn't open %s\n", szFileName);
		kputs(szString);
		return FALSE;
	}

	// === decode here
	decode(fpGUM, fpToFile, kputs);

	fclose(fpToFile);

	fpToFile = fopen(szFileName, "r+b");
	_dos_setftime(fileno(fpToFile), date, time);
	fclose(fpToFile);



	sprintf(szString, "%" PRId32 "b ", lFileSize);
	kputs(szString);
	kputs("|15done.\n");

	return TRUE;
}

void install(void)
{
	FILE *fpGUM;
#ifdef __unix__
	FILE *fpAttr;
	mode_t  tMode;
	char szFileName[MAX_FILENAME_LEN];
	unsigned tmp;
#endif
	char cInput;

	Display("Install");

	// make sure he wants to
	kputs("\n|07Are you sure you wish to run the install now? (y/n):");
	cInput = get_answer("YN");

	if (cInput == 'N') {
		kputs("\n|04Installation aborted.\n");
		return;
	}

	InitFiles("INSTALL.FILES");
	Overwrite = FALSE;

	fpGUM = fopen(szGumName, "rb");
	if (!fpGUM) {
		printf("Can't find -%s-\n",szGumName);
		kputs("|04No .GUM to blowup!\n");
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

void upgrade(void)
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
	kputs("\n|07Are you sure you wish to upgrade? (y/n):");
	cInput = get_answer("YN");

	if (cInput == 'N') {
		kputs("\n|04Upgrade aborted.\n");
		return;
	}

	InitFiles("UPGRADE.FILES");
	Overwrite = FALSE;

	fpGUM = fopen(szGumName, "rb");
	if (!fpGUM) {
		kputs("|04No .GUM to blowup!\n");
		return;
	}

	while (GetGUM(fpGUM))
		;

#ifdef __unix__
	fpAttr = fopen("UnixAttr.DAT", "r");
	if (fpAttr) {
		while (fscanf(fpAttr, "%u %s\n", &tmp, szFileName) != EOF) {
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

char get_answer(char *szAllowableChars)
{
	char cKey, szString[20];
	unsigned int iTemp;

	for (;;) {
		cKey = getch();

		/* see if allowable */
		for (iTemp = 0; iTemp < strlen(szAllowableChars); iTemp++) {
			if (toupper(cKey) == toupper(szAllowableChars[iTemp]))
				break;
		}

		if (iTemp < strlen(szAllowableChars))
			break;  /* found allowable key */
	}
	sprintf(szString, "%c\n", cKey);
	kputs(szString);

	return (toupper(cKey));
}

BOOL FileExists(char *szFileName)
{
	FILE *fp;

	fp = fopen(szFileName, "r");
	if (!fp) {
		return FALSE;
	}

	fclose(fp);
	return TRUE;
}

void InitFiles(char *szFileName)
{
	// init files to be installed

	FILE *fpInput;
	char szString[128];
	BOOL FileFound = FALSE, Done = FALSE;
	int CurFile;

	for (CurFile = 0; CurFile < MAX_FILES; CurFile++)
		FileInfo[CurFile].szFileName[0] = 0;

	fpInput = fopen(szIniName, "r");
	if (!fpInput) {
		kputs("|06usage:   |14install <inifile>\n");
		reset_attribute();
		exit(0);
		return;
	}

	// search for filename
	for (;;) {
		if (!fgets(szString, 128, fpInput))
			break;

		// get rid of \n and \r
		while (szString[ strlen(szString) - 1] == '\n' || szString[ strlen(szString) - 1] == '\r')
			szString[ strlen(szString) - 1] = 0;

		if (szString[0] == ':' && stricmp(szFileName, &szString[1]) == 0) {
			FileFound = TRUE;
			break;
		}
	}

	if (FileFound == FALSE) {
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
		strcpy(FileInfo[CurFile].szFileName, &szString[2]);
		FileInfo[CurFile].WriteType = QUERY;

		if (szString[0] == 'o')
			FileInfo[CurFile].WriteType = OVERWRITE;
		else if (szString[0] == 's')
			FileInfo[CurFile].WriteType = SKIP;

		CurFile++;
	}

	fclose(fpInput);
}

int WriteType(char *szFileName)
{
	int CurFile;

	// search for file
	for (CurFile = 0; CurFile < MAX_FILES; CurFile++) {
		if (stricmp(FileInfo[CurFile].szFileName, szFileName) == 0) {
			// found file
			return (FileInfo[CurFile].WriteType);
		}
	}

	// if not found, return query
	return QUERY;
}

void Extract(char *szExtractFile, char *szNewName)
{
	char szKey[80];
	char szEncryptedName[MAX_FILENAME_LEN], cInput;
	char *pcFrom, *pcTo, szFileName[MAX_FILENAME_LEN], szString[128];
	FILE *fpToFile, *fpGUM;
	int32_t lFileSize, lCompressSize;
	uint16_t date, time;

	ClearAll();

	fpGUM = fopen(szGumName, "rb");
	if (!fpGUM) {
		kputs("|04No .GUM to blowup!\n");
		return;
	}

	for (;;) {
		/* read in filename */
		if (!fread(&szEncryptedName, sizeof(szEncryptedName), sizeof(char), fpGUM)) {
			kputs("|04file not found!\n");
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

		/* make key using filename */
		sprintf(szKey, "%s%x%x", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);
		// printf("key = '%s%x%x'\n", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);

		pcTo = szKey;
		while (*pcTo) {
			*pcTo ^= CODE2;
			pcTo++;
		}

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
		strcpy(szFileName, szNewName);

	// if dirname, makedir
	if (szFileName[0] == '/') {
		sprintf(szString, "|14%-*s\n", MAX_FILENAME_LEN, szFileName);
		kputs(szString);

		MKDIR(&szFileName[1]);
		fclose(fpGUM);
		return;
	}

	if (FileExists(szFileName)) {
		kputs("|03exists.  overwrite? (yes/no/rename/always) :");

		cInput = get_answer("YNAR");

		if (cInput == 'A') {
			Overwrite = ALWAYS;
		}
		else if (cInput == 'R') {
			kputs("\n|03enter new file name: ");
			GetLine(szFileName, MAX_FILENAME_LEN);
		}
		else if (cInput == 'N') {
			// skip file
			fseek(fpGUM, lCompressSize, SEEK_CUR);
			fclose(fpGUM);
			return;
		}
	}

	/* open file to write to */
	fpToFile = fopen(szFileName, "wb");
	if (!fpToFile) {
		sprintf(szString, "|04Couldn't write to %s\n", szFileName);
		kputs(szString);
		return;
	}

	sprintf(szString, "|06Extracting %s\n", szFileName);
	kputs(szString);

	//== decode it here
	decode(fpGUM, fpToFile, kputs);

	fclose(fpToFile);
	fclose(fpGUM);

	fpToFile = fopen(szFileName, "r+b");
	_dos_setftime(fileno(fpToFile), date, time);
	fclose(fpToFile);

	sprintf(szString, "(%" PRId32 " bytes) ", lFileSize);
	kputs(szString);
	kputs("|15Done.\n");
}

void ListFiles(void)
{
	char szKey[80];
	char szEncryptedName[MAX_FILENAME_LEN];
	char *pcFrom, *pcTo, szFileName[MAX_FILENAME_LEN], szString[128];
	FILE *fpGUM;
	int32_t lFileSize, TotalBytes = 0, lCompressSize;
	int FilesFound = 0;
	uint16_t date, time;
	BOOL Done = FALSE;

	fpGUM = fopen(szGumName, "rb");
	if (!fpGUM) {
		kputs("|04No .GUM to blowup!\n");
		return;
	}

	while (!Done) {
		/* read in filename */
		if (!fread(&szEncryptedName, sizeof(szEncryptedName), sizeof(char), fpGUM)) {
			// done
			fclose(fpGUM);
			sprintf(szString, "\n|14%" PRId32 " total bytes\n\n", TotalBytes);
			kputs(szString);
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

		/* make key using filename */
		sprintf(szKey, "%s%x%x", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);
		// printf("key = '%s%x%x'\n", szEncryptedName, szEncryptedName[0], szEncryptedName[1]);

		pcTo = szKey;
		while (*pcTo) {
			*pcTo ^= CODE2;
			pcTo++;
		}

		if (szFileName[0] == '/') {
			sprintf(szString, "|15%14s  |06-- |07      directory  ",
					szFileName);
			kputs(szString);

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

			// show dir like DOS :)
			sprintf(szString, "|15%14s  |06-- |07%9" PRId32 " bytes  ",
					szFileName, lFileSize);
			kputs(szString);
		}
		FilesFound++;

		if ((FilesFound % 2) == 0)
			kputs("\n");

		if (FilesFound % (2*(25-STARTROW)-4) == 0 && FilesFound) {
			kputs("[more]");
			if (toupper(getch()) == 'Q')
				Done = TRUE;
			kputs("\r       \r");
		}

		fseek(fpGUM, lCompressSize, SEEK_CUR);

		TotalBytes += lFileSize;
	}

	if ((FilesFound % 2) != 0)
		kputs("\n");

	fclose(fpGUM);
}

void GetGumName(void)
{
	// init files to be installed
	FILE *fpInput;
	char szString[128];
	int CurFile;

	for (CurFile = 0; CurFile < MAX_FILES; CurFile++)
		FileInfo[CurFile].szFileName[0] = 0;

	fpInput = fopen(szIniName, "r");
	if (!fpInput) {
		// no ini file
		kputs("|06usage:   |14install <inifile>\n");
		reset_attribute();
		exit(0);
	}

	// search for filename
	for (;;) {
		if (!fgets(szString, 128, fpInput)) {
			kputs("\n|12Couldn't get GUM filename\n");
			reset_attribute();
			exit(0);
			break;
		}

		// get rid of \n and \r
		while (szString[ strlen(szString) - 1] == '\n' || szString[ strlen(szString) - 1] == '\r')
			szString[ strlen(szString) - 1] = 0;

		if (szString[0] == '!') {
			strcpy(szGumName, &szString[1]);
			break;
		}
	}

	fclose(fpInput);
}

#ifndef __MSDOS__
unsigned short _dos_setftime(int handle, unsigned short date, unsigned short time)
{
	struct tm dos_dt;
	time_t file_dt;
#ifdef _WIN32
	struct _utimbuf tm_buf;
#elif defined(__unix__)
	struct timeval tmv_buf;
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
	tmv_buf.tv_sec = file_dt;
	tmv_buf.tv_usec = file_dt * 1000;
	return (futimes(handle, &tmv_buf));
#else
#error "_dos_setftime needs a setting function, compilation aborted"
#endif
}
#endif
