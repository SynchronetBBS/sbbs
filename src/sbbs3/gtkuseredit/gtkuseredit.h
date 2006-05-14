#ifndef _GTKUSEREDIT_H_
#define _GTKUSEREDIT_H_

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "sbbs.h"

extern scfg_t	cfg;
extern user_t	user;
extern GladeXML *xml;
extern int		totalusers;
extern int		current_user;
extern char		glade_path[MAX_PATH+1];

#endif
