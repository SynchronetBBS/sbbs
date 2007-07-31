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

int x_kbhit(void)
{
	return(tty_kbhit());
}

void x_gotoxy(int x, int y)
{
	CursRow=cio_textinfo.wintop+y-2;
	CursCol=cio_textinfo.winleft+x-2;
	cio_textinfo.curx=x;
	cio_textinfo.cury=y;
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
	if(font==getfont())
		return(0);
	font_force=force;
	new_font=font;
	sem_wait(&font_set);
	return(setfont_return);
}

int x_getfont(void)
{
	return(new_font);
}

int x_loadfont(char *filename)
{
	return(x_load_font(filename));
}

int x_get_window_info(int *width, int *height, int *xpos, int *ypos)
{
	if(width)
		*width=x11_window_width;
	if(height)
		*height=x11_window_height;
	if(xpos)
		*xpos=x11_window_xpos;
	if(ypos)
		*ypos=x11_window_ypos;
	
	return(0);
}
