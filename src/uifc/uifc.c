/* uifc.c */

/* Original implementation of UIFC (user interface) library based on conio */

/* $Id$ */

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

#include "uifc.h"
#include <share.h>
#include <conio.h>

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

static char hclr,lclr,bclr,cclr,show_free_mem=0;
static int cursor;
static char* helpfile=0;
static uint helpline=0;
static char blk_scrn[MAX_BFLN];
static win_t sav[MAX_BUFS];
static uint max_opts=MAX_OPTS;
static uifcapi_t* api;

/* Prototypes */
static int  uprintf(int x, int y, char attr, char *fmt,...);
static void bottomline(int line);
static char *utimestr(time_t *intime);
static void help(void);
static int	getstr(char *outstr, int max, long mode);

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

#ifdef __FLAT__
int inkey(int mode)
{
	int c;

if(mode)
	return(kbhit());
c=getch();
if(!c)
	c=(getch()<<8);
return(c);
}
#else
int inkey(int mode)
{
if(mode)
	return(bioskey(1));
while(!bioskey(1));
return(bioskey(0));
}
#endif

static uint mousecursor=0x28;

/****************************************************************************/
/* Initialization function, see uifc.h for details.							*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uifcini(uifcapi_t* uifcapi)
{
	int 	i;
	struct	text_info txtinfo;
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

    if(api->scrn_len!=0) {
        switch(api->scrn_len) {
            case 14:
                textmode(C80X14);
                break;
            case 21:
                textmode(C80X21);
                break;
            case 25:
                textmode(C80);
                break;
            case 28:
                textmode(C80X28);
                break;
            case 43:
                textmode(C80X43);
                break;
            case 50:
                textmode(C80X50);
                break;
            case 60:
                textmode(C80X60);
                break;
            default:
                textmode(C4350);
                break;
        }
    }

    clrscr();
    gettextinfo(&txtinfo);
    /* unsupported mode? */
    if(txtinfo.screenheight<MIN_LINES
        || txtinfo.screenheight>MAX_LINES
        || txtinfo.screenwidth<80) {
        textmode(C80);  /* set mode to 80x25*/
        gettextinfo(&txtinfo);
    }

    api->scrn_len=txtinfo.screenheight;
    if(api->scrn_len<MIN_LINES || api->scrn_len>MAX_LINES) {
        cprintf("\7UIFC: Screen length (%u) must be between %d and %d lines\r\n"
            ,api->scrn_len,MIN_LINES,MAX_LINES);
        return(-2);
    }
    api->scrn_len--; /* account for status line */

    if(txtinfo.screenwidth<80) {
        cprintf("\7UIFC: Screen width (%u) must be at least 80 characters\r\n"
            ,txtinfo.screenwidth);
        return(-3);
    }

#ifndef __FLAT__

    r.w.ax=0x0000;				/* reset mouse and get driver status */
    INT_86(0x33,&r,&r);

    if(r.w.ax==0xffff) {		/* mouse driver installed */
        uifc_status|=UIFC_MOUSE;


        r.w.ax=0x0020;			/* enable mouse driver */
        INT_86(0x33,&r,&r);

        r.w.ax=0x000a;			/* set text pointer type */
        r.w.bx=0x0000;			/* software cursor */
        r.w.cx=0x77ff;
        r.w.dx=mousecursor<<8;
        INT_86(0x33,&r,&r);

        r.w.ax=0x0013;			/* set double speed threshold */
        r.w.dx=32;				/* double speed threshold */
        INT_86(0x33,&r,&r);

        r.w.ax=0x0001;			/* show mouse pointer */
        INT_86(0x33,&r,&r); }

    #endif


    if(!(api->mode&UIFC_COLOR)
        && (api->mode&UIFC_MONO
            || txtinfo.currmode==MONO || txtinfo.currmode==BW80)) {
        bclr=BLACK;
        hclr=WHITE;
        lclr=LIGHTGRAY;
        cclr=LIGHTGRAY;
    } else {
        bclr=BLUE;
        hclr=YELLOW;
        lclr=WHITE;
        cclr=CYAN;
    }
    for(i=0;i<MAX_BFLN;i+=2) {
        blk_scrn[i]='°';
        blk_scrn[i+1]=cclr|(bclr<<4);
    }

    cursor=_NOCURSOR;
    _setcursortype(cursor);

    return(0);
}

static void hidemouse(void)
{
#ifndef __FLAT__
	union  REGS r;

if(uifc_status&UIFC_MOUSE) {
	r.w.ax=0x0002;			/* hide mouse pointer */
	INT_86(0x33,&r,&r); }
#endif
}

static void showmouse(void)
{
#ifndef __FLAT__
	union  REGS r;

if(uifc_status&UIFC_MOUSE) {
	r.w.ax=0x0001;			/* show mouse pointer */
	INT_86(0x33,&r,&r); }
#endif
}


void uifcbail(void)
{

hidemouse();
_setcursortype(_NORMALCURSOR);
textattr(LIGHTGRAY);
clrscr();
}

/****************************************************************************/
/* Clear screen, fill with background attribute, display application title.	*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uscrn(char *str)
{
    textattr(bclr|(cclr<<4));
    gotoxy(1,1);
    clreol();
    gotoxy(3,1);
    cputs(str);
    if(!puttext(1,2,80,api->scrn_len,blk_scrn))
        return(-1);
    gotoxy(1,api->scrn_len+1);
    clreol();
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
static void timedisplay(void)
{
	static time_t savetime;
	time_t now;

now=time(NULL);
if(difftime(now,savetime)>=60) {
	uprintf(55,1,bclr|(cclr<<4),utimestr(&now));
	savetime=now; }
}

/****************************************************************************/
/* Truncates white-space chars off end of 'str'								*/
/****************************************************************************/
static void truncsp(char *str)
{
	uint c;

	c=strlen(str);
	while(c && (uchar)str[c-1]<=SP) c--;
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

#ifndef __FLAT__
	union REGS reg,r;

hidemouse();

r.w.ax=0x0006;	/* Get button release info */
r.w.bx=0x0000;	/* Left button */
INT_86(0x33,&r,&r);  /* Clears any buffered mouse clicks */

r.w.ax=0x0006;	/* Get button release info */
r.w.bx=0x0001;	/* Right button */
INT_86(0x33,&r,&r);  /* Clears any buffered mouse clicks */

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
	sav[api->savnum].bot=SCRN_TOP+top+height; }


#ifndef __FLAT__
if(show_free_mem) {
	#if defined(__LARGE__) || defined(__HUGE__) || defined(__COMPACT__)
	uprintf(58,1,bclr|(cclr<<4),"%10ld bytes free",farcoreleft());
	#else
	uprintf(58,1,bclr|(cclr<<4),"%10u bytes free",coreleft());
	#endif
	}
#endif


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
	*(ptr++)='[';
	*(ptr++)=hclr|(bclr<<4);
	*(ptr++)='þ';
	*(ptr++)=lclr|(bclr<<4);
	*(ptr++)=']';
	*(ptr++)=hclr|(bclr<<4);
	*(ptr++)='[';
	*(ptr++)=hclr|(bclr<<4);
	*(ptr++)='?';
	*(ptr++)=lclr|(bclr<<4);
	*(ptr++)=']';
	*(ptr++)=hclr|(bclr<<4);
	i=6; }
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
		a=bclr|(LIGHTGRAY<<4);
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

		r.w.ax=0x0003;		/* Get button status and mouse position */
		INT_86(0x33,&r,&r);

		if(r.w.bx&1) {		/* Left button down */

			if(r.w.cx/8>=SCRN_LEFT+left
				&& r.w.cx/8<=SCRN_LEFT+left+width
				&& r.w.dx/8>=SCRN_TOP+top+2
				&& r.w.dx/8<=(SCRN_TOP+top+height)-3) {

				hidemouse();
				gettext(SCRN_LEFT+3+left,SCRN_TOP+y
					,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
				for(i=1;i<width*2;i+=2)
					line[i]=lclr|(bclr<<4);
				puttext(SCRN_LEFT+3+left,SCRN_TOP+y
					,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);

				(*cur)=(r.w.dx/8)-(SCRN_TOP+top+2);
				if(bar)
					(*bar)=(*cur);
				y=top+3+((r.w.dx/8)-(SCRN_TOP+top+2));

				gettext(SCRN_LEFT+3+left,SCRN_TOP+y
					,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
				for(i=1;i<width*2;i+=2)
					line[i]=bclr|(LIGHTGRAY<<4);
				puttext(SCRN_LEFT+3+left,SCRN_TOP+y
					,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
				showmouse(); } }

		r.w.ax=0x0006;		/* Get button release information */
		r.w.bx=0x0000;		/* left button */
		INT_86(0x33,&r,&r);

		if(r.w.bx) {	  /* Left button release same as CR */

			if(r.w.cx/8>=SCRN_LEFT+left
				&& r.w.cx/8<=SCRN_LEFT+left+width
				&& r.w.dx/8>=SCRN_TOP+top+2
				&& r.w.dx/8<=(SCRN_TOP+top+height)-3) {

				(*cur)=(r.w.dx/8)-(SCRN_TOP+top+2);
				if(bar)
					(*bar)=(*cur);
				y=top+3+((r.w.dx/8)-(SCRN_TOP+top+2));

				if(!opts || (mode&WIN_XTR && (*cur)==opts-1))
					continue;

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
				return(*cur); }
			else if(r.w.cx/8>=SCRN_LEFT+left+3
				&& r.w.cx/8<=SCRN_LEFT+left+5
				&& r.w.dx/8==(SCRN_TOP+top)-1)		/* Clicked help icon */
				help();
			else
				goto hitesc; }

		r.w.ax=0x0006;		/* Get button release information */
		r.w.bx=0x0001;		/* right button */
		INT_86(0x33,&r,&r);

		if(r.w.bx) {	  /* Right button down, same as ESC */
hitesc:
            if(mode&WIN_ESC || (mode&WIN_CHE && api->changes)
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
            return(-1); }
				}
#endif

	if(inkey(1)) {
		i=inkey(0);
		if(!(i&0xff)) {
			s=0;
			switch(i>>8) {
				case 71:	/* home */
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
							,bclr|(LIGHTGRAY<<4)
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
						line[i]=bclr|(LIGHTGRAY<<4);
					puttext(SCRN_LEFT+3+left,SCRN_TOP+y
						,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
					showmouse();
					break;
				case 72:	/* up arrow */
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
								,i==opts-1 ? bclr|(LIGHTGRAY<<4)
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
							,bclr|(LIGHTGRAY<<4)
							,"%-*.*s",width-4,width-4,option[*cur]);
						showmouse(); }
					else {
						hidemouse();
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=bclr|(LIGHTGRAY<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
						showmouse(); }
					break;
#if 0
				case 0x49;	/* PgUp */
				case 0x51;	/* PgDn */
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
							,i==(*cur) bclr|(LIGHTGRAY<<4) : lclr|(bclr<<4)
							,"%-*.*s",width-4,width-4,option[i]);
					y=top+height-2;
					if(bar)
						(*bar)=height-5;
					gettext(SCRN_LEFT+3+left,SCRN_TOP+y
						,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
					for(i=1;i<148;i+=2)
						line[i]=bclr|(LIGHTGRAY<<4);
					puttext(SCRN_LEFT+3+left,SCRN_TOP+y
						,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
					showmouse();
                    break;
#endif
				case 79:	/* end */
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
								,i==opts-1 ? bclr|(LIGHTGRAY<<4)
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
						line[i]=bclr|(LIGHTGRAY<<4);
					puttext(SCRN_LEFT+3+left,SCRN_TOP+y
						,SCRN_LEFT+left+width-2,SCRN_TOP+y,line);
					showmouse();
					break;
				case 80:	/* dn arrow */
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
							,bclr|(LIGHTGRAY<<4)
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
							,bclr|(LIGHTGRAY<<4)
							,"%-*.*s",width-4,width-4,option[*cur]);
						showmouse(); }
					else {
						hidemouse();
						gettext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y
							,line);
						for(i=1;i<width*2;i+=2)
							line[i]=bclr|(LIGHTGRAY<<4);
						puttext(SCRN_LEFT+3+left,SCRN_TOP+y
							,SCRN_LEFT+left+width-2,SCRN_TOP+y
							,line);
						showmouse(); }
					break;
				case 0x3b:	/* F1 */
					help();
					break;
				case 0x3f:	/* F5 */
					if(mode&WIN_GET && !(mode&WIN_XTR && (*cur)==opts-1))
						return((*cur)|MSK_GET);
					break;
				case 0x40:	/* F6 */
					if(mode&WIN_PUT && !(mode&WIN_XTR && (*cur)==opts-1))
						return((*cur)|MSK_PUT);
					break;
				case 82:	/* insert */
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
						if(!opts)
							return(MSK_INS);
						return((*cur)|MSK_INS); }
					break;
				case 83:	/* delete */
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
						&& ((!a && s && !strncmpi(option[j]+b,search,s+1))
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
									,i==(*cur) ? bclr|(LIGHTGRAY<<4)
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
								,bclr|(LIGHTGRAY<<4)
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
							line[i]=bclr|(LIGHTGRAY<<4);
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
					case ESC:
					case '\b':	/* backspace */
						if(mode&WIN_ESC || (mode&WIN_CHE && api->changes)
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
	}
}


/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, int left, int top, char *prompt, char *str,
	int max, int kmode)
{
	unsigned char c,tmp[81],save_buf[2048],in_win[2048]
		,shade[160],width,height=3;
	int i,j,plen,slen;

hidemouse();
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
i=getstr(str,max,kmode);
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
static int getstr(char *outstr, int max, long mode)
{
	uchar   ch,str[256],ins=0,buf[256],y;
	int     i,j,k,f=0;	/* i=offset, j=length */
#ifndef __FLAT__
	union  REGS r;
#endif

cursor=_NORMALCURSOR;
_setcursortype(cursor);
y=wherey();
if(mode&K_EDIT) {
/***
	truncsp(outstr);
***/
	outstr[max]=0;
	textattr(bclr|(LIGHTGRAY<<4));
	cputs(outstr);
	textattr(lclr|(bclr<<4));
	strcpy(str,outstr);
	i=j=strlen(str);
	while(inkey(1)==0) {
#ifndef __FLAT__
		if(api->mode&UIFC_MOUSE) {
			r.w.ax=0x0006;		/* Get button release information */
			r.w.bx=0x0000;		/* Left button */
			INT_86(0x33,&r,&r);
			if(r.w.bx) {		/* Left button release same as CR */
				cursor=_NOCURSOR;
				_setcursortype(cursor);
				return(i); }
			r.w.ax=0x0006;		/* Get button release information */
			r.w.bx=0x0001;		/* Right button */
			INT_86(0x33,&r,&r);
			if(r.w.bx) {		/* Right button release same as ESC */
				cursor=_NOCURSOR;
				_setcursortype(cursor);
				return(-1); } }
#endif
		mswait(1);
		}
	f=inkey(0);
	gotoxy(wherex()-i,y);
	if(!isprint(f&0xff) && f!=0x5300)
		cputs(outstr);
	else {
		cprintf("%*s",i,"");
		gotoxy(wherex()-i,y);
		i=j=0; } }
else
	i=j=0;


while(1) {


	if(i>j) j=i;
#ifndef __FLAT__
	if(api->mode&UIFC_MOUSE) {
		r.w.ax=0x0006;		/* Get button release information */
		r.w.bx=0x0000;		/* Left button */
		INT_86(0x33,&r,&r);
		if(r.w.bx)			/* Left button release same as CR */
			break;
		r.w.ax=0x0006;		/* Get button release information */
		r.w.bx=0x0001;		/* Right button */
		INT_86(0x33,&r,&r);
		if(r.w.bx) {		/* Right button release same as ESC */
			cursor=_NOCURSOR;
			_setcursortype(cursor);
			return(-1); } }
#endif
	if(f || inkey(1)) {
		if(f) k=f;			/* First key */
		else k=inkey(0);
		f=0;
		ch=k&0xff;
		if(!ch) {
			switch(k>>8) {
				case 0x3b:	/* F1 Help */
					help();
					continue;
				case 0x4b:	/* left arrow */
					if(i) {
						gotoxy(wherex()-1,y);
						i--; }
					continue;
				case 0x4d:	/* right arrow */
					if(i<j) {
						gotoxy(wherex()+1,y);
						i++; }
					continue;
				case 0x47:	/* home */
					if(i) {
						gotoxy(wherex()-i,y);
						i=0; }
					continue;
				case 0x4f:	/* end */
					if(i<j) {
						gotoxy(wherex()+(j-i),y);
						i=j; }
					continue;
				case 0x52:	/* insert */
					ins=!ins;
					if(ins)
						cursor=_SOLIDCURSOR;
					else
						cursor=_NORMALCURSOR;
					_setcursortype(cursor);
					continue;
				case 0x53:	/* delete */
					if(i<j) {
						gettext(wherex()+1,y,wherex()+(j-i),y,buf);
						puttext(wherex(),y,wherex()+(j-i)-1,y,buf);
						gotoxy(wherex()+(j-i),y);
						putch(SP);
						gotoxy(wherex()-((j-i)+1),y);
						for(k=i;k<j;k++)
							str[k]=str[k+1];
						j--; }
					continue; }
			continue; }
		if(ch==03 || ch==ESC) {
			cursor=_NOCURSOR;
			_setcursortype(cursor);
			return(-1); }
		if(ch==BS && i) {
			if(i==j) {
				cputs("\b \b");
				j--;
				i--; }
			else {
				gettext(wherex(),y,wherex()+(j-i),y,buf);
				puttext(wherex()-1,y,wherex()+(j-i)-1,y,buf);
				gotoxy(wherex()+(j-i),y);
				putch(SP);
				gotoxy(wherex()-((j-i)+2),y);
				i--;
				j--;
				for(k=i;k<j;k++)
					str[k]=str[k+1]; }
			continue; }
		if(ch==CR)
			break;
		if(ch==24) {  /* ctrl-x  */
			if(j) {
				gotoxy(wherex()-i,y);
				cprintf("%*s",j,"");
				gotoxy(wherex()-j,y);
				i=j=0; }
			continue; }
		if(ch==25) {  /* ctrl-y */
			if(i<j) {
				cprintf("%*s",(j-i),"");
				gotoxy(wherex()-(j-i),y);
				j=i; }
			continue; }
		if(mode&K_NUMBER && !isdigit(ch))
			continue;
		if(mode&K_ALPHA && !isalpha(ch))
			continue;
		if((ch>=SP || (ch==1 && mode&K_MSG)) && i<max && (!ins || j<max)) {
			if(mode&K_UPPER)
				ch=toupper(ch);
			if(ins) {
				gettext(wherex(),y,wherex()+(j-i),y,buf);
				puttext(wherex()+1,y,wherex()+(j-i)+1,y,buf);
				for(k=++j;k>i;k--)
					str[k]=str[k-1]; }
			putch(ch);
			str[i++]=ch; } }
	else
		mswait(1);
	}
str[j]=0;
if(mode&K_EDIT) {
	truncsp(str);
	if(strcmp(outstr,str))
		api->changes=1; }
else {
	if(j)
		api->changes=1; }
strcpy(outstr,str);
cursor=_NOCURSOR;
_setcursortype(cursor);
return(j);
}

/****************************************************************************/
/* Performs printf() through puttext() routine								*/
/****************************************************************************/
static int uprintf(int x, int y, char attr, char *fmat, ...)
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
uprintf(i,api->scrn_len+1,bclr|(cclr<<4),"ESC ");
i+=4;
uprintf(i,api->scrn_len+1,BLACK|(cclr<<4),"Exit");
i+=4;
gotoxy(i,api->scrn_len+1);
textattr(BLACK|(cclr<<4));
clreol();
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
void upop(char *str)
{
	static char sav[26*3*2];
	char buf[26*3*2];
	int i,j,k;

	hidemouse();
	if(!str) {
		puttext(28,12,53,14,sav);
		showmouse();
		return; }
	gettext(28,12,53,14,sav);
	memset(buf,SP,25*3*2);
	for(i=1;i<26*3*2;i+=2)
		buf[i]=(hclr|(bclr<<4));
	buf[0]='Ú';
	for(i=2;i<25*2;i+=2)
		buf[i]='Ä';
	buf[i]='¿'; i+=2;
	buf[i]='³'; i+=2;
	i+=2;
	k=strlen(str);
	i+=(((23-k)/2)*2);
	for(j=0;j<k;j++,i+=2) {
		buf[i]=str[j];
		buf[i+1]|=BLINK; }
	i=((25*2)+1)*2;
	buf[i]='³'; i+=2;
	buf[i]='À'; i+=2;
	for(;i<((26*3)-1)*2;i+=2)
		buf[i]='Ä';
	buf[i]='Ù';

	puttext(28,12,53,14,buf);
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

/************************************************************/
/* Help (F1) key function. Uses helpbuf as the help input.	*/
/************************************************************/
void help()
{
	char *savscrn,*buf,inverse=0,high=0
		,hbuf[HELPBUF_SIZE],str[256];
    char *p;
	uint i,j,k,len;
	unsigned short line;
	long l;
	FILE *fp;
#ifndef __FLAT__
	union  REGS r;
#endif

	_setcursortype(_NOCURSOR);

	if((savscrn=(char *)MALLOC(80*25*2))==NULL) {
		cprintf("UIFC line %d: error allocating %u bytes\r\n"
			,__LINE__,80*25*2);
		_setcursortype(cursor);
		return; }
	if((buf=(char *)MALLOC(76*21*2))==NULL) {
		cprintf("UIFC line %d: error allocating %u bytes\r\n"
			,__LINE__,76*21*2);
		FREE(savscrn);
		_setcursortype(cursor);
		return; }
	hidemouse();
	gettext(1,1,80,25,savscrn);
	memset(buf,SP,76*21*2);
	for(i=1;i<76*21*2;i+=2)
		buf[i]=(hclr|(bclr<<4));
	buf[0]='Ú';
	for(i=2;i<30*2;i+=2)
		buf[i]='Ä';
	buf[i]='´'; i+=4;
	buf[i]='O'; i+=2;
	buf[i]='n'; i+=2;
	buf[i]='l'; i+=2;
	buf[i]='i'; i+=2;
	buf[i]='n'; i+=2;
	buf[i]='e'; i+=4;
	buf[i]='H'; i+=2;
	buf[i]='e'; i+=2;
	buf[i]='l'; i+=2;
	buf[i]='p'; i+=4;
	buf[i]='Ã'; i+=2;
	for(j=i;j<i+(30*2);j+=2)
		buf[j]='Ä';
	i=j;
	buf[i]='¿'; i+=2;
	j=i;	/* leave i alone */
	for(k=0;k<19;k++) { 		/* the sides of the box */
		buf[j]='³'; j+=2;
		j+=(74*2);
		buf[j]='³'; j+=2; }
	buf[j]='À'; j+=2;
	for(k=j;k<j+(23*2);k+=2)
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
	for(j=k;j<k+(24*2);j+=2)
		buf[j]='Ä';
	buf[j]='Ù';

	if(!api->helpbuf) {
		if((fp=_fsopen(api->helpixbfile,"rb",SH_DENYWR))==NULL)
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
				if((fp=_fsopen(api->helpdatfile,"rb",SH_DENYWR))==NULL)
					sprintf(hbuf," ERROR  Cannot open help file:\r\n          %s"
						,api->helpdatfile);
				else {
					fseek(fp,l,SEEK_SET);
					fread(hbuf,HELPBUF_SIZE,1,fp);
					fclose(fp); } } } }
	else
		strcpy(hbuf,api->helpbuf);

	len=strlen(hbuf);

	i+=78*2;
	for(j=0;j<len;j++,i+=2) {
		if(hbuf[j]==LF) {
			while(i%(76*2)) i++;
			i+=2; }
		else if(hbuf[j]==2 || hbuf[j]=='~') { /* Ctrl-b toggles inverse */
			inverse=!inverse;
			i-=2; }
		else if(hbuf[j]==1 || hbuf[j]=='`') { /* Ctrl-a toggles high intensity */
			high=!high;
			i-=2; }
		else if(hbuf[j]!=CR) {
			buf[i]=hbuf[j];
			buf[i+1]=inverse ? (bclr|(cclr<<4))
				: high ? (hclr|(bclr<<4)) : (lclr|(bclr<<4)); } }
	puttext(3,3,78,23,buf);
	showmouse();
	while(1) {
		if(inkey(1)) {
			inkey(0);
			break; }
	#ifndef __FLAT__
		if(api->mode&UIFC_MOUSE) {
			r.w.ax=0x0006;		/* Get button release information */
			r.w.bx=0x0000;		/* Left button */
			INT_86(0x33,&r,&r);
			if(r.w.bx)			/* Left button release same as CR */
				break;
			r.w.ax=0x0006;		/* Get button release information */
			r.w.bx=0x0001;		/* Right button */
			INT_86(0x33,&r,&r);
			if(r.w.bx)			/* Left button release same as CR */
				break; }
	#endif
		mswait(1);
		}

	hidemouse();
	puttext(1,1,80,25,savscrn);
	showmouse();
	FREE(savscrn);
	FREE(buf);
	_setcursortype(cursor);
}

