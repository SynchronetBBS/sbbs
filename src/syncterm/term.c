#include <genwrap.h>
#include <ciolib.h>
#include <cterm.h>
#include <mouse.h>
#include <keys.h>

#include "rlogin.h"
#include "term.h"
#include "uifcinit.h"
#include "menu.h"

#define	BUFSIZE	2048

int backlines=2000;

struct terminal term;

void doterm(void)
{
	unsigned char ch[2];
	unsigned char buf[BUFSIZE];
	unsigned char prn[BUFSIZE];
	int	key;
	int i,j,k;
	unsigned char *scrollback;

	scrollback=malloc(term.width*2*backlines);
	memset(scrollback,0,term.width*2*backlines);
	cterm_init(term.height,term.width,term.x-1,term.y-1,backlines,scrollback);
	ch[1]=0;

	/* Main input loop */
	for(;;) {
		/* Get remote input */
		i=rlogin_recv(buf,sizeof(buf));
		switch(i) {
			case -1:
				free(scrollback);
				cterm_end();
				rlogin_close();
				uifcmsg("Disconnected","`Disconnected`\n\nRemote host dropped connection");
				return;
			case 0:
				break;
			default:
				cterm_write(buf,i,prn,sizeof(prn));
				rlogin_send(prn,strlen(prn),0);
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
							break;

						case CIO_KEY_LEFT:
							rlogin_send("\033[D",3,0);
							break;
						case CIO_KEY_RIGHT:
							rlogin_send("\033[C",3,0);
							break;
						case CIO_KEY_UP:
							rlogin_send("\033[A",3,0);
							break;
						case CIO_KEY_DOWN:
							rlogin_send("\033[B",3,0);
							break;
						case CIO_KEY_HOME:
							rlogin_send("\033[H",3,0);
							break;
						case CIO_KEY_END:
#ifdef CIO_KEY_SELECT
						case CIO_KEY_SELECT:	/* Some terminfo/termcap entries use KEY_SELECT as the END key! */
#endif
							rlogin_send("\033[K",3,0);
							break;
						case CIO_KEY_F(1):
							rlogin_send("\033OP",3,0);
							break;
						case CIO_KEY_F(2):
							rlogin_send("\033OQ",3,0);
							break;
						case CIO_KEY_F(3):
							rlogin_send("\033Ow",3,0);
							break;
						case CIO_KEY_F(4):
							rlogin_send("\033Ox",3,0);
							break;
						case 0x1f00:	/* ALT-S */
							viewscroll();
							break;
					}
					break;
				case 17:	/* CTRL-Q */
					cterm_end();
					free(scrollback);
					rlogin_close();
					return;
				case 19:	/* CTRL-S */
					i=wherex();
					j=wherey();
					switch(syncmenu()) {
						case -1:
							cterm_end();
							free(scrollback);
							rlogin_close();
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
						rlogin_send(ch,1,0);
					}
					
			}
		}
		SLEEP(1);
	}
}
