#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gen_defs.h>
#include <ciolib.h>

#include "config.h"
#include "effekt.h"
#include "key.h"
#include "syncdraw.h"
#include "miscfunctions.h"

char 
auswahl(int num, int col, int first)
{
	char            ac[10], ab[10];
	int             ch, x, y, z, i;
	struct			text_info	ti;
	struct			mouse_event	me;
	char			buf[80*2];

	gettextinfo(&ti);
	while(kbhit()) {
		ch=newgetch();
		if(ch==CIO_KEY_MOUSE)
			getmouse(&me);
	}
	for (y = 1; y <= num; y++) {
		for (x = 0; stri[y][x] > 0; x++) {
			if (stri[y][x] >= 'A' && stri[y][x] <= 'Z') {
				ab[y] = x;
				ac[y] = stri[y][x];
				break;
			};
		}
	}
	x = first;
	do {
		i = 0;
		for (y = 1; y <= num; y++) {
			if (x == y) {
				ch = 0;
				do {
					buf[i++]=toupper(stri[y][ch++]);
					buf[i++]=(ch == ab[y]+1)?(15+16):0x1B;
				} while (stri[y][ch] != 0);
			} else {
				ch = 0;
				do {
					buf[i++]=tolower(stri[y][ch++]);
					buf[i++]=(ch == ab[y]+1)?7:8;
				} while (stri[y][ch] != 0);
			}
			buf[i++]=' ';
			buf[i++]=0;
			buf[i++]=' ';
			buf[i++]=0;
		}
		puttext(col+1, ti.screenheight, col+(i/2), ti.screenheight, buf);
		ch = newgetch();
		if (ch == 27)
			ch = 65;
		if(ch==CIO_KEY_MOUSE) {
			getmouse(&me);
			if(me.endy==ti.screenheight) {
				z=col+1;
				for (y = 1; y <= num; y++) {
					if(me.endx<z)
						break;
					if(me.endx<z+(int)strlen(stri[y])) {
						x=y;
						if(me.event==CIOLIB_BUTTON_1_CLICK)
							ch=13;
						break;
					}
					z+=strlen(stri[y])+2;
				}
			}
		}
		for (y = 1; y <= num; y++)
			if (toupper(ch) == ac[y]) {
				x = y;
				ch = 13;
			}
		switch (ch) {
		case CIO_KEY_LEFT:
			x--;
			break;
		case CIO_KEY_RIGHT:
			x++;
			break;
		}
		if (x < 1)
			x = num;
		if (x > num)
			x = 1;
	} while (ch != 13);
	return x;
}

void 
clrline(void)
{
	struct			text_info	ti;
	char		buf[160];

	memset(buf,0,sizeof(buf));
	gettextinfo(&ti);
	puttext(1,ti.screenheight,80,ti.screenheight,buf);
}

char 
ReadChar(void)
{
#if 0
	int             ch, b;
	struct			text_info	ti;
	struct			mouse_event	me;

	gettextinfo(&ti);
	clrline();
	CoolWrite(1, ti.screenheight, "Enter Character :");
	do {
		ch = newgetch();
		if(ch==CIO_KEY_MOUSE)
			getmouse(&me);
		for (b = 1; b <= 10; b++)
			if (ch == CIO_KEY_F(b))
				return CharSet[ActiveCharset][b - 1];
	} while ((ch > 255));
	return ch;
#else
	return(asciitable());
#endif
}

void 
erasescreen(void)
{
	int             x, y;
	for (y = 0; y <= MaxLines; y++)
		for (x = 0; x < 80; x++) {
			Screen[ActivePage][y][x << 1] = 32;
			Screen[ActivePage][y][(x << 1) + 1] = 7;
		}
}

void 
ClearScreen(void)
{
	int             x;
	struct			text_info	ti;

	gettextinfo(&ti);
	stri[1] = "Yes";
	stri[2] = "No";
	clrline();
	CoolWrite(1, ti.screenheight, "Clear Screen :");
	x = auswahl(2, 15, 1);
	Undo = FALSE;
	if (x == 1)
		erasescreen();
	SaveScreen();
}

void 
lefttrim(void)
{
	int             y, d;
	for (y = 0; y <= LastLine; y++) {
		d = 0;
		while ((Screen[ActivePage][y][0] == 32) & (d < 158)) {
			d++;
			memmove(&Screen[ActivePage][y][0], &Screen[ActivePage][y][2], 158);
			Screen[ActivePage][y][158] = 32;
			Screen[ActivePage][y][159] = 7;
		}
	}
}
void 
righttrim(void)
{
	int             y, d;
	for (y = 0; y <= LastLine; y++) {
		d = 0;
		while ((Screen[ActivePage][y][158] == 32) & (d < 158)) {
			d++;
			memmove(&Screen[ActivePage][y][2], &Screen[ActivePage][y][0], 158);
			Screen[ActivePage][y][0] = 32;
			Screen[ActivePage][y][1] = 7;
		}
	}
}

void 
center(void)
{
	int             x, y, d=0;
	lefttrim();
	for (y = 0; y <= LastLine; y++) {
		for (x = 0; x <= 79; x++)
			if (Screen[ActivePage][y][x << 1] != 32)
				d = (80 - x) >> 1;
		while (d-- > 0) {
			memmove(&Screen[ActivePage][y][2], &Screen[ActivePage][y][0], 158);
			Screen[ActivePage][y][0] = 32;
			Screen[ActivePage][y][1] = 7;
		}
	}
}

void 
elitetrim(void)
{
	int             x, y;
	for (y = 0; y <= LastLine; y++)
		for (x = 0; x <= 79; x++)
			Screen[ActivePage][y][x << 1] = translate(Screen[ActivePage][y][x << 1]);
}

void 
global(void)
{
	int             y, x, ch;
	struct			text_info	ti;

	gettextinfo(&ti);
	stri[1] = "Fill";
	stri[2] = "Copy";
	stri[3] = "Text";
	stri[4] = "Abort";
	clrline();
	CoolWrite(1, ti.screenheight, "Global :");
	x = auswahl(4, 10, 1);
	switch (x) {
	case 1:
		stri[1] = "Character";
		stri[2] = "aTribute";
		stri[3] = "Fore";
		stri[4] = "Back";
		stri[5] = "Abort";
		clrline();
		CoolWrite(1, ti.screenheight, "Fill :");
		x = auswahl(5, 8, 1);
		switch (x) {
		case 1:
			ch = ReadChar();
			for (y = 0; y <= MaxLines; y++)
				for (x = 0; x < 80; x++)
					Screen[ActivePage][y][x << 1] = ch;
			break;
		case 2:
			for (y = 0; y <= MaxLines; y++)
				for (x = 0; x < 80; x++)
					Screen[ActivePage][y][(x << 1) + 1] = Attribute;
			break;
		case 3:
			for (y = 0; y <= MaxLines; y++)
				for (x = 0; x < 80; x++)
					Screen[ActivePage][y][(x << 1) + 1] = (Attribute & 15) + (Screen[ActivePage][y][x * 2 + 1] & 240);
			break;
		case 4:
			for (y = 0; y <= MaxLines; y++)
				for (x = 0; x < 80; x++)
					Screen[ActivePage][y][(x << 1) + 1] = (Attribute & 240) + (Screen[ActivePage][y][x * 2 + 1] & 15);
			break;
		}
		break;
	case 2:
		stri[1] = "1";
		stri[2] = "2";
		stri[3] = "Abort";
		clrline();
		CoolWrite(1, ti.screenheight, "Copy to page :");
		switch (auswahl(3, 15, 1)) {
		case 1:
			CopyScreen(ActivePage, 1);
			break;
		case 2:
			CopyScreen(ActivePage, 2);
			break;
		};
		break;
	case 3:
		stri[1] = "Left";
		stri[2] = "Right";
		stri[3] = "Center";
		stri[4] = "Elite";
		stri[5] = "eFfect";
		stri[6] = "Abort";
		clrline();
		CoolWrite(1, ti.screenheight, "Text :");
		switch (auswahl(6, 8, 1)) {
		case 1:
			lefttrim();
			break;
		case 2:
			righttrim();
			break;
		case 3:
			center();
			break;
		case 4:
			elitetrim();
			break;
		case 5:
			draweffect(0, 0, 79, LastLine);
			break;
		}
	}
}

void 
SetPage(void)
{
	struct			text_info	ti;

	gettextinfo(&ti);
	stri[1] = "1";
	stri[2] = "2";
	clrline();
	CoolWrite(1, ti.screenheight, "Set Page :");
	ActivePage = auswahl(2, 12, ActivePage);
}

void 
UndoLast(void)
{
	struct			text_info	ti;

	gettextinfo(&ti);
	if (Undo != FALSE) {
		stri[1] = "Yes";
		stri[2] = "No";
		clrline();
		CoolWrite(1, ti.screenheight, "Undo :");
		if (auswahl(2, 7, ActivePage) == 1) {
			CopyScreen(UNDOPage, ActivePage);
		}
	}
}
void 
selectdrawmode(void)
{
	int             a;
	struct			text_info	ti;

	gettextinfo(&ti);
	stri[1] = "Character";
	stri[2] = "aTribute";
	stri[3] = "Fore";
	stri[4] = "Back";
	stri[5] = "Abort";
	clrline();
	CoolWrite(1, ti.screenheight, "Select drawmode :");
	DrawMode = 0xFF00;
	a = auswahl(5, 19, 1);
	switch (a) {
	case 1:
		a = ReadChar();
		DrawMode = 0x0100 + (a & 255);
		break;
	case 2:
		DrawMode = 0x0200 + Attribute;
		break;
	case 3:
		DrawMode = Attribute & 15;
		DrawMode += 0x0300;
		break;
	case 4:
		DrawMode = Attribute & 240;
		DrawMode += 0x0400;
		break;
	}
}

int 
exitprg(void)
{
	struct			text_info	ti;

	gettextinfo(&ti);
	stri[1] = "Yes";
	stri[2] = "No";
	clrline();
	CoolWrite(1, ti.screenheight, "Sure ? ");
	switch (auswahl(2, 8, 2)) {
	case 1:
		saveconfig();
		clrscr();
		gotoxy(1, 1);
		printf("Thanx 4 using this syncdraw\n");
		return(-1);
	}
	return(0);
}
