#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <ciolib.h>

#include "crt.h"
#include "key.h"
#include "syncdraw.h"
#include "effekt.h"
#include "miscfunctions.h"

teffekt         effect;

void 
draweffekt(int xpos, int ypos, int effekt, char *blabla, int highlite)
{
	int             x, y = 0, i, l, p;
	char            col;
	char			*buf;

	col = effect.Colortable[1][1];
	l=strlen(blabla);
	buf=(char *)malloc(l*2);
	if (!buf)
		exit(1);

	p=0;
	for (x = 0; x < l; x++) {
		y++;
		switch (effekt) {
		case 1:
			col = effect.Colortable[1][1];
			if (blabla[x] >= '0' && blabla[x] <= '9')
				col = effect.Colortable[1][2];
			if (blabla[x] >= 'A' && blabla[x] <= 'Z')
				col = effect.Colortable[1][3];
			if (blabla[x] >= 'a' && blabla[x] <= 'z')
				col = effect.Colortable[1][4];
			if (blabla[x] < 0)
				col = effect.Colortable[1][5];
			break;
		case 2:
			if (blabla[x] == ' ')
				y = 0;
			switch (y) {
			case 1:
				col = effect.Colortable[2][1];
				break;
			case 2:
				col = effect.Colortable[2][2];
				break;
			case 3:
				col = effect.Colortable[2][3];
				break;
			case 4:
				col = effect.Colortable[2][4];
				break;
			default:
				col = effect.Colortable[2][5];
			}
			break;
		case 3:
			col = effect.Colortable[3][5];
			for (i = 0; i < 4; i++)
				if (x == 0 + i || x == l - i - 1)
					col = effect.Colortable[3][1 + i];
			break;
		case 4:
			if (x % 2 == 0)
				col = effect.Colortable[4][1];
			else
				col = effect.Colortable[4][2];
			break;
		}
		buf[p++]=blabla[x];
		buf[p++]=col+(highlite?16:0);
	}
	puttext(xpos,ypos,xpos+l-1,ypos,buf);
	free(buf);
}

void 
changecolor(int Effekt)
{
	int             ch, x = 1, y, colnum = 5;
	struct			mouse_event	me;
	char			buf[12*5*2];
	char			sbuf[7*14*2];

	gettext(40, 10, 53, 16, sbuf);
	DrawBox(40, 10, 53, 16);
	memset(buf,0,sizeof(buf));
	for (y = 1; y <= 5; y++) {
		bufprintf(buf+((y-1)*12)*2,7,"Color %d:", y);
		buf[((y-1)*12+8)*2+1]=15;
		buf[((y-1)*12+9)*2]='#';
		buf[((y-1)*12+10)*2]='#';
		buf[((y-1)*12+11)*2+1]=15;
	}
	do {
		draweffekt(12,11+Effekt, Effekt, "ABCabc123!@# Sample", 0);
		for (y = 1; y <= colnum; y++) {
			if (y == x) {
				buf[((y-1)*12+8)*2]='>';
				buf[((y-1)*12+11)*2]='<';
			}
			else {
				buf[((y-1)*12+8)*2]=' ';
				buf[((y-1)*12+11)*2]=' ';
			}
			buf[((y-1)*12+9)*2+1]=effect.Colortable[Effekt][y];
			buf[((y-1)*12+10)*2+1]=effect.Colortable[Effekt][y];
		}
		puttext(41,11,52,15,buf);
		ch = newgetch();
		switch (ch) {
		case CIO_KEY_MOUSE:
			/* ToDo Add (good) Mouse stuff */
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
			if(me.endx==49 && me.endy>=11 && me.endy<=15
					&& me.event==CIOLIB_BUTTON_1_CLICK) {
				x=me.endy-10;
				effect.Colortable[Effekt][x]--;
				if (effect.Colortable[Effekt][x] < 0)
					effect.Colortable[Effekt][x] = 15;
			}
			if(me.endx >= 50 && me.endx <= 51
					 && me.endy>=11 && me.endy<=15) {
				x=me.endy-10;
				if(me.event==CIOLIB_BUTTON_1_CLICK)
					ch=13;
			}
			if(me.endx==52 && me.endy>=11 && me.endy<=15
					&& me.event == CIOLIB_BUTTON_1_CLICK) {
				x=me.endy-10;
				effect.Colortable[Effekt][x]++;
				if (effect.Colortable[Effekt][x] > 15)
					effect.Colortable[Effekt][x] = 0;
			}

			break;
		case CIO_KEY_LEFT:
			effect.Colortable[Effekt][x]--;
			if (effect.Colortable[Effekt][x] < 0)
				effect.Colortable[Effekt][x] = 15;
			break;
		case CIO_KEY_RIGHT:
			effect.Colortable[Effekt][x]++;
			if (effect.Colortable[Effekt][x] > 15)
				effect.Colortable[Effekt][x] = 0;
			break;
		case CIO_KEY_UP:
			x--;
			if (x == 0)
				x = colnum;
			break;
		case CIO_KEY_DOWN:
			x++;
			if (x > colnum)
				x = 1;
			break;
		}

	} while (ch != 27 && ch != 13);
	puttext(40, 10, 53, 16, sbuf);
}

void 
draweffect(int x1, int y1, int x2, int y2)
{
	int             x, y, c, Left, Right, i;
	char            a, col;

	Right=x2;
	col = effect.Colortable[1][1];
	for (y = y1; y <= y2; y++) {
		c = 0;
		Left = x1;
		for (x = x1; x <= x2; x++)
			if (Screen[ActivePage][y][x << 1] != ' ') {
				Left = x;
				break;
			}
		for (x = x2; x >= x1; x--)
			if (Screen[ActivePage][y][x << 1] != ' ') {
				Right = x;
				break;
			}
		for (x = x1; x <= x2; x++) {
			a = Screen[ActivePage][y][x << 1];
			c++;
			switch (effect.Effekt) {
			case 1:
				col = effect.Colortable[1][1];
				if (a >= '0' && a <= '9')
					col = effect.Colortable[1][2];
				if (a >= 'A' && a <= 'Z')
					col = effect.Colortable[1][3];
				if (a >= 'a' && a <= 'z')
					col = effect.Colortable[1][4];
				if (a < 0)
					col = effect.Colortable[1][5];
				break;
			case 2:
				if (a <= ' ' && a >= 0)
					c = 0;
				switch (c) {
				case 1:
					col = effect.Colortable[2][1];
					break;
				case 2:
					col = effect.Colortable[2][2];
					break;
				case 3:
					col = effect.Colortable[2][3];
					break;
				case 4:
					col = effect.Colortable[2][4];
					break;
				default:
					col = effect.Colortable[2][5];
				}
				break;
			case 3:
				col = effect.Colortable[3][5];
				for (i = 0; i < 4; i++)
					if (x == Left + i || x == Right - i)
						col = effect.Colortable[3][1 + i];
				break;
			case 4:
				if (x % 2 == 0)
					col = effect.Colortable[4][1];
				else
					col = effect.Colortable[4][2];
				break;

			}
			Screen[ActivePage][y][(x << 1) + 1] = col;
		}
	}

}

void 
select_effekt(void)
{
	int             ch, x, y = effect.Effekt;
	struct 			mouse_event	me;
	char			buf[19*2];

	if (y < 1)
		y = 1;
	DrawBox(11, 11, 31, 16);
	bufprintf(buf,7,"c - change color   ");
	puttext(12, 15, 30, 15, buf);
	do {
		for (x = 1; x <= 3; x++) {
			if (y == x)
				draweffekt(12,11+x,x, "ABCabc123!@# Sample", 1);
			else
				draweffekt(12,11+x,x, "ABCabc123!@# Sample", 0);
		}
		ch = newgetch();
		switch (ch) {
		case CIO_KEY_MOUSE:
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
			if(me.endx>=12 && me.endx<=32) {
				if(me.endy >= 12 && me.endy<=14) {
					y = me.endy-11;
					if(me.event==CIOLIB_BUTTON_1_CLICK)
						ch=13;
				}
				if(me.endy == 15 && me.event==CIOLIB_BUTTON_1_CLICK)
					changecolor(y);
			}
			break;
		case 'C':
		case 'c':
			changecolor(y);
			break;
		case CIO_KEY_UP:
			y--;
			if (y == 0)
				y = 3;
			break;
		case CIO_KEY_DOWN:
			y++;
			if (y == 4)
				y = 1;
			break;
		}
	} while (ch != 27 && ch != 13);
	if (ch == 13)
		effect.Effekt = y;
}
