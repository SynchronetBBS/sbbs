#include "xpdoor.h"

int main(int argc, char **argv)
{
	xpd_parse_cmdline(argc, argv);
	xpd_parse_dropfile();
	xpd_init();
	return(1);
}
