/*
 * Video ADT
 */

#ifndef THE_CLANS__VIDEO___H
#define THE_CLANS__VIDEO___H

#ifndef __unix__
# include <conio.h>
#endif

#include "defines.h"

#ifdef __MINGW32__
#define FAR
#endif

#define K_HOME      71
#define K_UP        72
#define K_PGUP      73
#define K_LEFT      75
#define K_RIGHT     77
#define K_END       79
#define K_DOWN      80
#define K_PGDN      81
#define K_INSERT    82
#define K_DELETE    83

#define HILIGHT     11|(1<<4)

#define COLOR   0xB800
#define MONO    0xB000

extern int ScreenWidth;
extern int ScreenLines;

void Video_Init(void);
/*
 * purpose  Initializes video system.
 * post     video mem, y-lookup initialized,
 */
void Video_Close(void);

void Input(char *string, size_t length);

void ClearArea(int x1, int y1,  int x2, int y2, uint8_t attr);
void xputs(const char *string, int x, int y);

int32_t Video_VideoType(void);

void DisplayStr(const char *szString);

void zputs(const char *string);
void qputs(const char *string, int x, int y);

void ScrollUp(void);
void SetScrollRegion(int top, int bottom);
void ClearScrollRegion(void);
void textattr(uint8_t attrib);
void * save_screen(void);
void restore_screen(void *state);
void ShowTextCursor(bool sh);
#if defined(_WIN32) || defined(__unix__)
void clrscr(void);
void gotoxy(int, int);
#endif

void ColorArea(int xPos1, int yPos1, int xPos2, int yPos2, uint8_t Color);
long DosGetLong(char *Prompt, long DefaultVal, long Maximum);
void DosGetStr(char *InputStr, int16_t MaxChars, bool HiBit);
char get_answer(char *szAllowableChars);
#ifdef __unix__
int cio_getch(void);
#endif

#endif
