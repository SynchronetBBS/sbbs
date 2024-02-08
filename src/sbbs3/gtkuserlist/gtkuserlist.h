#ifndef _GTKUSERLIST_H_
#define _GTKUSERLIST_H_

#include <gtk/gtk.h>

#undef JAVASCRIPT
#undef USE_CRYPTLIB
#include "sbbs.h"
#define USE_CRYPTLIB

extern GtkBuilder	*builder;
extern scfg_t		cfg;
extern uchar		*arbuf;

#endif
