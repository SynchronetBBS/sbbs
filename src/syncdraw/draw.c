#include <string.h>

#include <ciolib.h>
#include <gen_defs.h>

#include "syncdraw.h"
#include "key.h"
#include "options.h"
#include "miscfunctions.h"

enum {
	 MOVE_NONE
	,MOVE_UP
	,MOVE_DOWN
	,MOVE_RIGHT
	,MOVE_LEFT
};

void 
addlinestr(char *bottom, char *top, char *result)
{
	int             x;

	if (bottom[0] == 0) {
		strncpy(result, top, 5);
		return;
	}
	if (top[0] == 0) {
		result[0] = 0;
		return;
	}
	result[4] = 0;
	for (x = 0; x < 4; x++) {
		if (top[x] != 'N')
			result[x] = top[x];
		else
			result[x] = bottom[x];
	}
	if (result[1] != result[3] && result[1] != 'N' && result[3] != 'N') {
		if (top[1] != 'N')
			result[3] = top[1];
		else
			result[1] = top[3];
	}
	if (result[0] != result[2] && result[0] != 'N' && result[2] != 'N') {
		if (top[0] != 'N')
			result[2] = top[0];
		else
			result[0] = top[2];
	}
}

unsigned char 
str2line(char *buffer, unsigned char top)
{
	switch (buffer[0]) {
		case 'S':
		switch (buffer[1]) {
			case 'S':
			switch (buffer[2]) {
				case 'S':
				switch (buffer[3]) {
					case 'S':
					return 197;
				case 'N':
					return 195;
				}
			case 'N':
				switch (buffer[3]) {
				case 'S':
					return 193;
				case 'N':
					return 192;
				}
			}
		case 'D':
			switch (buffer[2]) {
			case 'S':
				switch (buffer[3]) {
				case 'D':
					return 216;
				case 'N':
					return 198;
				}
			case 'N':
				switch (buffer[3]) {
				case 'D':
					return 207;
				case 'N':
					return 212;
				}
			}
		case 'N':
			switch (buffer[2]) {
			case 'S':
				switch (buffer[3]) {
				case 'D':
					return 181;
				case 'S':
					return 180;
				case 'N':
					return 179;
				}
			case 'N':
				switch (buffer[3]) {
				case 'D':
					return 190;
				case 'S':
					return 217;
				}
			}
		}
	case 'D':
		switch (buffer[1]) {
		case 'S':
			switch (buffer[2]) {
			case 'D':
				switch (buffer[3]) {
				case 'S':
					return 215;
				case 'N':
					return 199;
				}
			case 'N':
				switch (buffer[3]) {
				case 'S':
					return 208;
				case 'N':
					return 211;
				}
			}
		case 'D':
			switch (buffer[2]) {
			case 'D':
				switch (buffer[3]) {
				case 'D':
					return 206;
				case 'N':
					return 204;
				}
			case 'N':
				switch (buffer[3]) {
				case 'D':
					return 202;
				case 'N':
					return 200;
				}
			}
		case 'N':
			switch (buffer[2]) {
			case 'D':
				switch (buffer[3]) {
				case 'D':
					return 185;
				case 'S':
					return 182;
				case 'N':
					return 186;
				}
			case 'N':
				switch (buffer[3]) {
				case 'D':
					return 188;
				case 'S':
					return 189;
				}
			}
		}
	case 'N':
		switch (buffer[1]) {
		case 'S':
			switch (buffer[2]) {
			case 'S':
				switch (buffer[3]) {
				case 'S':
					return 194;
				case 'N':
					return 218;
				}
			case 'D':
				switch (buffer[3]) {
				case 'S':
					return 210;
				case 'N':
					return 214;
				}
			case 'N':
				return 196;
			}
		case 'D':
			switch (buffer[2]) {
			case 'S':
				switch (buffer[3]) {
				case 'D':
					return 209;
				case 'N':
					return 213;
				}
			case 'D':
				switch (buffer[3]) {
				case 'D':
					return 203;
				case 'N':
					return 201;
				}
			case 'N':
				return 205;
			}
		case 'N':
			switch (buffer[2]) {
			case 'S':
				switch (buffer[3]) {
				case 'D':
					return 184;
				case 'S':
					return 191;
				}
			case 'D':
				switch (buffer[3]) {
				case 'D':
					return 187;
				case 'S':
					return 183;
				}
			}
		}
	}
	return top;
}

void 
line2str(unsigned char ch, char *buffer)
{
	switch (ch) {
		case 179:
		strcpy(buffer, "SNSN");
		break;
	case 180:
		strcpy(buffer, "SNSS");
		break;
	case 191:
		strcpy(buffer, "NNSS");
		break;
	case 217:
		strcpy(buffer, "SNNS");
		break;
	case 192:
		strcpy(buffer, "SSNN");
		break;
	case 218:
		strcpy(buffer, "NSSN");
		break;
	case 193:
		strcpy(buffer, "SSNS");
		break;
	case 194:
		strcpy(buffer, "NSSS");
		break;
	case 195:
		strcpy(buffer, "SSSN");
		break;
	case 196:
		strcpy(buffer, "NSNS");
		break;
	case 197:
		strcpy(buffer, "SSSS");
		break;
	case 181:
		strcpy(buffer, "SNSD");
		break;
	case 184:
		strcpy(buffer, "NNSD");
		break;
	case 190:
		strcpy(buffer, "SNND");
		break;
	case 212:
		strcpy(buffer, "SDNN");
		break;
	case 213:
		strcpy(buffer, "NDSN");
		break;
	case 207:
		strcpy(buffer, "SDND");
		break;
	case 209:
		strcpy(buffer, "NDSD");
		break;
	case 198:
		strcpy(buffer, "SDSN");
		break;
	case 216:
		strcpy(buffer, "SDSD");
		break;
	case 182:
		strcpy(buffer, "DNDS");
		break;
	case 183:
		strcpy(buffer, "NNDS");
		break;
	case 189:
		strcpy(buffer, "DNNS");
		break;
	case 211:
		strcpy(buffer, "DSNN");
		break;
	case 214:
		strcpy(buffer, "NSDN");
		break;
	case 208:
		strcpy(buffer, "DSNS");
		break;
	case 210:
		strcpy(buffer, "NSDS");
		break;
	case 199:
		strcpy(buffer, "DSDN");
		break;
	case 215:
		strcpy(buffer, "DSDS");
		break;
	case 185:
		strcpy(buffer, "DNDD");
		break;
	case 186:
		strcpy(buffer, "DNDN");
		break;
	case 187:
		strcpy(buffer, "NNDD");
		break;
	case 188:
		strcpy(buffer, "DNND");
		break;
	case 200:
		strcpy(buffer, "DDNN");
		break;
	case 201:
		strcpy(buffer, "NDDN");
		break;
	case 202:
		strcpy(buffer, "DDND");
		break;
	case 203:
		strcpy(buffer, "NDDD");
		break;
	case 204:
		strcpy(buffer, "DDDN");
		break;
	case 205:
		strcpy(buffer, "NDND");
		break;
	case 206:
		strcpy(buffer, "DDDD");
		break;
	default:
		buffer[0] = 0;
	}
}

void 
addtopage(int y, int x, char ch)
{
	char            backbuff[5];
	char            topbuff[5];
	char            result[5];
	line2str(Screen[ActivePage][y][x], backbuff);
	line2str(ch, topbuff);
	addlinestr(backbuff, topbuff, result);
	Screen[ActivePage][y][x] = str2line(result, ch);
}

void 
drawline(void)
{
	int             mv=MOVE_NONE, lmv=MOVE_NONE, ch, maxy = 22;
	struct			text_info	ti;
	struct			mouse_event	me;

	gettextinfo(&ti);
	if (FullScreen)
		maxy++;
	maxy += ti.screenheight - 25;
	clrline();
	CoolWrite(1,ti.screenheight,"Draw line, use cursorkeys, press <ESC> 2 quit");
	do {
		if (FullScreen)
			gotoxy( CursorX+1,CursorY+1);
		else
			gotoxy( CursorX+1,CursorY + 1+1);
		do {
			ch = getch();
			if(ch==0 || ch==0xe0)
				ch|=getch()<<8;
		}
		while (ch != CIO_KEY_DOWN && ch != CIO_KEY_UP && ch != CIO_KEY_LEFT && ch != CIO_KEY_RIGHT && ch != 27 && ch != CIO_KEY_MOUSE);
		CursorHandling(ch);
		if(ch==CIO_KEY_MOUSE) {
			getmouse(&me);
			if(me.event==CIOLIB_BUTTON_3_CLICK)
				ch=27;
		}
		switch (ch) {
		case CIO_KEY_DOWN:
			mv=MOVE_DOWN;
			break;
		case CIO_KEY_UP:
			mv=MOVE_UP;
			break;
		case CIO_KEY_LEFT:
			mv=MOVE_LEFT;
			break;
		case CIO_KEY_RIGHT:
			mv=MOVE_RIGHT;
			break;
		}
		if (CursorY > maxy) {
			CursorY = maxy;
			FirstLine++;
		}
		if (CursorY > LastLine) {
			LastLine = CursorY;
		}
		CursorCheck();
		switch (mv) {
		case MOVE_UP:
			switch(lmv) {
				case MOVE_NONE:
					Screen[ActivePage][CursorY + FirstLine + 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine + 1, CursorX * 2, CharSet[ActiveCharset][5]);
					break;
				case MOVE_UP:
					Screen[ActivePage][CursorY + FirstLine + 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine + 1, CursorX * 2, CharSet[ActiveCharset][5]);
					break;
				case MOVE_DOWN:
#if 0
					Screen[ActivePage][CursorY + FirstLine + 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine + 1, CursorX * 2, CharSet[ActiveCharset][5]);
#endif
					break;
				case MOVE_LEFT:
					Screen[ActivePage][CursorY + FirstLine + 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine + 1, CursorX * 2, CharSet[ActiveCharset][2]);
					break;
				case MOVE_RIGHT:
					Screen[ActivePage][CursorY + FirstLine + 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine + 1, CursorX * 2, CharSet[ActiveCharset][3]);
					break;
			}
			break;
		case MOVE_DOWN:
			switch(lmv) {
				case MOVE_NONE:
					Screen[ActivePage][CursorY + FirstLine - 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine - 1, CursorX * 2, CharSet[ActiveCharset][5]);
					break;
				case MOVE_UP:
#if 0
					Screen[ActivePage][CursorY + FirstLine - 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine - 1, CursorX * 2, CharSet[ActiveCharset][5]);
#endif
					break;
				case MOVE_DOWN:
					Screen[ActivePage][CursorY + FirstLine - 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine - 1, CursorX * 2, CharSet[ActiveCharset][5]);
					break;
				case MOVE_LEFT:
					Screen[ActivePage][CursorY + FirstLine - 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine - 1, CursorX * 2, CharSet[ActiveCharset][0]);
					break;
				case MOVE_RIGHT:
					Screen[ActivePage][CursorY + FirstLine - 1][CursorX * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine - 1, CursorX * 2, CharSet[ActiveCharset][1]);
					break;
			}
			break;
		case MOVE_LEFT:
			switch(lmv) {
				case MOVE_NONE:
					Screen[ActivePage][CursorY + FirstLine][(CursorX + 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX + 1) * 2, CharSet[ActiveCharset][4]);
					break;
				case MOVE_UP:
					Screen[ActivePage][CursorY + FirstLine][(CursorX + 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX + 1) * 2, CharSet[ActiveCharset][1]);
					break;
				case MOVE_DOWN:
					Screen[ActivePage][CursorY + FirstLine][(CursorX + 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX + 1) * 2, CharSet[ActiveCharset][3]);
					break;
				case MOVE_LEFT:
					Screen[ActivePage][CursorY + FirstLine][(CursorX + 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX + 1) * 2, CharSet[ActiveCharset][4]);
					break;
				case MOVE_RIGHT:
#if 0
					Screen[ActivePage][CursorY + FirstLine][(CursorX + 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX + 1) * 2, CharSet[ActiveCharset][4]);
#endif
					break;
			}
			break;
		case MOVE_RIGHT:
			switch(lmv) {
				case MOVE_NONE:
					Screen[ActivePage][CursorY + FirstLine][(CursorX - 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX - 1) * 2, CharSet[ActiveCharset][4]);
					break;
				case MOVE_UP:
					Screen[ActivePage][CursorY + FirstLine][(CursorX - 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX - 1) * 2, CharSet[ActiveCharset][0]);
					break;
				case MOVE_DOWN:
					Screen[ActivePage][CursorY + FirstLine][(CursorX - 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX - 1) * 2, CharSet[ActiveCharset][2]);
					break;
				case MOVE_LEFT:
#if 0
					Screen[ActivePage][CursorY + FirstLine][(CursorX - 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX - 1) * 2, CharSet[ActiveCharset][4]);
#endif
					break;
				case MOVE_RIGHT:
					Screen[ActivePage][CursorY + FirstLine][(CursorX - 1) * 2 + 1] = Attribute;
					addtopage(CursorY + FirstLine, (CursorX - 1) * 2, CharSet[ActiveCharset][4]);
					break;
			}
			break;
		}
		if(mv != MOVE_NONE) {
			lmv=mv;
			mv=MOVE_NONE;
		}
		Statusline();
		ShowScreen(0, 1);
		Colors(Attribute);
	} while (ch != 27);
}

void 
drawmode(void)
{
	int             ch, maxy = 22;
	struct			text_info	ti;
	struct			mouse_event	me;

	gettextinfo(&ti);
	if (FullScreen)
		maxy++;
	maxy += ti.screenheight - 25;
	selectdrawmode();
	if (DrawMode == 255 << 8)
		goto end;
	clrline();
	CoolWrite(1, ti.screenheight, "Drawmode, use cursorkeys, press <ESC> 2 quit");
	do {
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
		}
		if (CursorY > maxy) {
			CursorY = maxy;
			FirstLine++;
		}
		CursorCheck();
		switch (DrawMode & 0xFF00) {
		case 0x0100:
			Screen[ActivePage][CursorY + FirstLine][CursorX * 2] = DrawMode & 0xFF;
			break;
		case 0x0200:
			Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 1] = DrawMode & 0xFF;
			break;
		case 0x0300:
			Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 1] = (DrawMode & 0xFF) + (Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 1] & 240);
			break;
		case 0x0400:
			Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 1] = (DrawMode & 0xFF) + (Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 1] & 15);
			break;
		}
		Statusline();
		ShowScreen(0, 1);
		Colors(Attribute);
	} while (ch != 27);
end:
	return;
}
