#include "dirwrap.h"
#include "ini_file.h"

#include "events.h"
#include "gtkmonitor.h"
#include "gtkmonitor.h"
#include "util_funcs.h"

scfg_t		cfg;
int			nodes=0;
GtkListStore	*store = NULL;
GtkTreeSelection *sel;
int				quickslots=0;
GtkListStore	*quickstore = NULL;
GtkBuilder*	builder;
struct gtkmonitor_config gtkm_conf;
extern const char builder_interface[];

void read_ini(void)
{
	FILE	*inif;
	char	path[MAX_PATH+1];

	complete_path(path, cfg.ctrl_dir, "gtkmonitor.ini");
	inif=fopen(path, "r");
	iniReadString(inif, "commands", "ViewStdOut", "%f | xmessage -file -", gtkm_conf.view_stdout);
	iniReadString(inif, "commands", "ViewTextFile", "xmessage -file %f", gtkm_conf.view_text_file);
	iniReadString(inif, "commands", "EditTextFile", "xedit %f",gtkm_conf.edit_text_file);
	iniReadString(inif, "commands", "ViewCtrlAFile", "%!asc2ans %f | %!syncview -l",gtkm_conf.view_ctrla_file);
	iniReadString(inif, "commands", "ViewHTMLFile", "firefox file://%f", gtkm_conf.view_html_file);
	if(inif)
		fclose(inif);
	else
		write_ini();
}

void write_ini(void)
{
	FILE	*inif;
	char	path[MAX_PATH+1];
	str_list_t	inifile;

	complete_path(path, cfg.ctrl_dir, "gtkmonitor.ini");
	inif=fopen(path, "r");
	if(inif) {
		inifile=iniReadFile(inif);
		fclose(inif);
	}
	else
		inifile=strListInit();
	iniSetString(&inifile, "commands", "ViewStdOut", gtkm_conf.view_stdout, NULL);
	iniSetString(&inifile, "commands", "ViewTextFile", gtkm_conf.view_text_file, NULL);
	iniSetString(&inifile, "commands", "EditTextFile", gtkm_conf.edit_text_file, NULL);
	iniSetString(&inifile, "commands", "ViewCtrlAFile", gtkm_conf.view_ctrla_file, NULL);
	iniSetString(&inifile, "commands", "ViewHTMLFile", gtkm_conf.view_html_file, NULL);
	inif=fopen(path, "w");
	if(inif) {
		iniWriteFile(inif, inifile);
		fclose(inif);
	}
	else
		display_message("Cannot Create .ini File","Unable to create the .ini file.  Check your permissions.","gtk-dialog-error");
	strListFree(&inifile);
}

void refresh_events(void)
{
	int		i;
	GtkWidget	*w;
	GtkWidget	*menu;
	GtkComboBox	*cb;
	GtkTreeIter	curr;
	GtkCellRenderer *column;
	char	str[1024];
	char	flags[33];

    /* Read .cfg files here */
	free_cfg(&cfg);
    if(!load_cfg(&cfg, NULL, TRUE, str)) {
		char error[256];
		SAFEPRINTF(error, "ERROR Loading Configuration Files: %s", str);
		display_message("Load Error",error,"gtk-dialog-error");
        return;
	}

	/* Update timed events */
	w = GTK_WIDGET(gtk_builder_get_object (builder, "force_timed_event1"));
	if(w==NULL)
		fprintf(stderr,"Cannot get timed event widget\n");
	else {
		menu=gtk_menu_item_get_submenu(GTK_MENU_ITEM(w));
		gtk_widget_destroy(GTK_WIDGET(menu));
		menu=gtk_menu_new();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (w), menu);
		if(menu!=NULL) {
			for(i=0; i<cfg.total_events; i++) {
				w = gtk_menu_item_new_with_label(cfg.event[i]->code);
				gtk_widget_show (w);
				gtk_container_add (GTK_CONTAINER (menu), w);
				g_signal_connect ((gpointer) w, "activate"
						,G_CALLBACK (on_force_event)
						,NULL);
			}
		}
		else
			fprintf(stderr,"Cannot get timed event submenu\n");
	}

	/* Update network call-outs */
	w=GTK_WIDGET (gtk_builder_get_object (builder, "force_network_callout1"));
	if(w==NULL)
		fprintf(stderr,"Cannot get network callout widget\n");
	else {
		menu=gtk_menu_item_get_submenu(GTK_MENU_ITEM(w));
		gtk_widget_destroy(GTK_WIDGET(menu));
		menu=gtk_menu_new();
		gtk_menu_item_set_submenu (GTK_MENU_ITEM (w), menu);
		if(menu!=NULL) {
			for(i=0; i<cfg.total_qhubs; i++) {
				w = gtk_menu_item_new_with_label(cfg.qhub[i]->id);
				gtk_widget_show (w);
				gtk_container_add (GTK_CONTAINER (menu), w);
				g_signal_connect ((gpointer) w, "activate"
						,G_CALLBACK (on_force_qnet)
						,NULL);
			}
		}
		else
			fprintf(stderr,"Cannot get timed event submenu\n");
	}
	refresh_data(NULL);

	/* Set up quick validation values */
	cb=GTK_COMBO_BOX (gtk_builder_get_object (builder, "cNodeQuickValidate"));
	/* Fist call... set up grid */
	if(quickstore == NULL) {
		quickstore = gtk_list_store_new(1, G_TYPE_STRING);
		gtk_combo_box_set_model(cb, GTK_TREE_MODEL(quickstore));
		gtk_list_store_insert(quickstore, &curr, 0);
		gtk_list_store_set(quickstore, &curr, 0, "Quick Validation Sets", -1);
		column = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(cb), column, TRUE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(cb), column,
				"text", 0,
				NULL
		);
		gtk_combo_box_set_active(GTK_COMBO_BOX(cb), 0);
	}

	gtk_list_store_move_after(quickstore, &curr, 0);
	for(i=1; i<=10; i++) {
		if(i > quickslots) {
			gtk_list_store_append(quickstore, &curr);
			quickslots++;
		}
		sprintf(str,"%-2d  SL: %-2d  F1: %s",i,cfg.val_level[i],ltoaf(cfg.val_flags1[i],flags));
		gtk_list_store_set(quickstore, &curr, 0, str, -1);
		gtk_tree_model_iter_next(GTK_TREE_MODEL(quickstore), &curr);
	}
}

void add_to_stats(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	stats_t	*nstats=data;
	stats_t	stats;
	gchar	*node;

	gtk_tree_model_get(model, iter, 0, &node, -1);
	getstats(&cfg, atoi(node), &stats);
	nstats->logons+=stats.logons;
	nstats->ltoday+=stats.ltoday;
	nstats->timeon+=stats.timeon;
	nstats->ttoday+=stats.ttoday;
	nstats->uls+=stats.uls;
	nstats->ulb+=stats.ulb;
	nstats->dls+=stats.dls;
	nstats->dlb+=stats.dlb;
	nstats->ptoday+=stats.ptoday;
	nstats->etoday+=stats.etoday;
	nstats->ftoday+=stats.ftoday;
	nstats->nusers+=stats.nusers;
}

/* Refreshes global variables... ie: Number of users */
int refresh_data(gpointer data)
{
	char		str[1024];
	char		str2[1024];
	char		str3[1024];
	GtkWidget	*w;
	int			i,j;
	GtkTreeIter	curr;
	node_t	node;
	stats_t	sstats;
	stats_t	nstats;
	int		shownodes;

	/* Update the node list stuff */
	w=GTK_WIDGET(gtk_builder_get_object (builder, "lNodeList"));

	/* Fist call... set up grid */
	if(store == NULL) {
		store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
		gtk_tree_view_set_model(GTK_TREE_VIEW(w), GTK_TREE_MODEL(store));
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(w), 0, "Node", gtk_cell_renderer_text_new(), "text", 0, NULL);
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(w), 1, "Status", gtk_cell_renderer_text_new(), "text", 1, NULL);
		nodes=0;
		sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (w));
		gtk_tree_selection_set_mode (sel, GTK_SELECTION_MULTIPLE);
		g_signal_connect (G_OBJECT (sel), "changed", G_CALLBACK (update_stats_callback), NULL);
	}
	else
		gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &curr);

	for(i=1; i<=cfg.sys_nodes; i++) {
		if(i > nodes) {
			gtk_list_store_insert(store, &curr, i-1);
			sprintf(str,"%d",i);
			gtk_list_store_set(store, &curr, 0, str, -1);
			nodes++;
		}

		if((j=getnodedat(&cfg,i,&node,FALSE,NULL)))
			sprintf(str,"Error reading node data (%d)!",j);
		else
			nodestatus(&cfg,&node,str,1023, i);
		gtk_list_store_set(store, &curr, 1, str, -1);
		gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &curr);
	}
	if(nodes >= i) {
		for(i=nodes; i<=cfg.sys_nodes; i--) {
			gtk_list_store_remove(store, &curr);
			nodes--;
		}
	}

	getstats(&cfg, 0, &sstats);

	shownodes=gtk_tree_selection_count_selected_rows(sel);
	if(shownodes) {
		memset(&nstats, 0, sizeof(nstats));
		gtk_tree_selection_selected_foreach(sel
				,add_to_stats
				,&nstats);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "locknodebutton"));
	gtk_widget_set_sensitive(w, shownodes>0);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "downnodebutton"));
	gtk_widget_set_sensitive(w, shownodes>0);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "interruptnodebutton"));
	gtk_widget_set_sensitive(w, shownodes>0);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "bRerunNode"));
	gtk_widget_set_sensitive(w, shownodes>0);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "bRerunNode"));
	gtk_widget_set_sensitive(w, shownodes>0);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "bClearErrors"));
	gtk_widget_set_sensitive(w, shownodes>0);
	if(shownodes==1) {
		gtk_tree_selection_selected_foreach(sel
				,get_lastselected_node
				,&i);
		if((j=getnodedat(&cfg,i,&node,FALSE,NULL))) {
			sprintf(str,"Error reading node data (%d)!",j);
			node.status=NODE_WFC;
		}
		j=(node.status==NODE_QUIET || node.status==NODE_INUSE);

		w=GTK_WIDGET(gtk_builder_get_object (builder, "bChatWithUser"));
		gtk_widget_set_sensitive(w, j);
		w=GTK_WIDGET(gtk_builder_get_object (builder, "bSendMessageToUser"));
		gtk_widget_set_sensitive(w, j);
		w=GTK_WIDGET(gtk_builder_get_object (builder, "bEditUser"));
		gtk_widget_set_sensitive(w, j);
		w=GTK_WIDGET(gtk_builder_get_object (builder, "cNodeQuickValidate"));
//		gtk_widget_set_sensitive(w, j);
gtk_widget_set_sensitive(w, TRUE);
	}
	else {
		w=GTK_WIDGET(gtk_builder_get_object (builder, "bChatWithUser"));
		gtk_widget_set_sensitive(w, FALSE);
		w=GTK_WIDGET(gtk_builder_get_object (builder, "bSendMessageToUser"));
		gtk_widget_set_sensitive(w, FALSE);
		w=GTK_WIDGET(gtk_builder_get_object (builder, "bEditUser"));
		gtk_widget_set_sensitive(w, FALSE);
		w=GTK_WIDGET(gtk_builder_get_object (builder, "cNodeQuickValidate"));
//		gtk_widget_set_sensitive(w, FALSE);
gtk_widget_set_sensitive(w, TRUE);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eLogonsToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get logons today widget\n");
	else {
		if(shownodes)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.ltoday),getnumstr(str3,sstats.ltoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.ltoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eTimeToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get time today widget\n");
	else {
		if(shownodes)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.ttoday),getnumstr(str3,sstats.ttoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.ttoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eEmailToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get e-mail today widget\n");
	else {
		if(shownodes)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.etoday),getnumstr(str3,sstats.etoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.etoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eFeedbackToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get feedback today widget\n");
	else {
		if(shownodes)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.ftoday),getnumstr(str3,sstats.ftoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.ftoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eNewUsersToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get new users today widget\n");
	else {
		if(shownodes)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.nusers),getnumstr(str3,sstats.nusers));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.nusers));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "ePostsToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get posts today widget\n");
	else {
		if(shownodes)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.ptoday),getnumstr(str3,sstats.ptoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.ptoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eLogonsTotal"));
	if(w==NULL)
		fprintf(stderr,"Cannot get logons total widget\n");
	else {
		if(shownodes)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.logons),getnumstr(str3,sstats.logons));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.logons));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eTimeTotal"));
	if(w==NULL)
		fprintf(stderr,"Cannot get time total widget\n");
	else {
		if(shownodes)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.timeon),getnumstr(str3,sstats.timeon));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.timeon));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eEmailTotal"));
	if(w==NULL)
		fprintf(stderr,"Cannot get email total widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,getmail(&cfg,0,0,0)));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eFeedbackTotal"));
	if(w==NULL)
		fprintf(stderr,"Cannot get feedback total widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,getmail(&cfg,1,0,0)));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eUsersTotal"));
	if(w==NULL)
		fprintf(stderr,"Cannot get users total widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,total_users(&cfg)));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eFilesUploadedToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get files uploaded today widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,sstats.uls));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eBytesUploadedToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get bytes uploaded today widget\n");
	else {
		sprintf(str,"%s",getsizestr(str2,sstats.ulb,TRUE));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eFilesDownloadedToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get files downloaded today widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,sstats.dls));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=GTK_WIDGET(gtk_builder_get_object (builder, "eBytesDownloadedToday"));
	if(w==NULL)
		fprintf(stderr,"Cannot get bytes downloaded today widget\n");
	else {
		sprintf(str,"%s",getsizestr(str2,sstats.dlb,TRUE));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	/* Setup the stats updater */
	if(data != NULL)
		g_timeout_add(1000 /* Milliseconds */, refresh_data, data);

	return(0);
}

/* Initializes global stuff, loads first user etc */
int read_config(void)
{
	char	ctrl_dir[MAX_PATH+1];

	SAFECOPY(ctrl_dir, get_ctrl_dir());
	prep_dir("",ctrl_dir,sizeof(ctrl_dir));
	if(!isdir(ctrl_dir)) {
		display_message("Environment Error","SBBSCTRL does not point to a directory","gtk-dialog-error");
		return(-1);
	}
    memset(&cfg,0,sizeof(cfg));
    cfg.size=sizeof(cfg);
    SAFECOPY(cfg.ctrl_dir,ctrl_dir);

	/* Read the ctrl struct */
	refresh_events();

	/* Read the .ini file */
	read_ini();

	/* Passing any non-NULL argument is required to set up the timeout */
	if(refresh_data(refresh_data))
		return(-1);
	return(0);
}

int main(int argc, char *argv[]) {
	GtkWindow*	xml;

    gtk_init(&argc, &argv);

	builder = gtk_builder_new();
	gtk_builder_add_from_string(builder, builder_interface, -1, NULL);

    /* connect the signals in the interface */
	gtk_builder_connect_signals (builder, NULL);

	/* Get MainWindow and display it */
	xml = GTK_WINDOW (gtk_builder_get_object (builder, "MainWindow"));
	gtk_window_present(xml);

	/* Set up the global config stuff. */
	memset(&cfg, 0, sizeof(cfg));
	if(read_config())
		return(1);

    /* start the event loop */
    gtk_main();
    return 0;
}
