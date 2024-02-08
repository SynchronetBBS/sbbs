#ifndef _GTKMONITOR_H_
#define _GTKMONITOR_H_

#include <gtk/gtk.h>

#include "sbbs.h"

struct gtkmonitor_config {
	char	view_stdout[MAX_PATH+1];	/* %f | xmessage -file - */
	char	view_text_file[MAX_PATH+1];	/* xmessage -file %f */
	char	edit_text_file[MAX_PATH+1];	/* xedit %f */
	char	view_ctrla_file[MAX_PATH+1];/* %!asc2ans %f | %ssyncview -l */
	char	view_html_file[MAX_PATH+1];	/* firefox file://%f */
};

extern struct gtkmonitor_config gtkm_conf;
extern scfg_t			cfg;
extern GtkBuilder*		builder;
extern int				nodes;
extern GtkListStore*	store;
extern GtkTreeSelection*sel;
extern char				glade_path[MAX_PATH+1];

int refresh_data(gpointer data);
void refresh_events(void);
void write_ini(void);

#endif
