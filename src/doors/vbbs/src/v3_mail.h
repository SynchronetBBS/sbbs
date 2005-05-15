#ifndef _V3_MAIL_H_
#define _V3_MAIL_H_

void maketmpmsg(char *tpath,char *from, char *fromsys, char *datetxt, char *subj);
void sendmail(void);
void network(void);
int countmail(void);
void viewmail(int letter);
void readnetmail(void);

#endif

