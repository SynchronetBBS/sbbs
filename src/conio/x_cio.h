/* $Id$ */

#ifdef __unix__
#include "ciolib.h"

#ifdef __cplusplus
extern "C" {
#endif
int x_puttext(int sx, int sy, int ex, int ey, void *fill);
int x_gettext(int sx, int sy, int ex, int ey, void *fill);
void x_textattr(int attr);
int x_kbhit(void);
void x_delay(long msec);
int x_wherey(void);
int x_wherex(void);
int x_putch(int ch);
void x_gotoxy(int x, int y);
void x_initciolib(long inmode);
void x_gettextinfo(struct text_info *info);
void x_setcursortype(int type);
int x_getch(void);
int x_getche(void);
int x_beep(void);
void x_textmode(int mode);
#ifdef __cplusplus
}
#endif

#endif
