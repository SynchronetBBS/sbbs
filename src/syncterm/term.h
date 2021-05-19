/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: term.h,v 1.20 2020/05/02 03:09:15 rswindell Exp $ */

#ifndef _TERM_H_
#define _TERM_H_

#include "bbslist.h"
#include "ciolib.h"

struct terminal {
	int	height;
	int	width;
	int	x;
	int	y;
	int nostatus;
};

#define XMODEM_128B		(1<<10)	/* Use 128 byte block size (ick!) */

extern struct terminal term;
extern struct cterminal	*cterm;
extern int log_level;

void zmodem_upload(struct bbslist *bbs, FILE *fp, char *path);
void xmodem_upload(struct bbslist *bbs, FILE *fp, char *path, long mode, int lastch);
void xmodem_download(struct bbslist *bbs, long mode, char *path);
void zmodem_download(struct bbslist *bbs);
BOOL doterm(struct bbslist *);
void mousedrag(struct vmem_cell *scrollback);
void get_cterm_size(int *cols, int *rows, int ns);
int get_cache_fn_base(struct bbslist *bbs, char *fn, size_t fnsz);

#endif
