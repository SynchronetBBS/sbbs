#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <string.h>
#include <utime.h>
#include <unistd.h>

#include "gtkmonitor.h"
#include "util_funcs.h"
#include "datewrap.h"

int got_date=0;

G_MODULE_EXPORT void destroy_calendar_window(GtkWidget *t, gpointer data)
{
	if(!got_date)
		gtk_main_quit();
	gtk_widget_hide_on_delete(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(t))));
}

G_MODULE_EXPORT void changed_day(GtkWidget *t, gpointer data)
{
	got_date=1;
	gtk_main_quit();
}

int get_date(GtkWidget *t, isoDate_t *date)
{
	GtkWidget	*w;
	GtkWindow	*win;
	GtkWidget	*thiswin;
	gint		x,x_off;
	gint		y,y_off;
	guint		year;
	guint		month;
	guint		day;
	isoDate_t	odate=*date;

	got_date=0;
	win=GTK_WINDOW(gtk_builder_get_object (builder, "CalendarWindow"));
	if(win==NULL) {
		fprintf(stderr,"Could not locate Calendar window\n");
		return(-1);
	}

	thiswin = gtk_widget_get_toplevel(t);
	if(thiswin==NULL) {
		fprintf(stderr,"Could not locate main window\n");
		return(-1);
	}
	if(!(gtk_widget_translate_coordinates(GTK_WIDGET(t)
			,GTK_WIDGET(thiswin), 0, 0, &x_off, &y_off))) {
		fprintf(stderr,"Could not get position of button in window");
	}
	gtk_window_get_position(GTK_WINDOW(thiswin), &x, &y);

	gtk_window_move(GTK_WINDOW(win), x+x_off, y+y_off);

	w=GTK_WIDGET(gtk_builder_get_object (builder, "Calendar"));
	if(w==NULL) {
		fprintf(stderr,"Could not locate Calendar widget\n");
		return(-1);
	}
	gtk_calendar_select_month(GTK_CALENDAR(w), isoDate_month(*date)-1, isoDate_year(*date));
	gtk_calendar_select_day(GTK_CALENDAR(w), isoDate_day(*date));
	gtk_window_present(GTK_WINDOW(win));
	/* Wait for window to close... */
	gtk_main();
	w=GTK_WIDGET(gtk_builder_get_object (builder, "Calendar"));
	if(w==NULL)
		return(-1);
	gtk_calendar_get_date(GTK_CALENDAR(w), &year, &month, &day);
	gtk_widget_hide_on_delete(GTK_WIDGET(gtk_widget_get_toplevel(GTK_WIDGET(w))));
	*date=isoDate_create(year, month+1, day);
	return(odate!=*date);
}

G_MODULE_EXPORT void update_stats_callback(GtkWidget *wiggy, gpointer data)
{
	refresh_data(NULL);
}

G_MODULE_EXPORT void on_guru_brain1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"guru.dat");
}

G_MODULE_EXPORT void on_text_strings1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"text.dat");
}

G_MODULE_EXPORT void on_default_colours1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"attr.cfg");
}

G_MODULE_EXPORT void on_nodes_full_message1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"nonodes.txt");
}

G_MODULE_EXPORT void on_answer_screen1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"answer.asc");
}

G_MODULE_EXPORT void on_logon_message1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"menu/logon.asc");
}

G_MODULE_EXPORT void on_auto_message1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"menu/auto.msg");
}

G_MODULE_EXPORT void on_zip_file_comment1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"zipmsg.txt");
}

G_MODULE_EXPORT void on_system_information1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"system.msg");
}

G_MODULE_EXPORT void on_new_user_message1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"newuser.msg");
}

G_MODULE_EXPORT void on_new_user_welcome_email1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"welcome.msg");
}

G_MODULE_EXPORT void on_new_user_password_failure1_activate(GtkWidget *wiggy, gpointer data) {
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"nupguess.msg");
}

G_MODULE_EXPORT void on_new_user_feedbakc_instructions1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"feedback.msg");
}

G_MODULE_EXPORT void on_allowed_rlogin_list1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"rlogin.cfg");
}

G_MODULE_EXPORT void on_alias_list1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"alias.cfg");
}

G_MODULE_EXPORT void on_domain_list1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"domains.cfg");
}

G_MODULE_EXPORT void on_spam_bait_list1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"spambait.cfg");
}

G_MODULE_EXPORT void on_spam_block_list1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"spamblock.cfg");
}

G_MODULE_EXPORT void on_allowed_relay_list1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"relay.cfg");
}

G_MODULE_EXPORT void on_dnsbased_blacklists1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"dns_blacklist.cfg");
}

G_MODULE_EXPORT void on_dnsblacklist_exempt_ips1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"dnsbl_exempt.cfg.cfg");
}

G_MODULE_EXPORT void on_external_mail_processing1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"mailproc.ini");
}

G_MODULE_EXPORT void on_login_message1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"ftplogin.txt");
}

G_MODULE_EXPORT void on_failed_login_mesage1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"ftpbadlogin.txt");
}

G_MODULE_EXPORT void on_hello_message1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"ftphello.txt");
}

G_MODULE_EXPORT void on_goodbye_message1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"ftpbye.txt");
}

G_MODULE_EXPORT void on_filename_aliases1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"ftpalias.cfg");
}

G_MODULE_EXPORT void on_mime_types1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"mime_types.ini");
}

G_MODULE_EXPORT void on_cgi_environment_variables1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"cgi_env.ini");
}

G_MODULE_EXPORT void on_external_content_handlers1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"web_handler.ini");
}

G_MODULE_EXPORT void on_servicesini1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"services.ini");
}

G_MODULE_EXPORT void on_error_log1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.view_text_file ,cfg.logs_dir,"error.log");
}

G_MODULE_EXPORT void on_statistics_log1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.view_stdout ,cfg.exec_dir,"slog");
}

G_MODULE_EXPORT void on_todays_log1_activate(GtkWidget *wiggy, gpointer data)
{
	time_t	t;
	struct tm *tm;
	char	fn[20];

	t=time(NULL);
	tm=localtime(&t);
	sprintf(fn,"logs/%02d%02d%02d.log",tm->tm_mon+1,tm->tm_mday,tm->tm_year%100);
	exec_cmdstr(gtkm_conf.view_text_file, cfg.logs_dir,fn);
}

G_MODULE_EXPORT void on_yesterdays_log1_activate(GtkWidget *wiggy, gpointer data)
{
	time_t	t;
	struct tm *tm;
	char	fn[20];

	t=time(NULL);
	t-=24*60*60;
	tm=localtime(&t);
	sprintf(fn,"logs/%02d%02d%02d.log",tm->tm_mon+1,tm->tm_mday,tm->tm_year%100);
	exec_cmdstr(gtkm_conf.view_text_file, cfg.logs_dir,fn);
}

G_MODULE_EXPORT void on_another_days_log1_activate(GtkWidget *wiggy, gpointer data) {
	isoDate_t	date;
	char	fn[20];

	date=time_to_isoDate(time(NULL));
	get_date(wiggy, &date);
	sprintf(fn,"logs/%02d%02d%02d.log",isoDate_month(date),isoDate_day(date),isoDate_year(date)%100);
	exec_cmdstr(gtkm_conf.view_text_file, cfg.logs_dir, fn);
}

G_MODULE_EXPORT void on_spam_log1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.view_text_file, cfg.logs_dir,"spam.log");
}

G_MODULE_EXPORT void on_ip_address_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file, cfg.text_dir,"ip.can");
}

G_MODULE_EXPORT void on_ip_address_filter_silent1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file, cfg.text_dir,"ip-silent.can");
}

G_MODULE_EXPORT void on_ip_address_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file, cfg.text_dir,"badip.msg");
}

G_MODULE_EXPORT void on_host_name_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"host.can");
}

G_MODULE_EXPORT void on_host_name_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"badhost.msg");
}

G_MODULE_EXPORT void on_user_name_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"name.can");
}

G_MODULE_EXPORT void on_user_name_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"badname.msg");
}

G_MODULE_EXPORT void on_email_address_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"email.can");
}

G_MODULE_EXPORT void on_email_address_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"bademail.msg");
}

G_MODULE_EXPORT void on_email_subject_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"subject.can");
}

G_MODULE_EXPORT void on_file_name_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"file.can");
}

G_MODULE_EXPORT void on_file_name_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"badfile.msg");
}

G_MODULE_EXPORT void on_phone_number_filter1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"phone.can");
}

G_MODULE_EXPORT void on_phone_number_filter_message1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.text_dir,"badphone.msg");
}

G_MODULE_EXPORT void on_twit_list1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.edit_text_file,cfg.ctrl_dir,"twitlist.cfg");
}

G_MODULE_EXPORT void on_hack_attempt_log1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.view_text_file,cfg.logs_dir,"hack.log");
}

G_MODULE_EXPORT void on_configure1_activate(GtkWidget *wiggy, gpointer data)
{
	run_external(cfg.exec_dir,"scfg");
}

G_MODULE_EXPORT void on_edit3_activate(GtkWidget *wiggy, gpointer data)
{
	run_external(cfg.exec_dir,"gtkuseredit");
}

G_MODULE_EXPORT void on_truncate_deleted_users1_activate(GtkWidget *wiggy, gpointer data)
{
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
	display_message("Users Truncated", str, NULL);
}

G_MODULE_EXPORT void on_stop6_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown");
}

G_MODULE_EXPORT void on_recycle6_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle");
}

G_MODULE_EXPORT void on_stop1_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.telnet");
}

G_MODULE_EXPORT void on_recycle5_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.telnet");
}

G_MODULE_EXPORT void on_stop2_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.mail");
}

G_MODULE_EXPORT void on_recycle1_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.mail");
}

G_MODULE_EXPORT void on_stop3_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.ftp");
}

G_MODULE_EXPORT void on_recycle2_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.ftp");
}

G_MODULE_EXPORT void on_stop4_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.web");
}

G_MODULE_EXPORT void on_recycle3_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.web");
}

G_MODULE_EXPORT void on_stop5_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "shutdown.services");
}

G_MODULE_EXPORT void on_recycle4_activate(GtkWidget *wiggy, gpointer data)
{
	touch_sem(cfg.ctrl_dir, "recycle.services");
}

G_MODULE_EXPORT void on_statistics_pane1_activate(GtkWidget *wiggy, gpointer data)
{
	GtkWidget *w;

	w=GTK_WIDGET(gtk_builder_get_object (builder, "StatisticsPane"));
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

G_MODULE_EXPORT void create_force_sem(GtkWidget *wiggy, gpointer data)
{
	gchar	*label;
	char	fn[MAX_PATH+1];

	label=(gchar *)gtk_label_get_text(GTK_LABEL(wiggy));
	if(label != NULL) {
		sprintf(fn,"%s%s.now",(gchar *)data, label);
		touch_sem(cfg.data_dir, fn);
	}
}

G_MODULE_EXPORT void on_force_event(GtkWidget *wiggy, gpointer data)
{
	/* There's only one child... so this is a bit of a cheat */
	gtk_container_foreach(GTK_CONTAINER(wiggy), create_force_sem, "");
}

G_MODULE_EXPORT void on_force_qnet(GtkWidget *wiggy, gpointer data)
{
	/* There's only one child... so this is a bit of a cheat */
	gtk_container_foreach(GTK_CONTAINER(wiggy), create_force_sem, "qnet/");
}

G_MODULE_EXPORT void on_reload_configuration1_activate(GtkWidget *wiggy, gpointer data)
{
	refresh_events();
}

G_MODULE_EXPORT void toggle_node_bits(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	*bit=data;
	int	fd = -1;
	char	*node_str;
	int		n,i;
	node_t	node;
	char	str[128];

	gtk_tree_model_get(model, iter, 0, &node_str, -1);
	n=atoi(node_str);

	if((i=getnodedat(&cfg,n,&node,TRUE,&fd))) {
		sprintf(str,"Error reading node %d data (%d)!",n,i);
		display_message("Read Error", str, "gtk-dialog-error");
	}
	else {
		node.misc ^= *bit;
		putnodedat(&cfg, n, &node, TRUE, fd);
	}
}

G_MODULE_EXPORT void lock_nodes(GtkWidget *wiggy, gpointer data)
{
	int		bit=NODE_LOCK;

	gtk_tree_selection_selected_foreach(sel
			,toggle_node_bits
			,&bit);
	refresh_data(NULL);
}

G_MODULE_EXPORT void down_nodes(GtkWidget *wiggy, gpointer data)
{
	int		bit=NODE_DOWN;

	gtk_tree_selection_selected_foreach(sel
			,toggle_node_bits
			,&bit);
	refresh_data(NULL);
}

G_MODULE_EXPORT void interrupt_nodes(GtkWidget *wiggy, gpointer data)
{
	int		bit=NODE_INTR;

	gtk_tree_selection_selected_foreach(sel
			,toggle_node_bits
			,&bit);
	refresh_data(NULL);
}

G_MODULE_EXPORT void rerun_nodes(GtkWidget *wiggy, gpointer data)
{
	int		bit=NODE_RRUN;

	gtk_tree_selection_selected_foreach(sel
			,toggle_node_bits
			,&bit);
	refresh_data(NULL);
}

G_MODULE_EXPORT void do_clear_errors(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	fd = -1;
	char	*node_str;
	int		n,i;
	node_t	node;
	char	str[128];

	gtk_tree_model_get(model, iter, 0, &node_str, -1);
	n=atoi(node_str);

	if((i=getnodedat(&cfg,n,&node,TRUE,&fd))) {
		sprintf(str,"Error reading node %d data (%d)!",n,i);
		display_message("Read Error",str,"gtk-dialog-error");
	}
	else {
		node.errors = 0;
		putnodedat(&cfg, n, &node, TRUE, fd);
	}
}

G_MODULE_EXPORT void clear_errors(GtkWidget *wiggy, gpointer data)
{
	gtk_tree_selection_selected_foreach(sel
			,do_clear_errors
			,NULL);
	refresh_data(NULL);
}

G_MODULE_EXPORT void on_about1_activate(GtkWidget *wiggy, gpointer data)
{
	gtk_window_present(GTK_WINDOW(gtk_builder_get_object (builder, "AboutWindow")));
}

G_MODULE_EXPORT void get_lastselected_node(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	int	*i=data;
	gchar	*node;

	gtk_tree_model_get(model, iter, 0, &node, -1);
	*i=atoi(node);
}

G_MODULE_EXPORT void chatwith_node(GtkWidget *wiggy, gpointer data)
{
	char	str[MAX_PATH+1];
	int		i;

	gtk_tree_selection_selected_foreach(sel
			,get_lastselected_node
			,&i);
	sprintf(str,"gtkchat %d",i);
	run_external(cfg.exec_dir,str);
}

G_MODULE_EXPORT void edituseron_node(GtkWidget *wiggy, gpointer data)
{
	char	str[MAX_PATH+1];
	int		i;
	node_t	node;

	gtk_tree_selection_selected_foreach(sel
			,get_lastselected_node
			,&i);

	if((i=getnodedat(&cfg,i,&node,FALSE,NULL))) {
		sprintf(str,"Error reading node data (%d)!",i);
		display_message("Read Error",str,"gtk-dialog-error");
	}
	else {
		sprintf(str,"gtkuseredit %d",node.useron);
		run_external(cfg.exec_dir,str);
	}
}

G_MODULE_EXPORT void close_this_window(GtkWidget *wiggy, gpointer data)
{
	gtk_widget_destroy(GTK_WIDGET(gtk_widget_get_toplevel(wiggy)));
}

G_MODULE_EXPORT void quick_validate(int usernum, int set)
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

/* Show user list */
G_MODULE_EXPORT void on_list1_activate(GtkWidget *wiggy, gpointer data)
{
	run_external(cfg.exec_dir,"gtkuserlist");
}

G_MODULE_EXPORT void quickvalidate_useron_node(GtkWidget *wiggy, gpointer data)
{
	char	str[MAX_PATH+1];
	int		i;
	int		set;
	node_t	node;

	set=gtk_combo_box_get_active(GTK_COMBO_BOX(wiggy))-1;
	if(set>=0) {
		gtk_tree_selection_selected_foreach(sel
				,get_lastselected_node
				,&i);

		if((i=getnodedat(&cfg,i,&node,FALSE,NULL))) {
			sprintf(str,"Error reading node data (%d)!",i);
			display_message("Read Error",str,"gtk-dialog-error");
		}
		else {
			quick_validate(node.useron, set);
		}
		gtk_combo_box_set_active(GTK_COMBO_BOX(wiggy), 0);
	}
}

char *select_filename(GtkWidget *wiggy, char *title, char *name, char *in_pattern, char *dir, char *fn)
{
	GtkWidget 		*chooser;
	GtkFileFilter	*filter;
	char			*search;
	char			*next;
	char			*p1,*p2;
	char			*pattern;
	char			pat[MAX_PATH+1];

	chooser=gtk_file_chooser_dialog_new(title
			,GTK_WINDOW(gtk_widget_get_toplevel(wiggy))
			,GTK_FILE_CHOOSER_ACTION_OPEN
			,GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL
			,GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT
			,NULL);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(chooser), TRUE);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser) ,dir);
	filter=gtk_file_filter_new();
	gtk_file_filter_set_name(filter, name);
	pattern=strdup(in_pattern);
	search=pattern;
	while((next=strtok(search, ";"))!=NULL) {
		search=NULL;
		pat[0]=0;
		p2=pat;
		for(p1=next;*p1;p1++) {
			if(toupper(*p1)!=tolower(*p1)) {
				*(p2++)='[';
				*(p2++)=toupper(*p1);
				*(p2++)=tolower(*p1);
				*(p2++)=']';
			}
			else
				*(p2++)=*p1;
			*p2=0;
		}
		gtk_file_filter_add_pattern(filter, pat);
	}
	free(pattern);
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
	filter=gtk_file_filter_new();
	gtk_file_filter_set_name(filter, "All Files");
	gtk_file_filter_add_pattern(filter, "*");
	gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(chooser), filter);
	
	switch(gtk_dialog_run(GTK_DIALOG(chooser))) {
		case GTK_RESPONSE_ACCEPT:
			strcpy(fn, gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser)));
			break;
		default:
			fn[0]=0;
	}
	gtk_widget_destroy(chooser);
	return(fn);
}

G_MODULE_EXPORT void on_text_file1_activate(GtkWidget *wiggy, gpointer data)
{
	char	fn[MAX_PATH+1];
	select_filename(wiggy, "Edit Text File", "Text Files", "*.txt", cfg.text_dir, fn);
	if(fn[0])
		exec_cmdstr(gtkm_conf.edit_text_file, NULL, fn);
}

G_MODULE_EXPORT void on_javascript_file1_activate(GtkWidget *wiggy, gpointer data)
{
	char	fn[MAX_PATH+1];
	select_filename(wiggy, "Edit Javascript File", "Javascript Files", "*.js", cfg.exec_dir, fn);
	if(fn[0])
		exec_cmdstr(gtkm_conf.edit_text_file, NULL, fn);
}

G_MODULE_EXPORT void on_configuration_file1_activate(GtkWidget *wiggy, gpointer data)
{
	char	fn[MAX_PATH+1];

	select_filename(wiggy, "Edit Configuration File", "Configuration Files", "*.cfg;*.ini;*.conf", cfg.ctrl_dir, fn);
	if(fn[0])
		exec_cmdstr(gtkm_conf.edit_text_file, NULL, fn);
}

G_MODULE_EXPORT void on_edit_and_compile_baja_script1_activate(GtkWidget *wiggy, gpointer data)
{
	char	fn[MAX_PATH+1];
	char	compile[MAX_PATH*2+1];

	select_filename(wiggy, "Edit/Compile Baja Script", "BAJA Files", "*.src", cfg.exec_dir, fn);
	if(!run_cmd_mutex_initalized) {
		pthread_mutex_init(&run_cmd_mutex, NULL);
		run_cmd_mutex_initalized=1;
	}
	if(fn[0]) {
		exec_cmdstr(gtkm_conf.edit_text_file, NULL, fn);
		/* Spin on the lock waiting for the edit command to start */
		while(!pthread_mutex_trylock(&run_cmd_mutex))
			pthread_mutex_unlock(&run_cmd_mutex);
		sprintf(compile, "baja %s", fn);
		exec_cmdstr(gtkm_conf.view_stdout,cfg.exec_dir,compile);
	}
}

G_MODULE_EXPORT void on_index1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.view_html_file,cfg.ctrl_dir,"../docs/index.htm");
}

G_MODULE_EXPORT void on_sysop_manual1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.view_html_file,cfg.ctrl_dir,"../docs/sysop.html");
}

G_MODULE_EXPORT void on_frequently_asked_questions_faq1_activate(GtkWidget *wiggy, gpointer data)
{
	exec_cmdstr(gtkm_conf.view_text_file,cfg.ctrl_dir,"../docs/v3cfgfaq.txt");
}

G_MODULE_EXPORT void on_preview_file1_activate(GtkWidget *wiggy, gpointer data)
{
	char	fn[MAX_PATH+1];

	select_filename(wiggy, "Preview File", "ANSI/Ctrl-A Files", "*.ans;*.asc;*.msg", cfg.text_dir, fn);
	if(fn[0])
		exec_cmdstr(gtkm_conf.view_ctrla_file,NULL, fn);
}

G_MODULE_EXPORT void on_edit_and_preview_file1_activate(GtkWidget *wiggy, gpointer data)
{
	char	fn[MAX_PATH+1];

	select_filename(wiggy, "Edit and Preview File", "ANSI/Ctrl-A Files", "*.ans;*.asc;*.msg", cfg.text_dir, fn);
	if(!run_cmd_mutex_initalized) {
		pthread_mutex_init(&run_cmd_mutex, NULL);
		run_cmd_mutex_initalized=1;
	}
	if(fn[0]) {
		exec_cmdstr(gtkm_conf.edit_text_file, NULL, fn);
		/* Spin on the lock waiting for the edit command to start */
		while(!pthread_mutex_trylock(&run_cmd_mutex))
			pthread_mutex_unlock(&run_cmd_mutex);
		exec_cmdstr(gtkm_conf.view_ctrla_file, NULL, fn);
	}
}

G_MODULE_EXPORT void sendmessageto_node(GtkWidget *wiggy, gpointer data)
{
	char	fn[MAX_PATH+1];
	char	str[MAX_PATH+1];
	int		i;
	int		tmp;
	node_t	node;
	time_t	edited;
	struct utimbuf tb;
	char	*msg;

	gtk_tree_selection_selected_foreach(sel
			,get_lastselected_node
			,&i);

	if((i=getnodedat(&cfg,i,&node,FALSE,NULL))) {
		sprintf(str,"Error reading node data (%d)!",i);
		display_message("Read Error",str,"gtk-dialog-error");
	}
	else {
		strcpy(fn,"/tmp/gtkmonitor-msg-XXXXXXXX");
		tmp=mkstemp(fn);
		if(tmp!=-1) {
			write(tmp,"\1n\1y\1hMessage From Sysop:\1w \n\n",30);
			close(tmp);
			/* Set modified time back one second so we can tell if the sysop
			   saved the file or not */
			edited=fdate(fn);
			edited--;
			tb.actime=edited;
			tb.modtime=edited;
			utime(fn, &tb);
			/* If utime() failed for some reason, sleep for a second */
			if(fdate(fn)!=edited)
				SLEEP(1000);
			if(!run_cmd_mutex_initalized) {
				pthread_mutex_init(&run_cmd_mutex, NULL);
				run_cmd_mutex_initalized=1;
			}
			exec_cmdstr(gtkm_conf.edit_text_file, NULL, fn);
			/* Spin on the lock waiting for the edit command to start */
			while(!pthread_mutex_trylock(&run_cmd_mutex))
				pthread_mutex_unlock(&run_cmd_mutex);
			/* Now, spin on the lock waiting for it to *exit* */
			while(pthread_mutex_trylock(&run_cmd_mutex)) {
				/* Allow events to happen as normal */
				while(gtk_events_pending()) {
					if(gtk_main_iteration())
						gtk_main_quit();
				}
				SLEEP(1);
			}
			pthread_mutex_unlock(&run_cmd_mutex);
			/* Now, read the message back in and send to the user */
			if(fdate(fn)!=edited) {
				i=flength(fn);
				if((msg=(char *)malloc(i))==NULL)
					display_message("malloc() Error", "Cannot allocate enough memory for the message", "gtk-dialog-error");
				else {
					tmp=open(fn, O_RDONLY);
					if(tmp==-1)
						display_message("open() Error", "Cannot open temp message file", "gtk-dialog-error");
					else {
						if(read(tmp, msg, i)!=i)
							display_message("read() Error", "Problem reading message file", "gtk-dialog-error");
						else {
							putsmsg(&cfg, node.useron, msg);
						}
					}
					free(msg);
				}
			}
		}
	}
}

G_MODULE_EXPORT void on_properties1_activate(GtkWidget *wiggy, gpointer data)
{
	GtkWidget	*dialog;
	GtkWidget	*w;

	dialog=GTK_WIDGET(gtk_builder_get_object (builder, "PreferencesDialog"));

	/* Put in the current values */
	w=GTK_WIDGET(gtk_builder_get_object (builder, "eEditTextFile"));
	gtk_entry_set_text(GTK_ENTRY(w),gtkm_conf.edit_text_file);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "eViewTextFile"));
	gtk_entry_set_text(GTK_ENTRY(w),gtkm_conf.view_text_file);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "eViewStdout"));
	gtk_entry_set_text(GTK_ENTRY(w),gtkm_conf.view_stdout);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "eViewCtrlAFile"));
	gtk_entry_set_text(GTK_ENTRY(w),gtkm_conf.view_ctrla_file);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "eViewHTMLFile"));
	gtk_entry_set_text(GTK_ENTRY(w),gtkm_conf.view_html_file);

	switch(gtk_dialog_run(GTK_DIALOG(dialog))) {
		case GTK_RESPONSE_OK:
			/* Read out the new values */
			w=GTK_WIDGET(gtk_builder_get_object (builder, "eEditTextFile"));
			strcpy(gtkm_conf.edit_text_file,gtk_entry_get_text(GTK_ENTRY(w)));
			w=GTK_WIDGET(gtk_builder_get_object (builder, "eViewTextFile"));
			strcpy(gtkm_conf.view_text_file,gtk_entry_get_text(GTK_ENTRY(w)));
			w=GTK_WIDGET(gtk_builder_get_object (builder, "eViewStdout"));
			strcpy(gtkm_conf.view_stdout,gtk_entry_get_text(GTK_ENTRY(w)));
			w=GTK_WIDGET(gtk_builder_get_object (builder, "eViewCtrlAFile"));
			strcpy(gtkm_conf.view_ctrla_file,gtk_entry_get_text(GTK_ENTRY(w)));
			w=GTK_WIDGET(gtk_builder_get_object (builder, "eViewHTMLFile"));
			strcpy(gtkm_conf.view_html_file,gtk_entry_get_text(GTK_ENTRY(w)));
			write_ini();
	}
	gtk_widget_hide_on_delete(dialog);
}
