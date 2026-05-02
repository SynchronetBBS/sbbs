/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _SSH_H_
#define _SSH_H_

#ifdef _MSC_VER
/* MSVC keeps __STDC_NO_ATOMICS__ defined even with /experimental:c11atomics
 * because its atomic support is incomplete (no generic _Atomic with locks),
 * and the stdatomic.h header otherwise refuses to expand.  Same trick used
 * in conn.h. */
#undef __STDC_NO_ATOMICS__
#endif
#include <stdatomic.h>
#include <stdbool.h>

#include "sftp.h"      /* sftpc_state_t */
#include "sockwrap.h"  /* SOCKET */

void init_crypt(void);
void exit_crypt(void);
int  ssh_connect(struct bbslist *bbs);
int  ssh_close(void);
void ssh_send_window_change(int text_cols, int text_rows,
                            int pixel_cols, int pixel_rows);
void ssh_input_thread(void *args);
void ssh_output_thread(void *args);

extern SOCKET ssh_sock;

/* Per-SSH-session SFTP client state, owned by ssh.c.  sftp_state is
 * valid when sftp_available is true.  Lifetime spans from
 * ssh_connect() success to the start of ssh_close(): once ssh_close
 * sets sftp_available = false, callers must not issue new sftpc_*
 * calls, though in-flight calls are still woken by sftpc_finish().
 * Sole external consumer is wren_bind_sftp.c, which guards every
 * Wren-side SFTP foreign method on these two variables. */
extern sftpc_state_t  sftp_state;
extern _Atomic bool   sftp_available;

/* Deprecated — remove once telnets.c / uifc call sites no longer
   reference it.  Kept for link-compat during the st-dssh branch. */
void cryptlib_error_message(int status, const char *msg);

#endif	// ifndef _SSH_H_
