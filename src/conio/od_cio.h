/* $Id$ */

#ifdef __unix__
#include "conio.h"

#ifdef __cplusplus
extern "C" {
#endif
int OD_puttext(int sx, int sy, int ex, int ey, unsigned char *fill);
int OD_gettext(int sx, int sy, int ex, int ey, unsigned char *fill);
void OD_textattr(unsigned char attr);
int OD_kbhit(void);
void OD_delay(long msec);
int OD_wherey(void);
int OD_wherex(void);
int OD_putch(unsigned char ch);
void OD_gotoxy(int x, int y);
void OD_initciowrap(long inmode);
void OD_gettextinfo(struct text_info *info);
void OD_setcursortype(int type);
int OD_getch(void);
int OD_getche(void);
int OD_beep(void);
void OD_textmode(int mode);
#ifdef __cplusplus
}
#endif

#endif
