#include <gtk/gtk.h>
#include <glade/glade.h>

#include "sbbs.h"
#include "dirwrap.h"

#include "events.h"
#include "gtkuseredit.h"

scfg_t	cfg;
user_t	user;
GladeXML *xml;
int		totalusers=0;
int		current_user=0;

/* Refreshes global variables... ie: Number of users */
int refresh_globals(void)
{
	gchar		newlabel[14];
	GtkWidget	*lTotalUsers;

	totalusers=lastuser(&cfg);
	sprintf(newlabel,"of %d",totalusers);
	lTotalUsers=glade_xml_get_widget(xml, "lTotalUsers");
	if(lTotalUsers==NULL) {
		fprintf(stderr,"Cannot get the total users widget\n");
		return(-1);
	}
	gtk_label_set_text(GTK_LABEL(lTotalUsers), newlabel);
	return(0);
}

/* Initializes global stuff, loads first user etc */
int read_config(void)
{
	char	ctrl_dir[MAX_PATH+1];
	char	str[1024];
	char	*p;

	p=getenv("SBBSCTRL");
	if(p==NULL) {
		fprintf(stderr,"SBBSCTRL not set\n");
		return(-1);
	}
	SAFECOPY(ctrl_dir, p);
	prep_dir("",ctrl_dir,sizeof(ctrl_dir));
	if(!isdir(ctrl_dir)) {
		fprintf(stderr,"SBBSCTRL does not point to a directory\n");
		return(-1);
	}
    /* Read .cfg files here */
    memset(&cfg,0,sizeof(cfg));
    cfg.size=sizeof(cfg);
    SAFECOPY(cfg.ctrl_dir,ctrl_dir);
    if(!load_cfg(&cfg, NULL, TRUE, str)) {
		fprintf(stderr,"Cannot load configuration data\n");
        return(-1);
	}
	if(refresh_globals())
		return(-1);
	if(totalusers > 0)
		update_current_user(1);
	return(0);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    glade_init();

    /* load the interface */
    xml = glade_xml_new("gtkuseredit.glade", "MainWindow", NULL);
    /* connect the signals in the interface */
    glade_xml_signal_autoconnect(xml);
	/* Set up the global config stuff. */
	if(read_config())
		return(1);
    /* start the event loop */
    gtk_main();
    return 0;
}
