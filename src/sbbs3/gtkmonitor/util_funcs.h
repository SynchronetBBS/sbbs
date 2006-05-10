#ifndef _UTIL_FUNCS_H_
#define _UTIL_FUNCS_H_

#include "gen_defs.h"	/* BOOL */

void run_cmdline(void *cmdline);
void run_external(char *path, char *filename);
void view_stdout(char *path, char *filename);
void view_text_file(char *path, char *filename);
void edit_text_file(char *path, char *filename);
char *getsizestr(char *outstr, long size, BOOL bytes);
char *getnumstr(char *outstr, ulong size);
void touch_sem(char *path, char *filename);
void display_message(char *title, char *message);

#endif
