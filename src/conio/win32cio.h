#ifndef _WIN32CIO_H_
#define _WIN32CIO_H_

#ifdef __cplusplus
extern "C" {
#endif
int win32_kbhit(void);
int win32_getch(void);
int win32_getche(void);
int win32_getmouse(struct cio_mouse_event *mevent);
int win32_hidemouse(void);
int win32_showmouse(void);

int	win32_gettext(int left, int top, int right, int bottom, void*);
void	win32_gettextinfo(struct text_info*);
void	win32_gotoxy(int x, int y);
void	win32_highvideo(void);
void	win32_lowvideo(void);
void	win32_normvideo(void);
int	win32_puttext(int left, int top, int right, int bottom, void*);
void	win32_textattr(int newattr);
void	win32_textbackground(int newcolor);
void	win32_textcolor(int newcolor);
void	win32_textmode(int newmode);
void	win32_setcursortype(int);
int	win32_getch(void);
int	win32_getche(void);
int	win32_kbhit(void);
int	win32_putch(int);
int	win32_wherex(void);
int	win32_wherey(void);
void	win32_settitle(const char *title);

#ifdef __cplusplus
}
#endif

#endif
