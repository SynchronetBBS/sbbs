#include <gtk/gtk.h>
#include <glade/glade.h>

#include "sbbs.h"
#include "dirwrap.h"

scfg_t		cfg;
GladeXML	*xml;
int			nodes=0;
GtkListStore	*store = NULL;
GtkTreeSelection *sel;

/* Refreshes global variables... ie: Number of users */
int refresh_data(gpointer data)
{
	char		str[1024];
	char		str2[1024];
	char		str3[1024];
	char		*p;
	GtkWidget	*w;
	int			i,j;
	GtkTreeIter	curr;
	GtkTreeModel	*model;
	node_t	node;
	stats_t	sstats;
	stats_t	nstats;
	int		shownode;

    /* Read .cfg files here */
    if(!load_cfg(&cfg, NULL, TRUE, str)) {
		fprintf(stderr,"Cannot load configuration data\n");
        return(-1);
	}

	/* Update the node list stuff */
	w=glade_xml_get_widget(xml, "lNodeList");

	/* Fist call... set up grid */
	if(store == NULL) {
		store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
		gtk_tree_view_set_model(GTK_TREE_VIEW(w), GTK_TREE_MODEL(store));
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(w), 0, "Node", gtk_cell_renderer_text_new(), "text", 0, NULL);
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(w), 1, "Status", gtk_cell_renderer_text_new(), "text", 1, NULL);
		nodes=0;
		sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (w));
		gtk_tree_selection_set_mode (sel, GTK_SELECTION_SINGLE);
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

		if((j=getnodedat(&cfg,i,&node,NULL)))
			sprintf(str,"Error reading node data (%d)!",j);
		else
			nodestatus(&cfg,&node,str,1023);
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
	if(gtk_tree_selection_get_selected(sel, &model, &curr)) {
		gtk_tree_model_get(model, &curr, 0, &p, -1);
		shownode=atoi(p);
		getstats(&cfg, shownode, &nstats);
	}
	else
		shownode=0;

	w=glade_xml_get_widget(xml, "eLogonsToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get logons today widget\n");
	else {
		if(shownode)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.ltoday),getnumstr(str3,sstats.ltoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.ltoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eTimeToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get time today widget\n");
	else {
		if(shownode)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.ttoday),getnumstr(str3,sstats.ttoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.ttoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eEmailToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get e-mail today widget\n");
	else {
		if(shownode)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.etoday),getnumstr(str3,sstats.etoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.etoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eFeedbackToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get feedback today widget\n");
	else {
		if(shownode)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.ftoday),getnumstr(str3,sstats.ftoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.ftoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eNewUsersToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get new users today widget\n");
	else {
		if(shownode)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.nusers),getnumstr(str3,sstats.nusers));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.nusers));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "ePostsToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get posts today widget\n");
	else {
		if(shownode)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.ptoday),getnumstr(str3,sstats.ptoday));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.ptoday));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eLogonsTotal");
	if(w==NULL)
		fprintf(stderr,"Cannot get logons total widget\n");
	else {
		if(shownode)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.logons),getnumstr(str3,sstats.logons));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.logons));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eTimeTotal");
	if(w==NULL)
		fprintf(stderr,"Cannot get time total widget\n");
	else {
		if(shownode)
			sprintf(str,"%s/%s",getnumstr(str2,nstats.timeon),getnumstr(str3,sstats.timeon));
		else
			sprintf(str,"%s",getnumstr(str2,sstats.timeon));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eEmailTotal");
	if(w==NULL)
		fprintf(stderr,"Cannot get email total widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,getmail(&cfg,0,0)));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eFeedbackTotal");
	if(w==NULL)
		fprintf(stderr,"Cannot get feedback total widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,getmail(&cfg,1,0)));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eUsersTotal");
	if(w==NULL)
		fprintf(stderr,"Cannot get users total widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,total_users(&cfg)));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eFilesUploadedToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get files uploaded today widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,sstats.uls));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eBytesUploadedToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get bytes uploaded today widget\n");
	else {
		sprintf(str,"%s",getsizestr(str2,sstats.ulb,TRUE));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eFilesDownloadedToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get files downloaded today widget\n");
	else {
		sprintf(str,"%s",getnumstr(str2,sstats.dls));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	w=glade_xml_get_widget(xml, "eBytesDownloadedToday");
	if(w==NULL)
		fprintf(stderr,"Cannot get bytes downloaded today widget\n");
	else {
		sprintf(str,"%s",getsizestr(str2,sstats.dlb,TRUE));
		gtk_entry_set_text(GTK_ENTRY(w),str);
	}

	/* Setup the stats updater */
	g_timeout_add(1000 /* Milliseconds */, refresh_data, NULL);

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

	if(refresh_data(NULL))
		return(-1);
	return(0);
}

int main(int argc, char *argv[]) {
	char			glade_path[MAX_PATH+1];
	GError			*error = NULL;
	GtkIconTheme	*icon_theme;
	GdkPixbuf		*pixbuf;

    gtk_init(&argc, &argv);
    glade_init();

    /* load the interface */
	strcpy(glade_path, argv[0]);
	strcpy(getfname(glade_path), "gtkmonitor.glade");
    xml = glade_xml_new(glade_path, "MainWindow", NULL);

    /* connect the signals in the interface */
    glade_xml_signal_autoconnect(xml);

	/* Set up the global config stuff. */
	if(read_config())
		return(1);

    /* start the event loop */
    gtk_main();
    return 0;
}
