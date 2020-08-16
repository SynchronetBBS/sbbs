/* Copyright (C), 2007 by Stephen Hurd */

/* $Id: telnet_io.h,v 1.11 2019/08/24 08:06:10 rswindell Exp $ */

#ifndef _TELNET_IO_H_
#define _TELNET_IO_H_

#ifndef TELNET_NO_DLL
#define TELNET_NO_DLL
#endif
#include <cterm.h>
#include "telnet.h"
#include "bbslist.h"

extern uchar	telnet_local_option[0x100];
extern uchar	telnet_remote_option[0x100];

BYTE*	telnet_interpret(BYTE* inbuf, int inlen, BYTE* outbuf, int *outlen, struct bbslist *bbs);
void	request_telnet_opt(uchar cmd, uchar opt);

#endif
