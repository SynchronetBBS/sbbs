/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright 2004 Rob Swindell - http://www.synchro.net/copyright.html		*
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <threadwrap.h>

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "x_cio.h"
#include "console.h"
WORD	x_curr_attr=0x0700;

const int x_tabs[10]={9,17,25,33,41,49,57,65,73,80};

int x_puttext(int sx, int sy, int ex, int ey, void *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;

	gettextinfo(&ti);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ti.screenwidth
			|| sy > ti.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > ti.screenwidth
			|| ey > ti.screenheight
			|| fill==NULL)
		return(0);

	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=*(out++);
			sch |= (*(out++))<<8;
			vmem[y*DpyCols+x]=sch;
		}
	}
	return(1);
}

int x_gettext(int sx, int sy, int ex, int ey, void *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;

	gettextinfo(&ti);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ti.screenwidth
			|| sy > ti.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > ti.screenwidth
			|| ey > ti.screenheight
			|| fill==NULL)
		return(0);

	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=vmem[y*DpyCols+x];
			*(out++)=sch & 0xff;
			*(out++)=sch >> 8;
		}
	}
	return(1);
}

void x_textattr(int attr)
{
	x_curr_attr=attr<<8;
}

int x_kbhit(void)
{
	return(tty_kbhit());
}

void x_delay(long msec)
{
	usleep(msec*1000);
}

int x_wherey(void)
{
	return(CursRow+1);
}

int x_wherex(void)
{
	return(CursCol+1);
}

/* Put the character _c on the screen at the current cursor position. 
 * The special characters return, linefeed, bell, and backspace are handled
 * properly, as is line wrap and scrolling. The cursor position is updated. 
 */
int x_putch(int ch)
{
	struct text_info ti;
	WORD sch;
	int i;

	sch=x_curr_attr|ch;

	switch(ch) {
		case '\r':
			gettextinfo(&ti);
			CursCol=ti.winleft-1;
			break;
		case '\n':
			gettextinfo(&ti);
			if(wherey()==ti.winbottom-ti.wintop+1)
				wscroll();
			else
				CursRow++;
			break;
		case '\b':
			if(CursCol>0)
				CursCol--;
			vmem[CursCol+CursRow*DpyCols]=x_curr_attr|' ';
			break;
		case 7:		/* Bell */
			tty_beep();
			break;
		case '\t':
			for(i=0;i<10;i++) {
				if(x_tabs[i]>wherex()) {
					while(wherex()<x_tabs[i]) {
						putch(' ');
					}
					break;
				}
			}
			if(i==10) {
				putch('\r');
				putch('\n');
			}
			break;
		default:
			gettextinfo(&ti);
			if(wherey()==ti.winbottom-ti.wintop+1
					&& wherex()==ti.winright-ti.winleft+1) {
				vmem[CursCol+CursRow*DpyCols]=sch;
				wscroll();
				gotoxy(ti.winleft,wherey());
			}
			else {
				if(wherex()==ti.winright-ti.winleft+1) {
					vmem[CursCol+CursRow*DpyCols]=sch;
					gotoxy(ti.winleft,ti.cury+1);
				}
				else {
					vmem[CursCol+CursRow*DpyCols]=sch;
					gotoxy(ti.curx+1,ti.cury);
				}
			}
			break;
	}

	return(ch);
}

void x_gotoxy(int x, int y)
{
	CursRow=y-1;
	CursCol=x-1;
}

void x_gettextinfo(struct text_info *info)
{
	info->currmode=CurrMode;
	info->screenheight=DpyRows+1;
	info->screenwidth=DpyCols;
	info->curx=x_wherex();
	info->cury=x_wherey();
	info->attribute=x_curr_attr>>8;
}

void x_setcursortype(int type)
{
	switch(type) {
		case _NOCURSOR:
			CursStart=0xff;
			CursEnd=0;
			break;
		case _SOLIDCURSOR:
			CursStart=0;
			CursEnd=FH-1;
			break;
		default:
		    CursStart = InitCS;
		    CursEnd = InitCE;
			break;
	}
}

int x_getch(void)
{
	return(tty_read(TTYF_BLOCK));
}

int x_getche(void)
{
	int ch;

	if(x_nextchar)
		return(x_getch());
	ch=x_getch();
	if(ch)
		putch(ch);
	return(ch);
}

int x_beep(void)
{
	tty_beep();
	return(0);
}

void x_textmode(int mode)
{
	console_new_mode=mode;
	sem_wait(&console_mode_changed);
}

void x_setname(const char *name)
{
	x_win_name(name);
}

void x_settitle(const char *title)
{
	x_win_title(title);
}

void x_copytext(const char *text, size_t buflen)
{
	pthread_mutex_lock(&copybuf_mutex);
	if(copybuf!=NULL) {
		free(copybuf);
		copybuf=NULL;
	}

	copybuf=(char *)malloc(buflen+1);
	if(copybuf!=NULL) {
		strcpy(copybuf, text);
		sem_post(&copybuf_set);
	}
	pthread_mutex_unlock(&copybuf_mutex);
	return;
}

char *x_getcliptext(void)
{
	char *ret=NULL;

	sem_post(&pastebuf_request);
	sem_wait(&pastebuf_set);
	if(pastebuf!=NULL) {
		ret=(char *)malloc(strlen(pastebuf)+1);
		if(ret!=NULL)
			strcpy(ret,pastebuf);
	}
	sem_post(&pastebuf_request);
	return(ret);
}

int x_setfont(int font, int force)
{
	font_force=force;
	new_font=font;
	sem_wait(&font_set);
	return(setfont_return);
}
