/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _SSH_H_
#define _SSH_H_

#include <stdbool.h>

#include "sockwrap.h"	/* SOCKET */

void init_crypt(void);
void exit_crypt(void);
int  ssh_connect(struct bbslist *bbs);
int  ssh_close(void);
void ssh_input_thread(void *args);
void ssh_output_thread(void *args);

extern SOCKET ssh_sock;

/* Deprecated — remove once telnets.c / uifc call sites no longer
   reference it.  Kept for link-compat during the st-dssh branch. */
void cryptlib_error_message(int status, const char *msg);

#endif	// ifndef _SSH_H_
