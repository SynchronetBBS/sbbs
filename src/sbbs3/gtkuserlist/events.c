#include "events.h"
#include "sbbsdefs.h"
#include "gtkuserlist.h"

char *complete_path(char *dest, char *path, char *filename)
{
	if(path != NULL) {
		strcpy(dest, path);
		backslash(dest);
	}
	else
		dest[0]=0;

	if(filename != NULL)
		strcat(dest, filename);
	fexistcase(dest);			/* TODO: Hack: Fixes upr/lwr case fname */
	return(dest);
}

void exec_cmdline(void *cmdline)
{
	GtkWidget	*w;

	if(cmdline==NULL)
		return;
	system((char *)cmdline);
	free(cmdline);
	w=GTK_WIDGET(gtk_builder_get_object(builder, "UserListWindow"));
	gtk_widget_set_sensitive(GTK_WIDGET(w), TRUE);
}

void run_external(char *path, char *filename)
{
	static char	cmdline[MAX_PATH*2];
	GtkWidget	*w;

	w=GTK_WIDGET(gtk_builder_get_object(builder, "UserListWindow"));
	gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
	complete_path(cmdline,path,filename);
	_beginthread(exec_cmdline, 0, strdup(cmdline));
}

void display_message(char *title, char *message, char *icon)
{
	GtkWidget *dialog, *label;

	dialog=gtk_dialog_new_with_buttons(title
			,GTK_WINDOW(gtk_builder_get_object(builder, "UserListWindow"))
			,GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT
			,"_OK"
			,GTK_RESPONSE_NONE
			,NULL);
	if(icon==NULL)
		icon="gtk-info";
	gtk_window_set_icon_name(GTK_WINDOW(dialog), icon);
	label = gtk_label_new (message);

	g_signal_connect_swapped (dialog
			,"response"
			,G_CALLBACK(gtk_widget_destroy)
            ,dialog);
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)),
			label);
	gtk_widget_show_all (dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
}

G_MODULE_EXPORT void update_userlist_sensitive_callback(GtkTreeSelection *wiggy, gpointer data)
{
	GtkWidget	*w;
	int selected;

	w=GTK_WIDGET(gtk_builder_get_object(builder, "bUserListEditUser"));
	selected=gtk_tree_selection_count_selected_rows(wiggy);
	gtk_widget_set_sensitive(w, selected==1);
}

G_MODULE_EXPORT void update_userlist_item(GtkListStore *lstore, GtkTreeIter *curr, int usernum)
{
	char			sex[2];
	char			first[9];
	char			last[9];
	user_t			user;

	user.number=usernum;
	getuserdat(&cfg, &user);
	if(arbuf) {
		if(!chk_ar(&cfg, arbuf, &user, /* client: */NULL)) {
			gtk_list_store_remove(lstore, curr);
			return;
		}
	}
	sex[0]=user.sex;
	sex[1]=0;
	unixtodstr(&cfg, user.firston, first);
	unixtodstr(&cfg, user.laston, last);
	gtk_list_store_set(lstore, curr
		,0,user.number
		,1,user.alias
		,2,user.name
		,3,user.level
		,4,getage(&cfg, user.birth)
		,5,sex
		,6,user.location
		,7,user.modem
		,8,user.note
		,9,user.comp
		,10,user.phone
		,11,user.netmail
		,12,user.logons
		,13,first
		,14,last
		,15,user.firston
		,16,user.laston
		,-1);
}

G_MODULE_EXPORT void update_userlist_callback(GtkWidget *wiggy, gpointer data)
{
	GtkWidget		*w;
	GtkListStore	*lstore = NULL;
	int				totalusers;
	int				i;
	GtkTreeIter		curr;
	char			str[1024];

	free_cfg(&cfg);
	if(!load_cfg(&cfg, NULL, TRUE, str)) {
		char error[256];
		SAFEPRINTF(error, "ERROR Loading Configuration Data: %s", str);
		display_message("Load Error",error,"gtk-dialog-error");
		return;
    }

	w=GTK_WIDGET(gtk_builder_get_object(builder, "lUserList"));
	lstore=GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(w)));
	gtk_list_store_clear(lstore);
	totalusers=lastuser(&cfg);
	for(i=1; i<=totalusers; i++) {
		gtk_list_store_insert(lstore, &curr, i-1);
		update_userlist_item(lstore, &curr, i);
	}
}

void quick_validate(int usernum, int set)
{
	user_t		user;
	int			res;
	char		str[1024];

	user.number=usernum;
	if((res=getuserdat(&cfg,&user))) {
		sprintf(str,"Error loading user %d.\n",usernum);
		display_message("Load Error",str,"gtk-dialog-error");
		return;
	}
	user.flags1=cfg.val_flags1[set];
	user.flags2=cfg.val_flags2[set];
	user.flags3=cfg.val_flags3[set];
	user.flags4=cfg.val_flags4[set];
	user.exempt=cfg.val_exempt[set];
	user.rest=cfg.val_rest[set];
	if(cfg.val_expire[set]) {
		user.expire=time(NULL)
			+(cfg.val_expire[set]*24*60*60);
	}
	else
		user.expire=0;
	user.level=cfg.val_level[set];
	if((res=putuserdat(&cfg,&user))) {
		sprintf(str,"Error saving user %d.\n",usernum);
		display_message("Save Error",str,"gtk=dialog-error");
	}
}

G_MODULE_EXPORT void userlist_do_quick_validate(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	*set=data;
	int	usernum;

	gtk_tree_model_get(model, iter, 0, &usernum, -1);
	quick_validate(usernum,*set);
	update_userlist_item(GTK_LIST_STORE(model), iter, usernum);
}

G_MODULE_EXPORT void on_userlist_quick_validate(GtkWidget *wiggy, gpointer data)
{
	int		set;
	GtkWidget	*w;

	w=GTK_WIDGET(gtk_builder_get_object(builder, "lUserList"));
	set=gtk_combo_box_get_active(GTK_COMBO_BOX(wiggy))-1;
	if(set>=0) {
		gtk_tree_selection_selected_foreach(gtk_tree_view_get_selection (GTK_TREE_VIEW (w))
				,userlist_do_quick_validate
				,&set);
		gtk_combo_box_set_active(GTK_COMBO_BOX(wiggy), 0);
	}
}

G_MODULE_EXPORT void get_lastselected_user(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	*i=data;
	int	user;

	gtk_tree_model_get(model, iter, 0, &user, -1);
	*i=user;
}

G_MODULE_EXPORT void userlist_edituser(GtkWidget *wiggy, gpointer data)
{
	char	str[MAX_PATH+1];
	int		i;
	GtkWidget	*w;

	w=GTK_WIDGET(gtk_builder_get_object(builder, "lUserList"));
	gtk_tree_selection_selected_foreach(gtk_tree_view_get_selection (GTK_TREE_VIEW (w))
			,get_lastselected_user
			,&i);

	sprintf(str,"gtkuseredit %d",i);
	run_external(cfg.exec_dir,str);
}

G_MODULE_EXPORT void apply_ars_filter(GtkWidget *wiggy, gpointer data)
{
	GtkWidget	*w;

	w=GTK_WIDGET(gtk_builder_get_object(builder, "eArsFilter"));
	arbuf=arstr(NULL, (char *)gtk_entry_get_text(GTK_ENTRY(w)), &cfg);
	update_userlist_callback(wiggy, data);
}

G_MODULE_EXPORT void clear_ars_filter(GtkWidget *wiggy, gpointer data)
{
	GtkWidget	*w;

	w=GTK_WIDGET(gtk_builder_get_object(builder, "eArsFilter"));
	gtk_entry_set_text(GTK_ENTRY(w),"");
	arbuf=NULL;
	update_userlist_callback(wiggy, data);
}
