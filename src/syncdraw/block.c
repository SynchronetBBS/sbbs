#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <gen_defs.h>
#include <ciolib.h>

#include "effekt.h"
#include "key.h"
#include "syncdraw.h"
#include "miscfunctions.h"
#include "options.h"
#define COPYPage 3

int             X1, Y1, X2, Y2;

void 
EraseBlock(void)
{
	int             x, y;
	for (x = X1; x <= X2; x++)
		for (y = Y1; y <= Y2; y++) {
			Screen[ActivePage][y][x << 1] = 32;
			Screen[ActivePage][y][(x << 1) + 1] = 7;
		}
}

void 
StampBlock(char under)
{
	int             x, y;
	unsigned char   c;
/*	ToDo Find out what he meant by this...
 *	if (CursorY + FirstLine + Y2 - Y1 > LastLine)
 *		LastLine = CursorY + FirstLine + y - Y1;
 */
	for (y = Y1; y <= Y2; y++)
		for (x = X1; x <= X2; x++)
			if (CursorX + (x - X1) < 80) {
				if (under) {
					c = Screen[ActivePage][CursorY + FirstLine + y - Y1][(CursorX + x - X1) << 1];
					if ((c == 32) | (c == 0)) {
						Screen[ActivePage][CursorY + FirstLine + y - Y1][(CursorX + x - X1) << 1] = Screen[COPYPage][y][x << 1];
						Screen[ActivePage][CursorY + FirstLine + y - Y1][((CursorX + x - X1) << 1) + 1] = Screen[COPYPage][y][(x << 1) + 1];
					}
				} else {
					Screen[ActivePage][CursorY + FirstLine + y - Y1][(CursorX + x - X1) << 1] = Screen[COPYPage][y][x << 1];
					Screen[ActivePage][CursorY + FirstLine + y - Y1][((CursorX + x - X1) << 1) + 1] = Screen[COPYPage][y][(x << 1) + 1];
				}
			}
}

void 
CopyBlock(char Mode)
{
	char            under = 0;
	int             x, y, ch, ymax = 22;
	struct			text_info	ti;
	struct			mouse_event	me;
	char	*buf;

	gettextinfo(&ti);
	if (FullScreen)
		ymax++;
	ymax += (ti.screenheight - 25);
	SaveScreen();
	Undo = TRUE;
	clrline();
	CodeWrite(1, ti.screenheight, "[S]tamp [P]age [U]nder [X]/[Y] Flip [RETURN] [ESC]");
	for (x = X1; x <= X2; x++)
		for (y = Y1; y <= Y2; y++) {
			Screen[COPYPage][y][x << 1] = Screen[ActivePage][y][x << 1];
			Screen[COPYPage][y][(x << 1) + 1] = Screen[ActivePage][y][(x << 1) + 1];
		}
	if (Mode == 2)
		EraseBlock();
	/*
	 * CursorX=X1; CursorY=Y1%24; FirstLine=Y1-CursorY;
	 */

	Dragging=TRUE;
	do {
		buf=(char *)malloc(80*ti.screenheight*2);

		memcpy(buf,Screen[ActivePage][FirstLine],80*ti.screenheight*2);
		for (y=Y1;y<=Y2;y++) {
			if(CursorY+y-Y1>=ymax)
				continue;
			for(x=X1;x<=X2;x++) {
				if(CursorX+x-X1>=80)
					continue;
				if(under) {
					if(buf[((CursorY+y-Y1))*160+(CursorX+x-X1)*2]==32 || buf[((CursorY+y-Y1))*160+(CursorX+x-X1)*2]==0) {
						buf[((CursorY+y-Y1))*160+(CursorX+x-X1)*2]=Screen[COPYPage][y][x*2];
						buf[((CursorY+y-Y1))*160+(CursorX+x-X1)*2+1]=Screen[COPYPage][y][x*2+1];
					}
				}
				else {
					buf[((CursorY+y-Y1))*160+(CursorX+x-X1)*2]=Screen[COPYPage][y][x*2];
					buf[((CursorY+y-Y1))*160+(CursorX+x-X1)*2+1]=Screen[COPYPage][y][x*2+1];
				}
			}
		}
		if(FullScreen)
			puttext(1,1,80,ti.screenheight-1,buf);
		else
			puttext(1,2,80,ti.screenheight-1,buf);
		free(buf);

		Statusline();
		Colors(Attribute);
		if (FullScreen)
			gotoxy( CursorX+1,CursorY+1);
		else
			gotoxy( CursorX+1,CursorY + 1+1);
		ch = newgetch();
		CursorHandling(ch);
		if (ch==CIO_KEY_MOUSE) {
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_1_CLICK)
				ch = 13;
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch = 's';
		}
		switch (ch) {
		case 27:
			CopyScreen(UNDOPage, ActivePage);
			/*
			 * memcpy(&Screen[ActivePage],
			 * &Screen[UNDOPage],sizeof(Screen[ActivePage]));
			 */
			break;
		case 'y':
			for (y = Y1; y <= Y1 + ((Y2 - Y1) / 2); y++) {
				memcpy(&Screen[COPYPage][Y1 - 1][X1 * 2], &Screen[COPYPage][y][X1 * 2], (X2 - X1 + 1) * 2);
				memcpy(&Screen[COPYPage][y][X1 * 2], &Screen[COPYPage][Y2 - (y - Y1)][X1 * 2], (X2 - X1 + 1) * 2);
				memcpy(&Screen[COPYPage][Y2 - (y - Y1)][X1 * 2], &Screen[COPYPage][Y1 - 1][X1 * 2], (X2 - X1 + 1) * 2);
			}
			break;
		case 'x':
			for (x = X1; x <= X1 + ((X2 - X1) / 2); x++)
				for (y = Y1; y <= Y2; y++) {
					Screen[COPYPage][y][(X1 - 1) * 2] = Screen[COPYPage][y][x * 2];
					Screen[COPYPage][y][((X1 - 1) * 2) + 1] = Screen[COPYPage][y][(x * 2) + 1];
					Screen[COPYPage][y][x * 2] = Screen[COPYPage][y][(X2 - (x - X1)) * 2];
					Screen[COPYPage][y][(x * 2) + 1] = Screen[COPYPage][y][((X2 - (x - X1)) * 2) + 1];
					Screen[COPYPage][y][(X2 - (x - X1)) * 2] = Screen[COPYPage][y][(X1 - 1) * 2];
					Screen[COPYPage][y][((X2 - (x - X1)) * 2) + 1] = Screen[COPYPage][y][((X1 - 1) * 2) + 1];
				}

			break;
		case 'u':
			under = !under;
			break;
		case 'p':
			if (++ActivePage > 2)
				ActivePage = 1;
			break;
		case 's':
			StampBlock(under);
			break;
		case 13:
			StampBlock(under);
			ch = 27;
			break;
		}
		if (CursorY > ymax) {
			CursorY = ymax;
			FirstLine++;
		}
		CursorCheck();
	} while (ch != 27);
	Dragging=FALSE;
}

void 
blocklefttrim(void)
{
	int             y, d;
	for (y = Y1; y <= Y2; y++) {
		d = 0;
		while ((Screen[ActivePage][y][X1 << 1] == 32) & (d < (X2 - X1))) {
			d++;
			memmove(&Screen[ActivePage][y][X1 << 1], &Screen[ActivePage][y][(X1 << 1) + 2], (X2 - X1) << 1);
			Screen[ActivePage][y][X2 << 1] = 32;
			Screen[ActivePage][y][(X2 << 1) + 1] = 7;
		}
	}
}
void 
blockrighttrim(void)
{
	int             y, d;
	for (y = Y1; y <= Y2; y++) {
		d = 0;
		while ((Screen[ActivePage][y][X2 << 1] == 32) & (d < X2 - X1)) {
			d++;
			memmove(&Screen[ActivePage][y][(X1 << 1) + 2], &Screen[ActivePage][y][X1 << 1], (X2 - X1) << 1);
			Screen[ActivePage][y][X1 << 1] = 32;
			Screen[ActivePage][y][(X1 << 1) + 1] = 7;
		}
	}
}
void 
blockcenter(void)
{
	int             x, y, d = 0;
	blocklefttrim();
	for (y = Y1; y <= Y2; y++) {
		for (x = X1; x <= X2; x++)
			if ((Screen[ActivePage][y][x << 1] == 32))
				d++;
		d /= 2;
		while (d >= 1) {
			d--;
			memmove(&Screen[ActivePage][y][(X1 << 1) + 2], &Screen[ActivePage][y][X1 << 1], (X2 - X1) << 1);
			Screen[ActivePage][y][X1 << 1] = 32;
			Screen[ActivePage][y][(X1 << 1) + 1] = 7;
		}
	}

}
void 
blockelite(void)
{
	int             x, y;
	for (x = X1; x <= X2; x++)
		for (y = Y1; y <= Y2; y++)
			Screen[ActivePage][y][x << 1] = translate(Screen[ActivePage][y][x << 1]);
}

void 
blockmode(void)
{
	int             x1, y1, ch;
	int             x, y, maxy = 22;
	struct			text_info	ti;
	struct			mouse_event	me;
	char *buf;

	gettextinfo(&ti);
	if (FullScreen)
		maxy++;
	maxy += ti.screenheight - 25;
	clrline();
	CodeWrite(1, ti.screenheight, "[C]opy [M]ove [F]ill [E]rase [D]elete [O]utline [T]ext [ESC]");
	x1 = CursorX;
	y1 = CursorY + FirstLine;
	do {
		if (CursorY + FirstLine > y1) {
			Y1 = y1;
			Y2 = CursorY + FirstLine;
		} else {
			Y2 = y1;
			Y1 = CursorY + FirstLine;
		}
		if (CursorX > x1) {
			X1 = x1;
			X2 = CursorX;
		} else {
			X2 = x1;
			X1 = CursorX;
		}

		buf=(char *)malloc(80*ti.screenheight*2);
		memcpy(buf,Screen[ActivePage][FirstLine],80*ti.screenheight*2);
		for(y=Y1-FirstLine;y<=Y2-FirstLine;y++) {
			for(x=X1;x<=X2;x++) {
				buf[y*160+x*2+1]=112;
			}
		}
		if(FullScreen)
			puttext(1,1,80,ti.screenheight-1,buf);
		else
			puttext(1,2,80,ti.screenheight-1,buf);
		free(buf);

		Statusline();
		Colors(Attribute);
		if (FullScreen)
			gotoxy( CursorX+1,CursorY+1);
		else
			gotoxy( CursorX+1,CursorY + 1+1);
		ch = newgetch();
		CursorHandling(ch);
		if(ch==CIO_KEY_MOUSE) {
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
			if(me.event==CIOLIB_BUTTON_1_CLICK) {
				if(me.endy==ti.screenheight) {
					if(me.endx >= 1 && me.endx<=6)
						ch='c';
					if(me.endx >= 8 && me.endx<=13)
						ch='m';
					if(me.endx >= 15 && me.endx<=20)
						ch='f';
					if(me.endx >= 22 && me.endx<=28)
						ch='e';
					if(me.endx >= 30 && me.endx<=37)
						ch='d';
					if(me.endx >= 39 && me.endx<=47)
						ch='o';
					if(me.endx >= 49 && me.endx<=54)
						ch='t';
					if(me.endx >= 56 && me.endx<=60)
						ch=27;
				}
			}
		}
		switch (tolower(ch)) {
		case 'm':
			CopyBlock(2);
			ch = 27;
			break;
		case 'c':
			CopyBlock(1);
			ch = 27;
			break;
		case 'e':	/* ERASE BLOCK */
			EraseBlock();
			ch = 27;
			break;
		case 'd':	/* DELETE BLOCK */
			EraseBlock();
			for (y = Y1; y <= Y2; y++) {
				memcpy(&Screen[ActivePage][y][(X1 << 1)],
				       &Screen[ActivePage][y][(X2 << 1) + 2], (79 - X2) << 1);
				for (x = 79 - (X2 - X1); x <= 79; x++) {
					Screen[ActivePage][y][x << 1] = 32;
					Screen[ActivePage][y][(x << 1) + 1] = 7;
				}
			}
			ch = 27;
			break;
		case 'o':	/* OUTLINE */
			for (y = Y1; y <= Y2; y++) {
				Screen[ActivePage][y][X1 << 1] = CharSet[ActiveCharset][5];
				Screen[ActivePage][y][X2 << 1] = CharSet[ActiveCharset][5];
				Screen[ActivePage][y][(X1 << 1) + 1] = Attribute;
				Screen[ActivePage][y][(X2 << 1) + 1] = Attribute;
			}
			for (x = X1; x <= X2; x++) {
				Screen[ActivePage][Y1][x << 1] = CharSet[ActiveCharset][4];
				Screen[ActivePage][Y2][x << 1] = CharSet[ActiveCharset][4];
				Screen[ActivePage][Y1][(x << 1) + 1] = Attribute;
				Screen[ActivePage][Y2][(x << 1) + 1] = Attribute;
			}
			Screen[ActivePage][Y1][X1 << 1] = CharSet[ActiveCharset][0];
			Screen[ActivePage][Y1][X2 << 1] = CharSet[ActiveCharset][1];
			Screen[ActivePage][Y2][X1 << 1] = CharSet[ActiveCharset][2];
			Screen[ActivePage][Y2][X2 << 1] = CharSet[ActiveCharset][3];
			ch = 27;
			break;
		case 't':	/* TEXT */
			clrline();
			CodeWrite(1, ti.screenheight, "[L]eft [C]enter [R]ight [E]lite e[F]fect [ESC]");
			do {
				ch = newgetch();
				if(ch==CIO_KEY_MOUSE) {
					getmouse(&me);
					if(me.event==CIOLIB_BUTTON_3_CLICK)
						ch=27;
					if(me.event==CIOLIB_BUTTON_1_CLICK) {
						if(me.endy==ti.screenheight) {
							if(me.endx >= 1 && me.endx<=6)
								ch='l';
							if(me.endx >= 8 && me.endx<=15)
								ch='c';
							if(me.endx >= 17 && me.endx<=23)
								ch='r';
							if(me.endx >= 25 && me.endx<=31)
								ch='e';
							if(me.endx >= 33 && me.endx<=40)
								ch='f';
							if(me.endx >= 42 && me.endx<=46)
								ch=27;
						}
					}
				}
				switch (tolower(ch)) {
				case 'f':
					draweffect(X1, Y1, X2, Y2);
					ch = 27;
					break;
				case 'e':
					blockelite();
					ch = 27;
					break;
				case 'l':
					blocklefttrim();
					ch = 27;
					break;
				case 'r':
					blockrighttrim();
					ch = 27;
					break;
				case 'c':
					blockcenter();
					ch = 27;
					break;
				}
			} while (ch != 27);
			break;
		case 'f':	/* FiLL */
			clrline();
			CodeWrite(1, ti.screenheight, "[C]haracter [A]ttribute [F]ore [B]ack [ESC]");
			do {
				ch = newgetch();
				if(ch==CIO_KEY_MOUSE) {
					getmouse(&me);
					if(me.event==CIOLIB_BUTTON_3_CLICK)
						ch=27;
					if(me.event==CIOLIB_BUTTON_1_CLICK) {
						if(me.endy==ti.screenheight) {
							if(me.endx >= 1 && me.endx<=11)
								ch='c';
							if(me.endx >= 13 && me.endx<=23)
								ch='a';
							if(me.endx >= 25 && me.endx<=30)
								ch='f';
							if(me.endx >= 32 && me.endx<=37)
								ch='b';
							if(me.endx >= 39 && me.endx<=43)
								ch=27;
						}
					}
				}
				switch (tolower(ch)) {
				case 'c':
					ch = ReadChar();
					for (x = X1; x <= X2; x++)
						for (y = Y1; y <= Y2; y++)
							Screen[ActivePage][y][x << 1] = ch;
					ch = 27;
					break;
				case 'a':
					for (x = X1; x <= X2; x++)
						for (y = Y1; y <= Y2; y++)
							Screen[ActivePage][y][(x << 1) + 1] = Attribute;
					ch = 27;
					break;
				case 'f':
					for (x = X1; x <= X2; x++)
						for (y = Y1; y <= Y2; y++)
							Screen[ActivePage][y][(x << 1) + 1] = (Attribute & 15) + (Screen[ActivePage][y][(x << 1) + 1] & 240);
					ch = 27;
					break;
				case 'b':
					for (x = X1; x <= X2; x++)
						for (y = Y1; y <= Y2; y++)
							Screen[ActivePage][y][(x << 1) + 1] = (Attribute & 240) + (Screen[ActivePage][y][(x << 1) + 1] & 15);
					ch = 27;
					break;
				}
			} while (ch != 27);
			break;
		}
		if (CursorY > maxy) {
			CursorY = maxy;
			FirstLine++;
		}
		CursorCheck();
	} while (ch != 27);
}
