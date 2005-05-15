#ifndef VBBS_BASIC_H
#define VBBS_BASIC_H

#include <time.h>

#if !defined(_MSC_VER) && !defined(__BORLANDC__)
void strupr(char *text);
void strlwr(char *text);
#endif /* !_MSC_VER && !__BORLANDC__ */

int exist(char *fname);
void nl(void);
void paws(void);
int yesno(void);
int noyes(void);
int printfile(char *fname);
void center(char *text);
int tm_copy(struct tm *fr, struct tm *to);
char moreyn(void);
int mcistrset(char *mcstr, int chop);
int mcistrrset(char *mcstr, int chop);
int mcistrcset(char *mcstr, int chop);
void new_user(void);
void act_change(int idx, int val, int opflag);
void downskore(int sk);
void upskore(int sk);
void fillvars(void);

#endif /* VBBS_BASIC_H */
