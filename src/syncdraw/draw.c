#include <string.h>

#include <ciolib.h>
#include <gen_defs.h>
#include <keys.h>

#include "mdraw.h"
#include "key.h"
#include "options.h"
#include "miscfunctions.h"

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
		strncpy(buffer, "SNSN", 5);
		break;
	case 180:
		strncpy(buffer, "SNSS", 5);
		break;
	case 191:
		strncpy(buffer, "NNSS", 5);
		break;
	case 217:
		strncpy(buffer, "SNNS", 5);
		break;
	case 192:
		strncpy(buffer, "SSNN", 5);
		break;
	case 218:
		strncpy(buffer, "NSSN", 5);
		break;
	case 193:
		strncpy(buffer, "SSNS", 5);
		break;
	case 194:
		strncpy(buffer, "NSSS", 5);
		break;
	case 195:
		strncpy(buffer, "SSSN", 5);
		break;
	case 196:
		strncpy(buffer, "NSNS", 5);
		break;
	case 197:
		strncpy(buffer, "SSSS", 5);
		break;
	case 181:
		strncpy(buffer, "SNSD", 5);
		break;
	case 184:
		strncpy(buffer, "NNSD", 5);
		break;
	case 190:
		strncpy(buffer, "SNND", 5);
		break;
	case 212:
		strncpy(buffer, "SDNN", 5);
		break;
	case 213:
		strncpy(buffer, "NDSN", 5);
		break;
	case 207:
		strncpy(buffer, "SDND", 5);
		break;
	case 209:
		strncpy(buffer, "NDSD", 5);
		break;
	case 198:
		strncpy(buffer, "SDSN", 5);
		break;
	case 216:
		strncpy(buffer, "SDSD", 5);
		break;
	case 182:
		strncpy(buffer, "DNDS", 5);
		break;
	case 183:
		strncpy(buffer, "NNDS", 5);
		break;
	case 189:
		strncpy(buffer, "DNNS", 5);
		break;
	case 211:
		strncpy(buffer, "DSNN", 5);
		break;
	case 214:
		strncpy(buffer, "NSDN", 5);
		break;
	case 208:
		strncpy(buffer, "DSNS", 5);
		break;
	case 210:
		strncpy(buffer, "NSDS", 5);
		break;
	case 199:
		strncpy(buffer, "DSDN", 5);
		break;
	case 215:
		strncpy(buffer, "DSDS", 5);
		break;
	case 185:
		strncpy(buffer, "DNDD", 5);
		break;
	case 186:
		strncpy(buffer, "DNDN", 5);
		break;
	case 187:
		strncpy(buffer, "NNDD", 5);
		break;
	case 188:
		strncpy(buffer, "DNND", 5);
		break;
	case 200:
		strncpy(buffer, "DDNN", 5);
		break;
	case 201:
		strncpy(buffer, "NDDN", 5);
		break;
	case 202:
		strncpy(buffer, "DDND", 5);
		break;
	case 203:
		strncpy(buffer, "NDDD", 5);
		break;
	case 204:
		strncpy(buffer, "DDDN", 5);
		break;
	case 205:
		strncpy(buffer, "NDND", 5);
		break;
	case 206:
		strncpy(buffer, "DDDD", 5);
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
	int             a=0, b=0, c=0, d=0, ch, maxy = 22;
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
			if(ch==0 || ch==0xff)
				ch|=getch()<<8;
		}
		while (ch != CIO_KEY_DOWN && ch != CIO_KEY_UP && ch != CIO_KEY_LEFT && ch != CIO_KEY_RIGHT && ch != 27 && ch != CIO_KEY_MOUSE);
		CursorHandling(ch);
		if(ch==CIO_KEY_MOUSE)
			getmouse(&me);
		switch (ch) {
		case CIO_KEY_DOWN:
			a = 1;
			break;
		case CIO_KEY_UP:
			a = -1;
			break;
		case CIO_KEY_LEFT:
			b = -1;
			break;
		case CIO_KEY_RIGHT:
			b = 1;
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
		switch (a) {
		case 1:
			Screen[ActivePage][CursorY + FirstLine - 1][CursorX * 2 + 1] = Attribute;
			if (d == -1)
				addtopage(CursorY + FirstLine - 1, CursorX * 2, CharSet[ActiveCharset][0]);
			/*Screen[ActivePage][CursorY + FirstLine - 1][CursorX * 2] = CharSet[ActiveCharset][0]; */
			else
			if (d == 1)
				addtopage(CursorY + FirstLine - 1, CursorX * 2, CharSet[ActiveCharset][1]);
			/*Screen[ActivePage][CursorY + FirstLine - 1][CursorX * 2] = CharSet[ActiveCharset][1]; */
			else
			addtopage(CursorY + FirstLine - 1, CursorX * 2, CharSet[ActiveCharset][5]);
			/*Screen[ActivePage][CursorY + FirstLine - 1][CursorX * 2] = CharSet[ActiveCharset][5]; */
			break;
		case -1:
			Screen[ActivePage][CursorY + FirstLine + 1][CursorX * 2 + 1] = Attribute;
			if (d == -1)
				addtopage(CursorY + FirstLine + 1, CursorX * 2, CharSet[ActiveCharset][2]);
			/*Screen[ActivePage][CursorY + FirstLine + 1][CursorX * 2] = CharSet[ActiveCharset][2]; */
			else
			if (d == 1)
				addtopage(CursorY + FirstLine + 1, CursorX * 2, CharSet[ActiveCharset][3]);
			/*Screen[ActivePage][CursorY + FirstLine + 1][CursorX * 2] = CharSet[ActiveCharset][3]; */
			else
			addtopage(CursorY + FirstLine + 1, CursorX * 2, CharSet[ActiveCharset][5]);
			/*Screen[ActivePage][CursorY + FirstLine + 1][CursorX * 2] = CharSet[ActiveCharset][5]; */
			break;
		}
		switch (b) {
		case 1:
			Screen[ActivePage][CursorY + FirstLine][CursorX * 2 - 1] = Attribute;
			if (c == 1)
				addtopage(CursorY + FirstLine, CursorX * 2 - 2, CharSet[ActiveCharset][2]);
			/*Screen[ActivePage][CursorY + FirstLine][CursorX * 2 - 2] = CharSet[ActiveCharset][2]; */
			else
			if (c == -1)
				addtopage(CursorY + FirstLine, CursorX * 2 - 2, CharSet[ActiveCharset][0]);
			/*Screen[ActivePage][CursorY + FirstLine][CursorX * 2 - 2] = CharSet[ActiveCharset][0]; */
			else
			addtopage(CursorY + FirstLine, CursorX * 2 - 2, CharSet[ActiveCharset][4]);
			/*Screen[ActivePage][CursorY + FirstLine][CursorX * 2 - 2] = CharSet[ActiveCharset][4]; */
			break;

		case -1:
			Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 3] = Attribute;
			if (c == 1)
				addtopage(CursorY + FirstLine, CursorX * 2 + 2, CharSet[ActiveCharset][3]);
			/*Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 2] = CharSet[ActiveCharset][3]; */
			else
			if (c == -1)
				addtopage(CursorY + FirstLine, CursorX * 2 + 2, CharSet[ActiveCharset][1]);
			/*Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 2] = CharSet[ActiveCharset][1]; */
			else
			addtopage(CursorY + FirstLine, CursorX * 2 + 2, CharSet[ActiveCharset][4]);
			/*Screen[ActivePage][CursorY + FirstLine][CursorX * 2 + 2] = CharSet[ActiveCharset][4]; */
			break;
		}
		c = a;
		d = b;
		a = 0;
		b = 0;
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
		if(ch==CIO_KEY_MOUSE)
			getmouse(&me);
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
