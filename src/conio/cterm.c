#include <genwrap.h>
#include <ciolib.h>
#include <keys.h>

#include "cterm.h"

#define	BUFSIZE	2048

struct terminal {
	int	height;
	int	width;
	int	x;
	int	y;
	char *buffer;
	int	attr;
	int save_xpos;
	int save_ypos;
	char	escbuf[1024];
	int	sequence;
	char	musicbuf[1024];
	int music;
	char *scrollback;
	int backpos;
	int backlines;
};

static struct terminal term;

/* const int tabs[11]={1,8,16,24,32,40,48,56,64,72,80}; */
const int tabs[11]={9,17,25,33,41,49,57,65,73,80,80.1};

void play_music(void)
{
	/* ToDo Music code parsing stuff */
	term.music=0;
}

void scrolldown(void)
{
	char *buf;
	int i,j;

	buf=(char *)malloc(term.width*(term.height-1)*2);
	gettext(term.x+1,term.y+1,term.x+term.width,term.y+term.height-1,buf);
	puttext(term.x+1,term.y+2,term.x+term.width,term.y+term.height,buf);
	j=0;
	for(i=0;i<term.width;i++) {
		buf[j++]=' ';
		buf[j++]=term.attr;
	}
	puttext(term.x+1,term.y+1,term.x+term.width,term.y+1,buf);
	free(buf);
}

void scrollup(void)
{
	char *buf;
	int i,j;

	term.backpos++;
	if(term.scrollback!=NULL) {
		if(term.backpos>term.backlines) {
			memmove(term.scrollback,term.scrollback+term.width*2,term.width*2*(term.backlines-1));
			term.backpos--;
		}
		gettext(term.x+1,term.y+1,term.x+term.width,term.y+1,term.scrollback+(term.backpos-1)*term.width*2);
	}
	buf=(char *)malloc(term.width*(term.height-1)*2);
	gettext(term.x+1,term.y+2,term.x+term.width,term.y+term.height,buf);
	puttext(term.x+1,term.y+1,term.x+term.width,term.y+term.height-1,buf);
	j=0;
	for(i=0;i<term.width;i++) {
		buf[j++]=' ';
		buf[j++]=term.attr;
	}
	puttext(term.x+1,term.y+term.height,term.x+term.width,term.y+term.height,buf);
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
		buf[j++]=term.attr;
	}
	puttext(term.x+1,term.y+wherey(),term.x+wherex(),term.y+wherey(),buf);
	free(buf);
}

void clear2eol(void)
{
	char *buf;
	int i,j;

	buf=(char *)malloc((term.width-wherex()+1)*2);
	j=0;
	for(i=wherex();i<=term.width;i++) {
		buf[j++]=' ';
		buf[j++]=term.attr;
	}
	puttext(term.x+wherex(),term.y+wherey(),term.x+term.width,term.y+wherey(),buf);
	free(buf);
}

void clearscreen(char attr)
{
	char *buf;
	int x,y,j;

	term.backpos+=term.height;
	if(term.scrollback!=NULL) {
		if(term.backpos>term.backlines) {
			memmove(term.scrollback,term.scrollback+term.width*2*(term.backpos-term.backlines),term.width*2*(term.backlines-(term.backpos-term.backlines)));
			term.backpos=term.backlines;
		}
		gettext(term.x+1,term.y+1,term.x+term.width,term.y+term.height,term.scrollback+(term.backpos-term.height)*term.width*2);
	}
	buf=(char *)malloc(term.width*(term.height)*2);
	j=0;
	for(x=0;x<term.width;x++) {
		for(y=0;y<term.height;y++) {
			buf[j++]=' ';
			buf[j++]=attr;
		}
	}
	puttext(term.x+1,term.y+1,term.x+term.width,term.y+term.height,buf);
	free(buf);
}

void do_ansi(char *retbuf, int retsize)
{
	char	*p;
	char	*p2;
	char	tmp[1024];
	int		i,j,k;
	int		row,col;

	switch(term.escbuf[0]) {
		case '[':
			/* ANSI stuff */
			p=term.escbuf+strlen(term.escbuf)-1;
			switch(*p) {
				case '@':	/* Insert Char */
					i=wherex();
					j=wherey();
					gettext(term.x+wherex(),term.y+wherey(),term.x+term.width-1,term.y+wherey(),tmp);
					putch(' ');
					puttext(term.x+wherex()+1,term.y+wherey(),term.x+term.width,term.y+wherey(),tmp);
					gotoxy(i,j);
					break;
				case 'A':	/* Cursor Up */
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					i=wherey()-i;
					if(i<1)
						i=1;
					gotoxy(wherex(),i);
					break;
				case 'B':	/* Cursor Down */
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					i=wherey()+i;
					if(i>term.height)
						i=term.height;
					gotoxy(wherex(),i);
					break;
				case 'C':	/* Cursor Right */
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					i=wherex()+i;
					if(i>term.width)
						i=term.width;
					gotoxy(i,wherey());
					break;
				case 'D':	/* Cursor Left */
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					i=wherex()-i;
					if(i<1)
						i=1;
					gotoxy(i,wherey());
					break;
				case 'E':
					i=atoi(term.escbuf+1);
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
					if(strlen(term.escbuf)>1) {
						if((p=strtok(term.escbuf+1,";"))!=NULL) {
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
					if(row>term.height)
						row=term.height;
					if(col>term.width)
						col=term.width;
					gotoxy(col,row);
					break;
				case 'J':
					i=atoi(term.escbuf+1);
					switch(i) {
						case 0:
							clear2eol();
							p2=(char *)malloc(term.width*2);
							j=0;
							for(i=0;i<term.width;i++) {
								p2[j++]=' ';
								p2[j++]=term.attr;
							}
							for(i=wherey()+1;i<=term.height;i++) {
								puttext(term.x+1,term.y+i,term.x+term.width,term.y+i,p2);
							}
							free(p2);
							break;
						case 1:
							clear2bol();
							p2=(char *)malloc(term.width*2);
							j=0;
							for(i=0;i<term.width;i++) {
								p2[j++]=' ';
								p2[j++]=term.attr;
							}
							for(i=wherey()-1;i>=1;i--) {
								puttext(term.x+1,term.y+i,term.x+term.width,term.y+i,p2);
							}
							free(p2);
							break;
						case 2:
							clearscreen(term.attr);
							gotoxy(1,1);
							break;
					}
					break;
				case 'K':
					i=atoi(term.escbuf+1);
					switch(i) {
						case 0:
							clear2eol();
							break;
						case 1:
							clear2bol();
							break;
						case 2:
							p2=(char *)malloc(term.width*2);
							j=0;
							for(i=0;i<term.width;i++) {
								p2[j++]=' ';
								p2[j++]=term.attr;
							}
							puttext(term.x+1,term.y+wherey(),term.x+term.width,term.y+wherey(),p2);
							free(p2);
							break;
					}
					break;
				case 'L':
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					if(i>term.height-wherey())
						i=term.height-wherey();
					if(i<term.height-wherey()) {
						p2=(char *)malloc((term.height-wherey()-i)*term.width*2);
						gettext(term.x+1,term.y+wherey(),term.x+term.width,wherey()+(term.height-wherey()-i),p2);
						puttext(term.x+1,term.y+wherey()+i,term.x+term.width,wherey()+(term.height-wherey()),p2);
						j=0;
						free(p2);
					}
					p2=(char *)malloc(term.width*2);
					j=0;
					for(k=0;k<term.width;k++) {
						p2[j++]=' ';
						p2[j++]=term.attr;
					}
					for(k-0;j<i;i++) {
						puttext(term.x+1,term.y+i,term.x+term.width,term.y+i,p2);
					}
					free(p2);
					break;
				case 'M':
				case 'N':
					term.music=1;
					break;
				case 'P':	/* Delete char */
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					if(i>term.width-wherex())
						i=term.width-wherex();
					p2=(char *)malloc((term.width-wherex())*2);
					gettext(term.x+wherex(),term.y+wherey(),term.x+term.width,term.y+wherey(),p2);
					memmove(p2,p2+(i*2),(term.width-wherex()-i)*2);
					for(i=(term.width-wherex())*2-2;i>=wherex();i-=2)
						p2[i]=' ';
					puttext(term.x+wherex(),term.y+wherey(),term.x+term.width,term.y+wherey(),p2);
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
						if(tabs[j]<wherex()) {
							gotoxy(tabs[j],wherey());
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
					p2=term.escbuf+1;
					if(p2>p) {
						term.attr=7;
						break;
					}
					while((p=strtok(p2,";"))!=NULL) {
						p2=NULL;
						switch(atoi(p)) {
							case 0:
								term.attr=7;
								break;
							case 1:
								term.attr|=8;
								break;
							case 2:
								term.attr&=247;
								break;
							case 4:	/* Underscore */
								break;
							case 5:
							case 6:
								term.attr|=128;
								break;
							case 7:
								i=term.attr&7;
								j=term.attr&112;
								term.attr &= 136;
								term.attr |= j>>4;
								term.attr |= i<<4;
								break;
							case 8:
								j=term.attr&112;
								term.attr&=112;
								term.attr |= j>>4;
								break;
							case 30:
								term.attr&=248;
								break;
							case 31:
								term.attr&=248;
								term.attr|=4;
								break;
							case 32:
								term.attr&=248;
								term.attr|=2;
								break;
							case 33:
								term.attr&=248;
								term.attr|=6;
								break;
							case 34:
								term.attr&=248;
								term.attr|=1;
								break;
							case 35:
								term.attr&=248;
								term.attr|=5;
								break;
							case 36:
								term.attr&=248;
								term.attr|=3;
								break;
							case 37:
								term.attr&=248;
								term.attr|=7;
								break;
							case 40:
								term.attr&=143;
								break;
							case 41:
								term.attr&=143;
								term.attr|=4<<4;
								break;
							case 42:
								term.attr&=143;
								term.attr|=2<<4;
								break;
							case 43:
								term.attr&=143;
								term.attr|=6<<4;
								break;
							case 44:
								term.attr&=143;
								term.attr|=1<<4;
								break;
							case 45:
								term.attr&=143;
								term.attr|=5<<4;
								break;
							case 46:
								term.attr&=143;
								term.attr|=3<<4;
								break;
							case 47:
								term.attr&=143;
								term.attr|=7<<4;
								break;
						}
					}
					textattr(term.attr);
					break;
				case 'n':
					i=atoi(term.escbuf+1);
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
								sprintf(tmp,"%c[%d;%dR",27,term.height,term.width);
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
					term.save_xpos=wherex();
					term.save_ypos=wherey();
					break;
				case 'u':
					if(term.save_ypos>0 && term.save_ypos<=term.height
							&& term.save_xpos>0 && term.save_xpos<=term.width) {
						gotoxy(term.save_xpos,term.save_ypos);
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
	term.escbuf[0]=0;
	term.sequence=0;
}

void cterm_init(int height, int width, int xpos, int ypos, int backlines, unsigned char *scrollback)
{
	term.x=xpos;
	term.y=ypos;
	term.height=height;
	term.width=width;
	term.attr=7;
	term.save_xpos=0;
	term.save_ypos=0;
	term.escbuf[0]=0;
	term.sequence=0;
	term.music=0;
	term.backpos=0;
	term.backlines=backlines;
	term.scrollback=scrollback;
	if(term.scrollback!=NULL)
		memset(term.scrollback,0,term.width*2*term.backlines);
	textattr(term.attr);
	_setcursortype(_NORMALCURSOR);
	window(term.x+1,term.y+1,term.x+term.width,term.y+term.height);
	clrscr();
	gotoxy(1,1);
}

char *cterm_write(unsigned char *buf, int buflen, char *retbuf, int retsize)
{
	unsigned char ch[2];
	unsigned char prn[BUFSIZE];
	int	key;
	int i,j,k;
	char	*ret;

	if(retbuf!=NULL)
		retbuf[0]=0;
	ch[1]=0;
	switch(buflen) {
		case 0:
			break;
		default:
			prn[0]=0;
			for(j=0;j<buflen;j++) {
				ch[0]=buf[j];
				if(term.sequence) {
					strcat(term.escbuf,ch);
					if((ch[0]>='@' && ch[0]<='Z')
							|| (ch[0]>='a' && ch[0]<='z')) {
						do_ansi(retbuf, retsize);
					}
				}
				else if (term.music) {
					strcat(term.musicbuf,ch);
					if(ch[0]==14)
						play_music();
				}
				else {
					switch(buf[j]) {
						case 0:
							break;
						case 7:			/* Beep */
							cputs(prn);
							prn[0]=0;
							#ifdef __unix__
								putch(7);
							#else
								MessageBeep(MB_OK);
							#endif
							break;
						case 12:		/* ^L - Clear screen */
							cputs(prn);
							prn[0]=0;
							clearscreen(term.attr);
							gotoxy(1,1);
							break;
						case 27:		/* ESC */
							cputs(prn);
							prn[0]=0;
							term.sequence=1;
							break;
						case '\t':
							cputs(prn);
							prn[0]=0;
							for(k=0;k<11;k++) {
								if(tabs[k]>wherex()) {
									gotoxy(tabs[k],wherey());
									break;
								}
							}
							break;
						default:
							strcat(prn,ch);
					}
				}
			}
			cputs(prn);
			prn[0]=0;
			break;
	}
	return(retbuf);
}

void cterm_end(void)
{
	/* Nothing to be done here at the moment */
}
