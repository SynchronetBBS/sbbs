#ifndef _UTIL_FUNCS_H_
#define _UTIL_FUNCS_H_

#include "gen_defs.h"	/* BOOL */

extern int run_cmd_mutex_initalized;
extern pthread_mutex_t	run_cmd_mutex;

void exec_cmdstr(char *cmdstr, char *path, char *filename);
void run_cmdline(void *cmdline);
void run_external(char *path, char *filename);
char *getsizestr(char *outstr, long size, BOOL bytes);
char *getnumstr(char *outstr, ulong size);
void touch_sem(char *path, char *filename);
void display_message(char *title, char *message, char *icon);
char *complete_path(char *dest, char *path, char *filename);

#endif
