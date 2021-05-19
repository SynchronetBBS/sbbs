#ifndef RIPPER_H
#define RIPPER_H

#include "bbslist.h"

void init_rip(struct bbslist *bbs);
size_t parse_rip(BYTE *buf, unsigned blen, unsigned maxlen);
int rip_getch(void);
int rip_kbhit(void);
void suspend_rip(bool suspend);

#endif
