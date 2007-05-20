#ifndef _MODEM_H_
#define _MODEM_H_

#include "bbslist.h"

int modem_connect(struct bbslist *bbs);
int modem_close(void);

#endif
