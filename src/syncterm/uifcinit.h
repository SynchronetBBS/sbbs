/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _UIFCINIT_H_
#define _UIFCINIT_H_

#include <stdbool.h>
#include <uifc.h>

extern uifcapi_t uifc; /* User Interface (UIFC) Library API */
void set_uifc_title(void);
int init_uifc(bool scrn, bool bottom);
void uifcbail(void);
void uifcmsg(char *msg, char *helpbuf);
void uifcinput(char *title, int len, char *msg, int mode, char *helpbuf);
int confirm(char *msg, char *helpbuf);

#endif
