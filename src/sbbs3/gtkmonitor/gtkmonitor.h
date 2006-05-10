#ifndef _GTKMONITOR_H_
#define _GTKMONITOR_H_

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "sbbs.h"

extern scfg_t          cfg;
extern GladeXML        *xml;
extern int             nodes;
extern GtkListStore    *store;
extern GtkTreeSelection *sel;

int refresh_data(gpointer data);
void refresh_events(void);

#endif
