/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _CONN_PTY_H_
#define _CONN_PTY_H_
int pty_connect(struct bbslist *bbs);
int pty_close(void);
void pty_send_window_change(int text_cols, int text_rows,
                            int pixel_cols, int pixel_rows);

#endif
