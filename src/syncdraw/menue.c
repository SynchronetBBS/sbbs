#include <ciolib.h>
#include <gen_defs.h>
#include <string.h>

#include "options.h"
#include "save.h"
#include "load.h"
#include "key.h"
#include "crt.h"
#include "fonts.h"
#include "syncdraw.h"
#include "miscfunctions.h"
#include "sauce.h"
#include "effekt.h"
#include "draw.h"
#include "help.h"
#include "tabulator.h"
#include "attrs.h"

unsigned char   ActiveMenue = 1, MaxItem;
char           *MenueItem[20], Length;

int 
Menues(int x, int y)
{
	int             ch;
	int             a, b, c, d, i;
	struct			mouse_event	me;
	char			buf[80*25*2];

	DrawBox(x, y, x + Length + 1, y + MaxItem + 1);
	b = 1;
	c = 255;
	do {
		if (c != b) {
			i=0;
			for (a = 1; a <= MaxItem; a++) {
				for (d = 1; d <= 2; d++) {
					buf[i++]=MenueItem[a][d];
					buf[i++]=(b==a)?ATTR_NORM_BAR_CURR:ATTR_NORM_HIGH;
				}
				for (d = 3; d < Length - 6; d++) {
					buf[i++]=MenueItem[a][d];
					buf[i++]=(b == a)?ATTR_NORM_BAR_CURR:ATTR_NORM_LOW;
				}
				for (d = Length - 6; d <= Length; d++) {
					buf[i++]=MenueItem[a][d];
					buf[i++]=(b==a)?ATTR_HELP_BAR_CURR:ATTR_HELP;
				}
			}
			puttext(x+1,y+1,x+Length,y+MaxItem,buf);
		}
		c = b;
		if (FullScreen)
			gotoxy( CursorX+1,CursorY + 0+1);
		else
			gotoxy( CursorX+1,CursorY + 1+1);
		ch = newgetch();
		if(ch==CIO_KEY_MOUSE) {
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
			if (me.endx >= x && me.endx <= x + Length + 1 && me.endy > y && me.endy < y + MaxItem + 1) {
				b = me.endy - y;
				if (me.event == CIOLIB_BUTTON_1_CLICK)
					ch = 13;
			}
		}
		else
			memset(&me,0,sizeof(me));
		switch (ch) {
		case CIO_KEY_UP:
			b--;
			break;
		case CIO_KEY_DOWN:
			b++;
			break;
		case CIO_KEY_PPAGE:
			b=1;
			break;
		case CIO_KEY_NPAGE:
			b=MaxItem;
			break;
		case CIO_KEY_LEFT:
			return (253);
		case CIO_KEY_RIGHT:
			return (254);
		}
		if (b < 1)
			b = MaxItem;
		if (b > MaxItem)
			b = 1;
		if (me.endx > 80)
			me.endx = 80;
		if (me.endy > 25)
			me.endy = 25;
		if (me.event==CIOLIB_BUTTON_3_CLICK)
			ch = 27;
		if (me.endy == 1) {
			if (me.endx >= 3 && me.endx <= 7) {
				ActiveMenue = 1;
				return 252;
			}
			if (me.endx >= 14 && me.endx <= 18) {
				ActiveMenue = 2;
				return 252;
			}
			if (me.endx >= 25 && me.endx <= 31) {
				ActiveMenue = 3;
				return 252;
			}
			if (me.endx >= 38 && me.endx <= 43) {
				ActiveMenue = 4;
				return 252;
			}
			if (me.endx >= 50 && me.endx <= 54) {
				ActiveMenue = 5;
				return 252;
			}
			if (me.endx >= 63 && me.endx <= 69) {
				ActiveMenue = 6;
				return 252;
			}
			if (me.endx >= 74 && me.endx <= 77) {
				ActiveMenue = 7;
				return 252;
			}
		}
	} while ((ch != 13) & (ch != 27));
	if (ch == 13)
		return b;
	return 255;
}

int 
menue(void)
{
	int             x, a, b, i;
	char			buf[80*2*2];
	b = 0;
	ActiveMenue = 1;
	do {
		a = 0;
		if (ActiveMenue != b) {
			ShowScreen(0, 0);
			i=0;
			buf[i++]=223;
			buf[i++]=ATTR_NORM_LOW;
			i+=bufprintf(buf+i,ActiveMenue==1?ATTR_NORM_BAR_CURR:ATTR_NORM_BAR," FILES      ");
			i+=bufprintf(buf+i,ActiveMenue==2?ATTR_NORM_BAR_CURR:ATTR_NORM_BAR,"FONTS      ");
			i+=bufprintf(buf+i,ActiveMenue==3?ATTR_NORM_BAR_CURR:ATTR_NORM_BAR,"OPTIONS      ");
			i+=bufprintf(buf+i,ActiveMenue==4?ATTR_NORM_BAR_CURR:ATTR_NORM_BAR,"SCREEN      ");
			i+=bufprintf(buf+i,ActiveMenue==5?ATTR_NORM_BAR_CURR:ATTR_NORM_BAR,"MISC.        ");
			i+=bufprintf(buf+i,ActiveMenue==6?ATTR_NORM_BAR_CURR:ATTR_NORM_BAR,"TOGGLES    ");
			i+=bufprintf(buf+i,ActiveMenue==7?ATTR_NORM_BAR_CURR:ATTR_NORM_BAR,"HELP");
			buf[i++]=219;
			buf[i++]=ATTR_NORM_LOW;
			buf[i++]=220;
			buf[i++]=ATTR_NORM_LOW;
			buf[i++]=223;
			buf[i]=ATTR_NORM_LOW;
			puttext(1,1,80,1,buf);
		}
		b = ActiveMenue;
		switch (ActiveMenue) {
		case 1:
			MenueItem[1] = " LOAD       ALT+L  ";
			MenueItem[2] = " SAVE       ALT+S  ";
			MenueItem[3] = " QUIT       ALT+X  ";
			Length = 18;
			MaxItem = 3;
			a = Menues(1, 2);
			break;
		case 2:
			MenueItem[1] = " SELECT CHARSET        ";
			MenueItem[2] = " SELECT FONT    ALT+F  ";
			MenueItem[3] = " FONT MODE      ALT+N  ";
			MenueItem[4] = " OUTLINE TYPE   ALT+W  ";
			Length = 22;
			MaxItem = 4;
			a = Menues(12, 2);
			break;
		case 3:
			MenueItem[1] = " SAUCE SETUP   CTRL+S ";
			MenueItem[2] = " SET PAGE      ALT+P  ";
			MenueItem[3] = " TAB SETUP     ALT+T  ";
			MenueItem[4] = " GLOBAL        ALT+G  ";
			MenueItem[5] = " SET EFFECT    ALT+M  ";
			Length = 20;
			MaxItem = 5;
			a = Menues(23, 2);
			break;
		case 4:
			MenueItem[1] = " CLEAR PAGE    ALT+C  ";
			MenueItem[2] = " INSERT LINE   ALT+I  ";
			MenueItem[3] = " DELTE LINE    ALT+Y  ";
			MenueItem[4] = " INSERT COLUMN ALT+1  ";
			MenueItem[5] = " DELTE COLUMN  ALT+2  ";
			MenueItem[6] = " UNDO/RESTORE  ALT+R  ";
			Length = 21;
			MaxItem = 6;
			a = Menues(36, 2);
			break;
		case 5:
			MenueItem[1] = " SET COLORS    ALT+A  ";
			MenueItem[2] = " PICK UP COLOR ALT+U  ";
			MenueItem[3] = " ASCII TABLE   ALT+K  ";
			Length = 21;
			MaxItem = 3;
			a = Menues(48, 2);
			break;
		case 6:
			MenueItem[1] = " LINE DRAW        ALT+D  ";
			MenueItem[2] = " DRAW MODE        ALT+-  ";
			MenueItem[3] = " INSERT MODE      INS    ";
			MenueItem[4] = " ELITE MODE       ALT+E  ";
			Length = 24;
			MaxItem = 4;
			a = Menues(55, 2);
			break;
		case 7:
			MenueItem[1] = " HELP         ALT+H  ";
			MenueItem[2] = " ABOUT               ";
			Length = 20;
			MaxItem = 2;
			a = Menues(59, 2);
			break;
		};
		switch (a) {
		case 253:
			ActiveMenue--;
			break;
		case 254:
			ActiveMenue++;
			break;
		case 255:
			return 0;
		};
		if (ActiveMenue < 1)
			ActiveMenue = 7;
		if (ActiveMenue > 7)
			ActiveMenue = 1;
	} while (a > 200);
	return a + (ActiveMenue << 8);
}

int
menuemode(void)
{
	unsigned int    a;
	a = menue();
	switch ((a & 0xFF00) >> 8) {
	case 1:
		switch (a & 0xFF) {
		case 1:
			load();
			break;
		case 2:
			save();
			break;
		case 3:
			if(exitprg()==-1)
				return(-1);
			break;
		}
		break;
	case 2:
		switch (a & 0xFF) {
		case 1:
			ActiveCharset = SelectCharSet();
			break;
		case 2:
			a = SelectFont();
			if (a > 0) {
				SFont = a;
				Openfont(SFont);
			}
			break;
		case 3:
			FontMode = !FontMode;
			Undo = FALSE;
			SaveScreen();
			break;
		case 4:	/* Select Outline Font */
			SelectOutline();
			break;
		}
		break;
	case 3:
		switch (a & 0xFF) {
		case 1:
			SauceSet();
			break;
		case 2:
			SetPage();
			break;
		case 3:
			tabsetup();
			break;
		case 4:
			global          ();
			break;
		case 5:
			select_effekt();
			break;
		}
		break;
	case 4:
		switch (a & 0xFF) {
		case 1:
			ClearScreen();
			break;
		case 2:
			InsLine();
			break;
		case 3:
			DelLine();
			break;
		case 4:
			InsCol();
			break;
		case 5:
			DelCol();
			break;
		case 6:
			UndoLast();
			break;
		}
		break;
	case 5:
		switch (a & 0xFF) {
		case 1:
			Attribute = SelectColor();
			break;
		case 2:
			Attribute = Screen[ActivePage][CursorY + FirstLine][(CursorX * 2) + 1];
			break;
		case 3:
			putchacter(asciitable());
			break;
		}
		break;
	case 6:
		switch (a & 0xFF) {
		case 1:
			drawline();
			break;
		case 2:
			drawmode();
			break;
		case 3:
			InsertMode = !InsertMode;
			break;
		case 4:
			EliteMode = !EliteMode;
			break;
		}
	case 7:
		switch (a & 0xFF) {
		case 1:
			help();
			break;
		case 2:
			about();
			break;
		}
		break;
	}
	return(0);
}
