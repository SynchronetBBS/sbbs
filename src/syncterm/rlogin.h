/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _RLOGIN_H_
#define _RLOGIN_H_

int rlogin_connect(struct bbslist *bbs);
int rlogin_close(void);
void rlogin_input_thread(void *args);
void rlogin_output_thread(void *args);
extern SOCKET rlogin_sock;

#endif
