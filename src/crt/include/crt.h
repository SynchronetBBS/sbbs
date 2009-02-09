//CRT.H
//Video Handling Functions Header File
//ver 2.1

#if !defined (__CRT_H)
#define __CRT_H

#ifndef NULL
#include <stddef.h>
#endif

#ifdef CRT_FULL
#define SAVECRT
#define RESTORECRT
#define SAVECRTWIN
#define RESTORECRTWIN
#endif

	/* * * DATA TYPES * * */

struct crtwin
 {
	int left;
	int top;
	int right;
	int bottom;
 };

struct crtwin_inp
 {
	char *title; //Title (if s==NULL doesn't print title)
	int tcolor; //Title color
	int fchr;   //character used to fill window internal area
	int fcolor; //internal window area color
	int bcolor; //border color
	int btype;  //border type, same as crtframe
 };

	/* * * ENUMS AND DEFINES * * */

#if !defined(__COLORS)
#define __COLORS

enum CRT_COLORS{
	BLACK,			//dark colors
	BLUE,
	GREEN,
	CYAN,
	RED,
	MAGENTA,
	BROWN,
	LIGHTGRAY,
	DARKGRAY,		//light colors
	LIGHTBLUE,
	LIGHTGREEN,
	LIGHTCYAN,
	LIGHTRED,
	LIGHTMAGENTA,
	YELLOW,
	WHITE,
	BKCOLOR,			//selects background color if multiplied by a dark color
	TEXTBLINK = 128	//text blink bit
};
#else

#define BKCOLOR 16
#define TEXTBLINK 128

#endif

//macros to be used  with setdacpgmode
#define DACPAGE_64 0
#define DACPAGE_16 1

//macros to be used with changechar
#define CHANGCHR_NORM 0x1100
#define CHANGCHR_RECALC 0x1110

	/* * * GLOBAL VARIABLES * * */

extern int vmode_x; //number of columns in text mode (default=80)
extern int vmode_y; //number of rows in text mode (default=25)
extern int vmode_mode; //video mode expected by CRT functions (default=3)
extern int crt_direct; //set BIOS use if != 0. (default=0)
extern int crt_page; //selects the text page that must be used by
									 //functions
extern int crtwin_just; //used by printsj...
extern struct crtwin crtwin_dta; //used by printsj... and crtwindow

extern char * video_addr; //base VIDEO RAM address
extern const char crtframe_mat[]; //array used by crtframe
extern const char mkline_mat[]; //array used by mkline

extern int changechar_height; //used by changechar
extern int changechar_func; //and changecharg
extern int changechar_blk;

extern int crt_EVGA_addr; //EGA/VGA adapter base address
	//primary = 3C0h    alternate = 2C0h

	/* * * FUNCTIONS * * */

#ifdef __cplusplus
extern "C" {
#endif
void		crt_detect	(int mode);
void		crt_init		(int mode);
void		videomode (int newmode);
void 		setcrtmode (int mode);
int 		getcrtmode (void);
void 		setcrtpage (int page);
void		crt_gotoxy	(int  x, int y );
int		crt_getxy	(int *x, int *y);
int		getcrtchar	(int  x,	int y );
int		getcrtcolor (int  x, int y );
void		biosprintc  (int chr, int x, int y, int color, int func);
void		printcn	(int c, int x, int y);
void		printc	(int c, int x, int y, int color);
void		changecolor (int x, int y, int color);
void 		printct	(int c, int x, int y, int color, unsigned int type);
void		printsn	(char *s, int x, int y);
void		prints	(char *s, int x, int y, int color);
void		printxy	(char *s, int x, int y, int dx, int dy, int color);
void		printx	(char *s, int x, int y, int color);
int  		printtext (char *s, int x, int y, int color);
void 		printsj	(char *s, int y, int color);
void 		printsjc (char *s, int y, int color);
int 		printsnf	(int x, int y,					char *fmt,... );
int 		printsf	(int x, int y, int color,	char *fmt,... );
int 		printxyf (int x, int y, int dx, int dy, int color, char *fmt,... );
int 		printxf	(int x, int y, int color, 	char *fmt,... );
int 		printtextf	(int x, int y, int color, char *fmt,... );
int 		printsjf 	(int y, int color, char *fmt,... );
int 		printsjcf	(int y, int color, char *fmt,... );
void		fillscr	(int c, int color);
void 		barcolor (int xi, int yi, int xf, int yf, int color);
void		fillbar	(int c, int xi, int yi, int xf, int yf, int color);
void 		fillbarw (int c, int color);
void 		fillbox	(int c, int xi, int yi, int xf, int yf, int color, int func);
void		fillboxw (int c, int color, int func);
void		crtframe	(int xi, int yi, int xf, int yf, int color, unsigned int type);
void		crtframew(int color, unsigned int type);
void		mkline_aux 	(int cnt, int var, unsigned int mode, int pos, int color);
void		mkline		(int cnt, int bgn, int end, int color, unsigned int mode);
void 		crtwindow 	(struct crtwin_inp p0);
char *savevideo		(char *s);
char *restorevideo	(char *s);
char *savevideowin	(char *s, int xi, int yi, int xf, int yf);
char *restorevideowin(char *s, int xi, int yi, int xf, int yf);
char *savevideow		(char *s);
char *restorevideow	(char *s);

#ifdef SAVECRT
char *savecrt			(char *s, int mode);
#define savevideo(s) savecrt(s,0)
#endif

#ifdef RESTORECRT
char *restorecrt		(char *s, int mode);
#define restorevideo(s) restorecrt(s,0)
#endif

#ifdef SAVECRTWIN
char * savecrtwin		(char *s, int xi, int yi, int xf, int yf, int mode);
char *savecrtw			(char *s, int mode);
#define savevideowin(s,xi,yi,xf,yf) savecrtwin(s,xi,yi,xf,yf,0)
#define savevideow(s) savecrtw(s,0)
#endif

#ifdef RESTORECRTWIN
char * restorecrtwin	(char *s, int xi, int yi, int xf, int yf, int mode);
char *restorecrtw		(char *s, int mode);
#define restorevideowin(s,xi,yi,xf,yf) restorecrtwin(s,xi,yi,xf,yf,0)
#define restorevideow(s) restorecrtw(s,0)
#endif

void	crt_clrscr (void);
void	setcursorsh (unsigned int shape);
int	getcursorsh (void);
int 	getpalreg (int regpal);
void 	setpalreg (int regpal, int val);
int 	getbordercolor (void);
void	setbordercolor (int val);
void	getdacreg (int dacreg, char *red, char *green, char *blue);
void	setdacreg (int dacreg, char red, char green, char blue);
void	setdacpgmode	(int pgmode);
void	setdacpage		(int page);
int 	getdacpgstate 	(void);
void	setchrboxwidth	(int cmd);
void	settextblink	(int cmd);
void	changechar	(unsigned char *fmt, int ind, int qt);
void	changecharg (unsigned char *fmt, int rows);
void	crtfontspec (int blkspec);
#ifdef __cplusplus
}
#endif

//CRT MACROS

	//crtframe macros
#define moldura(xi,yi,xf,yf,color)\
	crtframe(xi,yi,xf,yf,color,0)	//draws a text frame with single outline
#define moldurad(xi,yi,xf,yf,color)\
	crtframe(xi,yi,xf,yf,color,1) //draws a text frame with double outline

	//crtframew macros
#define molduraw(color)\
	crtframew(color,0)
#define molduradw(color)\
	crtframew(color,1);

	//mkline macros
#define linha_hor(y,xi,xf,color)\
	mkline(y,xi,xf,color,0) //draws a single horizontal line
#define linha_ver(x,yi,yf,color)\
	mkline(x,yi,yf,color,1) //draws a single vertical line
#define linhad_hor(y,xi,xf,color)\
	mkline(y,xi,xf,color,2) //draws a double horizontal line
#define linhad_ver(x,yi,yf,color)\
	mkline(x,yi,yf,color,3) //draws a double horizontal line

	//macro setcrtwin
#define setcrtwin(xi,yi,xf,yf)\
	 crtwin_dta.left=xi;\
	 crtwin_dta.top=yi;\
	 crtwin_dta.right=xf;\
	 crtwin_dta.bottom=yf	//easely loads values to crtwin_dta

	//crtwindow macros
#define janela(p0)\
	p0.btype=0;\
	crtwindow(p0)	//draws a window with single outline
#define janelad(p0)\
	p0.btype=1;\
	crtwindow(p0)	//draws a window with double outline

#endif /* __CRT_H */

// By M rcio Afonso Arimura Fialho.
// Email: jlfialho@inconet.com.br OR jlfialho@yahoo.com.br (alternate)
// http://pessoal.iconet.com.br/jlfialho
// Freeware Library Functions - Free Distribution and Use.
// Read README.1ST for information.
