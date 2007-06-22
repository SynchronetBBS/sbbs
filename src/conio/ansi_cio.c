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

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>	/* malloc */

#include <genwrap.h>
#include <threadwrap.h>
#include <semwrap.h>

#ifdef __unix__
	#include <termios.h>
	struct termios tio_default;				/* Initial term settings */
#endif

#if (defined CIOLIB_IMPORTS)
 #undef CIOLIB_IMPORTS
#endif
#if (defined CIOLIB_EXPORTS)
 #undef CIOLIB_EXPORTS
#endif

#include "ciolib.h"
#include "ansi_cio.h"

int	CIOLIB_ANSI_TIMEOUT=500;

sem_t	got_key;
sem_t	got_input;
sem_t	used_input;
sem_t	goahead;
sem_t	need_key;
static BOOL	sent_ga=FALSE;
WORD	ansi_curr_attr=0x07<<8;

int ansi_rows=-1;
int ansi_cols=80;
int ansi_got_row=0;
int ansi_got_col=0;
int ansi_esc_delay=25;

const int 	ansi_tabs[10]={9,17,25,33,41,49,57,65,73,80};
const int 	ansi_colours[8]={0,4,2,6,1,5,3,7};
static WORD		ansi_inch;
static unsigned char		ansi_raw_inch;
WORD	*ansivmem;
int		ansi_row=0;
int		ansi_col=0;
int		force_move=1;

/* Control sequence table definitions. */
typedef struct
{
   char *pszSequence;
   int chExtendedKey;
} tODKeySequence;

#define ANSI_KEY_UP		72<<8
#define ANSI_KEY_DOWN	80<<8
#define ANSI_KEY_RIGHT	0x4d<<8
#define ANSI_KEY_LEFT	0x4b<<8
#define ANSI_KEY_HOME	0x47<<8
#define ANSI_KEY_END	0x4f<<8
#define ANSI_KEY_F1		0x3b<<8
#define ANSI_KEY_F2		0x3c<<8
#define ANSI_KEY_F3		0x3d<<8
#define ANSI_KEY_F4		0x3e<<8
#define ANSI_KEY_F5		0x3f<<8
#define ANSI_KEY_F6		0x40<<8
#define ANSI_KEY_F7		0x41<<8
#define ANSI_KEY_F8		0x42<<8
#define ANSI_KEY_F9		0x43<<8
#define ANSI_KEY_F10	0x44<<8
#define ANSI_KEY_PGUP	0x49<<8
#define ANSI_KEY_PGDN	0x51<<8
#define ANSI_KEY_INSERT	0x52<<8
#define ANSI_KEY_DELETE	0x53<<8

tODKeySequence ODaKeySequences[] =
{
   /* VT-52 control sequences. */
   {"\033A", ANSI_KEY_UP},
   {"\033B", ANSI_KEY_DOWN},
   {"\033C", ANSI_KEY_RIGHT},
   {"\033D", ANSI_KEY_LEFT},
   {"\033H", ANSI_KEY_HOME},
   {"\033K", ANSI_KEY_END},
   {"\033P", ANSI_KEY_F1},
   {"\033Q", ANSI_KEY_F2},
   {"\033?w", ANSI_KEY_F3},
   {"\033?x", ANSI_KEY_F4},
   {"\033?t", ANSI_KEY_F5},
   {"\033?u", ANSI_KEY_F6},
   {"\033?q", ANSI_KEY_F7},
   {"\033?r", ANSI_KEY_F8},
   {"\033?p", ANSI_KEY_F9},

   /* Control sequences common to VT-100/VT-102/VT-220/VT-320/ANSI. */
   {"\033[A", ANSI_KEY_UP},
   {"\033[B", ANSI_KEY_DOWN},
   {"\033[C", ANSI_KEY_RIGHT},
   {"\033[D", ANSI_KEY_LEFT},
   {"\033[M", ANSI_KEY_PGUP},
   {"\033[H\x1b[2J", ANSI_KEY_PGDN},
   {"\033[H", ANSI_KEY_HOME},
   {"\033[K", ANSI_KEY_END},
   {"\033OP", ANSI_KEY_F1},
   {"\033OQ", ANSI_KEY_F2},
   {"\033OR", ANSI_KEY_F3},
   {"\033OS", ANSI_KEY_F4},

   /* VT-220/VT-320 specific control sequences. */
   {"\033[17~", ANSI_KEY_F6},
   {"\033[18~", ANSI_KEY_F7},
   {"\033[19~", ANSI_KEY_F8},
   {"\033[20~", ANSI_KEY_F9},
   {"\033[21~", ANSI_KEY_F10},

   /* ANSI-specific control sequences. */
   {"\033[L", ANSI_KEY_HOME},
   {"\033Ow", ANSI_KEY_F3},
   {"\033Ox", ANSI_KEY_F4},
   {"\033Ot", ANSI_KEY_F5},
   {"\033Ou", ANSI_KEY_F6},
   {"\033Oq", ANSI_KEY_F7},
   {"\033Or", ANSI_KEY_F8},
   {"\033Op", ANSI_KEY_F9},

   /* PROCOMM-specific control sequences (non-keypad alternatives). */
   {"\033OA", ANSI_KEY_UP},
   {"\033OB", ANSI_KEY_DOWN},
   {"\033OC", ANSI_KEY_RIGHT},
   {"\033OD", ANSI_KEY_LEFT},
   {"\033OH", ANSI_KEY_HOME},
   {"\033OK", ANSI_KEY_END},

   /* Terminator */
   {"",0}
};

#ifdef NEEDS_CFMAKERAW
void
cfmakeraw(struct termios *t)
{
	t->c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
	t->c_oflag &= ~OPOST;
	t->c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
	t->c_cflag &= ~(CSIZE|PARENB);
	t->c_cflag |= CS8;
}
#endif

/* Do NOT call this to output to the last column of the last row. */
/* ONLY call this for chars which will move the cursor */
void ansi_sendch(char ch)
{
	if(!ch)
		ch=' ';
	ansi_col++;
	if(ansi_col>=ansi_cols) {
		/* Column 80 sux0rz */
		force_move=1;
		ansi_col=0;
		ansi_row++;
		if(ansi_row>=ansi_rows) {
			ansi_col=ansi_cols-1;
			ansi_row=ansi_rows-1;
		}
	}
	fwrite(&ch,1,1,stdout);
	/* We sent a control char... better make the next movement explicit */
	if(ch<' ' && ch > 0)
		force_move=1;
}

void ansi_sendstr(char *str,int len)
{
	if(len==-1)
		len=strlen(str);
	if(len) {
		fwrite(str,len,1,stdout);
	}
}

int ansi_puttext(int sx, int sy, int ex, int ey, void* buf)
{
	int x,y;
	int cx,cy,i,j;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;
	int		attrib;
	unsigned char *fill = (unsigned char*)buf;

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
	attrib=ti.attribute;

	i=0;		/* Did a nasty. */

	/* Check if this is a nasty screen clear... */
	j=0;		/* We can clearscreen */
	if(!i && sx==1 && sy==1 && ex==ti.screenwidth && ey==ti.screenheight && (*out==' ' || *out==0)) {
		j=1;		/* We can clearscreen */
		for(cy=sy-1;cy<ey;cy++) {
			for(cx=sx-1;cx<ex;cx++) {
				if(out[(cy*ti.screenwidth+cx)*2]!=*out
						|| out[(cy*ti.screenwidth+cx)*2+1]!=*(out+1)) {
					j=0;
					cx=ex;
					cy=ey;
				}
			}
		}
	}
	if(j) {
		textattr(*(out+1));
		/* Many terminals make ESC[2J home the cursor */
		ansi_sendstr("\033[2J\033[1;1H",-1);
		ansi_col=0;
		ansi_row=0;
		memcpy(ansivmem,out,ti.screenwidth*ti.screenheight*2);
		i=1;
	}
#if 0
	/* Check if this is a scroll */
	if(sx==1 && sy==1 && ex==ti.screenwidth && ey==ti.screenheight-1
			&& memcmp(buf,ansivmem,ti.screenwidth*(ti.screenheight-1)*2)==0) {
		/* We need to get to the bottom line... */
		if(ansi_row < ti.screenheight-1) {
			if(ansi_row > ti.screenheight-5) {
				ansi_sendstr("\n\n\n\n\n",ti.screenheight-ansi_row-1);
			}
			else {
				sprintf(str,"\033[%dB",ti.screenheight-ansi_row-1);
				ansi_sendstr(str,-1);
			}
		}
		ansi_sendstr("\n",1);
		memcpy(ansivmem,buf,ti.screenwidth*(ti.screenheight-1)*2);
		for(x=0;x<ti.screenwidth;x++)
			ansivmem[(ti.screenheight-1)*ti.screenwidth+x]=(ti.attribute<<8)|' ';
		i=1;
	}
#endif
#if 1
	/* Check if this *includes* a scroll */
	if(sx==1 && sy==1 && ex==ti.screenwidth && ey==ti.screenheight
			&& memcmp(buf,ansivmem,ti.screenwidth*(ti.screenheight-1)*2)==0) {
		/* We need to get to the bottom line... */
		if(ansi_row < ti.screenheight-1) {
			if(ansi_row > ti.screenheight-5) {
				ansi_sendstr("\n\n\n\n\n",ti.screenheight-ansi_row-1);
			}
			else {
				char str[6];
				sprintf(str,"\033[%dB",ti.screenheight-ansi_row-1);
				ansi_sendstr(str,-1);
			}
		}
		ansi_sendstr("\n",1);
		memcpy(ansivmem,buf,ti.screenwidth*(ti.screenheight-1)*2);
		for(x=0;x<ti.screenwidth;x++)
			ansivmem[(ti.screenheight-1)*ti.screenwidth+x]=(ti.attribute<<8)|' ';
		out += ti.screenwidth*(ti.screenheight-1)*2;
		sy=ti.screenheight;
	}
#endif
	if(!i) {
		for(y=sy-1;y<ey;y++) {
			for(x=sx-1;x<ex;x++) {
				/*
				 * Check if we can use clear2eol now... this means  the rest of the
				 * chars on the line are the same attr, and are all spaces or NULLs
				 * Also, if there's less than four chars left, it's not worth it.
				 */
				i=0;	/* number of differing chars from screen */
				j=1;	/* Can use clrtoeol? */
				for(cx=x; cx<ti.screenwidth; cx++) {
					/* Compare to source buffer */
					if(cx<ex) {
						/* a blank? */
						if(*(out+(cx-x)*2)==' ' || *(out+(cx-x)*2)==0) {
							/* same colour? */
							if(*(out+(cx-x)*2+1)==*(out+1)) {
								/* Is it any change? */
								if(*((WORD *)(out+(cx-x)*2)) != ansivmem[y*2+cx])
									/* And it's different! */
									i++;
							}
							else {
								j=0;
								break;
							}
						}
						else {
							j=0;
							break;
						}
					}
					else {
						/* Compare to screen */
						/* a blank? */
						if((ansivmem[y*2+cx]&0xff)!=' ' && (ansivmem[y*2+cx]&0xff)!=0) {
							j=0;
							break;
						}
					}
				}
				if(j && i>3) {
					ansi_gotoxy(x+1,y+1);
					textattr(*(out+1));
					ansi_sendstr("\033[K",-1);
					for(cx=x; cx<ex; cx++) {
						ansivmem[y*ansi_cols+cx]=*(out++);
						ansivmem[y*ansi_cols+cx]|=(*(out++))<<8;
					}
					break;
				}
				sch=*(out++);
				if(sch==27)
					sch=' ';
				if(sch==0)
					sch=' ';
				sch |= (*(out++))<<8;
				if(ansivmem[y*ansi_cols+x]==sch)
					continue;
				ansivmem[y*ansi_cols+x]=sch;
				ansi_gotoxy(x+1,y+1);
				if(y>=ansi_rows-1 && x>=ansi_cols-1)
					continue;
				if(attrib!=sch>>8) {
					textattr(sch>>8);
					attrib=sch>>8;
				}
				ansi_sendch((char)(sch&0xff));
			}
		}
	}

	if(!puttext_can_move)
		gotoxy(ti.curx,ti.cury);
	if(attrib!=ti.attribute)
		textattr(ti.attribute);
	return(1);
}

int ansi_gettext(int sx, int sy, int ex, int ey, void* buf)
{
	int x,y;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;
	unsigned char *fill = (unsigned char*)buf;

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
			sch=ansivmem[y*ansi_cols+x];
			*(out++)=sch & 0xff;
			*(out++)=sch >> 8;
		}
	}
	return(1);
}

void ansi_textattr(int attr)
{
	char str[16];
	int fg,ofg;
	int bg,obg;
	int bl,obl;
	int br,obr;
	int oa;

	str[0]=0;
	if(ansi_curr_attr==attr<<8)
		return;

	bl=attr&0x80;
	bg=(attr>>4)&0x7;
	fg=attr&0x07;
	br=attr&0x08;

	oa=ansi_curr_attr>>8;
	obl=oa&0x80;
	obg=(oa>>4)&0x7;
	ofg=oa&0x07;
	obr=oa&0x08;

	ansi_curr_attr=attr<<8;

	strcpy(str,"\033[");
	if(obl!=bl) {
		if(!bl)
#if 0
			strcat(str,"25;");
#else
		{
			strcat(str,"0;");
			ofg=7;
			obg=0;
			obr=0;
		}
#endif
		else
			strcat(str,"5;");
	}
	if(br!=obr) {
		if(br)
			strcat(str,"1;");
		else
#if 0
			strcat(str,"22;");
#else
		{
			strcat(str,"0;");
			if(bl)
				strcat(str,"5;");
			ofg=7;
			obg=0;
		}
#endif
	}
	if(fg!=ofg)
		sprintf(str+strlen(str),"3%d;",ansi_colours[fg]);
	if(bg!=obg)
		sprintf(str+strlen(str),"4%d;",ansi_colours[bg]);
	str[strlen(str)-1]='m';
	ansi_sendstr(str,-1);
}

#if defined(__BORLANDC__)
        #pragma argsused
#endif
static void ansi_keyparse(void *par)
{
	int		gotesc=0;
	char	seq[64];
	int		ch;
	int		i;
	char	*p;
	int		timeout=0;
	int		timedout=0;
	int		unknown=0;

	seq[0]=0;
	for(;;) {
		if(ansi_rows != -1)
			sem_wait(&goahead);
		if(timedout || unknown) {
			for(p=seq;*p;p++) {
				ansi_inch=*p;
				sem_post(&got_input);
				sem_wait(&used_input);
				sem_wait(&goahead);
			}
			gotesc=0;
			timeout=0;
			seq[0]=0;
		}
		if(!timedout)
			sem_post(&need_key);
		timedout=0;
		unknown=0;
		if(timeout) {
			if(sem_trywait_block(&got_key,timeout)) {
				timedout=1;
				sem_post(&goahead);
				continue;
			}
		}
		else
			sem_wait(&got_key);

		ch=ansi_raw_inch;

		switch(gotesc) {
			case 1:	/* Escape Sequence */
				timeout=CIOLIB_ANSI_TIMEOUT;
				seq[strlen(seq)+1]=0;
				seq[strlen(seq)]=ch;
				if(strlen(seq)>=sizeof(seq)-2) {
					gotesc=0;
					break;
				}
				if((ch<'0' || ch>'9')		/* End of ESC sequence */
						&& ch!=';'
						&& ch!='?'
						&& (strlen(seq)==2?ch != '[':1)
						&& (strlen(seq)==2?ch != 'O':1)) {
					unknown=1;
					gotesc=0;
					timeout=0;
					for(i=0;ODaKeySequences[i].pszSequence[0];i++) {
						if(!strcmp(seq,ODaKeySequences[i].pszSequence)) {
							ansi_inch=ODaKeySequences[i].chExtendedKey;
							sem_post(&got_input);
							/* Two-byte code, need to post twice and wait for one to
							   be received */
							sem_wait(&used_input);
							sem_wait(&goahead);
							sem_post(&got_input);
							sem_wait(&used_input);
							unknown=0;
							seq[0]=0;
							break;
						}
					}
					/* ANSI position report? */
					if(ch=='R') {
						if(strspn(seq,"\033[0123456789;R")==strlen(seq)) {
							p=seq+2;
							i=strtol(p,&p,10);
							if(i>ansi_rows)
								ansi_rows=i;
						}
						unknown=0;
					}
					if(unknown) {
						sem_post(&goahead);
						continue;
					}
				}
				else {
					/* Need more keys... keep looping */
					sem_post(&goahead);
				}
				break;
			default:
				if(ch==27) {
					seq[0]=27;
					seq[1]=0;
					gotesc=1;
					timeout=CIOLIB_ANSI_TIMEOUT;
					/* Need more keys... keep going... */
					sem_post(&goahead);
					break;
				}
				if(ch==10) {
					/* The \n that goes with the prev \r (hopefully) */
					/* Eat it and keep chuggin' */
					sem_post(&goahead);
					break;
				}
				ansi_inch=ch;
				sem_post(&got_input);
				sem_wait(&used_input);
				break;
		}
	}
}

#if defined(__BORLANDC__)
        #pragma argsused
#endif
static void ansi_keythread(void *params)
{
	int	sval=1;

	_beginthread(ansi_keyparse,1024,NULL);

	for(;;) {
		sem_wait(&need_key);
		/* If you already have a key, don't get another */
		sem_getvalue(&got_key,&sval);
		if((!sval) && fread(&ansi_raw_inch,1,1,stdin)==1)
			sem_post(&got_key);
		else
			SLEEP(1);
	}
}

int ansi_kbhit(void)
{
	int	sval=1;

	if(!sent_ga) {
		sem_post(&goahead);
		sent_ga=TRUE;
	}
	sem_getvalue(&got_input,&sval);
	return(sval);
}

void ansi_delay(long msec)
{
	SLEEP(msec);
}

int ansi_wherey(void)
{
	return(ansi_row+1);
}

int ansi_wherex(void)
{
	return(ansi_col+1);
}

/* Put the character _c on the screen at the current cursor position. 
 * The special characters return, linefeed, bell, and backspace are handled
 * properly, as is line wrap and scrolling. The cursor position is updated. 
 */
int ansi_putch(int ch)
{
	struct text_info ti;
	int i;
	unsigned char buf[2];

	buf[0]=ch;
	buf[1]=ansi_curr_attr>>8;

	gettextinfo(&ti);
	puttext_can_move=1;

	switch(ch) {
		case '\r':
			gotoxy(1,wherey());
			break;
		case '\n':
			if(wherey()==ti.winbottom-ti.wintop+1)
				wscroll();
			else
				gotoxy(wherex(),wherey()+1);
			break;
		case '\b':
			if(ansi_col>ti.winleft-1) {
				buf[0]=' ';
				gotoxy(wherex()-1,wherey());
				puttext(ansi_col+1,ansi_row+1,ansi_col+1,ansi_row+1,buf);
			}
			break;
		case 7:		/* Bell */
			ansi_sendstr("\007",1);
			break;
		case '\t':
			for(i=0;i<10;i++) {
				if(ansi_tabs[i]>ansi_col+1) {
					while(ansi_col+1<ansi_tabs[i]) {
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
			if(wherey()==ti.winbottom-ti.wintop+1
					&& wherex()==ti.winright-ti.winleft+1) {
				gotoxy(1,wherey());
				puttext(ansi_col+1,ansi_row+1,ansi_col+1,ansi_row+1,buf);
				wscroll();
			}
			else {
				if(wherex()==ti.winright-ti.winleft+1) {
					gotoxy(1,ti.cury+1);
					puttext(ansi_col+1,ansi_row+1,ansi_col+1,ansi_row+1,buf);
				}
				else {
					puttext(ansi_col+1,ansi_row+1,ansi_col+1,ansi_row+1,buf);
					gotoxy(ti.curx+1,ti.cury);
				}
			}
			break;
	}

	puttext_can_move=0;
	return(ch);
}

void ansi_gotoxy(int x, int y)
{
	char str[16];

	str[0]=0;
	if(x < 1
		|| x > ansi_cols
		|| y < 1
		|| y > ansi_rows)
		return;

	/* ToDo optimizations: use tabs for horizontal movement to tabstops */

	/* Movement forced... always send position code */
	if(force_move) {
		sprintf(str,"\033[%d;%dH",y,x);
		ansi_sendstr(str,-1);
		force_move=0;
		ansi_row=y-1;
		ansi_col=x-1;
		return;
	}

	/* Moving to col 1 (and not already there)... use \r */
	if(x==1 && ansi_col) {
		ansi_sendstr("\r",1);
		ansi_col=0;
	}

	/* Do we even NEED to move? */
	if(x==ansi_col+1 && y==ansi_row+1)
		return;

	/* If we're already on the correct column */
	if(x==ansi_col+1) {
		/* Do we need to move up? */
		if(y<ansi_row+1) {
			if(y==ansi_row)
				/* Only up one */
				strcpy(str,"\033[A");
			else
				sprintf(str,"\033[%dA",ansi_row+1-y);
			ansi_sendstr(str,-1);
			ansi_row=y-1;
			return;
		}

		/* We must have to move down then. */
		/* Only one, use a newline */
		if(y==ansi_row+2)
			strcpy(str,"\n");
		else
			sprintf(str,"\033[%dB",y-ansi_row-1);
		ansi_sendstr(str,-1);
		ansi_row=y-1;
		return;
	}

	/* Ok, we need to change the column then... is the row right though? */
	if(y==ansi_row+1) {
		/* Do we need to move left then? */
		if(x<ansi_col+1) {
			if(x==ansi_col)
				strcpy(str,"\033[D");
			else
				sprintf(str,"\033[%dD",ansi_col+1-x);
			ansi_sendstr(str,-1);
			ansi_col=x-1;
			return;
		}

		/* Must need to move right then */
		/* Check if we can use spaces */
		if(x-ansi_col-1 < 4) {
			int i;
			/* If all the intervening cells are spaces with the current BG, we're good */
			for(i=0; i<x-ansi_col-1; i++) {
				if(ansivmem[y*ansi_cols+ansi_col+i-1]!=ansi_curr_attr | ' ')
					break;
			}
			if(i==x-ansi_col-1) {
				ansi_sendstr("    ",x-ansi_col-1);
				return;
			}
		}
		if(x==ansi_col+2)
			strcpy(str,"\033[C");
		else
			sprintf(str,"\033[%dC",x-ansi_col-1);
		ansi_sendstr(str,-1);
		ansi_col=x-1;
		return;
	}

	/* Changing the row and the column... better use a fill movement then. */
	sprintf(str,"\033[%d;%dH",y,x);
	ansi_sendstr(str,-1);
	ansi_row=y-1;
	ansi_col=x-1;
	return;
}

void ansi_gettextinfo(struct text_info *info)
{
	info->currmode=3;
	info->screenheight=ansi_rows;
	info->screenwidth=ansi_cols;
	info->curx=ansi_wherex();
	info->cury=ansi_wherey();
	info->attribute=ansi_curr_attr>>8;
}

void ansi_setcursortype(int type)
{
	switch(type) {
		case _NOCURSOR:
		case _SOLIDCURSOR:
		default:
			break;
	}
}

int ansi_getch(void)
{
	int ch;

	if(!sent_ga) {
		sem_post(&goahead);
		sent_ga=TRUE;
	}
	sem_wait(&got_input);
	ch=ansi_inch&0xff;
	ansi_inch=ansi_inch>>8;
	sem_post(&used_input);
	sent_ga=FALSE;
	return(ch);
}

int ansi_getche(void)
{
	int ch;

	ch=ansi_getch();
	if(ch)
		putch(ch);
	return(ch);
}

int ansi_beep(void)
{
	putch(7);
	return(0);
}

#if defined(__BORLANDC__)
        #pragma argsused
#endif
void ansi_textmode(int mode)
{
}

#ifdef __unix__
void ansi_fixterm(void)
{
	tcsetattr(STDIN_FILENO,TCSANOW,&tio_default);
}
#endif

#ifndef ENABLE_EXTENDED_FLAGS
#define ENABLE_INSERT_MODE		0x0020
#define ENABLE_QUICK_EDIT_MODE	0x0040
#define ENABLE_EXTENDED_FLAGS	0x0080
#define ENABLE_AUTO_POSITION	0x0100
#endif

#if defined(__BORLANDC__)
        #pragma argsused
#endif
int ansi_initciolib(long inmode)
{
	int i;
	char *init="\033[s\033[99B_\033[6n\033[u\033[0m_\033[2J\033[H";
	time_t start;

#ifdef _WIN32
	if(isatty(fileno(stdin))) {
		if(!SetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), 0))
			return(0);

		if(!SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), 0))
			return(0);
	}
	setmode(fileno(stdout),_O_BINARY);
	setmode(fileno(stdin),_O_BINARY);
	setvbuf(stdout, NULL, _IONBF, 0);
#else
	struct termios tio_raw;

	if (isatty(STDIN_FILENO))  {
		tcgetattr(STDIN_FILENO,&tio_default);
		tio_raw = tio_default;
		/* cfmakeraw(&tio_raw); */
		tio_raw.c_iflag &= ~(IMAXBEL|IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL|IXON);
		tio_raw.c_oflag &= ~OPOST;
		tio_raw.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
		tio_raw.c_cflag &= ~(CSIZE|PARENB);
		tio_raw.c_cflag |= CS8;
		tcsetattr(STDIN_FILENO,TCSANOW,&tio_raw);
		setvbuf(stdout, NULL, _IONBF, 0);
		atexit(ansi_fixterm);
	}
#endif

	sem_init(&got_key,0,0);
	sem_init(&got_input,0,0);
	sem_init(&used_input,0,0);
	sem_init(&goahead,0,0);
	sem_init(&need_key,0,0);

	ansi_sendstr(init,-1);
	_beginthread(ansi_keythread,1024,NULL);
	start=time(NULL);
	while(time(NULL)-start < 5 && ansi_rows==-1)
		SLEEP(1);
	if(ansi_rows==-1)
		ansi_rows=24;
	ansivmem=(WORD *)malloc(ansi_rows*ansi_cols*sizeof(WORD));
	for(i=0;i<ansi_rows*ansi_cols;i++)
		ansivmem[i]=0x0720;
	/* drain all the semaphores */
	sem_reset(&got_key);
	sem_reset(&got_input);
	sem_reset(&used_input);
	sem_reset(&goahead);
	sem_reset(&need_key);
	return(1);
}
