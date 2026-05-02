/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _UIFCINIT_H_
#define _UIFCINIT_H_

#include <stdbool.h>
#include <uifc.h>

#include "filepick.h"   /* struct file_pick */

extern uifcapi_t uifc; /* User Interface (UIFC) Library API */
void set_uifc_title(void);
int init_uifc(bool scrn, bool bottom);
void uifcbail(void);
void uifcmsg(char *msg, char *helpbuf);
int uifcinput(char *title, int len, char *msg, int mode, char *helpbuf);
int confirm(char *msg, char *helpbuf);
/* Wrap filepick / filepick_multi in the same save/init/bail dance
 * as uifcmsg() so they can be invoked safely while a connection is
 * up (where the UIFC layer otherwise has stale per-session state).
 * Forwards return value from the underlying filepick call. */
int uifcfilepick(char *title, struct file_pick *fp,
                 const char *initial_dir, const char *default_mask, int opts);
int uifcfilepick_multi(char *title, struct file_pick *fp,
                       const char *initial_dir, const char *default_mask, int opts);

#endif
