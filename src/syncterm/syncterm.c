#include <stdlib.h>
#include <ciowrap.h>

#include "bbslist.h"
#include "rlogin.h"
#include "uifcinit.h"

#ifdef _WINSOCKAPI_

static WSADATA WSAData;
#define SOCKLIB_DESC WSAData.szDescription
static BOOL WSAInitialized=FALSE;

static BOOL winsock_startup(void)
{
	int		status;             /* Status Code */

    if((status = WSAStartup(MAKEWORD(1,1), &WSAData))==0) {
		lprintf(LOG_INFO,"%s %s",WSAData.szDescription, WSAData.szSystemStatus);
		WSAInitialized=TRUE;
		return (TRUE);
	}

    lprintf(LOG_ERR,"!WinSock startup ERROR %d", status);
	return (FALSE);
}

#else /* No WINSOCK */

#define winsock_startup()	(TRUE)
#define SOCKLIB_DESC NULL

#endif

int main(int argc, char **argv)
{
	struct bbslist *bbs;
	struct	text_info txtinfo;

	if(!winsock_startup())
		return(1);

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
			rlogin_close();
		}
	}
#ifdef _WINSOCKAPI_
	if(WSAInitialized && WSACleanup()!=0) 
		lprintf(LOG_ERR,"!WSACleanup ERROR %d",ERROR_VALUE);
#endif
}
