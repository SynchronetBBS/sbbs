/* $Id: */

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

#include <stdlib.h>
#include <string.h>

#include <genwrap.h>
#include <ciolib.h>
#include <keys.h>

#include "cterm.h"

#define	BUFSIZE	2048

struct cterminal cterm;

/* const int tabs[11]={1,8,16,24,32,40,48,56,64,72,80}; */
const int cterm_tabs[11]={9,17,25,33,41,49,57,65,73,80,80.1};

void play_music(void)
{
	/* ToDo Music code parsing stuff */
	cterm.music=0;
}

void scrolldown(void)
{
	char *buf;
	int i,j;

	buf=(char *)malloc(cterm.width*(cterm.height-1)*2);
	gettext(cterm.x,cterm.y,cterm.x+cterm.width-1,cterm.y+cterm.height-2,buf);
	puttext(cterm.x,cterm.y+1,cterm.x+cterm.width-1,cterm.y+cterm.height-1,buf);
	j=0;
	for(i=0;i<cterm.width;i++) {
		buf[j++]=' ';
		buf[j++]=cterm.attr;
	}
	puttext(cterm.x,cterm.y,cterm.x+cterm.width-1,cterm.y,buf);
	free(buf);
}

void scrollup(void)
{
	char *buf;
	int i,j;

	cterm.backpos++;
	if(cterm.scrollback!=NULL) {
		if(cterm.backpos>cterm.backlines) {
			memmove(cterm.scrollback,cterm.scrollback+cterm.width*2,cterm.width*2*(cterm.backlines-1));
			cterm.backpos--;
		}
		gettext(cterm.x,cterm.y,cterm.x+cterm.width-1,cterm.y,cterm.scrollback+(cterm.backpos-1)*cterm.width*2);
	}
	buf=(char *)malloc(cterm.width*(cterm.height-1)*2);
	gettext(cterm.x,cterm.y+1,cterm.x+cterm.width-1,cterm.y+cterm.height-1,buf);
	puttext(cterm.x,cterm.y,cterm.x+cterm.width-1,cterm.y+cterm.height-2,buf);
	j=0;
	for(i=0;i<cterm.width;i++) {
		buf[j++]=' ';
		buf[j++]=cterm.attr;
	}
	puttext(cterm.x,cterm.y+cterm.height-1,cterm.x+cterm.width-1,cterm.y+cterm.height-1,buf);
	free(buf);
}

void clear2bol(void)
{
	char *buf;
	int i,j;

	buf=(char *)malloc((wherex()+1)*2);
	j=0;
	for(i=1;i<=wherex();i++) {
		buf[j++]=' ';
		buf[j++]=cterm.attr;
	}
	puttext(cterm.x+1,cterm.y+wherey(),cterm.x+wherex(),cterm.y+wherey(),buf);
	free(buf);
}

void clear2eol(void)
{
	char *buf;
	int i,j;

	clreol();
}

void clearscreen(char attr)
{
	char *buf;
	int x,y,j;

	if(cterm.scrollback!=NULL) {
		cterm.backpos+=cterm.height;
		if(cterm.backpos>cterm.backlines) {
			memmove(cterm.scrollback,cterm.scrollback+cterm.width*2*(cterm.backpos-cterm.backlines),cterm.width*2*(cterm.backlines-(cterm.backpos-cterm.backlines)));
			cterm.backpos=cterm.backlines;
		}
		gettext(cterm.x,cterm.y,cterm.x+cterm.width-1,cterm.y+cterm.height-1,cterm.scrollback+(cterm.backpos-cterm.height)*cterm.width*2);
	}
	clrscr();
}

void do_ansi(char *retbuf, int retsize)
{
	char	*p;
	char	*p2;
	char	tmp[1024];
	int		i,j,k;
	int		row,col;

	switch(cterm.escbuf[0]) {
		case '[':
			/* ANSI stuff */
			p=cterm.escbuf+strlen(cterm.escbuf)-1;
			switch(*p) {
				case '@':	/* Insert Char */
					i=wherex();
					j=wherey();
					gettext(cterm.x+wherex(),cterm.y+wherey(),cterm.x+cterm.width-1,cterm.y+wherey(),tmp);
					putch(' ');
					puttext(cterm.x+wherex()+1,cterm.y+wherey(),cterm.x+cterm.width,cterm.y+wherey(),tmp);
					gotoxy(i,j);
					break;
				case 'A':	/* Cursor Up */
					i=atoi(cterm.escbuf+1);
					if(i==0)
						i=1;
					i=wherey()-i;
					if(i<1)
						i=1;
					gotoxy(wherex(),i);
					break;
				case 'B':	/* Cursor Down */
					i=atoi(cterm.escbuf+1);
					if(i==0)
						i=1;
					i=wherey()+i;
					if(i>cterm.height)
						i=cterm.height;
					gotoxy(wherex(),i);
					break;
				case 'C':	/* Cursor Right */
					i=atoi(cterm.escbuf+1);
					if(i==0)
						i=1;
					i=wherex()+i;
					if(i>cterm.width)
						i=cterm.width;
					gotoxy(i,wherey());
					break;
				case 'D':	/* Cursor Left */
					i=atoi(cterm.escbuf+1);
					if(i==0)
						i=1;
					i=wherex()-i;
					if(i<1)
						i=1;
					gotoxy(i,wherey());
					break;
				case 'E':
					i=atoi(cterm.escbuf+1);
					if(i==0)
						i=1;
					i=wherey()+i;
					for(j=0;j<i;j++)
						putch('\n');
					break;
				case 'f':
				case 'H':
					row=1;
					col=1;
					*(p--)=0;
					if(strlen(cterm.escbuf)>1) {
						if((p=strtok(cterm.escbuf+1,";"))!=NULL) {
							row=atoi(p);
							if((p=strtok(NULL,";"))!=NULL) {
								col=atoi(p);
							}
						}
					}
					if(row<1)
						row=1;
					if(col<1)
						col=1;
					if(row>cterm.height)
						row=cterm.height;
					if(col>cterm.width)
						col=cterm.width;
					gotoxy(col,row);
					break;
				case 'J':
					i=atoi(cterm.escbuf+1);
					switch(i) {
						case 0:
							clear2eol();
							p2=(char *)malloc(cterm.width*2);
							j=0;
							for(i=0;i<cterm.width;i++) {
								p2[j++]=' ';
								p2[j++]=cterm.attr;
							}
							for(i=wherey()+1;i<=cterm.height;i++) {
								puttext(cterm.x+1,cterm.y+i,cterm.x+cterm.width,cterm.y+i,p2);
							}
							free(p2);
							break;
						case 1:
							clear2bol();
							p2=(char *)malloc(cterm.width*2);
							j=0;
							for(i=0;i<cterm.width;i++) {
								p2[j++]=' ';
								p2[j++]=cterm.attr;
							}
							for(i=wherey()-1;i>=1;i--) {
								puttext(cterm.x+1,cterm.y+i,cterm.x+cterm.width,cterm.y+i,p2);
							}
							free(p2);
							break;
						case 2:
							clearscreen(cterm.attr);
							gotoxy(1,1);
							break;
					}
					break;
				case 'K':
					i=atoi(cterm.escbuf+1);
					switch(i) {
						case 0:
							clear2eol();
							break;
						case 1:
							clear2bol();
							break;
						case 2:
							p2=(char *)malloc(cterm.width*2);
							j=0;
							for(i=0;i<cterm.width;i++) {
								p2[j++]=' ';
								p2[j++]=cterm.attr;
							}
							puttext(cterm.x+1,cterm.y+wherey(),cterm.x+cterm.width,cterm.y+wherey(),p2);
							free(p2);
							break;
					}
					break;
				case 'L':
					i=atoi(cterm.escbuf+1);
					if(i==0)
						i=1;
					if(i>cterm.height-wherey())
						i=cterm.height-wherey();
					if(i<cterm.height-wherey()) {
						p2=(char *)malloc((cterm.height-wherey()-i)*cterm.width*2);
						gettext(cterm.x+1,cterm.y+wherey(),cterm.x+cterm.width,wherey()+(cterm.height-wherey()-i),p2);
						puttext(cterm.x+1,cterm.y+wherey()+i,cterm.x+cterm.width,wherey()+(cterm.height-wherey()),p2);
						j=0;
						free(p2);
					}
					p2=(char *)malloc(cterm.width*2);
					j=0;
					for(k=0;k<cterm.width;k++) {
						p2[j++]=' ';
						p2[j++]=cterm.attr;
					}
					for(i=0;j<i;i++) {
						puttext(cterm.x+1,cterm.y+i,cterm.x+cterm.width,cterm.y+i,p2);
					}
					free(p2);
					break;
				case 'M':
				case 'N':
					cterm.music=1;
					break;
				case 'P':	/* Delete char */
					i=atoi(cterm.escbuf+1);
					if(i==0)
						i=1;
					if(i>cterm.width-wherex())
						i=cterm.width-wherex();
					p2=(char *)malloc((cterm.width-wherex())*2);
					gettext(cterm.x+wherex(),cterm.y+wherey(),cterm.x+cterm.width,cterm.y+wherey(),p2);
					memmove(p2,p2+(i*2),(cterm.width-wherex()-i)*2);
					for(i=(cterm.width-wherex())*2-2;i>=wherex();i-=2)
						p2[i]=' ';
					puttext(cterm.x+wherex(),cterm.y+wherey(),cterm.x+cterm.width,cterm.y+wherey(),p2);
					break;
				case 'S':
					scrollup();
					break;
				case 'T':
					scrolldown();
					break;
				case 'U':
					clearscreen(7);
					gotoxy(1,1);
					break;
				case 'Y':	/* ToDo? BananaCom Clear Line */
					break;
				case 'Z':
					for(j=10;j>=0;j--) {
						if(cterm_tabs[j]<wherex()) {
							gotoxy(cterm_tabs[j],wherey());
							break;
						}
					}
					break;
				case 'b':	/* ToDo?  Banana ANSI */
					break;
				case 'g':	/* ToDo?  VT100 Tabs */
					break;
				case 'h':	/* ToDo?  Scrolling regeion, word-wrap, doorway mode */
					break;
				case 'i':	/* ToDo?  Printing */
					break;
				case 'l':	/* ToDo?  Scrolling regeion, word-wrap, doorway mode */
					break;
				case 'm':
					*(p--)=0;
					p2=cterm.escbuf+1;
					if(p2>p) {
						cterm.attr=7;
						break;
					}
					while((p=strtok(p2,";"))!=NULL) {
						p2=NULL;
						switch(atoi(p)) {
							case 0:
								cterm.attr=7;
								break;
							case 1:
								cterm.attr|=8;
								break;
							case 2:
								cterm.attr&=247;
								break;
							case 4:	/* Underscore */
								break;
							case 5:
							case 6:
								cterm.attr|=128;
								break;
							case 7:
								i=cterm.attr&7;
								j=cterm.attr&112;
								cterm.attr &= 136;
								cterm.attr |= j>>4;
								cterm.attr |= i<<4;
								break;
							case 8:
								j=cterm.attr&112;
								cterm.attr&=112;
								cterm.attr |= j>>4;
								break;
							case 30:
								cterm.attr&=248;
								break;
							case 31:
								cterm.attr&=248;
								cterm.attr|=4;
								break;
							case 32:
								cterm.attr&=248;
								cterm.attr|=2;
								break;
							case 33:
								cterm.attr&=248;
								cterm.attr|=6;
								break;
							case 34:
								cterm.attr&=248;
								cterm.attr|=1;
								break;
							case 35:
								cterm.attr&=248;
								cterm.attr|=5;
								break;
							case 36:
								cterm.attr&=248;
								cterm.attr|=3;
								break;
							case 37:
								cterm.attr&=248;
								cterm.attr|=7;
								break;
							case 40:
								cterm.attr&=143;
								break;
							case 41:
								cterm.attr&=143;
								cterm.attr|=4<<4;
								break;
							case 42:
								cterm.attr&=143;
								cterm.attr|=2<<4;
								break;
							case 43:
								cterm.attr&=143;
								cterm.attr|=6<<4;
								break;
							case 44:
								cterm.attr&=143;
								cterm.attr|=1<<4;
								break;
							case 45:
								cterm.attr&=143;
								cterm.attr|=5<<4;
								break;
							case 46:
								cterm.attr&=143;
								cterm.attr|=3<<4;
								break;
							case 47:
								cterm.attr&=143;
								cterm.attr|=7<<4;
								break;
						}
					}
					textattr(cterm.attr);
					break;
				case 'n':
					i=atoi(cterm.escbuf+1);
					switch(i) {
						case 6:
							if(retbuf!=NULL) {
								sprintf(tmp,"%c[%d;%dR",27,wherey(),wherex());
								if(strlen(retbuf)+strlen(tmp) < retsize)
									strcat(retbuf,tmp);
							}
							break;
						case 255:
							if(retbuf!=NULL) {
								sprintf(tmp,"%c[%d;%dR",27,cterm.height,cterm.width);
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
					break;
				case 's':
					cterm.save_xpos=wherex();
					cterm.save_ypos=wherey();
					break;
				case 'u':
					if(cterm.save_ypos>0 && cterm.save_ypos<=cterm.height
							&& cterm.save_xpos>0 && cterm.save_xpos<=cterm.width) {
						gotoxy(cterm.save_xpos,cterm.save_ypos);
					}
					break;
				case 'y':	/* ToDo?  VT100 Tests */
					break;
				case 'z':	/* ToDo?  Reset */
					break;
			}
			break;
		case 'D':
			scrollup();
			break;
		case 'M':
			scrolldown();
			break;
		case 'c':
			/* ToDo: Reset Terminal */
			break;
	}
	cterm.escbuf[0]=0;
	cterm.sequence=0;
}

void cterm_init(int height, int width, int xpos, int ypos, int backlines, unsigned char *scrollback)
{
	cterm.x=xpos;
	cterm.y=ypos;
	cterm.height=height;
	cterm.width=width;
	cterm.attr=7;
	cterm.save_xpos=0;
	cterm.save_ypos=0;
	cterm.escbuf[0]=0;
	cterm.sequence=0;
	cterm.music=0;
	cterm.backpos=0;
	cterm.backlines=backlines;
	cterm.scrollback=scrollback;
	if(cterm.scrollback!=NULL)
		memset(cterm.scrollback,0,cterm.width*2*cterm.backlines);
	textattr(cterm.attr);
	_setcursortype(_NORMALCURSOR);
	window(cterm.x,cterm.y,cterm.x+cterm.width-1,cterm.y+cterm.height-1);
	clrscr();
	gotoxy(1,1);
}

void ctputs(char *buf)
{
	char *outp;
	char *p;
	char outline[80];
	int		oldscroll;
	int		cx;
	int		cy;
	int		i;

	p=buf;
	outp=buf;
	oldscroll=_wscroll;
	_wscroll=0;
	cx=wherex();
	cy=wherey();
	for(p=buf;*p;p++) {
		switch(*p) {
			case '\r':
				cx=1;
				break;
			case '\n':
				if(cy==cterm.height) {
					*p=0;
					cputs(outp);
					outp=p+1;
					scrollup();
				}
				else
					cy++;
				break;
			case '\b':
				if(cx>0)
					cx--;
				break;
			case 7:		/* Bell */
				break;
			case '\t':
				for(i=0;i<10;i++) {
					if(cterm_tabs[i]>cx) {
						while(cx<cterm_tabs[i]) {
							cx++;
						}
						break;
					}
				}
				if(i==10) {
					cx=1;
					if(cy==cterm.height) {
						*p=0;
						cputs(outp);
						outp=p+1;
						scrollup();
					}
					else
						cy++;
				}
				break;
			default:
				if(cy==cterm.height
						&& cx==cterm.width) {
						*p=0;
						cputs(outp);
						outp=p+1;
						scrollup();
						cx=1;
				}
				else {
					if(cx==cterm.width) {
						cx=1;
						cy++;
					}
					else {
						cx++;
					}
				}
				break;
		}
	}
	cputs(outp);
	_wscroll=oldscroll;
}

char *cterm_write(unsigned char *buf, int buflen, char *retbuf, int retsize)
{
	unsigned char ch[2];
	unsigned char prn[BUFSIZE];
	int	key;
	int i,j,k;
	char	*ret;
	struct text_info	ti;
	int	olddmc;

	olddmc=dont_move_cursor;
	dont_move_cursor=1;
	if(retbuf!=NULL)
		retbuf[0]=0;
	gettextinfo(&ti);
	window(cterm.x,cterm.y,cterm.x+cterm.width-1,cterm.y+cterm.height-1);
	gotoxy(cterm.xpos,cterm.ypos);
	textattr(cterm.attr);
	ch[1]=0;
	switch(buflen) {
		case 0:
			break;
		default:
			prn[0]=0;
			for(j=0;j<buflen;j++) {
				ch[0]=buf[j];
				if(cterm.sequence) {
					strcat(cterm.escbuf,ch);
					if((ch[0]>='@' && ch[0]<='Z')
							|| (ch[0]>='a' && ch[0]<='z')) {
						do_ansi(retbuf, retsize);
					}
				}
				else if (cterm.music) {
					strcat(cterm.musicbuf,ch);
					if(ch[0]==14)
						play_music();
				}
				else {
					switch(buf[j]) {
						case 0:
							break;
						case 7:			/* Beep */
							ctputs(prn);
							prn[0]=0;
							#ifdef __unix__
								putch(7);
							#else
								MessageBeep(MB_OK);
							#endif
							break;
						case 12:		/* ^L - Clear screen */
							ctputs(prn);
							prn[0]=0;
							clearscreen(cterm.attr);
							gotoxy(1,1);
							break;
						case 27:		/* ESC */
							ctputs(prn);
							prn[0]=0;
							cterm.sequence=1;
							break;
						case '\t':
							ctputs(prn);
							prn[0]=0;
							for(k=0;k<11;k++) {
								if(cterm_tabs[k]>wherex()) {
									gotoxy(cterm_tabs[k],wherey());
									break;
								}
							}
							break;
						default:
							strcat(prn,ch);
					}
				}
			}
			ctputs(prn);
			prn[0]=0;
			break;
	}
	cterm.xpos=wherex();
	cterm.ypos=wherey();
#if 0
	window(ti.winleft,ti.wintop,ti.winright,ti.wintop);
	gotoxy(ti.curx,ti.cury);
	textattr(ti.attribute);
#endif

	dont_move_cursor=olddmc;
	gotoxy(wherex(),wherey());
	return(retbuf);
}

void cterm_end(void)
{
	/* Nothing to be done here at the moment */
}
