#include <stdarg.h>
#include <stdio.h>
#include <OpenDoor.h>

#include "conio.h"
#include "od_cio.h"

unsigned int OD_nextch;
int OD_attr;
const int OD_tabs[10]={9,17,25,33,41,49,57,65,73,80};

int OD_puttext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	struct text_info ti;

	gettextinfo(&ti);
	if(	sx < 1
		|| sx > ti.screenwidth
		|| sy < 1
		|| sy > ti.screenheight
		|| ex < sx
		|| ey < sy
		|| ex < 1
		|| ex > ti.screenwidth
		|| ey < 1
		|| ey > ti.screenheight)
		return(0);

	return(od_puttext(sx,sy,ex,ey,fill));
}

int OD_gettext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	struct text_info ti;

	gettextinfo(&ti);
	if(	sx < 1
		|| sx > ti.screenwidth
		|| sy < 1
		|| sy > ti.screenheight
		|| ex < sx
		|| ey < sy
		|| ex < 1
		|| ex > ti.screenwidth
		|| ey < 1
		|| ey > ti.screenheight)
		return(0);

	return(od_gettext(sx,sy,ex,ey,fill));
}

void OD_textattr(unsigned char attr)
{
	OD_attr=attr;
	od_set_attrib(attr);
}

int parsekey(int ch)
{
	switch(ch) {
		case OD_KEY_F1:
			OD_nextch=0x3b<<8;
			break;
			
		case OD_KEY_F2:
			OD_nextch=0x3c<<8;
			break;
			
		case OD_KEY_F3:
			OD_nextch=0x3d<<8;
			break;
			
		case OD_KEY_F4:
			OD_nextch=0x3e<<8;
			break;
			
		case OD_KEY_F5:
			OD_nextch=0x3f<<8;
			break;
			
		case OD_KEY_F6:
			OD_nextch=0x40<<8;
			break;
			
		case OD_KEY_F7:
			OD_nextch=0x41<<8;
			break;
			
		case OD_KEY_F8:
			OD_nextch=0x42<<8;
			break;
			
		case OD_KEY_F9:
			OD_nextch=0x43<<8;
			break;
			
		case OD_KEY_F10:
			OD_nextch=0x44<<8;
			break;

		case OD_KEY_UP:
			OD_nextch=0x48<<8;
			break;
			
		case OD_KEY_DOWN:
			OD_nextch=0x50<<8;
			break;

		case OD_KEY_LEFT:
			OD_nextch=0x4b<<8;
			break;

		case OD_KEY_RIGHT:
			OD_nextch=0x4d<<8;
			break;

		case OD_KEY_INSERT:
			OD_nextch=0x52<<8;
			break;

		case OD_KEY_DELETE:
			OD_nextch=0x53<<8;
			break;

		case OD_KEY_HOME:
			OD_nextch=0x47<<8;
			break;

		case OD_KEY_END:
			OD_nextch=0x4f<<8;
			break;

		case OD_KEY_PGUP:
			OD_nextch=0x49<<8;
			break;

		case OD_KEY_PGDN:
			OD_nextch=0x51<<8;
			break;

		case OD_KEY_SHIFTTAB:
			OD_nextch=0;
			break;
	}
}

int OD_kbhit(void)
{
	int ch;
	tODInputEvent ie;

	if(OD_nextch)
		return(1);

	if(!od_get_input(&ie,5,GETIN_NORMAL))
		return(0);

	if(ie.EventType==EVENT_CHARACTER) {
		OD_nextch=ie.chKeyPress;
		return(1);
	}

	parsekey(ie.chKeyPress);

	if(OD_nextch)
		return(1);

	return(0);
}

void OD_delay(long msec)
{
	usleep(msec*1000);
}

int OD_wherey(void)
{
	int row,col;
	struct text_info ti;

	od_get_cursor(&row,&col);
	return(row);
}

int OD_wherex(void)
{
	int row,col;
	struct text_info ti;	

	od_get_cursor(&row,&col);
	return(col);
}

/* Put the character _c on the screen at the current cursor position. 
 * The special characters return, linefeed, bell, and backspace are handled
 * properly, as is line wrap and scrolling. The cursor position is updated. 
 */
int OD_putch(unsigned char ch)
{
	struct text_info ti;
	int		ret;
	int		i;

	ret=ch;
	switch(ch) {
		case '\r':
			gotoxy(1,wherey());
			break;
		case '\n':
			gettextinfo(&ti);
			if(wherey()==ti.winbottom-ti.wintop+1) {
				wscroll();
			}
			else {
				gotoxy(wherex(),wherey()+1);
			}
			break;
		case 0x07:
			cio_api.beep();
			break;
		case 0x08:
			gotoxy(wherex()-1,wherey());
			putch(' ');
			gotoxy(wherex()-1,wherey());
			break;
		case '\t':
			for(i=0;i<10;i++) {
				if(OD_tabs[i]>wherex()) {
					while(wherex()<OD_tabs[i]) {
						putch(' ');
					}
					break;
				}
			}
			if(i==10) {
				putch('\r');
				putch('\n');
			}
			break;
		default:
			gettextinfo(&ti);
			if(OD_wherey()==ti.screenheight
					&& OD_wherex()==ti.screenwidth) {
				if(_wscroll) {
					od_putch(ch);
					gotoxy(ti.winleft,ti.cury);
				}
			}
			else {
				if(wherey()==ti.winbottom-ti.wintop+1
						&& wherex()==ti.winright-ti.winleft+1) {
					od_putch(ch);
					wscroll();
					gotoxy(ti.winleft,ti.cury);
				}
				else {
					if(wherex()==ti.winright-ti.winleft+1) {
						od_putch(ch);
						gotoxy(ti.winleft,ti.cury+1);
					}
					else {
						od_putch(ch);
						gotoxy(ti.curx+1,ti.cury);
					}
				}
			}
			break;
	}

	return(ch);
}

void OD_gotoxy(int x, int y)
{
	struct text_info ti;
	char ansi[16];

	gettextinfo(&ti);

	if(x>ti.screenwidth
			|| x < 1
			|| y>ti.screenheight
			|| y<1)
		return;

	sprintf(ansi,"%c[%d;%dH",27,y,x);
	od_set_cursor(y,x);
}

void OD_gettextinfo(struct text_info *info)
{
	info->currmode=3;
	info->screenheight=od_control.user_screen_length?od_control.user_screen_length:23;
	info->screenwidth=od_control.user_screenwidth?od_control.user_screenwidth:80;
	info->curx=wherex();
	info->cury=wherey();
	info->attribute=OD_attr;
}

void OD_setcursortype(int type)
{
}

int OD_getch(void)
{
	int ch;
	tODInputEvent ie;

	while(1) {
		if(OD_nextch) {
			ch=OD_nextch&0xff;
			OD_nextch>>=8;
			return(ch);
		}

		od_get_input(&ie,OD_NO_TIMEOUT,GETIN_NORMAL);

		if(ie.EventType==EVENT_CHARACTER) {
			return(ie.chKeyPress);
		}

		parsekey(ie.chKeyPress);
	}
}

int OD_getche(void)
{
	int ch;

	if(OD_nextch)
		return(OD_getch());
	ch=OD_getch();
	if(ch)
		putch(ch);
	return(ch);
}

int OD_beep(void)
{
	od_putch(7);
	return(0);
}

void OD_textmode(int mode)
{
}

ODAPIDEF void ODCALL OD_pers(BYTE btOperation)
{
	switch(btOperation) {
		case PEROP_UPDATE1:
		case PEROP_DISPLAY1:
			ODScrnSetAttribute(0x70);
			ODScrnSetCursorPos(1, 25);
			ODScrnDisplayString("Design me a better ONE-LINE Status Bar!  (Include Custom Keys Descriptions etc) ");
	}
}

void OD_initciowrap(long mode)
{
	od_control.od_mps=INCLUDE_MPS;
	od_add_personality("CONIO",1,24,OD_pers);
	od_set_personality("CONIO");
	od_init();
}
