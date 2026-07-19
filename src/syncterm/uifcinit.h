/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _UIFCINIT_H_
#define _UIFCINIT_H_

#include <stdbool.h>
#include <uifc.h>

#include "filepick.h"   /* struct file_pick */

extern uifcapi_t uifc; /* User Interface (UIFC) Library API */
void uifcbail(void);
/* Initialize and tear down UIFC around each native picker call. */
int uifcfilepick(const char *title, struct file_pick *fp,
                 const char *initial_path, const char *default_mask, int opts);
int uifcfilepick_multi(const char *title, struct file_pick *fp,
                       const char *initial_dir, const char *default_mask, int opts);

#endif
