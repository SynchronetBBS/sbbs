/*
 * Video ADT
 */

#ifdef __MINGW32__
#define FAR
#endif

char FAR *vid_address(void);

void Video_Init(void);
/*
 * purpose  Initializes video system.
 * post     video mem, y-lookup initialized,
 */

void Input(char *string, int16_t length);

void ClearArea(char x1, char y1,  char x2, char y2, char attr);
void xputs(char *string, int16_t x, int16_t y);

int32_t Video_VideoType(void);

void DisplayStr(char *szString);

void zputs(char *string);
void qputs(char *string, int16_t x, int16_t y);

void ScrollUp(void);
#ifdef _WIN32
void clrscr(void);
void gotoxy(int, int);
#endif
