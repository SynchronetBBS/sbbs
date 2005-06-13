/* $Id$ */

#include <gen_defs.h>
#include <stdio.h>

#include <ciolib.h>
#include <uifc.h>

#include "uifcinit.h"

uifcapi_t uifc; /* User Interface (UIFC) Library API */
static int uifc_initialized=0;

int	init_uifc(void) {
	int	i;
	struct	text_info txtinfo;

    gettextinfo(&txtinfo);
	if(uifc_initialized)
		return(0);
	uifc.scrn_len=txtinfo.screenheight;
	if((i=uifcini32(&uifc))!=0) {
		fprintf(stderr,"uifc library init returned error %d\n",i);
		return(-1);
	}
	uifc_initialized=1;

	if(uifc.scrn("SyncTERM")) {
		printf(" USCRN (len=%d) failed!\n",uifc.scrn_len+1);
		uifc_initialized=0;
		uifc.bail();
	}

	return(0);
}

void uifcbail(void)
{
	if(uifc_initialized) {
		uifc.bail();
	}
	uifc_initialized=0;
}

void uifcmsg(char *msg, char *helpbuf)
{
	int i;
	char	*buf;
	struct	text_info txtinfo;

    gettextinfo(&txtinfo);
	i=uifc_initialized;
	if(!i) {
		buf=(char *)malloc(txtinfo.screenheight*txtinfo.screenwidth*2);
		gettext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
	}
	init_uifc();
	if(uifc_initialized) {
		uifc.helpbuf=helpbuf;
		uifc.msg(msg);
	}
	else
		fprintf(stderr,"%s\n",msg);
	if(!i) {
		uifcbail();
		puttext(1,1,txtinfo.screenwidth,txtinfo.screenheight,buf);
		free(buf);
	}
}
