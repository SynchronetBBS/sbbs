#include <stdarg.h>

#include "ciowrap.h"
#include "x_cio.h"
#include "console.h"
WORD	x_curr_attr=0x0700;

int x_puttext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;

	gettextinfo(&ti);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ti.screenwidth
			|| sy > ti.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > ti.screenwidth
			|| ey > ti.screenheight
			|| fill==NULL)
		return(0);

	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=*(out++);
			sch |= (*(out++))<<8;
			vmem[y*DpyCols+x]=sch;
		}
	}
}

int x_gettext(int sx, int sy, int ex, int ey, unsigned char *fill)
{
	int x,y;
	unsigned char *out;
	WORD	sch;
	struct text_info	ti;

	gettextinfo(&ti);

	if(		   sx < 1
			|| sy < 1
			|| ex < 1
			|| ey < 1
			|| sx > ti.screenwidth
			|| sy > ti.screenheight
			|| sx > ex
			|| sy > ey
			|| ex > ti.screenwidth
			|| ey > ti.screenheight
			|| fill==NULL)
		return(0);

	out=fill;
	for(y=sy-1;y<ey;y++) {
		for(x=sx-1;x<ex;x++) {
			sch=vmem[y*DpyCols+x];
			*(out++)=sch & 0xff;
			*(out++)=sch >> 8;
		}
	}
}

void x_textattr(unsigned char attr)
{
	x_curr_attr=attr<<8;
}

int x_kbhit(void)
{
	return(tty_kbhit());
}

void x_delay(long msec)
{
	usleep(msec*1000);
}

int x_wherey(void)
{
	return(CursRow0)+1;
}

int x_wherex(void)
{
	return(CursCol0)+1;
}

/* Put the character _c on the screen at the current cursor position. 
 * The special characters return, linefeed, bell, and backspace are handled
 * properly, as is line wrap and scrolling. The cursor position is updated. 
 */
int x_putch(unsigned char ch)
{
	struct text_info ti;
	WORD sch;

	sch=x_curr_attr|ch;

	switch(ch) {
		case '\r':
			CursCol0=0;
			break;
		case '\n':
			gettextinfo(&ti);
			if(wherey()==ti.winbottom-ti.wintop+1)
				wscroll();
			else
				CursRow0++;
			break;
		case '\b':
			sch=0x0700;
			if(CursCol0>0)
				CursCol0--;
			vmem[CursCol0+CursRow0*DpyCols]=sch;
			break;
		case 7:		/* Bell */
			tty_beep();
			break;
		default:
			gettextinfo(&ti);
			if(wherey()==ti.winbottom-ti.wintop+1
					&& wherex()==ti.winright-ti.winleft+1) {
				vmem[CursCol0+CursRow0*DpyCols]=sch;
				wscroll();
				gotoxy(ti.winleft,wherey());
			}
			else {
				if(wherex()==ti.winright-ti.winleft+1) {
					vmem[CursCol0+CursRow0*DpyCols]=sch;
					gotoxy(ti.winleft,ti.cury+1);
				}
				else {
					vmem[CursCol0+CursRow0*DpyCols]=sch;
					gotoxy(ti.curx+1,ti.cury);
				}
			}
			break;
	}

	return(ch);
}

void x_gotoxy(int x, int y)
{
	CursRow0=y-1;
	CursCol0=x-1;
}

void x_gettextinfo(struct text_info *info)
{
	info->currmode=VideoMode;
	info->screenheight=DpyRows+1;
	info->screenwidth=DpyCols;
	info->curx=wherex();
	info->cury=wherey();
	info->attribute=x_curr_attr>>8;
}

void x_setcursortype(int type)
{
	switch(type) {
		case _NOCURSOR:
			CursStart=0xff;
			CursEnd=0;
			break;
		case _SOLIDCURSOR:
			CursStart=0;
			CursEnd=FH;
			break;
		default:
		    CursStart = VGA_CRTC[CRTC_CursStart];
		    CursEnd = VGA_CRTC[CRTC_CursEnd];
			break;
	}
}

int x_getch(void)
{
	return(tty_read(TTYF_BLOCK));
}

int x_getche(void)
{
	int ch;

	if(x_nextchar)
		return(x_getch());
	ch=x_getch();
	if(ch)
		putch(ch);
	return(ch);
}

int x_beep(void)
{
	tty_beep();
	return(0);
}

void x_textmode(int mode)
{
	init_mode(mode);
}
