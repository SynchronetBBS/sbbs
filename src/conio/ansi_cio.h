/* $Id$ */

#include "ciolib.h"

#ifdef __cplusplus
extern "C" {
#endif
int ansi_puttext(int sx, int sy, int ex, int ey, void *fill);
int ansi_gettext(int sx, int sy, int ex, int ey, void *fill);
void ansi_textattr(int);
int ansi_kbhit(void);
void ansi_delay(long msec);
int ansi_wherey(void);
int ansi_wherex(void);
int ansi_putch(int ch);
void ansi_gotoxy(int x, int y);
int ansi_initciolib(long inmode);
void ansi_gettextinfo(struct text_info *info);
void ansi_setcursortype(int type);
int ansi_getch(void);
int ansi_getche(void);
int ansi_beep(void);
void ansi_textmode(int mode);
#ifdef __cplusplus
}
#endif

