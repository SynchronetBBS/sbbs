/* $Id$ */

#ifdef __unix__
#include "ciowrap.h"

#ifdef __cplusplus
extern "C" {
#endif
short curses_color(short color);
int curs_puttext(int sx, int sy, int ex, int ey, unsigned char *fill);
int curs_gettext(int sx, int sy, int ex, int ey, unsigned char *fill);
void curs_textattr(unsigned char attr);
int curs_kbhit(void);
void curs_delay(long msec);
int curs_wherey(void);
int curs_wherex(void);
void _putch(unsigned char ch, BOOL refresh_now);
void curs_putch(unsigned char ch);
int curs_cprintf(char *fmat, ...);
void curs_cputs(unsigned char *str);
void curs_gotoxy(int x, int y);
void curs_clrscr(void);
void curs_initciowrap(long inmode);
void curs_gettextinfo(struct text_info *info);
void curs_setcursortype(int type);
void curs_textbackground(int colour);
void curs_textcolor(int colour);
void curs_clreol(void);
int curs_getch(void);
int curs_getche(void);
void curs_highvideo(void);
void curs_lowvideo(void);
void curs_normvideo(void);
#ifdef __cplusplus
}
#endif

#endif
