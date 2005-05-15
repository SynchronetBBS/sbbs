#ifndef _V3_IO_H_
#define _V3_IO_H_

int fexist(char *fname);
int countplyrs();
int saveplyr(tUserRecord *user, int usernum);
int readplyr(tUserRecord *user, int usernum);
int readapd(void);
void read_vtext(int vnum);
int count_vtext(void);
void read_actions(int actno);
int count_actions(void);
int count_virus(void);
int count_cpu(void);
int count_msgsr(void);
void read_msgsr(int actno);
void viruslist(void);
int vlcount(void);
void basedir(char *retstr);
void writemail(char *mfile, char *msg);
int readmaildat(mailrec *mr, int letter);
int savemaildat(mailrec *mr, int letter);

int vbbs_io_lock_game(const char* fullname);
void vbbs_io_unlock_game(void);

#endif

