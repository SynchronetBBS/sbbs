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

