/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _TELNET_H_
#define _TELNET_H_

#include "sockwrap.h"

extern SOCKET telnet_sock;
void telnet_binary_mode_on(void);
void telnet_binary_mode_off(void);
int telnet_connect(struct bbslist *bbs);
int telnet_close(void);

#endif
