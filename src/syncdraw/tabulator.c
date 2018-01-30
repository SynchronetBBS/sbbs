#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <ciolib.h>
#include <gen_defs.h>

#include "key.h"
#include "syncdraw.h"
#include "miscfunctions.h"
#include "options.h"

int 
tabforward(int a)
{
	int             b;
	for (b = a + 2; b <= 80; b++)
		if (tabs[b] == TRUE)
			return b - 1;
	return 79;
}

int 
tabback(int a)
{
	int             b;
	for (b = a; b >= 1; b--)
		if (tabs[b] == TRUE)
			return b - 1;
	return 0;
}

void 
tabsetup(void)
{
	int             x = 0, y, ch, ax;
	char           *str;
	struct			text_info	ti;
	struct			mouse_event	me;
	char			buf[80*2];

	gettextinfo(&ti);
	clrline();
	CodeWrite(1, ti.screenheight, "[<]/[>] Move Cursor [S]et/Clear [R]eset [E]rase [I]ncrement [ESC]");
	do {
		bufprintf(buf,15,"(%02d)",x+1);
		puttext(1,ti.screenheight-2,4,ti.screenheight-2,buf);
		memset(buf,0,sizeof(buf));
		for (y = 1; y <= 80; y++) {
			if (tabs[y] == TRUE) {
				buf[(y-1)*2]=254;
				buf[(y-1)*2+1]=7;
			} else {
				buf[(y-1)*2]=250;
				buf[(y-1)*2+1]=7;
			}
		}
		puttext(1,ti.screenheight-1,80,ti.screenheight-1,buf);
		if(wherex()!=x+1 || wherey() != ti.screenheight - 1)
			gotoxy( x+1,ti.screenheight - 1);
		ch = newgetch();
		if(ch == CIO_KEY_MOUSE) {
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
			if(me.endy==ti.screenheight-1) {
				x=me.endx-1;
				if(me.event==CIOLIB_BUTTON_1_CLICK) {
					tabs[x + 1] = !tabs[x + 1];
					ch=13;
				}
			}
			if(me.endy==ti.screenheight && me.event==CIOLIB_BUTTON_1_CLICK) {
				if(me.endx==2)
					ch=CIO_KEY_LEFT;
				if(me.endx==6)
					ch=CIO_KEY_RIGHT;
				if(me.endx>=21 && me.endx<=31)
					ch='S';
				if(me.endx>=33 && me.endx<=39)
					ch='R';
				if(me.endx>=41 && me.endx<=47)
					ch='E';
				if(me.endx>=49 && me.endx<=59)
					ch='I';
				if(me.endx>=61 && me.endx<=65)
					ch=27;
			}
		}
		switch (ch) {
		case 'S':
		case 's':
			tabs[x + 1] = !tabs[x + 1];
			break;
		case 'I':
		case 'i':
			gettext(10, ti.screenheight - 2, 30, ti.screenheight - 2, buf);
			CoolWrite(10, ti.screenheight - 2, "Increment (1-79) :");
			str = "";
			str = inputfield(str, 2, 27, ti.screenheight - 3);
			ax = strtol(str, NULL, 0);
			for (y = x; y <= 80; y++)
				if (((y - x) % ax) == 0)
					tabs[y] = TRUE;
			puttext(10, ti.screenheight - 2, 30, ti.screenheight - 2, buf);
			break;
		case 'R':
		case 'r':
			for (y = 1; y <= 80; y++)
				if (y % 8 == 0)
					tabs[y] = TRUE;
				else
					tabs[y] = FALSE;
			break;
		case 'E':
		case 'e':
			for (y = 1; y <= 80; y++)
				tabs[y] = FALSE;
			break;
		case 9:
			x = tabforward(x);
			break;
		case 27:
#if 0		/* ToDo do something about this mess (Backtab weirdness!) */
			ch = getch();
			if(ch==0 || ch==0xe0)
				ch|=getch()<<8;
			if(ch==CIO_KEY_MOUSE)
				getmouse(&me);
			if (ch == 9)
				x = tabback(x);
			else
				ch = 27;
#endif
			break;
		case CIO_KEY_LEFT:
			if (x > 0)
				x--;
			else
				x=79;
			break;
		case CIO_KEY_RIGHT:
			if (x < 79)
				x++;
			else
				x=0;
			break;
		}
	} while (ch != 27);
}
