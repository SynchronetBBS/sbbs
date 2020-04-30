#include "dirwrap.h"
#include "ini_file.h"

#include "events.h"
#include "gtkuserlist.h"

scfg_t		cfg;
uchar		*arbuf=NULL;
GtkBuilder*	builder;
extern const char builder_interface[];

int main(int argc, char **argv)
{
	GtkWidget		*w;
	int				i;
	char			str[1025];
	char			flags[33];
	GtkListStore	*lstore = NULL;
	GtkTreeSelection *lsel;
	char	*p;
	GtkListStore	*quickstore = NULL;
	GtkTreeIter		curr;
	GtkCellRenderer *column;

    gtk_init(&argc, &argv);

    /* Read .cfg files here */
    memset(&cfg,0,sizeof(cfg));
	p=getenv("SBBSCTRL");
	if(p==NULL) {
		display_message("Environment Error","SBBSCTRL not set","gtk-dialog-error");
		return(-1);
	}
	SAFECOPY(cfg.ctrl_dir, p);
	prep_dir("",cfg.ctrl_dir,sizeof(cfg.ctrl_dir));
	if(!isdir(cfg.ctrl_dir)) {
		display_message("Environment Error","SBBSCTRL does not point to a directory","gtk-dialog-error");
		return(-1);
	}
    cfg.size=sizeof(cfg);

    if(!load_cfg(&cfg, NULL, TRUE, str)) {
		fprintf(stderr,"Cannot load configuration data (%s)",str);
        return(-1);
	}

	builder = gtk_builder_new ();
	gtk_builder_add_from_string(builder, builder_interface, -1, NULL);

    /* connect the signals in the interface */
	gtk_builder_connect_signals (builder, NULL);

	/* Set up user list */
	w=GTK_WIDGET(gtk_builder_get_object(builder, "lUserList"));
	lstore = gtk_list_store_new(17
			,G_TYPE_INT
			,G_TYPE_STRING
			,G_TYPE_STRING
			,G_TYPE_INT
			,G_TYPE_INT
			,G_TYPE_STRING
			,G_TYPE_STRING
			,G_TYPE_STRING
			,G_TYPE_STRING
			,G_TYPE_STRING
			,G_TYPE_STRING
			,G_TYPE_STRING
			,G_TYPE_INT
			,G_TYPE_STRING
			,G_TYPE_STRING
			,G_TYPE_INT
			,G_TYPE_INT
	);
	gtk_tree_view_set_model(GTK_TREE_VIEW(w), GTK_TREE_MODEL(lstore));
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,0
			,"Num"
			,gtk_cell_renderer_text_new()
			,"text"
			,0
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 0
			)
			,0
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,1
			,"Alias"
			,gtk_cell_renderer_text_new()
			,"text"
			,1
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 1
			)
			,1
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,2
			,"Name"
			,gtk_cell_renderer_text_new()
			,"text"
			,2
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 2
			)
			,2
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,3
			,"Lev"
			,gtk_cell_renderer_text_new()
			,"text"
			,3
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 3
			)
			,3
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,4
			,"Age"
			,gtk_cell_renderer_text_new()
			,"text"
			,4
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 4
			)
			,4
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,5
			,"Sex"
			,gtk_cell_renderer_text_new()
			,"text"
			,5
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 5
			)
			,5
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,6
			,"Location"
			,gtk_cell_renderer_text_new()
			,"text"
			,6
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 6
			)
			,6
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,7
			,"Protocol"
			,gtk_cell_renderer_text_new()
			,"text"
			,7
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 7
			)
			,7
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,8
			,"Address"
			,gtk_cell_renderer_text_new()
			,"text"
			,8
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 8
			)
			,8
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,9
			,"Host Name"
			,gtk_cell_renderer_text_new()
			,"text"
			,9
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 9
			)
			,9
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,10
			,"Phone"
			,gtk_cell_renderer_text_new()
			,"text"
			,10
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 10
			)
			,10
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,11
			,"Email"
			,gtk_cell_renderer_text_new()
			,"text"
			,11
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 11
			)
			,11
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,12
			,"Logons"
			,gtk_cell_renderer_text_new()
			,"text"
			,12
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 12
			)
			,12
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,13
			,"First On"
			,gtk_cell_renderer_text_new()
			,"text"
			,13
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 13
			)
			,15
	);
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW(w)
			,14
			,"Last On"
			,gtk_cell_renderer_text_new()
			,"text"
			,14
			,NULL);
	gtk_tree_view_column_set_sort_column_id(
			gtk_tree_view_get_column(
					GTK_TREE_VIEW(w), 14
			)
			,16
	);
	lsel = gtk_tree_view_get_selection (GTK_TREE_VIEW (w));
	gtk_tree_selection_set_mode (lsel, GTK_SELECTION_MULTIPLE);
	g_signal_connect (G_OBJECT (lsel), "changed", G_CALLBACK (update_userlist_sensitive_callback), NULL);

	/* Set up users */
	update_userlist_callback(GTK_WIDGET(w), NULL);
	update_userlist_sensitive_callback(lsel, NULL);

	/* Set up quick validation values */
	w=GTK_WIDGET(gtk_builder_get_object(builder, "cQuickValidate"));
	/* Fist call... set up grid */
	if(quickstore == NULL) {
		quickstore = gtk_list_store_new(1, G_TYPE_STRING);
		gtk_combo_box_set_model(GTK_COMBO_BOX(w), GTK_TREE_MODEL(quickstore));
		gtk_list_store_insert(quickstore, &curr, 0);
		gtk_list_store_set(quickstore, &curr, 0, "Quick Validation Sets", -1);
		column = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(w), column, TRUE);
		gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(w), column,
				"text", 0,
				NULL
		);
		gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);
	}

	gtk_list_store_move_after(quickstore, &curr, 0);
	for(i=1; i<=10; i++) {
		gtk_list_store_append(quickstore, &curr);
		sprintf(str,"%-2d  SL: %-2d  F1: %s",i,cfg.val_level[i],ltoaf(cfg.val_flags1[i],flags));
		gtk_list_store_set(quickstore, &curr, 0, str, -1);
		gtk_tree_model_iter_next(GTK_TREE_MODEL(quickstore), &curr);
	}

	/* Show 'er to the user */
	gtk_window_present(GTK_WINDOW(gtk_builder_get_object(builder, "UserListWindow")));
	gtk_main();
	return 0;
}
