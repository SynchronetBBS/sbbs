/* $Id$ */

#ifndef _UIFCINIT_H_
#define _UIFCINIT_H_

#include <uifc.h>

extern	uifcapi_t uifc; /* User Interface (UIFC) Library API */
int	init_uifc(BOOL scrn, BOOL bottom);
void uifcbail(void);
void uifcmsg(char *msg, char *helpbuf);
int confirm(char *msg, char *helpbuf);

#endif
