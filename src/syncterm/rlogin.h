/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _RLOGIN_H_
#define _RLOGIN_H_
int rlogin_connect(struct bbslist *bbs);
int rlogin_close(void);
void rlogin_input_thread(void *args);
void rlogin_output_thread(void *args);
void rlogin_send_window_change(int text_cols, int text_rows,
                               int pixel_cols, int pixel_rows);
void rlogin_binary_mode_on(void);
void *rlogin_tx_parse_cb(const void *inbuf, size_t inlen, size_t *olen);

extern SOCKET rlogin_sock;

#endif
