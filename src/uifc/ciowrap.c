#include <sys/time.h>
#include <unistd.h>

#include "ciowrap.h"
#include "uifc.h"		/* UIFC_IBM */

static int lastattr=0;
static long mode;

short curses_color(short color)
{
	switch(color)
	{
		case 0 :
			return(COLOR_BLACK);
		case 1 :
			return(COLOR_BLUE);
		case 2 :
			return(COLOR_GREEN);
		case 3 :
			return(COLOR_CYAN);
		case 4 :
			return(COLOR_RED);
		case 5 :
			return(COLOR_MAGENTA);
		case 6 :
			return(COLOR_YELLOW);
		case 7 :
			return(COLOR_WHITE);
		case 8 :
			return(COLOR_BLACK);
		case 9 :
			return(COLOR_BLUE);
		case 10 :
			return(COLOR_GREEN);
		case 11 :
			return(COLOR_CYAN);
		case 12 :
			return(COLOR_RED);
		case 13 :
			return(COLOR_MAGENTA);
		case 14 :
			return(COLOR_YELLOW);
		case 15 :
			return(COLOR_WHITE);
	}
	return(0);
}

int puttext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	int x,y;
	int fillpos=0;
	unsigned char attr;
	unsigned char fill_char;
	unsigned char orig_attr;
	int oldx, oldy;

	getyx(stdscr,oldy,oldx);	
	orig_attr=lastattr;
	for(y=sy-1;y<=ey-1;y++)
	{
		for(x=sx-1;x<=ex-1;x++)
		{
			fill_char=fill[fillpos++];
			attr=fill[fillpos++];
			textattr(attr);
			move(y, x);
			_putch(fill_char,FALSE);
		}
	}
	textattr(orig_attr);
	move(oldy, oldx);
	refresh();
	return(1);
}

int gettext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	int x,y;
	int fillpos=0;
	chtype attr;
	unsigned char attrib;
	unsigned char colour;
	int oldx, oldy;
	unsigned char thischar;
	int	ext_char;

	getyx(stdscr,oldy,oldx);	
	for(y=sy-1;y<=ey-1;y++)
	{
		for(x=sx-1;x<=ex-1;x++)
		{
			attr=mvinch(y, x);
			if(attr&A_ALTCHARSET && !(mode&UIFC_IBM)){
				ext_char=A_ALTCHARSET|(attr&255);
				/* likely ones */
				if (ext_char == ACS_CKBOARD)
				{
					thischar=176;
				}
				else if (ext_char == ACS_BOARD)
				{
					thischar=177;
				}
				else if (ext_char == ACS_BSSB)
				{
					thischar=218;
				}
				else if (ext_char == ACS_SSBB)
				{
					thischar=192;
				}
				else if (ext_char == ACS_BBSS)
				{
					thischar=191;
				}
				else if (ext_char == ACS_SBBS)
				{
					thischar=217;
				}
				else if (ext_char == ACS_SBSS)
				{
					thischar=180;
				}
				else if (ext_char == ACS_SSSB)
				{
					thischar=195;
				}
				else if (ext_char == ACS_SSBS)
				{
					thischar=193;
				}
				else if (ext_char == ACS_BSSS)
				{
					thischar=194;
				}
				else if (ext_char == ACS_BSBS)
				{
					thischar=196;
				}
				else if (ext_char == ACS_SBSB)
				{
					thischar=179;
				}
				else if (ext_char == ACS_SSSS)
				{
					thischar=197;
				}
				else if (ext_char == ACS_BLOCK)
				{
					thischar=219;
				}
				else if (ext_char == ACS_UARROW)
				{
					thischar=30;
				}
				else if (ext_char == ACS_DARROW)
				{
					thischar=31;
				}
				
				/* unlikely (Not in ncurses) */
				else if (ext_char == ACS_SBSD)
				{
					thischar=181;
				}
				else if (ext_char == ACS_DBDS)
				{
					thischar=182;
				}
				else if (ext_char == ACS_BBDS)
				{
					thischar=183;
				}
				else if (ext_char == ACS_BBSD)
				{
					thischar=184;
				}
				else if (ext_char == ACS_DBDD)
				{
					thischar=185;
				}
				else if (ext_char == ACS_DBDB)
				{
					thischar=186;
				}
				else if (ext_char == ACS_BBDD)
				{
					thischar=187;
				}
				else if (ext_char == ACS_DBBD)
				{
					thischar=188;
				}
				else if (ext_char == ACS_DBBS)
				{
					thischar=189;
				}
				else if (ext_char == ACS_SBBD)
				{
					thischar=190;
				}
				else if (ext_char == ACS_SDSB)
				{
					thischar=198;
				}
				else if (ext_char == ACS_DSDB)
				{
					thischar=199;
				}
				else if (ext_char == ACS_DDBB)
				{
					thischar=200;
				}
				else if (ext_char == ACS_BDDB)
				{
					thischar=201;
				}
				else if (ext_char == ACS_DDBD)
				{
					thischar=202;
				}
				else if (ext_char == ACS_BDDD)
				{
					thischar=203;
				}
				else if (ext_char == ACS_DDDB)
				{
					thischar=204;
				}
				else if (ext_char == ACS_BDBD)
				{
					thischar=205;
				}
				else if (ext_char == ACS_DDDD)
				{
					thischar=206;
				}
				else if (ext_char == ACS_SDBD)
				{
					thischar=207;
				}
				else if (ext_char == ACS_DSBS)
				{
					thischar=208;
				}
				else if (ext_char == ACS_BDSD)
				{
					thischar=209;
				}
				else if (ext_char == ACS_BSDS)
				{
					thischar=210;
				}
				else if (ext_char == ACS_DSBB)
				{
					thischar=211;
				}
				else if (ext_char == ACS_SDBB)
				{
					thischar=212;
				}
				else if (ext_char == ACS_BDSB)
				{
					thischar=213;
				}
				else if (ext_char == ACS_BSDB)
				{
					thischar=214;
				}
				else if (ext_char == ACS_DSDS)
				{
					thischar=215;
				}
				else if (ext_char == ACS_SDSD)
				{
					thischar=216;
				}
				else
				{
					thischar=attr&255;
				}
			}
			else
				thischar=attr;
			fill[fillpos++]=(unsigned char)(thischar);
			attrib=0;
			if (attr & A_BOLD)  
			{
				attrib |= 8;
			}
			if (attr & A_BLINK)
			{
				attrib |= 128;
			}
			colour=PAIR_NUMBER(attr&A_COLOR)-1;
			colour=((colour&56)<<1)|(colour&7);
			fill[fillpos++]=colour|attrib;
		}
	}
	move(oldy, oldx);
	return(1);
}

void textattr(unsigned char attr)
{
	int   attrs=A_NORMAL;
	short	colour;

	if (lastattr==attr)
		return;

	lastattr=attr;
	
	if (attr & 8)  {
		attrs |= A_BOLD;
	}
	if (attr & 128)
	{
		attrs |= A_BLINK;
	}
	attrset(attrs);
	colour = COLOR_PAIR( ((attr&7)|((attr>>1)&56))+1 );
	#ifdef NCURSES_VERSION_MAJOR
	color_set(colour,NULL);
	#endif
	bkgdset(colour);
}

int kbhit(void)
{
	struct timeval timeout;
	fd_set	rfds;

	timeout.tv_sec=0;
	timeout.tv_usec=0;
	FD_ZERO(&rfds);
	FD_SET(fileno(stdin),&rfds);

	return(select(fileno(stdin)+1,&rfds,NULL,NULL,&timeout));
}

#ifndef __QNX__
void delay(long msec)
{
	usleep(msec*1000);
}
#endif

int wherey(void)
{
	int x,y;
	getyx(stdscr,y,x);
	return(y+1);
}

int wherex(void)
{
	int x,y;
	getyx(stdscr,y,x);
	return(x+1);
}

void _putch(unsigned char ch, BOOL refresh_now)
{
	int	cha;

	if(!(mode&UIFC_IBM))
	{
		switch(ch)
		{
			case 30:
				cha=ACS_UARROW;
				break;
			case 31:
				cha=ACS_DARROW;
				break;
			case 176:
				cha=ACS_CKBOARD;
				break;
			case 177:
				cha=ACS_BOARD;
				break;
			case 178:
				cha=ACS_BOARD;
				break;
			case 179:
				cha=ACS_SBSB;
				break;
			case 180:
				cha=ACS_SBSS;
				break;
			case 181:
				cha=ACS_SBSD;
				break;
			case 182:
				cha=ACS_DBDS;
				break;
			case 183:
				cha=ACS_BBDS;
				break;
			case 184:
				cha=ACS_BBSD;
				break;
			case 185:
				cha=ACS_DBDD;
				break;
			case 186:
				cha=ACS_DBDB;
				break;
			case 187:
				cha=ACS_BBDD;
				break;
			case 188:
				cha=ACS_DBBD;
				break;
			case 189:
				cha=ACS_DBBS;
				break;
			case 190:
				cha=ACS_SBBD;
				break;
			case 191:
				cha=ACS_BBSS;
				break;
			case 192:
				cha=ACS_SSBB;
				break;
			case 193:
				cha=ACS_SSBS;
				break;
			case 194:
				cha=ACS_BSSS;
				break;
			case 195:
				cha=ACS_SSSB;
				break;
			case 196:
				cha=ACS_BSBS;
				break;
			case 197:
				cha=ACS_SSSS;
				break;
			case 198:
				cha=ACS_SDSB;
				break;
			case 199:
				cha=ACS_DSDB;
				break;
			case 200:
				cha=ACS_DDBB;
				break;
			case 201:
				cha=ACS_BDDB;
				break;
			case 202:
				cha=ACS_DDBD;
				break;
			case 203:
				cha=ACS_BDDD;
				break;
			case 204:
				cha=ACS_DDDB;
				break;
			case 205:
				cha=ACS_BDBD;
				break;
			case 206:
				cha=ACS_DDDD;
				break;
			case 207:
				cha=ACS_SDBD;
				break;
			case 208:
				cha=ACS_DSBS;
				break;
			case 209:
				cha=ACS_BDSD;
				break;
			case 210:
				cha=ACS_BSDS;
				break;
			case 211:
				cha=ACS_DSBB;
				break;
			case 212:
				cha=ACS_SDBB;
				break;
			case 213:
				cha=ACS_BDSB;
				break;
			case 214:
				cha=ACS_BSDB;
				break;
			case 215:
				cha=ACS_DSDS;
				break;
			case 216:
				cha=ACS_SDSD;
				break;
			case 217:
				cha=ACS_SBBS;
				break;
			case 218:
				cha=ACS_BSSB;
				break;
			case 219:
				cha=ACS_BLOCK;
				break;
			default:
				cha=ch;
		}
	}
	else
	{
		switch(ch) {
			case 30:
				cha=ACS_UARROW;
				break;
			case 31:
				cha=ACS_DARROW;
				break;
			default:
				cha=ch;
		}
	}
			

	addch(cha);
	if(refresh_now)
		refresh();
}

int cprintf(char *fmat, ...)
{
    va_list argptr;
	char	str[MAX_BFLN];
	int		pos;

    va_start(argptr,fmat);
    vsprintf(str,fmat,argptr);
    va_end(argptr);
	for(pos=0;str[pos];pos++)
	{
		_putch(str[pos],FALSE);
	}
	refresh();
    return(1);
}

void cputs(char *str)
{
	int		pos;

	for(pos=0;str[pos];pos++)
	{
		_putch(str[pos],FALSE);
	}
	refresh();
}

void gotoxy(int x, int y)
{
	move(y-1,x-1);
	refresh();
}

void clrscr(void)
{
    clear();
	refresh();
}

void initciowrap(long inmode)
{
	short	fg, bg, pair=0;

	initscr();
	start_color();
	cbreak();
	noecho();
	nonl();
	keypad(stdscr, TRUE);
	scrollok(stdscr,FALSE);
	raw();

	/* Set up color pairs */
	for(bg=0;bg<8;bg++)  {
		for(fg=0;fg<8;fg++) {
			init_pair(++pair,curses_color(fg),curses_color(bg));
		}
	}
	mode = inmode;
}

void gettextinfo(struct text_info *info)
{
	getmaxyx(stdscr, info->screenheight, info->screenwidth);
	if(has_colors())
		info->currmode=COLOR_MODE;
	else
		info->currmode=MONO;
}

void _setcursortype(int type) {
	switch(type) {
		case _NOCURSOR:
			curs_set(0);
			break;
		
		case _SOLIDCURSOR:
			curs_set(2);
			break;
		
		default:	/* Normal cursor */
			curs_set(1);
			break;

	}
	refresh();
}
