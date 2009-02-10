#include "xpdoor.h"

int main(int argc, char **argv)
{
	int		i,x,y;
	char	buf[2];

	xpd_init();
	xpd_parse_cmdline(argc, argv);
	xpd_parse_dropfile();

	cprintf("Drop path: %s\r\n",xpd_info.drop.path?xpd_info.drop.path:"<Unknown>");

	cprintf("Welcome %s!\r\n\r\n",xpd_info.drop.user.full_name?xpd_info.drop.user.full_name:"<unknown>");

	cputs("Press <ENTER> to try out raw writes...");
	getch();

	xpd_rwrite("\x1b[2J\x1b[1;1H\x1b[32mTest\r\n", -1);

	cputs("\r\nPress <ENTER> for an ASCII table: ");
	getch();
	clrscr();
	gotoxy(1,1);
	for(i=0; i<256; i++) {
//if(i==0x0a)
//continue;
		textattr(8);
		cprintf("%02x:",i);
#if 0
		buf[0]=i;
		buf[1]=7;
		x=wherex();
		y=wherey();
		putch(' ');
		puttext(x,y,x,y,buf);
#else
		textattr(7);
		buf[0]=0;
		buf[1]=i;
		xpd_rwrite(buf,2);
#endif
		putch(' ');
	}
	textattr(7);

	cputs("\r\nPress <ENTER> to exit: ");
	getch();
	cputs("\r\n");
	xpd_exit();
	return(1);
}
