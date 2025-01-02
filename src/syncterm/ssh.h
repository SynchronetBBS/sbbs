/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _SSH_H_
#define _SSH_H_

#include <stdbool.h>

#include "cryptlib.h"
void init_crypt(void);
void exit_crypt(void);
int ssh_connect(struct bbslist *bbs);
int ssh_close(void);
void ssh_input_thread(void *args);
void ssh_output_thread(void *args);

extern SOCKET          ssh_sock;
extern CRYPT_SESSION   ssh_session;
extern int             ssh_channel;
extern pthread_mutex_t ssh_mutex;
extern int             sftp_channel;
extern bool            sftp_active;
void cryptlib_error_message(int status, const char *msg);

#endif // ifndef _SSH_H_
