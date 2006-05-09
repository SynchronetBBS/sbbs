#include <stdlib.h>
#include "dirwrap.h"
#include "threadwrap.h"

void run_cmdline(void *cmdline)
{
	system((char *)cmdline);
}

/* ToDo: This will need to read the command-line from a config file */
void view_text_file(char *path, char *filename)
{
	static char	cmdline[MAX_PATH*2];
	char	p[MAX_PATH+1];

	if(path != NULL) {
		strcpy(p, path);
		backslash(p);
	}
	else
		p[0]=0;

	strcat(p, filename);
	sprintf(cmdline, "xmessage -file %s", p);
	_beginthread(run_cmdline, 0, cmdline);
}

/* ToDo: This will need to read the command-line from a config file */
void edit_text_file(char *path, char *filename)
{
	static char	cmdline[MAX_PATH*2];
	char	p[MAX_PATH+1];

	if(path != NULL) {
		strcpy(p, path);
		backslash(p);
	}
	else
		p[0]=0;

	strcat(p, filename);
	sprintf(cmdline, "xedit %s", p);
	_beginthread(run_cmdline, 0, cmdline);
}

