#ifndef _WIN32CIO_H_
#define _WIN32CIO_H_

#ifdef __cplusplus
extern "C" {
#endif
int win32_kbhit(void);
int win32_getch();
int win32_getche(void);
int win32_getmouse(struct cio_mouse_event *mevent);
int win32_hidemouse(void);
int win32_showmouse(void);

#if !defined(__BORLANDC__)
void	clreol(void);
void	clrscr(void);
void	delline(void);
int		gettext(int left, int top, int right, int bottom, void*);
void	gettextinfo(struct text_info*);
void	gotoxy(int x, int y);
void	highvideo(void);
void	insline(void);
void	lowvideo(void);
int		movetext(int left, int top, int right, int bottom, int destleft, int desttop);
void	normvideo(void);
int		puttext(int left, int top, int right, int bottom, void*);
void	textattr(int newattr);
void	textbackground(int newcolor);
void	textcolor(int newcolor);
void	textmode(int newmode);
void	window(int left, int top, int right, int bottom);
void	_setcursortype(int);
char*	cgets(char*);
int		cprintf(const char*, ...);
int		cputs(const char*);
int		cscanf(const char*, ... );
int		getch(void);
int		getche(void);
char*	getpass(const char*);
int		kbhit(void);
int		putch(int);
int		ungetch(int);
int		wherex(void);
int		wherey(void);

#endif


#ifdef __cplusplus
}
#endif

#endif
