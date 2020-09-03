/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: uifcinit.h,v 1.11 2015/02/09 07:34:23 deuce Exp $ */

#ifndef _UIFCINIT_H_
#define _UIFCINIT_H_

#include <uifc.h>

extern	uifcapi_t uifc; /* User Interface (UIFC) Library API */
int	init_uifc(BOOL scrn, BOOL bottom);
void uifcbail(void);
void uifcmsg(char *msg, char *helpbuf);
void uifcinput(char *title, int len, char *msg, int mode, char *helpbuf);
int confirm(char *msg, char *helpbuf);

#endif
