#include <gtk/gtk.h>

#include "sbbs.h"
#include "dirwrap.h"

#include "events.h"
#include "gtkuseredit.h"

scfg_t		cfg;
user_t		user;
int			totalusers=0;
int			current_user=0;
char		glade_path[MAX_PATH+1];
GtkBuilder*	builder;
extern const char builder_interface[];

/* Refreshes global variables... ie: Number of users */
int refresh_globals(void)
{
	char		str[1024];
	GtkComboBox	*cCommandShell;
	GtkListStore*cCS;
	GtkComboBox	*cExternalEditor;
	GtkListStore*cEE;
	GtkComboBox	*cDefaultDownloadProtocol;
	GtkListStore*cDDP;
	GtkComboBox	*cTempQWKFileType;
	GtkListStore*cTQFT;
	GtkWidget	*w;
	GtkTreeIter	curr;
	int			i;
	GtkCellRenderer *column;
	gboolean	notatend;

	/* Create new lists as needed */
	cCommandShell=GTK_COMBO_BOX(gtk_builder_get_object(builder, "cCommandShell"));
	if((cCS = GTK_LIST_STORE(gtk_combo_box_get_model(cCommandShell))) == NULL) {
		cCS = gtk_list_store_new(1, G_TYPE_STRING);
		gtk_combo_box_set_model(cCommandShell, GTK_TREE_MODEL(cCS));
		column = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(cCommandShell), column, TRUE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(cCommandShell), column,
				"text", 0,
				NULL
		);
		gtk_combo_box_set_active(GTK_COMBO_BOX(cCommandShell), 0);
	}

	cExternalEditor=GTK_COMBO_BOX(gtk_builder_get_object(builder, "cExternalEditor"));
	if((cEE = GTK_LIST_STORE(gtk_combo_box_get_model(cExternalEditor))) == NULL) {
		cEE = gtk_list_store_new(1, G_TYPE_STRING);
		gtk_combo_box_set_model(cExternalEditor, GTK_TREE_MODEL(cEE));
		gtk_list_store_insert(cEE, &curr, 0);
		gtk_list_store_set(cEE, &curr, 0, "Internal Editor", -1);
		column = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(cExternalEditor), column, TRUE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(cExternalEditor), column,
				"text", 0,
				NULL
		);
		gtk_combo_box_set_active(GTK_COMBO_BOX(cExternalEditor), 0);
	}

	cDefaultDownloadProtocol=GTK_COMBO_BOX(gtk_builder_get_object(builder, "cDefaultDownloadProtocol"));
	if((cDDP = GTK_LIST_STORE(gtk_combo_box_get_model(cDefaultDownloadProtocol))) == NULL) {
		cDDP = gtk_list_store_new(1, G_TYPE_STRING);
		gtk_combo_box_set_model(cDefaultDownloadProtocol, GTK_TREE_MODEL(cDDP));
		gtk_list_store_insert(cDDP, &curr, 0);
		gtk_list_store_set(cDDP, &curr, 0, "Not Specified", -1);
		column = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(cDefaultDownloadProtocol), column, TRUE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(cDefaultDownloadProtocol), column,
				"text", 0,
				NULL
		);
		gtk_combo_box_set_active(GTK_COMBO_BOX(cDefaultDownloadProtocol), 0);
	}

	cTempQWKFileType=GTK_COMBO_BOX(gtk_builder_get_object(builder, "cTempQWKFileType"));
	if((cTQFT = GTK_LIST_STORE(gtk_combo_box_get_model(cTempQWKFileType))) == NULL) {
		cTQFT = gtk_list_store_new(1, G_TYPE_STRING);
		gtk_combo_box_set_model(cTempQWKFileType, GTK_TREE_MODEL(cTQFT));
		column = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(cTempQWKFileType), column, TRUE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(cTempQWKFileType), column,
				"text", 0,
				NULL
		);
		gtk_combo_box_set_active(GTK_COMBO_BOX(cTempQWKFileType), 0);
	}

    /* Read .cfg files here */
    if(!load_cfg(&cfg, NULL, TRUE, str)) {
		fprintf(stderr,"Cannot load configuration data: %s\n", str);
        return(-1);
	}

	/* Re-add combobox values */
	notatend = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cCS), &curr);
	for(i=0; i<cfg.total_shells; i++) {
		if(!notatend)
			gtk_list_store_append(cCS, &curr);
		gtk_list_store_set(cCS, &curr, 0, cfg.shell[i]->name, -1);
		notatend = gtk_tree_model_iter_next(GTK_TREE_MODEL(cCS), &curr);
	}
	while(notatend)
		notatend = gtk_list_store_remove(cCS, &curr);

	notatend = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cEE), &curr);
	if(notatend)
		notatend = gtk_tree_model_iter_next(GTK_TREE_MODEL(cEE), &curr);
	for(i=0; i<cfg.total_xedits; i++) {
		if(!notatend)
			gtk_list_store_append(cEE, &curr);
		gtk_list_store_set(cEE, &curr, 0, cfg.xedit[i]->name, -1);
		notatend = gtk_tree_model_iter_next(GTK_TREE_MODEL(cEE), &curr);
	}
	while(notatend)
		notatend = gtk_list_store_remove(cEE, &curr);

	notatend = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cDDP), &curr);
	if(notatend)
		notatend = gtk_tree_model_iter_next(GTK_TREE_MODEL(cDDP), &curr);
	for(i=0; i<cfg.total_prots; i++) {
		if(!notatend)
			gtk_list_store_append(cDDP, &curr);
		gtk_list_store_set(cDDP, &curr, 0, cfg.prot[i]->name, -1);
		notatend = gtk_tree_model_iter_next(GTK_TREE_MODEL(cDDP), &curr);
	}
	while(notatend)
		notatend = gtk_list_store_remove(cDDP, &curr);

	notatend = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(cTQFT), &curr);
	for(i=0; i<cfg.total_fcomps; i++) {
		if(!notatend)
			gtk_list_store_append(cTQFT, &curr);
		gtk_list_store_set(cTQFT, &curr, 0, cfg.fcomp[i]->ext, -1);
		notatend = gtk_tree_model_iter_next(GTK_TREE_MODEL(cTQFT), &curr);
	}
	while(notatend)
		notatend = gtk_list_store_remove(cTQFT, &curr);

	totalusers=lastuser(&cfg);
	sprintf(str,"of %d",totalusers);
	w=GTK_WIDGET(gtk_builder_get_object(builder, "lTotalUsers"));
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

	if(refresh_globals())
		return(-1);
	if(totalusers > 0)
		update_current_user(1);
	return(0);
}

int main(int argc, char *argv[]) {
	GtkWindow *xml;

    gtk_init(&argc, &argv);

    /* load the interface */
	strcpy(glade_path, argv[0]);
	strcpy(getfname(glade_path), "gtkuseredit.glade");
	builder = gtk_builder_new ();
	gtk_builder_add_from_string (builder, builder_interface, -1, NULL);

    /* connect the signals in the interface */
	gtk_builder_connect_signals (builder, NULL);

	if(read_config())
		return(1);

	/* Get MainWindow and display it */
	xml = GTK_WINDOW (gtk_builder_get_object (builder, "MainWindow"));
	gtk_window_present(xml);

	if(argc>1) {
		if(atoi(argv[1]))
			update_current_user(atoi(argv[1]));
		else {
			unsigned int	nu;
			nu=matchuser(&cfg, argv[1], TRUE);
			if(nu)
				update_current_user(nu);
			else {
				GtkWindow        *cxml;
				cxml = GTK_WINDOW(gtk_builder_get_object(builder, "NotFoundWindow"));
				gtk_window_present(cxml);
			}
		}
	}

    /* start the event loop */
    gtk_main();
    return 0;
}
