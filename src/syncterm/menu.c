#include <uifc.h>
#include <ciowrap.h>

#include "term.h"
#include "uifcinit.h"

void viewscroll(void)
{
	int	top;
	int key;
	int i;

	uifcbail();
	drawwin();
	top=term.backpos-term.height;
	for(;;) {
		if(top<1)
			top=1;
		if(top>term.backpos-term.height)
			top=term.backpos-term.height;
		puttext(term.x+1,term.y+1,term.x+term.width,term.y+term.height,term.scrollback+(term.width*2*top));
		key=getch();
		switch(key) {
			case 'j':
			case 'J':
			case KEY_UP:
				top--;
				break;
			case 'k':
			case 'K':
			case KEY_DOWN:
				top++;
				break;
			case 'h':
			case 'H':
			case KEY_PPAGE:
				top-=term.height;
				break;
			case 'l':
			case 'L':
			case KEY_NPAGE:
				top+=term.height;
				break;
			case ESC:
				return;
			case KEY_F(1):
				init_uifc();
				uifc.helpbuf=	"`Scrollback Buffer`\n\n"
								"~ J ~ or ~ Up Arrow ~   Scrolls up one line\n"
								"~ K ~ or ~ Down Arrow ~ Scrolls down one line\n"
								"~ H ~ or ~ Page Up ~    Scrolls up one screen\n"
								"~ L ~ or ~ Page Down ~  Scrolls down one screen\n";
				uifc.showhelp();
				uifcbail();
				drawwin();
				break;
		}
	}
}

int syncmenu(void)
{
	char	*opts[3]={	 "Disconnect"
						,"Scrollback"
						,""};
	int		opt=0;
	int		i;
	struct	text_info txtinfo;
	char	*buf;
	int		ret;

    gettextinfo(&txtinfo);
	buf=(char *)malloc(txtinfo.screenheight*txtinfo.screenwidth*2);
	gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);

	for(ret=0;!ret;) {
		init_uifc();
		i=uifc.list(WIN_MID|WIN_SAV,0,0,0,&opt,NULL,"SyncTERM Online Menu",opts);
		switch(i) {
			case -1:	/* Cancel */
				ret=1;
				break;
			case 0:		/* Disconnect */
				ret=-1;
				break;
			case 1:
				viewscroll();
				break;
		}
	}

	uifcbail();
	puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	free(buf);
	return(ret);
}
