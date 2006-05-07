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
	char		str[1024];
	GtkWidget	*cCommandShell;
	GtkWidget	*cExternalEditor;
	GtkWidget	*cDefaultDownloadProtocol;
	GtkWidget	*cTempQWKFileType;
	GtkWidget	*w;
	int			i;

	/* Clear out old combo boxes */
	cCommandShell=glade_xml_get_widget(xml, "cCommandShell");
	if(w==NULL) {
		fprintf(stderr,"Cannot get the command shell widget\n");
		return(-1);
	}
	for(i=0; i<cfg.total_shells; i++)
		gtk_combo_box_remove_text(GTK_COMBO_BOX(cCommandShell), 0);
	cExternalEditor=glade_xml_get_widget(xml, "cExternalEditor");
	if(w==NULL) {
		fprintf(stderr,"Cannot get the external editor widget\n");
		return(-1);
	}
	for(i=0; i<cfg.total_xedits; i++)
		gtk_combo_box_remove_text(GTK_COMBO_BOX(cExternalEditor), 0);
	cDefaultDownloadProtocol=glade_xml_get_widget(xml, "cDefaultDownloadProtocol");
	if(w==NULL) {
		fprintf(stderr,"Cannot get the default download protocol widget\n");
		return(-1);
	}
	for(i=0; i<cfg.total_prots; i++)
		gtk_combo_box_remove_text(GTK_COMBO_BOX(cDefaultDownloadProtocol), 0);
	cTempQWKFileType=glade_xml_get_widget(xml, "cTempQWKFileType");
	if(w==NULL) {
		fprintf(stderr,"Cannot get the temp/QWK file type widget\n");
		return(-1);
	}
	for(i=0; i<cfg.total_fcomps; i++)
		gtk_combo_box_remove_text(GTK_COMBO_BOX(cTempQWKFileType), 0);

    /* Read .cfg files here */
    if(!load_cfg(&cfg, NULL, TRUE, str)) {
		fprintf(stderr,"Cannot load configuration data\n");
        return(-1);
	}

	/* Re-add combobox values */
	for(i=0; i<cfg.total_shells; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(cCommandShell), cfg.shell[i]->name);
	gtk_combo_box_append_text(GTK_COMBO_BOX(cExternalEditor), "Internal Editor");
	for(i=0; i<cfg.total_xedits; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(cExternalEditor), cfg.xedit[i]->name);
	for(i=0; i<cfg.total_prots; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(cDefaultDownloadProtocol), cfg.prot[i]->name);
	for(i=0; i<cfg.total_fcomps; i++)
		gtk_combo_box_append_text(GTK_COMBO_BOX(cTempQWKFileType), cfg.fcomp[i]->ext);

	totalusers=lastuser(&cfg);
	sprintf(str,"of %d",totalusers);
	w=glade_xml_get_widget(xml, "lTotalUsers");
	if(w==NULL) {
		fprintf(stderr,"Cannot get the total users widget\n");
		return(-1);
	}
	gtk_label_set_text(GTK_LABEL(w), str);

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
    memset(&cfg,0,sizeof(cfg));
    cfg.size=sizeof(cfg);
    SAFECOPY(cfg.ctrl_dir,ctrl_dir);

	/*
	 * The following are to make up for Glade hacks... we want a text list
	 * in the combo boxes... Galde does this *if* there's predefined text.
	 * So, we need to remove it during the refresh.
	 */
	cfg.total_shells=1;
	cfg.total_xedits=1;
	cfg.total_prots=1;
	cfg.total_fcomps=1;

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
