#ifndef RETRO_H
#define RETRO_H

extern bool retro_set;

void retro_beep(void);
int retro_kbhit(void);
int retro_getch(void);
void retro_textmode(int);

#endif
