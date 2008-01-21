/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _MODEM_H_
#define _MODEM_H_

#include "bbslist.h"

int modem_connect(struct bbslist *bbs);
int serial_close(void);
int modem_close(void);

#endif
