/* uifc32.c */

/* Curses implementation of UIFC (user interface) library based on uifc.c */

/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2005 Rob Swindell - http://www.synchro.net/copyright.html		*
 *																			*
 * This library is free software; you can redistribute it and/or			*
 * modify it under the terms of the GNU Lesser General Public License		*
 * as published by the Free Software Foundation; either version 2			*
 * of the License, or (at your option) any later version.					*
 * See the GNU Lesser General Public License for more details: lgpl.txt or	*
 * http://www.fsf.org/copyleft/lesser.html									*
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

#ifdef __unix__
	#include <stdio.h>
	#include <unistd.h>
	#ifdef __QNX__
		#include <strings.h>
	#endif
    #define mswait(x) delay(x)
#elif defined(_WIN32)
	#include <share.h>
	#include <windows.h>
	#define mswait(x) Sleep(x)
#endif

#include "ciolib.h"
#include "keys.h"
#include "uifc.h"
#define MAX_GETSTR	5120

#define BLINK	128

static int   cursor;
static char* helpfile=0;
static uint  helpline=0;
static size_t blk_scrn_len;
static char* blk_scrn;
static uchar* tmp_buffer;
static uchar* tmp_buffer2;
static win_t sav[MAX_BUFS];
static uifcapi_t* api;

/* Prototypes */
static int   uprintf(int x, int y, unsigned attr, char *fmt,...);
static void  bottomline(int line);
static char  *utimestr(time_t *intime);
static void  help(void);
static int   ugetstr(int left, int top, int width, char *outstr, int max, long mode, int *lastkey);
static void  timedisplay(BOOL force);

/* API routines */
static void uifcbail(void);
static int  uscrn(char *str);
static int  ulist(int mode, int left, int top, int width, int *dflt, int *bar
	,char *title, char **option);
static int  uinput(int imode, int left, int top, char *prompt, char *str
	,int len ,int kmode);
static void umsg(char *str);
static void upop(char *str);
static void sethelp(int line, char* file);
static void showbuf(int mode, int left, int top, int width, int height, char *title
	, char *hbuf, int *curp, int *barp);

/* Dynamic menu support */
static int *last_menu_cur=NULL;
static int *last_menu_bar=NULL;
static int save_menu_cur=-1;
static int save_menu_bar=-1;
static int save_menu_opts=-1;

static void reset_dynamic(void) {
	last_menu_cur=NULL;
	last_menu_bar=NULL;
	save_menu_cur=-1;
	save_menu_bar=-1;
	save_menu_opts=-1;
}

/****************************************************************************/
/* Initialization function, see uifc.h for details.							*/
/* Returns 0 on success.													*/
/****************************************************************************/

void uifc_mouse_enable(void)
{
	ciomouse_setevents(0);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_1_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_2_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_3_CLICK);
	showmouse();
}

void uifc_mouse_disable(void)
{
	ciomouse_setevents(0);
	hidemouse();
}

int kbwait(void) {
	int timeout=0;
	while(timeout++<50) {
		if(kbhit())
			return(TRUE);
		mswait(1);
	}
	return(FALSE);
}

int inkey(void)
{
	int c;

	c=getch();
	if(!c || c==0xff)
		c|=(getch()<<8);
	return(c);
}

int uifcini32(uifcapi_t* uifcapi)
{
	unsigned	i;
	struct	text_info txtinfo;

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
    api->showhelp=help;
	api->showbuf=showbuf;
	api->timedisplay=timedisplay;
	api->bottomline=bottomline;
	api->getstrxy=ugetstr;
	api->printf=uprintf;

	/* A esc_delay of less than 10 is stupid... silently override */
	if(api->esc_delay < 10)
		api->esc_delay=25;

#ifdef NCURSES_VERSION_MAJOR
	if(cio_api.mode==CIOLIB_MODE_CURSES) {
		ESCDELAY=api->esc_delay;
#endif

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

#if 0
    clrscr();
#endif

    gettextinfo(&txtinfo);
    /* unsupported mode? */
    if(txtinfo.screenheight<MIN_LINES
/*        || txtinfo.screenheight>MAX_LINES */
        || txtinfo.screenwidth<40) {
        textmode(C80);  /* set mode to 80x25*/
        gettextinfo(&txtinfo);
    }
	window(1,1,txtinfo.screenwidth,txtinfo.screenheight);

    api->scrn_len=txtinfo.screenheight;
    if(api->scrn_len<MIN_LINES) {
        cprintf("\7UIFC: Screen length (%u) must be %d lines or greater\r\n"
            ,api->scrn_len,MIN_LINES);
        return(-2);
    }
    api->scrn_len--; /* account for status line */

    if(txtinfo.screenwidth<40) {
        cprintf("\7UIFC: Screen width (%u) must be at least 40 characters\r\n"
            ,txtinfo.screenwidth);
        return(-3);
    }
	api->scrn_width=txtinfo.screenwidth;

    if(!(api->mode&UIFC_COLOR)
        && (api->mode&UIFC_MONO
            || txtinfo.currmode==MONO || txtinfo.currmode==BW40 || txtinfo.currmode==BW80
            || txtinfo.currmode==MONO14 || txtinfo.currmode==BW40X14 || txtinfo.currmode==BW80X14
            || txtinfo.currmode==MONO21 || txtinfo.currmode==BW40X21 || txtinfo.currmode==BW80X21
            || txtinfo.currmode==MONO28 || txtinfo.currmode==BW40X28 || txtinfo.currmode==BW80X28
            || txtinfo.currmode==MONO43 || txtinfo.currmode==BW40X43 || txtinfo.currmode==BW80X43
            || txtinfo.currmode==MONO50 || txtinfo.currmode==BW40X50 || txtinfo.currmode==BW80X50
            || txtinfo.currmode==MONO60 || txtinfo.currmode==BW40X60 || txtinfo.currmode==BW80X60))
	{
        api->bclr=BLACK;
        api->hclr=WHITE;
        api->lclr=LIGHTGRAY;
        api->cclr=LIGHTGRAY;
			api->lbclr=BLACK|(LIGHTGRAY<<4);	/* lightbar color */
    } else {
        api->bclr=BLUE;
        api->hclr=YELLOW;
        api->lclr=WHITE;
        api->cclr=CYAN;
		api->lbclr=BLUE|(LIGHTGRAY<<4);	/* lightbar color */
    }

	blk_scrn_len=api->scrn_width*api->scrn_len*2;
	if((blk_scrn=(char *)malloc(blk_scrn_len))==NULL)  {
				cprintf("UIFC line %d: error allocating %u bytes."
					,__LINE__,blk_scrn_len);
				return(-1);
	}
	if((tmp_buffer=(uchar *)malloc(blk_scrn_len))==NULL)  {
				cprintf("UIFC line %d: error allocating %u bytes."
					,__LINE__,blk_scrn_len);
				return(-1);
	}
	if((tmp_buffer2=(uchar *)malloc(blk_scrn_len))==NULL)  {
				cprintf("UIFC line %d: error allocating %u bytes."
					,__LINE__,blk_scrn_len);
				return(-1);
	}
    for(i=0;i<blk_scrn_len;i+=2) {
        blk_scrn[i]='°';
        blk_scrn[i+1]=api->cclr|(api->bclr<<4);
    }

    cursor=_NOCURSOR;
    _setcursortype(cursor);

	if(cio_api.mouse) {
		api->mode|=UIFC_MOUSE;
		uifc_mouse_enable();
	}

	api->initialized=TRUE;

    return(0);
}

void docopy(void)
{
	int	key;
	struct mouse_event mevent;
	unsigned char *screen;
	unsigned char *sbuffer;
	int sbufsize=0;
	int x,y,startx,starty,endx,endy,lines;
	int outpos;
	char *copybuf;

	sbufsize=api->scrn_width*2*(api->scrn_len+1);
	screen=(unsigned char*)malloc(sbufsize);
	sbuffer=(unsigned char*)malloc(sbufsize);
	gettext(1,1,api->scrn_width,api->scrn_len+1,screen);
	while(1) {
		key=getch();
		if(key==0 || key==0xff)
			key|=getch()<<8;
		switch(key) {
			case CIO_KEY_MOUSE:
				getmouse(&mevent);
				if(mevent.startx<mevent.endx) {
					startx=mevent.startx;
					endx=mevent.endx;
				}
				else {
					startx=mevent.endx;
					endx=mevent.startx;
				}
				if(mevent.starty<mevent.endy) {
					starty=mevent.starty;
					endy=mevent.endy;
				}
				else {
					starty=mevent.endy;
					endy=mevent.starty;
				}
				switch(mevent.event) {
					case CIOLIB_BUTTON_1_DRAG_MOVE:
						memcpy(sbuffer,screen,sbufsize);
						for(y=starty-1;y<endy;y++) {
							for(x=startx-1;x<endx;x++) {
								int pos=y*api->scrn_width+x;
								if((sbuffer[pos*2+1]&0x70)!=0x10)
									sbuffer[pos*2+1]=sbuffer[pos*2+1]&0x8F|0x10;
								else
									sbuffer[pos*2+1]=sbuffer[pos*2+1]&0x8F|0x60;
								if(((sbuffer[pos*2+1]&0x70)>>4) == (sbuffer[pos*2+1]&0x0F)) {
									sbuffer[pos*2+1]|=0x08;
								}
							}
						}
						puttext(1,1,api->scrn_width,api->scrn_len+1,sbuffer);
						break;
					case CIOLIB_BUTTON_1_DRAG_END:
						lines=abs(mevent.endy-mevent.starty)+1;
						copybuf=malloc((endy-starty+1)*(endx-startx+1)+1+lines*2);
						outpos=0;
						for(y=starty-1;y<endy;y++) {
							for(x=startx-1;x<endx;x++) {
								copybuf[outpos++]=screen[(y*api->scrn_width+x)*2];
							}
							copybuf[outpos++]='\r';
							copybuf[outpos++]='\n';
						}
						copybuf[outpos]=0;
						copytext(copybuf, strlen(copybuf));
						puttext(1,1,api->scrn_width,api->scrn_len+1,screen);
						free(copybuf);
						free(screen);
						free(sbuffer);
						return;
				}
				break;
			default:
				puttext(1,1,api->scrn_width,api->scrn_len+1,screen);
				ungetch(key);
				free(screen);
				free(sbuffer);
				return;
		}
	}
}

static int uifc_getmouse(struct mouse_event *mevent)
{
	mevent->startx=0;
	mevent->starty=0;
	mevent->event=0;
	if(api->mode&UIFC_MOUSE) {
		getmouse(mevent);
		if(mevent->event==CIOLIB_BUTTON_3_CLICK)
			return(ESC);
		if(mevent->event==CIOLIB_BUTTON_1_DRAG_START) {
			docopy();
			return(0);
		}
		if(mevent->starty==api->buttony) {
			if(mevent->startx>=api->exitstart
					&& mevent->startx<=api->exitend
					&& mevent->event==CIOLIB_BUTTON_1_CLICK) {
				return(ESC);
			}
			if(mevent->startx>=api->helpstart
					&& mevent->startx<=api->helpend
					&& mevent->event==CIOLIB_BUTTON_1_CLICK) {
				return(CIO_KEY_F(1));
			}
		}
		return(0);
	}
	return(-1);
}

void uifcbail(void)
{
	_setcursortype(_NORMALCURSOR);
	textattr(LIGHTGRAY);
	uifc_mouse_disable();
	suspendciolib();
	FREE_AND_NULL(blk_scrn);
	FREE_AND_NULL(tmp_buffer);
	FREE_AND_NULL(tmp_buffer2);
	api->initialized=FALSE;
}

/****************************************************************************/
/* Clear screen, fill with background attribute, display application title.	*/
/* Returns 0 on success.													*/
/****************************************************************************/
int uscrn(char *str)
{
    textattr(api->bclr|(api->cclr<<4));
    gotoxy(1,1);
    clreol();
    gotoxy(3,1);
	cputs(str);
    if(!puttext(1,2,api->scrn_width,api->scrn_len,blk_scrn))
        return(-1);
    gotoxy(1,api->scrn_len+1);
    clreol();
	reset_dynamic();
	setname(str);
    return(0);
}

/****************************************************************************/
/****************************************************************************/
static void scroll_text(int x1, int y1, int x2, int y2, int down)
{
	gettext(x1,y1,x2,y2,tmp_buffer2);
	if(down)
		puttext(x1,y1+1,x2,y2,tmp_buffer2);
	else
		puttext(x1,y1,x2,y2-1,tmp_buffer2+(((x2-x1)+1)*2));
}

/****************************************************************************/
/* Updates time in upper left corner of screen with current time in ASCII/  */
/* Unix format																*/
/****************************************************************************/
static void timedisplay(BOOL force)
{
	static time_t savetime;
	time_t now;

	now=time(NULL);
	if(force || difftime(now,savetime)>=60) {
		uprintf(api->scrn_width-25,1,api->bclr|(api->cclr<<4),utimestr(&now));
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
	while(c && (uchar)str[c-1]<=' ') c--;
	if(str[c]!=0)	/* don't write to string constants */
		str[c]=0;
}

/****************************************************************************/
/* General menu function, see uifc.h for details.							*/
/****************************************************************************/
int ulist(int mode, int left, int top, int width, int *cur, int *bar
	, char *initial_title, char **option)
{
	uchar line[256],shade[256],*ptr
		,search[MAX_OPLN],bline=0,*win;
	int height,y;
	int i,j,opts=0,s=0; /* s=search index into options */
	int	is_redraw=0;
	int s_top=SCRN_TOP;
	int s_left=SCRN_LEFT;
	int s_right=SCRN_RIGHT;
	int s_bottom=api->scrn_len-3;
	int hbrdrsize=2;
	int lbrdrwidth=1;
	int rbrdrwidth=1;
	int vbrdrsize=4;
	int tbrdrwidth=3;
	int bbrdrwidth=1;
	int title_len;
	struct mouse_event mevnt;
	char	*title=NULL;
	int	a,b,c,longopt;
	int	optheight=0;

	title=strdup(initial_title==NULL?"":initial_title);

	uifc_mouse_disable();

	title_len=strlen(title);

	if(mode&WIN_FAT) {
		s_top=1;
		s_left=2;
		s_right=api->scrn_width-3;  /* Leave space for the shadow */
		s_bottom=api->scrn_len-1;   /* Leave one for the shadow */
	}
	if(mode&WIN_NOBRDR) {
		hbrdrsize=0;
		vbrdrsize=0;
		lbrdrwidth=0;
		rbrdrwidth=0;
		tbrdrwidth=0;
		bbrdrwidth=0;
	}

	if(mode&WIN_SAV && api->savnum>=MAX_BUFS-1)
		putch(7);
	if(api->helpbuf!=NULL || api->helpixbfile[0]!=0) bline|=BL_HELP;
	if(mode&WIN_INS) bline|=BL_INS;
	if(mode&WIN_DEL) bline|=BL_DEL;
	if(mode&WIN_GET) bline|=BL_GET;
	if(mode&WIN_PUT) bline|=BL_PUT;
	if(mode&WIN_EDIT) bline|=BL_EDIT;
	if(api->bottomline != NULL)
		api->bottomline(bline);
	while(option!=NULL && opts<MAX_OPTS)
		if(option[opts]==NULL || option[opts][0]==0)
			break;
		else opts++;
	if(mode&WIN_XTR && opts<MAX_OPTS)
		opts++;
	optheight=opts+vbrdrsize;
	height=optheight;
	if(mode&WIN_FIXEDHEIGHT) {
		height=api->list_height;
	}
	if(top+height>s_bottom)
		height=(s_bottom)-top;
	if(optheight>height)
		optheight=height;
	if(!width || width<title_len+hbrdrsize+2) {
		width=title_len+hbrdrsize+2;
		for(i=0;i<opts;i++) {
			truncsp(option[i]);
			if((j=strlen(option[i])+hbrdrsize+2+1)>width)
				width=j;
		}
	}
	if(!(mode&WIN_NOBRDR) && api->mode&UIFC_MOUSE && bline&BL_HELP && width<8)
		width=8;
	if(width>(s_right+1)-s_left) {
		width=(s_right+1)-s_left;
		if(title_len>(width-hbrdrsize-2)) {
			*(title+width-hbrdrsize-2-3)='.';
			*(title+width-hbrdrsize-2-2)='.';
			*(title+width-hbrdrsize-2-1)='.';
			*(title+width-hbrdrsize-2)=0;
			title_len=strlen(title);
		}
	}
	if(mode&WIN_L2R)
		left=(s_right-s_left-width+1)/2;
	else if(mode&WIN_RHT)
		left=s_right-(width+hbrdrsize+2+left);
	if(mode&WIN_T2B)
		top=(api->scrn_len-height+1)/2-2;
	else if(mode&WIN_BOT)
		top=s_bottom-height-top;
	if(left<0)
		left=0;
	if(top<0)
		top=0;

	/* Dynamic Menus */
	if(mode&WIN_DYN
			&& cur != NULL
			&& bar != NULL
			&& last_menu_cur==cur
			&& last_menu_bar==bar
			&& save_menu_cur==*cur
			&& save_menu_bar==*bar
			&& save_menu_opts==opts)
		is_redraw=1;
	if(mode&WIN_DYN && mode&WIN_REDRAW)
		is_redraw=1;
	if(mode&WIN_DYN && mode&WIN_NODRAW)
		is_redraw=0;

	if(!is_redraw) {
		if(mode&WIN_SAV && api->savdepth==api->savnum) {
			if((sav[api->savnum].buf=(char *)malloc((width+3)*(height+2)*2))==NULL) {
				cprintf("UIFC line %d: error allocating %u bytes."
					,__LINE__,(width+3)*(height+2)*2);
				free(title);
				uifc_mouse_enable();
				return(-1);
			}
			gettext(s_left+left,s_top+top,s_left+left+width+1
				,s_top+top+height,sav[api->savnum].buf);
			sav[api->savnum].left=s_left+left;
			sav[api->savnum].top=s_top+top;
			sav[api->savnum].right=s_left+left+width+1;
			sav[api->savnum].bot=s_top+top+height;
			api->savdepth++;
		}
		else if(mode&WIN_SAV
			&& (sav[api->savnum].left!=s_left+left
			|| sav[api->savnum].top!=s_top+top
			|| sav[api->savnum].right!=s_left+left+width+1
			|| sav[api->savnum].bot!=s_top+top+height)) { /* dimensions have changed */
			puttext(sav[api->savnum].left,sav[api->savnum].top,sav[api->savnum].right,sav[api->savnum].bot
				,sav[api->savnum].buf);	/* put original window back */
			free(sav[api->savnum].buf);
			if((sav[api->savnum].buf=(char *)malloc((width+3)*(height+2)*2))==NULL) {
				cprintf("UIFC line %d: error allocating %u bytes."
					,__LINE__,(width+3)*(height+2)*2);
				free(title);
				uifc_mouse_enable();
				return(-1);
			}
			gettext(s_left+left,s_top+top,s_left+left+width+1
				,s_top+top+height,sav[api->savnum].buf);	  /* save again */
			sav[api->savnum].left=s_left+left;
			sav[api->savnum].top=s_top+top;
			sav[api->savnum].right=s_left+left+width+1;
			sav[api->savnum].bot=s_top+top+height;
		}
	}

	if(!is_redraw) {
		if(mode&WIN_ORG) { /* Clear around menu */
			if(top)
				puttext(1,2,api->scrn_width,s_top+top-1,blk_scrn);
			if((unsigned)(s_top+height+top)<=api->scrn_len)
				puttext(1,s_top+height+top,api->scrn_width,api->scrn_len,blk_scrn);
			if(left)
				puttext(1,s_top+top,s_left+left-1,s_top+height+top
					,blk_scrn);
			if(s_left+left+width<=s_right)
				puttext(s_left+left+width,s_top+top,/* s_right+2 */api->scrn_width
					,s_top+height+top,blk_scrn);
		}
		ptr=tmp_buffer;
		if(!(mode&WIN_NOBRDR)) {
			*(ptr++)='É';
			*(ptr++)=api->hclr|(api->bclr<<4);

			if(api->mode&UIFC_MOUSE) {
				*(ptr++)='[';
				*(ptr++)=api->hclr|(api->bclr<<4);
				/* *(ptr++)='þ'; */
				*(ptr++)=0xfe;
				*(ptr++)=api->lclr|(api->bclr<<4);
				*(ptr++)=']';
				*(ptr++)=api->hclr|(api->bclr<<4);
				i=3;
				if(bline&BL_HELP) {
					*(ptr++)='[';
					*(ptr++)=api->hclr|(api->bclr<<4);
					*(ptr++)='?';
					*(ptr++)=api->lclr|(api->bclr<<4);
					*(ptr++)=']';
					*(ptr++)=api->hclr|(api->bclr<<4);
					i+=3;
				}
				api->buttony=s_top+top;
				api->exitstart=s_left+left+1;
				api->exitend=s_left+left+3;
				api->helpstart=s_left+left+4;
				api->helpend=s_left+left+6;
			}
			else
				i=0;

			for(;i<width-2;i++) {
				*(ptr++)='Í';
				*(ptr++)=api->hclr|(api->bclr<<4);
			}
			*(ptr++)='»';
			*(ptr++)=api->hclr|(api->bclr<<4);
			*(ptr++)='º';
			*(ptr++)=api->hclr|(api->bclr<<4);
			a=title_len;
			b=(width-a-1)/2;
			for(i=0;i<b;i++) {
				*(ptr++)=' ';
				*(ptr++)=api->hclr|(api->bclr<<4);
			}
			for(i=0;i<a;i++) {
				*(ptr++)=title[i];
				*(ptr++)=api->hclr|(api->bclr<<4);
			}
			for(i=0;i<width-(a+b)-2;i++) {
				*(ptr++)=' ';
				*(ptr++)=api->hclr|(api->bclr<<4);
			}
			*(ptr++)='º';
			*(ptr++)=api->hclr|(api->bclr<<4);
			*(ptr++)='Ì';
			*(ptr++)=api->hclr|(api->bclr<<4);
			for(i=0;i<width-2;i++) {
				*(ptr++)='Í';
				*(ptr++)=api->hclr|(api->bclr<<4);
			}
			*(ptr++)='¹';
			*(ptr++)=api->hclr|(api->bclr<<4);
		}

		if((*cur)>=opts)
			(*cur)=opts-1;			/* returned after scrolled */

		if(!bar) {
			if((*cur)>height-vbrdrsize-1)
				(*cur)=height-vbrdrsize-1;
			if((*cur)>opts-1)
				(*cur)=opts-1;
			i=0;
		}
		else {
			if((*bar)>=opts)
				(*bar)=opts-1;
			if((*bar)>height-vbrdrsize-1)
				(*bar)=height-vbrdrsize-1;
			if((*cur)==opts-1)
				(*bar)=height-vbrdrsize-1;
			if((*bar)>opts-1)
				(*bar)=opts-1;
			if((*bar)<0)
				(*bar)=0;
			if((*cur)<(*bar))
				(*cur)=(*bar);
			i=(*cur)-(*bar);
			if(i+(height-vbrdrsize-1)>=opts) {
				i=opts-(height-vbrdrsize);
				if(i<0)
					i=0;
				(*cur)=i+(*bar);
			}
		}
		if((*cur)<0)
			(*cur)=0;

		j=0;
		if(i<0) i=0;
		longopt=0;
		while(j<height-vbrdrsize) {
			if(!(mode&WIN_NOBRDR)) {
				*(ptr++)='º';
				*(ptr++)=api->hclr|(api->bclr<<4);
			}
			*(ptr++)=' ';
			*(ptr++)=api->hclr|(api->bclr<<4);
			*(ptr++)='³';
			*(ptr++)=api->lclr|(api->bclr<<4);
			if(i==(*cur))
				a=api->lbclr;
			else
				a=api->lclr|(api->bclr<<4);
			if(i<opts) {
				b=strlen(option[i]);
				if(b>longopt)
					longopt=b;
				if(b+hbrdrsize+2>width)
					b=width-hbrdrsize-2;
				for(c=0;c<b;c++) {
					*(ptr++)=option[i][c];
					*(ptr++)=a; 
				}
			}
			else
				c=0;
			while(c<width-hbrdrsize-2) {
				*(ptr++)=' ';
				*(ptr++)=a;
				c++;
			}
			if(!(mode&WIN_NOBRDR)) {
				*(ptr++)='º';
				*(ptr++)=api->hclr|(api->bclr<<4);
			}
			i++;
			j++; 
		}
		if(!(mode&WIN_NOBRDR)) {
			*(ptr++)='È';
			*(ptr++)=api->hclr|(api->bclr<<4);
			for(i=0;i<width-2;i++) {
				*(ptr++)='Í';
				*(ptr++)=api->hclr|(api->bclr<<4); 
			}
			*(ptr++)='¼';
			*(ptr)=api->hclr|(api->bclr<<4);	/* Not incremented to shut ot BCC */
		}
		puttext(s_left+left,s_top+top,s_left+left+width-1
			,s_top+top+height-1,tmp_buffer);
		if(bar)
			y=top+tbrdrwidth+(*bar);
		else
			y=top+tbrdrwidth+(*cur);
		if(opts+vbrdrsize>height && ((!bar && (*cur)!=opts-1)
			|| (bar && ((*cur)-(*bar))+(height-vbrdrsize)<opts))) {
			gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
			textattr(api->lclr|(api->bclr<<4));
			putch(31);	   /* put down arrow */
			textattr(api->hclr|(api->bclr<<4)); 
		}

		if(bar && (*bar)!=(*cur)) {
			gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
			textattr(api->lclr|(api->bclr<<4));
			putch(30);	   /* put the up arrow */
			textattr(api->hclr|(api->bclr<<4)); 
		}

		if(!(mode&WIN_NOBRDR)) {
			/* Shadow */
			if(api->bclr==BLUE) {
				gettext(s_left+left+width,s_top+top+1,s_left+left+width+1
					,s_top+top+height-1,shade);
				for(i=1;i<height*4;i+=2)
					shade[i]=DARKGRAY;
				puttext(s_left+left+width,s_top+top+1,s_left+left+width+1
					,s_top+top+height-1,shade);
				gettext(s_left+left+2,s_top+top+height,s_left+left+width+1
					,s_top+top+height,shade);
				for(i=1;i<width*2;i+=2)
					shade[i]=DARKGRAY;
				puttext(s_left+left+2,s_top+top+height,s_left+left+width+1
					,s_top+top+height,shade);
			}
		}
	}
	else {	/* Is a redraw */
		i=(*cur)-(*bar);
		j=2;

		longopt=0;
		while(j<height-bbrdrwidth-1) {
			ptr=tmp_buffer;
			if(i==(*cur))
				a=api->lbclr;
			else
				a=api->lclr|(api->bclr<<4);
			if(i<opts) {
				b=strlen(option[i]);
				if(b>longopt)
					longopt=b;
				if(b+hbrdrsize+2>width)
					b=width-hbrdrsize-2;
				for(c=0;c<b;c++) {
					*(ptr++)=option[i][c];
					*(ptr++)=a; 
				}
			}
			else
				c=0;
			while(c<width-hbrdrsize-2) {
				*(ptr++)=' ';
				*(ptr++)=a;
				c++; 
			}
			i++;
			j++; 
			puttext(s_left+left+lbrdrwidth+2,s_top+top+j,s_left+left+width-rbrdrwidth-1
				,s_top+top+j,tmp_buffer);
		}
		if(bar)
			y=top+tbrdrwidth+(*bar);
		else
			y=top+tbrdrwidth+(*cur);
	}
	free(title);

	last_menu_cur=cur;
	last_menu_bar=bar;
	uifc_mouse_enable();

	if(mode&WIN_IMM)
		return(-2);

	if(mode&WIN_ORG) {
		if(api->timedisplay != NULL)
			api->timedisplay(/* force? */TRUE);
	}

	while(1) {
	#if 0					/* debug */
		gotoxy(30,1);
		cprintf("y=%2d h=%2d c=%2d b=%2d s=%2d o=%2d"
			,y,height,*cur,bar ? *bar :0xff,api->savdepth,opts);
	#endif
		if(api->timedisplay != NULL)
			api->timedisplay(/* force? */FALSE);
		i=0;
		if(kbwait()) {
			i=inkey();
			if(i==CIO_KEY_MOUSE) {
				if((i=uifc_getmouse(&mevnt))==0) {
					/* Clicked in menu */
					if(mevnt.startx>=s_left+left+lbrdrwidth+2
							&& mevnt.startx<=s_left+left+width-rbrdrwidth-1
							&& mevnt.starty>=s_top+top+tbrdrwidth
							&& mevnt.starty<=(s_top+top+optheight)-bbrdrwidth-1
							&& mevnt.event==CIOLIB_BUTTON_1_CLICK) {

						(*cur)=(mevnt.starty)-(s_top+top+tbrdrwidth);
						if(bar)
							(*bar)=(*cur);
						y=top+tbrdrwidth+((mevnt.starty)-(s_top+top+tbrdrwidth));

						if(!opts)
							continue;

						if(mode&WIN_ACT) {
							uifc_mouse_disable();
							if((win=(char *)malloc((width+3)*(height+2)*2))==NULL) {
								cprintf("UIFC line %d: error allocating %u bytes."
									,__LINE__,(width+3)*(height+2)*2);
								return(-1);
							}
							gettext(s_left+left,s_top+top,s_left
								+left+width-1,s_top+top+height-1,win);
							for(i=1;i<(width*height*2);i+=2)
								win[i]=api->lclr|(api->cclr<<4);
							j=(((y-top)*width)*2)+7+((width-hbrdrsize-2)*2);
							for(i=(((y-top)*width)*2)+7;i<j;i+=2)
								win[i]=api->hclr|(api->cclr<<4);

							puttext(s_left+left,s_top+top,s_left
								+left+width-1,s_top+top+height-1,win);
							free(win);
							uifc_mouse_enable();
						}
						else if(mode&WIN_SAV) {
							uifc_mouse_disable();
							puttext(sav[api->savnum].left,sav[api->savnum].top
								,sav[api->savnum].right,sav[api->savnum].bot
								,sav[api->savnum].buf);
							uifc_mouse_enable();
							free(sav[api->savnum].buf);
							api->savdepth--;
						}
						if(mode&WIN_XTR && (*cur)==opts-1)
							return(MSK_INS|*cur);
						return(*cur);
					}
					/* Clicked Scroll Up */
					else if(mevnt.startx==s_left+left+lbrdrwidth
							&& mevnt.starty==s_top+top+tbrdrwidth
							&& mevnt.event==CIOLIB_BUTTON_1_CLICK) {
						i=CIO_KEY_PPAGE;
					}
					/* Clicked Scroll Down */
					else if(mevnt.startx==s_left+left+lbrdrwidth
							&& mevnt.starty==(s_top+top+height)-bbrdrwidth-1
							&& mevnt.event==CIOLIB_BUTTON_1_CLICK) {
						i=CIO_KEY_NPAGE;
					}
					/* Clicked Outside of Window */
					else if((mevnt.startx<s_left+left
							|| mevnt.startx>s_left+left+width-1
							|| mevnt.starty<s_top+top
							|| mevnt.starty>s_top+top+height-1)
							&& (mevnt.event==CIOLIB_BUTTON_1_CLICK
							|| mevnt.event==CIOLIB_BUTTON_3_CLICK)) {
						if(mode&WIN_UNGETMOUSE) {
							ungetmouse(&mevnt);
							i=CIO_KEY_MOUSE;
						}
						else {
							i=ESC;
						}
					}
				}
			}
			/* For compatibility with terminals lacking special keys */
			switch(i) {
				case '\b':
					i=ESC;
					break;
				case '+':
					i=CIO_KEY_IC;	/* insert */
					break;
				case '-':
				case DEL:
					i=CIO_KEY_DC;	/* delete */
					break;
				case CTRL_B:
					if(!(api->mode&UIFC_NOCTRL))
						i=CIO_KEY_HOME;
					break;
				case CTRL_E:
					if(!(api->mode&UIFC_NOCTRL))
						i=CIO_KEY_END;
					break;
				case CTRL_U:
					if(!(api->mode&UIFC_NOCTRL))
						i=CIO_KEY_PPAGE;
					break;
				case CTRL_D:
					if(!(api->mode&UIFC_NOCTRL))
						i=CIO_KEY_NPAGE;
					break;
				case CTRL_Z:
					if(!(api->mode&UIFC_NOCTRL))
						i=CIO_KEY_F(1);	/* help */
					break;
				case CTRL_C:
					if(!(api->mode&UIFC_NOCTRL))
						i=CIO_KEY_F(5);	/* copy */
					break;
				case CTRL_V:
					if(!(api->mode&UIFC_NOCTRL))
						i=CIO_KEY_F(6);	/* paste */
					break;
			}
			if(i>255) {
				s=0;
				switch(i) {
					/* ToDo extended keys */
					case CIO_KEY_HOME:	/* home */
						if(!opts)
							break;
						if(opts+vbrdrsize>optheight) {
							gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
							textattr(api->lclr|(api->bclr<<4));
							putch(' ');    /* Delete the up arrow */
							gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
							putch(31);	   /* put the down arrow */
							uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth
								,api->lbclr
								,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[0]);
							for(i=1;i<optheight-vbrdrsize;i++)    /* re-display options */
								uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth+i
									,api->lclr|(api->bclr<<4)
									,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[i]);
							(*cur)=0;
							if(bar)
								(*bar)=0;
							y=top+tbrdrwidth;
							break; 
						}
						gettext(s_left+left+lbrdrwidth+2,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=api->lclr|(api->bclr<<4);
						puttext(s_left+left+lbrdrwidth+2,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						(*cur)=0;
						if(bar)
							(*bar)=0;
						y=top+tbrdrwidth;
						gettext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=api->lbclr;
						puttext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						break;
					case CIO_KEY_UP:	/* up arrow */
						if(!opts)
							break;
						if(!(*cur) && opts+vbrdrsize>optheight) {
							gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth); /* like end */
							textattr(api->lclr|(api->bclr<<4));
							putch(30);	   /* put the up arrow */
							gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
							putch(' ');    /* delete the down arrow */
							for(i=(opts+vbrdrsize)-optheight,j=0;i<opts;i++,j++)
								uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth+j
									,i==opts-1 ? api->lbclr
										: api->lclr|(api->bclr<<4)
									,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[i]);
							(*cur)=opts-1;
							if(bar)
								(*bar)=optheight-vbrdrsize-1;
							y=top+optheight-bbrdrwidth-1;
							break; 
						}
						gettext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=api->lclr|(api->bclr<<4);
						puttext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						if(!(*cur)) {
							y=top+optheight-bbrdrwidth-1;
							(*cur)=opts-1;
							if(bar)
								(*bar)=optheight-vbrdrsize-1; 
						}
						else {
							(*cur)--;
							y--;
							if(bar && *bar)
								(*bar)--; 
						}
						if(y<top+tbrdrwidth) {	/* scroll */
							if(!(*cur)) {
								gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
								textattr(api->lclr|(api->bclr<<4));
								putch(' '); /* delete the up arrow */
							}
							if((*cur)+optheight-vbrdrsize==opts-1) {
								gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
								textattr(api->lclr|(api->bclr<<4));
								putch(31);	/* put the dn arrow */
							}
							y++;
							scroll_text(s_left+left+lbrdrwidth+1,s_top+top+tbrdrwidth
								,s_left+left+width-rbrdrwidth-1,s_top+top+height-bbrdrwidth-1,1);
							uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth
								,api->lbclr
								,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[*cur]);
						}
						else {
							gettext(s_left+lbrdrwidth+2+left,s_top+y
								,s_left+left+width-rbrdrwidth-1,s_top+y,line);
							for(i=1;i<width*2;i+=2)
								line[i]=api->lbclr;
							puttext(s_left+lbrdrwidth+2+left,s_top+y
								,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						}
						break;
					case CIO_KEY_PPAGE:	/* PgUp */
						if(!opts)
							break;
						*cur -= (optheight-vbrdrsize-1);
						if(*cur<0)
							*cur = 0;
						if(bar)
							*bar=0;
						y=top+tbrdrwidth;
						gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
						textattr(api->lclr|(api->bclr<<4));
						if(*cur && opts>height-tbrdrwidth)  /* Scroll mode */
							putch(30);	   /* put the up arrow */
						else
							putch(' ');    /* delete the up arrow */
						gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
						if(opts > height-tbrdrwidth && *cur + height - vbrdrsize < opts)
							putch(31);	   /* put the down arrow */
						else
							putch(' ');    /* delete the down arrow */
						for(i=*cur,j=0;i<=*cur-vbrdrsize-1+optheight;i++,j++)
							uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth+j
								,i==*cur ? api->lbclr
									: api->lclr|(api->bclr<<4)
								,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[i]);
						break;
					case CIO_KEY_NPAGE:	/* PgDn */
						if(!opts)
							break;
						*cur += (height-vbrdrsize-1);
						if(*cur>opts-1)
							*cur = opts-1;
						if(bar)
							*bar = optheight-vbrdrsize-1;
						y=top+optheight-bbrdrwidth-1;
						gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
						textattr(api->lclr|(api->bclr<<4));
						if(*cur>height-vbrdrsize-1)  /* Scroll mode */
							putch(30);	   /* put the up arrow */
						else
							putch(' ');    /* delete the up arrow */
						gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
						if(*cur < opts-1)
							putch(31);	   /* put the down arrow */
						else
							putch(' ');    /* delete the down arrow */
						for(i=*cur+vbrdrsize+1-optheight,j=0;i<=*cur;i++,j++)
							uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth+j
								,i==*cur ? api->lbclr
									: api->lclr|(api->bclr<<4)
								,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[i]);
						break;
					case CIO_KEY_END:	/* end */
						if(!opts)
							break;
						if(opts+vbrdrsize>height) {	/* Scroll mode */
							gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
							textattr(api->lclr|(api->bclr<<4));
							putch(30);	   /* put the up arrow */
							gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
							putch(' ');    /* delete the down arrow */
							for(i=(opts+vbrdrsize)-height,j=0;i<opts;i++,j++)
								uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth+j
									,i==opts-1 ? api->lbclr
										: api->lclr|(api->bclr<<4)
									,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[i]);
							(*cur)=opts-1;
							y=top+optheight-bbrdrwidth-1;
							if(bar)
								(*bar)=optheight-vbrdrsize-1;
							break; 
						}
						gettext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=api->lclr|(api->bclr<<4);
						puttext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						(*cur)=opts-1;
						y=top+optheight-bbrdrwidth-1;
						if(bar)
							(*bar)=optheight-vbrdrsize-1;
						gettext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						for(i=1;i<148;i+=2)
							line[i]=api->lbclr;
						puttext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						break;
					case CIO_KEY_DOWN:	/* dn arrow */
						if(!opts)
							break;
						if((*cur)==opts-1 && opts+vbrdrsize>height) { /* like home */
							gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
							textattr(api->lclr|(api->bclr<<4));
							putch(' ');    /* Delete the up arrow */
							gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
							putch(31);	   /* put the down arrow */
							uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth
								,api->lbclr
								,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[0]);
							for(i=1;i<height-vbrdrsize;i++)    /* re-display options */
								uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth+i
									,api->lclr|(api->bclr<<4)
									,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[i]);
							(*cur)=0;
							y=top+tbrdrwidth;
							if(bar)
								(*bar)=0;
							break; 
						}
						gettext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						for(i=1;i<width*2;i+=2)
							line[i]=api->lclr|(api->bclr<<4);
						puttext(s_left+lbrdrwidth+2+left,s_top+y
							,s_left+left+width-rbrdrwidth-1,s_top+y,line);
						if((*cur)==opts-1) {
							(*cur)=0;
							y=top+tbrdrwidth;
							if(bar) {
								/* gotoxy(1,1); cprintf("bar=%08lX ",bar); */
								(*bar)=0; 
							}
						}
						else {
							(*cur)++;
							y++;
							if(bar && (*bar)<height-vbrdrsize-1) {
								/* gotoxy(1,1); cprintf("bar=%08lX ",bar); */
								(*bar)++; 
							}
						}
						if(y==top+height-bbrdrwidth) {	/* scroll */
							if(*cur==opts-1) {
								gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
								textattr(api->lclr|(api->bclr<<4));
								putch(' ');	/* delete the down arrow */
							}
							if((*cur)+vbrdrsize==height) {
								gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
								textattr(api->lclr|(api->bclr<<4));
								putch(30);	/* put the up arrow */
							}
							y--;
							/* gotoxy(1,1); cprintf("\rdebug: %4d ",__LINE__); */
							scroll_text(s_left+left+lbrdrwidth+1,s_top+top+tbrdrwidth
								,s_left+left+width-rbrdrwidth-1,s_top+top+height-bbrdrwidth-1,0);
							/* gotoxy(1,1); cprintf("\rdebug: %4d ",__LINE__); */
							uprintf(s_left+left+lbrdrwidth+2,s_top+top+height-bbrdrwidth-1
								,api->lbclr
								,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[*cur]);
						}
						else {
							gettext(s_left+lbrdrwidth+2+left,s_top+y
								,s_left+left+width-rbrdrwidth-1,s_top+y
								,line);
							for(i=1;i<width*2;i+=2)
								line[i]=api->lbclr;
							puttext(s_left+lbrdrwidth+2+left,s_top+y
								,s_left+left+width-rbrdrwidth-1,s_top+y
								,line);
						}
						break;
					case CIO_KEY_F(1):	/* F1 - Help */
						api->showhelp();
						break;
					case CIO_KEY_F(2):	/* F2 - Edit */
						if(mode&WIN_XTR && (*cur)==opts-1)	/* can't edit */
							break;							/* extra line */
						if(mode&WIN_EDIT) {
							if(mode&WIN_EDITACT) {
								gettext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
								for(i=1;i<(width*height*2);i+=2)
									tmp_buffer[i]=api->lclr|(api->cclr<<4);
								j=(((y-top)*width)*2)+7+((width-hbrdrsize-2)*2);
								for(i=(((y-top)*width)*2)+7;i<j;i+=2)
									tmp_buffer[i]=api->hclr|(api->cclr<<4);
								puttext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
							}
							return((*cur)|MSK_EDIT); 
						}
						break;
					case CIO_KEY_F(5):	/* F5 - Copy */
						if(mode&WIN_GET && !(mode&WIN_XTR && (*cur)==opts-1))
							return((*cur)|MSK_GET);
						break;
					case CIO_KEY_F(6):	/* F6 - Paste */
						if(mode&WIN_PUT && !(mode&WIN_XTR && (*cur)==opts-1))
							return((*cur)|MSK_PUT);
						break;
					case CIO_KEY_IC:	/* insert */
						if(mode&WIN_INS) {
							if(mode&WIN_INSACT) {
								gettext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
								for(i=1;i<(width*height*2);i+=2)
									tmp_buffer[i]=api->lclr|(api->cclr<<4);
								if(opts) {
									j=(((y-top)*width)*2)+7+((width-hbrdrsize-2)*2);
									for(i=(((y-top)*width)*2)+7;i<j;i+=2)
										tmp_buffer[i]=api->hclr|(api->cclr<<4); 
								}
								puttext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
							}
							if(!opts) {
								return(MSK_INS); 
							}
							return((*cur)|MSK_INS); 
						}
						break;
					case CIO_KEY_DC:	/* delete */
						if(mode&WIN_XTR && (*cur)==opts-1)	/* can't delete */
							break;							/* extra line */
						if(mode&WIN_DEL) {
							if(mode&WIN_DELACT) {
								gettext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
								for(i=1;i<(width*height*2);i+=2)
									tmp_buffer[i]=api->lclr|(api->cclr<<4);
								j=(((y-top)*width)*2)+7+((width-hbrdrsize-2)*2);
								for(i=(((y-top)*width)*2)+7;i<j;i+=2)
									tmp_buffer[i]=api->hclr|(api->cclr<<4);
								puttext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
							}
							return((*cur)|MSK_DEL); 
						}
						break;
					default:
						if(mode&WIN_EXTKEYS)
							return(-2-i);
						break;
				} 
			}
			else {
				i&=0xff;
				if(isalnum(i) && opts && option[0][0]) {
					search[s]=i;
					search[s+1]=0;
					for(j=(*cur)+1,a=b=0;a<2;j++) {   /* a = search count */
						if(j==opts) {					/* j = option count */
							j=-1;						/* b = letter count */
							continue; 
						}
						if(j==(*cur)) {
							b++;
							continue; 
						}
						if(b>=longopt) {
							b=0;
							a++; 
						}
						if(a==1 && !s)
							break;
						if(strlen(option[j])>(size_t)b
							&& ((!a && s && !strnicmp(option[j]+b,search,s+1))
							|| ((a || !s) && toupper(option[j][b])==toupper(i)))) {
							if(a) s=0;
							else s++;
							if(y+(j-(*cur))+2>height+top) {
								(*cur)=j;
								gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
								textattr(api->lclr|(api->bclr<<4));
								putch(30);	   /* put the up arrow */
								if((*cur)==opts-1) {
									gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
									putch(' ');	/* delete the down arrow */
								}
								for(i=((*cur)+vbrdrsize+1)-height,j=0;i<(*cur)+1;i++,j++)
									uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth+j
										,i==(*cur) ? api->lbclr
											: api->lclr|(api->bclr<<4)
										,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[i]);
								y=top+height-bbrdrwidth-1;
								if(bar)
									(*bar)=optheight-vbrdrsize-1;
								break; 
							}
							if(y-((*cur)-j)<top+tbrdrwidth) {
								(*cur)=j;
								gotoxy(s_left+left+lbrdrwidth,s_top+top+tbrdrwidth);
								textattr(api->lclr|(api->bclr<<4));
								if(!(*cur))
									putch(' ');    /* Delete the up arrow */
								gotoxy(s_left+left+lbrdrwidth,s_top+top+height-bbrdrwidth-1);
								putch(31);	   /* put the down arrow */
								uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth
									,api->lbclr
									,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2,option[(*cur)]);
								for(i=1;i<height-vbrdrsize;i++) 	/* re-display options */
									uprintf(s_left+left+lbrdrwidth+2,s_top+top+tbrdrwidth+i
										,api->lclr|(api->bclr<<4)
										,"%-*.*s",width-hbrdrsize-2,width-hbrdrsize-2
										,option[(*cur)+i]);
								y=top+tbrdrwidth;
								if(bar)
									(*bar)=0;
								break; 
							}
							gettext(s_left+lbrdrwidth+2+left,s_top+y
								,s_left+left+width-rbrdrwidth-1,s_top+y,line);
							for(i=1;i<width*2;i+=2)
								line[i]=api->lclr|(api->bclr<<4);
							puttext(s_left+lbrdrwidth+2+left,s_top+y
								,s_left+left+width-rbrdrwidth-1,s_top+y,line);
							if((*cur)>j)
								y-=(*cur)-j;
							else
								y+=j-(*cur);
							if(bar) {
								if((*cur)>j)
									(*bar)-=(*cur)-j;
								else
									(*bar)+=j-(*cur); 
							}
							(*cur)=j;
							gettext(s_left+lbrdrwidth+2+left,s_top+y
								,s_left+left+width-rbrdrwidth-1,s_top+y,line);
							for(i=1;i<width*2;i+=2)
								line[i]=api->lbclr;
							puttext(s_left+lbrdrwidth+2+left,s_top+y
								,s_left+left+width-rbrdrwidth-1,s_top+y,line);
							break; 
						} 
					}
					if(a==2)
						s=0;
				}
				else
					switch(i) {
						case CR:
							if(!opts)
								break;
							if(mode&WIN_ACT) {
								gettext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
								for(i=1;i<(width*height*2);i+=2)
									tmp_buffer[i]=api->lclr|(api->cclr<<4);
								j=(((y-top)*width)*2)+7+((width-hbrdrsize-2)*2);
								for(i=(((y-top)*width)*2)+7;i<j;i+=2)
									tmp_buffer[i]=api->hclr|(api->cclr<<4);

								puttext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
							}
							else if(mode&WIN_SAV) {
								puttext(sav[api->savnum].left,sav[api->savnum].top
									,sav[api->savnum].right,sav[api->savnum].bot
									,sav[api->savnum].buf);
								free(sav[api->savnum].buf);
								api->savdepth--; 
							}
							if(mode&WIN_XTR && (*cur)==opts-1)
								return(MSK_INS|*cur);
							return(*cur);
						case 3:
						case ESC:
							if((mode&WIN_ESC || (mode&WIN_CHE && api->changes))
								&& !(mode&WIN_SAV)) {
								gettext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
								for(i=1;i<(width*height*2);i+=2)
									tmp_buffer[i]=api->lclr|(api->cclr<<4);
								puttext(s_left+left,s_top+top,s_left
									+left+width-1,s_top+top+height-1,tmp_buffer);
							}
							else if(mode&WIN_SAV) {
								puttext(sav[api->savnum].left,sav[api->savnum].top
									,sav[api->savnum].right,sav[api->savnum].bot
									,sav[api->savnum].buf);
								free(sav[api->savnum].buf);
								api->savdepth--; 
							}
							return(-1);
						default:
							if(mode&WIN_EXTKEYS)
								return(-2-i);
				}
			}
		}
		else
			mswait(1);
		if(mode&WIN_DYN) {
			save_menu_cur=*cur;
			save_menu_bar=*bar;
			save_menu_opts=opts;
			return(-2-i);
		}
	}
}


/*************************************************************************/
/* This function is a windowed input string input routine.               */
/*************************************************************************/
int uinput(int mode, int left, int top, char *inprompt, char *str,
	int max, int kmode)
{
	unsigned char save_buf[2048],in_win[2048]
		,shade[160];
	int	width;
	int height=3;
	int i,plen,slen,j;
	int	iwidth;
	int l;
	char *prompt;
	int s_top=SCRN_TOP;
	int s_left=SCRN_LEFT;
	int s_right=SCRN_RIGHT;
	int s_bottom=api->scrn_len-3;
	int hbrdrsize=2;
	int lbrdrwidth=1;
	int rbrdrwidth=1;
	int vbrdrsize=4;
	int tbrdrwidth=1;
	int bbrdrwidth=1;

	reset_dynamic();

	if(mode&WIN_FAT) {
		s_top=1;
		s_left=2;
		s_right=api->scrn_width-3;  /* Leave space for the shadow */
		s_bottom=api->scrn_len-1;   /* Leave one for the shadow */
	}
	if(mode&WIN_NOBRDR) {
		hbrdrsize=0;
		vbrdrsize=0;
		lbrdrwidth=0;
		rbrdrwidth=0;
		tbrdrwidth=0;
		bbrdrwidth=0;
		height=1;
	}

	prompt=strdup(inprompt==NULL ? "":inprompt);
	plen=strlen(prompt);
	if(!plen)
		slen=2+hbrdrsize;
	else
		slen=4+hbrdrsize;

	width=plen+slen+max;
	if(width>(s_right-s_left+1))
		width=(s_right-s_left+1);
	if(mode&WIN_T2B)
		top=(api->scrn_len-height+1)/2-2;
	if(mode&WIN_L2R)
		left=(s_right-s_left-width+1)/2;
	if(left<=-(s_left))
		left=-(s_left)+1;
	if(top<0)
		top=0;
	if(mode&WIN_SAV)
		gettext(s_left+left,s_top+top,s_left+left+width+1
			,s_top+top+height,save_buf);
	iwidth=width-plen-slen;
	while(iwidth<1 && plen>4) {
		plen=strlen(prompt);
		prompt[plen-1]=0;
		prompt[plen-2]='.';
		prompt[plen-3]='.';
		prompt[plen-4]='.';
		plen--;
		iwidth=width-plen-slen;
	}

	i=0;
	if(!(mode&WIN_NOBRDR)) {
		in_win[i++]='É';
		in_win[i++]=api->hclr|(api->bclr<<4);
		for(j=1;j<width-1;j++) {
			in_win[i++]='Í';
			in_win[i++]=api->hclr|(api->bclr<<4);
		}
		if(api->mode&UIFC_MOUSE && width>6) {
			j=2;
			in_win[j++]='[';
			in_win[j++]=api->hclr|(api->bclr<<4);
			/* in_win[4]='þ'; */
			in_win[j++]=0xfe;
			in_win[j++]=api->lclr|(api->bclr<<4);
			in_win[j++]=']';
			in_win[j++]=api->hclr|(api->bclr<<4);
			l=3;
			if(api->helpbuf!=NULL || api->helpixbfile[0]!=0) {
				in_win[j++]='[';
				in_win[j++]=api->hclr|(api->bclr<<4);
				in_win[j++]='?';
				in_win[j++]=api->lclr|(api->bclr<<4);
				in_win[j++]=']';
				in_win[j++]=api->hclr|(api->bclr<<4);
				l+=3;
			}
			api->buttony=s_top+top;
			api->exitstart=s_left+left+1;
			api->exitend=s_left+left+3;
			api->helpstart=s_left+left+4;
			api->helpend=s_left+left+l;
		}

		in_win[i++]='»';
		in_win[i++]=api->hclr|(api->bclr<<4);
		in_win[i++]='º';
		in_win[i++]=api->hclr|(api->bclr<<4);
	}

	if(plen) {
		in_win[i++]=' ';
		in_win[i++]=api->lclr|(api->bclr<<4); 
	}

	for(j=0;prompt[j];j++) {
		in_win[i++]=prompt[j];
		in_win[i++]=api->lclr|(api->bclr<<4); 
	}

	if(plen) {
		in_win[i++]=':';
		in_win[i++]=api->lclr|(api->bclr<<4);
	}

	for(j=0;j<iwidth+2;j++) {
		in_win[i++]=' ';
		in_win[i++]=api->lclr|(api->bclr<<4); 
	}

	if(!(mode&WIN_NOBRDR)) {
		in_win[i++]='º';
		in_win[i++]=api->hclr|(api->bclr<<4);
		in_win[i++]='È';
		in_win[i++]=api->hclr|(api->bclr<<4);
		for(j=1;j<width-1;j++) {
			in_win[i++]='Í';
			in_win[i++]=api->hclr|(api->bclr<<4); 
		}
		in_win[i++]='¼';
		in_win[i]=api->hclr|(api->bclr<<4);	/* I is not incremented to shut up BCC */
	}
	puttext(s_left+left,s_top+top,s_left+left+width-1
		,s_top+top+height-1,in_win);

	if(!(mode&WIN_NOBRDR)) {
		/* Shadow */
		if(api->bclr==BLUE) {
			gettext(s_left+left+width,s_top+top+1,s_left+left+width+1
				,s_top+top+(height-1),shade);
			for(j=1;j<12;j+=2)
				shade[j]=DARKGRAY;
			puttext(s_left+left+width,s_top+top+1,s_left+left+width+1
				,s_top+top+(height-1),shade);
			gettext(s_left+left+2,s_top+top+3,s_left+left+width+1
				,s_top+top+height,shade);
			for(j=1;j<width*2;j+=2)
				shade[j]=DARKGRAY;
			puttext(s_left+left+2,s_top+top+3,s_left+left+width+1
				,s_top+top+height,shade); 
		}
	}

	textattr(api->lclr|(api->bclr<<4));
	if(!plen)
		i=ugetstr(s_left+left+2,s_top+top+tbrdrwidth,iwidth,str,max,kmode,NULL);
	else
		i=ugetstr(s_left+left+plen+4,s_top+top+tbrdrwidth,iwidth,str,max,kmode,NULL);
	if(mode&WIN_SAV)
		puttext(s_left+left,s_top+top,s_left+left+width+1
			,s_top+top+height,save_buf);
	free(prompt);
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

/***************************************/
/* Private sub - updates a ugetstr box */
/***************************************/
void getstrupd(int left, int top, int width, char *outstr, int cursoffset, int *scrnoffset)
{
	_setcursortype(_NOCURSOR);
	if(cursoffset<*scrnoffset)
		*scrnoffset=cursoffset;

	if(*scrnoffset+width < cursoffset)
		*scrnoffset=cursoffset-width;

	gotoxy(left,top);
	cprintf("%-*.*s",width,width,outstr+*scrnoffset);
	gotoxy(left+(cursoffset-*scrnoffset),top);
	_setcursortype(cursor);
}

/****************************************************************************/
/* Gets a string of characters from the user. Turns cursor on. Allows 	    */
/* Different modes - K_* macros. ESC aborts input.                          */
/****************************************************************************/
int ugetstr(int left, int top, int width, char *outstr, int max, long mode, int *lastkey)
{
	uchar   *str,ins=0;
	int	ch;
	int     i,j,k,f=0;	/* i=offset, j=length */
	BOOL	gotdecimal=FALSE;
	int	soffset=0;
	struct mouse_event	mevnt;
	unsigned char	*pastebuf=NULL;
	unsigned char	*pb=NULL;

	if((str=(uchar *)malloc(max+1))==NULL) {
		cprintf("UIFC line %d: error allocating %u bytes\r\n"
			,__LINE__,(max+1));
		_setcursortype(cursor);
		return(-1); 
	}
	gotoxy(left,top);
	cursor=_NORMALCURSOR;
	_setcursortype(cursor);
	str[0]=0;
	if(mode&K_EDIT && outstr[0]) {
	/***
		truncsp(outstr);
	***/
		outstr[max]=0;
		i=j=strlen(outstr);
		textattr(api->lbclr);
		getstrupd(left, top, width, outstr, i, &soffset);
		textattr(api->lclr|(api->bclr<<4));
		if(strlen(outstr)<(size_t)width) {
			k=wherex();
			f=wherey();
			cprintf("%*s",width-strlen(outstr),"");
			gotoxy(k,f);
		}
		strcpy(str,outstr);
#if 0
		while(kbwait()==0) {
			mswait(1);
		}
#endif
		f=inkey();
		if(f==CIO_KEY_MOUSE) {
			if((f=uifc_getmouse(&mevnt))==0) {
				if(mode & K_MOUSEEXIT
						&& (mevnt.starty != top
							|| mevnt.startx > left+width
						    || mevnt.startx < left)
						&& mevnt.event==CIOLIB_BUTTON_1_CLICK) {
					if(lastkey)
						*lastkey=CIO_KEY_MOUSE;
					ungetmouse(&mevnt);
					return(j);
				}
				if(mevnt.startx>=left
						&& mevnt.startx<=left+width
						&& mevnt.event==CIOLIB_BUTTON_1_CLICK) {
					i=mevnt.startx-left+soffset;
					if(i>j)
						i=j;
				}
				if(mevnt.starty == top
						&& mevnt.startx>=left
						&& mevnt.startx<=left+width
						&& (mevnt.event==CIOLIB_BUTTON_2_CLICK
						|| mevnt.event==CIOLIB_BUTTON_3_CLICK)) {
					i=mevnt.startx-left+soffset;
					if(i>j)
						i=j;
					pastebuf=getcliptext();
					pb=pastebuf;
					f=0;
				}
			}
		}

		if(f == CR 
				|| (f >= 0xff && f != CIO_KEY_DC) 
				|| (f == '\t' && mode&K_TABEXIT) 
				|| (f == '%' && mode&K_SCANNING)
				|| f==CTRL_B
				|| f==CTRL_E
				|| f==CTRL_V
				|| f==CTRL_Z
				|| f==0)
		{
			getstrupd(left, top, width, str, i, &soffset);
		}
		else
		{
			getstrupd(left, top, width, str, i, &soffset);
			i=j=0;
		}
	}
	else
		i=j=0;

	ch=0;
	while(ch!=CR)
	{
		if(i>j) j=i;
		str[j]=0;
		getstrupd(left, top, width, str, i, &soffset);
		if(f || pb!=NULL || (ch=inkey())!=0)
		{
			if(f) {
				ch=f;
				f=0;
			}
			else if(pb!=NULL) {
				ch=*(pb++);
				if(!*pb) {
					free(pastebuf);
					pastebuf=NULL;
					pb=NULL;
				}
			}
			if(ch==CIO_KEY_MOUSE) {
				if((ch=uifc_getmouse(&mevnt))==0) {
					if(mode & K_MOUSEEXIT 
							&& (mevnt.starty != top
								|| mevnt.startx > left+width
						    	|| mevnt.startx < left)
							&& mevnt.event==CIOLIB_BUTTON_1_CLICK) {
						if(lastkey)
							*lastkey=CIO_KEY_MOUSE;
						ungetmouse(&mevnt);
						ch=CR;
						continue;
					}
					if(mevnt.starty == top
							&& mevnt.startx>=left
							&& mevnt.startx<=left+width
							&& mevnt.event==CIOLIB_BUTTON_1_CLICK) {
						i=mevnt.startx-left+soffset;
						if(i>j)
							i=j;
					}
					if(mevnt.starty == top
							&& mevnt.startx>=left
							&& mevnt.startx<=left+width
							&& (mevnt.event==CIOLIB_BUTTON_2_CLICK
							|| mevnt.event==CIOLIB_BUTTON_3_CLICK)) {
						i=mevnt.startx-left+soffset;
						if(i>j)
							i=j;
						pastebuf=getcliptext();
						pb=pastebuf;
					}
				}
			}
			if(lastkey != NULL)
				*lastkey=ch;
			switch(ch)
			{
				case CTRL_Z:
				case CIO_KEY_F(1):	/* F1 Help */
					api->showhelp();
					continue;
				case CIO_KEY_LEFT:	/* left arrow */
					if(i)
					{
						i--;
					}
					continue;
				case CIO_KEY_RIGHT:	/* right arrow */
					if(i<j)
					{
						i++;
					}
					continue;
				case CTRL_B:
				case CIO_KEY_HOME:	/* home */
					if(i)
					{
						i=0;
					}
					continue;
				case CTRL_E:
				case CIO_KEY_END:	/* end */
					if(i<j)
					{
						i=j;
					}
					continue;
				case CTRL_V:
				case CIO_KEY_IC:	/* insert */
					ins=!ins;
					if(ins)
						cursor=_SOLIDCURSOR;
					else
						cursor=_NORMALCURSOR;
					_setcursortype(cursor);
					continue;
				case BS:
					if(i)
					{
						if(i==j)
						{
							j--;
							i--;
						}
						else {
							i--;
							j--;
							if(str[i]=='.')
								gotdecimal=FALSE;
							for(k=i;k<=j;k++)
								str[k]=str[k+1]; 
						}
						continue;
					}
				case CIO_KEY_DC:	/* delete */
					if(i<j)
					{
						if(str[i]=='.')
							gotdecimal=FALSE;
						for(k=i;k<j;k++)
							str[k]=str[k+1];
						j--;
					}
					continue;
				case CTRL_C:
				case ESC:
					{
						cursor=_NOCURSOR;
						_setcursortype(cursor);
						if(pastebuf!=NULL)
							free(pastebuf);
						return(-1);
					}
				case CR:
					break;
				case 3840:	/* Backtab */
				case '\t':
					if(mode&K_TABEXIT)
						ch=CR;
					break;
				case '%':	/* '%' indicates that a UPC is coming next */
					if(mode&K_SCANNING)
						ch=CR;
					break;
				case CIO_KEY_F(2):
				case CIO_KEY_UP:
				case CIO_KEY_DOWN:
					if(mode&K_DEUCEEXIT) {
						ch=CR;
						break;
					}
					continue;
				case CTRL_X:
					if(j)
					{
						i=j=0;
					}
					continue;
				case CTRL_Y:
					if(i<j)
					{
						j=i;
					}
					continue;
			}
			if(mode&K_NUMBER && !isdigit(ch))
				continue;
			if(mode&K_DECIMAL && !isdigit(ch)) {
				if(ch!='.')
					continue;
				if(gotdecimal)
					continue;
				gotdecimal=TRUE;
			}
			if(mode&K_ALPHA && !isalpha(ch))
				continue;
#if 0
			/* This broke swedish chars... */
			if((ch>=' ' || (ch==1 && mode&K_MSG)) && i<max && (!ins || j<max) && isprint(ch))
#else
			if((ch>=' ' || (ch==1 && mode&K_MSG)) && i<max && (!ins || j<max) && ch < 256)
#endif
			{
				if(mode&K_UPPER)
					ch=toupper(ch);
				if(ins)
				{
					for(k=++j;k>i;k--)
						str[k]=str[k-1];
				}
				str[i++]=ch; 
			}
		}
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
	cursor=_NOCURSOR;
	_setcursortype(cursor);
	if(pastebuf!=NULL)
		free(pastebuf);
	return(j);
}

/****************************************************************************/
/* Performs printf() through puttext() routine								*/
/****************************************************************************/
static int uprintf(int x, int y, unsigned attr, char *fmat, ...)
{
	va_list argptr;
	char str[256],buf[512];
	int i,j;

    va_start(argptr,fmat);
    vsprintf(str,fmat,argptr);
    va_end(argptr);
    for(i=j=0;str[i];i++) {
        buf[j++]=str[i];
        buf[j++]=attr; 
	}
    puttext(x,y,x+(i-1),y,buf);
    return(i);
}


/****************************************************************************/
/* Display bottom line of screen in inverse                                 */
/****************************************************************************/
void bottomline(int line)
{
	int i=0;

	uprintf(i,api->scrn_len+1,api->bclr|(api->cclr<<4),"    ");
	i+=4;
	if(line&BL_HELP) {
		uprintf(i,api->scrn_len+1,api->bclr|(api->cclr<<4),"F1 ");
		i+=3;
		uprintf(i,api->scrn_len+1,BLACK|(api->cclr<<4),"Help  ");
		i+=6;
	}
	if(line&BL_EDIT) {
		uprintf(i,api->scrn_len+1,api->bclr|(api->cclr<<4),"F2 ");
		i+=3;
		uprintf(i,api->scrn_len+1,BLACK|(api->cclr<<4),"Edit Item  ");
		i+=11; 
	}
	if(line&BL_GET) {
		uprintf(i,api->scrn_len+1,api->bclr|(api->cclr<<4),"F5 ");
		i+=3;
		uprintf(i,api->scrn_len+1,BLACK|(api->cclr<<4),"Copy Item  ");
		i+=11; 
	}
	if(line&BL_PUT) {
		uprintf(i,api->scrn_len+1,api->bclr|(api->cclr<<4),"F6 ");
		i+=3;
		uprintf(i,api->scrn_len+1,BLACK|(api->cclr<<4),"Paste  ");
		i+=7; 
	}
	if(line&BL_INS) {
		uprintf(i,api->scrn_len+1,api->bclr|(api->cclr<<4),"INS ");
		i+=4;
		uprintf(i,api->scrn_len+1,BLACK|(api->cclr<<4),"Add Item  ");
		i+=10; 
	}
	if(line&BL_DEL) {
		uprintf(i,api->scrn_len+1,api->bclr|(api->cclr<<4),"DEL ");
		i+=4;
		uprintf(i,api->scrn_len+1,BLACK|(api->cclr<<4),"Delete Item  ");
		i+=13; 
	}
	uprintf(i,api->scrn_len+1,api->bclr|(api->cclr<<4),"ESC ");	/* Backspace is no good no way to abort editing */
	i+=4;
	uprintf(i,api->scrn_len+1,BLACK|(api->cclr<<4),"Exit");
	i+=4;
	gotoxy(i,api->scrn_len+1);
	textattr(BLACK|(api->cclr<<4));
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
			break; 
	}
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
			break; 
	}
	if(gm->tm_hour>12) {
		strcpy(mer,"pm");
		hour=gm->tm_hour-12; 
	}
	else {
		if(!gm->tm_hour)
			hour=12;
		else
			hour=gm->tm_hour;
		strcpy(mer,"am"); 
	}
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

	if(!str) {
		/* puttext(28,12,53,14,sav); */
		puttext((api->scrn_width-26+1)/2+1,(api->scrn_len-3+1)/2+1
			,(api->scrn_width+26-1)/2+1,(api->scrn_len+3-1)/2+1,sav);
		return;
	}
	/* gettext(28,12,53,14,sav); */
	gettext((api->scrn_width-26+1)/2+1,(api->scrn_len-3+1)/2+1
			,(api->scrn_width+26-1)/2+1,(api->scrn_len+3-1)/2+1,sav);
	memset(buf,' ',25*3*2);
	for(i=1;i<26*3*2;i+=2)
		buf[i]=(api->hclr|(api->bclr<<4));
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
		buf[i+1]|=BLINK;
	}
	i=((25*2)+1)*2;
	buf[i]='³'; i+=2;
	buf[i]='À'; i+=2;
	for(;i<((26*3)-1)*2;i+=2)
		buf[i]='Ä';
	buf[i]='Ù';

	/* puttext(28,12,53,14,buf); */
	puttext((api->scrn_width-26+1)/2+1,(api->scrn_len-3+1)/2+1
			,(api->scrn_width+26-1)/2+1,(api->scrn_len+3-1)/2+1,buf);
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
	char inverse=0,high=0;
	char *textbuf;
    char *p;
	char *oldp=NULL;
	int i,j,k,len;
	int	 lines;
	int pad=1;
	int	is_redraw=0;
	uint title_len=0;
	struct mouse_event	mevnt;

	_setcursortype(_NOCURSOR);
	
	title_len=strlen(title);
	if(api->mode&UIFC_MOUSE)
		title_len+=6;

	if((unsigned)(top+height)>api->scrn_len-3)
		height=(api->scrn_len-3)-top;
	if(!width || (unsigned)width<title_len+6)
		width=title_len+6;
	if((unsigned)width>api->scrn_width)
		width=api->scrn_width;
	if(mode&WIN_L2R)
		left=(api->scrn_width-width+2)/2;
	else if(mode&WIN_RHT)
		left=SCRN_RIGHT-(width+4+left);
	if(mode&WIN_T2B)
		top=(api->scrn_len-height+1)/2;
	else if(mode&WIN_BOT)
		top=api->scrn_len-height-3-top;
	if(left<0)
		left=0;
	if(top<0)
		top=0;

	if(mode&WIN_PACK)
		pad=0;

	/* Dynamic Menus */
	if(mode&WIN_DYN
			&& curp != NULL
			&& barp != NULL
			&& last_menu_cur==curp
			&& last_menu_bar==barp
			&& save_menu_cur==*curp
			&& save_menu_bar==*barp)
		is_redraw=1;
	if(mode&WIN_DYN && mode&WIN_REDRAW)
		is_redraw=1;
	if(mode&WIN_DYN && mode&WIN_NODRAW)
		is_redraw=0;

	gettext(1,1,api->scrn_width,api->scrn_len,tmp_buffer);

	if(!is_redraw) {
		memset(tmp_buffer2,' ',width*height*2);
		for(i=1;i<width*height*2;i+=2)
			tmp_buffer2[i]=(api->hclr|(api->bclr<<4));
	    tmp_buffer2[0]='Ú';
		j=title_len;
		if(j>width-6) {
			*(title+width-6)=0;
			j=width-6;
		}
		for(i=2;i<(width-j);i+=2)
   		      tmp_buffer2[i]='Ä';
		if(api->mode&UIFC_MOUSE && !mode&WIN_DYN) {
			tmp_buffer2[2]='[';
			tmp_buffer2[3]=api->hclr|(api->bclr<<4);
			/* tmp_buffer2[4]='þ'; */
			tmp_buffer2[4]=0xfe;
			tmp_buffer2[5]=api->lclr|(api->bclr<<4);
			tmp_buffer2[6]=']';
			tmp_buffer2[7]=api->hclr|(api->bclr<<4);
			/* Buttons are ignored - leave it this way to not confuse stuff from help() */
		}
	    tmp_buffer2[i]='´'; i+=4;
		for(p=title;*p;p++) {
			tmp_buffer2[i]=*p;
			i+=2;
		}
		i+=2;
   		tmp_buffer2[i]='Ã'; i+=2;
		for(j=i;j<((width-1)*2);j+=2)
   		    tmp_buffer2[j]='Ä';
		i=j;
    	tmp_buffer2[i]='¿'; i+=2;
		j=i;	/* leave i alone */
		for(k=0;k<(height-2);k++) { 		/* the sides of the box */
	        tmp_buffer2[j]='³'; j+=2;
			j+=((width-2)*2);
        	tmp_buffer2[j]='³'; j+=2; 
		}
	    tmp_buffer2[j]='À'; j+=2;
		if(!(mode&WIN_DYN) && (width>31)) {
			for(k=j;k<j+(((width-4)/2-13)*2);k+=2)
				tmp_buffer2[k]='Ä';
			tmp_buffer2[k]='´'; k+=4;
			tmp_buffer2[k]='H'; k+=2;
			tmp_buffer2[k]='i'; k+=2;
			tmp_buffer2[k]='t'; k+=4;
			tmp_buffer2[k]='a'; k+=2;
			tmp_buffer2[k]='n'; k+=2;
			tmp_buffer2[k]='y'; k+=4;
			tmp_buffer2[k]='k'; k+=2;
			tmp_buffer2[k]='e'; k+=2;
			tmp_buffer2[k]='y'; k+=4;
			tmp_buffer2[k]='t'; k+=2;
			tmp_buffer2[k]='o'; k+=4;
			tmp_buffer2[k]='c'; k+=2;
			tmp_buffer2[k]='o'; k+=2;
			tmp_buffer2[k]='n'; k+=2;
			tmp_buffer2[k]='t'; k+=2;
			tmp_buffer2[k]='i'; k+=2;
			tmp_buffer2[k]='n'; k+=2;
			tmp_buffer2[k]='u'; k+=2;
			tmp_buffer2[k]='e'; k+=4;
	    	tmp_buffer2[k]='Ã'; k+=2;
			for(j=k;j<k+(((width-4)/2-12)*2);j+=2)
		        tmp_buffer2[j]='Ä';
		}
		else {
			for(k=j;k<j+((width-2)*2);k+=2)
				tmp_buffer2[k]='Ä';
			j=k;
		}
	    tmp_buffer2[j]='Ù';
		puttext(left,top+1,left+width-1,top+height,tmp_buffer2);
	}
	len=strlen(hbuf);

	lines=0;
	k=0;
	for(j=0;j<len;j++) {
		if(mode&WIN_HLP && (hbuf[j]==2 || hbuf[j]=='~' || hbuf[j]==1 || hbuf[j]=='`'))
			continue;
		if(hbuf[j]==CR)
			continue;
		k++;
		if((hbuf[j]==LF) || (k>=width-2-pad-pad && (hbuf[j+1]!='\n' && hbuf[j+1]!='\r'))) {
			k=0;
			lines++;
		}
	}
	if(k)
		lines++;
	if(lines < height-2-pad-pad)
		lines=height-2-pad-pad;

	if((textbuf=(char *)malloc((width-2-pad-pad)*lines*2))==NULL) {
		cprintf("UIFC line %d: error allocating %u bytes\r\n"
			,__LINE__,(width-2-pad-pad)*lines*2);
		_setcursortype(cursor);
		return; 
	}
	memset(textbuf,' ',(width-2-pad-pad)*lines*2);
	for(i=1;i<(width-2-pad-pad)*lines*2;i+=2)
		textbuf[i]=(api->hclr|(api->bclr<<4));

	i=0;

	for(j=i;j<len;j++,i+=2) {
		if(hbuf[j]==LF) {
			i+=2;
			while(i%((width-2-pad-pad)*2)) i++; i-=2;
		}
		else if(mode&WIN_HLP && (hbuf[j]==2 || hbuf[j]=='~')) {		 /* Ctrl-b toggles inverse */
			inverse=!inverse;
			i-=2; 
		}
		else if(mode&WIN_HLP && (hbuf[j]==1 || hbuf[j]=='`')) {		 /* Ctrl-a toggles high intensity */
			high=!high;
			i-=2; 
		}
		else if(hbuf[j]!=CR) {
			textbuf[i]=hbuf[j];
			textbuf[i+1]=inverse ? (api->bclr|(api->cclr<<4))
				: high ? (api->hclr|(api->bclr<<4)) : (api->lclr|(api->bclr<<4));
			if((i+2)%((width-2-pad-pad)*2)==0 && (hbuf[j+1]==LF) || (hbuf[j+1]==CR && hbuf[j+2]==LF))
				i-=2;
		}
		else
			i-=2;
	}
	i=0;
	p=textbuf;
	if(mode&WIN_DYN) {
		puttext(left+1+pad,top+2+pad,left+width-2-pad,top+height-1-pad,p);
	}
	else {
		while(i==0) {
			if(p!=oldp) {
				if(p > textbuf+(lines-(height-2-pad-pad))*(width-2-pad-pad)*2)
					p=textbuf+(lines-(height-2-pad-pad))*(width-2-pad-pad)*2;
				if(p<textbuf)
					p=textbuf;
				if(p!=oldp) {
					puttext(left+1+pad,top+2+pad,left+width-2-pad,top+height-1-pad,p);
					oldp=p;
				}
			}
			if(kbwait()) {
				j=inkey();
				if(j==CIO_KEY_MOUSE) {
					/* Ignores return value to avoid hitting help/exit hotspots */
					if(uifc_getmouse(&mevnt)>=0) {
						/* Clicked Scroll Up */
						if(mevnt.startx>=left+pad
								&& mevnt.startx<=left+pad+width-3
								&& mevnt.starty>=top+pad+1
								&& mevnt.starty<=top+pad+(height/2)-2
								&& mevnt.event==CIOLIB_BUTTON_1_CLICK) {
							p = p-((width-2-pad-pad)*2*(height-5));
							continue;
						}
						/* Clicked Scroll Down */
						else if(mevnt.startx>=left+pad
								&& mevnt.startx<=left+pad+width
								&& mevnt.starty<=top+pad+height-2
								&& mevnt.starty>=top+pad+height-(height/2+1)-2
								&& mevnt.event==CIOLIB_BUTTON_1_CLICK) {
							p=p+(width-2-pad-pad)*2*(height-5);
							continue;
						}
						i=1;
					}
					continue;
				}
				switch(j) {
					case CIO_KEY_HOME:	/* home */
						p=textbuf;
						break;

					case CIO_KEY_UP:	/* up arrow */
						p = p-((width-2-pad-pad)*2);
						break;
					
					case CIO_KEY_PPAGE:	/* PgUp */
						p = p-((width-2-pad-pad)*2*(height-5));
						break;

					case CIO_KEY_NPAGE:	/* PgDn */
						p=p+(width-2-pad-pad)*2*(height-5);
						break;

					case CIO_KEY_END:	/* end */
						p=textbuf+(lines-height+1)*(width-2-pad-pad)*2;
						break;

					case CIO_KEY_DOWN:	/* dn arrow */
						p = p+((width-2-pad-pad)*2);
						break;

					default:
						i=1;
				}
			}
			mswait(1);
		}

		puttext(1,1,api->scrn_width,api->scrn_len,tmp_buffer);
	}
	if(is_redraw)			/* Force redraw of menu also. */
		reset_dynamic();
	free(textbuf);
	_setcursortype(cursor);
}

/************************************************************/
/* Help (F1) key function. Uses helpbuf as the help input.	*/
/************************************************************/
static void help(void)
{
	char hbuf[HELPBUF_SIZE],str[256];
    char *p;
	unsigned short line;	/* This must be 16-bits */
	long l;
	FILE *fp;

	if(api->helpbuf==NULL && api->helpixbfile[0]==0)
		return;

	_setcursortype(_NOCURSOR);

	if(api->helpbuf!=NULL)
		strcpy(hbuf,api->helpbuf);
	else {
		if((fp=fopen(api->helpixbfile,"rb"))==NULL) {
			sprintf(hbuf,"\2 ERROR \2 Cannot open help index:\r\n          %s"
				,api->helpixbfile);
		}
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
				fread(&line,sizeof(line),1,fp);
				if(stricmp(str,p) || line!=helpline) {
					fseek(fp,sizeof(l),SEEK_CUR);
					continue; 
				}
				fread(&l,sizeof(l),1,fp);
				break;
			}
			fclose(fp);
			if(l==-1L)
				sprintf(hbuf,"\2 ERROR \2 Cannot locate help key (%s:%u) in:\r\n"
					"         %s",p,helpline,api->helpixbfile);
			else {
				if((fp=fopen(api->helpdatfile,"rb"))==NULL)
					sprintf(hbuf,"\2 ERROR \2 Cannot open help file:\r\n          %s"
						,api->helpdatfile);
				else {
					fseek(fp,l,SEEK_SET);
					fread(hbuf,HELPBUF_SIZE,1,fp);
					fclose(fp); 
				}
			}
		}
	}

	showbuf(WIN_MID|WIN_HLP, 0, 0, 76, api->scrn_len, "Online Help", hbuf, NULL, NULL);
}
