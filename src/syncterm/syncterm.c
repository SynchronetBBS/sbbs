#include <stdlib.h>
#include <ciowrap.h>

#include "bbslist.h"
#include "rlogin.h"
#include "uifcinit.h"

int main(int argc, char **argv)
{
	struct bbslist *bbs;
	struct	text_info txtinfo;

	initciowrap(UIFC_IBM|COLOR_MODE);
    gettextinfo(&txtinfo);
	if((txtinfo.screenwidth<40) || txtinfo.screenheight<24) {
		fputs("Window too small, must be at lest 80x24\n",stderr);
		return(1);
	}

	atexit(uifcbail);
	while((bbs=show_bbslist(BBSLIST_SELECT))!=NULL) {
		if(!rlogin_connect(bbs->addr,bbs->port,bbs->user,bbs->password)) {
			/* ToDo: Update the entry with new lastconnected */
			/* ToDo: Disallow duplicate entries */
			uifcbail();
			if(drawwin())
				return(1);
			doterm();
		}
	}
}
