/* uifcc.c */

/* Curses implementation of UIFC (user interface) library based on uifc.c */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2002 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This program is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU General Public License				*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU General Public License for more details: gpl.txt or			*
 * http://www.fsf.org/copyleft/gpl.html										*
 *																			*
 * Anonymous FTP access to the most recent released source is available at	*
 * ftp://vert.synchro.net, ftp://cvs.synchro.net and ftp://ftp.synchro.net	*
 *																			*
 * Anonymous CVS access to the development source and modification history	*
 * is available at cvs.synchro.net:/cvsroot/sbbs, example:					*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs login			*
 *     (just hit return, no password is necessary)							*
 * cvs -d :pserver:anonymous@cvs.synchro.net:/cvsroot/sbbs checkout src		*
 *																			*
 * For Synchronet coding style and modification guidelines, see				*
 * http://www.synchro.net/source.html										*
 *																			*
 * You are encouraged to submit any modifications (preferably in Unix diff	*
 * format) via e-mail to mods@synchro.net									*
 *																			*
 * Note: If this box doesn't appear square, then you need to fix your tabs.	*
 ****************************************************************************/

#include <stdio.h>
#include <curs_fix.h>
#include <unistd.h>
#include <sys/time.h>
#include "uifc.h"
#ifdef __QNX__
#include <strings.h>
#endif

#if defined(__OS2__)

#define INCL_BASE
#include <os2.h>

void mswait(int msec)
{
DosSleep(msec ? msec : 1);
}

#elif defined(_WIN32)
	#include <windows.h>
	#define mswait(x) Sleep(x)
#elif defined(__FLAT__)
    #define mswait(x) delay(x)
#endif

							/* Bottom line elements */
#define BL_INS      (1<<0)  /* INS key */
#define BL_DEL      (1<<1)  /* DEL key */
#define BL_GET      (1<<2)  /* Get key */
#define BL_PUT      (1<<3)  /* Put key */

enum {
	 BLACK
	,BLUE	
	,GREEN	
	,CYAN	
	,RED		
	,MAGENTA	
	,BROWN	
	,LIGHTGRAY	
	,DARKGRAY	
	,LIGHTBLUE	
	,LIGHTGREEN	
	,LIGHTCYAN	
	,LIGHTRED	
	,LIGHTMAGENTA
	,YELLOW
	,WHITE
};

#define BLINK	128
#define SH_DENYWR	1
#define SH_DENYRW	2
#ifndef O_BINARY
#define O_BINARY	0
#endif

static char hfclr,hbclr,hclr,lclr,bclr,cclr,show_free_mem=0;
static char* helpfile=0;
static uint helpline=0;
static char blk_scrn[MAX_BFLN];
static win_t sav[MAX_BUFS];
static uint max_opts=MAX_OPTS;
static uifcapi_t* api;
static int lastattr=0;

/* Prototypes */
static int  uprintf(int x, int y, unsigned char attr, char *fmt,...);
static void bottomline(int line);
static char *utimestr(time_t *intime);
static void help();
static int puttext(int sx, int sy, int ex, int ey, unsigned char *fill);
static int gettext(int sx, int sy, int ex, int ey, unsigned char *fill);
static short curses_color(short color);
static void textattr(unsigned char attr);
static int kbhit(void);
#ifndef __QNX__
static void delay(int msec);
#endif
static int ugetstr(char *outstr, int max, long mode);
static int wherey(void);
static int wherex(void);
static int cprintf(char *fmat, ...);
static void cputs(char *str);
static void gotoxy(int x, int y);
static void _putch(unsigned char ch, BOOL refresh);
static void timedisplay(void);
#define putch(x)	_putch(x,TRUE)

/* API routines */
static void uifcbail(void);
static int uscrn(char *str);
static int ulist(int mode, int left, int top, int width, int *dflt, int *bar
	,char *title, char **option);
static int uinput(int imode, int left, int top, char *prompt, char *str
	,int len ,int kmode);
static void umsg(char *str);
static void upop(char *str);
static void sethelp(int line, char* file);
void showbuf(int mode, int left, int top, int width, int height, char *title, char *hbuf, int *curp, int *barp);

/* Dynamic menu support */
static int *last_menu_cur=NULL;
static int *last_menu_bar=NULL;
static int save_menu_cur=-1;
static int save_menu_bar=-1;

void reset_dynamic(void) {
	last_menu_cur=NULL;
	last_menu_bar=NULL;
	save_menu_cur=-1;
	save_menu_bar=-1;
}

int inkey(int mode)
{
	if(mode)
		return(kbhit());
	return(getch());
}

/****************************************************************************/
/* Initialization function, see uifc.h for details.							*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uifcinic(uifcapi_t* uifcapi)
{
	int 	i;
	int		height, width;
	short	fg, bg, pair=0;
#ifndef __FLAT__
	union	REGS r;
#endif

    if(uifcapi==NULL || uifcapi->size!=sizeof(uifcapi_t))
        return(-1);

    api=uifcapi;

    /* install function handlers */            
    api->bail=uifcbail;
    api->scrn=uscrn;
    api->msg=umsg;
    api->pop=upop;
    api->list=ulist;
    api->input=uinput;
    api->sethelp=sethelp;
#ifdef __unix__
    api->showhelp=help;
	api->showbuf=showbuf;
	api->timedisplay=timedisplay;
#endif

#if defined(LOCALE)
  (void) setlocale(LC_ALL, "");
#endif

	initscr();
	start_color();
	cbreak();
	noecho();
	nonl();
//	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);
	scrollok(stdscr,FALSE);
	raw();
	#ifdef NCURSES_VERSION_MAJOR
	ESCDELAY=api->esc_delay;
	#endif

	// Set up color pairs
	for(bg=0;bg<8;bg++)  {
		for(fg=0;fg<8;fg++) {
			init_pair(++pair,curses_color(fg),curses_color(bg));
		}
	}

    clear();
	refresh();
	getmaxyx(stdscr,height,width);
    /* unsupported mode? */
    if(height<MIN_LINES
        || height>MAX_LINES
        || width<80) {
	}
	
    api->scrn_len=height;
    if(api->scrn_len<MIN_LINES || api->scrn_len>MAX_LINES) {
		uifcbail();
        fprintf(stderr,"\7UIFC: Screen length (%u) must be between %d and %d lines\n"
            ,api->scrn_len,MIN_LINES,MAX_LINES);
        return(-2);
    }
    if(width*height*2>MAX_BFLN) {
		uifcbail();
        fprintf(stderr,"\7UIFC: Screen size (%u x %u) must be smaller than %u\n"
            ,width,height,(uchar)(MAX_BFLN/2));
        return(-2);
    }
    api->scrn_len--; /* account for status line */
	api->scrn_width=width;
	

    if(width<80) {
		uifcbail();
        fprintf(stderr,"\7UIFC: Screen width (%u) must be at least 80 characters\n"
            ,width);
        return(-3);
    }


/* ToDo - Set up mousemask() here */

    if(!has_colors()) {
        bclr=BLACK;
        hclr=WHITE;
        lclr=LIGHTGRAY;
        cclr=LIGHTGRAY;
		hbclr=BLACK;		/* Highlight Background Colour */
		hfclr=WHITE;		/* Highlight Foreground Colour */
    } else {
        bclr=BLUE;
        hclr=YELLOW;
        lclr=WHITE;
        cclr=CYAN;
		hbclr=LIGHTGRAY;
		hfclr=YELLOW;
    }
    for(i=0;i<MAX_BFLN;i+=2) {
        blk_scrn[i]='°';
        blk_scrn[i+1]=cclr|(bclr<<4);
    }

	curs_set(0);
	refresh();

    return(0);
}

short curses_color(short color)
{
	switch(color)
	{
		case 0 :
			return(COLOR_BLACK);
		case 1 :
			return(COLOR_BLUE);
		case 2 :
			return(COLOR_GREEN);
		case 3 :
			return(COLOR_CYAN);
		case 4 :
			return(COLOR_RED);
		case 5 :
			return(COLOR_MAGENTA);
		case 6 :
			return(COLOR_YELLOW);
		case 7 :
			return(COLOR_WHITE);
		case 8 :
			return(COLOR_BLACK);
		case 9 :
			return(COLOR_BLUE);
		case 10 :
			return(COLOR_GREEN);
		case 11 :
			return(COLOR_CYAN);
		case 12 :
			return(COLOR_RED);
		case 13 :
			return(COLOR_MAGENTA);
		case 14 :
			return(COLOR_YELLOW);
		case 15 :
			return(COLOR_WHITE);
	}
	return(0);
}

static void hidemouse(void)
{
/* ToDo - Mouse stuff here */
}

static void showmouse(void)
{
/* ToDo - Mouse stuff here */
}


void uifcbail(void)
{
	curs_set(1);
	clear();
	nl();
	nocbreak();
	noraw();
	refresh();
	endwin();
}

/****************************************************************************/
/* Clear screen, fill with background attribute, display application title.	*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uscrn(char *str)
{
    textattr(bclr|(cclr<<4));
    gotoxy(1,1);
    clrtoeol();
    gotoxy(3,1);
	cputs(str);
    if(!puttext(1,2,api->scrn_width,api->scrn_len,blk_scrn))
        return(-1);
    gotoxy(1,api->scrn_len+1);
    clrtoeol();
	refresh();
	reset_dynamic();
    return(0);
}

/****************************************************************************/
/****************************************************************************/
static void scroll_text(int x1, int y1, int x2, int y2, int down)
{
	uchar buf[MAX_BFLN];

	gettext(x1,y1,x2,y2,buf);
	if(down)
		puttext(x1,y1+1,x2,y2,buf);
	else
		puttext(x1,y1,x2,y2-1,buf+(((x2-x1)+1)*2));
}

/****************************************************************************/
/* Updates time in upper left corner of screen with current time in ASCII/  */
/* Unix format																*/
/****************************************************************************/
static void timedisplay()
{
	static time_t savetime;
	time_t now;

	now=time(NULL);
	if(difftime(now,savetime)>=60) {
		uprintf(55,1,bclr|(cclr<<4),utimestr(&now));
		savetime=now; 
	}
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
	if(str[c]!=0)	/* don't write to string constants */
		str[c]=0;
}

/****************************************************************************/
/* General menu function, see uifc.h for details.							*/
/****************************************************************************/
int ulist(int mode, int left, int top, int width, int *cur, int *bar
	, char *title, char **option)
{
	uchar line[256],shade[256],win[MAX_BFLN],*ptr,a,b,c,longopt
		,search[MAX_OPLN],bline=0;
	int height,y;
	int i,j,opts=0,s=0; /* s=search index into options */
	int	is_redraw=0;

	#ifndef __FLAT__
	/* ToDo Mouse stuff */
	#endif

	if(mode&WIN_SAV && api->savnum>=MAX_BUFS-1)
		putch(7);
	i=0;
	if(mode&WIN_INS) bline|=BL_INS;
	if(mode&WIN_DEL) bline|=BL_DEL;
	if(mode&WIN_GET) bline|=BL_GET;
	if(mode&WIN_PUT) bline|=BL_PUT;
	bottomline(bline);
	while(opts<max_opts && opts<MAX_OPTS)
		if(option[opts][0]==0)
			break;
		else opts++;
	if(mode&WIN_XTR && opts<max_opts && opts<MAX_OPTS)
		option[opts++][0]=0;
	height=opts+4;
	if(top+height>api->scrn_len-3)
		height=(api->scrn_len-3)-top;
	if(!width || width<strlen(title)+6) {
		width=strlen(title)+6;
		for(i=0;i<opts;i++) {
			truncsp(option[i]);
			if((j=strlen(option[i])+5)>width)
				width=j; } }
	if(width>(SCRN_RIGHT+1)-SCRN_LEFT)
		width=(SCRN_RIGHT+1)-SCRN_LEFT;
	if(mode&WIN_L2R)
		left=36-(width/2);
	else if(mode&WIN_RHT)
		left=SCRN_RIGHT-(width+4+left);
	if(mode&WIN_T2B)
		top=(api->scrn_len/2)-(height/2)-2;
	else if(mode&WIN_BOT)
		top=api->scrn_len-height-3-top;

	/* Dynamic Menus */
#ifdef __unix__
	if(mode&WIN_DYN
			&& cur != NULL 
			&& bar != NULL 
			&& last_menu_cur==cur 
			&& last_menu_bar==bar
			&& save_menu_cur==*cur
			&& save_menu_bar==*bar)
		is_redraw=1;
#endif

	if(!is_redraw) {
		if(mode&WIN_SAV && api->savdepth==api->savnum) {
			if((sav[api->savnum].buf=(char *)MALLOC((width+3)*(height+2)*2))==NULL) {
				cprintf("UIFC line %d: error allocating %u bytes."
					,__LINE__,(width+3)*(height+2)*2);
				return(-1); }
			gettext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT+left+width+1
				,SCRN_TOP+top+height,sav[api->savnum].buf);
			sav[api->savnum].left=SCRN_LEFT+left;
			sav[api->savnum].top=SCRN_TOP+top;
			sav[api->savnum].right=SCRN_LEFT+left+width+1;
			sav[api->savnum].bot=SCRN_TOP+top+height;
			api->savdepth++; }
		else if(mode&WIN_SAV
			&& (sav[api->savnum].left!=SCRN_LEFT+left
			|| sav[api->savnum].top!=SCRN_TOP+top
			|| sav[api->savnum].right!=SCRN_LEFT+left+width+1
			|| sav[api->savnum].bot!=SCRN_TOP+top+height)) { /* dimensions have changed */
			puttext(sav[api->savnum].left,sav[api->savnum].top,sav[api->savnum].right,sav[api->savnum].bot
				,sav[api->savnum].buf);	/* put original window back */
			FREE(sav[api->savnum].buf);
			if((sav[api->savnum].buf=(char *)MALLOC((width+3)*(height+2)*2))==NULL) {
				cprintf("UIFC line %d: error allocating %u bytes."
					,__LINE__,(width+3)*(height+2)*2);
				return(-1); }
			gettext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT+left+width+1
				,SCRN_TOP+top+height,sav[api->savnum].buf);	  /* save again */
			sav[api->savnum].left=SCRN_LEFT+left;
			sav[api->savnum].top=SCRN_TOP+top;
			sav[api->savnum].right=SCRN_LEFT+left+width+1;
			sav[api->savnum].bot=SCRN_TOP+top+height; } }


	#ifndef __FLAT__
	if(show_free_mem) {
	/* ToDo Show free memory */
	//	uprintf(58,1,bclr|(cclr<<4),"%10u bytes free",coreleft());
		}
	#endif


	if(!is_redraw) {
		if(mode&WIN_ORG) { /* Clear around menu */
			if(top)
				puttext(SCRN_LEFT,SCRN_TOP,SCRN_RIGHT+2,SCRN_TOP+top-1,blk_scrn);
			if(SCRN_TOP+height+top<=api->scrn_len)
				puttext(SCRN_LEFT,SCRN_TOP+height+top,SCRN_RIGHT+2,api->scrn_len,blk_scrn);
			if(left)
				puttext(SCRN_LEFT,SCRN_TOP+top,SCRN_LEFT+left-1,SCRN_TOP+height+top
					,blk_scrn);
			if(SCRN_LEFT+left+width<=SCRN_RIGHT)
				puttext(SCRN_LEFT+left+width,SCRN_TOP+top,SCRN_RIGHT+2
					,SCRN_TOP+height+top,blk_scrn); }
		ptr=win;
		*(ptr++)='É';
		*(ptr++)=hclr|(bclr<<4);

		if(api->mode&UIFC_MOUSE) {
			/* ToDo Mouse stuff */
			i=0;
		}
		else
			i=0;
		for(;i<width-2;i++) {
			*(ptr++)='Í';
			*(ptr++)=hclr|(bclr<<4); }
		*(ptr++)='»';
		*(ptr++)=hclr|(bclr<<4);
		*(ptr++)='º';
		*(ptr++)=hclr|(bclr<<4);
		a=strlen(title);
		b=(width-a-1)/2;
		for(i=0;i<b;i++) {
			*(ptr++)=' ';
			*(ptr++)=hclr|(bclr<<4); }
		for(i=0;i<a;i++) {
			*(ptr++)=title[i];
			*(ptr++)=hclr|(bclr<<4); }
		for(i=0;i<width-(a+b)-2;i++) {
			*(ptr++)=' ';
			*(ptr++)=hclr|(bclr<<4); }
		*(ptr++)='º';
		*(ptr++)=hclr|(bclr<<4);
		*(ptr++)='Ì';
		*(ptr++)=hclr|(bclr<<4);
		for(i=0;i<width-2;i++) {
			*(ptr++)='Í';
			*(ptr++)=hclr|(bclr<<4); }
		*(ptr++)='¹';
		*(ptr++)=hclr|(bclr<<4);

		if((*cur)>=opts)
			(*cur)=opts-1;			/* returned after scrolled */

		if(!bar) {
			if((*cur)>height-5)
				(*cur)=height-5;
			i=0; }
		else {
			if((*bar)>=opts)
				(*bar)=opts-1;
			if((*bar)>height-5)
				(*bar)=height-5;
			if((*cur)==opts-1)
				(*bar)=height-5;
			if((*bar)<0)
				(*bar)=0;
			if((*cur)<(*bar))
				(*cur)=(*bar);
			i=(*cur)-(*bar);
	//
			if(i+(height-5)>=opts) {
				i=opts-(height-4);
				(*cur)=i+(*bar);
				}
			}
			if((*cur)<0)
				(*cur)=0;

			j=0;
			if(i<0) i=0;
			longopt=0;
			while(j<height-4 && i<opts) {
				*(ptr++)='º';
				*(ptr++)=hclr|(bclr<<4);
				*(ptr++)=' ';
				*(ptr++)=hclr|(bclr<<4);
				*(ptr++)='³';
				*(ptr++)=lclr|(bclr<<4);
				if(i==(*cur))
					a=hfclr|(hbclr<<4);
				else
					a=lclr|(bclr<<4);
				b=strlen(option[i]);
				if(b>longopt)
					longopt=b;
				if(b+4>width)
					b=width-4;
				for(c=0;c<b;c++) {
					*(ptr++)=option[i][c];
					*(ptr++)=a; }
				while(c<width-4) {
					*(ptr++)=' ';
					*(ptr++)=a;
					c++; }
				*(ptr++)='º';
				*(ptr++)=hclr|(bclr<<4);
				i++;
				j++; }
			*(ptr++)='È';
			*(ptr++)=hclr|(bclr<<4);
			for(i=0;i<width-2;i++) {
				*(ptr++)='Í';
				*(ptr++)=hclr|(bclr<<4); }
			*(ptr++)='¼';
			*(ptr++)=hclr|(bclr<<4);
			puttext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT+left+width-1
				,SCRN_TOP+top+height-1,win);
			if(bar)
				y=top+3+(*bar);
			else
				y=top+3+(*cur);
			if(opts+4>height && ((!bar && (*cur)!=opts-1)
				|| (bar && ((*cur)-(*bar))+(height-4)<opts))) {
				gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+height-2);
				textattr(lclr|(bclr<<4));
				putch(31);	   /* put down arrow */
				textattr(hclr|(bclr<<4)); }

			if(bar && (*bar)!=(*cur)) {
				gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+3);
				textattr(lclr|(bclr<<4));
				putch(30);	   /* put the up arrow */
				textattr(hclr|(bclr<<4)); }

			if(bclr==BLUE) {
				gettext(SCRN_LEFT+left+width,SCRN_TOP+top+1,SCRN_LEFT+left+width+1
					,SCRN_TOP+top+height-1,shade);
				for(i=1;i<height*4;i+=2)
					shade[i]=DARKGRAY;
				puttext(SCRN_LEFT+left+width,SCRN_TOP+top+1,SCRN_LEFT+left+width+1
					,SCRN_TOP+top+height-1,shade);
				gettext(SCRN_LEFT+left+2,SCRN_TOP+top+height,SCRN_LEFT+left+width+1
					,SCRN_TOP+top+height,shade);
				for(i=1;i<width*2;i+=2)
					shade[i]=DARKGRAY;
				puttext(SCRN_LEFT+left+2,SCRN_TOP+top+height,SCRN_LEFT+left+width+1
					,SCRN_TOP+top+height,shade); }
		showmouse();
	}
	else {	/* Is a redraw */
		i=(*cur)-(*bar);
		j=2;

		longopt=0;
		while(j<height-2 && i<opts) {
			ptr=win;
			if(i==(*cur))
				a=hfclr|(hbclr<<4);
			else
				a=lclr|(bclr<<4);
			b=strlen(option[i]);
			if(b>longopt)
				longopt=b;
			if(b+4>width)
				b=width-4;
			for(c=0;c<b;c++) {
				*(ptr++)=option[i][c];
				*(ptr++)=a; }
			while(c<width-4) {
				*(ptr++)=' ';
				*(ptr++)=a;
				c++; }
			i++;
			j++; 
			puttext(SCRN_LEFT+left+3,SCRN_TOP+top+j,SCRN_LEFT+left+width-2
				,SCRN_TOP+top+j,win); }
		if(bar)
			y=top+3+(*bar);
		else
			y=top+3+(*cur);
	}

	#ifdef __unix__
	last_menu_cur=cur;
	last_menu_bar=bar;
	#endif
	while(1) {
	#if 0					/* debug */
		gotoxy(30,1);
		cprintf("y=%2d h=%2d c=%2d b=%2d s=%2d o=%2d"
			,y,height,*cur,bar ? *bar :0xff,api->savdepth,opts);
	#endif
		if(!show_free_mem)
			timedisplay();
	#ifndef __FLAT__
		if(api->mode&UIFC_MOUSE) {
		/* ToDo Serious mouse stuff here */
		}
	#endif

		i=0;
		if(inkey(1)) {
			i=inkey(0);
			if(i==KEY_BACKSPACE || i==BS)
				i=ESC;
			if(i>255) {
				s=0;
				switch(i) {
					/* ToDo extended keys */
					case KEY_HOME:	/* home */
						if(!opts)
							break;
						if(opts+4>height) {
							hidemouse();
							gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+3);
							textattr(lclr|(bclr<<4));
							putch(' ');    /* Delete the up arrow */
							gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+height-2);
							putch(31);	   /* put the down arrow */
							uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3
								,hfclr|(hbclr<<4)
								,"%-*.*s",width-4,width-4,option[0]);
							for(i=1;i<height-4;i++)    /* re-display options */
								uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3+i
									,lclr|(bclr<<4)
									,"%-*.*s",width-4,width-4,option[i]);
							(*cur)=0;
							if(bar)
								(*bar)=0;
							y=top+3;
							showmouse();
							break; }
						hidemouse();
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=lclr|(bclr<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						(*cur)=0;
						if(bar)
							(*bar)=0;
						y=top+3;
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=hfclr|(hbclr<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						showmouse();
						break;
					case KEY_UP:	/* up arrow */
						if(!opts)
							break;
						if(!(*cur) && opts+4>height) {
							hidemouse();
							gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+3); /* like end */
							textattr(lclr|(bclr<<4));
							putch(30);	   /* put the up arrow */
							gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+height-2);
							putch(' ');    /* delete the down arrow */
							for(i=(opts+4)-height,j=0;i<opts;i++,j++)
								uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3+j
									,i==opts-1 ? hfclr|(hbclr<<4)
										: lclr|(bclr<<4)
									,"%-*.*s",width-4,width-4,option[i]);
							(*cur)=opts-1;
							if(bar)
								(*bar)=height-5;
							y=top+height-2;
							showmouse();
							break; }
						hidemouse();
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=lclr|(bclr<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						showmouse();
						if(!(*cur)) {
							y=top+height-2;
							(*cur)=opts-1;
							if(bar)
								(*bar)=height-5; }
						else {
							(*cur)--;
							y--;
							if(bar && *bar)
								(*bar)--; }
						if(y<top+3) {	/* scroll */
							hidemouse();
							if(!(*cur)) {
								gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+3);
								textattr(lclr|(bclr<<4));
								putch(' '); }  /* delete the up arrow */
							if((*cur)+height-4==opts-1) {
								gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+height-2);
								textattr(lclr|(bclr<<4));
								putch(31); }   /* put the dn arrow */
							y++;
							scroll_text(SCRN_LEFT+left+2,SCRN_TOP+top+3
								,SCRN_LEFT+left+width-3,SCRN_TOP+top+height-2,1);
							uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3
								,hfclr|(hbclr<<4)
								,"%-*.*s",width-4,width-4,option[*cur]);
							showmouse(); }
						else {
							hidemouse();
							gettext(SCRN_LEFT+3+left,SCRN_TOP+y
								,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
							for(i=1;i<width*2;i+=2)
								line[i]=hfclr|(hbclr<<4);
							puttext(SCRN_LEFT+3+left,SCRN_TOP+y
								,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
							showmouse(); }
						break;
#if 0
					case KEY_PPAGE;	/* PgUp */
					case KEY_NPAGE;	/* PgDn */
						if(!opts || (*cur)==(opts-1))
							break;
						(*cur)+=(height-4);
						if((*cur)>(opts-1))
							(*cur)=(opts-1);

						hidemouse();
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=lclr|(bclr<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);

						for(i=(opts+4)-height,j=0;i<opts;i++,j++)
							uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3+j
								,i==(*cur) hfclr|(hbclr<<4) : lclr|(bclr<<4)
								,"%-*.*s",width-4,width-4,option[i]);
						y=top+height-2;
						if(bar)
							(*bar)=height-5;
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						for(i=1;i<148;i+=2)
							line[i]=hfclr|(hbclr<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						showmouse();
						break;
#endif
					case KEY_END:	/* end */
						if(!opts)
							break;
						if(opts+4>height) {	/* Scroll mode */
							hidemouse();
							gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+3);
							textattr(lclr|(bclr<<4));
							putch(30);	   /* put the up arrow */
							gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+height-2);
							putch(' ');    /* delete the down arrow */
							for(i=(opts+4)-height,j=0;i<opts;i++,j++)
								uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3+j
									,i==opts-1 ? hfclr|(hbclr<<4)
										: lclr|(bclr<<4)
									,"%-*.*s",width-4,width-4,option[i]);
							(*cur)=opts-1;
							y=top+height-2;
							if(bar)
								(*bar)=height-5;
							showmouse();
							break; }
						hidemouse();
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=lclr|(bclr<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						(*cur)=opts-1;
						y=top+height-2;
						if(bar)
							(*bar)=height-5;
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						for(i=1;i<148;i+=2)
							line[i]=hfclr|(hbclr<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						showmouse();
						break;
					case KEY_DOWN:	/* dn arrow */
						if(!opts)
							break;
						if((*cur)==opts-1 && opts+4>height) { /* like home */
							hidemouse();
							gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+3);
							textattr(lclr|(bclr<<4));
							putch(' ');    /* Delete the up arrow */
							gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+height-2);
							putch(31);	   /* put the down arrow */
							uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3
								,hfclr|(hbclr<<4)
								,"%-*.*s",width-4,width-4,option[0]);
							for(i=1;i<height-4;i++)    /* re-display options */
								uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3+i
									,lclr|(bclr<<4)
									,"%-*.*s",width-4,width-4,option[i]);
							(*cur)=0;
							y=top+3;
							if(bar)
								(*bar)=0;
							showmouse();
							break; }
						hidemouse();
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=lclr|(bclr<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						showmouse();
						if((*cur)==opts-1) {
							(*cur)=0;
							y=top+3;
							if(bar) {
								/* gotoxy(1,1); cprintf("bar=%08lX ",bar); */
								(*bar)=0; } }
						else {
							(*cur)++;
							y++;
							if(bar && (*bar)<height-5) {
								/* gotoxy(1,1); cprintf("bar=%08lX ",bar); */
								(*bar)++; } }
						if(y==top+height-1) {	/* scroll */
							hidemouse();
							if(*cur==opts-1) {
								gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+height-2);
								textattr(lclr|(bclr<<4));
								putch(' '); }  /* delete the down arrow */
							if((*cur)+4==height) {
								gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+3);
								textattr(lclr|(bclr<<4));
								putch(30); }   /* put the up arrow */
							y--;
							/* gotoxy(1,1); cprintf("\rdebug: %4d ",__LINE__); */
							scroll_text(SCRN_LEFT+left+2,SCRN_TOP+top+3
								,SCRN_LEFT+left+width-3,SCRN_TOP+top+height-2,0);
							/* gotoxy(1,1); cprintf("\rdebug: %4d ",__LINE__); */
							uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+height-2
								,hfclr|(hbclr<<4)
								,"%-*.*s",width-4,width-4,option[*cur]);
							showmouse(); }
						else {
							hidemouse();
							gettext(SCRN_LEFT+3+left,SCRN_TOP+y
								,SCRN_LEFT+left+width-2,SCRN_TOP+y
								,line);
							for(i=1;i<width*2;i+=2)
								line[i]=hfclr|(hbclr<<4);
							puttext(SCRN_LEFT+3+left,SCRN_TOP+y
								,SCRN_LEFT+left+width-2,SCRN_TOP+y
								,line);
							showmouse(); }
						break;
					case KEY_F(1):	/* F1 */
						help();
						break;
					case KEY_F(5):	/* F5 */
						if(mode&WIN_GET && !(mode&WIN_XTR && (*cur)==opts-1)) {
							return((*cur)|MSK_GET); }
						break;
					case KEY_F(6):	/* F6 */
						if(mode&WIN_PUT && !(mode&WIN_XTR && (*cur)==opts-1)) {
							return((*cur)|MSK_PUT); }
						break;
					case KEY_IC:	/* insert */
						if(mode&WIN_INS) {
							if(mode&WIN_INSACT) {
								hidemouse();
								gettext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT
									+left+width-1,SCRN_TOP+top+height-1,win);
								for(i=1;i<(width*height*2);i+=2)
									win[i]=lclr|(cclr<<4);
								if(opts) {
									j=(((y-top)*width)*2)+7+((width-4)*2);
									for(i=(((y-top)*width)*2)+7;i<j;i+=2)
										win[i]=hclr|(cclr<<4); }
								puttext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT
									+left+width-1,SCRN_TOP+top+height-1,win);
								showmouse(); }
							if(!opts) {
								return(MSK_INS); }
							return((*cur)|MSK_INS); }
						break;
					case KEY_DC:	/* delete */
						if(mode&WIN_XTR && (*cur)==opts-1)	/* can't delete */
							break;							/* extra line */
						if(mode&WIN_DEL) {
							if(mode&WIN_DELACT) {
								hidemouse();
								gettext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT
									+left+width-1,SCRN_TOP+top+height-1,win);
								for(i=1;i<(width*height*2);i+=2)
									win[i]=lclr|(cclr<<4);
								j=(((y-top)*width)*2)+7+((width-4)*2);
								for(i=(((y-top)*width)*2)+7;i<j;i+=2)
									win[i]=hclr|(cclr<<4);
								puttext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT
									+left+width-1,SCRN_TOP+top+height-1,win);
								showmouse(); }
							return((*cur)|MSK_DEL); }
						break;	} }
			else {
				i&=0xff;
				if(isalnum(i) && opts && option[0][0]) {
					search[s]=i;
					search[s+1]=0;
					for(j=(*cur)+1,a=b=0;a<2;j++) {   /* a = search count */
						if(j==opts) {					/* j = option count */
							j=-1;						/* b = letter count */
							continue; }
						if(j==(*cur)) {
							b++;
							continue; }
						if(b>=longopt) {
							b=0;
							a++; }
						if(a==1 && !s)
							break;
						if(strlen(option[j])>b
							&& ((!a && s && !strncasecmp(option[j]+b,search,s+1))
							|| ((a || !s) && toupper(option[j][b])==toupper(i)))) {
							if(a) s=0;
							else s++;
							if(y+(j-(*cur))+2>height+top) {
								(*cur)=j;
								gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+3);
								textattr(lclr|(bclr<<4));
								putch(30);	   /* put the up arrow */
								if((*cur)==opts-1) {
									gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+height-2);
									putch(' '); }  /* delete the down arrow */
								for(i=((*cur)+5)-height,j=0;i<(*cur)+1;i++,j++)
									uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3+j
										,i==(*cur) ? hfclr|(hbclr<<4)
											: lclr|(bclr<<4)
										,"%-*.*s",width-4,width-4,option[i]);
								y=top+height-2;
								if(bar)
									(*bar)=height-5;
								break; }
							if(y-((*cur)-j)<top+3) {
								(*cur)=j;
								gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+3);
								textattr(lclr|(bclr<<4));
								if(!(*cur))
									putch(' ');    /* Delete the up arrow */
								gotoxy(SCRN_LEFT+left+1,SCRN_TOP+top+height-2);
								putch(31);	   /* put the down arrow */
								uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3
									,hfclr|(hbclr<<4)
									,"%-*.*s",width-4,width-4,option[(*cur)]);
								for(i=1;i<height-4;i++) 	/* re-display options */
									uprintf(SCRN_LEFT+left+3,SCRN_TOP+top+3+i
										,lclr|(bclr<<4)
										,"%-*.*s",width-4,width-4
										,option[(*cur)+i]);
								y=top+3;
								if(bar)
									(*bar)=0;
								break; }
							hidemouse();
							gettext(SCRN_LEFT+3+left,SCRN_TOP+y
								,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
							for(i=1;i<width*2;i+=2)
								line[i]=lclr|(bclr<<4);
							puttext(SCRN_LEFT+3+left,SCRN_TOP+y
								,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
							if((*cur)>j)
								y-=(*cur)-j;
							else
								y+=j-(*cur);
							if(bar) {
								if((*cur)>j)
									(*bar)-=(*cur)-j;
								else
									(*bar)+=j-(*cur); }
							(*cur)=j;
							gettext(SCRN_LEFT+3+left,SCRN_TOP+y
								,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
							for(i=1;i<width*2;i+=2)
								line[i]=hfclr|(hbclr<<4);
							puttext(SCRN_LEFT+3+left,SCRN_TOP+y
								,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
							showmouse();
							break; } }
					if(a==2)
						s=0; }
				else
					switch(i) {
						case CR:
							if(!opts || (mode&WIN_XTR && (*cur)==opts-1))
								break;
							if(mode&WIN_ACT) {
								hidemouse();
								gettext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT
									+left+width-1,SCRN_TOP+top+height-1,win);
								for(i=1;i<(width*height*2);i+=2)
									win[i]=lclr|(cclr<<4);
								j=(((y-top)*width)*2)+7+((width-4)*2);
								for(i=(((y-top)*width)*2)+7;i<j;i+=2)
									win[i]=hclr|(cclr<<4);

								puttext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT
									+left+width-1,SCRN_TOP+top+height-1,win);
								showmouse(); }
							else if(mode&WIN_SAV) {
								hidemouse();
								puttext(sav[api->savnum].left,sav[api->savnum].top
									,sav[api->savnum].right,sav[api->savnum].bot
									,sav[api->savnum].buf);
								showmouse();
								FREE(sav[api->savnum].buf);
								api->savdepth--; }
							return(*cur);
						case 3:
						case ESC:
							if((mode&WIN_ESC || (mode&WIN_CHE && api->changes))
								&& !(mode&WIN_SAV)) {
								hidemouse();
								gettext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT
									+left+width-1,SCRN_TOP+top+height-1,win);
								for(i=1;i<(width*height*2);i+=2)
									win[i]=lclr|(cclr<<4);
								puttext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT
									+left+width-1,SCRN_TOP+top+height-1,win);
								showmouse(); }
							else if(mode&WIN_SAV) {
								hidemouse();
								puttext(sav[api->savnum].left,sav[api->savnum].top
									,sav[api->savnum].right,sav[api->savnum].bot
									,sav[api->savnum].buf);
								showmouse();
								FREE(sav[api->savnum].buf);
								api->savdepth--; }
							return(-1); } } }
		else
			mswait(1);
#ifdef __unix__
		if(mode&WIN_DYN) {
			save_menu_cur=*cur;
			save_menu_bar=*bar;
			return(-2-i);
		}
#endif

		}
}


/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, int left, int top, char *prompt, char *str,
	int max, int kmode)
{
	unsigned char c,save_buf[2048],in_win[2048]
		,shade[160],width,height=3;
	int i,plen,slen;

	hidemouse();
	reset_dynamic();
	plen=strlen(prompt);
	if(!plen)
		slen=4;
	else
		slen=6;
	width=plen+slen+max;
	if(mode&WIN_T2B)
		top=(api->scrn_len/2)-(height/2)-2;
	if(mode&WIN_L2R)
		left=36-(width/2);
	if(mode&WIN_SAV)
		gettext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT+left+width+1
			,SCRN_TOP+top+height,save_buf);
	i=0;
	in_win[i++]='É';
	in_win[i++]=hclr|(bclr<<4);
	for(c=1;c<width-1;c++) {
		in_win[i++]='Í';
		in_win[i++]=hclr|(bclr<<4); }
	in_win[i++]='»';
	in_win[i++]=hclr|(bclr<<4);
	in_win[i++]='º';
	in_win[i++]=hclr|(bclr<<4);

	if(plen) {
		in_win[i++]=SP;
		in_win[i++]=lclr|(bclr<<4); }

	for(c=0;prompt[c];c++) {
		in_win[i++]=prompt[c];
		in_win[i++]=lclr|(bclr<<4); }

	if(plen) {
		in_win[i++]=':';
		in_win[i++]=lclr|(bclr<<4);
		c++; }

	for(c=0;c<max+2;c++) {
		in_win[i++]=SP;
		in_win[i++]=lclr|(bclr<<4); }

	in_win[i++]='º';
	in_win[i++]=hclr|(bclr<<4);
	in_win[i++]='È';
	in_win[i++]=hclr|(bclr<<4);
	for(c=1;c<width-1;c++) {
		in_win[i++]='Í';
		in_win[i++]=hclr|(bclr<<4); }
	in_win[i++]='¼';
	in_win[i++]=hclr|(bclr<<4);
	puttext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT+left+width-1
		,SCRN_TOP+top+height-1,in_win);

	if(bclr==BLUE) {
		gettext(SCRN_LEFT+left+width,SCRN_TOP+top+1,SCRN_LEFT+left+width+1
			,SCRN_TOP+top+(height-1),shade);
		for(c=1;c<12;c+=2)
			shade[c]=DARKGRAY;
		puttext(SCRN_LEFT+left+width,SCRN_TOP+top+1,SCRN_LEFT+left+width+1
			,SCRN_TOP+top+(height-1),shade);
		gettext(SCRN_LEFT+left+2,SCRN_TOP+top+3,SCRN_LEFT+left+width+1
			,SCRN_TOP+top+height,shade);
		for(c=1;c<width*2;c+=2)
			shade[c]=DARKGRAY;
		puttext(SCRN_LEFT+left+2,SCRN_TOP+top+3,SCRN_LEFT+left+width+1
			,SCRN_TOP+top+height,shade); }

	textattr(lclr|(bclr<<4));
	if(!plen)
		gotoxy(SCRN_LEFT+left+2,SCRN_TOP+top+1);
	else
		gotoxy(SCRN_LEFT+left+plen+4,SCRN_TOP+top+1);
	i=ugetstr(str,max,kmode);
	if(mode&WIN_SAV)
		puttext(SCRN_LEFT+left,SCRN_TOP+top,SCRN_LEFT+left+width+1
			,SCRN_TOP+top+height,save_buf);
	showmouse();
	return(i);
}

/****************************************************************************/
/* Displays the message 'str' and waits for the user to select "OK"         */
/****************************************************************************/
void umsg(char *str)
{
	int i=0;
	char *ok[2]={"OK",""};

	if(api->mode&UIFC_INMSG)	/* non-cursive */
		return;
	api->mode|=UIFC_INMSG;
	if(api->savdepth) api->savnum++;
	ulist(WIN_SAV|WIN_MID,0,0,0,&i,0,str,ok);
	if(api->savdepth) api->savnum--;
	api->mode&=~UIFC_INMSG;
}

/****************************************************************************/
/* Gets a string of characters from the user. Turns cursor on. Allows 	    */
/* Different modes - K_* macros. ESC aborts input.                          */
/* Cursor should be at END of where string prompt will be placed.           */
/****************************************************************************/
static int ugetstr(char *outstr, int max, long mode)
{
	uchar   str[256],ins=0,buf[256],y;
	int		ch;
	int     i,j,k,f=0;	/* i=offset, j=length */
#ifndef __FLAT__
	union  REGS r;
#endif

	curs_set(1);
	y=wherey();
	if(mode&K_EDIT && outstr[0]) {
	/***
		truncsp(outstr);
	***/
		outstr[max]=0;
		textattr(hfclr|(hbclr<<4));
		cputs(outstr);
		textattr(lclr|(bclr<<4));
		strcpy(str,outstr);
		i=j=strlen(str);
#if 0
		while(inkey(1)==0) {
#ifndef __FLAT__
			if(api->mode&UIFC_MOUSE) {
			/* ToDo More mouse stuff */
			}
#endif
			mswait(1);
		}
#endif
		f=inkey(0);
		gotoxy(wherex()-i,y);
		if(f == CR || f >= 0xff)
		{
			cputs(outstr);
		}
		else
		{
			cprintf("%*s",i,"");
			gotoxy(wherex()-i,y);
			i=j=0;
		}
	}
	else
		i=j=0;

	ch=0;
	while(ch!=CR)
	{
		if(i>j) j=i;
#ifndef __FLAT__
		if(api->mode&UIFC_MOUSE)
		{
			/* ToDo More Mouse Stuff */
		}
#endif
		if(f || inkey(1))
		{
			if(f)
				ch=f;
			else
				ch=inkey(0);
			f=0;
			switch(ch)
			{
				case KEY_F(1):	/* F1 Help */
					help();
					continue;
				case KEY_LEFT:	/* left arrow */
					if(i)
					{
						gotoxy(wherex()-1,y);
						i--;
					}
					continue;
				case KEY_RIGHT:	/* right arrow */
					if(i<j)
					{
						gotoxy(wherex()+1,y);
						i++;
					}
					continue;
				case KEY_HOME:	/* home */
					if(i)
					{
						gotoxy(wherex()-i,y);
						i=0;
					}
					continue;
				case KEY_END:	/* end */
					if(i<j)
					{
						gotoxy(wherex()+(j-i),y);
						i=j;
					}
					continue;
				case KEY_IC:	/* insert */
					ins=!ins;
					if(ins)
					{
						curs_set(2);
						refresh();
					}
					else
					{
						curs_set(1);
						refresh();
					}
					continue;
				case BS:
				case KEY_BACKSPACE:
					if(i)
					{
						if(i==j)
						{
							cputs("\b \b");
							j--;
							i--;
						}
						else {
							gettext(wherex(),y,wherex()+(j-i),y,buf);
							puttext(wherex()-1,y,wherex()+(j-i)-1,y,buf);
							gotoxy(wherex()+(j-i),y);
							putch(SP);
							gotoxy(wherex()-((j-i)+2),y);
							i--;
							j--;
							for(k=i;k<j;k++)
								str[k]=str[k+1]; 
						}
						continue; 
					}
				case KEY_DC:	/* delete */
					if(i<j)
					{
						gettext(wherex()+1,y,wherex()+(j-i),y,buf);
						puttext(wherex(),y,wherex()+(j-i)-1,y,buf);
						gotoxy(wherex()+(j-i),y);
						putch(SP);
						gotoxy(wherex()-((j-i)+1),y);
						for(k=i;k<j;k++)
							str[k]=str[k+1];
						j--;
					}
					continue;
				case 3:
				case ESC:
					{
						curs_set(0);
						refresh();
						return(-1);
					}
				case CR:
					break;
				case 24:   /* ctrl-x  */
					if(j)
					{
						gotoxy(wherex()-i,y);
						cprintf("%*s",j,"");
						gotoxy(wherex()-j,y);
						i=j=0;
					}
					continue;
				case 25:   /* ctrl-y */
					if(i<j)
					{
						cprintf("%*s",(j-i),"");
						gotoxy(wherex()-(j-i),y);
						j=i;
					}
					continue;
			}
			if(mode&K_NUMBER && !isdigit(ch))
				continue;
			if(mode&K_ALPHA && !isalpha(ch))
				continue;
			if((ch>=SP || (ch==1 && mode&K_MSG)) && i<max && (!ins || j<max) && isprint(ch))
			{
				if(mode&K_UPPER)
					ch=toupper(ch);
				if(ins)
				{
					gettext(wherex(),y,wherex()+(j-i),y,buf);
					puttext(wherex()+1,y,wherex()+(j-i)+1,y,buf);
					for(k=++j;k>i;k--)
						str[k]=str[k-1];
				}
				putch(ch);
				str[i++]=ch; 
			} 
		}
		else
			mswait(1);
	}


	str[j]=0;
	if(mode&K_EDIT)
	{
		truncsp(str);
		if(strcmp(outstr,str))
			api->changes=1;
	}
	else
	{
		if(j)
			api->changes=1;
	}
	strcpy(outstr,str);
	curs_set(0);
	refresh();
	return(j);
}

/****************************************************************************/
/* Performs printf() through puttext() routine								*/
/****************************************************************************/
static int uprintf(int x, int y, unsigned char attr, char *fmat, ...)
{
	va_list argptr;
	char str[256],buf[512];
	int i,j;

    va_start(argptr,fmat);
    vsprintf(str,fmat,argptr);
    va_end(argptr);
    for(i=j=0;str[i];i++) {
        buf[j++]=str[i];
        buf[j++]=attr; }
    puttext(x,y,x+(i-1),y,buf);
    return(i);
}


/****************************************************************************/
/* Display bottom line of screen in inverse                                 */
/****************************************************************************/
void bottomline(int line)
{
	int i=4;

	uprintf(i,api->scrn_len+1,bclr|(cclr<<4),"F1 ");
	i+=3;
	uprintf(i,api->scrn_len+1,BLACK|(cclr<<4),"Help  ");
	i+=6;
	if(line&BL_GET) {
		uprintf(i,api->scrn_len+1,bclr|(cclr<<4),"F5 ");
		i+=3;
		uprintf(i,api->scrn_len+1,BLACK|(cclr<<4),"Copy Item  ");
		i+=11; }
	if(line&BL_PUT) {
		uprintf(i,api->scrn_len+1,bclr|(cclr<<4),"F6 ");
		i+=3;
		uprintf(i,api->scrn_len+1,BLACK|(cclr<<4),"Paste  ");
		i+=7; }
	if(line&BL_INS) {
		uprintf(i,api->scrn_len+1,bclr|(cclr<<4),"INS ");
		i+=4;
		uprintf(i,api->scrn_len+1,BLACK|(cclr<<4),"Add Item  ");
		i+=10; }
	if(line&BL_DEL) {
		uprintf(i,api->scrn_len+1,bclr|(cclr<<4),"DEL ");
		i+=4;
		uprintf(i,api->scrn_len+1,BLACK|(cclr<<4),"Delete Item  ");
		i+=13; }
	uprintf(i,api->scrn_len+1,bclr|(cclr<<4),"ESC ");	/* Backspace is no good no way to abort editing */
	i+=4;
	uprintf(i,api->scrn_len+1,BLACK|(cclr<<4),"Exit");
	i+=4;
	gotoxy(i,api->scrn_len+1);
	textattr(BLACK|(cclr<<4));
	clrtoeol();
}


/*****************************************************************************/
/* Generates a 24 character ASCII string that represents the time_t pointer  */
/* Used as a replacement for ctime()                                         */
/*****************************************************************************/
char *utimestr(time_t *intime)
{
	static char str[25];
	char wday[4],mon[4],mer[3],hour;
	struct tm *gm;

	gm=localtime(intime);
	switch(gm->tm_wday) {
		case 0:
			strcpy(wday,"Sun");
			break;
		case 1:
			strcpy(wday,"Mon");
			break;
		case 2:
			strcpy(wday,"Tue");
			break;
		case 3:
			strcpy(wday,"Wed");
			break;
		case 4:
			strcpy(wday,"Thu");
			break;
		case 5:
			strcpy(wday,"Fri");
			break;
		case 6:
			strcpy(wday,"Sat");
			break; }
	switch(gm->tm_mon) {
		case 0:
			strcpy(mon,"Jan");
			break;
		case 1:
			strcpy(mon,"Feb");
			break;
		case 2:
			strcpy(mon,"Mar");
			break;
		case 3:
			strcpy(mon,"Apr");
			break;
		case 4:
			strcpy(mon,"May");
			break;
		case 5:
			strcpy(mon,"Jun");
			break;
		case 6:
			strcpy(mon,"Jul");
			break;
		case 7:
			strcpy(mon,"Aug");
			break;
		case 8:
			strcpy(mon,"Sep");
			break;
		case 9:
			strcpy(mon,"Oct");
			break;
		case 10:
			strcpy(mon,"Nov");
			break;
		case 11:
			strcpy(mon,"Dec");
			break; }
	if(gm->tm_hour>12) {
		strcpy(mer,"pm");
		hour=gm->tm_hour-12; }
	else {
		if(!gm->tm_hour)
			hour=12;
		else
			hour=gm->tm_hour;
		strcpy(mer,"am"); }
	sprintf(str,"%s %s %02d %4d %02d:%02d %s",wday,mon,gm->tm_mday,1900+gm->tm_year
		,hour,gm->tm_min,mer);
	return(str);
}

/****************************************************************************/
/* Status popup/down function, see uifc.h for details.						*/
/****************************************************************************/
#define UPOP_ROW	12
#define UPOP_ROWS	3
#define UPOP_COLS	80
void upop(char *str)
{
	static char sav[UPOP_COLS*UPOP_ROWS*2];
	char buf[UPOP_COLS*UPOP_ROWS*2];
	int i,j,k;

	reset_dynamic();
	hidemouse();
	if(!str) {
		puttext(1,UPOP_ROW,UPOP_COLS,UPOP_ROW+UPOP_ROWS,sav);
		showmouse();
		return; }
	gettext(1,UPOP_ROW,UPOP_COLS,UPOP_ROW+UPOP_ROWS,sav);
	memset(buf,SP,UPOP_COLS*UPOP_ROWS*2);
	for(i=1;i<(UPOP_COLS+1)*UPOP_COLS*2;i+=2)
		buf[i]=(hclr|(bclr<<4));
		buf[0]='Ú';
	for(i=2;i<UPOP_COLS*2;i+=2)
        buf[i]='Ä';
		buf[i]='¿'; i+=2;
        buf[i]='³'; i+=2;
	i+=2;
	k=strlen(str);
	i+=(((UPOP_COLS-k)/2)*2);
	for(j=0;j<k;j++,i+=2) {
		buf[i]=str[j];
		buf[i+1]|=BLINK; }
	i=((25*2)+1)*2;
        buf[i]='³'; i+=2;
        buf[i]='À'; i+=2;
	for(;i<((26*3)-1)*2;i+=2)
        buf[i]='Ä';
    buf[i]='Ù';

	puttext(1,UPOP_ROW,UPOP_COLS,UPOP_ROW+UPOP_ROWS,buf);
	showmouse();
}

/****************************************************************************/
/* Sets the current help index by source code file and line number.			*/
/****************************************************************************/
void sethelp(int line, char* file)
{
    helpline=line;
    helpfile=file;
}

/****************************************************************************/
/* Shows a scrollable text buffer - optionally parsing "help markup codes"	*/
/****************************************************************************/
void showbuf(int mode, int left, int top, int width, int height, char *title, char *hbuf, int *curp, int *barp)
{
	char *savscrn,inverse=0,high=0;
	char *buf;
	char *textbuf;
    char *p;
	uint i,j,k,len;
	int  old_curs;
	int	 lines;
#ifndef __FLAT__
	union  REGS r;
#endif
	int pad=1;
	int	is_redraw=0;

	if(top+height>api->scrn_len-3)
		height=(api->scrn_len-3)-top;
	if(!width || width<strlen(title)+6)
		width=strlen(title)+6;
	if(width>api->scrn_width)
		width=api->scrn_width;
	if(mode&WIN_L2R)
		left=(api->scrn_width-width)/2+1;
	else if(mode&WIN_RHT)
		left=api->scrn_width-(width+4+left);
	if(mode&WIN_T2B)
		top=(api->scrn_len/2)-(height/2)-2;
	else if(mode&WIN_BOT)
		top=api->scrn_len-height-3-top;

	if(mode&WIN_PACK)
		pad=0;

	old_curs=curs_set(0);

	/* Dynamic Menus */
#ifdef __unix__
	if(mode&WIN_DYN
			&& curp != NULL 
			&& barp != NULL 
			&& last_menu_cur==curp
			&& last_menu_bar==barp
			&& save_menu_cur==*curp
			&& save_menu_bar==*barp)
		is_redraw=1;
#endif

	if((savscrn=(char *)MALLOC(api->scrn_width*api->scrn_len*2))==NULL) {
		cprintf("UIFC line %d: error allocating %u bytes\r\n"
			,__LINE__,api->scrn_width*25*2);
		curs_set(1);
		return; }
	if((buf=(char *)MALLOC(width*height*2))==NULL) {
		cprintf("UIFC line %d: error allocating %u bytes\r\n"
			,__LINE__,width*21*2);
		FREE(savscrn);
		curs_set(1);
		return; }
	hidemouse();
	gettext(1,1,api->scrn_width,api->scrn_len,savscrn);

	if(!is_redraw) {
		memset(buf,SP,width*height*2);
		for(i=1;i<width*height*2;i+=2)
			buf[i]=(hclr|(bclr<<4));
	    buf[0]='Ú';
		j=strlen(title);
		if(j>width-6)
			*(title+width-6)=0;
		for(i=2;i<((width-6)/2-(j/2))*2;i+=2)
   		      buf[i]='Ä';
	    buf[i]='´'; i+=4;
		for(p=title;*p;p++) {
			buf[i]=*p;
			i+=2;
		}
		i+=2;
   		buf[i]='Ã'; i+=2;
		for(j=i;j<((width-1)*2);j+=2)
   		    buf[j]='Ä';
		i=j;
    	buf[i]='¿'; i+=2;
		j=i;	/* leave i alone */
		for(k=0;k<(height-2);k++) { 		/* the sides of the box */
	        buf[j]='³'; j+=2;
			j+=((width-2)*2);
        	buf[j]='³'; j+=2; }
	    buf[j]='À'; j+=2;
		if(!(mode&WIN_DYN) && (width>31)) {
			for(k=j;k<j+(((width-4)/2-13)*2);k+=2)
				buf[k]='Ä';
			buf[k]='´'; k+=4;
			buf[k]='H'; k+=2;
			buf[k]='i'; k+=2;
			buf[k]='t'; k+=4;
			buf[k]='a'; k+=2;
			buf[k]='n'; k+=2;
			buf[k]='y'; k+=4;
			buf[k]='k'; k+=2;
			buf[k]='e'; k+=2;
			buf[k]='y'; k+=4;
			buf[k]='t'; k+=2;
			buf[k]='o'; k+=4;
			buf[k]='c'; k+=2;
			buf[k]='o'; k+=2;
			buf[k]='n'; k+=2;
			buf[k]='t'; k+=2;
			buf[k]='i'; k+=2;
			buf[k]='n'; k+=2;
			buf[k]='u'; k+=2;
			buf[k]='e'; k+=4;
	    	buf[k]='Ã'; k+=2;
			for(j=k;j<k+(((width-4)/2-12)*2);j+=2)
		        buf[j]='Ä';
		}	
		else {
			for(k=j;k<j+((width-2)*2);k+=2)
				buf[k]='Ä';
			j=k;
		}
	    buf[j]='Ù';
		puttext(left,top+1,left+width-1,top+height,buf);
	}
	len=strlen(hbuf);
	
	i=0;
	lines=1;		/* The first one is free */
	k=0;
	for(j=0;j<len;j++) {
		k++;
		if(hbuf[j]==LF)
			lines++;
		if(k>72) {
			k=0;
			lines++;
		}
	}
	if(lines < height-2-pad-pad)
		lines=height-2-pad-pad;

	if((buf=(char *)textbuf=MALLOC((width-2-pad-pad)*lines*2))==NULL) {
		cprintf("UIFC line %d: error allocating %u bytes\r\n"
			,__LINE__,(width-2-pad-pad)*lines*2);
		FREE(savscrn);
		FREE(buf);
		curs_set(1);
		return; }
	memset(textbuf,SP,(width-2-pad-pad)*lines*2);
	for(i=1;i<(width-2-pad-pad)*lines*2;i+=2)
		buf[i]=(hclr|(bclr<<4));
	i=0;
	for(j=0;j<len;j++,i+=2) {
		if(hbuf[j]==LF) {
			i+=2;
			while(i%((width-2-pad-pad)*2)) i++; i-=2; }
		else if(mode&WIN_HLP && (hbuf[j]==2 || hbuf[j]=='~')) {		 /* Ctrl-b toggles inverse */
			inverse=!inverse;
			i-=2; }
		else if(mode&WIN_HLP && (hbuf[j]==1 || hbuf[j]=='`')) {		 /* Ctrl-a toggles high intensity */
			high=!high;
			i-=2; }
		else if(hbuf[j]!=CR) {
			textbuf[i]=hbuf[j];
			textbuf[i+1]=inverse ? (bclr|(cclr<<4))
				: high ? (hclr|(bclr<<4)) : (lclr|(bclr<<4)); } }
	showmouse();
	i=0;
	p=textbuf;
	if(mode&WIN_DYN) {
		puttext(left+1+pad,top+2+pad,left+width-2-pad,top+height-1-pad,p);
	}
	else {
		while(i==0) {
			puttext(left+1+pad,top+2+pad,left+width-2-pad,top+height-1-pad,p);
			if(inkey(1)) {
				switch(inkey(0)) {
					case KEY_HOME:	/* home */
						p=textbuf;
						break;

					case KEY_UP:	/* up arrow */
						p = p-((width-4)*2);
						if(p<textbuf)
							p=textbuf;
						break;
					
					case KEY_PPAGE:	/* PgUp */
						p = p-((width-4)*2*(height-5));
						if(p<textbuf)
							p=textbuf;
						break;

					case KEY_NPAGE:	/* PgDn */
						p=p+(width-4)*2*(height-5);
						if(p > textbuf+(lines-height+1)*(width-4)*2)
							p=textbuf+(lines-height+1)*(width-4)*2;
						if(p<textbuf)
							p=textbuf;
						break;

					case KEY_END:	/* end */
						p=textbuf+(lines-height+1)*(width-4)*2;
						if(p<textbuf)
							p=textbuf;
						break;

					case KEY_DOWN:	/* dn arrow */
						p = p+((width-4)*2);
						if(p > textbuf+(lines-height+1)*(width-4)*2)
							p=textbuf+(lines-height+1)*(width-4)*2;
						if(p<textbuf)
							p=textbuf;
						break;

					default:
						i=1;
				}
			}
			mswait(1);
		}
		#ifndef __FLAT__
			if(api->mode&UIFC_MOUSE) {
				/* ToDo more mouse stiff */
			}
		#endif

		hidemouse();
		puttext(1,1,api->scrn_width,api->scrn_len,savscrn);
		showmouse();
	}
#ifdef __unix__
	if(is_redraw)			/* Force redraw of menu also. */
		reset_dynamic();
#endif
	FREE(savscrn);
	FREE(buf);
	if(old_curs != ERR)
		curs_set(old_curs);
}

/************************************************************/
/* Help (F1) key function. Uses helpbuf as the help input.	*/
/************************************************************/
void help()
{
	char hbuf[HELPBUF_SIZE],str[256];
    char *p;
	unsigned short line;
	long l;
	FILE *fp;
	int  old_curs;
#ifndef __FLAT__
	union  REGS r;
#endif

	old_curs=curs_set(0);

	if(!api->helpbuf) {
		if((fp=fopen(api->helpixbfile,"rb"))==NULL)
			sprintf(hbuf," ERROR  Cannot open help index:\r\n          %s"
				,api->helpixbfile);
		else {
			p=strrchr(helpfile,'/');
			if(p==NULL)
				p=strrchr(helpfile,'\\');
			if(p==NULL)
				p=helpfile;
			else
				p++;
			l=-1L;
			while(!feof(fp)) {
				if(!fread(str,12,1,fp))
					break;
				str[12]=0;
				fread(&line,2,1,fp);
				if(stricmp(str,p) || line!=helpline) {
					fseek(fp,4,SEEK_CUR);
					continue; }
				fread(&l,4,1,fp);
				break; }
			fclose(fp);
			if(l==-1L)
				sprintf(hbuf," ERROR  Cannot locate help key (%s:%u) in:\r\n"
					"         %s",p,helpline,api->helpixbfile);
			else {
				if((fp=fopen(api->helpdatfile,"rb"))==NULL)
					sprintf(hbuf," ERROR  Cannot open help file:\r\n          %s"
						,api->helpdatfile);
				else {
					fseek(fp,l,SEEK_SET);
					fread(hbuf,HELPBUF_SIZE,1,fp);
					fclose(fp); } } } }
	else
		strcpy(hbuf,api->helpbuf);

	showbuf(WIN_MID|WIN_HLP, 0, 0, 76, api->scrn_len-2, "Online Help", hbuf, NULL, NULL);
}

static int puttext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	int x,y;
	int fillpos=0;
	unsigned char attr;
	unsigned char fill_char;
	unsigned char orig_attr;
	int oldx, oldy;

	getyx(stdscr,oldy,oldx);	
	orig_attr=lastattr;
	for(y=sy-1;y<=ey-1;y++)
	{
		for(x=sx-1;x<=ex-1;x++)
		{
			fill_char=fill[fillpos++];
			attr=fill[fillpos++];
			textattr(attr);
			move(y, x);
			_putch(fill_char,FALSE);
		}
	}
	textattr(orig_attr);
	move(oldy, oldx);
	refresh();
	return(1);
}

static int gettext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	int x,y;
	int fillpos=0;
	chtype attr;
	unsigned char attrib;
	unsigned char colour;
	int oldx, oldy;
	unsigned char thischar;
	int	ext_char;

	getyx(stdscr,oldy,oldx);	
	for(y=sy-1;y<=ey-1;y++)
	{
		for(x=sx-1;x<=ex-1;x++)
		{
			attr=mvinch(y, x);
			if(attr&A_ALTCHARSET && !(api->mode&UIFC_IBM)){
				ext_char=A_ALTCHARSET|(attr&255);
				/* likely ones */
				if (ext_char == ACS_CKBOARD)
				{
					thischar=176;
				}
				else if (ext_char == ACS_BOARD)
				{
					thischar=177;
				}
				else if (ext_char == ACS_BSSB)
				{
					thischar=218;
				}
				else if (ext_char == ACS_SSBB)
				{
					thischar=192;
				}
				else if (ext_char == ACS_BBSS)
				{
					thischar=191;
				}
				else if (ext_char == ACS_SBBS)
				{
					thischar=217;
				}
				else if (ext_char == ACS_SBSS)
				{
					thischar=180;
				}
				else if (ext_char == ACS_SSSB)
				{
					thischar=195;
				}
				else if (ext_char == ACS_SSBS)
				{
					thischar=193;
				}
				else if (ext_char == ACS_BSSS)
				{
					thischar=194;
				}
				else if (ext_char == ACS_BSBS)
				{
					thischar=196;
				}
				else if (ext_char == ACS_SBSB)
				{
					thischar=179;
				}
				else if (ext_char == ACS_SSSS)
				{
					thischar=197;
				}
				else if (ext_char == ACS_BLOCK)
				{
					thischar=219;
				}
				else if (ext_char == ACS_UARROW)
				{
					thischar=30;
				}
				else if (ext_char == ACS_DARROW)
				{
					thischar=31;
				}
				
				/* unlikely (Not in ncurses) */
				else if (ext_char == ACS_SBSD)
				{
					thischar=181;
				}
				else if (ext_char == ACS_DBDS)
				{
					thischar=182;
				}
				else if (ext_char == ACS_BBDS)
				{
					thischar=183;
				}
				else if (ext_char == ACS_BBSD)
				{
					thischar=184;
				}
				else if (ext_char == ACS_DBDD)
				{
					thischar=185;
				}
				else if (ext_char == ACS_DBDB)
				{
					thischar=186;
				}
				else if (ext_char == ACS_BBDD)
				{
					thischar=187;
				}
				else if (ext_char == ACS_DBBD)
				{
					thischar=188;
				}
				else if (ext_char == ACS_DBBS)
				{
					thischar=189;
				}
				else if (ext_char == ACS_SBBD)
				{
					thischar=190;
				}
				else if (ext_char == ACS_SDSB)
				{
					thischar=198;
				}
				else if (ext_char == ACS_DSDB)
				{
					thischar=199;
				}
				else if (ext_char == ACS_DDBB)
				{
					thischar=200;
				}
				else if (ext_char == ACS_BDDB)
				{
					thischar=201;
				}
				else if (ext_char == ACS_DDBD)
				{
					thischar=202;
				}
				else if (ext_char == ACS_BDDD)
				{
					thischar=203;
				}
				else if (ext_char == ACS_DDDB)
				{
					thischar=204;
				}
				else if (ext_char == ACS_BDBD)
				{
					thischar=205;
				}
				else if (ext_char == ACS_DDDD)
				{
					thischar=206;
				}
				else if (ext_char == ACS_SDBD)
				{
					thischar=207;
				}
				else if (ext_char == ACS_DSBS)
				{
					thischar=208;
				}
				else if (ext_char == ACS_BDSD)
				{
					thischar=209;
				}
				else if (ext_char == ACS_BSDS)
				{
					thischar=210;
				}
				else if (ext_char == ACS_DSBB)
				{
					thischar=211;
				}
				else if (ext_char == ACS_SDBB)
				{
					thischar=212;
				}
				else if (ext_char == ACS_BDSB)
				{
					thischar=213;
				}
				else if (ext_char == ACS_BSDB)
				{
					thischar=214;
				}
				else if (ext_char == ACS_DSDS)
				{
					thischar=215;
				}
				else if (ext_char == ACS_SDSD)
				{
					thischar=216;
				}
				else
				{
					thischar=attr&255;
				}
			}
			else
				thischar=attr;
			fill[fillpos++]=(unsigned char)(thischar);
			attrib=0;
			if (attr & A_BOLD)  
			{
				attrib |= 8;
			}
			if (attr & A_BLINK)
			{
				attrib |= 128;
			}
			colour=PAIR_NUMBER(attr&A_COLOR)-1;
			colour=((colour&56)<<1)|(colour&7);
			fill[fillpos++]=colour|attrib;
		}
	}
	move(oldy, oldx);
	return(1);
}

static void textattr(unsigned char attr)
{
	int   attrs=A_NORMAL;
	short	colour;

	if (lastattr==attr)
		return;

	lastattr=attr;
	
	if (attr & 8)  {
		attrs |= A_BOLD;
	}
	if (attr & 128)
	{
		attrs |= A_BLINK;
	}
	attrset(attrs);
	colour = COLOR_PAIR( ((attr&7)|((attr>>1)&56))+1 );
	#ifdef NCURSES_VERSION_MAJOR
	color_set(colour,NULL);
	#endif
	bkgdset(colour);
}

static int kbhit(void)
{
	struct timeval timeout;
	fd_set	rfds;
	
	timeout.tv_sec=1;
	timeout.tv_usec=0;
	FD_ZERO(&rfds);
	FD_SET(fileno(stdin),&rfds);

	return(select(fileno(stdin)+1,&rfds,NULL,NULL,&timeout));
}

#ifndef __QNX__
static void delay(msec)
{
	usleep(msec*1000);
}
#endif

static int wherey(void)
{
	int x,y;
	getyx(stdscr,y,x);
	return(y+1);
}

static int wherex(void)
{
	int x,y;
	getyx(stdscr,y,x);
	return(x+1);
}

static void _putch(unsigned char ch, BOOL refresh_now)
{
	int	cha;

	if(!(api->mode&UIFC_IBM))
	{
		switch(ch)
		{
			case 30:
				cha=ACS_UARROW;
				break;
			case 31:
				cha=ACS_DARROW;
				break;
			case 176:
				cha=ACS_CKBOARD;
				break;
			case 177:
				cha=ACS_BOARD;
				break;
			case 178:
				cha=ACS_BOARD;
				break;
			case 179:
				cha=ACS_SBSB;
				break;
			case 180:
				cha=ACS_SBSS;
				break;
			case 181:
				cha=ACS_SBSD;
				break;
			case 182:
				cha=ACS_DBDS;
				break;
			case 183:
				cha=ACS_BBDS;
				break;
			case 184:
				cha=ACS_BBSD;
				break;
			case 185:
				cha=ACS_DBDD;
				break;
			case 186:
				cha=ACS_DBDB;
				break;
			case 187:
				cha=ACS_BBDD;
				break;
			case 188:
				cha=ACS_DBBD;
				break;
			case 189:
				cha=ACS_DBBS;
				break;
			case 190:
				cha=ACS_SBBD;
				break;
			case 191:
				cha=ACS_BBSS;
				break;
			case 192:
				cha=ACS_SSBB;
				break;
			case 193:
				cha=ACS_SSBS;
				break;
			case 194:
				cha=ACS_BSSS;
				break;
			case 195:
				cha=ACS_SSSB;
				break;
			case 196:
				cha=ACS_BSBS;
				break;
			case 197:
				cha=ACS_SSSS;
				break;
			case 198:
				cha=ACS_SDSB;
				break;
			case 199:
				cha=ACS_DSDB;
				break;
			case 200:
				cha=ACS_DDBB;
				break;
			case 201:
				cha=ACS_BDDB;
				break;
			case 202:
				cha=ACS_DDBD;
				break;
			case 203:
				cha=ACS_BDDD;
				break;
			case 204:
				cha=ACS_DDDB;
				break;
			case 205:
				cha=ACS_BDBD;
				break;
			case 206:
				cha=ACS_DDDD;
				break;
			case 207:
				cha=ACS_SDBD;
				break;
			case 208:
				cha=ACS_DSBS;
				break;
			case 209:
				cha=ACS_BDSD;
				break;
			case 210:
				cha=ACS_BSDS;
				break;
			case 211:
				cha=ACS_DSBB;
				break;
			case 212:
				cha=ACS_SDBB;
				break;
			case 213:
				cha=ACS_BDSB;
				break;
			case 214:
				cha=ACS_BSDB;
				break;
			case 215:
				cha=ACS_DSDS;
				break;
			case 216:
				cha=ACS_SDSD;
				break;
			case 217:
				cha=ACS_SBBS;
				break;
			case 218:
				cha=ACS_BSSB;
				break;
			case 219:
				cha=ACS_BLOCK;
				break;
			default:
				cha=ch;
		}
	}
	else
	{
		switch(ch) {
			case 30:
				cha=ACS_UARROW;
				break;
			case 31:
				cha=ACS_DARROW;
				break;
			default:
				cha=ch;
		}
	}
			

	addch(cha);
	if(refresh_now)
		refresh();
}

static int cprintf(char *fmat, ...)
{
    va_list argptr;
	char	str[MAX_BFLN];
	int		pos;

    va_start(argptr,fmat);
    vsprintf(str,fmat,argptr);
    va_end(argptr);
	for(pos=0;str[pos];pos++)
	{
		_putch(str[pos],FALSE);
	}
	refresh();
    return(1);
}

static void cputs(char *str)
{
	int		pos;

	for(pos=0;str[pos];pos++)
	{
		_putch(str[pos],FALSE);
	}
	refresh();
}

static void gotoxy(int x, int y)
{
	move(y-1,x-1);
	refresh();
}
