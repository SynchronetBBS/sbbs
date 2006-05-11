#include "events.h"
#include "gtkmonitor.h"
#include "util_funcs.h"

void update_stats_callback(GtkWidget *wiggy, gpointer data)
{
	refresh_data(NULL);
}

void on_guru_brain1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.ctrl_dir,"guru.dat");
}

void on_text_strings1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.ctrl_dir,"text.dat");
}

void on_default_colours1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.ctrl_dir,"attr.cfg");
}

void on_nodes_full_message1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.text_dir,"nonodes.txt");
}

void on_answer_screen1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.text_dir,"answer.asc");
}

void on_logon_message1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.text_dir,"menu/logon.asc");
}

void on_auto_message1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.text_dir,"menu/auto.msg");
}

void on_zip_file_comment1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.text_dir,"zipmsg.txt");
}

void on_system_information1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.text_dir,"system.msg");
}

void on_new_user_message1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.text_dir,"newuser.msg");
}

void on_new_user_welcome_email1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.text_dir,"welcome.msg");
}

void on_new_user_password_failure1_activate(GtkWidget *wiggy, gpointer data) {
	edit_text_file(cfg.text_dir,"nupguess.msg");
}

void on_new_user_feedbakc_instructions1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"feedback.msg");
}

void on_allowed_rlogin_list1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"rlogin.cfg");
}

void on_alias_list1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"alias.cfg");
}

void on_domain_list1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"domains.cfg");
}

void on_spam_bait_list1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"spambait.cfg");
}

void on_spam_block_list1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"spamblock.cfg");
}

void on_allowed_relay_list1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"relay.cfg");
}

void on_dnsbased_blacklists1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"dns_blacklist.cfg");
}

void on_dnsblacklist_exempt_ips1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"dnsbl_exempt.cfg.cfg");
}

void on_external_mail_processing1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"mailproc.ini");
}

void on_login_message1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"ftplogin.txt");
}

void on_failed_login_mesage1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"ftpbadlogin.txt");
}

void on_hello_message1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"ftphello.txt");
}

void on_goodbye_message1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"ftpbye.txt");
}

void on_filename_aliases1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"ftpalias.cfg");
}

void on_mime_types1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"mime_types.ini");
}

void on_cgi_environment_variables1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"cgi_env.ini");
}

void on_external_content_handlers1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"web_handler.ini");
}

void on_servicesini1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"services.ini");
}

void on_error_log1_activate(GtkWidget *wiggy, gpointer data)
{
	view_text_file(cfg.logs_dir,"error.log");
}

void on_statistics_log1_activate(GtkWidget *wiggy, gpointer data)
{
	view_stdout(cfg.exec_dir,"slog");
}

void on_todays_log1_activate(GtkWidget *wiggy, gpointer data)
{
	time_t	t;
	struct tm *tm;
	char	fn[20];

	t=time(NULL);
	tm=localtime(&t);
	sprintf(fn,"logs/%02d%02d%02d.log",tm->tm_mon+1,tm->tm_mday,tm->tm_year%100);
	view_text_file(cfg.logs_dir,fn);
}

void on_yesterdays_log1_activate(GtkWidget *wiggy, gpointer data)
{
	time_t	t;
	struct tm *tm;
	char	fn[20];

	t=time(NULL);
	t-=24*60*60;
	tm=localtime(&t);
	sprintf(fn,"logs/%02d%02d%02d.log",tm->tm_mon+1,tm->tm_mday,tm->tm_year%100);
	view_text_file(cfg.logs_dir,fn);
}

void on_another_days_log1_activate(GtkWidget *wiggy, gpointer data) {
	/* ToDo */
}

void on_spam_log1_activate(GtkWidget *wiggy, gpointer data)
{
	view_text_file(cfg.logs_dir,"spam.log");
}

void on_ip_address_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"ip.can");
}

void on_ip_address_filter_silent1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"ip-silent.can");
}

void on_ip_address_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"badip.msg");
}

void on_host_name_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"host.can");
}

void on_host_name_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"badhost.msg");
}

void on_user_name_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"name.can");
}

void on_user_name_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"badname.msg");
}

void on_email_address_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"email.can");
}

void on_email_address_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"bademail.msg");
}

void on_email_subject_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"subject.can");
}

void on_file_name_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"file.can");
}

void on_file_name_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"badfile.msg");
}

void on_phone_number_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"phone.can");
}

void on_phone_number_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.text_dir,"badphone.msg");
}

void on_twit_list1_activate(GtkWidget *wiggy, gpointer data)
{
	edit_text_file(cfg.ctrl_dir,"twitlist.cfg");
}

void on_hack_attempt_log1_activate(GtkWidget *wiggy, gpointer data)
{
	view_text_file(cfg.logs_dir,"hack.log");
}

void on_configure1_activate(GtkWidget *wiggy, gpointer data)
{
	run_external(cfg.exec_dir,"scfg");
}

void on_edit3_activate(GtkWidget *wiggy, gpointer data)
{
	run_external(cfg.exec_dir,"gtkuseredit");
}

void on_truncate_deleted_users1_activate(GtkWidget *wiggy, gpointer data)
{
    int usernumber;
    int deleted=0;
    user_t user;
    char str[128];

    while((user.number=lastuser(&cfg))!=0) {
        if(getuserdat(&cfg,&user)!=0)
            break;
        if(!(user.misc&DELETED))
            break;
        if(!del_lastuser(&cfg))
            break;
        deleted++;
    }
    sprintf(str,"%u Deleted User Records Removed",deleted);
	display_message("Users Truncated", str);
}

void on_stop6_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown");
}

void on_recycle6_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle");
}

void on_stop1_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.telnet");
}

void on_recycle5_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.telnet");
}

void on_stop2_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.mail");
}

void on_recycle1_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.mail");
}

void on_stop3_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.ftp");
}

void on_recycle2_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.ftp");
}

void on_stop4_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.web");
}

void on_recycle3_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.web");
}

void on_stop5_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.services");
}

void on_recycle4_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.services");
}

void on_statistics_pane1_activate(GtkWidget *wiggy, gpointer data)
{
	GtkWidget *w;

	w=glade_xml_get_widget(xml, "StatisticsPane");
	if(w==NULL)
		fprintf(stderr,"Cannot get the statistics pane.\n");
	else {
		switch(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(wiggy))) {
			case 0:
				gtk_widget_hide(w);
				break;
			default:
				gtk_widget_show(w);
		}
	}
}

void create_force_sem(GtkWidget *wiggy, gpointer data)
{
	gchar	*label;
	char	fn[MAX_PATH+1];

	label=(gchar *)gtk_label_get_text(GTK_LABEL(wiggy));
	if(label != NULL) {
		sprintf(fn,"%s%s.now",(gchar *)data, label);
		touch_sem(cfg.data_dir, fn);
	}
}

void on_force_event(GtkWidget *wiggy, gpointer data)
{
	/* There's only one child... so this is a bit of a cheat */
	gtk_container_foreach(GTK_CONTAINER(wiggy), create_force_sem, "");
}

void on_force_qnet(GtkWidget *wiggy, gpointer data)
{
	/* There's only one child... so this is a bit of a cheat */
	gtk_container_foreach(GTK_CONTAINER(wiggy), create_force_sem, "qnet/");
}

void on_reload_configuration1_activate(GtkWidget *wiggy, gpointer data)
{
	refresh_events();
}

void toggle_node_bits(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	*bit=data;
	int	fd;
	char	*node_str;
	int		n,i;
	node_t	node;

	gtk_tree_model_get(model, iter, 0, &node_str, -1);
	n=atoi(node_str);

	if((i=getnodedat(&cfg,n,&node,&fd)))
		fprintf(stderr,"Error reading node %d data (%d)!",n,i);
	else {
		node.misc ^= *bit;
		putnodedat(&cfg, n, &node, fd);
	}
}

void lock_nodes(GtkWidget *wiggy, gpointer data)
{
	int		bit=NODE_LOCK;

	gtk_tree_selection_selected_foreach(sel
			,toggle_node_bits
			,&bit);
	refresh_data(NULL);
}

void down_nodes(GtkWidget *wiggy, gpointer data)
{
	int		bit=NODE_DOWN;

	gtk_tree_selection_selected_foreach(sel
			,toggle_node_bits
			,&bit);
	refresh_data(NULL);
}

void interrupt_nodes(GtkWidget *wiggy, gpointer data)
{
	int		bit=NODE_INTR;

	gtk_tree_selection_selected_foreach(sel
			,toggle_node_bits
			,&bit);
	refresh_data(NULL);
}

void rerun_nodes(GtkWidget *wiggy, gpointer data)
{
	int		bit=NODE_RRUN;

	gtk_tree_selection_selected_foreach(sel
			,toggle_node_bits
			,&bit);
	refresh_data(NULL);
}

void do_clear_errors(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	*bit=data;
	int	fd;
	char	*node_str;
	int		n,i;
	node_t	node;

	gtk_tree_model_get(model, iter, 0, &node_str, -1);
	n=atoi(node_str);

	if((i=getnodedat(&cfg,n,&node,&fd)))
		fprintf(stderr,"Error reading node %d data (%d)!",n,i);
	else {
		node.errors = 0;
		putnodedat(&cfg, n, &node, fd);
	}
}

void clear_errors(GtkWidget *wiggy, gpointer data)
{
	gtk_tree_selection_selected_foreach(sel
			,do_clear_errors
			,NULL);
	refresh_data(NULL);
}

void on_about1_activate(GtkWidget *wiggy, gpointer data)
{
	GladeXML	*axml;
    axml = glade_xml_new("gtkmonitor.glade", "AboutWindow", NULL);
	if(axml==NULL) {
		fprintf(stderr,"Could not locate AboutWindow widget\n");
		return;
	}
    /* connect the signals in the interface */
    glade_xml_signal_autoconnect(axml);
	gtk_window_present(GTK_WINDOW(glade_xml_get_widget(axml, "AboutWindow")));
}

void get_lastselected_node(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	*i=data;
	gchar	*node;

	gtk_tree_model_get(model, iter, 0, &node, -1);
	*i=atoi(node);
}

void chatwith_node(GtkWidget *wiggy, gpointer data)
{
	char	str[MAX_PATH+1];
	int		i;

	gtk_tree_selection_selected_foreach(sel
			,get_lastselected_node
			,&i);
	sprintf(str,"gtkchat %d",i);
	run_external(cfg.exec_dir,str);
}

void edituseron_node(GtkWidget *wiggy, gpointer data)
{
	char	str[MAX_PATH+1];
	int		i;
	node_t	node;

	gtk_tree_selection_selected_foreach(sel
			,get_lastselected_node
			,&i);

	if((i=getnodedat(&cfg,i,&node,NULL)))
		fprintf(stderr,"Error reading node data (%d)!",i);
	else {
		sprintf(str,"gtkuseredit %d",node.useron);
		run_external(cfg.exec_dir,str);
	}
}

void close_this_window(GtkWidget *wiggy, gpointer data)
{
	gtk_widget_destroy(GTK_WIDGET(gtk_widget_get_toplevel(wiggy)));
}

GladeXML		*lxml;

void update_userlist_sensitive_callback(GtkTreeSelection *wiggy, gpointer data)
{
	GtkWidget	*w;
	int selected;

	w=glade_xml_get_widget(lxml, "bUserListEditUser");
	selected=gtk_tree_selection_count_selected_rows(wiggy);
	gtk_widget_set_sensitive(w, selected==1);
}

void update_userlist_item(GtkListStore *lstore, GtkTreeIter *curr, int usernum)
{
	char			sex[2];
	char			first[9];
	char			last[9];
	user_t			user;

	user.number=usernum;
	getuserdat(&cfg, &user);
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

void update_userlist_callback(GtkWidget *wiggy, gpointer data)
{
	GtkWidget		*w;
	GtkListStore	*lstore = NULL;
	int				totalusers;
	int				i;
	GtkTreeIter		curr;

	w=glade_xml_get_widget(lxml, "lUserList");
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
		display_message("Load Error",str);
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
		display_message("Save Error",str);
	}
}

void userlist_do_quick_validate(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	*set=data;
	int	usernum;

	gtk_tree_model_get(model, iter, 0, &usernum, -1);
	quick_validate(usernum,*set);
	update_userlist_item(GTK_LIST_STORE(model), iter, usernum);
}

void on_userlist_quick_validate(GtkWidget *wiggy, gpointer data)
{
	int		set;
	GtkWidget	*w;

	w=glade_xml_get_widget(lxml, "lUserList");
	set=gtk_combo_box_get_active(GTK_COMBO_BOX(wiggy))-1;
	if(set>=0) {
		gtk_tree_selection_selected_foreach(gtk_tree_view_get_selection (GTK_TREE_VIEW (w))
				,userlist_do_quick_validate
				,&set);
		gtk_combo_box_set_active(GTK_COMBO_BOX(wiggy), 0);
	}
}

/* Show user list */
on_list1_activate(GtkWidget *wiggy, gpointer data)
{
	GtkWidget		*w;
	int				i;
	char			str[1025];
	char			flags[33];
	GtkListStore	*lstore = NULL;
	GtkTreeSelection *lsel;

    lxml = glade_xml_new("gtkmonitor.glade", "UserListWindow", NULL);
	if(lxml==NULL) {
		fprintf(stderr,"Could not locate UserListWindow widget\n");
		return;
	}
    /* connect the signals in the interface */
    glade_xml_signal_autoconnect(lxml);

	/* Set up user list */
	w=glade_xml_get_widget(lxml, "lUserList");
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
	w=glade_xml_get_widget(lxml, "cQuickValidate");
	gtk_combo_box_set_active(GTK_COMBO_BOX(w), 0);
	for(i=0;i<10;i++) {
		sprintf(str,"%d  SL: %-2d  F1: %s",i,cfg.val_level[i],ltoaf(cfg.val_flags1[i],flags));
		gtk_combo_box_append_text(GTK_COMBO_BOX(w), str);
	}

	/* Show 'er to the user */
	gtk_window_present(GTK_WINDOW(glade_xml_get_widget(lxml, "UserListWindow")));
}

void get_lastselected_user(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	*i=data;
	int	user;

	gtk_tree_model_get(model, iter, 0, &user, -1);
	*i=user;
}

void userlist_edituser(GtkWidget *wiggy, gpointer data)
{
	char	str[MAX_PATH+1];
	int		i;
	GtkWidget	*w;

	w=glade_xml_get_widget(lxml, "lUserList");
	gtk_tree_selection_selected_foreach(gtk_tree_view_get_selection (GTK_TREE_VIEW (w))
			,get_lastselected_user
			,&i);

	sprintf(str,"gtkuseredit %d",i);
	run_external(cfg.exec_dir,str);
}

void quickvalidate_useron_node(GtkWidget *wiggy, gpointer data)
{
	char	str[MAX_PATH+1];
	int		i;
	int		set;
	node_t	node;
	GtkWidget	*w;

	set=gtk_combo_box_get_active(GTK_COMBO_BOX(wiggy))-1;
	if(set>=0) {
		gtk_tree_selection_selected_foreach(sel
				,get_lastselected_node
				,&i);

		if((i=getnodedat(&cfg,i,&node,NULL)))
			fprintf(stderr,"Error reading node data (%d)!",i);
		else {
			quick_validate(node.useron, set);
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(wiggy), 0);
	}
}
