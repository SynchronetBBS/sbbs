#include <genwrap.h>
#include <ciolib.h>
#include <cterm.h>
#include <mouse.h>
#include <keys.h>

#include "conn.h"
#include "term.h"
#include "uifcinit.h"
#include "menu.h"

#define	BUFSIZE	2048

int backlines=2000;

struct terminal term;

void mousedrag(unsigned char *scrollback)
{
	int	key;
	struct mouse_event mevent;
	unsigned char *screen;
	unsigned char *sbuffer;
	int sbufsize=0;
	int pos, startpos,endpos, lines;
	int outpos;
	char *copybuf;

	sbufsize=term.width*2*term.height;
	screen=(unsigned char*)malloc(sbufsize);
	sbuffer=(unsigned char*)malloc(sbufsize);
	gettext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
	while(1) {
		key=getch();
		if(key==0 || key==0xff)
			key|=getch()<<8;
		switch(key) {
			case CIO_KEY_MOUSE:
				getmouse(&mevent);
				startpos=((mevent.starty-1)*term.width)+(mevent.startx-1);
				endpos=((mevent.endy-1)*term.width)+(mevent.endx-1);
				if(endpos<startpos) {
					pos=endpos;
					endpos=startpos;
					startpos=pos;
				}
				switch(mevent.event) {
					case CIOLIB_BUTTON_1_DRAG_MOVE:
						memcpy(sbuffer,screen,sbufsize);
						for(pos=startpos;pos<=endpos;pos++) {
							if(sbuffer[pos*2+1]&0x70!=0x10)
								sbuffer[pos*2+1]=sbuffer[pos*2+1]&0x8F|0x10;
							else
								sbuffer[pos*2+1]=sbuffer[pos*2+1]&0x8F|0x60;
							if((sbuffer[pos*2+1]&0x70)>>4 == sbuffer[pos*2+1]&0x0F) {
								sbuffer[pos*2+1]|=0x60;
								sbuffer[pos*2+1]&=0xEF;
							}
						}
						puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,sbuffer);
						break;
					default:
						lines=abs(mevent.endy-mevent.starty)+1;
						copybuf=malloc(endpos-startpos+4+lines*2);
						outpos=0;
						for(pos=startpos;pos<=endpos;pos++) {
							copybuf[outpos++]=screen[pos*2];
							if((pos+1)%term.width==0) {
								copybuf[outpos++]='\r';
								copybuf[outpos++]='\n';
							}
						}
						copybuf[outpos]=0;
						copytext(copybuf, strlen(copybuf));
						puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
						free(copybuf);
						free(screen);
						free(sbuffer);
						return;
				}
				break;
			default:
				puttext(term.x-1,term.y-1,term.x+term.width-2,term.y+term.height-2,screen);
				ungetch(key);
				free(screen);
				free(sbuffer);
				return;
		}
	}
}

void doterm(void)
{
	unsigned char ch[2];
	unsigned char buf[BUFSIZE];
	unsigned char prn[BUFSIZE];
	int	key;
	int i,j,k;
	unsigned char *scrollback;
	char *p;

	ciomouse_setevents(0);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_START);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_MOVE);
	ciomouse_addevent(CIOLIB_BUTTON_1_DRAG_END);
	ciomouse_addevent(CIOLIB_BUTTON_3_CLICK);
	ciomouse_addevent(CIOLIB_BUTTON_2_CLICK);
	scrollback=malloc(term.width*2*backlines);
	memset(scrollback,0,term.width*2*backlines);
	cterm_init(term.height,term.width,term.x-1,term.y-1,backlines,scrollback);
	ch[1]=0;

	/* Main input loop */
	for(;;) {
		/* Get remote input */
		i=conn_recv(buf,sizeof(buf));
		switch(i) {
			case -1:
				free(scrollback);
				cterm_end();
				conn_close();
				uifcmsg("Disconnected","`Disconnected`\n\nRemote host dropped connection");
				return;
			case 0:
				break;
			default:
				cterm_write(buf,i,prn,sizeof(prn));
				conn_send(prn,strlen(prn),0);
				break;
		}

		/* Get local input */
		while(kbhit()) {
			struct mouse_event mevent;
			key=getch();
			switch(key) {
				case 0xff:
				case 0:
					key|=getch()<<8;
					switch(key) {
						case CIO_KEY_MOUSE:
							getmouse(&mevent);
							switch(mevent.event) {
								case CIOLIB_BUTTON_1_DRAG_START:
									mousedrag(scrollback);
									break;
								case CIOLIB_BUTTON_2_CLICK:
								case CIOLIB_BUTTON_3_CLICK:
									p=getcliptext();
									if(p!=NULL) {
										conn_send(p,strlen(p),0);
										free(p);
									}
									break;
							}
									
							break;
						case CIO_KEY_LEFT:
							conn_send("\033[D",3,0);
							break;
						case CIO_KEY_RIGHT:
							conn_send("\033[C",3,0);
							break;
						case CIO_KEY_UP:
							conn_send("\033[A",3,0);
							break;
						case CIO_KEY_DOWN:
							conn_send("\033[B",3,0);
							break;
						case CIO_KEY_HOME:
							conn_send("\033[H",3,0);
							break;
						case CIO_KEY_END:
#ifdef CIO_KEY_SELECT
						case CIO_KEY_SELECT:	/* Some terminfo/termcap entries use KEY_SELECT as the END key! */
#endif
							conn_send("\033[K",3,0);
							break;
						case CIO_KEY_F(1):
							conn_send("\033OP",3,0);
							break;
						case CIO_KEY_F(2):
							conn_send("\033OQ",3,0);
							break;
						case CIO_KEY_F(3):
							conn_send("\033Ow",3,0);
							break;
						case CIO_KEY_F(4):
							conn_send("\033Ox",3,0);
							break;
						case 0x1f00:	/* ALT-S */
							viewscroll();
							break;
					}
					break;
				case 17:	/* CTRL-Q */
					cterm_end();
					free(scrollback);
					conn_close();
					return;
				case 19:	/* CTRL-S */
					i=wherex();
					j=wherey();
					switch(syncmenu()) {
						case -1:
							cterm_end();
							free(scrollback);
							conn_close();
							return;
					}
					gotoxy(i,j);
					break;
				case '\b':
					key='\b';
					/* FALLTHROUGH to default */
				default:
					if(key<256) {
						ch[0]=key;
						conn_send(ch,1,0);
					}
					
			}
		}
		SLEEP(1);
	}
}
