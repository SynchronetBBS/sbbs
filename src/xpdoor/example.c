#include "xpdoor.h"

int main(int argc, char **argv)
{
	xpd_init();
	xpd_parse_cmdline(argc, argv);
	xpd_parse_dropfile();

	cprintf("Drop path: %s\r\n",xpd_info.drop.path?xpd_info.drop.path:"<Unknown>");

	cprintf("Welcome %s!\r\n\r\n",xpd_info.drop.user.full_name?xpd_info.drop.user.full_name:"<unknown>");

	cputs("Press <ENTER> to try out raw writes...");
	getch();

	xpd_rwrite("\x1b[2J\x1b[1;1H\x1b[32mTest\r\n", -1);
	
	cputs("\r\nPress <ENTER> to exit: ");
	getch();
	cputs("\r\n");
	xpd_exit();
	return(1);
}
