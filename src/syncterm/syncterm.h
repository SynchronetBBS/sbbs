#ifndef _SYNCTERM_H_
#define _SYNCTERM_H_

#include "bbslist.h"

enum {
	 SYNCTERM_PATH_INI
	,SYNCTERM_PATH_LIST
	,SYNCTERM_DEFAULT_TRANSFER_PATH
};

struct syncterm_settings {
	int		confirm_close;
	int		startup_mode;
	int		backlines;
};

extern char *inpath;
extern char *syncterm_version;
extern unsigned char *scrollback_buf;
extern unsigned int   scrollback_lines;
extern struct syncterm_settings settings;
void parse_url(char *url, struct bbslist *bbs, int dflt_conn_type, int force_defaults);
extern int default_font;
extern char *font_names[];
extern int safe_mode;
char *get_syncterm_filename(char *fn, int fnlen, int type, int shared);
void load_settings(struct syncterm_settings *set);

#endif
