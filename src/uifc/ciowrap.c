#include "ciowrap.h"
#ifndef NO_X
 #include "x_cio.h"
#endif
#include "curs_cio.h"

cioapi_t	cio_api;

void initciowrap(int mode)
{
#ifndef NO_X
	if(!console_init()) {
		cio_api.puttext=x_puttext;
		cio_api.gettext=x_gettext;
		cio_api.textattr=x_textattr;
		cio_api.textbackground=x_textbackground;
		cio_api.textcolor=x_textcolor;
		cio_api.kbhit=x_kbhit;
		cio_api.delay=x_delay;
		cio_api.wherey=x_wherey;
		cio_api.wherex=x_wherex;
		cio_api.putch=x_putch;
		cio_api.c_printf=x_cprintf;
		cio_api.cputs=x_cputs;
		cio_api.gotoxy=x_gotoxy;
		cio_api.clrscr=x_clrscr;
		cio_api.gettextinfo=x_gettextinfo;
		cio_api.setcursortype=x_setcursortype;
		cio_api.clreol=x_clreol;
		cio_api.getch=x_getch;
		return;
	}
	fprintf(stderr,"X init failed\n");
#endif
	curs_initciowrap(mode);
	cio_api.puttext=curs_puttext;
	cio_api.gettext=curs_gettext;
	cio_api.textattr=curs_textattr;
	cio_api.textbackground=curs_textbackground;
	cio_api.textcolor=curs_textcolor;
	cio_api.kbhit=curs_kbhit;
	cio_api.delay=curs_delay;
	cio_api.wherey=curs_wherey;
	cio_api.wherex=curs_wherex;
	cio_api.putch=curs_putch;
	cio_api.c_printf=curs_cprintf;
	cio_api.cputs=curs_cputs;
	cio_api.gotoxy=curs_gotoxy;
	cio_api.clrscr=curs_clrscr;
	cio_api.gettextinfo=curs_gettextinfo;
	cio_api.setcursortype=curs_setcursortype;
	cio_api.clreol=curs_clreol;
	cio_api.getch=getch;
}
