#include <stdlib.h>
#include "util_funcs.h"
#include "dirwrap.h"
#include "threadwrap.h"
#include "gtkmonitor.h"
#include "semwrap.h"

int run_cmd_mutex_initalized=0;
pthread_mutex_t	run_cmd_mutex;

/*****************************************************************************/
/* Returns command line generated from instr with %c replacments             */
/*****************************************************************************/
char *mycmdstr(char *instr, char *fpath)
{
    char str[256],str2[128];
    int i,j,len;
#if 0
    char 	cmd[MAX_PATH+1];
#else
	char	*cmd;
#endif

	cmd=(char *)malloc(MAX_PATH+1);
	if(cmd==NULL) {
		display_message("malloc() Error", "Cannot allocate enough memory for the message", "gtk-dialog-error");
		return(NULL);
	}
	len=strlen(instr);
	for(i=j=0;i<len && j<128;i++) {
		if(instr[i]=='%') {
			i++;
			cmd[j]=0;
			switch(toupper(instr[i])) {
				case 'F':   /* File path */
					strcat(cmd,fpath);
					break;
				case 'G':   /* Temp directory */
					if(cfg.temp_dir[0]!='\\' 
							&& cfg.temp_dir[0]!='/' 
							&& cfg.temp_dir[1]!=':') {
						strcpy(str,cfg.ctrl_dir);
						strcat(str,cfg.temp_dir);
						if(FULLPATH(str2,str,40))
							strcpy(str,str2);
						backslash(str);
						strcat(cmd,str);
					}
					else
						strcat(cmd,cfg.temp_dir);
					break;
				case 'J':
					if(cfg.data_dir[0]!='\\' 
							&& cfg.data_dir[0]!='/' 
							&& cfg.data_dir[1]!=':') {
						strcpy(str,cfg.ctrl_dir);
						strcat(str,cfg.data_dir);
						if(FULLPATH(str2,str,40))
							strcpy(str,str2);
						backslash(str);
						strcat(cmd,str);
					}
					else
						strcat(cmd,cfg.data_dir);
					break;
				case 'K':
					strcat(cmd,cfg.ctrl_dir);
					break;
				case 'O':   /* SysOp */
					strcat(cmd,cfg.sys_op);
					break;
				case 'Q':   /* QWK ID */
					strcat(cmd,cfg.sys_id);
					break;
				case '!':   /* EXEC Directory */
					strcat(cmd,cfg.exec_dir);
					break;
                case '@':   /* EXEC Directory for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
                    strcat(cmd,cfg.exec_dir);
#endif
                    break;
				case '%':   /* %% for percent sign */
					strcat(cmd,"%");
					break;
				case '.':	/* .exe for DOS/OS2/Win32, blank for Unix */
#ifndef __unix__
					strcat(cmd,".exe");
#endif
					break;
				case '?':	/* Platform */
#ifdef __OS2__
					strcpy(str,"OS2");
#else
					strcpy(str,PLATFORM_DESC);
#endif
					strlwr(str);
					strcat(cmd,str);
					break;
				default:    /* unknown specification */
					sprintf(cmd,"ERROR Checking Command Line '%s'",instr);
					display_message("Command line Error",cmd,"gtk-dialog-error");
					free(cmd);
					return(NULL);
			}
			j=strlen(cmd);
		}
		else
			cmd[j++]=instr[i];
	}
	cmd[j]=0;

	return(cmd);
}

void exec_cmdline(void *cmdline)
{
	GtkWidget	*w;

	if(cmdline==NULL)
		return;
	if(!run_cmd_mutex_initalized) {
		pthread_mutex_init(&run_cmd_mutex, NULL);
		run_cmd_mutex_initalized=1;
	}
	pthread_mutex_lock(&run_cmd_mutex);
	system((char *)cmdline);
	free(cmdline);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "MainWindow"));
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

	if(filename != NULL)
		strcat(dest, filename);
	fexistcase(dest);			/* TODO: Hack: Fixes upr/lwr case fname */
	return(dest);
}

void run_external(char *path, char *filename)
{
	static char	cmdline[MAX_PATH*2];
	GtkWidget	*w;

	w=GTK_WIDGET(gtk_builder_get_object (builder, "MainWindow"));
	gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
	complete_path(cmdline,path,filename);
	_beginthread(exec_cmdline, 0, strdup(cmdline));
}

void exec_cmdstr(char *cmdstr, char *path, char *filename)
{
	char	p[MAX_PATH+1];
	GtkWidget	*w;

	complete_path(p,path,filename);
	w=GTK_WIDGET(gtk_builder_get_object (builder, "MainWindow"));
	gtk_widget_set_sensitive(GTK_WIDGET(w), FALSE);
	_beginthread(exec_cmdline, 0, mycmdstr(cmdstr, p));
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
			,GTK_WINDOW(GTK_WIDGET(gtk_builder_get_object (builder, "MainWindow")))
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
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)),
			label);
	gtk_widget_show_all (dialog);
	gtk_dialog_run(GTK_DIALOG(dialog));
}
