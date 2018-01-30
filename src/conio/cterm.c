/* $Id$ */

/****************************************************************************
 * @format.tab-size 4		(Plain Text/Source Code File Header)			*
 * @format.use-tabs true	(see http://www.synchro.net/ptsc_hdr.html)		*
 *																			*
 * Copyright Rob Swindell - http://www.synchro.net/copyright.html			*
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
 #include <malloc.h>	/* alloca() on Win32 */
#endif

#include <genwrap.h>
#include <xpbeep.h>
#include <link_list.h>
#ifdef __unix__
	#include <xpsem.h>
#endif
#include <threadwrap.h>

#include "ciolib.h"

#include "cterm.h"

#define	BUFSIZE	2048

const int cterm_tabs[]={1,9,17,25,33,41,49,57,65,73,81,89,97,105,113,121,129,137,145};

const char *octave="C#D#EF#G#A#B";

/* Characters allowed in music strings... if one that is NOT in here is
 * found, CTerm leaves music capture mode and tosses the buffer away */
const char *musicchars="aAbBcCdDeEfFgGlLmMnNoOpPsStT0123456789.-+#<> ";
const uint note_frequency[]={	/* Hz*1000 */
/* Octave 0 (Note 1) */
	 65406
	,69296
	,73416
	,77782
	,82407
	,87307
	,92499
	,97999
	,103820
	,110000
	,116540
	,123470
/* Octave 1 */
	,130810
	,138590
	,146830
	,155560
	,164810
	,174610
	,185000
	,196000
	,207650
	,220000
	,233080
	,246940
/* Octave 2 */
	,261620
	,277180
	,293660
	,311130
	,329630
	,349230
	,369990
	,392000
	,415300
	,440000
	,466160
	,493880
/* Octave 3 */
	,523250
	,554370
	,587330
	,622250
	,659260
	,698460
	,739990
	,783990
	,830610
	,880000
	,932330
	,987770
/* Octave 4 */
	,1046500
	,1108700
	,1174700
	,1244500
	,1318500
	,1396900
	,1480000
	,1568000
	,1661200
	,1760000
	,1864600
	,1975500
/* Octave 5 */
	,2093000
	,2217500
	,2349300
	,2489000
	,2637000
	,2793800
	,2959900
	,3136000
	,3322400
	,3520000
	,3729300
	,3951100
/* Octave 6 */
	,4186000
	,4435000
	,4698600
	,4978000
	,5274000
	,5587000
	,5920000
	,6272000
	,6644000
	,7040000
	,7458600
	,7902200
};

struct note_params {
	int notenum;
	int	notelen;
	int	dotted;
	int	tempo;
	int	noteshape;
	int	foreground;
};

#ifdef CTERM_WITHOUT_CONIO
	#define GOTOXY(x,y)				cterm->ciolib_gotoxy(cterm, x, y)
	#define WHEREX()				cterm->ciolib_wherex(cterm)
	#define WHEREY()				cterm->ciolib_wherey(cterm)
	#define GETTEXT(a,b,c,d,e)		cterm->ciolib_gettext(cterm, a,b,c,d,e)
	#define GETTEXTINFO(a)			cterm->ciolib_gettextinfo(cterm,a)
	#define TEXTATTR(a)				cterm->ciolib_textattr(cterm,a)
	#define SETCURSORTYPE(a)		cterm->ciolib_setcursortype(cterm,a)
	#define MOVETEXT(a,b,c,d,e,f)	cterm->ciolib_movetext(cterm,a,b,c,d,e,f)
	#define CLREOL()				cterm->ciolib_clreol(cterm)
	#define CLRSCR()				cterm->ciolib_clrscr(cterm)
	#define SETVIDEOFLAGS(a)		cterm->ciolib_setvideoflags(cterm,a)
	#define GETVIDEOFLAGS()			cterm->ciolib_getvideoflags(cterm)
	#define PUTCH(a)				cterm->ciolib_putch(cterm,a)
	#define PUTTEXT(a,b,c,d,e)		cterm->ciolib_puttext(cterm,a,b,c,d,e)
	#define WINDOW(a,b,c,d)			cterm->ciolib_window(cterm,a,b,c,d)
	#define CPUTS(a)				cterm->ciolib_cputs(cterm,a)
	#define SETFONT(a,b,c)			cterm->ciolib_setfont(cterm,a,b,c)
#else
	#define GOTOXY(x,y)				cterm->ciolib_gotoxy(x, y)
	#define WHEREX()				cterm->ciolib_wherex()
	#define WHEREY()				cterm->ciolib_wherey()
	#define GETTEXT(a,b,c,d,e)		cterm->ciolib_gettext(a,b,c,d,e)
	#define GETTEXTINFO(a)			cterm->ciolib_gettextinfo(a)
	#define TEXTATTR(a)				cterm->ciolib_textattr(a)
	#define SETCURSORTYPE(a)		cterm->ciolib_setcursortype(a)
	#define MOVETEXT(a,b,c,d,e,f)	cterm->ciolib_movetext(a,b,c,d,e,f)
	#define CLREOL()				cterm->ciolib_clreol()
	#define CLRSCR()				cterm->ciolib_clrscr()
	#define SETVIDEOFLAGS(a)		cterm->ciolib_setvideoflags(a)
	#define GETVIDEOFLAGS()			cterm->ciolib_getvideoflags()
	#define PUTCH(a)				cterm->ciolib_putch(a)
	#define PUTTEXT(a,b,c,d,e)		cterm->ciolib_puttext(a,b,c,d,e)
	#define WINDOW(a,b,c,d)			cterm->ciolib_window(a,b,c,d)
	#define CPUTS(a)				cterm->ciolib_cputs(a)
	#define SETFONT(a,b,c)			cterm->ciolib_setfont(a,b,c)
#endif

#ifdef CTERM_WITHOUT_CONIO
/***************************************************************/
/* These funcions are used when conio is not                   */
/* They are mostly copied from either ciolib.c or bitmap_con.c */
/***************************************************************/
#define BD	((struct buffer_data *)cterm->extra)
#define CTERM_INIT()	while(BD==NULL) ciolib_init(cterm)

struct buffer_data {
	unsigned char	x;
	unsigned char	y;
	unsigned char	winleft;        /* left window coordinate */
	unsigned char	wintop;         /* top window coordinate */
	unsigned char	winright;       /* right window coordinate */
	unsigned char	winbottom;      /* bottom window coordinate */
	unsigned char	attr;    		  /* text attribute */
	unsigned char	currmode;       /* current video mode */
	unsigned char	cursor;
	unsigned char 	*vmem;
	int				vflags;
};

static void ciolib_init(struct cterminal *cterm)
{
	BD=malloc(sizeof(struct buffer_data));
	if(!BD)
		return
	BD->x=1;
	BD->y=1;
	BD->winleft=1;
	BD->wintop=1;
	BD->winright=cterm->width;
	BD->winbottom=cterm->height;
	BD->attr=7;
	BD->currmode=C80;	// TODO: Adjust based on size...
	BD->cursor=_NORMALCURSOR;
	BD->vmem=malloc(2*cterm->height*cterm->width);
	if(BD->vmem==NULL) {
		free(BD)
		return;
	}
	BD->vflags=0;
}

static void ciolib_gotoxy(struct cterminal *cterm,int x,int y)
{
	CTERM_INIT();

	if(x < 1
			|| x > BD->winright-BD->winleft+1
			|| y < 1
			|| y > BD->winbottom-BD->wintop+1)
		return;

	BD->x=x;
	BD->y=y;
}

static int ciolib_wherex(struct cterminal *cterm)
{
	CTERM_INIT();
	return BD->x
}

static int ciolib_wherey(struct cterminal *cterm)
{
	CTERM_INIT();
	return BD->y
}

static int ciolib_gettext(struct cterminal *cterm,int sx, int sy, int ex, int ey, void *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;

	CTERM_INIT();
	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ex
			|| sy > ey
			|| ex > cterm->width
			|| ey > cterm->height
			|| fill==NULL)
		return(0);

	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=BD->vmem[y*cio_textinfo.screenwidth+x];
			*(out++)=sch & 0xff;
			*(out++)=sch >> 8;
		}
	}
	return(1);
}

static void ciolib_gettextinfo(struct cterminal *cterm,struct text_info *ti)
{
	CTERM_INIT();
	ti->winleft=>BD->winleft;
	ti->wintop=BD->wintop;
	ti->winright=BD->winright;
	ti->winbottom=BD->winbottom;
	ti->attribute=BD->attr;
	ti->normattr=7;
	ti->currmode=BD->currmode;
	if (cterm->height > 0xff)
		ti->screenheight = 0xff;
	else
		ti->screenheight = cterm->height;
	if (cterm->width > 0xff)
		ti->screenwidth = 0xff;
	else
		ti->screenwidth = cterm->width;
	ti->curx=BD->x;
	ti->cury=BD->y;
}

static void ciolib_textattr(struct cterminal *cterm,int attr)
{
	CTERM_INIT();
	BD->attr=attr;
}

static void ciolib_setcursortype(struct cterminal *cterm,int cursor)
{
	CTERM_INIT();
	BD->cursor=cursor;
}

static int ciolib_movetext(struct cterminal *cterm, int x, int y, int ex, int ey, int tox, int toy)
{
	int	direction=1;
	int	cy;
	int	sy;
	int	destoffset;
	int	sourcepos;
	int width=ex-x+1;
	int height=ey-y+1;

	CTERM_INIT();
	if(		   x<1
			|| y<1
			|| ex<1
			|| ey<1
			|| tox<1
			|| toy<1
			|| x>cterm->width
			|| ex>cterm->width
			|| tox>cterm->width
			|| y>cterm->height
			|| ey>cterm->height
			|| toy>cterm->height)
		return(0);

	if(toy > y)
		direction=-1;

	sourcepos=(y-1)*cterm->width+(x-1);
	destoffset=(((toy-1)*cterm->width+(tox-1))-sourcepos);

	for(cy=(direction==-1?(height-1):0); cy<height && cy>=0; cy+=direction) {
		sourcepos=((y-1)+cy)*cio_textinfo.screenwidth+(x-1);
		memmove(&(BD->vmem[sourcepos+destoffset]), &(BD->vmem[sourcepos]), sizeof(BD->vmem[0])*width);
	}
	return(1);
}

static void conio_clreol(struct cterminal *cterm)
{
	int pos,x;
	WORD fill=(BD->attr<<8)|space;

	CTERM_INIT();
	pos=(BD->y+BD->wintop-2)*cterm->width;
	for(x=BD->x+BD->winleft-2; x<BD->right; x++)
		BD->vmem[pos+x]=fill;
}

static void ciolib_clrscr(struct cterminal *cterm)
{
	int x,y;
	WORD fill=(BD->attr<<8)|space;

	CTERM_INIT();
	for(y=cio_textinfo.wintop-1; y<cio_textinfo.winbottom; y++) {
		for(x=BD->winleft-1; x<BD->winright; x++)
			BD->vmem[y*cterm->width+x]=fill;
	}
}

static void ciolib_setvideoflags(struct cterminal *cterm, int flags)
{
	CTERM_INIT();
	BD->vflags=flags;
}

static int ciolib_getvideoflags(struct cterminal *cterm)
{
	CTERM_INIT();
	return BD->vflags;
}

static void ciolib_wscroll(struct cterminal *cterm)
{
	int os;

	CTERM_INIT();
	MOVETEXT(BD->winleft
			,BD->wintop+1
			,BD->winright
			,BD->winbottom
			,BD->winleft
			,BD->wintop);
	GOTOXY(cterm, 1,BD->winbottom-BD->wintop+1);
	os=_wscroll;
	_wscroll=0;
	CLREOL(cterm);
	_wscroll=os;
	GOTOXY(cterm, BD->x,BD->y);
}

static int ciolib_putch(struct cterminal *cterm,int a)
{
	unsigned char a1=a;
	unsigned char buf[2];
	int i;

	CTERM_INIT();
	buf[0]=a1;
	buf[1]=BD->attr;

	switch(a1) {
		case '\r':
			GOTOXY(1,cio_textinfo.cury);
			break;
		case '\n':
			if(BD->y==BD->winbottom-BD->wintop+1)
				ciolib_wscroll(cterm);
			else
				GOTOXY(BD->x, BD->y+1);
			break;
		case '\b':
			if(BD->x>1) {
				GOTOXY(BD->x-1,BD->y);
				buf[0]=' ';
				PUTTEXT(BD->x+BD->winleft-1
						,BD->y+BD->wintop-1
						,BD->x+BD->winleft-1
						,BD->y+BD->wintop-1
						,buf);
			}
			break;
		case 7:		/* Bell */
			break;
		case '\t':
			for(i=0;i<(sizeof(cterm_tabs)/sizeof(int));i++) {
				if(cterm_tabs[i]>BD->x) {
					buf[0]=' ';
					while(BD->x<cterm_tabs[i]) {
						PUTTEXT(BD->x+BD->winleft-1
								,BD->y+BD->wintop-1
								,BD->x+BD->winleft-1
								,BD->y+BD->wintop-1
								,buf);
						GOTOXY(BD->x+1,BD->y);
						if(BD->x==cterm->width)
							break;
					}
					break;
				}
			}
			if(BD->x==cterm->width) {
				GOTOXY(1,BD->y);
				if(BD->y==BD->winbottom-BD->wintop+1)
					ciolib_wscroll(cterm);
				else
					GOTOXY(BD->x, BD->y+1);
			}
			break;
		default:
			if(BD->y==BD->winbottom-BD->wintop+1
					&& BD->x==BD->winright-BD->winleft+1) {
				PUTTEXT(WHEREX()+BD->winleft-1
						,WHEREY()+BD->wintop-1
						,WHEREX()+BD->winleft-1
						,WHEREY()+BD->wintop-1
						,buf);
				ciolib_wscroll(cterm);
				GOTOXY(1, BD->winbottom-BD->wintop+1);
			}
			else {
				if(BD->x==BD->winright-BD->winleft+1) {
					PUTTEXT(WHEREX()+BD->winleft-1
							,WHEREY()+BD->wintop-1
							,WHEREX()+BD->winleft-1
							,WHEREY()+BD->wintop-1
							,buf);
					GOTOXY(1,BD->y+1);
				}
				else {
					PUTTEXT(WHEREX()+BD->winleft-1
							,WHEREY()+BD->wintop-1
							,WHEREX()+BD->winleft-1
							,WHEREY()+BD->wintop-1
							,buf);
					GOTOXY(BD->x+1, BD->y);
				}
			}
			break;
	}

	return(a1);
}

static int ciolib_puttext(struct cterminal *cterm,int sx, int sy, int ex, int ey, void *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;

	CTERM_INIT();
	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > cterm->width
			|| sy > cterm->height
			|| sx > ex
			|| sy > ey
			|| ex > cterm->width
			|| ey > cterm->height
			|| fill==NULL)
		return(0);

	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=*(out++);
			sch |= (*(out++))<<8;
			BD->vmem[y*cterm->width+x]=sch;
		}
	}
	return(1);
}

static void ciolib_window(struct cterminal *cterm,int sx, int sy, int ex, int ey)
{
	CTERM_INIT();
	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > cterm->width
			|| sy > cterm->height
			|| sx > ex
			|| sy > ey
			|| ex > cterm->width
			|| ey > cterm->height)
		return;
	BD->winleft=sx;
	BD->wintop=sy;
	BD->winright=ex;
	BD->winbottom=ey;
	GOTOXY(1,1);
}

static int ciolib_cputs(struct cterminal *cterm, char *str)
{
	int		pos;
	int		ret=0;
	int		olddmc;

	CTERM_INIT();
	for(pos=0;str[pos];pos++)
	{
		ret=str[pos];
		if(str[pos]=='\n')
			PUTCH('\r');
		PUTCH(str[pos]);
	}
	GOTOXY(WHEREX(),WHEREY());
	return(ret);
}

static int ciolib_setfont(struct cterminal *,int font, int force, int font_num)
{
	CTERM_INIT();
	return -1;
}
#endif

static void tone_or_beep(double freq, int duration, int device_open)
{
	if(device_open)
		xptone(freq,duration,WAVE_SHAPE_SINE_SAW_HARM);
	else
		xpbeep(freq,duration);
}

static void playnote_thread(void *args)
{
	/* Tempo is quarter notes per minute */
	int duration;
	int pauselen;
	struct note_params *note;
	int	device_open=FALSE;
	struct cterminal *cterm=(struct cterminal *)args;


	SetThreadName("PlayNote");
	cterm->playnote_thread_running=TRUE;
	while(1) {
		if(device_open) {
			if(!listSemTryWaitBlock(&cterm->notes,5000)) {
				xptone_close();
				device_open=FALSE;
				listSemWait(&cterm->notes);
			}
		}
		else
			listSemWait(&cterm->notes);
		device_open=xptone_open();
		note=listShiftNode(&cterm->notes);
		if(note==NULL)
			break;
		if(note->dotted)
			duration=360000/note->tempo;
		else
			duration=240000/note->tempo;
		duration/=note->notelen;
		switch(note->noteshape) {
			case CTERM_MUSIC_STACATTO:
				pauselen=duration/4;
				break;
			case CTERM_MUSIC_LEGATO:
				pauselen=0;
				break;
			case CTERM_MUSIC_NORMAL:
			default:
				pauselen=duration/8;
				break;
		}
		duration-=pauselen;
		if(note->notenum < 72 && note->notenum >= 0)
			tone_or_beep(((double)note_frequency[note->notenum])/1000,duration,device_open);
		else
			tone_or_beep(0,duration,device_open);
		if(pauselen)
			tone_or_beep(0,pauselen,device_open);
		if(note->foreground)
			sem_post(&cterm->note_completed_sem);
		free(note);
	}
	cterm->playnote_thread_running=FALSE;
	sem_post(&cterm->playnote_thread_terminated);
}

static void play_music(struct cterminal *cterm)
{
	int		i;
	char	*p;
	char	*out;
	int		offset;
	char	note;
	int		notelen;
	char	numbuf[10];
	int		dotted;
	int		notenum;
	struct	note_params *np;
	int		fore_count;

	if(cterm->quiet) {
		cterm->music=0;
		cterm->musicbuf[0]=0;
		return;
	}
	p=cterm->musicbuf;
	fore_count=0;
	if(cterm->music==1) {
		switch(toupper(*p)) {
			case 'F':
				cterm->musicfore=TRUE;
				p++;
				break;
			case 'B':
				cterm->musicfore=FALSE;
				p++;
				break;
			case 'N':
				if(!isdigit(*(p+1))) {
					cterm->noteshape=CTERM_MUSIC_NORMAL;
					p++;
				}
				break;
			case 'L':
				cterm->noteshape=CTERM_MUSIC_LEGATO;
				p++;
				break;
			case 'S':
				cterm->noteshape=CTERM_MUSIC_STACATTO;
				p++;
				break;
		}
	}
	for(;*p;p++) {
		notenum=0;
		offset=0;
		switch(toupper(*p)) {
			case 'M':
				p++;
				switch(toupper(*p)) {
					case 'F':
						cterm->musicfore=TRUE;
						break;
					case 'B':
						cterm->musicfore=FALSE;
						break;
					case 'N':
						cterm->noteshape=CTERM_MUSIC_NORMAL;
						break;
					case 'L':
						cterm->noteshape=CTERM_MUSIC_LEGATO;
						break;
					case 'S':
						cterm->noteshape=CTERM_MUSIC_STACATTO;
						break;
					default:
						p--;
				}
				break;
			case 'T':						/* Tempo */
				out=numbuf;
				while(isdigit(*(p+1)))
					*(out++)=*(++p);
				*out=0;
				cterm->tempo=strtoul(numbuf,NULL,10);
				if(cterm->tempo>255)
					cterm->tempo=255;
				if(cterm->tempo<32)
					cterm->tempo=32;
				break;
			case 'O':						/* Octave */
				out=numbuf;
				while(isdigit(*(p+1)))
					*(out++)=*(++p);
				*out=0;
				cterm->octave=strtoul(numbuf,NULL,10);
				if(cterm->octave>6)
					cterm->octave=6;
				break;
			case 'L':
				out=numbuf;
				while(isdigit(*(p+1)))
					*(out++)=*(++p);
				*out=0;
				cterm->notelen=strtoul(numbuf,NULL,10);
				if(cterm->notelen<1)
					cterm->notelen=1;
				if(cterm->notelen>64)
					cterm->notelen=64;
				break;
			case 'N':						/* Note by number */
				if(isdigit(*(p+1))) {
					out=numbuf;
					while(isdigit(*(p+1))) {
						*(out++)=*(p+1);
						p++;
					}
					*out=0;
					notenum=strtoul(numbuf,NULL,10);
				}
				if(notenum==0) {
					notenum=-1;
					offset=1;
				}
				/* Fall-through */
			case 'A':						/* Notes in current octave by letter */
			case 'B':
			case 'C':
			case 'D':
			case 'E':
			case 'F':
			case 'G':
			case 'P':
				note=toupper(*p);
				notelen=cterm->notelen;
				dotted=0;
				i=1;
				while(i) {
					i=0;
					if(*(p+1)=='+' || *(p+1)=='#') {	/* SHARP */
						offset+=1;
						p++;
						i=1;
					}
					if(*(p+1)=='-') {					/* FLAT */
						offset-=1;
						p++;
						i=1;
					}
					if(*(p+1)=='.') {					/* Dotted note (1.5*notelen) */
						dotted=1;
						p++;
						i=1;
					}
					if(isdigit(*(p+1))) {
						out=numbuf;
						while(isdigit(*(p+1))) {
							*(out++)=*(p+1);
							p++;
						}
						*out=0;
						notelen=strtoul(numbuf,NULL,10);
						i=1;
					}
				}
				if(note=='P') {
					notenum=-1;
					offset=0;
				}
				if(notenum==0) {
					out=strchr(octave,note);
					if(out==NULL) {
						notenum=-1;
						offset=1;
					}
					else {
						notenum=cterm->octave*12+1;
						notenum+=(out-octave);
					}
				}
				notenum+=offset;
				np=(struct note_params *)malloc(sizeof(struct note_params));
				if(np!=NULL) {
					np->notenum=notenum;
					np->notelen=notelen;
					np->dotted=dotted;
					np->tempo=cterm->tempo;
					np->noteshape=cterm->noteshape;
					np->foreground=cterm->musicfore;
					listPushNode(&cterm->notes, np);
					if(cterm->musicfore)
						fore_count++;
				}
				break;
			case '<':							/* Down one octave */
				cterm->octave--;
				if(cterm->octave<0)
					cterm->octave=0;
				break;
			case '>':							/* Up one octave */
				cterm->octave++;
				if(cterm->octave>6)
					cterm->octave=6;
				break;
		}
	}
	cterm->music=0;
	cterm->musicbuf[0]=0;
	while(fore_count) {
		sem_wait(&cterm->note_completed_sem);
		fore_count--;
	}
	xptone_complete();
}

static void scrolldown(struct cterminal *cterm)
{
	int top = cterm->y+cterm->top_margin-1;
	int height = cterm->bottom_margin;
	int x,y;

	MOVETEXT(cterm->x,top,cterm->x+cterm->width-1,top+height-2,cterm->x,top+1);
	x=WHEREX();
	y=WHEREY();
	GOTOXY(1,top);
	CLREOL();
	GOTOXY(x,y);
}

static void scrollup(struct cterminal *cterm)
{
	int top = cterm->y+cterm->top_margin-1;
	int height = cterm->bottom_margin-cterm->top_margin+1;
	int x,y;

	cterm->backpos++;
	if(cterm->scrollback!=NULL) {
		if(cterm->backpos>cterm->backlines) {
			memmove(cterm->scrollback,cterm->scrollback+cterm->width*2,cterm->width*2*(cterm->backlines-1));
			cterm->backpos--;
		}
		GETTEXT(cterm->x, top, cterm->x+cterm->width-1, top, cterm->scrollback+(cterm->backpos-1)*cterm->width*2);
	}
	MOVETEXT(cterm->x,top+1,cterm->x+cterm->width-1,top+height-1,cterm->x,top);
	x=WHEREX();
	y=WHEREY();
	GOTOXY(1,top+height-1);
	CLREOL();
	GOTOXY(x,y);
}

static void dellines(struct cterminal * cterm, int lines)
{
	int top = cterm->y+cterm->top_margin-1;
	int height = cterm->bottom_margin-cterm->top_margin+1;
	int i;
	int linestomove;
	int x,y;

	if(lines<1)
		return;
	linestomove=cterm->height-WHEREY();
	MOVETEXT(cterm->x,top+WHEREY()-1+lines,cterm->x+cterm->width-1,top+height-1,cterm->x,top+WHEREY()-1);
	x=WHEREX();
	y=WHEREY();
	for(i=height-lines+1; i<=height; i++) {
		GOTOXY(1,i);
		CLREOL();
	}
	GOTOXY(x,y);
}

static void clear2bol(struct cterminal * cterm)
{
	char *buf;
	int i,j,k;

	k=WHEREX();
	buf=(char *)malloc(k*2);
	j=0;
	for(i=0;i<k;i++) {
		if(cterm->emulation == CTERM_EMULATION_ATASCII)
			buf[j++]=0;
		else
			buf[j++]=' ';
		buf[j++]=cterm->attr;
	}
	PUTTEXT(cterm->x,cterm->y+WHEREY()-1,cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,buf);
	free(buf);
}

void CIOLIBCALL cterm_clearscreen(struct cterminal *cterm, char attr)
{
	if(!cterm->started)
		cterm_start(cterm);

	if(cterm->scrollback!=NULL) {
		cterm->backpos+=cterm->height;
		if(cterm->backpos>cterm->backlines) {
			memmove(cterm->scrollback,cterm->scrollback+cterm->width*2*(cterm->backpos-cterm->backlines),cterm->width*2*(cterm->backlines-(cterm->backpos-cterm->backlines)));
			cterm->backpos=cterm->backlines;
		}
		GETTEXT(cterm->x,cterm->y,cterm->x+cterm->width-1,cterm->y+cterm->height-1,cterm->scrollback+(cterm->backpos-cterm->height)*cterm->width*2);
	}
	CLRSCR();
	if(cterm->origin_mode)
		GOTOXY(1,cterm->top_margin);
	else
		GOTOXY(1,1);
}

static void do_ansi(struct cterminal *cterm, char *retbuf, size_t retsize, int *speed)
{
	char	*p;
	char	*p2;
	char	tmp[1024];
	int		i,j,k,l;
	int		row,col;
	int		max_row;
	struct text_info ti;

	switch(cterm->escbuf[0]) {
		case '[':
			/* ANSI stuff */
			p=cterm->escbuf+strlen(cterm->escbuf)-1;
			if(cterm->escbuf[1]>=60 && cterm->escbuf[1] <= 63) {	/* Private extensions */
				switch(*p) {
					case 'M':
						if(cterm->escbuf[1] == '=') {	/* ANSI Music setup */
							i=strtoul(cterm->escbuf+2,NULL,10);
							switch(i) {
								case 1:					/* BANSI (ESC[N) music only) */
									cterm->music_enable=CTERM_MUSIC_BANSI;
									break;
								case 2:					/* ESC[M ANSI music */
									cterm->music_enable=CTERM_MUSIC_ENABLED;
									break;
								default:					/* Disable ANSI Music */
									cterm->music_enable=CTERM_MUSIC_SYNCTERM;
									break;
							}
						}
						break;
					case 'h':
						if(!strcmp(cterm->escbuf,"[?6h")) {
							cterm->origin_mode=true;
							GOTOXY(1,cterm->top_margin);
						}
						if(!strcmp(cterm->escbuf,"[?7h")) {
							cterm->autowrap=true;
						}
						if(!strcmp(cterm->escbuf,"[?25h")) {
							cterm->cursor=_NORMALCURSOR;
							SETCURSORTYPE(cterm->cursor);
						}
						if(!strcmp(cterm->escbuf,"[?31h")) {
							i=GETVIDEOFLAGS();
							i|=CIOLIB_VIDEO_ALTCHARS;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[?32h")) {
							i=GETVIDEOFLAGS();
							i|=CIOLIB_VIDEO_NOBRIGHT;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[?33h")) {
							i=GETVIDEOFLAGS();
							i|=CIOLIB_VIDEO_BGBRIGHT;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[?34h")) {
							i=GETVIDEOFLAGS();
							i|=CIOLIB_VIDEO_BLINKALTCHARS;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[?35h")) {
							i=GETVIDEOFLAGS();
							i|=CIOLIB_VIDEO_NOBLINK;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[=255h"))
							cterm->doorway_mode=1;
						break;
					case 'l':
						if(!strcmp(cterm->escbuf,"[?6l")) {
							cterm->origin_mode=false;
							GOTOXY(1,1);
						}
						if(!strcmp(cterm->escbuf,"[?7l")) {
							cterm->autowrap=false;
						}
						if(!strcmp(cterm->escbuf,"[?25l")) {
							cterm->cursor=_NOCURSOR;
							SETCURSORTYPE(cterm->cursor);
						}
						if(!strcmp(cterm->escbuf,"[?31l")) {
							i=GETVIDEOFLAGS();
							i&=~CIOLIB_VIDEO_ALTCHARS;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[?32l")) {
							i=GETVIDEOFLAGS();
							i&=~CIOLIB_VIDEO_NOBRIGHT;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[?33l")) {
							i=GETVIDEOFLAGS();
							i&=~CIOLIB_VIDEO_BGBRIGHT;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[?34l")) {
							i=GETVIDEOFLAGS();
							i&=~CIOLIB_VIDEO_BLINKALTCHARS;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[?35l")) {
							i=GETVIDEOFLAGS();
							i&=~CIOLIB_VIDEO_NOBLINK;
							SETVIDEOFLAGS(i);
						}
						if(!strcmp(cterm->escbuf,"[=255l"))
							cterm->doorway_mode=0;
						break;
					case 'n':	/* Query (extended) state information */
						if(retbuf == NULL)
							break;
						tmp[0] = 0;
						if ((strcmp(cterm->escbuf,"[=n") == 0) || (strcmp(cterm->escbuf,"[=1n"))) {
							sprintf(tmp, "\x1b[=1;%u;%u;%u;%u;%u;%un"
								,CONIO_FIRST_FREE_FONT
								,(uint8_t)cterm->setfont_result
								,(uint8_t)cterm->altfont[0]
								,(uint8_t)cterm->altfont[1]
								,(uint8_t)cterm->altfont[2]
								,(uint8_t)cterm->altfont[3]
							);
						}
						if (!strcmp(cterm->escbuf,"[=2n")) {
							int vidflags = GETVIDEOFLAGS();
							strcpy(tmp, "\x1b[=2");
							if(cterm->origin_mode)
								strcat(tmp, ";6");
							if(cterm->autowrap)
								strcat(tmp, ";7");
							if(cterm->cursor == _NORMALCURSOR)
								strcat(tmp, ";25");
							if(vidflags & CIOLIB_VIDEO_ALTCHARS)
								strcat(tmp, ";31");
							if(vidflags & CIOLIB_VIDEO_NOBRIGHT)
								strcat(tmp, ";32");
							if(vidflags & CIOLIB_VIDEO_BGBRIGHT)
								strcat(tmp, ";33");
							if(vidflags & CIOLIB_VIDEO_BLINKALTCHARS)
								strcat(tmp, ";34");
							if(vidflags & CIOLIB_VIDEO_NOBLINK)
								strcat(tmp, ";35");
							if (strlen(tmp) == 4) {	// Nothing set
								strcat(tmp, ";");
							}
							strcat(tmp, "n");
							break;
						}
						if(*tmp && strlen(retbuf) + strlen(tmp) < retsize)
							strcat(retbuf, tmp);
						break;
					case 's':
						if(cterm->escbuf[1] == '?') {
							*p=0;
							p2=cterm->escbuf+2;
							GETTEXTINFO(&ti);
							i=GETVIDEOFLAGS();
							if(!*p2) {
								/* All the save stuff... */
								cterm->saved_mode_mask |= (CTERM_SAVEMODE_AUTOWRAP|CTERM_SAVEMODE_CURSOR|CTERM_SAVEMODE_ALTCHARS|CTERM_SAVEMODE_NOBRIGHT|CTERM_SAVEMODE_BGBRIGHT|CTERM_SAVEMODE_ORIGIN);
								cterm->saved_mode &= ~(CTERM_SAVEMODE_AUTOWRAP|CTERM_SAVEMODE_CURSOR|CTERM_SAVEMODE_ALTCHARS|CTERM_SAVEMODE_NOBRIGHT|CTERM_SAVEMODE_BGBRIGHT|CTERM_SAVEMODE_ORIGIN);
								cterm->saved_mode |= (cterm->autowrap)?CTERM_SAVEMODE_AUTOWRAP:0;
								cterm->saved_mode |= (cterm->cursor==_NORMALCURSOR)?CTERM_SAVEMODE_CURSOR:0;
								cterm->saved_mode |= (i&CIOLIB_VIDEO_ALTCHARS)?CTERM_SAVEMODE_ALTCHARS:0;
								cterm->saved_mode |= (i&CIOLIB_VIDEO_NOBRIGHT)?CTERM_SAVEMODE_NOBRIGHT:0;
								cterm->saved_mode |= (i&CIOLIB_VIDEO_BGBRIGHT)?CTERM_SAVEMODE_BGBRIGHT:0;
								cterm->saved_mode |= (i&CIOLIB_VIDEO_BLINKALTCHARS)?CTERM_SAVEMODE_BLINKALTCHARS:0;
								cterm->saved_mode |= (i&CIOLIB_VIDEO_NOBLINK)?CTERM_SAVEMODE_NOBLINK:0;
								cterm->saved_mode |= (cterm->origin_mode)?CTERM_SAVEMODE_ORIGIN:0;
								break;
							}
							while((p=strtok(p2,";"))!=NULL) {
								p2=NULL;
								if(!strcmp(p,"6")) {
									cterm->saved_mode_mask |= CTERM_SAVEMODE_ORIGIN;
									cterm->saved_mode &= ~(CTERM_SAVEMODE_AUTOWRAP|CTERM_SAVEMODE_CURSOR|CTERM_SAVEMODE_ALTCHARS|CTERM_SAVEMODE_NOBRIGHT|CTERM_SAVEMODE_BGBRIGHT|CTERM_SAVEMODE_ORIGIN);
									cterm->saved_mode |= cterm->origin_mode?CTERM_SAVEMODE_ORIGIN:0;
								}
								if(!strcmp(p,"7")) {
									cterm->saved_mode_mask |= CTERM_SAVEMODE_AUTOWRAP;
									cterm->saved_mode &= ~(CTERM_SAVEMODE_AUTOWRAP|CTERM_SAVEMODE_CURSOR|CTERM_SAVEMODE_ALTCHARS|CTERM_SAVEMODE_NOBRIGHT|CTERM_SAVEMODE_BGBRIGHT);
									cterm->saved_mode |= cterm->autowrap?CTERM_SAVEMODE_AUTOWRAP:0;
								}
								if(!strcmp(p,"25")) {
									cterm->saved_mode_mask |= CTERM_SAVEMODE_CURSOR;
									cterm->saved_mode &= ~(CTERM_SAVEMODE_CURSOR);
									cterm->saved_mode |= (cterm->cursor==_NORMALCURSOR)?CTERM_SAVEMODE_CURSOR:0;
								}
								if(!strcmp(p,"31")) {
									cterm->saved_mode_mask |= CTERM_SAVEMODE_ALTCHARS;
									cterm->saved_mode &= ~(CTERM_SAVEMODE_ALTCHARS);
									cterm->saved_mode |= (i&CIOLIB_VIDEO_ALTCHARS)?CTERM_SAVEMODE_ALTCHARS:0;
								}
								if(!strcmp(p,"32")) {
									cterm->saved_mode_mask |= CTERM_SAVEMODE_NOBRIGHT;
									cterm->saved_mode &= ~(CTERM_SAVEMODE_NOBRIGHT);
									cterm->saved_mode |= (i&CIOLIB_VIDEO_NOBRIGHT)?CTERM_SAVEMODE_NOBRIGHT:0;
								}
								if(!strcmp(p,"33")) {
									cterm->saved_mode_mask |= CTERM_SAVEMODE_BGBRIGHT;
									cterm->saved_mode &= ~(CTERM_SAVEMODE_BGBRIGHT);
									cterm->saved_mode |= (i&CIOLIB_VIDEO_BGBRIGHT)?CTERM_SAVEMODE_BGBRIGHT:0;
								}
								if(!strcmp(p,"34")) {
									cterm->saved_mode_mask |= CTERM_SAVEMODE_BLINKALTCHARS;
									cterm->saved_mode &= ~(CTERM_SAVEMODE_BLINKALTCHARS);
									cterm->saved_mode |= (i&CIOLIB_VIDEO_BLINKALTCHARS)?CTERM_SAVEMODE_BLINKALTCHARS:0;
								}
								if(!strcmp(p,"35")) {
									cterm->saved_mode_mask |= CTERM_SAVEMODE_NOBLINK;
									cterm->saved_mode &= ~(CTERM_SAVEMODE_NOBLINK);
									cterm->saved_mode |= (i&CIOLIB_VIDEO_NOBLINK)?CTERM_SAVEMODE_NOBLINK:0;
								}
							}
						}
						break;
					case 'u':
						if(cterm->escbuf[1] == '?') {
							*p=0;
							p2=cterm->escbuf+2;
							GETTEXTINFO(&ti);
							i=GETVIDEOFLAGS();
							if(!*p2) {
								/* All the save stuff... */
								if(cterm->saved_mode_mask & CTERM_SAVEMODE_ORIGIN)
									cterm->origin_mode=(cterm->saved_mode & CTERM_SAVEMODE_ORIGIN) ? true : false;
								if(cterm->saved_mode_mask & CTERM_SAVEMODE_AUTOWRAP)
									cterm->autowrap=(cterm->saved_mode & CTERM_SAVEMODE_AUTOWRAP) ? true : false;
								if(cterm->saved_mode_mask & CTERM_SAVEMODE_CURSOR) {
									cterm->cursor = (cterm->saved_mode & CTERM_SAVEMODE_CURSOR) ? _NORMALCURSOR : _NOCURSOR;
									SETCURSORTYPE(cterm->cursor);
								}
								if(cterm->saved_mode_mask & CTERM_SAVEMODE_ALTCHARS) {
									if(cterm->saved_mode & CTERM_SAVEMODE_ALTCHARS)
										i |= CIOLIB_VIDEO_ALTCHARS;
									else
										i &= ~CIOLIB_VIDEO_ALTCHARS;
								}
								if(cterm->saved_mode_mask & CTERM_SAVEMODE_BLINKALTCHARS) {
									if(cterm->saved_mode & CTERM_SAVEMODE_BLINKALTCHARS)
										i |= CIOLIB_VIDEO_BLINKALTCHARS;
									else
										i &= ~CIOLIB_VIDEO_BLINKALTCHARS;
								}
								if(cterm->saved_mode_mask & CTERM_SAVEMODE_NOBRIGHT) {
									if(cterm->saved_mode & CTERM_SAVEMODE_NOBRIGHT)
										i |= CIOLIB_VIDEO_NOBRIGHT;
									else
										i &= ~CIOLIB_VIDEO_NOBRIGHT;
								}
								if(cterm->saved_mode_mask & CTERM_SAVEMODE_NOBLINK) {
									if(cterm->saved_mode & CTERM_SAVEMODE_NOBLINK)
										i |= CIOLIB_VIDEO_NOBLINK;
									else
										i &= ~CIOLIB_VIDEO_NOBLINK;
								}
								if(cterm->saved_mode_mask & CTERM_SAVEMODE_BGBRIGHT) {
									if(cterm->saved_mode & CTERM_SAVEMODE_BGBRIGHT)
										i |= CIOLIB_VIDEO_BGBRIGHT;
									else
										i &= ~CIOLIB_VIDEO_BGBRIGHT;
								}
								SETVIDEOFLAGS(i);
								break;
							}
							while((p=strtok(p2,";"))!=NULL) {
								p2=NULL;
								if(!strcmp(p,"6")) {
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_ORIGIN)
										cterm->origin_mode=(cterm->saved_mode & CTERM_SAVEMODE_ORIGIN) ? true : false;
								}
								if(!strcmp(p,"7")) {
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_AUTOWRAP)
										cterm->autowrap=(cterm->saved_mode & CTERM_SAVEMODE_AUTOWRAP) ? true : false;
								}
								if(!strcmp(p,"25")) {
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_CURSOR) {
										cterm->cursor = (cterm->saved_mode & CTERM_SAVEMODE_CURSOR) ? _NORMALCURSOR : _NOCURSOR;
										SETCURSORTYPE(cterm->cursor);
									}
								}
								if(!strcmp(p,"31")) {
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_ALTCHARS) {
										if(cterm->saved_mode & CTERM_SAVEMODE_ALTCHARS)
											i |= CIOLIB_VIDEO_ALTCHARS;
										else
											i &= ~CIOLIB_VIDEO_ALTCHARS;
										SETVIDEOFLAGS(i);
									}
								}
								if(!strcmp(p,"32")) {
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_NOBRIGHT) {
										if(cterm->saved_mode & CTERM_SAVEMODE_NOBRIGHT)
											i |= CIOLIB_VIDEO_NOBRIGHT;
										else
											i &= ~CIOLIB_VIDEO_NOBRIGHT;
										SETVIDEOFLAGS(i);
									}
								}
								if(!strcmp(p,"33")) {
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_BGBRIGHT) {
										if(cterm->saved_mode & CTERM_SAVEMODE_BGBRIGHT)
											i |= CIOLIB_VIDEO_BGBRIGHT;
										else
											i &= ~CIOLIB_VIDEO_BGBRIGHT;
										SETVIDEOFLAGS(i);
									}
								}
								if(!strcmp(p,"34")) {
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_BLINKALTCHARS) {
										if(cterm->saved_mode & CTERM_SAVEMODE_BLINKALTCHARS)
											i |= CIOLIB_VIDEO_BLINKALTCHARS;
										else
											i &= ~CIOLIB_VIDEO_BLINKALTCHARS;
										SETVIDEOFLAGS(i);
									}
								}
								if(!strcmp(p,"35")) {
									if(cterm->saved_mode_mask & CTERM_SAVEMODE_NOBLINK) {
										if(cterm->saved_mode & CTERM_SAVEMODE_NOBLINK)
											i |= CIOLIB_VIDEO_NOBLINK;
										else
											i &= ~CIOLIB_VIDEO_NOBLINK;
										SETVIDEOFLAGS(i);
									}
								}
							}
						}
						break;
					case '{':
						if(cterm->escbuf[1] == '=') {	/* Font loading */
							i=255;
							j=0;
							if(strlen(cterm->escbuf)>2) {
								if((p=strtok(cterm->escbuf+2,";"))!=NULL) {
									i=strtoul(p,NULL,10);
									if(!i && cterm->escbuf[2] != '0')
										i=255;
									if((p=strtok(NULL,";"))!=NULL) {
										j=strtoul(p,NULL,10);
									}
								}
							}
							if(i>255)
								break;
							cterm->font_read=0;
							cterm->font_slot=i;
							switch(j) {
								case 0:
									cterm->font_size=4096;
									break;
								case 1:
									cterm->font_size=3586;
									break;
								case 2:
									cterm->font_size=2048;
									break;
								default:
									cterm->font_size=0;
									break;
							}
						}
						break;
				}
				break;
			}
			switch(*p) {
				case '@':	/* Insert Char */
					i=WHEREX();
					j=WHEREY();
					k=strtoul(cterm->escbuf+1,NULL,10);
					if(k<1)
						k=1;
					if(k>cterm->width - j)
						k=cterm->width - j;
					MOVETEXT(cterm->x+i-1,cterm->y+j-1,cterm->x+cterm->width-1-k,cterm->y+j-1,cterm->x+i-1+k,cterm->y+j-1);
					for(l=0; l< k; l++)
						PUTCH(' ');
					GOTOXY(i,j);
					break;
				case 'A':	/* Cursor Up */
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0)
						i=1;
					i=WHEREY()-i;
					if(i<cterm->top_margin)
						i=cterm->top_margin;
					GOTOXY(WHEREX(),i);
					break;
				case 'B':	/* Cursor Down */
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0)
						i=1;
					i=WHEREY()+i;
					if(i>cterm->bottom_margin)
						i=cterm->bottom_margin;
					GOTOXY(WHEREX(),i);
					break;
				case 'C':	/* Cursor Right */
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0)
						i=1;
					i=WHEREX()+i;
					if(i>cterm->width)
						i=cterm->width;
					GOTOXY(i,WHEREY());
					break;
				case 'D':	/* Cursor Left and Font Select */
					if(*(p-1)==' ') {	/* Font Select */
						i=0;
						j=0;
						if(strlen(cterm->escbuf)>2) {
							if((p=strtok(cterm->escbuf+1,";"))!=NULL) {
								i=strtoul(p,NULL,10);
								if((p=strtok(NULL,";"))!=NULL) {
									j=strtoul(p,NULL,10);
								}
							}
							switch(i) {
								case 0:	/* Four fonts are currently supported */
								case 1:
								case 2:
								case 3:
									cterm->setfont_result = SETFONT(j,FALSE,i+1);
									if(cterm->setfont_result == CIOLIB_SETFONT_SUCCESS)
										cterm->altfont[i] = j;
									break;
							}
						}
					}
					else {
						i=strtoul(cterm->escbuf+1,NULL,10);
						if(i==0)
							i=1;
						i=WHEREX()-i;
						if(i<1)
							i=1;
						GOTOXY(i,WHEREY());
					}
					break;
				case 'E':	/* Cursor next line */
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0)
						i=1;
					i=WHEREY()+i;
					if(i>cterm->bottom_margin)
						i=cterm->bottom_margin;
					GOTOXY(1,i);
					break;
				case 'F':	/* Cursor preceding line */
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0)
						i=1;
					i=WHEREY()-i;
					if(i<cterm->top_margin)
						i=cterm->top_margin;
					GOTOXY(1,i);
					break;
				case 'G':
					col=strtoul(cterm->escbuf+1,NULL,10);
					if(col<1)
						col=1;
					if(col>cterm->width)
						col=cterm->width;
					GOTOXY(col,WHEREY());
					break;
				case 'f':
				case 'H':
					row=1;
					max_row = cterm->height;
					if(cterm->origin_mode)
						max_row = cterm->bottom_margin - cterm->top_margin + 1;
					col=1;
					*p=0;
					if(strlen(cterm->escbuf)>1) {	// Remember, we truncated the 'H' or 'f'
						if((p=strtok(cterm->escbuf+1,";"))!=NULL) {
							row=strtoul(p,NULL,10);
							if((p=strtok(NULL,";"))!=NULL) {
								col=strtoul(p,NULL,10);
							}
						}
					}
					if(row<1)
						row=1;
					if(col<1)
						col=1;
					if(row>max_row)
						row=max_row;
					if(cterm->origin_mode)
						row += cterm->top_margin - 1;
					if(col>cterm->width)
						col=cterm->width;
					GOTOXY(col,row);
					break;
				case 'J':
					i=strtoul(cterm->escbuf+1,NULL,10);
					switch(i) {
						case 0:
							CLREOL();
							row=WHEREY();
							col=WHEREX();
							for(i=row+1;i<=cterm->height;i++) {
								GOTOXY(1,i);
								CLREOL();
							}
							GOTOXY(col,row);
							break;
						case 1:
							clear2bol(cterm);
							row=WHEREY();
							col=WHEREX();
							for(i=row-1;i>=1;i--) {
								GOTOXY(1,i);
								CLREOL();
							}
							GOTOXY(col,row);
							break;
						case 2:
							cterm_clearscreen(cterm, (char)cterm->attr);
							GOTOXY(1,1);
							break;
					}
					break;
				case 'K':
					i=strtoul(cterm->escbuf+1,NULL,10);
					switch(i) {
						case 0:
							CLREOL();
							break;
						case 1:
							clear2bol(cterm);
							break;
						case 2:
							row=WHEREY();
							col=WHEREX();
							GOTOXY(1,row);
							CLREOL();
							GOTOXY(col,row);
							break;
					}
					break;
				case 'L':		/* Insert line */
					row=WHEREY();
					col=WHEREX();
					if(row < cterm->top_margin || row > cterm->bottom_margin)
						break;
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0)
						i=1;
					if(i>cterm->bottom_margin-row)
						i=cterm->bottom_margin-row;
					if(i)
						MOVETEXT(cterm->x,cterm->y+row-1,cterm->x+cterm->width-1,cterm->y+cterm->bottom_margin-1-i,cterm->x,cterm->y+row-1+i);
					for(j=0;j<i;j++) {
						GOTOXY(1,row+j);
						CLREOL();
					}
					GOTOXY(col,row);
					break;
				case 'M':	/* ANSI music and also supposed to be delete line! */
					if(cterm->music_enable==CTERM_MUSIC_ENABLED) {
						cterm->music=1;
					}
					else {
						row=WHEREY();
						if(row >= cterm->top_margin && row <= cterm->bottom_margin) {
							i=strtoul(cterm->escbuf+1,NULL,10);
							if(i<1)
								i=1;
							dellines(cterm, i);
						}
					}
					break;
				case 'N':
					/* BananANSI style... does NOT start with MF or MB */
					/* This still conflicts (ANSI erase field) */
					if(cterm->music_enable >= CTERM_MUSIC_BANSI)
						cterm->music=2;
					break;
				case 'P':	/* Delete char */
					row=WHEREY();
					col=WHEREX();

					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0)
						i=1;
					if(i>cterm->width-col+1)
						i=cterm->width-col+1;
					MOVETEXT(cterm->x+col-1+i,cterm->y+row-1,cterm->x+cterm->width-1,cterm->y+row-1,cterm->x+col-1,cterm->y+row-1);
					GOTOXY(cterm->width-i,row);
					CLREOL();
					GOTOXY(col,row);
					break;
				case 'S':
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0 && cterm->escbuf[1] != '0')
						i=1;
					for(j=0; j<i; j++)
						scrollup(cterm);
					break;
				case 'T':
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0 && cterm->escbuf[1] != '0')
						i=1;
					for(j=0; j<i; j++)
						scrolldown(cterm);
					break;
#if 0
				case 'U':
					GETTEXTINFO(&ti);
					cterm_clearscreen(cterm, ti.normattr);
					if(cterm->origin_mode)
						GOTOXY(1,cterm->top_margin);
					else
						GOTOXY(1,1);
					break;
#endif
				case 'X':
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i<1)
						i=1;
					if(i>cterm->width-WHEREX())
						i=cterm->width-WHEREX();
					p2=malloc(i*2);
					j=0;
					for(k=0;k<i;k++) {
						p2[j++]=' ';
						p2[j++]=cterm->attr;
					}
					PUTTEXT(cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,cterm->x+WHEREX()-1+i-1,cterm->y+WHEREY()-1,p2);
					free(p2);
					break;
				case 'Z':
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(i==0 && cterm->escbuf[0] != '0')
						i=1;
					for(j=(sizeof(cterm_tabs)/sizeof(cterm_tabs[0]))-1;j>=0;j--) {
						if(cterm_tabs[j]<WHEREX()) {
							k=j-i+1;
							if(k<0)
								k=0;
							GOTOXY(cterm_tabs[k],WHEREY());
							break;
						}
					}
					break;
				case 'b':	/* ToDo?  Banana ANSI */
					break;
				case 'c':	/* Device Attributes */
					i=strtoul(cterm->escbuf+1,NULL,10);
					if(!i) {
						if(retbuf!=NULL) {
							if(strlen(retbuf) + strlen(cterm->DA)  < retsize)
								strcat(retbuf,cterm->DA);
						}
					}
					break;
				case 'g':	/* ToDo?  VT100 Tabs */
					break;
				case 'i':	/* ToDo?  Printing */
					break;
				case 'm':
					*(p--)=0;
					p2=cterm->escbuf+1;
					GETTEXTINFO(&ti);
					if(p2>p) {
						cterm->attr=ti.normattr;
						TEXTATTR(cterm->attr);
						break;
					}
					while((p=strtok(p2,";"))!=NULL) {
						p2=NULL;
						switch(strtoul(p,NULL,10)) {
							case 0:
								cterm->attr=ti.normattr;
								break;
							case 1:
								cterm->attr|=8;
								break;
							case 2:
								cterm->attr&=247;
								break;
							case 4:	/* Underscore */
								break;
							case 5:
							case 6:
								cterm->attr|=128;
								break;
							case 7:
							case 27:
								i=cterm->attr&7;
								j=cterm->attr&112;
								cterm->attr &= 136;
								cterm->attr |= j>>4;
								cterm->attr |= i<<4;
								break;
							case 8:
								j=cterm->attr&112;
								cterm->attr&=112;
								cterm->attr |= j>>4;
								break;
							case 22:
								cterm->attr &= 0xf7;
								break;
							case 25:
								cterm->attr &= 0x7f;
								break;
							case 30:
								cterm->attr&=248;
								break;
							case 31:
								cterm->attr&=248;
								cterm->attr|=4;
								break;
							case 32:
								cterm->attr&=248;
								cterm->attr|=2;
								break;
							case 33:
								cterm->attr&=248;
								cterm->attr|=6;
								break;
							case 34:
								cterm->attr&=248;
								cterm->attr|=1;
								break;
							case 35:
								cterm->attr&=248;
								cterm->attr|=5;
								break;
							case 36:
								cterm->attr&=248;
								cterm->attr|=3;
								break;
							case 37:
							case 39:
								cterm->attr&=248;
								cterm->attr|=7;
								break;
							case 49:
							case 40:
								cterm->attr&=143;
								break;
							case 41:
								cterm->attr&=143;
								cterm->attr|=4<<4;
								break;
							case 42:
								cterm->attr&=143;
								cterm->attr|=2<<4;
								break;
							case 43:
								cterm->attr&=143;
								cterm->attr|=6<<4;
								break;
							case 44:
								cterm->attr&=143;
								cterm->attr|=1<<4;
								break;
							case 45:
								cterm->attr&=143;
								cterm->attr|=5<<4;
								break;
							case 46:
								cterm->attr&=143;
								cterm->attr|=3<<4;
								break;
							case 47:
								cterm->attr&=143;
								cterm->attr|=7<<4;
								break;
						}
					}
					TEXTATTR(cterm->attr);
					break;
				case 'n':
					i=strtoul(cterm->escbuf+1,NULL,10);
					switch(i) {
						case 5:
							if(retbuf!=NULL) {
								strcpy(tmp,"\x1b[0n");
								if(strlen(retbuf)+strlen(tmp) < retsize)
									strcat(retbuf,tmp);
							}
							break;
						case 6:
							if(retbuf!=NULL) {
								row = WHEREY();
								if(cterm->origin_mode)
									row = row - cterm->top_margin + 1;
								sprintf(tmp,"%c[%d;%dR",27,row,WHEREX());
								if(strlen(retbuf)+strlen(tmp) < retsize)
									strcat(retbuf,tmp);
							}
							break;
						case 255:
							if(retbuf!=NULL) {
								row = cterm->height;
								if(cterm->origin_mode)
									row = (cterm->bottom_margin - cterm->top_margin) + 1;
								sprintf(tmp,"%c[%d;%dR",27,row,cterm->width);
								if(strlen(retbuf)+strlen(tmp) < retsize)
									strcat(retbuf,tmp);
							}
							break;
					}
					break;
				case 'p': /* ToDo?  ANSI keyboard reassignment */
					break;
				case 'q': /* ToDo?  VT100 keyboard lights */
					break;
				case 'r': /* ToDo?  Scrolling reigon */
					/* Set communication speed (if has a * before it) */
					if(*(p-1) == '*' && speed != NULL) {
						/*
						 * Ps1 			Comm Line 		Ps2 		Communication Speed
						 * none, 0, 1 	Host Transmit 	none, 0 	Use default speed.
						 * 2		 	Host Receive 	1 			300
						 * 3 			Printer 		2 			600
						 * 4		 	Modem Hi 		3 			1200
						 * 5		 	Modem Lo 		4 			2400
						 * 								5 			4800
						 * 								6 			9600
						 * 								7 			19200
						 * 								8 			38400
						 * 								9 			57600
						 * 								10 			76800
						 * 								11 			115200
						 */
						int newspeed=0;

						*(--p)=0;
						if(cterm->escbuf[1]) {
							p=strtok(cterm->escbuf+1,";");
							if(p!=NULL) {
								if(p!=cterm->escbuf+1 || strtoul(p,NULL,10)<2) {
									if(p==cterm->escbuf+1)
										p=strtok(NULL,";");
									if(p!=NULL) {
										switch(strtoul(p,NULL,10)) {
											case 0:
												newspeed=0;
												break;
											case 1:
												newspeed=300;
												break;
											case 2:
												newspeed=600;
												break;
											case 3:
												newspeed=1200;
												break;
											case 4:
												newspeed=2400;
												break;
											case 5:
												newspeed=4800;
												break;
											case 6:
												newspeed=9600;
												break;
											case 7:
												newspeed=19200;
												break;
											case 8:
												newspeed=38400;
												break;
											case 9:
												newspeed=57600;
												break;
											case 10:
												newspeed=76800;
												break;
											case 11:
												newspeed=115200;
												break;
											default:
												newspeed=-1;
												break;
										}
									}
								}
								else
									newspeed = -1;
							}
						}
						if(newspeed >= 0)
							*speed = newspeed;
					}
					else {
						row = 1;
						max_row = cterm->height;
						if(strlen(cterm->escbuf)>2) {
							if((p=strtok(cterm->escbuf+1,";"))!=NULL) {
								row=strtoul(p,NULL,10);
								if((p=strtok(NULL,";"))!=NULL) {
									max_row=strtoul(p,NULL,10);
								}
							}
						}
						if(row >= 1 && max_row > row && max_row <= cterm->height) {
							cterm->top_margin = row;
							cterm->bottom_margin = max_row;
						}
					}
					break;
				case 's':
					cterm->save_xpos=WHEREX();
					cterm->save_ypos=WHEREY();
					break;
				case 'u':
					if(cterm->save_ypos>0 && cterm->save_ypos<=cterm->height
							&& cterm->save_xpos>0 && cterm->save_xpos<=cterm->width) {
						if(cterm->origin_mode && (cterm->save_ypos < cterm->top_margin || cterm->save_ypos > cterm->bottom_margin))
							break;
						GOTOXY(cterm->save_xpos,cterm->save_ypos);
					}
					break;
				case 'y':	/* ToDo?  VT100 Tests */
					break;
				case 'z':	/* ToDo?  Reset */
					break;
				case '|':	/* SyncTERM ANSI Music */
					cterm->music=1;
					break;
			}
			break;
#if 0
		case 'D':
			scrollup(cterm);
			break;
		case 'M':
			scrolldown(cterm);
			break;
#endif
		case '_':	// Application Program Command - APC
			cterm->string = CTERM_STRING_APC;
			FREE_AND_NULL(cterm->strbuf);
			cterm->strbuf = malloc(1024);
			cterm->strbufsize = 1024;
			cterm->strbuflen = 0;
			break;
		case 'P':	// Device Control String - DCS
			cterm->string = CTERM_STRING_DCS;
			FREE_AND_NULL(cterm->strbuf);
			cterm->strbuf = malloc(1024);
			cterm->strbufsize = 1024;
			cterm->strbuflen = 0;
			break;
		case ']':	// Operating System Command - OSC
			cterm->string = CTERM_STRING_OSC;
			FREE_AND_NULL(cterm->strbuf);
			cterm->strbuf = malloc(1024);
			cterm->strbufsize = 1024;
			cterm->strbuflen = 0;
			break;
		case '^':	// Privacy Message - PM
			cterm->string = CTERM_STRING_PM;
			FREE_AND_NULL(cterm->strbuf);
			cterm->strbuf = malloc(1024);
			cterm->strbufsize = 1024;
			cterm->strbuflen = 0;
			break;
		case 'X':	// Start Of String - SOS
			cterm->string = CTERM_STRING_SOS;
			FREE_AND_NULL(cterm->strbuf);
			cterm->strbuf = malloc(1024);
			cterm->strbufsize = 1024;
			cterm->strbuflen = 0;
			break;
		case '\\':
			if (cterm->string) {
				// TODO: Handle the string...
				FREE_AND_NULL(cterm->strbuf);
				cterm->strbufsize = cterm->strbuflen = 0;
				cterm->string = 0;
			}
			break;
		case 'c':
			/* ToDo: Reset Terminal */
			break;
	}
	cterm->escbuf[0]=0;
	cterm->sequence=0;
}

struct cterminal* CIOLIBCALL cterm_init(int height, int width, int xpos, int ypos, int backlines, unsigned char *scrollback, int emulation)
{
	char	*revision="$Revision$";
	char *in;
	char	*out;
	int		i;
	struct cterminal *cterm;

	if((cterm=malloc(sizeof(struct cterminal)))==NULL)
		return cterm;
	memset(cterm, 0, sizeof(struct cterminal));
	cterm->x=xpos;
	cterm->y=ypos;
	cterm->height=height;
	cterm->width=width;
	cterm->top_margin=1;
	cterm->bottom_margin=height;
	cterm->save_xpos=0;
	cterm->save_ypos=0;
	cterm->escbuf[0]=0;
	cterm->sequence=0;
	cterm->string = 0;
	cterm->strbuf = NULL;
	cterm->strbuflen = 0;
	cterm->strbufsize = 0;
	cterm->music_enable=CTERM_MUSIC_BANSI;
	cterm->music=0;
	cterm->tempo=120;
	cterm->octave=4;
	cterm->notelen=4;
	cterm->noteshape=CTERM_MUSIC_NORMAL;
	cterm->musicfore=TRUE;
	cterm->backpos=0;
	cterm->backlines=backlines;
	cterm->scrollback=scrollback;
	cterm->log=CTERM_LOG_NONE;
	cterm->logfile=NULL;
	cterm->emulation=emulation;
	cterm->cursor=_NORMALCURSOR;
	cterm->autowrap=true;
	cterm->origin_mode=false;
	if(cterm->scrollback!=NULL)
		memset(cterm->scrollback,0,cterm->width*2*cterm->backlines);
	strcpy(cterm->DA,"\x1b[=67;84;101;114;109;");
	out=strchr(cterm->DA, 0);
	if(out != NULL) {
		for(in=revision; *in; in++) {
			if(isdigit(*in))
				*(out++)=*in;
			if(*in=='.')
				*(out++)=';';
		}
		*out=0;
	}
	strcat(cterm->DA,"c");
	cterm->setfont_result = CTERM_NO_SETFONT_REQUESTED;
	/* Fire up note playing thread */
	if(!cterm->playnote_thread_running) {
		listInit(&cterm->notes, LINK_LIST_SEMAPHORE|LINK_LIST_MUTEX);
		sem_init(&cterm->note_completed_sem,0,0);
		sem_init(&cterm->playnote_thread_terminated,0,0);
		_beginthread(playnote_thread, 0, cterm);
	}

	/* Set up tabs for ATASCII */
	if(cterm->emulation == CTERM_EMULATION_ATASCII) {
		for(i=0; i<(sizeof(cterm_tabs)/sizeof(cterm_tabs[0])); i++)
			cterm->escbuf[cterm_tabs[i]]=1;
	}

#ifndef CTERM_WITHOUT_CONIO
	cterm->ciolib_gotoxy=ciolib_gotoxy;
	cterm->ciolib_wherex=ciolib_wherex;
	cterm->ciolib_wherey=ciolib_wherey;
	cterm->ciolib_gettext=ciolib_gettext;
	cterm->ciolib_gettextinfo=ciolib_gettextinfo;
	cterm->ciolib_textattr=ciolib_textattr;
	cterm->ciolib_setcursortype=ciolib_setcursortype;
	cterm->ciolib_movetext=ciolib_movetext;
	cterm->ciolib_clreol=ciolib_clreol;
	cterm->ciolib_clrscr=ciolib_clrscr;
	cterm->ciolib_setvideoflags=ciolib_setvideoflags;
	cterm->ciolib_getvideoflags=ciolib_getvideoflags;
	cterm->ciolib_setscaling=ciolib_setscaling;
	cterm->ciolib_getscaling=ciolib_getscaling;
	cterm->ciolib_putch=ciolib_putch;
	cterm->ciolib_puttext=ciolib_puttext;
	cterm->ciolib_window=ciolib_window;
	cterm->ciolib_cputs=ciolib_cputs;
	cterm->ciolib_setfont=ciolib_setfont;
	cterm->_wscroll=&_wscroll;
	cterm->puttext_can_move=&puttext_can_move;
	cterm->hold_update=&hold_update;
#endif

	return cterm;
}

void CIOLIBCALL cterm_start(struct cterminal *cterm)
{
	struct text_info ti;

	if(!cterm->started) {
		GETTEXTINFO(&ti);
		cterm->attr=ti.normattr;
		TEXTATTR(cterm->attr);
		SETCURSORTYPE(cterm->cursor);
		cterm->started=1;
		if(ti.winleft != cterm->x || ti.wintop != cterm->y || ti.winright != cterm->x+cterm->width-1 || ti.winleft != cterm->y+cterm->height-1)
			WINDOW(cterm->x,cterm->y,cterm->x+cterm->width-1,cterm->y+cterm->height-1);
		cterm_clearscreen(cterm, cterm->attr);
		GOTOXY(1,1);
	}
}

static void ctputs(struct cterminal *cterm, char *buf)
{
	char *outp;
	char *p;
	int		oldscroll;
	int		cx;
	int		cy;
	int		i;

	outp=buf;
	oldscroll=*cterm->_wscroll;
	*cterm->_wscroll=0;
	cx=WHEREX();
	cy=WHEREY();
	if(cterm->log==CTERM_LOG_ASCII && cterm->logfile != NULL)
		fputs(buf, cterm->logfile);
	for(p=buf;*p;p++) {
		switch(*p) {
			case '\r':
				cx=1;
				break;
			case '\n':
				*p=0;
				CPUTS(outp);
				outp=p+1;
				if(cy==cterm->bottom_margin)
					scrollup(cterm);
				else if(cy < cterm->height)
					cy++;
				GOTOXY(cx,cy);
				break;
			case '\b':
				*p=0;
				CPUTS(outp);
				outp=p+1;
				if(cx>1)
					cx--;
				GOTOXY(cx,cy);
				break;
			case 7:		/* Bell */
				break;
			case '\t':
				*p=0;
				CPUTS(outp);
				outp=p+1;
				for(i=0;i<sizeof(cterm_tabs)/sizeof(cterm_tabs[0]);i++) {
					if(cterm_tabs[i]>cx) {
						cx=cterm_tabs[i];
						break;
					}
				}
				if(cx>cterm->width) {
					cx=1;
					if(cy==cterm->bottom_margin)
						scrollup(cterm);
					else if(cy < cterm->height)
						cy++;
				}
				GOTOXY(cx,cy);
				break;
			default:
				if(cx==cterm->width && (!cterm->autowrap)) {
					char ch;
					ch=*(p+1);
					*(p+1)=0;
					CPUTS(outp);
					*(p+1)=ch;
					outp=p+1;
					GOTOXY(cx,cy);
				}
				else {
					if(cy==cterm->bottom_margin
							&& cx==cterm->width) {
						char ch;
						ch=*(p+1);
						*(p+1)=0;
						CPUTS(outp);
						*(p+1)=ch;
						outp=p+1;
						scrollup(cterm);
						cx=1;
						GOTOXY(cx,cy);
					}
					else {
						if(cx==cterm->width && cy < cterm->height) {
							cx=1;
							cy++;
						}
						else {
							cx++;
						}
					}
				}
				break;
		}
	}
	CPUTS(outp);
	*cterm->_wscroll=oldscroll;
}

CIOLIBEXPORT char* CIOLIBCALL cterm_write(struct cterminal * cterm, const void *vbuf, int buflen, char *retbuf, size_t retsize, int *speed)
{
	const unsigned char *buf = (unsigned char *)vbuf;
	unsigned char ch[2];
	unsigned char prn[BUFSIZE];
	int j,k,l;
	struct text_info	ti;
	int	olddmc;
	int oldptnm;

	if(!cterm->started)
		cterm_start(cterm);

	oldptnm=*cterm->puttext_can_move;
	*cterm->puttext_can_move=1;
	olddmc=*cterm->hold_update;
	*cterm->hold_update=1;
	if(retbuf!=NULL)
		retbuf[0]=0;
	GETTEXTINFO(&ti);
	if(ti.winleft != cterm->x || ti.wintop != cterm->y || ti.winright != cterm->x+cterm->width-1 || ti.winbottom != cterm->y+cterm->height-1)
		WINDOW(cterm->x,cterm->y,cterm->x+cterm->width-1,cterm->y+cterm->height-1);
	GOTOXY(cterm->xpos,cterm->ypos);
	TEXTATTR(cterm->attr);
	SETCURSORTYPE(cterm->cursor);
	ch[1]=0;
	if(buflen==-1)
		buflen=strlen(buf);
	switch(buflen) {
		case 0:
			break;
		default:
			if(cterm->log==CTERM_LOG_RAW && cterm->logfile != NULL)
				fwrite(buf, buflen, 1, cterm->logfile);
			prn[0]=0;
			for(j=0;j<buflen;j++) {
				if(strlen(prn) >= sizeof(prn)-sizeof(cterm->escbuf)) {
					ctputs(cterm, prn);
					prn[0]=0;
				}
				ch[0]=buf[j];
				if (cterm->string) {
					switch (cterm->string) {
						case CTERM_STRING_APC:
							/* 0x08-0x0d, 0x20-0x7e */
						case CTERM_STRING_DCS:
							/* 0x08-0x0d, 0x20-0x7e */
						case CTERM_STRING_OSC:
							/* 0x08-0x0d, 0x20-0x7e */
						case CTERM_STRING_PM:
							/* 0x08-0x0d, 0x20-0x7e */
							if (ch[0] < 8 || (ch[0] > 0x0d && ch[0] < 0x20) || ch[0] > 0x7e) {
								cterm->string = 0;
								// TODO: Detect correct string termination...
								FREE_AND_NULL(cterm->strbuf);
								cterm->strbuflen = cterm->strbufsize = 0;
							}
							else {
								if (cterm->strbuf) {
									cterm->strbuf[cterm->strbuflen++] = ch[0];
									if (cterm->strbuflen == cterm->strbufsize) {
										char *p;

										cterm->strbufsize += 1024;
										p = realloc(cterm->strbuf, cterm->strbufsize);
										if (p == NULL) {
											FREE_AND_NULL(cterm->strbuf);
											cterm->strbuflen = cterm->strbufsize = 0;
										}
										else
											cterm->strbuf = p;
									}
								}
							}
							break;
						case CTERM_STRING_SOS:
							/* Anything but SOS or ST (ESC X or ESC \) */
							if (ch[0] == 'X' || ch[0] == '\\' && 
							    cterm->strbuf && cterm->strbuflen &&
							    cterm->strbuf[cterm->strbuflen-1] == '\e') {
								cterm->strbuflen--;
								cterm->string = 0;
								FREE_AND_NULL(cterm->strbuf);
								cterm->strbuflen = cterm->strbufsize = 0;
								cterm_write(cterm, "\e", 1, retbuf+strlen(retbuf), retsize-strlen(retbuf), speed);
							}
							else {
								if (cterm->strbuf == NULL) {
									cterm->string = 0;
									FREE_AND_NULL(cterm->strbuf);
									cterm->strbuflen = cterm->strbufsize = 0;
								}
								else {
									cterm->strbuf[cterm->strbuflen++] = ch[0];
									if (cterm->strbuflen == cterm->strbufsize) {
										char *p;

										cterm->strbufsize *= 2;
										p = realloc(cterm->strbuf, cterm->strbufsize);
										if (p == NULL) {
											cterm->string = 0;
											FREE_AND_NULL(cterm->strbuf);
											cterm->strbuflen = cterm->strbufsize = 0;
										}
									}
								}
							}
							break;
					}
				}
				if (!cterm->string) {
					if(cterm->font_size) {
						cterm->fontbuf[cterm->font_read++]=ch[0];
						if(cterm->font_read == cterm->font_size) {
#ifndef CTERM_WITHOUT_CONIO
							char *buf2;

							if((buf2=(char *)malloc(cterm->font_size))!=NULL) {
								memcpy(buf2,cterm->fontbuf,cterm->font_size);
								if(cterm->font_slot >= CONIO_FIRST_FREE_FONT) {
									switch(cterm->font_size) {
										case 4096:
											FREE_AND_NULL(conio_fontdata[cterm->font_slot].eight_by_sixteen);
											conio_fontdata[cterm->font_slot].eight_by_sixteen=buf2;
											FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
											conio_fontdata[cterm->font_slot].desc=strdup("Remote Defined Font");
											break;
										case 3586:
											FREE_AND_NULL(conio_fontdata[cterm->font_slot].eight_by_fourteen);
											conio_fontdata[cterm->font_slot].eight_by_fourteen=buf2;
											FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
											conio_fontdata[cterm->font_slot].desc=strdup("Remote Defined Font");
											break;
										case 2048:
											FREE_AND_NULL(conio_fontdata[cterm->font_slot].eight_by_eight);
											conio_fontdata[cterm->font_slot].eight_by_eight=buf2;
											FREE_AND_NULL(conio_fontdata[cterm->font_slot].desc);
											conio_fontdata[cterm->font_slot].desc=strdup("Remote Defined Font");
											break;
										default:
											FREE_AND_NULL(buf2);
											break;
									}
								}
								else
									FREE_AND_NULL(buf2);
							}
#endif
							cterm->font_size=0;
						}
					}
					else if(cterm->sequence) {
						k=strlen(cterm->escbuf);
						if(k+1 >= sizeof(cterm->escbuf)) {
							/* Broken sequence detected */
							strcat(prn,"\033");
							strcat(prn,cterm->escbuf);
							cterm->escbuf[0]=0;
							cterm->sequence=0;
						}
						else {
							strcat(cterm->escbuf,ch);
							if(k) {
								if(cterm->escbuf[0] != '[') {	/* Not a CSI code. */
									/* ANSI control characters */
									if(ch[0] >= 32 && ch[0] <= 47) {
										/* Legal intermediate character */
									}
									else if(ch[0] >= 48 && ch[0] <= 126) {
										/* Terminating character */
										do_ansi(cterm, retbuf, retsize, speed);
									}
									else {
										/* Broken sequence detected */
										strcat(prn,"\033");
										strcat(prn,cterm->escbuf);
										cterm->escbuf[0]=0;
										cterm->sequence=0;
									}
								}
								else {
									/* We know that it was a CSI at this point */
									/* Here's where we get funky! */
									/* the last character defines the set of legal next characters */
									if(ch[0] >= 48 && ch[0] <= 63) {
										/* Parameter character.  Only legal after '[' and other param chars */
										if(cterm->escbuf[k]!='[' 
												&& (cterm->escbuf[k] < 48 || cterm->escbuf[k] > 63)) {
											/* Broken sequence detected */
											strcat(prn,"\033");
											strcat(prn,cterm->escbuf);
											cterm->escbuf[0]=0;
											cterm->sequence=0;
										}
									}
									else if(ch[0] >= 32 && ch[0] <= 47) {
										/* Intermediate character.  Legal after '[', param, or intermetiate chars */
										if(cterm->escbuf[k]!='[' 
												&& (cterm->escbuf[k] < 48 || cterm->escbuf[k] > 63) 
												&& (cterm->escbuf[k] < 32 || cterm->escbuf[k] > 47)) {
											/* Broken sequence detected */
											strcat(prn,"\033");
											strcat(prn,cterm->escbuf);
											cterm->escbuf[0]=0;
											cterm->sequence=0;
										}
									}
									else if(ch[0] >= 64 && ch[0] <= 126) {
											/* Terminating character.  Always legal at this point. */
										do_ansi(cterm, retbuf, retsize, speed);
									}
									else {
										/* Broken sequence detected */
										strcat(prn,"\033");
										strcat(prn,cterm->escbuf);
										cterm->escbuf[0]=0;
										cterm->sequence=0;
									}
								}
							}
							else {
								/* First char after the ESC */
								if(ch[0] >= 32 && ch[0] <= 47) {
									/* Legal intermediate character */
									/* No CSI then */
								}
								else if(ch[0]=='[') {
									/* CSI received */
								}
								else if(ch[0] >= 48 && ch[0] <= 126) {
									/* Terminating character */
									do_ansi(cterm, retbuf, retsize, speed);
								}
								else {
									/* Broken sequence detected */
									strcat(prn,"\033");
									strcat(prn,cterm->escbuf);
									cterm->escbuf[0]=0;
									cterm->sequence=0;
								}
							}
							if(ch[0]=='\033') {	/* Broken sequence followed by a legal one! */
								if(prn[0])	/* Don't display the ESC */
									prn[strlen(prn)-1]=0;
								ctputs(cterm, prn);
								prn[0]=0;
								cterm->sequence=1;
							}
						}
					}
					else if (cterm->music) {
						if(ch[0]==14) {
							*cterm->hold_update=0;
							*cterm->puttext_can_move=0;
							GOTOXY(WHEREX(),WHEREY());
							SETCURSORTYPE(cterm->cursor);
							*cterm->hold_update=1;
							*cterm->puttext_can_move=1;
							play_music(cterm);
						}
						else {
							if(strchr(musicchars,ch[0])!=NULL)
								strcat(cterm->musicbuf,ch);
							else {
								/* Kill non-music strings */
								cterm->music=0;
								cterm->musicbuf[0]=0;
							}
						}
					}
					else {
						if(cterm->emulation == CTERM_EMULATION_ATASCII) {
							if(cterm->attr==7) {
								switch(buf[j]) {
									case 27:	/* ESC */
										cterm->attr=1;
										break;
									case 28:	/* Up (TODO: Wraps??) */
										l=WHEREY()-1;
										if(l<1)
											l=cterm->height;
										GOTOXY(WHEREX(),l);
										break;
									case 29:	/* Down (TODO: Wraps??) */
										l=WHEREY()+1;
										if(l>cterm->height)
											l=1;
										GOTOXY(WHEREX(),l);
										break;
									case 30:	/* Left (TODO: Wraps around to same line?) */
										l=WHEREX()-1;
										if(l<1)
											l=cterm->width;
										GOTOXY(l,WHEREY());
										break;
									case 31:	/* Right (TODO: Wraps around to same line?) */
										l=WHEREX()+1;
										if(l>cterm->width)
											l=1;
										GOTOXY(l,WHEREY());
										break;
									case 125:	/* Clear Screen */
										cterm_clearscreen(cterm, cterm->attr);
										break;
									case 126:	/* Backspace (TODO: Wraps around to previous line?) */
												/* DOES NOT delete char, merely erases */
										k=WHEREY();
										l=WHEREX()-1;

										if(l<1) {
											k--;
											if(k<1)
												break;
											l=cterm->width;
										}
										GOTOXY(l,k);
										PUTCH(0);
										GOTOXY(l,k);
										break;
									/* We abuse the ESC buffer for tab stops */
									case 127:	/* Tab (Wraps around to next line) */
										l=WHEREX();
										for(k=l+1; k<=cterm->width; k++) {
											if(cterm->escbuf[k]) {
												l=k;
												break;
											}
										}
										if(k>cterm->width) {
											l=1;
											k=WHEREY()+1;
											if(k>cterm->height) {
												scrollup(cterm);
												k=cterm->height;
											}
											GOTOXY(l,k);
										}
										else
											GOTOXY(l,WHEREY());
										break;
									case 155:	/* Return */
										k=WHEREY();
										if(k==cterm->height)
											scrollup(cterm);
										else
											k++;
										GOTOXY(1,k);
										break;
									case 156:	/* Delete Line */
										dellines(cterm, 1);
										GOTOXY(1,WHEREY());
										break;
									case 157:	/* Insert Line */
										l=WHEREX();
										k=WHEREY();
										if(k<cterm->height)
											MOVETEXT(cterm->x,cterm->y+k-1
													,cterm->x+cterm->width-1,cterm->y+cterm->height-2
													,cterm->x,cterm->y+k);
										GOTOXY(1,k);
										CLREOL();
										break;
									case 158:	/* Clear Tab */
										cterm->escbuf[WHEREX()]=0;
										break;
									case 159:	/* Set Tab */
										cterm->escbuf[WHEREX()]=1;
										break;
									case 253:	/* Beep */
										if(!cterm->quiet) {
											#ifdef __unix__
												PUTCH(7);
											#else
												MessageBeep(MB_OK);
											#endif
										}
										break;
									case 254:	/* Delete Char */
										l=WHEREX();
										k=WHEREY();
										if(l<cterm->width)
											MOVETEXT(cterm->x+l,cterm->y+k-1
													,cterm->x+cterm->width-1,cterm->y+k-1
													,cterm->x+l-1,cterm->y+k-1);
										GOTOXY(cterm->width,k);
										CLREOL();
										GOTOXY(l,k);
										break;
									case 255:	/* Insert Char */
										l=WHEREX();
										k=WHEREY();
										if(l<cterm->width)
											MOVETEXT(cterm->x+l-1,cterm->y+k-1
													,cterm->x+cterm->width-2,cterm->y+k-1
													,cterm->x+l,cterm->y+k-1);
										PUTCH(0);
										GOTOXY(l,k);
										break;
									default:
										/* Translate to screen codes */
										k=buf[j];
										if(k < 32) {
											k +=64;
										}
										else if(k < 96) {
											k -= 32;
										}
										else if(k < 128) {
											/* No translation */
										}
										else if(k < 160) {
											k +=64;
										}
										else if(k < 224) {
											k -= 32;
										}
										else if(k < 256) {
											/* No translation */
										}
										ch[0] = k;
										ch[1] = cterm->attr;
										PUTTEXT(cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,ch);
										ch[1]=0;
										if(WHEREX()==cterm->width) {
											if(WHEREY()==cterm->height) {
												scrollup(cterm);
												GOTOXY(1,WHEREY());
											}
											else
												GOTOXY(1,WHEREY()+1);
										}
										else
											GOTOXY(WHEREX()+1,WHEREY());
										break;
								}
							}
							else {
								switch(buf[j]) {
									case 155:	/* Return */
										k=WHEREY();
										if(k==cterm->height)
											scrollup(cterm);
										else
											k++;
										GOTOXY(1,k);
										break;
									default:
										/* Translate to screen codes */
										k=buf[j];
										if(k < 32) {
											k +=64;
										}
										else if(k < 96) {
											k -= 32;
										}
										else if(k < 128) {
											/* No translation */
										}
										else if(k < 160) {
											k +=64;
										}
										else if(k < 224) {
											k -= 32;
										}
										else if(k < 256) {
											/* No translation */
										}
										ch[0] = k;
										ch[1] = cterm->attr;
										PUTTEXT(cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,ch);
										ch[1]=0;
										if(WHEREX()==cterm->width) {
											if(WHEREY()==cterm->height) {
												scrollup(cterm);
												GOTOXY(1,cterm->height);
											}
											else
												GOTOXY(1,WHEREY()+1);
										}
										else
											GOTOXY(WHEREX()+1,WHEREY());
										break;
								}
								cterm->attr=7;
							}
						}
						else if(cterm->emulation == CTERM_EMULATION_PETASCII) {
							switch(buf[j]) {
								case 5:		/* White */
								case 28:	/* Red */
								case 30:	/* Green */
								case 31:	/* Blue */
								case 129:	/* Orange */
								case 144:	/* Black */
								case 149:	/* Brown */
								case 150:	/* Light Red */
								case 151:	/* Dark Gray */
								case 152:	/* Grey */
								case 153:	/* Light Green */
								case 154:	/* Light Blue */
								case 155:	/* Light Gray */
								case 156:	/* Purple */
								case 158:	/* Yellow */
								case 159:	/* Cyan */
									cterm->attr &= 0xf0;
									switch(buf[j]) {
										case 5:		/* White */
											cterm->attr |= 1;
											break;
										case 28:	/* Red */
											cterm->attr |= 2;
											break;
										case 30:	/* Green */
											cterm->attr |= 5;
											break;
										case 31:	/* Blue */
											cterm->attr |= 6;
											break;
										case 129:	/* Orange */
											cterm->attr |= 8;
											break;
										case 144:	/* Black */
											cterm->attr |= 0;
											break;
										case 149:	/* Brown */
											cterm->attr |= 9;
											break;
										case 150:	/* Light Red */
											cterm->attr |= 10;
											break;
										case 151:	/* Dark Gray */
											cterm->attr |= 11;
											break;
										case 152:	/* Grey */
											cterm->attr |= 12;
											break;
										case 153:	/* Light Green */
											cterm->attr |= 13;
											break;
										case 154:	/* Light Blue */
											cterm->attr |= 14;
											break;
										case 155:	/* Light Gray */
											cterm->attr |= 15;
											break;
										case 156:	/* Purple */
											cterm->attr |= 4;
											break;
										case 158:	/* Yellow */
											cterm->attr |= 7;
											break;
										case 159:	/* Cyan */
											cterm->attr |= 3;
											break;
									}
									TEXTATTR(cterm->attr);
									break;

								/* Movement */
								case 13:	/* "\r\n" and disabled reverse. */
								case 141:
									GOTOXY(1, WHEREY());
									/* Fall-through */
								case 17:
									if(WHEREY()==cterm->height)
										scrollup(cterm);
									else
										GOTOXY(WHEREX(), WHEREY()+1);
									break;
								case 147:
									cterm_clearscreen(cterm, cterm->attr);
									/* Fall through */
								case 19:
									GOTOXY(1,1);
									break;
								case 20:	/* Delete (Wrapping backspace) */
									k=WHEREY();
									l=WHEREX();

									if(l==1) {
										if(k==1)
											break;
										GOTOXY((l=cterm->width), k-1);
									}
									else
										GOTOXY(--l, k);
									if(l<cterm->width)
										MOVETEXT(cterm->x+l,cterm->y+k-1
												,cterm->x+cterm->width-1,cterm->y+k-1
												,cterm->x+l-1,cterm->y+k-1);
									GOTOXY(cterm->width,k);
									CLREOL();
									GOTOXY(l,k);
									break;
								case 157:	/* Cursor Left (wraps) */
									if(WHEREX()==1) {
										if(WHEREY() > 1)
											GOTOXY(cterm->width, WHEREY()-1);
									}
									else
										GOTOXY(WHEREX()-1, WHEREY());
									break;
								case 29:	/* Cursor Right (wraps) */
									if(WHEREX()==cterm->width) {
										if(WHEREY()==cterm->height) {
											scrollup(cterm);
											GOTOXY(1,WHEREY());
										}
										else
											GOTOXY(1,WHEREY()+1);
									}
									else
										GOTOXY(WHEREX()+1,WHEREY());
									break;
								case 145:	/* Cursor Up (No scroll */
									if(WHEREY()>1)
										GOTOXY(WHEREX(),WHEREY()-1);
									break;
								case 148:	/* Insert TODO verify last column */
											/* CGTerm does nothing there... we */
											/* Erase under cursor. */
									l=WHEREX();
									k=WHEREY();
									if(l<=cterm->width)
										MOVETEXT(cterm->x+l-1,cterm->y+k-1
												,cterm->x+cterm->width-2,cterm->y+k-1
												,cterm->x+l,cterm->y+k-1);
									PUTCH(' ');
									GOTOXY(l,k);
									break;

								/* Font change... whee! */
								case 14:	/* Lower case font */
									if(ti.currmode == C64_40X25)
										SETFONT(33,FALSE,1);
									else	/* Assume C128 */
										SETFONT(35,FALSE,1);
									break;
								case 142:	/* Upper case font */
									if(ti.currmode == C64_40X25)
										SETFONT(32,FALSE,1);
									else	/* Assume C128 */
										SETFONT(34,FALSE,1);
									break;
								case 18:	/* Reverse mode on */
									cterm->c64reversemode = 1;
									break;
								case 146:	/* Reverse mode off */
									cterm->c64reversemode = 0;
									break;

								/* Extras */
								case 7:			/* Beep */
									if(!cterm->quiet) {
										#ifdef __unix__
											PUTCH(7);
										#else
											MessageBeep(MB_OK);
										#endif
									}
									break;

								/* Translate to screen codes */
								default:
									k=buf[j];
									if(k<32) {
										break;
									}
									else if(k<64) {
										/* No translation */
									}
									else if(k<96) {
										k -= 64;
									}
									else if(k<128) {
										k -= 32;
									}
									else if(k<160) {
										break;
									}
									else if(k<192) {
										k -= 64;
									}
									else if(k<224) {
										k -= 128;
									}
									else {
										if(k==255)
											k = 94;
										else
											k -= 128;
									}
									if(cterm->c64reversemode)
										k+=128;
									ch[0] = k;
									ch[1] = cterm->attr;
									PUTTEXT(cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,ch);
									ch[1]=0;
									if(WHEREX()==cterm->width) {
										if(WHEREY()==cterm->height) {
											scrollup(cterm);
											GOTOXY(1,WHEREY());
										}
										else
											GOTOXY(1,WHEREY()+1);
									}
									else
										GOTOXY(WHEREX()+1,WHEREY());
									break;
							}
						}
						else {	/* ANSI-BBS */
							if(cterm->doorway_char) {
								ctputs(cterm, prn);
								ch[1]=cterm->attr;
								PUTTEXT(cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,cterm->x+WHEREX()-1,cterm->y+WHEREY()-1,ch);
								ch[1]=0;
								if(WHEREX()==cterm->width) {
									if(WHEREY()==cterm->bottom_margin) {
										scrollup(cterm);
										GOTOXY(1,WHEREY());
									}
									else if(WHEREY()==cterm->height)
										GOTOXY(1,WHEREY());
									else
										GOTOXY(1,WHEREY()+1);
								}
								else
									GOTOXY(WHEREX()+1,WHEREY());
								cterm->doorway_char=0;
							}
							else {
								switch(buf[j]) {
									case 0:
										if(cterm->doorway_mode)
											cterm->doorway_char=1;
										break;
									case 7:			/* Beep */
										ctputs(cterm, prn);
										prn[0]=0;
										if(cterm->log==CTERM_LOG_ASCII && cterm->logfile != NULL)
											fputs("\x07", cterm->logfile);
										if(!cterm->quiet) {
											#ifdef __unix__
												PUTCH(7);
											#else
												MessageBeep(MB_OK);
											#endif
										}
										break;
									case 12:		/* ^L - Clear screen */
										ctputs(cterm, prn);
										prn[0]=0;
										if(cterm->log==CTERM_LOG_ASCII && cterm->logfile != NULL)
											fputs("\x0c", cterm->logfile);
										cterm_clearscreen(cterm, (char)cterm->attr);
										if(cterm->origin_mode)
											GOTOXY(1,cterm->top_margin);
										else
											GOTOXY(1,1);
										break;
									case 27:		/* ESC */
										ctputs(cterm, prn);
										prn[0]=0;
										cterm->sequence=1;
										break;
									default:
										strcat(prn,ch);
								}
							}
						}
					}
				}
			}
			ctputs(cterm, prn);
			prn[0]=0;
			break;
	}
	cterm->xpos=WHEREX();
	cterm->ypos=WHEREY();
#if 0
	if(ti.winleft != cterm->x || ti.wintop != cterm->y || ti.winright != cterm->x+cterm->width-1 || ti.winleft != cterm->y+cterm->height-1)
		WINDOW(ti.winleft,ti.wintop,ti.winright,ti.winbottom);
	GOTOXY(ti.curx,ti.cury);
	TEXTATTR(ti.attribute);
#endif

	*cterm->hold_update=olddmc;
	*cterm->puttext_can_move=oldptnm;
	GOTOXY(WHEREX(),WHEREY());
	SETCURSORTYPE(cterm->cursor);
	return(retbuf);
}

int CIOLIBCALL cterm_openlog(struct cterminal *cterm, char *logfile, int logtype)
{
	if(!cterm->started)
		cterm_start(cterm);

	cterm->logfile=fopen(logfile, "ab");
	if(cterm->logfile==NULL)
		return(0);
	cterm->log=logtype;
	return(1);
}

void CIOLIBCALL cterm_closelog(struct cterminal *cterm)
{
	if(!cterm->started)
		cterm_start(cterm);

	if(cterm->logfile != NULL)
		fclose(cterm->logfile);
	cterm->logfile=NULL;
	cterm->log=CTERM_LOG_NONE;
}

void CIOLIBCALL cterm_end(struct cterminal *cterm)
{
	int i;

	if(cterm) {
		cterm_closelog(cterm);
#ifdef CTERM_WITHOUT_CONIO
		FREE_AND_NULL(BD->vmem);
		FREE_AND_NULL(BD);
#else
		for(i=CONIO_FIRST_FREE_FONT; i < 256; i++) {
			FREE_AND_NULL(conio_fontdata[i].eight_by_sixteen);
			FREE_AND_NULL(conio_fontdata[i].eight_by_fourteen);
			FREE_AND_NULL(conio_fontdata[i].eight_by_eight);
			FREE_AND_NULL(conio_fontdata[i].desc);
		}
#endif
		if(cterm->playnote_thread_running) {
			if(sem_trywait(&cterm->playnote_thread_terminated)==-1) {
				listSemPost(&cterm->notes);
				sem_wait(&cterm->playnote_thread_terminated);
			}
			sem_destroy(&cterm->playnote_thread_terminated);
			sem_destroy(&cterm->note_completed_sem);
			listFree(&cterm->notes);
		}

		if (cterm->strbuf)
			FREE_AND_NULL(cterm->strbuf);
		free(cterm);
	}
}
