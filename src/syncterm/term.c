#include <genwrap.h>
#include <ciowrap.h>

#include "term.h"
#include "uifcinit.h"
#include "menu.h"

struct terminal term;

const int tabs[11]={1,8,16,24,32,40,48,56,64,72,80};

int backlines=2000;

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
	if(term.backpos>backlines) {
		memmove(term.scrollback,term.scrollback+term.width*2,term.width*2*(backlines-1));
		term.backpos--;
	}
	gettext(term.x+1,term.y+1,term.x+term.width,term.y+1,term.scrollback+(term.backpos-1)*term.width*2);
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

	buf=(char *)malloc((term.xpos+1)*2);
	j=0;
	for(i=1;i<=term.xpos;i++) {
		buf[j++]=' ';
		buf[j++]=term.attr;
	}
	puttext(term.x+1,term.y+term.ypos,term.x+term.xpos,term.y+term.ypos,buf);
	free(buf);
}

void clear2eol(void)
{
	char *buf;
	int i,j;

	buf=(char *)malloc((term.width-term.xpos+1)*2);
	j=0;
	for(i=term.xpos;i<=term.width;i++) {
		buf[j++]=' ';
		buf[j++]=term.attr;
	}
	puttext(term.x+term.xpos,term.y+term.ypos,term.x+term.width,term.y+term.ypos,buf);
	free(buf);
}

void clearscreen(char attr)
{
	char *buf;
	int x,y,j;

	term.backpos+=term.height;
	if(term.backpos>backlines) {
		memmove(term.scrollback,term.scrollback+term.width*2*(term.backpos-backlines),term.width*2*(backlines-(term.backpos-backlines)));
		term.backpos=backlines;
	}
	gettext(term.x+1,term.y+1,term.x+term.width,term.y+term.height,term.scrollback+(term.backpos-term.height)*term.width*2);
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

void set_cursor(void)
{
	gotoxy(term.x+term.xpos,term.y+term.ypos);
}

void do_ansi(void)
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
					gettext(term.x+term.xpos,term.y+term.ypos,term.x+term.width-1,term.y+term.ypos,tmp);
					putch(' ');
					puttext(term.x+term.xpos+1,term.y+term.ypos,term.x+term.width,term.y+term.ypos,tmp);
					set_cursor();
					break;
				case 'A':	/* Cursor Up */
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					term.ypos-=i;
					if(term.ypos<1)
						term.ypos=1;
					set_cursor();
					break;
				case 'B':	/* Cursor Down */
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					term.ypos+=i;
					if(term.ypos>term.height)
						term.ypos=term.height;
					set_cursor();
					break;
				case 'C':	/* Cursor Right */
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					term.xpos+=i;
					if(term.xpos>term.width)
						term.xpos=term.width;
					set_cursor();
					break;
				case 'D':	/* Cursor Left */
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					term.xpos-=i;
					if(term.xpos<1)
						term.xpos=1;
					set_cursor();
					break;
				case 'E':
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					term.ypos+=i;
					for(j=term.height;j<term.ypos;j++)
						scrollup();
					if(term.ypos>term.height)
						term.ypos=term.height;
					set_cursor();
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
					term.ypos=row;
					term.xpos=col;
					set_cursor();
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
							for(i=term.ypos+1;i<=term.height;i++) {
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
							for(i=term.ypos-1;i>=1;i--) {
								puttext(term.x+1,term.y+i,term.x+term.width,term.y+i,p2);
							}
							free(p2);
							break;
						case 2:
							clearscreen(term.attr);
							term.ypos=1;
							term.xpos=1;
							set_cursor();
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
							puttext(term.x+1,term.y+term.ypos,term.x+term.width,term.y+term.ypos,p2);
							free(p2);
							break;
					}
					break;
				case 'L':
					i=atoi(term.escbuf+1);
					if(i==0)
						i=1;
					if(i>term.height-term.ypos)
						i=term.height-term.ypos;
					if(i<term.height-term.ypos) {
						p2=(char *)malloc((term.height-term.ypos-i)*term.width*2);
						gettext(term.x+1,term.y+term.ypos,term.x+term.width,term.ypos+(term.height-term.ypos-i),p2);
						puttext(term.x+1,term.y+term.ypos+i,term.x+term.width,term.ypos+(term.height-term.ypos),p2);
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
					if(i>term.width-term.xpos)
						i=term.width-term.xpos;
					p2=(char *)malloc((term.width-term.xpos)*2);
					gettext(term.x+term.xpos,term.y+term.ypos,term.x+term.width,term.y+term.ypos,p2);
					memmove(p2,p2+(i*2),(term.width-term.xpos-i)*2);
					for(i=(term.width-term.xpos)*2-2;i>=term.xpos;i-=2)
						p2[i]=' ';
					puttext(term.x+term.xpos,term.y+term.ypos,term.x+term.width,term.y+term.ypos,p2);
					break;
				case 'S':
					scrollup();
					break;
				case 'T':
					scrolldown();
					break;
				case 'U':
					clearscreen(7);
					term.ypos=1;
					term.xpos=1;
					set_cursor();
					break;
				case 'Y':	/* ToDo? BananaCom Clear Line */
					break;
				case 'Z':
					for(j=10;j>=0;j--) {
						if(tabs[j]<term.xpos) {
							term.xpos=tabs[j];
							break;
						}
					}
					set_cursor();
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
							sprintf(tmp,"%c[%d;%dR",27,term.ypos,term.xpos);
							rlogin_send(tmp,strlen(tmp),1000);
							break;
						case 255:
							sprintf(tmp,"%c[%d;%dR",27,term.height,term.width);
							rlogin_send(tmp,strlen(tmp),1000);
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
					term.save_xpos=term.xpos;
					term.save_ypos=term.ypos;
					break;
				case 'u':
					if(term.save_ypos>0 && term.save_ypos<=term.height
							&& term.save_xpos>0 && term.save_xpos<=term.width) {
						term.xpos=term.save_xpos;
						term.ypos=term.save_ypos;
						set_cursor();
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

void doterm(void)
{
	unsigned char ch[2];
	int	key;
	int i;

	term.xpos=1;
	term.ypos=1;
	term.attr=7;
	term.save_xpos=0;
	term.save_ypos=0;
	term.escbuf[0]=0;
	term.sequence=0;
	term.music=0;
	term.backpos=0;
	term.scrollback=malloc(term.width*2*backlines);
	memset(term.scrollback,0,term.width*2*backlines);
	ch[1]=0;
	gotoxy(term.xpos,term.ypos);
	textattr(term.attr);
	_setcursortype(_NORMALCURSOR);
	/* Main input loop */
	for(;;) {
		/* Get remote input */
		i=rlogin_recv(ch,1,100);
		switch(i) {
			case -1:
				uifcmsg("Disconnected","`Disconnected`\n\nRemote host dropped connection");
				return;
			case 1:
				if(term.sequence) {
					strcat(term.escbuf,ch);
					if((ch[0]>='@' && ch[0]<='Z')
							|| (ch[0]>='a' && ch[0]<='z')) {
						do_ansi();
					}
				}
				else if (term.music) {
					strcat(term.musicbuf,ch);
					if(ch[0]==14)
						play_music();
				}
				else {
					switch(ch[0]) {
						case 0:
							break;
						case 7:			/* Beep */
							#ifdef __unix__
								beep();
							#else
								MessageBeep(MB_OK);
							#endif
							break;
						case 12:		/* ^L - Clear screen */
							clearscreen(term.attr);
							term.ypos=1;
							term.xpos=1;
							set_cursor();
							break;
						case 27:		/* ESC */
							term.sequence=1;
							break;
						case '\n':
							term.ypos++;
							if(term.ypos>term.height) {
								term.ypos=term.height;
								scrollup();
							}
							set_cursor();
							break;
						case '\t':
							for(i=0;i<11;i++) {
								if(tabs[i]>term.xpos) {
									term.xpos=tabs[i];
									break;
								}
							}
							set_cursor();
							break;
						case '\r':
							term.xpos=1;
							set_cursor();
							break;
						case '\b':
							term.xpos--;
							if(term.xpos<1)
								term.xpos=1;
							putch(' ');
							set_cursor();
							break;
						default:
							putch(ch[0]);
							term.xpos++;
							if(term.xpos>term.width) {
								term.xpos=1;
								term.ypos++;
								if(term.ypos>term.height) {
									term.ypos=term.height;
									scrollup();
								}
								set_cursor();
							}
					}
				}
				break;
		}
		
		/* Get local input */
		if(kbhit()) {
			key=getch();
			switch(key) {
				case 17:	/* CTRL-Q */
					free(term.scrollback);
					return;
				case 19:	/* CTRL-S */
					switch(syncmenu()) {
						case -1:
							free(term.scrollback);
							return;
					}
					set_cursor();
					break;
				case KEY_LEFT:
					rlogin_send("\033[D",3,100);
					break;
				case KEY_RIGHT:
					rlogin_send("\033[C",3,100);
					break;
				case KEY_UP:
					rlogin_send("\033[A",3,100);
					break;
				case KEY_DOWN:
					rlogin_send("\033[B",3,100);
					break;
				case KEY_HOME:
					rlogin_send("\033[H",3,100);
					break;
				case KEY_END:
#ifdef KEY_SELECT
				case KEY_SELECT:	/* Some terminfo/termcap entries use KEY_SELECT as the END key! */
#endif
					rlogin_send("\033[K",3,100);
					break;
				case KEY_F(1):
					rlogin_send("\033OP",3,100);
					break;
				case KEY_F(2):
					rlogin_send("\033OQ",3,100);
					break;
				case KEY_F(3):
					rlogin_send("\033Ow",3,100);
					break;
				case KEY_F(4):
					rlogin_send("\033Ox",3,100);
					break;
#ifdef __unix__
				case 128|'S':	/* Under curses, ALT sets the high bit of the char */
				case 128|'s':	/* Under curses, ALT sets the high bit of the char */
					viewscroll();
					break;
#endif
				case KEY_BACKSPACE:
				case '\b':
					key='\b';
					/* FALLTHROUGH to default */
				default:
					if(key<256) {
						ch[0]=key;
						rlogin_send(ch,1,100);
					}
					
			}
		}
	}
}
