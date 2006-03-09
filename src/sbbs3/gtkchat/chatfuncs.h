#ifndef _CHATFUNCS_H_
#define _CHATFUNCS_H_

extern char			usrname[128];
int chat_open(int node_num, char *ctrl_dir);
int chat_check_remote(void);
int chat_read_byte(void);
int chat_write_byte(unsigned char ch);
int chat_close(void);

#endif
