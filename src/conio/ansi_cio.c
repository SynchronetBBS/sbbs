/* $Id: ansi_cio.c,v 1.86 2020/04/13 18:36:21 deuce Exp $ */

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

#include "ciolib.h"
#include "ansi_cio.h"

int	CIOLIB_ANSI_TIMEOUT=500;
int  (*ciolib_ansi_readbyte_cb)(void)=ansi_readbyte_cb;
int  (*ciolib_ansi_writebyte_cb)(const unsigned char ch)=ansi_writebyte_cb;
int  (*ciolib_ansi_initio_cb)(void)=ansi_initio_cb;
int  (*ciolib_ansi_writestr_cb)(const unsigned char *str, size_t len)=ansi_writestr_cb;

static sem_t	got_key;
static sem_t	got_input;
static sem_t	used_input;
static sem_t	goahead;
static sem_t	need_key;
static BOOL	sent_ga=FALSE;
static int ansix=1;
static int ansiy=1;

static int ansi_got_row=0;
static int doorway_enabled=0;

const int 	ansi_colours[8]={0,4,2,6,1,5,3,7};
static WORD		ansi_inch;
static int		ansi_raw_inch;
static WORD	*ansivmem;
static int		force_move=1;

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
#define ANSI_KEY_F11	0x85<<8
#define ANSI_KEY_F12	0x86<<8

static tODKeySequence ODaKeySequences[] =
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
   {"\033[1~",  ANSI_KEY_HOME}, /* Windows XP terminal.exe.  Is actually FIND */
   {"\033[2~",  ANSI_KEY_INSERT},
   {"\033[3~",  ANSI_KEY_DELETE},
   {"\033[4~",  ANSI_KEY_END},  /* Windows XP terminal.exe.  Is actually SELECT */
   {"\033[5~",  ANSI_KEY_PGUP},
   {"\033[6~",  ANSI_KEY_PGDN},
   {"\033[17~", ANSI_KEY_F6},
   {"\033[18~", ANSI_KEY_F7},
   {"\033[19~", ANSI_KEY_F8},
   {"\033[20~", ANSI_KEY_F9},
   {"\033[21~", ANSI_KEY_F10},
   {"\033[23~", ANSI_KEY_F11},
   {"\033[24~", ANSI_KEY_F12},

   /* XTerm specific control sequences */
   {"\033[15~", ANSI_KEY_F5},

   /* Old, deprecated XTerm specific control sequences */
   {"\033[11~", ANSI_KEY_F1},
   {"\033[12~", ANSI_KEY_F2},
   {"\033[13~", ANSI_KEY_F3},
   {"\033[14~", ANSI_KEY_F4},

   /* ANSI-specific control sequences. */
   {"\033[L", ANSI_KEY_HOME},
   {"\033Ow", ANSI_KEY_F3},
   {"\033Ox", ANSI_KEY_F4},
   {"\033Ot", ANSI_KEY_F5},
   {"\033Ou", ANSI_KEY_F6},
   {"\033Oq", ANSI_KEY_F7},
   {"\033Or", ANSI_KEY_F8},
   {"\033Op", ANSI_KEY_F9},

   /* ECMA 048-specific control sequences. */
   {"\033[V", ANSI_KEY_PGUP},
   {"\033[U", ANSI_KEY_PGDN},
   {"\033[@", ANSI_KEY_INSERT},
   
   
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
static void
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
static void ansi_sendch(char ch)
{
	if(!ch)
		ch=' ';
	ansix++;
	if(ansix>cio_textinfo.screenwidth) {
		/* Column 80 sux0rz */
		force_move=1;
		ansix=1;
		ansiy++;
		if(ansiy>cio_textinfo.screenheight) {
			ansix=cio_textinfo.screenwidth;
			ansiy=cio_textinfo.screenheight;
		}
	}
	if(doorway_enabled && ch < ' ')
		ciolib_ansi_writebyte_cb(0);
	ciolib_ansi_writebyte_cb((unsigned char)ch);
	/* We sent a control char... better make the next movement explicit */
	if(ch<' ' && ch > 0) {
		if(doorway_enabled) {
			/* In doorway mode, some chars may want to force movement... */
		}
		else
			force_move=1;
	}
}

static void ansi_sendstr(const char *str,int len)
{
	if(len==-1)
		len=strlen(str);
	if(len)
		ciolib_ansi_writestr_cb((unsigned char *)str, (size_t)len);
}

static void ansi_gotoxy_abs(int x, int y)
{
	char str[16];

	str[0]=0;
	if(x < 1
		|| x > cio_textinfo.screenwidth
		|| y < 1
		|| y > cio_textinfo.screenheight)
		return;

	/* ToDo optimizations: use tabs for horizontal movement to tabstops */

	/* Movement forced... always send position code */
	if(force_move) {
		sprintf(str,"\033[%d;%dH",y,x);
		ansi_sendstr(str,-1);
		force_move=0;
		ansiy=y;
		ansix=x;
		return;
	}

	/* Moving to col 1 (and not already there)... use \r */
	if(x==1 && ansix>1) {
		ansi_sendstr("\r",1);
		ansix=1;
	}

	/* Do we even NEED to move? */
	if(x==ansix && y==ansiy)
		return;

	/* If we're already on the correct column */
	if(x==ansix) {
		/* Do we need to move up? */
		if(y<ansiy) {
			if(y==ansiy-1)
				/* Only up one */
				strcpy(str,"\033[A");
			else
				sprintf(str,"\033[%dA",ansiy-y);
			ansi_sendstr(str,-1);
			ansiy=y;
			return;
		}

		/* We must have to move down then. */
		/* Only one, use a newline */
		if(y-ansiy < 4)
			ansi_sendstr("\n\n\n",y-ansiy);
		else {
			sprintf(str,"\033[%dB",y-ansiy);
			ansi_sendstr(str,-1);
		}
		ansiy=y;
		return;
	}

	/* Ok, we need to change the column then... is the row right though? */
	if(y==ansiy) {
		/* Do we need to move left then? */
		if(x<ansix) {
			if(x==ansix-1)
				strcpy(str,"\033[D");
			else
				sprintf(str,"\033[%dD",ansix-x);
			ansi_sendstr(str,-1);
			ansix=x;
			return;
		}

		/* Must need to move right then */
#if 1
		/* Check if we can use spaces */
		if(x-ansix < 5) {
			int i,j;
			j=1;
			/* If all the intervening cells are spaces with the current background, we're good */
			for(i=0; i<x-ansix; i++) {
				if((ansivmem[(y-1)*cio_textinfo.screenwidth+ansix-1+i] & 0xff) != ' '/* && (ansivmem[(ansiy-1)*cio_textinfo.screenwidth+ansix-1+i]) & 0xff != 0*/) {
					j=0;
					break;
				}
				if((ansivmem[(y-1)*cio_textinfo.screenwidth+ansix-1+i] & 0x7000) != ((cio_textinfo.attribute<<8) & 0x7000)) {
					j=0;
					break;
				}
			}
			if(j) {
				ansi_sendstr("    ",x-ansix);
				ansix=x;
				return;
			}
		}
#endif
		if(x==ansix+1)
			strcpy(str,"\033[C");
		else
			sprintf(str,"\033[%dC",x-ansix);
		ansi_sendstr(str,-1);
		ansix=x;
		return;
	}

	/* Changing the row and the column... better use a fill movement then. */
	sprintf(str,"\033[%d;%dH",y,x);
	ansi_sendstr(str,-1);
	ansiy=y;
	ansix=x;
	return;
}

void ansi_gotoxy(int x, int y)
{
	ansi_gotoxy_abs(x+cio_textinfo.winleft-1,y+cio_textinfo.wintop-1);
	cio_textinfo.curx=x;
	cio_textinfo.cury=y;
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

	/* Check if this actually does anything */
	/* TODO: This assumes a little-endian system!  Ack! */
	if(sx==1 && sy==1 && ex==ti.screenwidth && ey==ti.screenheight
			&& memcmp(buf,ansivmem,ti.screenwidth*ti.screenheight*2)==0)
		return(1);

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
		ansix=1;
		ansiy=1;
		memcpy(ansivmem,out,ti.screenwidth*ti.screenheight*2);
		i=1;
	}
#if 0
	/* Check if this is a scroll */
	if((!i) && sx==1 && sy==1 && ex==ti.screenwidth && ey==ti.screenheight-1
			&& memcmp(buf,ansivmem,ti.screenwidth*(ti.screenheight-1)*2)==0) {
		/* We need to get to the bottom line... */
		if(ansiy < ti.screenheight) {
			if(ansiy > ti.screenheight-4) {
				ansi_sendstr("\n\n\n\n\n",ti.screenheight-ansiy-2);
			}
			else {
				sprintf(str,"\033[%dB",ti.screenheight-ansiy-2);
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
	if((!i) && sx==1 && sy==1 && ex==ti.screenwidth && ey==ti.screenheight
			&& memcmp(buf,ansivmem+ti.screenwidth,ti.screenwidth*(ti.screenheight-1)*2)==0) {
		/* We need to get to the bottom line... */
		if(ansiy < ti.screenheight) {
			if(ansiy > ti.screenheight-4) {
				ansi_sendstr("\n\n\n\n\n",ti.screenheight-ansiy-2);
			}
			else {
				char str[6];
				sprintf(str,"\033[%dB",ti.screenheight-ansiy-2);
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
				j=0;	/* Can use clrtoeol? */
				for(cx=x; cx<ti.screenwidth; cx++) {
					/* First, make sure that the rest are spaces or NULLs */
					if(out[(cx-x)*2] != ' ' && out[(cx-x)*2] != 0)
						break;
					/* Next, make sure that the attribute is the same */
					if(out[(cx-x)*2+1] != out[1])
						break;
					/* Finally, if this isn't what's on screen, increment i */
					if((ansivmem[y*cio_textinfo.screenwidth+cx] & 0xff) != 0 && (ansivmem[y*cio_textinfo.screenwidth+cx] & 0xff) != ' ')
						i++;
					else if(ansivmem[y*cio_textinfo.screenwidth+cx] >> 8 != out[(cx-x)*2+1])
						i++;
				}
				if(cx==ti.screenwidth)
					j=1;

				if(j && i>3) {
					ansi_gotoxy_abs(x+1,y+1);
					textattr(*(out+1));
					ansi_sendstr("\033[K",-1);
					for(cx=x; cx<ex; cx++) {
						ansivmem[y*cio_textinfo.screenwidth+cx]=*(out++);
						ansivmem[y*cio_textinfo.screenwidth+cx]|=(*(out++))<<8;
					}
					break;
				}
				sch=*(out++);
				if(sch==27 && doorway_enabled==0)
					sch=' ';
				if(sch==0)
					sch=' ';
				sch |= (*(out++))<<8;
				if(ansivmem[y*cio_textinfo.screenwidth+x]==sch)
					continue;
				ansivmem[y*cio_textinfo.screenwidth+x]=sch;
				ansi_gotoxy_abs(x+1,y+1);
				if(y>=cio_textinfo.screenheight-1 && x>=cio_textinfo.screenwidth-1)
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
			sch=ansivmem[y*cio_textinfo.screenwidth+x];
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
	if(cio_textinfo.attribute==attr)
		return;

	bl=attr&0x80;
	bg=(attr>>4)&0x7;
	fg=attr&0x07;
	br=attr&0x08;

	oa=cio_textinfo.attribute;
	obl=oa&0x80;
	obg=(oa>>4)&0x7;
	ofg=oa&0x07;
	obr=oa&0x08;

	cio_textinfo.attribute=attr;

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
	cio_textinfo.attribute=attr;
}

#if defined(__BORLANDC__)
        #pragma argsused
#endif
static void ansi_keyparse(void *par)
{
	int		gotesc=0;
	int		gotnull=0;
	char	seq[64];
	int		ch;
	int		i;
	char	*p;
	int		timeout=0;
	int		timedout=0;
	int		unknown=0;

	SetThreadName("ANSI Keyparse");
	seq[0]=0;
	for(;;) {
		if(ansi_got_row)
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
		if(ch==-2) {
			ansi_inch=0x0100;
			sem_post(&got_input);
			/* Two-byte code, need to post twice times and wait for one to
			   be received */
			sem_wait(&used_input);
			sem_wait(&goahead);
			sem_post(&got_input);
			sem_wait(&used_input);
		}
		if(gotnull==2) {
			// 0xe0 enhanced keyboard key... translate to 0x00 key for now.

			ansi_inch=ch<<8;	// (ch<<8)|0xe0;
			sem_post(&got_input);
			/* Two-byte code, need to post twice times and wait for one to
			   be received */
			sem_wait(&used_input);
			sem_wait(&goahead);
			sem_post(&got_input);
			sem_wait(&used_input);
			gotnull=0;
			continue;
		}
		if(gotnull==1) {
			if(ch==0xe0) {
				gotnull=2;
				// Need another key... keep looping.
				sem_post(&goahead);
				continue;
			}
			ansi_inch=ch<<8;
			sem_post(&got_input);
			/* Two-byte code, need to post twice and wait for one to
			   be received */
			sem_wait(&used_input);
			sem_wait(&goahead);
			sem_post(&got_input);
			sem_wait(&used_input);
			gotnull=0;
			continue;
		}

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
							if(i>cio_textinfo.screenheight) {
								cio_textinfo.screenheight=i;
								if(*p==';') {
									i=strtol(p+1, NULL, 10);
									if(i>cio_textinfo.screenwidth)
										cio_textinfo.screenwidth=i;
								}
								ansi_got_row=cio_textinfo.screenheight;;
							}
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
				if(doorway_enabled && ch==0) {
					/* Got a NULL. ASSume this is a doorway mode char */
					gotnull=1;
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

	SetThreadName("ANSI Key");
	_beginthread(ansi_keyparse,1024,NULL);

	for(;;) {
		sem_wait(&need_key);
		/* If you already have a key, don't get another */
		sem_getvalue(&got_key,&sval);
		if(!sval) {
			ansi_raw_inch=ciolib_ansi_readbyte_cb();
			if(ansi_raw_inch >= 0 || ansi_raw_inch==-2)
				sem_post(&got_key);
			else
				SLEEP(1);
		}
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

void ansi_beep(void)
{
	ansi_sendstr("\7",1);
}

#if defined(__BORLANDC__)
        #pragma argsused
#endif
void ansi_textmode(int mode)
{
	cio_textinfo.winleft=1;
	cio_textinfo.wintop=1;
	cio_textinfo.winright=cio_textinfo.screenwidth;
	cio_textinfo.winbottom=cio_textinfo.screenheight;
	cio_textinfo.attribute=7;
	cio_textinfo.normattr=7;
	cio_textinfo.currmode=COLOR_MODE;
	cio_textinfo.curx=1;
	cio_textinfo.cury=1;
	ansix=1;
	ansiy=1;
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

/*
 * Returns -1 if no character read or -2 on abort
 */
int ansi_readbyte_cb(void)
{
	unsigned char ch;

	if(fread(&ch,1,1,stdin)!=1)
		return(-1);
	return(ch);
}

int ansi_writebyte_cb(const unsigned char ch)
{
	return(fwrite(&ch,1,1,stdout));
}

int ansi_writestr_cb(const unsigned char *str, size_t len)
{
	return(fwrite(str,len,1,stdout));
}

int ansi_initio_cb(void)
{
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
		cfmakeraw(&tio_raw);
		tcsetattr(STDIN_FILENO,TCSANOW,&tio_raw);
		setvbuf(stdout, NULL, _IONBF, 0);
		atexit(ansi_fixterm);
	}
#endif
	return(0);
}

#if defined(__BORLANDC__)
        #pragma argsused
#endif
int ansi_initciolib(long inmode)
{
	int i;
	char *init="\033[s\033[99B\033[99B\033[99B_\033[99C\033[99C\033[99C_\033[6n\033[u\033[0m_\033[2J\033[H";
	time_t start;

	cio_api.options = 0;

	ansi_textmode(1);
	cio_textinfo.screenheight=24;
	cio_textinfo.screenwidth=80;
	ciolib_ansi_initio_cb();

	sem_init(&got_key,0,0);
	sem_init(&got_input,0,0);
	sem_init(&used_input,0,0);
	sem_init(&goahead,0,0);
	sem_init(&need_key,0,0);

	ansi_sendstr(init,-1);
	_beginthread(ansi_keythread,1024,NULL);
	start=time(NULL);
	while(time(NULL)-start < 5 && !ansi_got_row)
		SLEEP(1);
	if(!ansi_got_row) {
		cio_textinfo.screenheight=24;
		cio_textinfo.screenwidth=80;
		ansi_got_row=24;
	}
	ansivmem=(WORD *)malloc(cio_textinfo.screenheight*cio_textinfo.screenwidth*sizeof(WORD));
	for(i=0;i<cio_textinfo.screenheight*cio_textinfo.screenwidth;i++)
		ansivmem[i]=0x0720;
	/* drain all the semaphores */
	sem_reset(&got_key);
	sem_reset(&got_input);
	sem_reset(&used_input);
	sem_reset(&goahead);
	sem_reset(&need_key);
	return(1);
}

CIOLIBEXPORT void CIOLIBCALL ansi_ciolib_setdoorway(int enable)
{
	if(cio_api.mode!=CIOLIB_MODE_ANSI)
		return;
	switch(enable) {
	case 0:
		ansi_sendstr("\033[=255l",7);
		doorway_enabled=0;
		break;
	default:
		ansi_sendstr("\033[=255h",7);
		doorway_enabled=1;
		break;
	}
}
