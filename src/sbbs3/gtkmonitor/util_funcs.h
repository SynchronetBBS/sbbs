#ifndef _UTIL_FUNCS_H_
#define _UTIL_FUNCS_H_

#include "gen_defs.h"	/* BOOL */

extern int run_cmd_mutex_initalized;
extern pthread_mutex_t	run_cmd_mutex;

void run_cmdline(void *cmdline);
void run_external(char *path, char *filename);
void view_stdout(char *path, char *filename);
void view_text_file(char *path, char *filename);
void edit_text_file(char *path, char *filename);
void view_html_file(char *path, char *filename);
char *getsizestr(char *outstr, long size, BOOL bytes);
char *getnumstr(char *outstr, ulong size);
void touch_sem(char *path, char *filename);
void display_message(char *title, char *message, char *icon);

#endif
