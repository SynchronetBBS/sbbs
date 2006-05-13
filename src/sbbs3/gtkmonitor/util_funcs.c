#include <stdlib.h>
#include "util_funcs.h"
#include "dirwrap.h"
#include "threadwrap.h"
#include "gtkmonitor.h"
#include "semwrap.h"

int run_cmd_mutex_initalized=0;
pthread_mutex_t	run_cmd_mutex;

void run_cmdline(void *cmdline)
{
	GtkWidget	*w;

	if(!run_cmd_mutex_initalized) {
		pthread_mutex_init(&run_cmd_mutex, NULL);
		run_cmd_mutex_initalized=1;
	}
	pthread_mutex_lock(&run_cmd_mutex);
	system((char *)cmdline);
	w=glade_xml_get_widget(xml, "MainWindow");
	gtk_widget_set_sensitive(GTK_WIDGET(w), TRUE);
	pthread_mutex_unlock(&run_cmd_mutex);
}

char *complete_path(char *dest, char *path, char *filename)
{
	if(path != NULL) {
		strcpy(dest, path);
		backslash(dest);
	}
	else
		dest[0]=0;

	strcat(dest, filename);
	fexistcase(dest);			/* Fixes upr/lwr case fname */
	return(dest);
}

/* ToDo: This will need to read the command-line from a config file */
void run_external(char *path, char *filename)
{
	static char	cmdline[MAX_PATH*2];
	GtkWidget	*w;

	w=glade_xml_get_widget(xml, "MainWindow");
	gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
	_beginthread(run_cmdline, 0, complete_path(cmdline,path,filename));
}

/* ToDo: This will need to read the command-line from a config file */
void view_stdout(char *path, char *filename)
{
	static char	cmdline[MAX_PATH*2];
	char	p[MAX_PATH+1];
	GtkWidget	*w;

	sprintf(cmdline, "%s | xmessage -file -", complete_path(p,path,filename));
	w=glade_xml_get_widget(xml, "MainWindow");
	gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
	_beginthread(run_cmdline, 0, cmdline);
}

/* ToDo: This will need to read the command-line from a config file */
void view_text_file(char *path, char *filename)
{
	static char	cmdline[MAX_PATH*2];
	char	p[MAX_PATH+1];
	GtkWidget	*w;

	complete_path(p,path,filename);
	if(!fexist(p)) {
		sprintf(cmdline,"The file %s does not exist.",p);
		display_message("File Does Not Exist", cmdline, "gtk-dialog-error");
	}
	else {
		if(access(p,R_OK)) {
			sprintf(cmdline,"Cannot read the file %s... check your permissions.",p);
			display_message("Cannot Read File", cmdline, "gtk-dialog-error");
		}
		else {
			sprintf(cmdline, "xmessage -file %s", p);
			w=glade_xml_get_widget(xml, "MainWindow");
			gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
			_beginthread(run_cmdline, 0, cmdline);
		}
	}
}

/* ToDo: This will need to read the command-line from a config file */
void edit_text_file(char *path, char *filename)
{
	static char	cmdline[MAX_PATH*2];
	char	p[MAX_PATH+1];
	GtkWidget	*w;

	sprintf(cmdline, "xedit %s", complete_path(p,path,filename));
	w=glade_xml_get_widget(xml, "MainWindow");
	gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
	_beginthread(run_cmdline, 0, cmdline);
}

/* ToDo: This will need to read the command-line from a config file */
void view_html_file(char *path, char *filename)
{
	static char	cmdline[MAX_PATH*2];
	char	p[MAX_PATH+1];
	GtkWidget	*w;

	sprintf(cmdline, "firefox file://%s", complete_path(p,path,filename));
	w=glade_xml_get_widget(xml, "MainWindow");
	gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
	_beginthread(run_cmdline, 0, cmdline);
}

void touch_sem(char *path, char *filename)
{
	char	fn[MAX_PATH*2];

	ftouch(complete_path(fn,path,filename));
}

/* Assumes a 12 char outstr */
char *getsizestr(char *outstr, long size, BOOL bytes) {
	if(bytes) {
		if(size < 1000) {	/* Bytes */
			snprintf(outstr,12,"%ld bytes",size);
			return(outstr);
		}
		if(size<10000) {	/* Bytes with comma */
			snprintf(outstr,12,"%ld,%03ld bytes",(size/1000),(size%1000));
			return(outstr);
		}
		size = size/1024;
	}
	if(size<1000) {	/* KB */
		snprintf(outstr,12,"%ld KB",size);
		return(outstr);
	}
	if(size<999999) { /* KB With comma */
		snprintf(outstr,12,"%ld,%03ld KB",(size/1000),(size%1000));
		return(outstr);
	}
	size = size/1024;
	if(size<1000) {	/* MB */
		snprintf(outstr,12,"%ld MB",size);
		return(outstr);
	}
	if(size<999999) { /* MB With comma */
		snprintf(outstr,12,"%ld,%03ld MB",(size/1000),(size%1000));
		return(outstr);
	}
	size = size/1024;
	if(size<1000) {	/* GB */
		snprintf(outstr,12,"%ld GB",size);
		return(outstr);
	}
	if(size<999999) { /* GB With comma */
		snprintf(outstr,12,"%ld,%03ld GB",(size/1000),(size%1000));
		return(outstr);
	}
	size = size/1024;
	if(size<1000) {	/* TB (Yeah, right) */
		snprintf(outstr,12,"%ld TB",size);
		return(outstr);
	}
	sprintf(outstr,"Plenty");
	return(outstr);
}

/* Assumes a 12 char outstr */
char *getnumstr(char *outstr, ulong size) {
	if(size < 1000) {
		snprintf(outstr,12,"%ld",size);
		return(outstr);
	}
	if(size<1000000) {
		snprintf(outstr,12,"%ld,%03ld",(size/1000),(size%1000));
		return(outstr);
	}
	if(size<1000000000) {
		snprintf(outstr,12,"%ld,%03ld,%03ld",(size/1000000),((size/1000)%1000),(size%1000));
		return(outstr);
	}
	size=size/1000000;
	if(size<1000000) {
		snprintf(outstr,12,"%ld,%03ld M",(size/1000),(size%1000));
		return(outstr);
	}
	if(size<10000000) {
		snprintf(outstr,12,"%ld,%03ld,%03ld M",(size/1000000),((size/1000)%1000),(size%1000));
		return(outstr);
	}
	sprintf(outstr,"Plenty");
	return(outstr);
}

void display_message(char *title, char *message, char *icon)
{
	GtkWidget *dialog, *label;

	dialog=gtk_dialog_new_with_buttons(title
			,GTK_WINDOW(glade_xml_get_widget(xml, "MainWindow"))
			,GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT
			,GTK_STOCK_OK
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
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
			label);
	gtk_widget_show_all (dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
}
