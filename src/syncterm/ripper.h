#ifndef RIPPER_H
#define RIPPER_H

#include "bbslist.h"

extern bool rip_did_reinit;

void init_rip(struct bbslist *bbs);
void init_rip_ver(int ripver);
size_t parse_rip(BYTE *buf, unsigned blen, unsigned maxlen);
int rip_getch(void);
int rip_kbhit(void);
void suspend_rip(bool suspend);

#endif
