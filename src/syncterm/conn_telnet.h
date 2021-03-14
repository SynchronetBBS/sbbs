/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _TELNET_H_
#define _TELNET_H_

#include "sockwrap.h"

extern SOCKET telnet_sock;
void telnet_binary_mode_on(void);
void telnet_binary_mode_off(void);
int telnet_connect(struct bbslist *bbs);
void *telnet_rx_parse_cb(const void *buf, size_t inlen, size_t *olen);
void *telnet_tx_parse_cb(const void *buf, size_t len, size_t *olen);
#define telnet_close rlogin_close

#endif
