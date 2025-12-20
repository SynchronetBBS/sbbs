/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _TERM_H_
#define _TERM_H_

#include <stdbool.h>

#include "bbslist.h"
#include "ciolib.h"

struct terminal {
	int height;
	int width;
	int x;
	int y;
	int nostatus;
};

enum mouse_modes {
	MM_OFF,
	MM_RIP = 1,
	MM_X10 = 9,
	MM_NORMAL_TRACKING = 1000,
	MM_HIGHLIGHT_TRACKING = 1001,
	MM_BUTTON_EVENT_TRACKING = 1002,
	MM_ANY_EVENT_TRACKING = 1003
};

struct mouse_state {
	uint32_t         flags;

#define MS_FLAGS_SGR (1 << 0)
#define MS_FLAGS_DISABLED (1 << 1)
#define MS_SGR_SET (1006)
	enum mouse_modes mode;
};

#define XMODEM_128B (1 << 10) /* Use 128 byte block size (ick!) */

extern struct terminal   term;
extern struct cterminal *cterm;
extern int               log_level;
void zmodem_upload(struct bbslist *bbs, FILE *fp, char *path);
void xmodem_upload(struct bbslist *bbs, FILE *fp, char *path, long mode, int lastch);
void xmodem_download(struct bbslist *bbs, long mode, char *path);
void zmodem_download(struct bbslist *bbs);
bool doterm(struct bbslist *);
void mousedrag(struct vmem_cell *scrollback);
void get_cterm_size(int *cols, int *rows, int ns);
int get_cache_fn_base(struct bbslist *bbs, char *fn, size_t fnsz);
int get_cache_fn_subdir(struct bbslist *bbs, char *fn, size_t fnsz, const char *subdir);
void send_login(struct bbslist *bbs);

#endif // ifndef _TERM_H_
