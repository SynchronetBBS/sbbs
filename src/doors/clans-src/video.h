/*
 * Video ADT
 */

char FAR *vid_address(void);

void Video_Init(void);
/*
 * purpose  Initializes video system.
 * post     video mem, y-lookup initialized,
 */

void Input(char *string, _INT16 length);

void ClearArea(char x1, char y1,  char x2, char y2, char attr);
void xputs(char *string, _INT16 x, _INT16 y);

long Video_VideoType(void);

void DisplayStr(char *szString);

void zputs(char *string);
void qputs(char *string, _INT16 x, _INT16 y);

void ScrollUp(void);
#ifdef _WIN32
void clrscr(void);
void gotoxy(int, int);
#endif
