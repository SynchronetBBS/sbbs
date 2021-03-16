#ifndef RIPPER_H
#define RIPPER_H

void init_rip(int enabled);
size_t parse_rip(BYTE *buf, unsigned blen, unsigned maxlen);
int rip_getch(void);
int rip_kbhit(void);
void suspend_rip(bool suspend);

#endif
