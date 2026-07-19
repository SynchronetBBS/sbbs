/* Copyright (C), 2007 by Stephen Hurd */

#ifndef _SSH_H_
#define _SSH_H_

#include <stdbool.h>

#include "sockwrap.h"  /* SOCKET */

void init_crypt(void);
void exit_crypt(void);
int  ssh_connect(struct bbslist *bbs);
int  ssh_close(void);
void ssh_send_window_change(int text_cols, int text_rows,
                            int pixel_cols, int pixel_rows);
void ssh_input_thread(void *args);
void ssh_output_thread(void *args);

/* Toggle the kernel SO_SNDBUF cap on the SSH socket.  active=true
 * caps at 64 KiB so a saturating SFTP transfer can't queue more
 * than one keystroke-budget's worth of data ahead of an interactive
 * keystroke; active=false restores the 1 MiB baseline so inline
 * transfers (zmodem/ymodem-G) get full BDP headroom.  No-op when no
 * SSH socket is open.  Caller should invoke only on flag
 * transitions to avoid flapping. */
void ssh_set_sftp_buffer_mode(bool active);

extern SOCKET ssh_sock;

#ifndef WITHOUT_DEUCESSH
#ifdef _MSC_VER
/* MSVC keeps __STDC_NO_ATOMICS__ defined even with /experimental:c11atomics
 * because its atomic support is incomplete (no generic _Atomic with locks),
 * and the stdatomic.h header otherwise refuses to expand.  Same trick used
 * in conn.h. */
#undef __STDC_NO_ATOMICS__
#endif
#include <stdatomic.h>

#include "sftp.h"      /* sftpc_state_t */

/* Per-SSH-session SFTP client state, owned by ssh.c.  sftp_state is
 * valid when sftp_available is true.  Lifetime spans from
 * ssh_connect() success to the start of ssh_close(): once ssh_close
 * sets sftp_available = false, callers must not issue new sftpc_*
 * calls, though in-flight calls are still woken by sftpc_finish().
 * Sole external consumer is wren_bind_sftp.c, which guards every
 * Wren-side SFTP foreign method on these two variables.  Only
 * declared when DeuceSSH is built in — non-SSH translation units
 * (telnets.c etc.) include this header but don't drag in sftp.h. */
extern sftpc_state_t  sftp_state;
extern _Atomic bool   sftp_available;
#endif

#endif	// ifndef _SSH_H_
