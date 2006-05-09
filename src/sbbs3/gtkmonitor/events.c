#include "events.h"
#include "gtkmonitor.h"
#include "util_funcs.h"

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
#warning Missing ftpbadlogin.txt

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

#warning Remove BBS stats form thinger.
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

void on_todays_log2_activate(GtkWidget *wiggy, gpointer data)
{
	time_t	t;
	struct tm *tm;
	char	fn[20];

	t=time(NULL);
	tm=localtime(&t);
	sprintf(fn,"logs/ms%02d%02d%02d.log",tm->tm_mon+1,tm->tm_mday,tm->tm_year%100);
	view_text_file(cfg.logs_dir,fn);
}

void on_yesterdays_log2_activate(GtkWidget *wiggy, gpointer data)
{
	time_t	t;
	struct tm *tm;
	char	fn[20];

	t=time(NULL);
	t-=24*60*60;
	tm=localtime(&t);
	sprintf(fn,"logs/ms%02d%02d%02d.log",tm->tm_mon+1,tm->tm_mday,tm->tm_year%100);
	view_text_file(cfg.logs_dir,fn);
}

void on_another_days_log2_activate(GtkWidget *wiggy, gpointer data) {
	/* ToDo */
}

void on_todays_log3_activate(GtkWidget *wiggy, gpointer data)
{
	time_t	t;
	struct tm *tm;
	char	fn[20];

	t=time(NULL);
	tm=localtime(&t);
	sprintf(fn,"logs/fs%02d%02d%02d.log",tm->tm_mon+1,tm->tm_mday,tm->tm_year%100);
	view_text_file(cfg.logs_dir,fn);
}

void on_yesterdays_log3_activate(GtkWidget *wiggy, gpointer data)
{
	time_t	t;
	struct tm *tm;
	char	fn[20];

	t=time(NULL);
	t-=24*60*60;
	tm=localtime(&t);
	sprintf(fn,"logs/fs%02d%02d%02d.log",tm->tm_mon+1,tm->tm_mday,tm->tm_year%100);
	view_text_file(cfg.logs_dir,fn);
}

void on_another_days_log3_activate(GtkWidget *wiggy, gpointer data) {
	/* ToDo */
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

